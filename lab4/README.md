# Lab 4: Packet Scheduling Algorithms

## Overview

This lab implements and compares queue scheduling algorithms used in routers and network devices to manage packet transmission fairly and efficiently. Scheduling algorithms determine which packet to transmit next when multiple packets are waiting.

## Objectives

- Understand packet scheduling algorithms
- Implement FCFS (First Come First Served) scheduling
- Implement WFQ (Weighted Fair Queuing) scheduling
- Measure fairness and resource allocation
- Analyze QoS (Quality of Service) implications
- Simulate realistic network traffic

## Files

### `source_code.cpp`
Comprehensive implementation of packet scheduling algorithms.

**Key Data Structures:**
- `Packet`: Represents a single packet with metadata
  - Packet ID
  - Source ID
  - Size (in bits)
  - Generation time
  - Finish number (for WFQ)
  
- `Source`: Represents a traffic source
  - Source ID
  - Packet rate (packets per second)
  - Packet size range (min-max)
  - Weight for WFQ
  - Active time window

## Scheduling Algorithms

### FCFS (First Come First Served)

**Algorithm:**
1. Packets arrive and are placed in a FIFO queue
2. Always serve the packet that arrived first
3. Simple and fair for equal-sized packets

**Characteristics:**
- **Fairness**: Not weight-aware
- **Complexity**: O(1) for enqueue/dequeue
- **Delay**: Variable, dependent on packet sizes
- **Suitable for**: Basic networks without QoS requirements

**Behavior:**
```
Arrival: P1(100 bits), P2(500 bits), P3(100 bits)
Service Order: P1 → P2 → P3
```

### WFQ (Weighted Fair Queuing)

**Algorithm:**
1. Assign weights to each traffic source
2. Calculate "finish number" for each packet
3. Virtual time progresses as packets are transmitted
4. Always serve packet with smallest finish number

**Finish Number Calculation:**
```
F(i) = max(F(i-1), A(i)) + P(i) / W(i)
```
Where:
- F(i) = Finish number of packet i
- A(i) = Arrival time of packet i
- P(i) = Packet size
- W(i) = Weight of source

**Characteristics:**
- **Fairness**: Weight-proportional fairness
- **Complexity**: O(log N) for priority queue operations
- **Delay**: More predictable, bounded delay
- **Suitable for**: Networks with QoS requirements

**Benefits:**
- Guaranteed bandwidth allocation
- Bounded latency
- Isolation between flows
- Supports priority-based scheduling

## Traffic Modeling

### Exponential Distribution
- Packet inter-arrival times follow exponential distribution
- Realistic for many real-world scenarios
- Parameterized by λ (packet rate)

### Packet Size Distribution
- Random size between l_min and l_max
- Can simulate mixed traffic patterns
- More realistic than uniform packet sizes

## Performance Metrics

### 1. Throughput
Total data transmitted per unit time
```
Throughput = Total Bits Transmitted / Simulation Time
```

### 2. Average Latency
Average time from packet generation to service completion
```
Latency = (Service Time - Generation Time) for each packet
```

### 3. Fairness Index (Jain's Fairness Index)
Measures how equally resources are distributed
```
Fairness = (Σ throughput_i)² / (N × Σ throughput_i²)
Range: [1/N, 1]  where 1 is perfectly fair
```

### 4. Delay Distribution
Statistical distribution of packet delays
- Min, Max, Average, Median, P95, P99

### 5. Bandwidth Utilization
```
Utilization = Actual Throughput / Link Capacity
```

## Compilation

```bash
# Standard C++11 compiler
g++ -std=c++11 -O2 -o scheduler source_code.cpp

# Or with clang
clang++ -std=c++11 -O2 -o scheduler source_code.cpp
```

## Execution

### Basic Run
```bash
./scheduler
```

### With Configuration
Configuration can be modified through:
1. Command-line arguments
2. Configuration file
3. Hard-coded parameters in source

