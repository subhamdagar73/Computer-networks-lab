#include "common.h"
#include <iomanip>
#include <fstream>

class NetworkClient
{
private:
    std::string server_ip;
    int server_port;

public:
    NetworkClient(const std::string &ip, int port)
        : server_ip(ip), server_port(port) { init_networking(); }

    ~NetworkClient() { cleanup_networking(); }

    bool communicate_with_server(const std::string &data_message = "Hello from client!")
    {
        int udp_port = negotiate_udp_port();
        if (udp_port <= 0)
        {
            std::cerr << "Failed to negotiate UDP port\n";
            return false;
        }

        std::cout << "Negotiated UDP port: " << udp_port << std::endl;

        // Not strictly needed now that server binds first, but harmless:
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return send_udp_data(udp_port, data_message);
    }

    // ----- Performance helpers (unchanged except closes) -----
    void measure_throughput_by_size()
    {
        std::cout << "\n=== Throughput Measurement by Message Size ===\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Size (KB)\tTCP Throughput (MB/s)\tUDP Throughput (MB/s)\n";
        std::cout << "-------------------------------------------------------\n";

        std::ofstream csv_file("throughput_by_size.csv");
        csv_file << "Size_KB,TCP_Throughput_MBps,UDP_Throughput_MBps\n";

        for (int size_kb = 1; size_kb <= 32; size_kb++)
        {
            int size_bytes = size_kb * 1024;
            std::string test_data(size_bytes, 'A' + (size_kb % 26));

            double tcp_throughput = measure_tcp_throughput(test_data, 10);
            double udp_throughput = measure_udp_throughput(test_data, 10);

            std::cout << size_kb << "\t\t" << tcp_throughput << "\t\t\t" << udp_throughput << std::endl;
            csv_file << size_kb << "," << tcp_throughput << "," << udp_throughput << "\n";

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        csv_file.close();
        std::cout << "Results saved to throughput_by_size.csv\n";
    }
    double measure_udp_throughput(const std::string &test_data, int iterations)
    {
        int udp_port = negotiate_udp_port();
        if (udp_port <= 0)
            return 0.0;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket < 0)
            return 0.0;

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(udp_port);
        inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

        auto start_time = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; i++)
        {
            Message data_msg(MSG_TYPE_DATA, test_data);
            send_message_udp(udp_socket, data_msg, server_addr);

            // Wait for ACK
            Message ack_msg;
            sockaddr_in recv_addr{};
            if (!receive_message_udp(udp_socket, ack_msg, recv_addr))
            {
                std::cerr << "Missed ACK for packet " << i + 1 << "\n";
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        close_skt(udp_socket);

        double total_bytes = static_cast<double>(test_data.length()) * iterations;
        double seconds = duration.count() / 1e6;
        return (total_bytes / (1024 * 1024)) / seconds;
    }

    void measure_bulk_transfer()
    {
        std::cout << "\n=== Bulk Transfer Measurement ===\n";
        std::cout << std::fixed << std::setprecision(2);

        std::ofstream csv_file("bulk_transfer.csv");
        csv_file << "Message_Size_Bytes,Total_Data_MB,TCP_Throughput_MBps,UDP_Throughput_MBps\n";

        std::vector<int> message_sizes = {512, 1024, 2048, 4096, 8192};
        std::vector<int> total_sizes = {1, 10}; // MB

        for (int total_mb : total_sizes)
        {
            std::cout << "\n--- " << total_mb << "MB Transfer Tests ---\n";
            std::cout << "Msg Size\tIterations\tTCP (MB/s)\tUDP (MB/s)\n";
            std::cout << "-------------------------------------------\n";

            for (int msg_size : message_sizes)
            {
                int iterations = (total_mb * 1024 * 1024) / msg_size;
                std::string test_data(msg_size, 'T');

                double tcp_throughput = measure_bulk_tcp(test_data, iterations);
                double udp_throughput = measure_bulk_udp(test_data, iterations);

                std::cout << msg_size << "\t\t" << iterations << "\t\t"
                          << tcp_throughput << "\t\t" << udp_throughput << std::endl;

                csv_file << msg_size << "," << total_mb << ","
                         << tcp_throughput << "," << udp_throughput << "\n";

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }

        csv_file.close();
        std::cout << "Results saved to bulk_transfer.csv\n";
    }

private:
    int negotiate_udp_port()
    {
        int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_socket < 0)
        {
            std::cerr << "TCP socket() failed: " << get_last_error() << std::endl;
            return -1;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0)
        {
            std::cerr << "Invalid server IP\n";
            close_skt(tcp_socket);
            return -1;
        }

        if (connect(tcp_socket, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0)
        {
            std::cerr << "connect() failed: " << get_last_error() << std::endl;
            close_skt(tcp_socket);
            return -1;
        }

        std::cout << "Connected to server " << server_ip << ":" << server_port << std::endl;

        Message request(MSG_TYPE_REQUEST, "Request UDP port");
        if (!send_message_tcp(tcp_socket, request))
        {
            std::cerr << "Failed to send TCP request\n";
            close_skt(tcp_socket);
            return -1;
        }

        Message response;
        if (!receive_message_tcp(tcp_socket, response) || response.type != MSG_TYPE_RESPONSE)
        {
            std::cerr << "Failed to receive TCP response for UDP port\n";
            close_skt(tcp_socket);
            return -1;
        }

        close_skt(tcp_socket);

        std::string port_str(response.data, response.length);
        return std::atoi(port_str.c_str());
    }

    bool send_udp_data(int udp_port, const std::string &data)
    {
        int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket < 0)
        {
            std::cerr << "UDP socket() failed: " << get_last_error() << std::endl;
            return false;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(udp_port);
        if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0)
        {
            std::cerr << "Bad server IP\n";
            close_skt(udp_socket);
            return false;
        }

        Message data_msg(MSG_TYPE_DATA, data);
        if (!send_message_udp(udp_socket, data_msg, server_addr))
        {
            std::cerr << "Failed to send UDP data\n";
            close_skt(udp_socket);
            return false;
        }

        std::cout << "Sent UDP data (" << data.length() << " bytes)\n";

        // Receive timeout
#ifdef _WIN32
        int timeout_ms = 10000; // 10s
        setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&timeout_ms), sizeof(timeout_ms));
#else
        timeval tv{10, 0};
        setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

