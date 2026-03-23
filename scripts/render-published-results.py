#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import textwrap
from dataclasses import dataclass
from pathlib import Path
from typing import Literal

import matplotlib
import matplotlib.pyplot as plt
import pandas as pd


matplotlib.rcParams["font.family"] = "sans-serif"
matplotlib.rcParams["font.sans-serif"] = ["Helvetica", "Arial", "DejaVu Sans"]
matplotlib.rcParams["axes.unicode_minus"] = False


SuiteName = Literal["single-thread", "multi-thread"]


@dataclass(frozen=True)
class CompilerDefault:
    march: str
    mtune: str
    simd_level: str


@dataclass(frozen=True)
class BenchmarkConfig:
    matrix_shape: str
    dtype: str
    rng_seed: int
    total_loop_count: int
    warmup_iterations: int
    measured_iterations: int
    repeat_count: int
    thread_counts: list[int] | None = None
    binding_policy: str | None = None


@dataclass(frozen=True)
class SimdMode:
    name: str
    display_label: str
    definition: str


@dataclass(frozen=True)
class SuiteMeta:
    suite: SuiteName
    cpu_model: str
    cpu_slug: str
    physical_core_count: int
    logical_cpu_count: int
    has_smt: bool
    compiler_default: CompilerDefault
    benchmark: BenchmarkConfig
    csv_filename: str
    simd_modes: list[SimdMode]
    meta_path: Path
    report_dir: Path


@dataclass(frozen=True)
class PublishedArtifact:
    markdown_keys: list[str]
    filename: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Render published benchmark plots from raw CSV + metadata artifacts.",
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root. Defaults to the parent of this script.",
    )
    parser.add_argument(
        "--report-dir",
        action="append",
        type=Path,
        default=[],
        help=(
            "External report directory containing single-thread-meta-*.json or "
            "thread-scaling-meta-*.json. May be passed multiple times."
        ),
    )
    return parser.parse_args()


def load_suite_meta(meta_path: Path) -> SuiteMeta:
    raw = json.loads(meta_path.read_text(encoding="utf-8"))
    compiler_default = CompilerDefault(
        march=raw["compiler_default"]["march"],
        mtune=raw["compiler_default"]["mtune"],
        simd_level=raw["compiler_default"]["simd_level"],
    )
    benchmark = BenchmarkConfig(
        matrix_shape=raw["benchmark"]["matrix_shape"],
        dtype=raw["benchmark"]["dtype"],
        rng_seed=int(raw["benchmark"]["rng_seed"]),
        total_loop_count=int(raw["benchmark"]["total_loop_count"]),
        warmup_iterations=int(raw["benchmark"]["warmup_iterations"]),
        measured_iterations=int(raw["benchmark"]["measured_iterations"]),
        repeat_count=int(raw["benchmark"]["repeat_count"]),
        thread_counts=[
            int(value) for value in raw["benchmark"].get("thread_counts", [])
        ]
        or None,
        binding_policy=raw["benchmark"].get("binding_policy"),
    )
    simd_modes = [
        SimdMode(
            name=published_mode_name(item["name"]),
            display_label=published_mode_label(item["name"]),
            definition=published_mode_definition(item["name"]),
        )
        for item in raw["simd_modes"]
    ]
    suite = raw["suite"]
    if suite not in ("single-thread", "multi-thread"):
        raise ValueError(f"unsupported suite in {meta_path}: {suite}")
    return SuiteMeta(
        suite=suite,
        cpu_model=raw["cpu_model"],
        cpu_slug=raw["cpu_slug"],
        physical_core_count=int(raw["physical_core_count"]),
        logical_cpu_count=int(raw["logical_cpu_count"]),
        has_smt=bool(raw["has_smt"]),
        compiler_default=compiler_default,
        benchmark=benchmark,
        csv_filename=raw["csv_filename"],
        simd_modes=simd_modes,
        meta_path=meta_path,
        report_dir=meta_path.parent,
    )


