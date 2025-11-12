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

public:
  using value_type = T;

  RingBuffer(size_t capacity) {
    cap_ = capacity;
    buff_ = new T[cap_];
  }

  ~RingBuffer() { delete buff_; }

  bool pop(T &val) {
    size_t writeIdx = writeIdx_.load(memory_order_acquire);
    size_t readIdx = readIdx_.load(memory_order_relaxed);

    if (writeIdx == readIdx)
      return false;

    val = buff_[readIdx % cap_];
    readIdx_.store(readIdx + 1, memory_order_release);

    return true;
  }

  bool push(T val) {
    size_t writeIdx = writeIdx_.load(memory_order_relaxed);
    size_t readIdx = readIdx_.load(memory_order_acquire);

    if (writeIdx - readIdx == cap_)
      return false;

    buff_[writeIdx % cap_] = val;
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
