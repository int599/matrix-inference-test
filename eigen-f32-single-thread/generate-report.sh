#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
readonly REPORT_DIR="${SCRIPT_DIR}/reports"
readonly DATA_PATH="${REPORT_DIR}/single-thread-data.tsv"
readonly REPEATS=5
declare -ra SIMD_MODES=("scalar" "sse42" "avx128" "avx256")

# shellcheck source=../scripts/platform-info.sh
. "${ROOT_DIR}/scripts/platform-info.sh"

readonly REPORT_NAME="single-thread-report-${MATRIX_BENCH_CPU_SLUG}.md"
readonly REPORT_PATH="${REPORT_DIR}/${REPORT_NAME}"
readonly CSV_NAME="single-thread-data-${MATRIX_BENCH_CPU_SLUG}.csv"
readonly CSV_PATH="${REPORT_DIR}/${CSV_NAME}"
readonly META_NAME="single-thread-meta-${MATRIX_BENCH_CPU_SLUG}.json"
readonly META_PATH="${REPORT_DIR}/${META_NAME}"

if [[ ! -f "${DATA_PATH}" ]]; then
  printf 'missing data file: %s\n' "${DATA_PATH}" >&2
  exit 1
fi

{
  printf "# Eigen F32 Single-thread Benchmark Report\n\n"
  printf -- '- CPU model: `%s`\n' "${MATRIX_BENCH_CPU_MODEL}"
  printf -- '- Physical cores: `%s`\n' "${MATRIX_BENCH_PHYSICAL_CORE_COUNT}"
  printf -- '- Logical CPUs: `%s`\n' "${MATRIX_BENCH_LOGICAL_CPU_COUNT}"
  printf -- '- Data file: `reports/%s`\n' "$(basename "${DATA_PATH}")"
  printf -- '- CSV file: `reports/%s`\n' "${CSV_NAME}"
  printf -- '- Metadata file: `reports/%s`\n' "${META_NAME}"
  printf "\n## Summary\n\n"
  printf "| SIMD Mode | Status | Best us | Mean us | Worst us |\n"
  printf "|---|---|---:|---:|---:|\n"
  for simd_mode in "${SIMD_MODES[@]}"; do
    local_display_mode="$(matrix_bench_display_simd_mode "${simd_mode}")"
    row_count="$(awk -F'\t' -v mode="${simd_mode}" '$1 == mode && $2 == "ok" {count += 1} END {print count + 0}' "${DATA_PATH}")"
    if [[ "${row_count}" == "0" ]]; then
      printf '| %s | unsupported on `%s` | NA | NA | NA |\n' "${local_display_mode}" "${MATRIX_BENCH_CPU_MODEL}"
      continue
    fi

    best_us="$(awk -F'\t' -v mode="${simd_mode}" '$1 == mode && $2 == "ok" && (count == 0 || $4 < best) {best = $4; count += 1} END {print best}' "${DATA_PATH}")"
    mean_us="$(awk -F'\t' -v mode="${simd_mode}" '$1 == mode && $2 == "ok" {sum += $4; count += 1} END {if (count == 0) {print "NA"} else {printf("%d", sum / count)}}' "${DATA_PATH}")"
    worst_us="$(awk -F'\t' -v mode="${simd_mode}" '$1 == mode && $2 == "ok" && (count == 0 || $4 > worst) {worst = $4; count += 1} END {print worst}' "${DATA_PATH}")"
    printf '| %s | ok | %s | %s | %s |\n' "${local_display_mode}" "${best_us}" "${mean_us}" "${worst_us}"
  done
  printf "\n## Results\n\n"
  printf "| SIMD Mode | Status | Run | Per-inference us | Total ms for 100 loops |\n"
  printf "|---|---|---|---:|---:|\n"
  awk -F'\t' '
    function display_mode(mode) {
      if (mode == "scalar") {
        return "SCALAR";
      }
      if (mode == "sse42") {
        return "SSE4.2";
      }
      if (mode == "avx128") {
        return "AVX";
      }
      if (mode == "avx256") {
        return "AVX2";
      }
      return mode;
    }
    NR > 1 {
      printf("| %s | %s | %s | %s | %s |\n", display_mode($1), $2, $3, $4, $5)
    }' "${DATA_PATH}"
  printf "\n## Notes\n\n"
  printf -- '- This suite runs the single-thread benchmark `%s` times for each supported SIMD mode.\n' "${REPEATS}"
  printf -- '- Hosted compiler default is inferred as `%s`: target `%s`, tune `%s`.\n' \
    "${MATRIX_BENCH_COMPILER_DEFAULT_SIMD_LEVEL}" \
    "${MATRIX_BENCH_COMPILER_DEFAULT_MARCH}" \
    "${MATRIX_BENCH_COMPILER_DEFAULT_MTUNE}"
  printf -- '- `SCALAR` disables Eigen vectorization, disables compiler tree vectorization, and disables AVX-family targets explicitly.\n'
  printf -- '- `SSE4.2` uses explicit `-msse4.2` and disables AVX-family targets.\n'
  printf -- '- `AVX` means explicit AVX1 (`-mavx`) while keeping `avx2`, `fma`, and `avx512*` disabled.\n'
  printf -- '- `AVX2` uses explicit `-mavx2 -mfma`.\n'
  printf -- '- Primary metric is one inference loop (`per_inference_us`). Total ms is shown only as supporting context for the 100-loop measurement window.\n'
  printf -- '- Repeat-by-repeat rows stay in this detailed artifact and in the CSV; published PNGs are rendered locally from the CSV + metadata with `scripts/render-published-results.py`.\n'
  printf -- '- Unsupported modes stay in the tables so CPU capability gaps remain visible in the artifact.\n'
} > "${REPORT_PATH}"

printf 'report_path=%s\n' "${REPORT_PATH}"