def copy_artifacts(meta: SuiteMeta, output_dir: Path) -> Path:
    source_csv = meta.report_dir / meta.csv_filename
    target_csv = output_dir / source_csv.name
    target_meta = output_dir / meta.meta_path.name
    csv_df = pd.read_csv(source_csv)
    csv_df["simd_mode"] = csv_df["simd_mode"].map(published_mode_name)
    csv_df.to_csv(target_csv, index=False)

    raw_meta = json.loads(meta.meta_path.read_text(encoding="utf-8"))
    for item in raw_meta["simd_modes"]:
        raw_name = item["name"]
        item["name"] = published_mode_name(raw_name)
        item["display_label"] = published_mode_label(raw_name)
        item["definition"] = published_mode_definition(raw_name)
    target_meta.write_text(json.dumps(raw_meta, indent=2) + "\n", encoding="utf-8")
    return target_csv


def artifact_stem(meta: SuiteMeta) -> str:
    if meta.suite == "single-thread":
        return f"single-thread-summary-{meta.cpu_slug}"
    return f"thread-scaling-avx2-{meta.cpu_slug}"


def artifact_markdown_keys(meta: SuiteMeta) -> list[str]:
    if meta.suite == "single-thread":
        return [f"single-thread-summary-{meta.cpu_slug}"]
    return [
        f"thread-scaling-avx2-{meta.cpu_slug}",
        f"thread-scaling-avx256-{meta.cpu_slug}",
    ]


def artifact_hash(meta: SuiteMeta, csv_path: Path, renderer_path: Path) -> str:
    digest = hashlib.sha256()
    digest.update(renderer_path.read_bytes())
    digest.update(csv_path.read_bytes())
    digest.update(meta.meta_path.read_bytes())
    return digest.hexdigest()[:10]


def cleanup_prior_pngs(output_dir: Path, stems: list[str]) -> None:
    for stem in stems:
        for path in output_dir.glob(f"{stem}-*.png"):
            path.unlink()
        legacy_path = output_dir / f"{stem}.png"
        if legacy_path.exists():
            legacy_path.unlink()


def output_png_path(meta: SuiteMeta, output_dir: Path, csv_path: Path, renderer_path: Path) -> Path:
    stem = artifact_stem(meta)
    cleanup_prior_pngs(output_dir, artifact_markdown_keys(meta))
    return output_dir / f"{stem}-{artifact_hash(meta, csv_path, renderer_path)}.png"


def read_csv(meta: SuiteMeta, output_dir: Path) -> pd.DataFrame:
    csv_path = output_dir / meta.csv_filename
    return pd.read_csv(csv_path)


def published_mode_name(raw_name: str) -> str:
    return {
        "avx128": "avx",
        "avx256": "avx2",
    }.get(raw_name, raw_name)


def published_mode_label(raw_name: str) -> str:
    return {
        "scalar": "SCALAR",
        "sse42": "SSE4.2",
        "avx128": "AVX",
        "avx256": "AVX2",
    }[raw_name]


def published_mode_definition(raw_name: str) -> str:
    return {
        "scalar": "SCALAR disables Eigen vectorization, compiler tree vectorization, and AVX-family targets.",
        "sse42": "SSE4.2 uses explicit -msse4.2 and disables AVX-family targets.",
        "avx128": "AVX uses explicit AVX1 (-mavx) with avx2, fma, and avx512* disabled.",
        "avx256": "AVX2 uses explicit -mavx2 -mfma.",
    }[raw_name]


def rewrite_markdown_refs(repo_root: Path, artifacts: list[PublishedArtifact]) -> None:
    for relative_markdown_path in ("README.md", "docs/benchmark-results.md"):
        markdown_path = repo_root / relative_markdown_path
        text = markdown_path.read_text(encoding="utf-8")
        original_text = text
        for artifact in artifacts:
            prefix = "docs/results/" if relative_markdown_path == "README.md" else "results/"
            replacement = f"{prefix}{artifact.filename}"
            for artifact_key in artifact.markdown_keys:
                text = replace_artifact_ref(
                    text=text,
                    artifact_key=artifact_key,
                    replacement=replacement,
                )
        if text != original_text:
            markdown_path.write_text(text, encoding="utf-8")


