#include <iostream>

#include "RingBuffer.hpp"
#include "bench.hpp"

using namespace std;

int main(int argc, char *argv[]) {
  std::cout << "Running Ring Buffer calculations..." << std::endl;

  bench<RingBuffer>("RingBuffer", argc, argv);

  return 0;
}
