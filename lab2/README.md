# Lab 2: Network Communication (TCP/UDP Socket Programming)

## Overview

This lab demonstrates socket-level network programming with concurrent client-server communication. It implements a network communication system that supports both TCP and UDP protocols with performance measurement capabilities.

## Objectives

- Understand socket programming fundamentals (TCP and UDP)
- Implement concurrent server handling multiple clients
- Learn about port negotiation and protocol selection
- Measure network performance metrics (throughput, latency)
- Practice multi-threaded programming in network applications

## Files

### `server.cpp`
Implements a concurrent server with the following features:
- Multi-threaded architecture with worker threads
- TCP connection acceptance and request queuing
- UDP data reception
- Configurable scheduling policies (FCFS, Round-Robin, Priority-based)
- Performance monitoring and statistics collection
- Clean shutdown mechanism

**Key Classes:**
- `ConcurrentServer`: Main server implementation with threading support
- `ClientRequest`: Data structure for client requests
- Request queue with thread-safe operations

### `client.cpp`
Implements a network client with:
- TCP connection to server
- UDP port negotiation with server
- UDP data transmission
- Throughput measurement capabilities
- Message size variation testing
- Performance analysis by message size

**Key Classes:**
- `NetworkClient`: Client implementation
- Performance measurement methods
- CSV output for analysis

### `common.h`
Shared header file containing:
- Cross-platform socket definitions (Windows/Linux)
- Networking utility functions
- Data structures for requests and responses
- Helper functions for TCP/UDP operations
- Error handling utilities

## Key Concepts

### Protocol Handling
- **TCP**: Connection-oriented, ordered delivery, no message boundaries
- **UDP**: Connectionless, faster, no reliability guarantees

### Port Negotiation
The client and server negotiate a UDP port over TCP to establish UDP communication after initial TCP handshake.

### Performance Metrics
- **Throughput**: Data volume transferred per unit time (MB/s)
- **Latency**: Time delay in message transmission
- Measurement taken for various message sizes

### Concurrent Processing
- Worker thread pool pattern
- Thread-safe queue for request handling
- Mutex and condition variable for synchronization

## Compilation

### Linux/macOS
```bash
g++ -std=c++11 -pthread -o server server.cpp common.h
g++ -std=c++11 -pthread -o client client.cpp common.h
```

### Windows (MSVC)
```bash
cl /std:c++latest server.cpp
cl /std:c++latest client.cpp
```

## Execution

### Start Server
```bash
./server <port> <scheduling_policy>
# Example:
./server 8080 FCFS
```

**Scheduling Policies**: FCFS (First Come First Served), RR (Round Robin), Priority

### Run Client (in separate terminal)
```bash
./client <server_ip> <port>
# Example:
./client 127.0.0.1 8080
```

## Output

The server produces:
- Real-time log of client connections and requests
- Total clients served statistics
- Request processing times

The client produces:
- Throughput measurements by message size
- CSV file: `throughput_by_size.csv`
- Performance comparison between TCP and UDP

## Example Output

```
=== Throughput Measurement by Message Size ===
Size (KB)    TCP Throughput (MB/s)    UDP Throughput (MB/s)
1            150.25                   180.50
2            165.75                   195.30
...
```

## Extensions

Possible enhancements:
1. Add latency measurement
2. Implement different scheduling algorithms
3. Add packet loss simulation
4. Implement flow control mechanisms
5. Add SSL/TLS security
6. Performance optimization techniques

## Common Issues

1. **Address already in use**: Port may not be released immediately. Wait or use a different port.
2. **Connection refused**: Ensure server is running and listening on the specified port.
3. **Permission denied**: On Linux, ports < 1024 require elevated privileges.

## Related Concepts

- Socket API (Berkeley sockets)
- TCP/IP protocol stack
- Multi-threading and synchronization
- Network performance analysis
- Client-Server architecture

---

**Last Updated**: May 2026
