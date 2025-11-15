# RingBuffer Performance Analysis: Deep Dive

## Executive Summary

This document provides a comprehensive analysis of the RingBuffer SPSC lock-free queue implementation across multiple optimization stages and hardware scenarios. The key finding is that **moving from shared member variable caching to thread-local static caching, combined with explicit cache-line padding, achieved a 154% throughput improvement (285M → 725M ops/s)** through elimination of inter-thread cache coherency traffic and improvement of branch predictor accuracy. This importance of cache was highlighted when I measure performance where cpus share L1, L2 cache and where they do not

---

## Performance Progression: Commit-by-Commit Analysis

### Benchmark Setup

All benchmarks were performed under the following conditions:

- **CPU Frequency**: Fixed at ~5 GHz (laptop plugged in)
- **Build**: CMake Release mode with `-O3 -DNDEBUG` optimizations
- **Iterations**: Multiple runs for stability confirmation
- **Platform**: 12-core Intel processor (as per lscpu output in README.md)

> **Critical Note**: Thermal throttling and CPU frequency scaling dramatically affect results. All measurements taken at consistent ~5 GHz. Unplugged laptop runs at 2.2 GHz, which reduces throughput by a lot.

### Throughput Summary Table

| Commit | Description | CPU 0-1 (M ops/s) | CPU 2-3 (M ops/s) | CPU 4-5 (M ops/s) | Key Metric |
|--------|-------------|-------------------|-------------------|-------------------|-----------|
| `db9eedf` | Thread Sanitizer (baseline) | 191.5 | 191.7 | 78.3 | baseline |
| `01caf71` | Cached Indices (shared members) | 282.0 | 280.8 | 220.4 | +47% |
| `6accd1e` | Cache-line Padding + Thread-local | 720.1 | 706.0 | 405.2 | +155% vs baseline |

---

## Stage 1: Baseline - Thread Sanitizer (db9eedf)

### What Changed
- Added thread sanitizer to verify no race conditions
- No algorithmic changes

### Performance Characteristics (CPU 0-1)

```
Throughput: 191.5M ops/s
```

### Analysis

This is the baseline SPSC implementation with simple atomic index updates. At this stage, the code has basic synchronization but no optimization for cache coherency or branch prediction.

**CPU Pair Behavior:**
- CPU 0-1: 191.5M ops/s (shared L3 cache)
- CPU 2-3: 191.7M ops/s (shared L3 cache, similar performance)
- CPU 4-5: 78.3M ops/s (no shared L1/L2, significant degradation)

**Key Insight**: The 2.4x performance cliff between CPU pairs with shared L3 (191M) vs. without (78M) reveals that cache hierarchy is **critical** for SPSC queues.

---

## Stage 2: Cached Indices with Shared Member Variables (01caf71)

### What Changed

```cpp
// BEFORE: Atomic-only approach
bool pop(T &val) {
  size_t writeIdx = writeIdx_.load(...);
  if (readIdx_ == writeIdx_) {
    return false;  // Empty
  }
  // ... dequeue
}

// AFTER: Cached index optimization
class RingBuffer {
private:
  size_t cachedReadIdx_;   // ← Instance member (shared between threads!)
  size_t cachedWriteIdx_;  // ← Instance member
```

Added thread-local caching of remote indices to reduce atomic operations. **However**, the caches were stored as **instance member variables**, causing them to share cache lines with the actual atomics.

### Performance Metrics (CPU 0-1)

| Metric | Value | vs. Previous |
|--------|-------|-------------|
| Throughput | 282.0M ops/s | +47% |
| Improvement | +90.5M ops/s | |


### CPU Pair Analysis

- CPU 0-1: 282.0M ops/s
- CPU 2-3: 280.8M ops/s
- CPU 4-5: 220.4M ops/s (+181% improvement over db9eedf!)

**Surprising Finding**: The cached index optimization helped CPU 4-5 much more (+181%) than CPU 0-1 (+47%). Why?

