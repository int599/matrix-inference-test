#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_DIR="${SCRIPT_DIR}"
readonly EXT_DIR="${REPO_DIR}/ext"
readonly EIGEN_VERSION="3.4.0"
readonly EIGEN_DIR="${EXT_DIR}/eigen-${EIGEN_VERSION}"
readonly ARCHIVE_NAME="eigen-${EIGEN_VERSION}.zip"
readonly ARCHIVE_PATH="${EXT_DIR}/${ARCHIVE_NAME}"
readonly DOWNLOAD_URL="https://gitlab.com/libeigen/eigen/-/archive/${EIGEN_VERSION}/${ARCHIVE_NAME}"

mkdir -p "${EXT_DIR}"

if [[ -f "${EIGEN_DIR}/Eigen/Dense" ]]; then
  echo "Eigen ${EIGEN_VERSION} already present at ${EIGEN_DIR}"
  exit 0
fi

echo "Downloading ${DOWNLOAD_URL}"
curl -L --fail --output "${ARCHIVE_PATH}" "${DOWNLOAD_URL}"

rm -rf "${EIGEN_DIR}"
unzip -q "${ARCHIVE_PATH}" -d "${EXT_DIR}"

if [[ ! -f "${EIGEN_DIR}/Eigen/Dense" ]]; then
  echo "Eigen archive extracted, but ${EIGEN_DIR}/Eigen/Dense was not found" >&2
  exit 1
fi

echo "Eigen ${EIGEN_VERSION} ready at ${EIGEN_DIR}"
