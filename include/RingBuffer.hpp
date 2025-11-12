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

  atomic<size_t> readIdx_, writeIdx_;

public:
  using value_type = T;

  RingBuffer(size_t capacity) {
    cap_ = capacity;
    buff_ = new T[cap_];
  }

  ~RingBuffer() { delete buff_; }

  bool pop(T &val) {
    if (empty()) {
      return false;
    }
    val = buff_[readIdx_ % cap_];
    ++readIdx_;

    return true;
  }

  bool push(T val) {
    if (full()) {
      return false;
    }
    buff_[writeIdx_ % cap_] = val;
    ++writeIdx_;

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
