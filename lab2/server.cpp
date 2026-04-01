#include "common.h"
#include <algorithm>
#include <random>


class ConcurrentServer
{
private:
    int tcp_port;
    SchedulingPolicy policy;
    std::atomic<bool> running{false};

    std::queue<ClientRequest> request_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;

    std::vector<std::thread> worker_threads;
    static const int NUM_WORKERS = 4;

    std::atomic<int> total_clients_served{0};

public:
    ConcurrentServer(int port, SchedulingPolicy sched_policy)
        : tcp_port(port), policy(sched_policy)
    {
        init_networking();
    }

    ~ConcurrentServer()
    {
        stop();
        cleanup_networking();
    }

    void start()
    {
        running = true;
        for (int i = 0; i < NUM_WORKERS; ++i)
        {
            worker_threads.emplace_back(&ConcurrentServer::worker_thread, this);
        }
        accept_connections();
    }

    void stop()
    {
        running = false;
        queue_cv.notify_all();
        for (auto &t : worker_threads)
            if (t.joinable())
                t.join();
    }

private:
    void accept_connections()
    {
        int server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0)
        {
            std::cerr << "TCP socket() failed: " << get_last_error() << std::endl;
            return;
        }

        int opt = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&opt), sizeof(opt));

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(tcp_port);

        if (bind(server_socket, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0)
        {
            std::cerr << "TCP bind() failed: " << get_last_error() << std::endl;
            close_skt(server_socket);
            return;
        }

        if (listen(server_socket, DEFAULT_BACKLOG) < 0)
        {
            std::cerr << "TCP listen() failed: " << get_last_error() << std::endl;
            close_skt(server_socket);
            return;
        }

        std::cout << "Server listening on TCP " << tcp_port
                  << " with " << (policy == FCFS ? "FCFS" : "Round-Robin") << " scheduling\n";

        while (running)
        {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(server_socket, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
            if (client_socket < 0)
            {
                if (running)
                    std::cerr << "accept() failed: " << get_last_error() << std::endl;
                continue;
            }

            {
                std::lock_guard<std::mutex> lk(queue_mutex);
                request_queue.emplace(client_socket, client_addr);
            }
            queue_cv.notify_one();
        }

        close_skt(server_socket);
    }

    void worker_thread()
    {
        while (running)
        {
            ClientRequest req(0, {});
            {
                std::unique_lock<std::mutex> lk(queue_mutex);
                queue_cv.wait(lk, [&]
                              { return !request_queue.empty() || !running; });
                if (!running)
                    break;
                req = request_queue.front();
                request_queue.pop();
            }
            handle_client(req);
            total_clients_served++;
        }
    }

    // Bind-first UDP: creates and binds UDP socket, discovers the real port, then sends it to client.
    void handle_client(const ClientRequest &request)
    {
        int client_socket = request.client_socket;

        Message req_msg;
        if (!receive_message_tcp(client_socket, req_msg) || req_msg.type != MSG_TYPE_REQUEST)
        {
            std::cerr << "Bad/failed TCP request from client\n";
            close_skt(client_socket);
            return;
        }

        // Create UDP socket and bind to port 0 (let OS select a free port)
        int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket < 0)
        {
            std::cerr << "UDP socket() failed: " << get_last_error() << std::endl;
            close_skt(client_socket);
            return;
        }

        // Optional: reuse address
        int opt = 1;
        setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&opt), sizeof(opt));

        sockaddr_in udp_addr{};
        udp_addr.sin_family = AF_INET;
        udp_addr.sin_addr.s_addr = INADDR_ANY;
        udp_addr.sin_port = htons(0); // port 0 = ephemeral free port

        if (bind(udp_socket, reinterpret_cast<sockaddr *>(&udp_addr), sizeof(udp_addr)) < 0)
        {
            std::cerr << "UDP bind() failed: " << get_last_error() << std::endl;
            close_skt(udp_socket);
            close_skt(client_socket);
            return;
        }

        // Discover the actual bound port
        sockaddr_in bound_addr{};
        socklen_t len = sizeof(bound_addr);
        if (getsockname(udp_socket, reinterpret_cast<sockaddr *>(&bound_addr), &len) < 0)
        {
            std::cerr << "getsockname() failed: " << get_last_error() << std::endl;
            close_skt(udp_socket);
            close_skt(client_socket);
            return;
        }
        int udp_port = ntohs(bound_addr.sin_port);
        std::cout << "Allocated UDP port " << udp_port << " for client\n";

        // Send UDP port back to client over TCP (now it's definitely bound)
        Message resp(MSG_TYPE_RESPONSE, std::to_string(udp_port));
        if (!send_message_tcp(client_socket, resp))
        {
            std::cerr << "Failed to send UDP port to client\n";
            close_skt(udp_socket);
            close_skt(client_socket);
            return;
        }
        // After sending UDP port, keep TCP open to handle bulk transfers
        std::cout << "TCP socket ready for bulk data transfer\n";

        while (true)
        {
            Message data_msg;
            if (!receive_message_tcp(client_socket, data_msg))
            {
                std::cout << "TCP client closed connection or read error.\n";
                break; // exit when client disconnects
            }

            if (data_msg.type == MSG_TYPE_DATA)
            {
                // Just consume the data, don't print every packet (too slow)
                // std::cout << "Received TCP data of size: " << data_msg.length << " bytes\n";
            }
            else
            {
                std::cerr << "Unexpected TCP message type: " << data_msg.type << "\n";
            }
        }

        // TCP phase done
        close_skt(client_socket);

        // Set receive timeout on UDP
#ifdef _WIN32
        int timeout_ms = 30000; // 30s
        setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&timeout_ms), sizeof(timeout_ms));
#else
        timeval tv{30, 0};
        setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

        try
        {
            // Receive one data message and ACK it
            Message data_msg;
            sockaddr_in client_udp_addr{};
            std::cout << "Waiting for UDP data on port " << udp_port << "...\n";
            while (true)
            {
                if (!receive_message_udp(udp_socket, data_msg, client_udp_addr))
                {
                    std::cerr << "UDP receive failed/timeout or no more data\n";
                    break; // exit loop on timeout
                }

                if (data_msg.type != MSG_TYPE_DATA)
                {
                    std::cerr << "Unexpected UDP msg type: " << data_msg.type << "\n";
                    break;
                }

                std::cout << "Received UDP " << data_msg.length << " bytes from "
                          << inet_ntoa(client_udp_addr.sin_addr) << ":" << ntohs(client_udp_addr.sin_port) << "\n";

                // Send acknowledgment
                Message ack(MSG_TYPE_DATA_RESP, "ACK");
                send_message_udp(udp_socket, ack, client_udp_addr);
            }

            close_skt(udp_socket);
        }
        catch (...)
        {
            std::cerr << "Exception in UDP handling\n";
        }

        close_skt(udp_socket);
    }
};

static void print_usage(const char *prog)
{
    std::cout << "Usage: " << prog << " <port> [FCFS|RR]\n";
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535)
    {
        std::cerr << "Invalid port\n";
        return 1;
    }

    SchedulingPolicy pol = FCFS;
    if (argc >= 3)
    {
        std::string p = argv[2];
        if (p == "RR")
            pol = RR;
        else if (p != "FCFS")
        {
            std::cerr << "Policy must be FCFS or RR\n";
            return 1;
        }
    }

    try
    {
        ConcurrentServer server(port, pol);
        server.start();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Server error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
