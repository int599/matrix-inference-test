#include "tsc_timer.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#ifndef __NR_perf_event_open
#if defined(__x86_64__)
#define __NR_perf_event_open 298
#elif defined(__i386__)
#define __NR_perf_event_open 336
#elif defined(__aarch64__)
#define __NR_perf_event_open 241
#else
#error "__NR_perf_event_open not defined for this architecture"
#endif
#endif

extern "C" {
int perf_event_open(
  struct perf_event_attr* hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
  return static_cast<int>(syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags));
}
}

namespace matrix_bench {

std::uint64_t get_tsc_freq() {
  perf_event_attr pe{};
  pe.type = PERF_TYPE_HARDWARE;
  pe.size = sizeof(perf_event_attr);
  pe.config = PERF_COUNT_HW_INSTRUCTIONS;
  pe.disabled = 1;
  pe.exclude_kernel = 1;
  pe.exclude_hv = 1;

  const int fd = perf_event_open(&pe, 0, -1, -1, 0);
  if (fd == -1) {
    throw std::runtime_error(std::string("perf_event_open failed: ") + std::strerror(errno));
  }

  void* addr = mmap(nullptr, 4 * 1024, PROT_READ, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    close(fd);
    throw std::runtime_error(std::string("mmap failed: ") + std::strerror(errno));
  }

  auto* page = static_cast<perf_event_mmap_page*>(addr);
  if (page->cap_user_time != 1) {
    munmap(addr, 4 * 1024);
    close(fd);
    throw std::runtime_error("Perf system doesn't support user time");
  }

  const std::uint32_t time_mult = page->time_mult;
  const std::uint16_t time_shift = page->time_shift;
  const std::uint64_t freq = (1'000'000'000ULL << time_shift) / time_mult;

  munmap(addr, 4 * 1024);
  close(fd);
  return freq;
}

std::uint64_t infer_tsc_freq() {
#if !(defined(__x86_64__) || defined(__i386__))
  throw std::runtime_error("infer_tsc_freq is supported only on x86/x86_64");
#else
  timespec ts_s{};
  timespec ts_e{};

  std::uint64_t tsc_s = rdtsc();
  if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_s) != 0) {
    throw std::runtime_error(std::string("clock_gettime failed: ") + std::strerror(errno));
  }
  std::uint64_t tsc_e2 = rdtsc();

  std::uint64_t min_tsc_gtod = tsc_e2 - tsc_s;
  std::uint64_t tsc_gtod = min_tsc_gtod;

  int sample_count = 0;
  do {
    tsc_s = rdtsc();
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_s) != 0) {
      throw std::runtime_error(std::string("clock_gettime failed: ") + std::strerror(errno));
    }
    tsc_e2 = rdtsc();
    tsc_gtod = tsc_e2 - tsc_s;
    if (tsc_gtod < min_tsc_gtod) {
      min_tsc_gtod = tsc_gtod;
    }
  } while (++sample_count < 20 || (tsc_gtod > min_tsc_gtod * 2 && sample_count < 100));

  constexpr std::uint64_t interval_ns = 100'000'000ULL;
  std::uint64_t elapsed_ns = 0;
  std::uint64_t tsc_e = 0;

  do {
    tsc_e = rdtsc();
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_e) != 0) {
      throw std::runtime_error(std::string("clock_gettime failed: ") + std::strerror(errno));
    }
    const std::uint64_t tsc_e3 = rdtsc();
    if (tsc_e3 < tsc_e) {
      throw std::runtime_error("TSC moved backwards during measure");
    }
    tsc_gtod = tsc_e3 - tsc_e;

    std::int64_t sec = static_cast<std::int64_t>(ts_e.tv_sec) -
                       static_cast<std::int64_t>(ts_s.tv_sec);
    std::int64_t nsec = static_cast<std::int64_t>(ts_e.tv_nsec) -
                        static_cast<std::int64_t>(ts_s.tv_nsec);
    if (nsec < 0) {
      sec -= 1;
      nsec += 1'000'000'000LL;
    }
    if (sec < 0 || (sec == 0 && nsec < 0)) {
      throw std::runtime_error("monotonic clock moved backwards");
    }
    elapsed_ns = static_cast<std::uint64_t>(sec) * 1'000'000'000ULL +
                 static_cast<std::uint64_t>(nsec);
  } while (elapsed_ns < interval_ns || tsc_gtod > min_tsc_gtod * 2);

  if (elapsed_ns == 0) {
    throw std::runtime_error("zero measurement interval");
  }

  const std::uint64_t cycles = tsc_e - tsc_s;
  const std::uint64_t hz = (cycles * 1'000'000'000ULL) / elapsed_ns;

  constexpr std::uint64_t kMinHz = 100'000'000ULL;
  constexpr std::uint64_t kMaxHz = 10'000'000'000ULL;
  if (hz < kMinHz || hz > kMaxHz) {
    throw std::runtime_error("inferred TSC frequency out of bounds");
  }
  return hz;
#endif
}

std::uint64_t resolve_tsc_freq() {
  static std::uint64_t cached_hz = 0;
  if (cached_hz != 0) {
    return cached_hz;
  }

  try {
    cached_hz = get_tsc_freq();
  } catch (...) {
    cached_hz = infer_tsc_freq();
  }
  return cached_hz;
}

}  // namespace matrix_bench
