#include "can_socket.h"

#include <iostream>
#include <cstring>      // memset, strerror
#include <cerrno>       // errno

// Linux-specific headers for SocketCAN
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>     // close()

namespace diag {

CanSocket::CanSocket(const std::string& interfaceName)
    : m_socketFd(-1)
    , m_interfaceName(interfaceName)
{
    // Step 1: Create a raw CAN socket
    m_socketFd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socketFd < 0) {
        std::cerr << "Failed to create CAN socket: "
                  << strerror(errno) << "\n";
        return;
    }

    // Step 2: Find the network interface index for "vcan0"
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interfaceName.c_str(), IFNAMSIZ - 1);

    if (ioctl(m_socketFd, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "Interface '" << interfaceName << "' not found: "
                  << strerror(errno) << "\n";
        close(m_socketFd);
        m_socketFd = -1;
        return;
    }

    // Step 3: Bind the socket to vcan0
    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(m_socketFd,
             reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(addr)) < 0) {
        std::cerr << "Failed to bind CAN socket: "
                  << strerror(errno) << "\n";
        close(m_socketFd);
        m_socketFd = -1;
        return;
    }
    
    // Timeout so the server loop wakes up and can check m_running
    struct timeval tv;
    tv.tv_sec  = 1;
    tv.tv_usec = 0;
    setsockopt(m_socketFd, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&tv), sizeof(tv));

    std::cout << "[CanSocket] Opened " << interfaceName << "\n";
}

CanSocket::~CanSocket() {
    if (m_socketFd >= 0) {
        close(m_socketFd);
        std::cout << "[CanSocket] Closed " << m_interfaceName << "\n";
    }
}

bool CanSocket::send(uint32_t canId, const std::vector<uint8_t>& data) {
    if (!isOpen()) return false;
    if (data.size() > 8) {
        std::cerr << "CAN frame payload too large (max 8 bytes)\n";
        return false;
    }

    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.can_id  = canId;
    frame.can_dlc = static_cast<uint8_t>(data.size()); // DLC = Data Length Code
    memcpy(frame.data, data.data(), data.size());

    ssize_t written = write(m_socketFd, &frame, sizeof(frame));
    if (written != sizeof(frame)) {
        std::cerr << "Failed to send CAN frame: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

bool CanSocket::receive(uint32_t& outCanId, std::vector<uint8_t>& outData) {
    if (!isOpen()) return false;

    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));

    // read() blocks here until a frame arrives on the bus
    ssize_t bytesRead = read(m_socketFd, &frame, sizeof(frame));
    if (bytesRead != static_cast<ssize_t>(sizeof(frame))) {
        // EAGAIN/EWOULDBLOCK = timeout expired, not a real error
        // This is expected — it lets the caller check m_running
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "Failed to receive CAN frame: "
                      << strerror(errno) << "\n";
        }
        return false;
    }

    outCanId = frame.can_id;
    outData.assign(frame.data, frame.data + frame.can_dlc);
    return true;
}

}