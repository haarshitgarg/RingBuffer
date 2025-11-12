#include <iostream>

#include "RingBuffer.hpp"
#include "bench.hpp"
#include "rigtorp.hpp"

using namespace std;

// Specialization for rigtorp::SPSCQueue
template <typename T>
struct isRigtorp<rigtorp::SPSCQueue<T>> : std::true_type {};
int main(int argc, char *argv[]) {
  std::cout << "Running Ring Buffer calculations..." << std::endl;

  bench<RingBuffer>("RingBuffer", argc, argv);
  bench<rigtorp::SPSCQueue>("rigtorp", argc, argv);

  return 0;
}
