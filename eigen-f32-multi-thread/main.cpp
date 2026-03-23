#include <Eigen/Dense>

#include <atomic>
#include <barrier>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <sched.h>

#include "cpu_topology.h"
#include "tsc_timer.h"

#ifndef MATRIX_BENCH_SIMD_MODE_NAME
#define MATRIX_BENCH_SIMD_MODE_NAME "unknown"
#endif

#ifndef MATRIX_BENCH_REQUIRED_CPU_FLAGS
#define MATRIX_BENCH_REQUIRED_CPU_FLAGS "unknown"
#endif

namespace {

constexpr Eigen::Index MatrixSide = 256;
constexpr std::size_t WarmupIterations = 5;
constexpr std::size_t BenchmarkIterations = 100;
constexpr std::uint32_t RngSeed = 20260323U;

using MatrixF32 = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

struct BenchmarkResult {
  matrix_bench::TSC total_cycles{};
  double total_ms{};
  double avg_cycles{};
  double avg_us{};
  double checksum{};
};

std::size_t parse_thread_count(int argc, char** argv) {
  std::size_t thread_count = 1;

  for (int idx = 1; idx < argc; ++idx) {
    std::string arg = argv[idx];
    if (arg == "-j") {
      if (idx + 1 >= argc) {
        throw std::runtime_error("missing value after -j");
      }
      thread_count = static_cast<std::size_t>(std::stoul(argv[++idx]));
      continue;
    }
    throw std::runtime_error("unsupported argument: " + arg);
  }

  if (thread_count == 0) {
    throw std::runtime_error("thread count must be positive");
  }
  return thread_count;
}

MatrixF32 make_random_matrix(std::mt19937& rng) {
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  MatrixF32 matrix(MatrixSide, MatrixSide);

  for (Eigen::Index row = 0; row < matrix.rows(); ++row) {
    for (Eigen::Index col = 0; col < matrix.cols(); ++col) {
      matrix(row, col) = dist(rng);
    }
  }

  return matrix;
}

std::vector<MatrixF32> make_random_matrix_batch(std::mt19937& rng, std::size_t count) {
  std::vector<MatrixF32> matrices;
  matrices.reserve(count);

  for (std::size_t idx = 0; idx < count; ++idx) {
    matrices.push_back(make_random_matrix(rng));
  }

  return matrices;
}

struct OutputShard {
  Eigen::Index col_begin{};
  Eigen::Index col_count{};
};

std::vector<OutputShard> make_output_shards(std::size_t parts) {
  std::vector<OutputShard> shards;
  shards.reserve(parts);

  for (std::size_t idx = 0; idx < parts; ++idx) {
    const Eigen::Index col_begin =
      static_cast<Eigen::Index>((MatrixSide * static_cast<Eigen::Index>(idx)) /
                                static_cast<Eigen::Index>(parts));
    const Eigen::Index col_end =
      static_cast<Eigen::Index>((MatrixSide * static_cast<Eigen::Index>(idx + 1)) /
                                static_cast<Eigen::Index>(parts));
    shards.push_back(OutputShard{
      .col_begin = col_begin,
      .col_count = col_end - col_begin,
    });
  }

  return shards;
}

std::vector<MatrixF32> make_weight_shards(const MatrixF32& weight_matrix, const std::vector<OutputShard>& shards) {
  std::vector<MatrixF32> weight_shards;
  weight_shards.reserve(shards.size());

  for (const auto& shard : shards) {
    weight_shards.push_back(weight_matrix.middleCols(shard.col_begin, shard.col_count));
  }

  return weight_shards;
}

long long rounded_i64(double value) {
  return static_cast<long long>(std::llround(value));
}

BenchmarkResult make_result(
  matrix_bench::TSC total_cycles,
  std::size_t iterations,
  std::uint64_t tsc_freq_hz,
  const MatrixF32& result) {
  const double total_ms =
    static_cast<double>(total_cycles) * 1'000.0 / static_cast<double>(tsc_freq_hz);

  return BenchmarkResult{
    .total_cycles = total_cycles,
    .total_ms = total_ms,
    .avg_cycles = static_cast<double>(total_cycles) / static_cast<double>(iterations),
    .avg_us = total_ms * 1'000.0 / static_cast<double>(iterations),
    .checksum = static_cast<double>(result.sum()),
  };
}

void print_result(const char* label, std::size_t iterations, const BenchmarkResult& result) {
  std::cout << label << "_loop_count=" << iterations << '\n';
  std::cout << label << "_iterations=" << iterations << '\n';
  std::cout << label << "_per_inference_us=" << rounded_i64(result.avg_us) << '\n';
  std::cout << label << "_total_cycles=" << result.total_cycles << '\n';
  std::cout << label << "_total_ms=" << rounded_i64(result.total_ms) << '\n';
  std::cout << label << "_avg_cycles=" << rounded_i64(result.avg_cycles) << '\n';
  std::cout << label << "_checksum=" << rounded_i64(result.checksum) << '\n';
}

void validate_result(const MatrixF32& result, const MatrixF32& reference) {
  const bool all_finite = result.array().isFinite().all();
  if (!all_finite) {
    throw std::runtime_error("matrix multiply produced non-finite values");
  }

  const double checksum = static_cast<double>(result.sum());
  if (!std::isfinite(checksum)) {
    throw std::runtime_error("matrix checksum is not finite");
  }

  const float max_error = (result - reference).cwiseAbs().maxCoeff();
  if (max_error > 1.0e-4f) {
    throw std::runtime_error("multi-thread result mismatches reference");
  }
}

}  // namespace

