#ifndef PERIPHERY_SERIAL_HPP
#define PERIPHERY_SERIAL_HPP
#include <ostream>
#include <chrono>
#include <cstdint>

#include "buffer.h"

namespace periphery {

class Serial {
public:
    enum class DataBits { Five, Six, Seven, Eight };
    enum class StopBits { One, Two };
    enum class Parity { None, Even, Odd};
    enum class Handshake { None, RtsCts, XonXoff, RtsCtsXonXoff };
    Serial(const std::string& path, uint32_t baudrate, DataBits databits, Parity parity, StopBits stopbits, Handshake handshake);
    Serial(const std::string& path, uint32_t baudrate, DataBits databits, Parity parity, StopBits stopbits);
    Serial(const std::string& path, uint32_t baudrate);
    ~Serial();

    /* Disable copy constructor and copy assignment. */
    Serial(const Serial&) = delete;
    Serial& operator=(const Serial&) = delete;

    bool poll(std::chrono::milliseconds timeout) const;
    void flush() const;

    void write(const_buffer buf) const;
    void write(const std::string& data) const;
    
    int read(mutable_buffer buf) const;
    void read_all(mutable_buffer buf) const;
    int read_timeout(mutable_buffer buf, std::chrono::milliseconds timeout) const;
    int read_all_timeout(mutable_buffer buf, std::chrono::milliseconds timeout) const;

private:
    int fd_;
};

}

#endif