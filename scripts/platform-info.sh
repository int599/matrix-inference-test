#!/usr/bin/env bash

readonly MATRIX_BENCH_SIMD_MODES="scalar,sse42,avx128,avx256"

matrix_bench_read_lscpu_field() {
  local key="$1"
  local field=""
  local value=""

  while IFS=: read -r field value; do
    field="${field#"${field%%[![:space:]]*}"}"
    field="${field%"${field##*[![:space:]]}"}"

    if [[ "${field}" != "${key}" ]]; then
      continue
    fi

    value="${value#"${value%%[![:space:]]*}"}"
    printf '%s\n' "${value}"
    return 0
  done < <(lscpu)

  return 1
}

matrix_bench_slugify_cpu_model() {
  local cpu_model="$1"
  printf '%s' "${cpu_model}" \
    | tr '[:upper:]' '[:lower:]' \
    | sed -E 's/\(r\)|\(tm\)|cpu|processor//g; s/[^a-z0-9]+/-/g; s/^-+|-+$//g; s/-+/-/g'
}

matrix_bench_join_by_comma() {
  local first=1
  local value
  for value in "$@"; do
    if (( first == 0 )); then
      printf ','
    fi
    printf '%s' "${value}"
    first=0
  done
}

matrix_bench_compute_thread_counts() {
  local logical_cpu_count="$1"
  local thread_count=1
  local thread_counts=()

  while (( thread_count <= logical_cpu_count )); do
    thread_counts+=("${thread_count}")
    thread_count=$((thread_count * 2))
  done

  matrix_bench_join_by_comma "${thread_counts[@]}"
}

matrix_bench_cpu_flags() {
  awk -F: '
    /^Flags:/ || /^flags[[:space:]]*:/ {
      gsub(/^[[:space:]]+/, "", $2);
      print $2;
      exit;
    }' /proc/cpuinfo
}

matrix_bench_compiler_default_macros() {
  "${CXX:-g++}" -dM -E -x c++ /dev/null
}

matrix_bench_compiler_default_target_value() {
  local key="$1"
  local cc_bin="${CC:-gcc}"
  local field=""
  local value=""
  local extra=""

  while read -r field value extra; do
    if [[ "${field}" != "${key}" ]]; then
      continue
    fi

    printf '%s\n' "${value}"
    return 0
  done < <("${cc_bin}" -Q --help=target 2>/dev/null)

  return 1
}

matrix_bench_compiler_default_simd_level() {
  local macros="$1"

  if grep -q '__AVX512F__' <<< "${macros}"; then
    printf 'AVX-512'
  elif grep -q '__AVX2__' <<< "${macros}"; then
    printf 'AVX2'
  elif grep -q '__AVX__' <<< "${macros}"; then
    printf 'AVX'
  elif grep -q '__SSE4_2__' <<< "${macros}"; then
    printf 'SSE4.2'
  elif grep -q '__SSE4_1__' <<< "${macros}"; then
    printf 'SSE4.1'
  elif grep -q '__SSSE3__' <<< "${macros}"; then
    printf 'SSSE3'
  elif grep -q '__SSE3__' <<< "${macros}"; then
    printf 'SSE3'
  elif grep -q '__SSE2__' <<< "${macros}"; then
    printf 'SSE2'
  else
    printf 'scalar'
  fi
}

matrix_bench_display_simd_mode() {
  local simd_mode="$1"

  case "${simd_mode}" in
    scalar)
      printf 'SCALAR'
      ;;
    sse42)
      printf 'SSE4.2'
      ;;
    avx128)
      printf 'AVX'
      ;;
    avx256)
      printf 'AVX2'
      ;;
    *)
      printf '%s' "${simd_mode}"
      ;;
  esac
}

matrix_bench_tsv_to_csv() {
  local input_path="$1"
  local output_path="$2"

  awk -F'\t' '
    BEGIN {
      OFS = ",";
    }
    {
      for (idx = 1; idx <= NF; ++idx) {
        gsub(/"/, "\"\"", $idx);
        printf "%s\"%s\"", (idx == 1 ? "" : OFS), $idx;
      }
      printf "\n";
    }' "${input_path}" > "${output_path}"
}

matrix_bench_host_supports_simd_mode() {
  local simd_mode="$1"
  local flags=" ${MATRIX_BENCH_CPU_FLAGS} "

  case "${simd_mode}" in
    scalar)
      return 0
      ;;
    sse42)
      [[ "${flags}" == *" sse4_2 "* ]]
      return
      ;;
    avx128)
      [[ "${flags}" == *" avx "* ]]
      return
      ;;
    avx256)
      [[ "${flags}" == *" avx2 "* && "${flags}" == *" fma "* ]]
      return
      ;;
    *)
      printf 'unsupported simd mode: %s\n' "${simd_mode}" >&2
      return 1
      ;;
  esac
}

MATRIX_BENCH_CPU_MODEL="$(matrix_bench_read_lscpu_field 'Model name')"
MATRIX_BENCH_CPU_SLUG="$(matrix_bench_slugify_cpu_model "${MATRIX_BENCH_CPU_MODEL}")"
MATRIX_BENCH_CPU_FLAGS="$(matrix_bench_cpu_flags)"
MATRIX_BENCH_COMPILER_DEFAULT_MACROS="$(matrix_bench_compiler_default_macros)"
MATRIX_BENCH_COMPILER_DEFAULT_MARCH="$(matrix_bench_compiler_default_target_value '-march=')"
MATRIX_BENCH_COMPILER_DEFAULT_MTUNE="$(matrix_bench_compiler_default_target_value '-mtune=')"
MATRIX_BENCH_COMPILER_DEFAULT_SIMD_LEVEL="$(
  matrix_bench_compiler_default_simd_level "${MATRIX_BENCH_COMPILER_DEFAULT_MACROS}"
)"
MATRIX_BENCH_LOGICAL_CPU_COUNT="$(matrix_bench_read_lscpu_field 'CPU(s)')"
MATRIX_BENCH_THREADS_PER_CORE="$(matrix_bench_read_lscpu_field 'Thread(s) per core')"
MATRIX_BENCH_CORES_PER_SOCKET="$(matrix_bench_read_lscpu_field 'Core(s) per socket')"
MATRIX_BENCH_SOCKET_COUNT="$(matrix_bench_read_lscpu_field 'Socket(s)')"
MATRIX_BENCH_PHYSICAL_CORE_COUNT=$((MATRIX_BENCH_CORES_PER_SOCKET * MATRIX_BENCH_SOCKET_COUNT))
MATRIX_BENCH_HAS_SMT=0
if (( MATRIX_BENCH_LOGICAL_CPU_COUNT > MATRIX_BENCH_PHYSICAL_CORE_COUNT )); then
  MATRIX_BENCH_HAS_SMT=1
fi
MATRIX_BENCH_THREAD_COUNTS="$(matrix_bench_compute_thread_counts "${MATRIX_BENCH_LOGICAL_CPU_COUNT}")"