int main(int argc, char** argv) {
  const std::size_t participant_count = parse_thread_count(argc, argv);
  const auto binding_plan = matrix_bench::build_binding_plan(participant_count);
  const std::uint64_t tsc_freq_hz = matrix_bench::resolve_tsc_freq();
  const std::size_t total_loop_count = WarmupIterations + BenchmarkIterations;

  std::mt19937 rng(RngSeed);
  auto input_tensors = make_random_matrix_batch(rng, total_loop_count);
  MatrixF32 weight_matrix = make_random_matrix(rng);
  MatrixF32 reference = input_tensors.back() * weight_matrix;
  MatrixF32 result(MatrixSide, MatrixSide);
  auto shards = make_output_shards(participant_count);
  auto weight_shards = make_weight_shards(weight_matrix, shards);
  std::vector<int> observed_worker_cpus(binding_plan.worker_cpus.size(), -1);
  int observed_main_cpu = -1;

  matrix_bench::bind_current_thread_to_cpu(binding_plan.main_cpu);
  observed_main_cpu = sched_getcpu();

  std::barrier start_barrier(static_cast<std::ptrdiff_t>(binding_plan.worker_cpus.size() + 1));
  std::barrier finish_barrier(static_cast<std::ptrdiff_t>(binding_plan.worker_cpus.size() + 1));
  std::atomic<const MatrixF32*> current_input{nullptr};
  std::atomic<bool> stop{false};

  std::vector<std::thread> workers;
  workers.reserve(binding_plan.worker_cpus.size());

  for (std::size_t worker_idx = 0; worker_idx < binding_plan.worker_cpus.size(); ++worker_idx) {
    const int cpu = binding_plan.worker_cpus[worker_idx];
    const std::size_t shard_idx = worker_idx + 1;

    workers.emplace_back([&, worker_idx, shard_idx, cpu]() {
      matrix_bench::bind_current_thread_to_cpu(cpu);
      observed_worker_cpus[worker_idx] = sched_getcpu();
      const auto shard = shards[shard_idx];

      while (true) {
        start_barrier.arrive_and_wait();
        if (stop.load(std::memory_order_acquire)) {
          break;
        }

        const MatrixF32& input_tensor = *current_input.load(std::memory_order_acquire);
        result.middleCols(shard.col_begin, shard.col_count).noalias() =
          input_tensor * weight_shards[shard_idx];
        finish_barrier.arrive_and_wait();
      }
    });
  }

  auto run_phase = [&](std::size_t start_index, std::size_t iterations) {
    const matrix_bench::TSC start_tsc = matrix_bench::rdtsc();
    for (std::size_t iter = 0; iter < iterations; ++iter) {
      current_input.store(&input_tensors[start_index + iter], std::memory_order_release);
      start_barrier.arrive_and_wait();

      const auto main_shard = shards.front();
      result.middleCols(main_shard.col_begin, main_shard.col_count).noalias() =
        input_tensors[start_index + iter] * weight_shards.front();

      finish_barrier.arrive_and_wait();
    }
    const matrix_bench::TSC end_tsc = matrix_bench::rdtsc();
    return make_result(end_tsc - start_tsc, iterations, tsc_freq_hz, result);
  };

  BenchmarkResult warmup_result = run_phase(0, WarmupIterations);
  BenchmarkResult measured_result = run_phase(WarmupIterations, BenchmarkIterations);

  stop.store(true, std::memory_order_release);
  start_barrier.arrive_and_wait();
  for (auto& worker : workers) {
    worker.join();
  }

  validate_result(result, reference);

  std::cout << "matrix_shape=" << MatrixSide << "x" << MatrixSide << '\n';
  std::cout << "dtype=float32\n";
  std::cout << "simd_mode=" << MATRIX_BENCH_SIMD_MODE_NAME << '\n';
  std::cout << "required_cpu_flags=" << MATRIX_BENCH_REQUIRED_CPU_FLAGS << '\n';
  std::cout << "rng_seed=" << RngSeed << '\n';
  std::cout << "participant_count=" << participant_count << '\n';
  std::cout << "physical_core_count=" << binding_plan.physical_core_count << '\n';
  std::cout << "logical_cpu_count=" << binding_plan.logical_cpu_count << '\n';
  std::cout << "uses_hyper_threads="
            << (participant_count > binding_plan.physical_core_count ? 1 : 0) << '\n';
  std::cout << "binding_policy=" << binding_plan.policy << '\n';
  std::cout << "compute_cpus=" << matrix_bench::join_cpu_list(binding_plan.compute_cpus) << '\n';
  std::cout << "main_cpu=" << binding_plan.main_cpu << '\n';
  std::cout << "worker_cpus=" << matrix_bench::join_cpu_list(binding_plan.worker_cpus) << '\n';
  std::cout << "observed_main_cpu=" << observed_main_cpu << '\n';
  std::cout << "observed_worker_cpus="
            << matrix_bench::join_cpu_list(observed_worker_cpus) << '\n';
  std::cout << "sibling_groups="
            << matrix_bench::format_sibling_groups(binding_plan.sibling_groups) << '\n';
  std::cout << "total_loop_count=" << total_loop_count << '\n';
  std::cout << "tsc_freq_hz=" << tsc_freq_hz << '\n';
  std::cout << "validation_checksum=" << rounded_i64(static_cast<double>(reference.sum())) << '\n';
  print_result("warmup", WarmupIterations, warmup_result);
  print_result("measured", BenchmarkIterations, measured_result);

  return 0;
}
