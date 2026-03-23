cmake_minimum_required(VERSION 3.20)

project(matrix_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MatrixBenchSimdModes scalar sse42 avx128 avx256)

function(matrix_bench_apply_simd_mode target mode)
  target_compile_definitions(
    ${target}
    PRIVATE
      EIGEN_DONT_PARALLELIZE=1
      MATRIX_BENCH_SIMD_MODE_NAME=\"${mode}\")

  if(mode STREQUAL "scalar")
    target_compile_definitions(
      ${target}
      PRIVATE
        EIGEN_DONT_VECTORIZE=1
        MATRIX_BENCH_REQUIRED_CPU_FLAGS=\"none\")
    target_compile_options(
      ${target}
      PRIVATE
        -fno-tree-vectorize
        -mno-avx
        -mno-avx2
        -mno-fma
        -mno-avx512f
        -mno-avx512dq
        -mno-avx512cd
        -mno-avx512bw
        -mno-avx512vl)
  elseif(mode STREQUAL "sse42")
    target_compile_definitions(
      ${target}
      PRIVATE
        MATRIX_BENCH_REQUIRED_CPU_FLAGS=\"sse4_2\")
    target_compile_options(
      ${target}
      PRIVATE
        -msse4.2
        -mno-avx
        -mno-avx2
        -mno-fma
        -mno-avx512f
        -mno-avx512dq
        -mno-avx512cd
        -mno-avx512bw
        -mno-avx512vl)
  elseif(mode STREQUAL "avx128")
    target_compile_definitions(
      ${target}
      PRIVATE
        MATRIX_BENCH_REQUIRED_CPU_FLAGS=\"avx\")
    target_compile_options(
      ${target}
      PRIVATE
        -mavx
        -mno-avx2
        -mno-fma
        -mno-avx512f
        -mno-avx512dq
        -mno-avx512cd
        -mno-avx512bw
        -mno-avx512vl)
  elseif(mode STREQUAL "avx256")
    target_compile_definitions(${target} PRIVATE MATRIX_BENCH_REQUIRED_CPU_FLAGS=\"avx2,fma\")
    target_compile_options(${target} PRIVATE -mavx2 -mfma)
  else()
    message(FATAL_ERROR "Unsupported SIMD mode: ${mode}")
  endif()
endfunction()

add_subdirectory(eigen-f32-single-thread)
add_subdirectory(eigen-f32-multi-thread)