**Root Cause Analysis**: CPUs 4-5 have **no shared L1/L2** but **do share L3**. At baseline (db9eedf), the queues were extremely contended at the L3 level. The cached indices reduced the frequency of L3 accesses, giving CPU 4-5 a massive boost. CPUs 0-1, already enjoying shared L1/L2, saw only incremental gains.

---

## Stage 3: Thread-Local Static Caching + Cache-Line Padding (6accd1e) - Analysis Branch

### What Changed

```cpp
// BEFORE: Instance member caching (false sharing problem)
class RingBuffer {
private:
  size_t cachedReadIdx_;   // ← Shared between threads via same object
  size_t cachedWriteIdx_;  // ← Same issue

// AFTER: Thread-local static caching (no sharing!)
bool pop(T &val) {
  static thread_local size_t cachedWriteIdx = 0;  // ← Per-thread copy on stack

// ALSO: Explicit cache-line padding to prevent coherency on atomics
class RingBuffer {
private:
  alignas(64) atomic<size_t> readIdx_;
  char pad1[64];  // ← Forces readIdx_ and writeIdx_ to different cache lines

  alignas(64) atomic<size_t> writeIdx_;
  char pad2[64];
```

Two critical optimizations:
1. **Thread-local static caching**: Moves cached indices from shared object memory to per-thread stack/TLS
2. **Explicit cache-line padding**: Ensures atomics don't share cache lines

### Performance Metrics (CPU 0-1)

| Metric | Value | vs. Stage 2 | vs. Baseline |
|--------|-------|----------|-----------|
| Throughput | 720.1M ops/s | +155% | +376% |
| IPC (insn/cycle) | 2.47 | +137% | |
| Branch Miss Rate | 0.99% | -5.48pp | |
| Branch Misses | 20.5M | -83.4% | |

### Deep Analysis: Why 155% Improvement?

#### 1. **Elimination of Cross-Thread Memory Traffic**

**Before (Shared Members):**
```
Producer Thread          Shared Memory          Consumer Thread
    |                         |                        |
    |--- Write cachedReadIdx --X-- Read cachedWriteIdx |
    |                    (HITM!)                       |
    |                    STALL                         |
```

The producer's update to `cachedReadIdx_` invalidates the cache line in the consumer's L1/L2. The consumer must:
1. Wait for the invalidation
2. Reload from L3 or memory
3. This causes pipeline stalls and branch predictor pollution

**After (Thread-Local):**
```
Producer Thread (local stack)    Shared Atomics    Consumer Thread (local stack)
    |                                 |                   |
    | (no interaction with consumer's cache)               |
    |--- Read/Write shared atomics only when necessary ---|
```

Thread-local data never causes coherency traffic. Each thread only touches the shared atomics when its cache misses (rare case).

#### 2. **Cache-Line Padding Impact**

Even though `readIdx_` and `writeIdx_` are logically separate (different threads access different ones), they can still share a 64-byte cache line:

**Before Padding:**
```
Shared Memory Layout:
[readIdx_ (8 bytes)]  [writeIdx_ (8 bytes)]  [other data...]
← Same 64-byte cache line →
```

Result: Producer writes `writeIdx_`, invalidating consumer's cache line. Consumer must reload even though it only cares about `readIdx_`.

**After Padding:**
```
[readIdx_ (8 bytes)] [pad 56 bytes] [writeIdx_ (8 bytes)] [pad 56 bytes]
← Cache line 0 →                    ← Cache line 1 →
```

Producer and consumer no longer interfere. Producer can invalidate its cache line independently.

#### 3. **Branch Predictor Recovery**

The **0.99% branch miss rate** (vs. 5.48% before) tells the real story:

- **Before**: Every cache line invalidation from the other thread changes the CPU's micro-architectural state, causing branch mispredictions
- **After**: Consistent, predictable branch patterns within each thread

