// C++11
#include <cstdlib>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

// POSIX 2008 Headers:
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/select.h>
#include <termios.h>

// Linux:
#include <sys/ioctl.h>

#include "serial.h"

namespace periphery {

speed_t baudrate_to_bits(uint32_t baudrate);

Serial::Serial(const std::string& path, uint32_t baudrate, DataBits databits, Parity parity, StopBits stopbits, Handshake handshake)
{
    struct termios settings;
    memset(&settings, 0, sizeof(settings));

    // c_iflag:  ignore break characters
    settings.c_iflag = IGNBRK
        | (parity    != Parity::None             ? INPCK | ISTRIP : 0)
        | (handshake == Handshake::XonXoff       ? IXON | IXOFF   : 0)
        | (handshake == Handshake::RtsCtsXonXoff ? IXON | IXOFF   : 0);

    // c_oflag:
    settings.c_oflag = 0;

    // c_lflag:
    settings.c_lflag = 0;

    // c_cflag:  enable receive, ignore modem control lines
    settings.c_cflag = CREAD | CLOCAL;

    // c_cflag:  databits
    switch (databits) {
        case DataBits::Five  :  settings.c_cflag |= CS5; break;
        case DataBits::Six   :  settings.c_cflag |= CS6; break;
        case DataBits::Seven :  settings.c_cflag |= CS7; break;
        case DataBits::Eight :  settings.c_cflag |= CS8; break;
        default              :  throw std::invalid_argument("databits invalid");
    }

    // c_cflag:  parity
    switch (parity) {
        case Parity::Even  :  settings.c_cflag |= PARENB; break;
        case Parity::Odd   :  settings.c_cflag |= (PARENB | PARODD); break;
        case Parity::None  :  break;
        default            :  throw std::invalid_argument("parity invalid");
    }

    // c_cflags: stopbits
    switch (stopbits) {
        case StopBits::Two   :  settings.c_cflag |= CSTOPB; break;
        case StopBits::One   :  break;
        default              :  throw std::invalid_argument("stopbits invalid");
    }

    // c_cflags: RTS/CTS
    switch (handshake) {
        case Handshake::RtsCts         :  settings.c_cflag |= CRTSCTS; break;
        case Handshake::RtsCtsXonXoff  :  settings.c_cflag |= CRTSCTS; break;
        case Handshake::None           :  break;
        case Handshake::XonXoff        :  break;
        default                        :  throw std::invalid_argument("handshake invalid");
    }

    // baudrate
    cfsetispeed(&settings, baudrate_to_bits(baudrate));
    cfsetospeed(&settings, baudrate_to_bits(baudrate));

    fd_ = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd_ < 0) {
        throw std::system_error(EFAULT, std::system_category());
    }

    int error = tcsetattr(fd_, TCSANOW, &settings);
    if (error < 0) {
        auto e = std::system_error(EFAULT, std::system_category());
        ::close(fd_);
        throw e;
    }
}

Serial::Serial(const std::string& path, uint32_t baudrate, DataBits databits, Parity parity, StopBits stopbits)
    : Serial(path, baudrate, databits, parity, stopbits, Handshake::None) { }

Serial::Serial(const std::string& path, uint32_t baudrate)
    : Serial(path, baudrate, DataBits::Eight, Parity::None, StopBits::One, Handshake::None) { }

Serial::~Serial()
{
    int error = ::close(fd_);
    if (error < 0) {
        // ... can't throw in destructor, maybe log an error? ...
    }
}

bool Serial::poll(std::chrono::milliseconds timeout) const
{
    struct pollfd fds[1];

    fds[0].fd = fd_;
    fds[0].events = POLLIN | POLLPRI;
    
    int ret = ::poll(fds, 1, timeout.count());
    if (ret < 0) {
        throw std::system_error(EFAULT, std::system_category());
    }

    return (ret > 0);
}

void Serial::flush() const
{
    int error = tcdrain(fd_);
    if (error < 0) {
        throw std::system_error(EFAULT, std::system_category());
    }
}

void Serial::write(const_buffer buf) const
{
    while (buf.size() > 0) {
        ssize_t ret = ::write(fd_, buf.data(), buf.size());
        if (ret < 0) {
            throw std::system_error(EFAULT, std::system_category());
        }
        buf +=ret;
    }
}

void Serial::write(const std::string& data) const
{

}

int Serial::read(mutable_buffer buf) const
{
    ssize_t ret = ::read(fd_, buf.data(), buf.size());
    if (ret < 0) {
        throw std::system_error(EFAULT, std::system_category());
    }
    return ret;
}

void Serial::read_all(mutable_buffer buf) const
{
    while (buf.size() > 0) {
        int bytesRead = this->read(buf);
        buf += bytesRead;
    }
}

int Serial::read_timeout(mutable_buffer buf, std::chrono::milliseconds timeout) const
{
    ssize_t ret;

    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;

    if (timeout.count() >= 0) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd_, &rfds);

        ret = ::select(fd_ + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            throw std::system_error(EFAULT, std::system_category());
        }

        if (ret == 0) {
            return 0;
        }
    }

    ret = ::read(fd_, buf.data(), buf.size());
    if (ret < 0) {
        throw std::system_error(EFAULT, std::system_category()); 
    }
    
    return ret;
}

int Serial::read_all_timeout(mutable_buffer buf, std::chrono::milliseconds timeout) const
{
    ssize_t ret;
    auto original_size = buf.size();

    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;

    do {
        if (timeout.count() >= 0) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd_, &rfds);

            ret = ::select(fd_ + 1, &rfds, NULL, NULL, &tv);
            if (ret < 0) {
                throw std::system_error(EFAULT, std::system_category());
            }
            if (ret == 0) {
                break;
            }
        }
        ret = ::read(fd_, buf.data(), buf.size());
        if (ret < 0) {
            throw std::system_error(EFAULT, std::system_category());
        }

        buf += ret;
    } while (buf.size() > 0);

    return original_size - buf.size();
}


speed_t baudrate_to_bits(uint32_t baudrate)
{
    switch(baudrate) {
        case 50: return B50;
        case 75: return B75;
        case 110: return B110;
        case 134: return B134;
        case 150: return B150;
        case 200: return B200;
        case 300: return B300;
        case 600: return B600;
        case 1200: return B1200;
        case 1800: return B1800;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 500000: return B500000;
        case 576000: return B576000;
        case 921600: return B921600;
        case 1000000: return B1000000;
        case 1152000: return B1152000;
        case 1500000: return B1500000;
        case 2000000: return B2000000;
#ifdef B2500000
        case 2500000: return B2500000;
#endif
#ifdef B3000000
        case 3000000: return B3000000;
#endif
#ifdef B3500000
        case 3500000: return B3500000;
#endif
#ifdef B4000000
        case 4000000: return B4000000;
#endif
        default: throw std::invalid_argument("baurate invalid");
    }
}

}