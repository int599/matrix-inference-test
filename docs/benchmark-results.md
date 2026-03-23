---
description: 详细基准结果说明，包含按 CPU 型号归档的单线程重复明细、多线程结果和原始 CSV 指针。
---

# 详细基准结果

本文档补充 `README.md` 中省略的详细结果，主要包含：

- 单线程重复明细
- 多线程缩放结果
- 原始 CSV / metadata 指针

## Intel Xeon E-2488

- Hosted compiler default 推断为 `-march=x86-64-v2`、`-mtune=generic`、SIMD level `SSE4.2`。
- 原始数据：
  - [single-thread-data-intel-xeon-e-e-2488.csv](results/single-thread-data-intel-xeon-e-e-2488.csv)
  - [thread-scaling-data-intel-xeon-e-e-2488.csv](results/thread-scaling-data-intel-xeon-e-e-2488.csv)
  - [single-thread-meta-intel-xeon-e-e-2488.json](results/single-thread-meta-intel-xeon-e-e-2488.json)
  - [thread-scaling-meta-intel-xeon-e-e-2488.json](results/thread-scaling-meta-intel-xeon-e-e-2488.json)

### 单线程汇总

| SIMD 模式 | 均值 |
|---|---:|
| `SCALAR` | `2989 us` |
| `SSE4.2` | `889 us` |
| `AVX` | `424 us` |
| `AVX2` | `320 us` |

![Intel 单线程汇总](results/single-thread-summary-intel-xeon-e-e-2488-4feb3bd472.png)

### 单线程重复明细

| SIMD 模式 | run | 单次 inference 延迟 |
|---|---|---:|
| `SCALAR` | run-1 | 2977 us |
| `SCALAR` | run-2 | 2980 us |
| `SCALAR` | run-3 | 2999 us |
| `SCALAR` | run-4 | 3001 us |
| `SCALAR` | run-5 | 2989 us |
| `SSE4.2` | run-1 | 888 us |
| `SSE4.2` | run-2 | 890 us |
| `SSE4.2` | run-3 | 889 us |
| `SSE4.2` | run-4 | 888 us |
| `SSE4.2` | run-5 | 888 us |
| `AVX` | run-1 | 424 us |
| `AVX` | run-2 | 424 us |
| `AVX` | run-3 | 424 us |
| `AVX` | run-4 | 425 us |
| `AVX` | run-5 | 425 us |
| `AVX2` | run-1 | 319 us |
| `AVX2` | run-2 | 319 us |
| `AVX2` | run-3 | 321 us |
| `AVX2` | run-4 | 320 us |
| `AVX2` | run-5 | 319 us |

### 多线程缩放

| SIMD 模式 | `-j` | 单次 inference 延迟 | 路径 |
|---|---:|---:|---|
| `SCALAR` | 1 | 3012 us | physical-core path |
| `SCALAR` | 2 | 1514 us | physical-core path |
| `SCALAR` | 4 | 773 us | physical-core path |
| `SCALAR` | 8 | 385 us | physical-core path |
| `SCALAR` | 16 | 417 us | SMT sibling path |
| `SSE4.2` | 1 | 888 us | physical-core path |
| `SSE4.2` | 2 | 432 us | physical-core path |
| `SSE4.2` | 4 | 233 us | physical-core path |
| `SSE4.2` | 8 | 127 us | physical-core path |
| `SSE4.2` | 16 | 125 us | SMT sibling path |
| `AVX` | 1 | 425 us | physical-core path |
| `AVX` | 2 | 203 us | physical-core path |
| `AVX` | 4 | 114 us | physical-core path |
| `AVX` | 8 | 74 us | physical-core path |
| `AVX` | 16 | 72 us | SMT sibling path |
| `AVX2` | 1 | 321 us | physical-core path |
| `AVX2` | 2 | 156 us | physical-core path |
| `AVX2` | 4 | 91 us | physical-core path |
| `AVX2` | 8 | 63 us | physical-core path |
| `AVX2` | 16 | 59 us | SMT sibling path |

![Intel 多线程 AVX2 缩放](results/thread-scaling-avx2-intel-xeon-e-e-2488-6ff5d0e179.png)

补充说明：

- 该 CPU 有 `8` 个物理核心、`16` 个逻辑 CPU。
- `-j 16` 会进入 SMT sibling path。
- `SSE4.2` 对应当前 hosted compiler default 推断出的 SIMD level。
- `AVX` 表示显式 AVX1，不包含 `avx2` / `fma`。

