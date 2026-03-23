set(EIGEN_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/ext/eigen-3.4.0")

if (NOT EXISTS "${EIGEN_INCLUDE_DIR}/Eigen/Dense")
  message(FATAL_ERROR "Eigen not found at ${EIGEN_INCLUDE_DIR}. Run ./download-eigen.sh first.")
endif()

foreach(simd_mode IN LISTS MatrixBenchSimdModes)
  string(REPLACE "-" "_" simd_suffix "${simd_mode}")
  set(target_name "eigen_f32_single_thread_${simd_suffix}")

  add_executable(${target_name} main.cpp tsc_timer.cpp)
  target_include_directories(${target_name} PRIVATE "${EIGEN_INCLUDE_DIR}")
  matrix_bench_apply_simd_mode(${target_name} "${simd_mode}")
endforeach()
