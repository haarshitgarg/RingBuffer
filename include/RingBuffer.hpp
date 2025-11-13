#ifndef RINGBUFFER_HPP
#define RINGBUFFER_HPP

/* Implementation of ring buffer
 * It is a lock free and wait free ring buffer for good performance
 *
 * */
#include <alloca.h>
#include <atomic>
#include <cstddef>

using namespace std;

template <typename T> class RingBuffer {
private:
  T *buff_;
  size_t cap_;

  alignas(64) atomic<size_t> readIdx_;
  alignas(64) atomic<size_t> writeIdx_;
  alignas(64) size_t mask_;

  static size_t getPowerOfTwo(size_t capacity) {
    if (capacity <= 2) {
      return 2;
    }
    capacity = capacity / 2;

    size_t newCapacity = 1;
    while (capacity != 0) {
      capacity = capacity / 2;
      newCapacity = 2 * newCapacity;
    }

    return newCapacity;
  }

public:
  using value_type = T;

  RingBuffer(size_t capacity) {
    cap_ = getPowerOfTwo(capacity);
    buff_ = new T[cap_];
    readIdx_ = 0;
    writeIdx_ = 0;
    mask_ = cap_ - 1;
  }

  ~RingBuffer() { delete buff_; }

  bool pop(T &val) {
    size_t writeIdx = writeIdx_.load(memory_order_acquire);
    size_t readIdx = readIdx_.load(memory_order_relaxed);

    if (writeIdx == readIdx)
      return false;

    val = buff_[readIdx & mask_];
    readIdx_.store(readIdx + 1, memory_order_release);

    return true;
  }

  bool push(T val) {
    size_t writeIdx = writeIdx_.load(memory_order_relaxed);
    size_t readIdx = readIdx_.load(memory_order_acquire);

    if (writeIdx - readIdx == cap_)
      return false;

    buff_[writeIdx & mask_] = val;
    writeIdx_.store(writeIdx + 1, memory_order_release);

    return true;
  }

  bool empty() {
    if (readIdx_ == writeIdx_) {
      return true;
    }
    return false;
  }

  bool full() {
    if (writeIdx_ - readIdx_ == cap_) {
      return true;
    }
    return false;
  }
};

#endif // RINGBUFFER_HPP
