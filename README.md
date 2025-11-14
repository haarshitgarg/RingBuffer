# Ring Buffer

Why am I making this? The answer is simple. I  was doomscrolling youtube and watched a video on facinating ways to optimise performance. People go to insane lengths to get as much performance as possible and it was facinating. So, here I am making a Ring Buffer after watching a few lectures from cpp con

## MY PC

Pasting the `lscpu --extended` for future reference on how I should test.

> I can see that CPU pairs **(0, 1)** and **(2, 3)** share all caches — the ones with the best potential performance based on MHz.

| CPU | Node | Socket | Core | L1d:L1i:L2:L3 | Online | Max MHz | Min MHz | Current MHz |
|-----|------|--------|------|---------------|--------|---------|---------|-------------|
| 0   | 0    | 0      | 0    | 0:0:0:0       | yes    | 5000.0  | 400.0   | 527.9       |
| 1   | 0    | 0      | 0    | 0:0:0:0       | yes    | 5000.0  | 400.0   | 473.6       |
| 2   | 0    | 0      | 1    | 4:4:1:0       | yes    | 5000.0  | 400.0   | 1129.5      |
| 3   | 0    | 0      | 1    | 4:4:1:0       | yes    | 5000.0  | 400.0   | 400.0       |
| 4   | 0    | 0      | 2    | 8:8:2:0       | yes    | 3800.0  | 400.0   | 1241.0      |
| 5   | 0    | 0      | 3    | 9:9:2:0       | yes    | 3800.0  | 400.0   | 1238.2      |
| 6   | 0    | 0      | 4    | 10:10:2:0     | yes    | 3800.0  | 400.0   | 1258.6      |
| 7   | 0    | 0      | 5    | 11:11:2:0     | yes    | 3800.0  | 400.0   | 1277.0      |
| 8   | 0    | 0      | 6    | 12:12:3:0     | yes    | 3800.0  | 400.0   | 420.5       |
| 9   | 0    | 0      | 7    | 13:13:3:0     | yes    | 3800.0  | 400.0   | 438.1       |
| 10  | 0    | 0      | 8    | 14:14:3:0     | yes    | 3800.0  | 400.0   | 436.1       |
| 11  | 0    | 0      | 9    | 15:15:3:0     | yes    | 3800.0  | 400.0   | 439.9       |

## Performance

I will keep on updating the performance based on commits. Because I want to measure how much I do improve
of-course the core I choose has a huge impact on the performance. So, I will try to measure performance and make inference based on multiple scenarios

### Commit: `db9eedf324f004b436355d49f10aef6d1793dc9a`

Comparing `RingBuffer` (SPSC lock-free implementation with cached indices) against `rigtorp` reference implementation from CppCon 2023 by Charles Frasch.

> **Note:** Results are ballpark figures from multiple runs with minimal fluctuation.

#### CPU 0, 1 (Shared Cache, High Performance)

| Implementation | Throughput (ops/s) |
|---|---|
| RingBuffer | 22,760,155 |
| rigtorp | 16,609,405 |

#### CPU 2, 3 (Shared Cache, High Performance)

| Implementation | Throughput (ops/s) |
|---|---|
| RingBuffer | 23,390,090 |
| rigtorp | 16,751,176 |

**Observation:** Both combinations produce nearly identical results—CPU selection is the only difference. The importance of shared cache is evident.

#### CPU 4, 5 (No Shared L1/L2 Cache)

| Implementation | Throughput (ops/s) |
|---|---|
| RingBuffer | 3,775,031 |
| rigtorp | 3,361,023 |

#### CPU 7, 8 (Mismatched Cache Hierarchy)

| Implementation | Throughput (ops/s) |
|---|---|
| RingBuffer | 2,372,411 |
| rigtorp | 2,522,076 |

**Observation:** Performance degrades significantly when CPUs don't share caches or have mismatched cache hierarchies. Further investigation needed to understand the impact of L2 cache sharing across varying CPU performance tiers.

