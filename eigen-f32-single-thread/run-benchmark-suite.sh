#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
readonly BUILD_DIR="${SCRIPT_DIR}/build"
readonly REPORT_DIR="${SCRIPT_DIR}/reports"
readonly DATA_PATH="${REPORT_DIR}/single-thread-data.tsv"
readonly REPEATS=5

# shellcheck source=../scripts/platform-info.sh
. "${ROOT_DIR}/scripts/platform-info.sh"

readonly REPORT_NAME="single-thread-report-${MATRIX_BENCH_CPU_SLUG}.md"
readonly REPORT_PATH="${REPORT_DIR}/${REPORT_NAME}"
readonly CSV_NAME="single-thread-data-${MATRIX_BENCH_CPU_SLUG}.csv"
readonly CSV_PATH="${REPORT_DIR}/${CSV_NAME}"
readonly META_NAME="single-thread-meta-${MATRIX_BENCH_CPU_SLUG}.json"
readonly META_PATH="${REPORT_DIR}/${META_NAME}"

target_path_for_mode() {
  local simd_mode="$1"
  local simd_suffix="${simd_mode//-/_}"
  printf '%s/eigen-f32-single-thread/eigen_f32_single_thread_%s\n' "${BUILD_DIR}" "${simd_suffix}"
}

mkdir -p "${REPORT_DIR}"
rm -f "${REPORT_DIR}"/run-*.txt "${REPORT_DIR}"/single-thread-report-*.md \
  "${REPORT_DIR}"/single-thread-summary-*.png "${REPORT_DIR}"/single-thread-data-*.csv \
  "${REPORT_DIR}"/single-thread-meta-*.json "${DATA_PATH}"

make -C "${SCRIPT_DIR}" configure >/dev/null

printf "simd_mode\tstatus\trun_label\tmeasured_per_inference_us\tmeasured_total_ms\n" > "${DATA_PATH}"

IFS=',' read -r -a simd_modes <<< "${MATRIX_BENCH_SIMD_MODES}"
suite_matrix_shape=""
suite_dtype=""
suite_rng_seed=""
suite_total_loop_count=""
suite_warmup_iterations=""
suite_measured_iterations=""

for simd_mode in "${simd_modes[@]}"; do
  make -C "${SCRIPT_DIR}" build SIMD_MODE="${simd_mode}" >/dev/null
  target_path="$(target_path_for_mode "${simd_mode}")"

  if ! matrix_bench_host_supports_simd_mode "${simd_mode}"; then
    printf 'simd_mode=%s\nstatus=unsupported\nrequired_cpu_flags=%s\n' \
      "${simd_mode}" "${MATRIX_BENCH_CPU_FLAGS}" > "${REPORT_DIR}/run-${simd_mode}-unsupported.txt"
    for repeat_idx in $(seq 1 "${REPEATS}"); do
      printf "%s\tunsupported\trun-%s\tNA\tNA\n" "${simd_mode}" "${repeat_idx}" >> "${DATA_PATH}"
    done
    continue
  fi

  for repeat_idx in $(seq 1 "${REPEATS}"); do
    output="$("${target_path}")"
    printf "%s\n" "${output}" > "${REPORT_DIR}/run-${simd_mode}-r${repeat_idx}.txt"

    if [[ -z "${suite_matrix_shape}" ]]; then
      suite_matrix_shape="$(printf "%s\n" "${output}" | awk -F= '/^matrix_shape=/{print $2}')"
      suite_dtype="$(printf "%s\n" "${output}" | awk -F= '/^dtype=/{print $2}')"
      suite_rng_seed="$(printf "%s\n" "${output}" | awk -F= '/^rng_seed=/{print $2}')"
      suite_total_loop_count="$(printf "%s\n" "${output}" | awk -F= '/^total_loop_count=/{print $2}')"
      suite_warmup_iterations="$(printf "%s\n" "${output}" | awk -F= '/^warmup_iterations=/{print $2}')"
      suite_measured_iterations="$(printf "%s\n" "${output}" | awk -F= '/^measured_iterations=/{print $2}')"
    fi

    measured_total_ms="$(printf "%s\n" "${output}" | awk -F= '/^measured_total_ms=/{print $2}')"
    measured_per_inference_us="$(printf "%s\n" "${output}" | awk -F= '/^measured_per_inference_us=/{print $2}')"

    printf "%s\tok\trun-%s\t%s\t%s\n" \
      "${simd_mode}" "${repeat_idx}" "${measured_per_inference_us}" "${measured_total_ms}" >> "${DATA_PATH}"
  done
done

if [[ -z "${suite_matrix_shape}" ]]; then
  printf 'failed to capture single-thread suite metadata from benchmark output\n' >&2
  exit 1
fi