**IPC Improvement Breakdown:**
- Main bottleneck in Stage 2 wasn't code length—it was **stalls waiting for cache coherency**
- Stage 3 eliminates these stalls, allowing the CPU to execute more instructions per cycle
- **2.47 IPC** = CPU executing nearly 2.5 instructions per clock cycle on a 3-wide superscalar

### CPU Pair Scaling

| CPU Pair | db9eedf | 01caf71 | 6accd1e | Scaling |
|----------|---------|---------|---------|---------|
| 0-1 | 191.5M | 282.0M | 720.1M | 3.76x |
| 2-3 | 191.7M | 280.8M | 706.0M | 3.68x |
| 4-5 | 78.3M | 220.4M | 370.2M | 4.73x |

**Key Finding**: CPU 4-5 (no shared L1/L2) sees **4.73x** scaling, compared to 3.76x for CPU 0-1. Why?

**Root Cause**: Without shared L2, the baseline contention is already severe. Thread-local caching + padding provides **larger relative gains** by:
1. Reducing L3 coherency traffic
2. Allowing each thread's caches to stay hot
3. Better branch prediction isolation

---

## Hardware Scenario Analysis

### Scenario 1: Shared Cache (CPU 0-1, 2-3)

**Performance**: 700-720M ops/s (stage 3)

**HFT Implication**: For low-latency trading, always **pin producer/consumer to cores sharing cache if possible**.

### Scenario 2: No Shared L1/L2 (CPU 4-5)

**Performance**: 370M ops/s (stage 3), degraded from 720M

**Why Performance Degrades:**
- L2 misses force L3 lookups (10-20 cycle latency)
- Cross-L2 coherency is more expensive

**Scaling**:
- Stage 1 baseline: 78.3M ops/s
- Stage 3 improved: 370.2M ops/s
- **4.73x improvement** through optimizations

Even with cache line padding and thread-local caching, the lack of shared L2 costs ~50% throughput compared to shared-L3 scenario.

---

## Micro-Architectural Insights

### 1. Memory Ordering Choices

The code uses three different memory orders strategically:

```cpp
// Fast-path load (consumer)
size_t readIdx = readIdx_.load(memory_order_relaxed);
// Cost: 1 cycle (just read register)

// Synchronization boundary (when cache misses)
cachedWriteIdx = writeIdx_.load(memory_order_acquire);
// Cost: ~15-20 cycles if cache miss, 1 cycle if hit
// Purpose: Establish happens-before relationship

// Index advancement (producer)
writeIdx_.store(writeIdx + 1, memory_order_release);
// Cost: ~1 cycle + coherency latency
// Purpose: Ensure buffer writes visible before advancing
```

**Why This Matters for IPC:**
- `memory_order_relaxed` allows the CPU to execute subsequent instructions speculatively (high IPC)
- `memory_order_acquire` forces dependency chains, reducing parallelism
- By using acquire only on cache miss (rare), we keep fast-path IPC high

**If using `memory_order_seq_cst` everywhere:**
- Throughput would drop (serialization overhead)

---

## Appendix: Raw Measurement Data

### Throughput (ops/s)

```
Commit | Desc | CPU 0-1 | CPU 2-3 | CPU 4-5
-------|------|---------|---------|--------
db9eedf | Baseline | 191.5M | 191.7M | 78.3M
01caf71 | Cached Idx | 282.0M | 280.8M | 220.4M
6accd1e | Thread-local + Padding | 720.1M | 706.0M | 370.2M
```

### Perf Stats (CPU 0-1)

**db9eedf:**
- IPC: ~1.03
- Branch Miss Rate: ~5-6%
- Cycles: ~19.7B

**01caf71:**
- IPC: ~1.04
- Branch Miss Rate: ~5-6%
- Cycles: ~17B (less work due to cached indices)

**6accd1e:**
- IPC: 2.47
- Branch Miss Rate: 0.99%
- Cycles: 7.7B (much fewer cycles, better execution)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-15
**CPU Configuration**: Intel 12-core
