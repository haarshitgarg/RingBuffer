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

## Performance Analysis

Performance testing is critical for HFT workloads. All measurements use `-O3 -DNDEBUG` Release builds with CPU affinity pinning to ensure consistency.

> **Important:** Thermal throttling and CPU frequency scaling significantly impact results. All measurements taken at **fixed CPU frequency (~3.8 GHz when plugged in, ~2.2 GHz on battery)**. Lock frequency using `cpufreq-set` for reproducible results.

### Branch Comparison: Thread-Local Caching & Cache-Line Padding Impact

#### Analysis Branch (`6accd1ec`) vs Main Branch (`01caf71`)

This comparison demonstrates the impact of moving from **shared member variables** to **thread-local static caching** combined with **explicit cache-line padding**.

**Change Summary:**
- Moved `cachedReadIdx_` and `cachedWriteIdx_` from instance members to **`static thread_local`** variables
- Added explicit **64-byte padding** (`pad1`, `pad2`, `pad3`) to prevent false sharing on atomic indices
- Removed unnecessary HITM (cache-to-cache transfers) between producer and consumer threads

##### Performance Metrics (CPU 0, 1 - Shared L3 Cache)

| Metric | Main | Analysis | Improvement |
|--------|------|----------|-------------|
| **Throughput (ops/s)** | 285.6M | 725.4M | **+154% ↑** |
| **IPC (insn/cycle)** | 1.04 | 2.47 | **+137% ↑** |
| **Branch Misses** | 123.5M | 20.5M | **-83.4% ↓** |
| **Branch Miss Rate** | 5.48% | 0.99% | **-4.49 pp ↓** |
| **Total Cycles** | 19.7B | 7.7B | **-61% ↓** |

##### Code-Level Analysis: Why This Matters

**Main Branch Issue: Shared Member Variables**
```cpp
// Instance member - shared between threads in same cache line
class RingBuffer {
private:
  size_t cachedReadIdx_;   // ← Both threads contend here
  size_t cachedWriteIdx_;  // ← Cache coherency traffic (HITM)
```

Problem: Both producer and consumer threads update these variables from the same memory location. Even though they update *different* variables, they can share a cache line (typically 64 bytes), causing:
- **HITM (cache-to-cache transfers)** between cores
- **Branch predictor pollution** when cache state changes
- **Stalls on atomic loads** waiting for coherency protocol

**Analysis Branch Solution: Thread-Local Static**
```cpp
// Thread-local - completely separate memory per thread
bool pop(T &val) {
  static thread_local size_t cachedWriteIdx = 0;  // ← Per-thread copy
  size_t readIdx = readIdx_.load(memory_order_relaxed);
  // ...
}
```

Benefits:
- **Zero cross-thread memory traffic** for the cached indices
- **Perfect branch prediction** within each thread (consistent patterns)
- **Reduced cache coherency overhead** - only the true shared atomics (`readIdx_`, `writeIdx_`) cause coherency
- **Better CPU cache utilization** - each thread's working set shrinks

**Explicit Cache-Line Padding**
```cpp
alignas(64) atomic<size_t> readIdx_;   // Consumer only updates this
char pad1[64];                          // ← Forces next atomic to different cache line

alignas(64) atomic<size_t> writeIdx_;  // Producer only updates this
char pad2[64];                          // ← Prevents false sharing
```

Even though `readIdx_` and `writeIdx_` are updated by different threads (no real data race), they can *still* cause stalls if they share a cache line. The padding ensures each atomic gets its own dedicated cache line.

##### Why IPC Increased 137%

The **Instructions Per Cycle** jump from 1.04 → 2.47 reveals why throughput scales so dramatically:

1. **Fewer cache misses** - Thread-local data is always cache-hot
2. **Better branch prediction** - 5.48% miss rate → 0.99% miss rate means the CPU almost never stalls for branch misprediction flushes
3. **Reduced memory stalls** - No HITM traffic waiting for coherency

## Key Insights for HFT Implementation

### 1. **False Sharing is Invisible but Deadly**
Even though `readIdx_` and `writeIdx_` are logically separate, they can share a 64-byte cache line, causing expensive coherency traffic. The analysis branch's explicit padding ensures each atomic gets dedicated cache real estate.

### 2. **Cached Index Pattern is Critical**
The SPSC queue maintains thread-local copies of remote indices (`cachedReadIdx`, `cachedWriteIdx`) to minimize atomic operations. However, **these caches must be thread-local**, not shared instance members, to avoid self-inflicted coherency traffic.

### 3. **Memory Ordering Choices Matter**
- **`memory_order_relaxed`** on the fast-path load: Only synchronization happens once every buffer-full/empty cycle
- **`memory_order_acquire`** on the remote index load: Establishes synchronization boundary when we actually need to check remote state
- **`memory_order_release`** on index stores: Ensures writes to the buffer are visible before the index advances

Using `memory_order_seq_cst` everywhere would incur **massive performance penalties** (~50-70% throughput loss).

### 4. **CPU Affinity & Cache Hierarchy are Non-Negotiable**
- Throughput **scales 6-10x** between CPUs with shared cache vs. separate L1, L2
- Always pin producer/consumer to cores sharing the cache for HFT workloads