        Message ack_msg;
        sockaddr_in recv_addr{};
        if (!receive_message_udp(udp_socket, ack_msg, recv_addr))
        {
            std::cerr << "UDP acknowledgment timeout or error\n";
            close_skt(udp_socket);
            return false;
        }

        if (ack_msg.type != MSG_TYPE_DATA_RESP)
        {
            std::cerr << "Invalid ACK type: " << ack_msg.type << "\n";
            close_skt(udp_socket);
            return false;
        }

        std::cout << "Received acknowledgment: " << std::string(ack_msg.data, ack_msg.length) << std::endl;

        close_skt(udp_socket);
        return true;
    }

    // ----- perf helpers below are unchanged logic-wise -----
    double measure_tcp_throughput(const std::string &test_data, int iterations)
    {
        int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_socket < 0)
            return 0.0;

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

        if (connect(tcp_socket, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0)
        {
            std::cerr << "TCP connect failed\n";
            close_skt(tcp_socket);
            return 0.0;
        }

        auto start_time = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++)
        {
            Message data_msg(MSG_TYPE_DATA, test_data);
            send_message_tcp(tcp_socket, data_msg);
        }
        auto end_time = std::chrono::high_resolution_clock::now();

        close_skt(tcp_socket);

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        double total_bytes = static_cast<double>(test_data.length()) * iterations;
        double seconds = duration.count() / 1e6;
        return (total_bytes / (1024 * 1024)) / seconds;
    }

    double measure_bulk_tcp(const std::string &test_data, int iterations)
    {
        int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_socket < 0)
            return 0.0;

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

        if (connect(tcp_socket, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            std::cerr << "TCP connect failed\n";
            close_skt(tcp_socket);
            return 0.0;
        }

        // std::cout << "TCP connection established for bulk transfer.\n";

        auto start_time = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; i++)
        {
            Message data_msg(MSG_TYPE_DATA, test_data);
            if (!send_message_tcp(tcp_socket, data_msg))
            {
                // std::cerr << "TCP send failed at iteration " << i << "\n";
                break;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        close_skt(tcp_socket);

        double total_bytes = static_cast<double>(test_data.length()) * iterations;
        double seconds = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() / 1e6;
        return (total_bytes / (1024 * 1024)) / seconds;
    }

    double measure_bulk_udp(const std::string &test_data, int iterations)
    {
        int udp_port = negotiate_udp_port();
        if (udp_port <= 0)
            return 0.0;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket < 0)
            return 0.0;

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(udp_port);
        inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

        auto start_time = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; i++)
        {
            Message data_msg(MSG_TYPE_DATA, test_data);
            if (!send_message_udp(udp_socket, data_msg, server_addr))
            {
                std::cerr << "UDP send failed at iteration " << i << "\n";
                break;
            }

            // Optional: tiny delay every 10k packets to avoid dropping
            if (i % 10000 == 0 && i > 0)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        close_skt(udp_socket);

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        double total_bytes = static_cast<double>(test_data.length()) * iterations;
        double seconds = duration.count() / 1e6;
        return (total_bytes / (1024 * 1024)) / seconds;
    }
};

