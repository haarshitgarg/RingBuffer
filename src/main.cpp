#include <iostream>

#include "RingBuffer.hpp"

using namespace std;
int main() {
    std::cout<<"Running Ring Buffer calculations..."<<std::endl;

    RingBuffer myBuffer = RingBuffer<int>(4);

    myBuffer.push(1);
    myBuffer.push(2);
    myBuffer.push(3);
    myBuffer.push(4);
    myBuffer.push(5);

    int val;
    while(myBuffer.pop(val)) {
        cout<<"Popping value: "<<val<<endl;
    }

    return 0;
}

