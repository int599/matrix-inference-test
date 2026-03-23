#pragma once

#include <cstdint>

namespace matrix_bench {

using TSC = std::uint64_t;

inline TSC rdtsc() {
  return __builtin_ia32_rdtsc();
}

std::uint64_t get_tsc_freq();
std::uint64_t infer_tsc_freq();
std::uint64_t resolve_tsc_freq();

}  // namespace matrix_bench