## AMD Ryzen 9 7950X 16-Core Processor

- Hosted compiler default 同样推断为 `-march=x86-64-v2`、`-mtune=generic`、SIMD level `SSE4.2`。
- 原始数据：
  - [single-thread-data-amd-ryzen-9-7950x-16-core.csv](results/single-thread-data-amd-ryzen-9-7950x-16-core.csv)
  - [thread-scaling-data-amd-ryzen-9-7950x-16-core.csv](results/thread-scaling-data-amd-ryzen-9-7950x-16-core.csv)
  - [single-thread-meta-amd-ryzen-9-7950x-16-core.json](results/single-thread-meta-amd-ryzen-9-7950x-16-core.json)
  - [thread-scaling-meta-amd-ryzen-9-7950x-16-core.json](results/thread-scaling-meta-amd-ryzen-9-7950x-16-core.json)

### 单线程汇总

| SIMD 模式 | 均值 |
|---|---:|
| `SCALAR` | `2070 us` |
| `SSE4.2` | `692 us` |
| `AVX` | `327 us` |
| `AVX2` | `286 us` |

![AMD 单线程汇总](results/single-thread-summary-amd-ryzen-9-7950x-16-core-c3145ab0d4.png)

### 单线程重复明细

| SIMD 模式 | run | 单次 inference 延迟 |
|---|---|---:|
| `SCALAR` | run-1 | 2068 us |
| `SCALAR` | run-2 | 2071 us |
| `SCALAR` | run-3 | 2063 us |
| `SCALAR` | run-4 | 2071 us |
| `SCALAR` | run-5 | 2075 us |
| `SSE4.2` | run-1 | 688 us |
| `SSE4.2` | run-2 | 694 us |
| `SSE4.2` | run-3 | 693 us |
| `SSE4.2` | run-4 | 693 us |
| `SSE4.2` | run-5 | 690 us |
| `AVX` | run-1 | 327 us |
| `AVX` | run-2 | 329 us |
| `AVX` | run-3 | 326 us |
| `AVX` | run-4 | 328 us |
| `AVX` | run-5 | 325 us |
| `AVX2` | run-1 | 288 us |
| `AVX2` | run-2 | 285 us |
| `AVX2` | run-3 | 286 us |
| `AVX2` | run-4 | 286 us |
| `AVX2` | run-5 | 286 us |

### 多线程缩放

| SIMD 模式 | `-j` | 单次 inference 延迟 | 路径 |
|---|---:|---:|---|
| `SCALAR` | 1 | 2088 us | physical-core path |
| `SCALAR` | 2 | 1052 us | physical-core path |
| `SCALAR` | 4 | 544 us | physical-core path |
| `SCALAR` | 8 | 311 us | physical-core path |
| `SCALAR` | 16 | 185 us | physical-core path |
| `SSE4.2` | 1 | 693 us | physical-core path |
| `SSE4.2` | 2 | 315 us | physical-core path |
| `SSE4.2` | 4 | 171 us | physical-core path |
| `SSE4.2` | 8 | 101 us | physical-core path |
| `SSE4.2` | 16 | 72 us | physical-core path |
| `AVX` | 1 | 329 us | physical-core path |
| `AVX` | 2 | 132 us | physical-core path |
| `AVX` | 4 | 77 us | physical-core path |
| `AVX` | 8 | 55 us | physical-core path |
| `AVX` | 16 | 51 us | physical-core path |
| `AVX2` | 1 | 290 us | physical-core path |
| `AVX2` | 2 | 125 us | physical-core path |
| `AVX2` | 4 | 73 us | physical-core path |
| `AVX2` | 8 | 53 us | physical-core path |
| `AVX2` | 16 | 44 us | physical-core path |

![AMD 多线程 AVX2 缩放](results/thread-scaling-avx2-amd-ryzen-9-7950x-16-core-c70fa73ac7.png)

补充说明：

- 该 CPU 在当前拓扑下表现为 `16` 个物理核心、`16` 个逻辑 CPU。
- 因此 `-j 16` 仍然属于 physical-core path。
- `SSE4.2` 对应当前 hosted compiler default 推断出的 SIMD level。
- `AVX` 和 `AVX2` 的差距在单线程更明显，在 `-j 8` / `-j 16` 时已经收敛到很接近。
