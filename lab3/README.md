# Lab 3: Network Protocols (Reliable Data Transfer & Routing)

## Overview

This lab explores reliable data transfer protocols and routing algorithms using the NS-3 network simulator. It implements fundamental protocols that ensure reliable communication over unreliable channels and demonstrates how networks route packets efficiently.

## Objectives

- Understand reliable data transfer mechanisms
- Implement protocol state machines (ARQ protocols)
- Learn about congestion control in TCP
- Explore routing algorithms (Distance Vector Routing)
- Use NS-3 simulator for network protocol evaluation

## Files

### `abp.cc` - Alternating Bit Protocol (Stop-and-Wait)
Implements the simplest reliable data transfer protocol.

**Protocol Features:**
- Alternating bit sequence number (0 or 1)
- Stop-and-Wait mechanism: sender waits for ACK before sending next packet
- Timeout and retransmission on packet loss
- Handles packet corruption and loss

**Key Components:**
- `ABPHeader`: Custom packet header with type (DATA/ACK) and sequence number
- Sender state machine
- Receiver state machine
- Error handling and timeout logic

**Command Line Parameters:**
```bash
--SimTime=100        # Simulation duration in seconds
--Loss=0.01          # Packet error rate (0.01 = 1%)
--Payload=1000       # Payload size in bytes
--Interval=1.0       # Time between packets in seconds
--Timeout=1.0        # Retransmission timeout in seconds
```

### `gbn.cc` - Go-Back-N Protocol
Implements a more efficient sliding window protocol.

**Protocol Features:**
- Sender window size N (sends up to N packets without waiting)
- Receiver window size 1 (in-order delivery)
- Cumulative ACK: acknowledges all packets up to sequence number
- On error, sender retransmits from the lost packet onwards
- Better throughput than ABP, reduced latency

**Key Components:**
- `GbnHeader`: Packet header with sequence number
- Sender sliding window management
- Receiver in-order delivery buffer
- Selective retransmission logic

**Advantages over ABP:**
- Higher throughput due to pipelining
- Reduced idle time waiting for ACKs
- Suitable for higher bandwidth-delay product links

### `dvr.cpp` - Distance Vector Routing (DVR)
Implements a simplified Distance Vector Routing protocol.

**Protocol Features:**
- Distributed routing algorithm
- Each router maintains routing information base (RIB)
- Dijkstra's shortest path algorithm
- Direct neighbor cost information
- Routing table update simulation

**Key Components:**
- `RIBEntry`: Routing Information Base entry (next hop, cost)
- Router network topology
- Shortest path computation using Dijkstra
- Routing table management

**Topology:**
```
    R0 --- R1
    |       |
    R2 --- R3
```

### `tcp-congestion-comparison.cc`
Compares different TCP congestion control algorithms.

**Algorithms Compared:**
- TCP Reno
- TCP Tahoe
- TCP NewReno
- Other variants

**Metrics Measured:**
- Throughput
- Fairness
- Convergence behavior
- Responsiveness to congestion

## NS-3 Simulator Setup

### Installation
```bash
# Clone NS-3 from official repository
git clone https://gitlab.com/nsnam/ns-3-dev.git
cd ns-3-dev

# Configure
./waf configure --enable-examples --enable-tests

# Build
./waf build
```

### Compilation

Copy files to `ns-3-dev/scratch/` directory:
```bash
cp abp.cc gbn.cc /path/to/ns-3-dev/scratch/
cd /path/to/ns-3-dev
./waf
```

### Execution

```bash
./waf --run "scratch/abp --SimTime=100 --Loss=0.01"
./waf --run "scratch/gbn --SimTime=100 --Loss=0.05"
```

## Key Concepts

### Reliable Data Transfer (RDT)
Ensures packets are:
1. **Delivered** in order
2. **No duplicates**
3. **Received correctly** despite channel imperfections

### Automation Repeat reQuest (ARQ)
Mechanisms for detecting and retransmitting lost/corrupted packets:
- **Stop-and-Wait ARQ** (ABP)
- **Go-Back-N ARQ** (GBN)
- **Selective Repeat ARQ** (SACK)

### Sliding Window Protocol
- **Sender window**: Range of packets sender can transmit without ACK
- **Receiver window**: Range of acceptable sequence numbers
- Improves throughput by pipelining

### Routing Algorithms
- **Distance Vector**: Each router tells neighbors its distances to all destinations
- **Link State**: Each router broadcasts its local topology
- **Shortest Path**: Minimizes total cost (hops, delay, bandwidth)

## Performance Comparison

### ABP vs GBN

| Metric | ABP | GBN |
|--------|-----|-----|
| Throughput | Low | High |
| Complexity | Simple | Moderate |
| Window Size | 1 | N |
| ACK Type | Individual | Cumulative |
| Efficiency | ~50% | 70-90% |

## Expected Output

### Throughput Analysis
```
[Time: 10s] ABP Throughput: 500 Kbps
[Time: 10s] GBN Throughput: 850 Kbps

Protocol Performance Summary:
- ABP: 500 Kbps average
- GBN: 850 Kbps average
- Improvement: 70%
```

### Routing Table
```
Router R0 Routing Table:
Destination | Next Hop | Cost
R0          | Direct   | 0
R1          | R1       | 10
R2          | R2       | 20
R3          | R1       | 30
```

## Experiments to Try

1. **Vary Loss Rate**: Observe protocol behavior under different error rates
   ```bash
   --Loss=0.001  # Low loss
   --Loss=0.05   # High loss
   --Loss=0.1    # Very high loss
   ```

2. **Vary Window Size**: Analyze impact of window size on GBN
   - Test window sizes: 1, 2, 4, 8, 16

3. **Vary Link Delay**: Observe throughput with different propagation delays

4. **Measure Fairness**: Compare congestion control algorithms

## Extensions

1. Implement Selective Repeat ARQ protocol
2. Add random packet delays
3. Compare with TCP congestion control
4. Add link failures and recovery
5. Implement adaptive timeout mechanisms

## Troubleshooting

- **Module not found**: Ensure files are in `scratch/` directory
- **Compilation errors**: Check NS-3 version compatibility
- **No output**: Enable logging with `NS_LOG=*`

## Related Concepts

- Error detection and correction
- Flow control
- Congestion control
- Routing protocols (RIP, OSPF)
- Network simulator architecture

---

**Last Updated**: May 2026