static void print_usage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " <server_ip> <server_port> [options]\n"
              << "Options:\n"
              << "  -test            Run basic communication test\n"
              << "  -perf-size       Measure throughput by message size (1-32KB)\n"
              << "  -perf-bulk       Measure bulk transfer performance (1MB, 10MB)\n"
              << "  -custom <size>   Send custom message of specified size (KB)\n";
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        print_usage(argv[0]);
        return 1;
    }

    std::string server_ip = argv[1];
    int server_port = std::atoi(argv[2]);
    if (server_port <= 0 || server_port > 65535)
    {
        std::cerr << "Invalid port number\n";
        return 1;
    }

    try
    {
        NetworkClient client(server_ip, server_port);

        if (argc == 3)
        {
            std::cout << "Running basic communication test...\n";
            if (client.communicate_with_server())
            {
                std::cout << "Communication test completed successfully!\n";
            }
            else
            {
                std::cout << "Communication test failed!\n";
                return 1;
            }
            return 0;
        }

        for (int i = 3; i < argc; i++)
        {
            std::string arg = argv[i];
            if (arg == "-test")
            {
                std::cout << "Running basic communication test...\n";
                if (client.communicate_with_server())
                {
                    std::cout << "Communication test completed successfully!\n";
                }
                else
                {
                    std::cout << "Communication test failed!\n";
                }
            }
            else if (arg == "-perf-size")
            {
                client.measure_throughput_by_size();
            }
            else if (arg == "-perf-bulk")
            {
                client.measure_bulk_transfer();
            }
            else if (arg == "-custom" && i + 1 < argc)
            {
                int size_kb = std::atoi(argv[i + 1]);
                if (size_kb > 0 && size_kb <= 32)
                {
                    std::string custom_data(size_kb * 1024, 'X');
                    std::cout << "Sending custom message of " << size_kb << "KB...\n";
                    if (client.communicate_with_server(custom_data))
                    {
                        std::cout << "Custom message sent successfully!\n";
                    }
                    else
                    {
                        std::cout << "Failed to send custom message!\n";
                    }
                }
                else
                {
                    std::cerr << "Invalid custom size (1-32KB)\n";
                }
                i++;
            }
            else
            {
                std::cerr << "Unknown option: " << arg << "\n";
                print_usage(argv[0]);
                return 1;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Client error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
