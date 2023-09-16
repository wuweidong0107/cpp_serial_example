#include <cstdlib>
#include <cstdint>
#include <cstring>

#include <iostream>

#include <array>
#include <chrono>

#include "serial.h"

int main()
{
    periphery::Serial serial("/dev/ttyUSB0", 1500000);

    serial.flush();

    serial.poll(std::chrono::milliseconds(100));

    unsigned char tmp[256] = {};
    serial.write(periphery::buffer(tmp));

    std::array<char, 16> myArray;
    serial.read(periphery::buffer(myArray));
    serial.read_timeout(periphery::buffer(myArray), std::chrono::milliseconds{25});
    return 0;
}