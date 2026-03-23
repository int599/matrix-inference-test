#include <Eigen/Dense>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

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

long long rounded_i64(double value) {
  return static_cast<long long>(std::llround(value));
}

BenchmarkResult benchmark_multiply(
  const std::vector<MatrixF32>& input_tensors,
  const MatrixF32& weight_matrix,
  std::size_t start_index,
  std::size_t iterations,
  std::uint64_t tsc_freq_hz) {
  MatrixF32 result(MatrixSide, MatrixSide);

  const matrix_bench::TSC start_tsc = matrix_bench::rdtsc();
  for (std::size_t iter = 0; iter < iterations; ++iter) {
    result.noalias() = input_tensors[start_index + iter] * weight_matrix;
  }
  const matrix_bench::TSC end_tsc = matrix_bench::rdtsc();
  const matrix_bench::TSC total_cycles = end_tsc - start_tsc;

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

void validate_result(const MatrixF32& result) {
  const bool all_finite = result.array().isFinite().all();
  if (!all_finite) {
    throw std::runtime_error("matrix multiply produced non-finite values");
  }

  const double checksum = static_cast<double>(result.sum());
  if (!std::isfinite(checksum)) {
    throw std::runtime_error("matrix checksum is not finite");
  }
}

}  // namespace

int main() {
  std::mt19937 rng(RngSeed);
  const std::uint64_t tsc_freq_hz = matrix_bench::resolve_tsc_freq();
  const std::size_t total_loop_count = WarmupIterations + BenchmarkIterations;

  auto input_tensors = make_random_matrix_batch(rng, total_loop_count);
  MatrixF32 weight_matrix = make_random_matrix(rng);

  MatrixF32 validation = input_tensors.front() * weight_matrix;
  validate_result(validation);

  BenchmarkResult warmup_result =
    benchmark_multiply(input_tensors, weight_matrix, 0, WarmupIterations, tsc_freq_hz);
  BenchmarkResult measured_result = benchmark_multiply(
    input_tensors, weight_matrix, WarmupIterations, BenchmarkIterations, tsc_freq_hz);

  std::cout << "matrix_shape=" << MatrixSide << "x" << MatrixSide << '\n';
  std::cout << "dtype=float32\n";
  std::cout << "simd_mode=" << MATRIX_BENCH_SIMD_MODE_NAME << '\n';
  std::cout << "required_cpu_flags=" << MATRIX_BENCH_REQUIRED_CPU_FLAGS << '\n';
  std::cout << "rng_seed=" << RngSeed << '\n';
  std::cout << "total_loop_count=" << total_loop_count << '\n';
  std::cout << "tsc_freq_hz=" << tsc_freq_hz << '\n';
  std::cout << "validation_checksum=" << rounded_i64(static_cast<double>(validation.sum())) << '\n';
  print_result("warmup", WarmupIterations, warmup_result);
  print_result("measured", BenchmarkIterations, measured_result);

  return 0;
}