def replace_artifact_ref(text: str, artifact_key: str, replacement: str) -> str:
    import re

    pattern = rf"(docs/results/|results/){re.escape(artifact_key)}(?:-[0-9a-f]{{10}})?\.png"
    return re.sub(pattern, replacement, text)


def color_for_mode(mode_name: str) -> str:
    return {
        "scalar": "#8b9099",
        "sse42": "#3c7f5d",
        "avx": "#3f88c5",
        "avx2": "#285c9a",
    }[mode_name]


def style_axes(ax: plt.Axes) -> None:
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.grid(True, axis="y", alpha=0.2, linewidth=0.6)
    ax.set_axisbelow(True)


def annotate_bars(ax: plt.Axes, bars: list[matplotlib.patches.Rectangle], values: list[float], suffix: str) -> None:
    for bar, value in zip(bars, values, strict=True):
        ax.text(
            bar.get_x() + bar.get_width() / 2.0,
            bar.get_height(),
            f"{value:.0f} {suffix}",
            ha="center",
            va="bottom",
            fontsize=8,
            color="#3f3f46",
        )


def add_footnote(fig: plt.Figure, lines: list[str]) -> None:
    wrapped_lines: list[str] = []
    for line in lines:
        wrapped_lines.extend(textwrap.wrap(line, width=130) or [""])
    fig.text(
        0.5,
        0.02,
        "\n".join(wrapped_lines),
        ha="center",
        va="bottom",
        fontsize=7,
        color="#6b7280",
    )