**Typical Configuration:**
```cpp
SchedulerType scheduler = SchedulerType::FCFS;  // or WFQ
double simulation_time = 100.0;  // seconds
double link_bandwidth = 1e6;     // 1 Mbps
```

## Output Analysis

### Console Output
```
=== Scheduler Simulation Results ===

FCFS Scheduler:
- Total Packets: 1000
- Total Throughput: 950 Kbps
- Average Latency: 125.5 ms
- Fairness Index: 0.65

WFQ Scheduler (Weights: [1, 2, 1]):
- Total Packets: 1000
- Total Throughput: 950 Kbps
- Average Latency: 95.3 ms
- Fairness Index: 0.95

Source Throughputs:
  Source 0: 316 Kbps (33%)
  Source 1: 633 Kbps (67%)
  Source 2: 0 Kbps (0%)
```

### Output Files
- `scheduler_output.csv`: Detailed packet-level statistics
- `latency_distribution.dat`: Delay histogram data
- `fairness_analysis.txt`: Fairness metrics

## Experiments

### Experiment 1: Effect of Weights in WFQ
```cpp
Sources: [1, 2, 1]  // Equal weight
         [1, 3, 1]  // Different weights
         [1, 5, 1]  // High weight difference
```

Expected: Throughput allocation matches weight ratios

### Experiment 2: Effect of Packet Size Distribution
```cpp
Uniform sizes vs. Mixed sizes
Uniform: l_min = l_max (constant size)
Mixed: l_min = 100, l_max = 1500 (realistic)
```

Expected: Variable sizes impact fairness differently

### Experiment 3: Load Variation
```cpp
Light load:   λ = 100 packets/sec
Medium load:  λ = 500 packets/sec
Heavy load:   λ = 1000 packets/sec
```

Expected: Latency increases with load, fairness maintained for WFQ

### Experiment 4: Comparison Table

| Metric | FCFS | WFQ |
|--------|------|-----|
| Simple sizes (equal) | Fair | Fair |
| Mixed sizes | Unfair | Fair |
| Variable weights | N/A | Fair |
| Computational complexity | Low | High |
| Worst-case latency | Unbounded | Bounded |

## Key Concepts

### Fairness
- **Bandwidth fairness**: Each source gets proportional share
- **Delay fairness**: Bounded and predictable delays
- **Important for**: Multi-user networks, SaaS platforms

### Quality of Service (QoS)
Guarantees for:
- Minimum bandwidth
- Maximum latency
- Packet loss rate
- Jitter control

### Scheduler Types
1. **Work-Conserving**: Transmit whenever link is available
2. **Non-Work-Conserving**: May idle link for fairness
3. **Preemptive**: Can interrupt transmission
4. **Non-Preemptive**: Current packet completes

## Advanced Topics

1. **Deficit Round Robin (DRR)**: Simpler alternative to WFQ
2. **Hierarchical Scheduling**: Multi-level priority queues
3. **Rate Limiting**: Token bucket, leaky bucket algorithms
4. **Congestion Management**: RED (Random Early Detection)

## Real-World Applications

- **Network Routers**: Fair bandwidth allocation
- **Cloud Data Centers**: Resource sharing between VMs
- **Video Streaming**: Priority-based streaming quality
- **IoT Gateways**: Resource-constrained devices
- **Network Switches**: QoS implementation

## Extensions

1. Add three-tier scheduling hierarchy
2. Implement Deficit Round Robin (DRR)
3. Add dynamic weight adjustment
4. Simulate link failures
5. Implement congestion notification
6. Add rate limiting mechanisms

## Common Issues

1. **Division by zero**: Handle zero weights appropriately
2. **Integer overflow**: Use `long long` for packet IDs and times
3. **Virtual time precision**: Use double precision for calculations
4. **Queue management**: Handle empty queues correctly

## Related Concepts

- Queueing theory
- Markov chains
- Network calculus
- Convex optimization
- Game theory (fairness)

---

**Last Updated**: May 2026
