#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
readonly PYTHON_BIN="${PYTHON_BIN:-${HOME}/anaconda3/bin/python}"

"${PYTHON_BIN}" "${SCRIPT_DIR}/render-published-results.py" --repo-root "${ROOT_DIR}" "$@"