matrix_bench_tsv_to_csv "${DATA_PATH}" "${CSV_PATH}"
export MATRIX_BENCH_SIMD_MODES
export MATRIX_BENCH_CPU_MODEL
export MATRIX_BENCH_CPU_SLUG
export MATRIX_BENCH_PHYSICAL_CORE_COUNT
export MATRIX_BENCH_LOGICAL_CPU_COUNT
export MATRIX_BENCH_HAS_SMT
export MATRIX_BENCH_COMPILER_DEFAULT_MARCH
export MATRIX_BENCH_COMPILER_DEFAULT_MTUNE
export MATRIX_BENCH_COMPILER_DEFAULT_SIMD_LEVEL
MATRIX_BENCH_META_PATH="${META_PATH}" \
MATRIX_BENCH_META_SUITE="single-thread" \
MATRIX_BENCH_META_CSV_NAME="${CSV_NAME}" \
MATRIX_BENCH_META_MATRIX_SHAPE="${suite_matrix_shape}" \
MATRIX_BENCH_META_DTYPE="${suite_dtype}" \
MATRIX_BENCH_META_RNG_SEED="${suite_rng_seed}" \
MATRIX_BENCH_META_TOTAL_LOOP_COUNT="${suite_total_loop_count}" \
MATRIX_BENCH_META_WARMUP_ITERATIONS="${suite_warmup_iterations}" \
MATRIX_BENCH_META_MEASURED_ITERATIONS="${suite_measured_iterations}" \
MATRIX_BENCH_META_REPEAT_COUNT="${REPEATS}" \
python3 - <<'PY'
import json
import os
from pathlib import Path

mode_defs = {
    "scalar": "SCALAR disables Eigen vectorization, compiler tree vectorization, and AVX-family targets.",
    "sse42": "SSE4.2 uses explicit -msse4.2 and disables AVX-family targets.",
    "avx128": "AVX means explicit AVX1 (-mavx) with avx2, fma, and avx512* disabled.",
    "avx256": "AVX2 uses explicit -mavx2 -mfma.",
}
mode_labels = {
    "scalar": "SCALAR",
    "sse42": "SSE4.2",
    "avx128": "AVX",
    "avx256": "AVX2",
}
mode_order = os.environ["MATRIX_BENCH_SIMD_MODES"].split(",")
meta = {
    "suite": os.environ["MATRIX_BENCH_META_SUITE"],
    "cpu_model": os.environ["MATRIX_BENCH_CPU_MODEL"],
    "cpu_slug": os.environ["MATRIX_BENCH_CPU_SLUG"],
    "physical_core_count": int(os.environ["MATRIX_BENCH_PHYSICAL_CORE_COUNT"]),
    "logical_cpu_count": int(os.environ["MATRIX_BENCH_LOGICAL_CPU_COUNT"]),
    "has_smt": os.environ["MATRIX_BENCH_HAS_SMT"] == "1",
    "compiler_default": {
        "march": os.environ["MATRIX_BENCH_COMPILER_DEFAULT_MARCH"],
        "mtune": os.environ["MATRIX_BENCH_COMPILER_DEFAULT_MTUNE"],
        "simd_level": os.environ["MATRIX_BENCH_COMPILER_DEFAULT_SIMD_LEVEL"],
    },
    "benchmark": {
        "matrix_shape": os.environ["MATRIX_BENCH_META_MATRIX_SHAPE"],
        "dtype": os.environ["MATRIX_BENCH_META_DTYPE"],
        "rng_seed": int(os.environ["MATRIX_BENCH_META_RNG_SEED"]),
        "total_loop_count": int(os.environ["MATRIX_BENCH_META_TOTAL_LOOP_COUNT"]),
        "warmup_iterations": int(os.environ["MATRIX_BENCH_META_WARMUP_ITERATIONS"]),
        "measured_iterations": int(os.environ["MATRIX_BENCH_META_MEASURED_ITERATIONS"]),
        "repeat_count": int(os.environ["MATRIX_BENCH_META_REPEAT_COUNT"]),
    },
    "csv_filename": os.environ["MATRIX_BENCH_META_CSV_NAME"],
    "simd_modes": [
        {
            "name": name,
            "display_label": mode_labels[name],
            "definition": mode_defs[name],
        }
        for name in mode_order
    ],
}
Path(os.environ["MATRIX_BENCH_META_PATH"]).write_text(
    json.dumps(meta, indent=2) + "\n",
    encoding="utf-8",
)
PY
"${SCRIPT_DIR}/generate-report.sh" >/dev/null

printf "report_path=%s\n" "${REPORT_PATH}"
printf "data_path=%s\n" "${DATA_PATH}"
printf "csv_path=%s\n" "${CSV_PATH}"
printf "meta_path=%s\n" "${META_PATH}"
printf "plotting_tool=local-matplotlib\n"