def render_single_thread(meta: SuiteMeta, df: pd.DataFrame, output_path: Path) -> None:
    mode_order = [mode.name for mode in meta.simd_modes]
    display_labels = [mode.display_label for mode in meta.simd_modes]
    means: list[float] = []
    colors: list[str] = []
    for mode in meta.simd_modes:
        subset = df[(df["simd_mode"] == mode.name) & (df["status"] == "ok")]
        means.append(float(subset["measured_per_inference_us"].mean()))
        colors.append(color_for_mode(mode.name))

    fig, ax = plt.subplots(figsize=(10.5, 7.0))
    bars = ax.bar(display_labels, means, color=colors, edgecolor="#233142", linewidth=0.8, width=0.58)
    style_axes(ax)
    ax.set_ylabel("Mean per-inference latency (us)")
    ax.set_xlabel("SIMD Mode")
    ax.set_title(f"Eigen F32 Single-thread Summary by SIMD Mode | {meta.cpu_model}", pad=12)
    annotate_bars(ax, list(bars), means, "us")

    mode_definition_text = " | ".join(
        f"{mode.display_label} = {mode.definition}"
        for mode in meta.simd_modes
    )
    add_footnote(
        fig,
        [
            "What: mean single-thread per-inference latency by explicit SIMD mode for 256x256 float32 matrix multiply; lower is better.",
            (
                f"Data: Linux raw CSV {meta.csv_filename} with {meta.benchmark.repeat_count} runs per mode; "
                f"compiler default inferred as {meta.compiler_default.simd_level} "
                f"({meta.compiler_default.march} / {meta.compiler_default.mtune})."
            ),
            f"How to read: one bar per mode in fixed order; values above bars are mean latency. {mode_definition_text}",
        ],
    )
    fig.tight_layout(rect=(0, 0.17, 1, 0.96))
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def render_multi_thread(meta: SuiteMeta, df: pd.DataFrame, output_path: Path) -> None:
    subset = df[(df["simd_mode"] == "avx2") & (df["status"] == "ok")].copy()
    subset["participant_count"] = subset["participant_count"].astype(int)
    subset["measured_per_inference_us"] = subset["measured_per_inference_us"].astype(float)
    subset["uses_hyper_threads"] = subset["uses_hyper_threads"].astype(int)
    subset = subset.sort_values("participant_count")

    labels = [f"j={value}" for value in subset["participant_count"].tolist()]
    values = subset["measured_per_inference_us"].tolist()
    colors = ["#d97706" if value == 1 else "#285c9a" for value in subset["uses_hyper_threads"].tolist()]
    hatches = ["//" if value == 1 else "" for value in subset["uses_hyper_threads"].tolist()]

    fig, ax = plt.subplots(figsize=(10.5, 7.0))
    bars = ax.bar(labels, values, color=colors, edgecolor="#233142", linewidth=0.8, width=0.58)
    for bar, hatch in zip(bars, hatches, strict=True):
        if hatch:
            bar.set_hatch(hatch)
    style_axes(ax)
    ax.set_ylabel("Per-inference latency (us)")
    ax.set_xlabel("Total compute participants (-j, main thread included)")
    ax.set_title(f"Eigen F32 Multi-thread Scaling | AVX2 | {meta.cpu_model}", pad=12)
    annotate_bars(ax, list(bars), values, "us")

    smt_note = (
        "orange hatched bar = SMT sibling path"
        if meta.has_smt and subset["uses_hyper_threads"].any()
        else "all bars = physical-core path"
    )
    add_footnote(
        fig,
        [
            "What: best multi-thread per-inference latency for the explicit AVX2 build across thread counts; lower is better.",
            (
                f"Data: Linux raw CSV {meta.csv_filename} with {meta.benchmark.repeat_count} process runs per thread count; "
                f"matrix = {meta.benchmark.matrix_shape} {meta.benchmark.dtype}; "
                f"compiler default inferred as {meta.compiler_default.simd_level}."
            ),
            f"How to read: j = total compute participants including the main thread. AVX2 = explicit -mavx2 -mfma. {smt_note}.",
        ],
    )
    fig.tight_layout(rect=(0, 0.14, 1, 0.96))
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def find_meta_files(repo_root: Path, suite_dir_name: str, pattern: str) -> list[Path]:
    suite_root = repo_root / suite_dir_name
    report_dirs = sorted(path for path in suite_root.glob("reports*") if path.is_dir())
    meta_paths: list[Path] = []
    for report_dir in report_dirs:
        meta_paths.extend(sorted(report_dir.glob(pattern)))
    return meta_paths


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    renderer_path = Path(__file__).resolve()
    output_dir = repo_root / "docs" / "results"
    output_dir.mkdir(parents=True, exist_ok=True)

    meta_paths: list[Path] = []
    for report_dir in args.report_dir:
        resolved_report_dir = report_dir.resolve()
        meta_paths.extend(sorted(resolved_report_dir.glob("single-thread-meta-*.json")))
        meta_paths.extend(sorted(resolved_report_dir.glob("thread-scaling-meta-*.json")))

    if not meta_paths:
        meta_paths = find_meta_files(
            repo_root,
            "eigen-f32-single-thread",
            "single-thread-meta-*.json",
        )
        meta_paths.extend(
            find_meta_files(
                repo_root,
                "eigen-f32-multi-thread",
                "thread-scaling-meta-*.json",
            )
        )

    if not meta_paths:
        raise SystemExit("no metadata files found under benchmark report directories")

    written_paths: list[Path] = []
    written_artifacts: list[PublishedArtifact] = []
    for meta_path in meta_paths:
        meta = load_suite_meta(meta_path)
        copied_csv_path = copy_artifacts(meta, output_dir)
        df = read_csv(meta, output_dir)
        png_path = output_png_path(meta, output_dir, copied_csv_path, renderer_path)
        if meta.suite == "single-thread":
            render_single_thread(meta, df, png_path)
        else:
            render_multi_thread(meta, df, png_path)
        written_paths.append(png_path)
        written_artifacts.append(
            PublishedArtifact(
                markdown_keys=artifact_markdown_keys(meta),
                filename=png_path.name,
            )
        )

    rewrite_markdown_refs(repo_root, written_artifacts)

    for path in written_paths:
        print(f"plot_path={path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
