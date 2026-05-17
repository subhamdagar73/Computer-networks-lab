# Computer Networks Lab

A comprehensive collection of network protocols, algorithms, and simulation projects demonstrating fundamental and advanced concepts in computer networking.

## Overview

This repository contains practical implementations and simulations of various network protocols and scheduling algorithms used in computer networks courses. Each lab builds upon networking concepts from basic socket programming to advanced protocol implementations.

## Directory Structure

### [Lab 2: Network Communication](lab2/)
TCP/UDP socket programming with concurrent client-server communication.
- **Focus**: Socket-level network programming, concurrent connections, performance measurement
- **Key Files**: `client.cpp`, `server.cpp`, `common.h`
- **Concepts**: TCP/UDP protocols, port negotiation, throughput measurement, concurrent threading

### [Lab 3: Network Protocols](lab3/)
Implementation of reliable data transfer protocols using ns-3 network simulator.
- **Focus**: Protocol design, error handling, reliability, congestion control
- **Key Files**: `abp.cc`, `gbn.cc`, `dvr.cpp`, `tcp-congestion-comparison.cc`
- **Concepts**: Alternating Bit Protocol, Go-Back-N, Distance Vector Routing, TCP congestion control

### [Lab 4: Packet Scheduling](lab4/)
Queue scheduling algorithms for packet processing in networks.
- **Focus**: Scheduling algorithms, resource allocation, QoS
- **Key Files**: `source_code.cpp`
- **Concepts**: FCFS (First Come First Served), WFQ (Weighted Fair Queuing)

### [Lab 5: MPLS Simulation](lab5/)
Multiprotocol Label Switching simulation and routing.
- **Focus**: Label switching, routing optimization, network simulation
- **Key Files**: `mpls_simulation.cpp`
- **Concepts**: MPLS, shortest path routing, routing protocols (Dijkstra), network topology

## Prerequisites

- **C++11** or later
- **NS-3** simulator (for Lab 3 files: abp.cc, gbn.cc)
- **Standard libraries** (for standalone C++ implementations)

## Getting Started

1. Navigate to the desired lab directory:
   ```bash
   cd lab2  # or lab3, lab4, lab5
   ```

2. Refer to the lab-specific README for compilation and execution instructions

## Labs Summary

| Lab | Topic | Type | Language |
|-----|-------|------|----------|
| Lab 2 | Network Communication | Implementation | C++ |
| Lab 3 | Network Protocols | Simulation | C++ (ns-3) |
| Lab 4 | Packet Scheduling | Implementation | C++ |
| Lab 5 | MPLS Routing | Simulation | C++ |

## Learning Outcomes

After completing these labs, you will understand:
- Socket programming and network communication fundamentals
- Reliable data transfer protocol design and implementation
- Network routing and path optimization
- Packet scheduling and quality of service
- Network simulation and performance analysis

## Notes

- Some labs use the NS-3 network simulator framework
- Implementations demonstrate both theoretical concepts and practical coding
- Code includes examples of concurrent programming and network-level optimizations

---

For detailed information about each lab, navigate to the respective lab directory and read the lab-specific README.
