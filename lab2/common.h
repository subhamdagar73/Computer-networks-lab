#ifndef COMMON_H
#define COMMON_H

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
// We’ll use close_skt() everywhere to be portable.
#define close_skt closesocket
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#define close_skt close
#endif

#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <cerrno>

// ---------------------- Message Types ----------------------
#define MSG_TYPE_REQUEST 1
#define MSG_TYPE_RESPONSE 2
#define MSG_TYPE_DATA 3
#define MSG_TYPE_DATA_RESP 4

// ---------------------- Constants --------------------------
#define MAX_MESSAGE_SIZE 32768
#define BUFFER_SIZE 65536
#define DEFAULT_BACKLOG 10

// ---------------------- Message Structure -------------------
struct Message
{
    int type;
    int length;
    char data[MAX_MESSAGE_SIZE];

    Message() : type(0), length(0) { memset(data, 0, sizeof(data)); }

    Message(int t, const std::string &msg) : type(t)
    {
        length = static_cast<int>(msg.length());
        memset(data, 0, sizeof(data));
        if (length > 0 && length <= MAX_MESSAGE_SIZE)
        {
            memcpy(data, msg.c_str(), length);
        }
    }
};

// ---------------------- Client Request ----------------------
struct ClientRequest
{
    int client_socket;
    struct sockaddr_in client_addr;
    std::chrono::steady_clock::time_point arrival_time;

    ClientRequest(int sock, struct sockaddr_in addr)
        : client_socket(sock), client_addr(addr),
          arrival_time(std::chrono::steady_clock::now()) {}
};

// ---------------------- Scheduling Policies -----------------
enum SchedulingPolicy
{
    FCFS,
    RR
};

// ---------------------- Networking Helpers ------------------
inline void init_networking()
{
#ifdef _WIN32
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0)
    {
        std::cerr << "WSAStartup failed with error: " << res << std::endl;
        exit(1);
    }
#endif
}

inline void cleanup_networking()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

inline int get_last_error()
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

// ---------------------- TCP Send/Receive --------------------
inline bool send_message_tcp(int socket, const Message &msg)
{
    int net_type = htonl(msg.type);
    if (send(socket, reinterpret_cast<const char *>(&net_type), sizeof(net_type), 0) != sizeof(net_type))
    {
        return false;
    }

    int net_length = htonl(msg.length);
    if (send(socket, reinterpret_cast<const char *>(&net_length), sizeof(net_length), 0) != sizeof(net_length))
    {
        return false;
    }

    if (msg.length > 0)
    {
        int bytes_sent = send(socket, msg.data, msg.length, 0);
        if (bytes_sent != msg.length)
        {
            return false;
        }
    }
    return true;
}

inline bool receive_message_tcp(int socket, Message &msg)
{
    int net_type;
    int bytes_received = recv(socket, reinterpret_cast<char *>(&net_type), sizeof(net_type), 0);
    if (bytes_received != sizeof(net_type))
        return false;
    msg.type = ntohl(net_type);

    int net_length;
    bytes_received = recv(socket, reinterpret_cast<char *>(&net_length), sizeof(net_length), 0);
    if (bytes_received != sizeof(net_length))
        return false;
    msg.length = ntohl(net_length);

    if (msg.length < 0 || msg.length > MAX_MESSAGE_SIZE)
        return false;

    if (msg.length > 0)
    {
        int total_received = 0;
        while (total_received < msg.length)
        {
            bytes_received = recv(socket, msg.data + total_received, msg.length - total_received, 0);
            if (bytes_received <= 0)
                return false;
            total_received += bytes_received;
        }
    }
    return true;
}

// ---------------------- UDP Send/Receive --------------------
inline bool send_message_udp(int socket, const Message &msg, const struct sockaddr_in &addr)
{
    char buffer[sizeof(int) * 2 + MAX_MESSAGE_SIZE];

    int net_type = htonl(msg.type);
    memcpy(buffer, &net_type, sizeof(net_type));

    int net_length = htonl(msg.length);
    memcpy(buffer + sizeof(int), &net_length, sizeof(net_length));

    if (msg.length > 0)
    {
        memcpy(buffer + 2 * sizeof(int), msg.data, msg.length);
    }

    int total_size = 2 * sizeof(int) + msg.length;
    int bytes_sent = sendto(socket, buffer, total_size, 0,
                            reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));

    return bytes_sent == total_size;
}

inline bool receive_message_udp(int socket, Message &msg, struct sockaddr_in &addr)
{
    char buffer[sizeof(int) * 2 + MAX_MESSAGE_SIZE];
    socklen_t addr_len = sizeof(addr);

    int bytes_received = recvfrom(socket, buffer, sizeof(buffer), 0,
                                  reinterpret_cast<struct sockaddr *>(&addr), &addr_len);

    if (bytes_received < static_cast<int>(2 * sizeof(int)))
    {
        return false;
    }

    int net_type;
    memcpy(&net_type, buffer, sizeof(net_type));
    msg.type = ntohl(net_type);

    int net_length;
    memcpy(&net_length, buffer + sizeof(int), sizeof(net_length));
    msg.length = ntohl(net_length);

    if (msg.length < 0 || msg.length > MAX_MESSAGE_SIZE ||
        static_cast<size_t>(bytes_received) != (2 * sizeof(int) + static_cast<size_t>(msg.length)))
    {
        return false;
    }

    if (msg.length > 0)
    {
        memcpy(msg.data, buffer + 2 * sizeof(int), msg.length);
    }

    return true;
}

#endif // COMMON_H
