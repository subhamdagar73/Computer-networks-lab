/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * TCP Congestion Control Comparison - FULLY CORRECTED
 * CS342 Assignment 3 - Problem 2
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include <fstream>
#include <string>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpCongestionControl");

// Global variables for tracking
std::ofstream cwndFile;
std::ofstream dropFile;
std::ofstream bytesFile;
uint32_t totalDrops = 0;
uint64_t totalTcpBytes = 0;
Ptr<PacketSink> tcpSink;

// Callback for congestion window changes
static void
CwndChange(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndFile << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

// Callback for queue disc drops
static void
QueueDiscDrop(Ptr<const QueueDiscItem> item)
{
    // Check if it's a TCP packet by examining the IP header
    Ptr<const Ipv4QueueDiscItem> ipItem = DynamicCast<const Ipv4QueueDiscItem>(item);
    if (ipItem)
    {
        const Ipv4Header& ipHeader = ipItem->GetHeader();
        if (ipHeader.GetProtocol() == 6) // TCP protocol number
        {
            totalDrops++;
            dropFile << Simulator::Now().GetSeconds() << "\t" << totalDrops << std::endl;
        }
    }
}

// Periodic callback to track bytes received at sink
static void
TrackBytes()
{
    if (tcpSink)
    {
        uint64_t currentBytes = tcpSink->GetTotalRx();
        bytesFile << Simulator::Now().GetSeconds() << "\t" << currentBytes << std::endl;
    }
    // Schedule next call
    Simulator::Schedule(MilliSeconds(10), &TrackBytes);
}

// Trace congestion window - try multiple times to ensure connection is established
void
TraceCwnd(uint32_t nodeId, uint32_t socketId)
{
    std::ostringstream oss;
    oss << "/NodeList/" << nodeId << "/$ns3::TcpL4Protocol/SocketList/" << socketId << "/CongestionWindow";
    
    bool connected = Config::ConnectWithoutContextFailSafe(oss.str(), MakeCallback(&CwndChange));
    
    if (!connected)
    {
        // Try again after a short delay
        if (Simulator::Now().GetSeconds() < 0.1)
        {
            Simulator::Schedule(MilliSeconds(10), &TraceCwnd, nodeId, socketId);
        }
    }
}

void
RunSimulation(std::string tcpVariant)
{
    NS_LOG_INFO("Running simulation for " << tcpVariant);

    // Set TCP variant
    if (tcpVariant == "TcpWestwood")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", 
                          StringValue("ns3::TcpWestwoodPlus"));
    }
    else
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", 
                          StringValue("ns3::" + tcpVariant));
    }

    // Set TCP parameters for better behavior
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(1));
    Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(Seconds(0.2)));
    Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue(MilliSeconds(1)));
    
    // Set initial congestion window
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));
    
    // Set device queue to minimum
    pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1p"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Install traffic control with proper queue size
    // BDP = 1 Mbps * 20ms = 20000 bits = 2500 bytes ≈ 2.5 packets
    // Using 10 packets as specified
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue("10p"));
    QueueDiscContainer qdiscs = tch.Install(devices);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create FTP application using BulkSendApplication
    uint16_t ftpPort = 9;
    
    BulkSendHelper ftp("ns3::TcpSocketFactory", 
                       InetSocketAddress(interfaces.GetAddress(1), ftpPort));
    ftp.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited
    ftp.SetAttribute("SendSize", UintegerValue(1024));
    
    ApplicationContainer ftpApp = ftp.Install(nodes.Get(0));
    ftpApp.Start(Seconds(0.0));
    ftpApp.Stop(Seconds(2.0));

    // Create packet sink for FTP
    PacketSinkHelper ftpSinkHelper("ns3::TcpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), ftpPort));
    ApplicationContainer ftpSinkApp = ftpSinkHelper.Install(nodes.Get(1));
    ftpSinkApp.Start(Seconds(0.0));
    ftpSinkApp.Stop(Seconds(2.5));
    
    // Store sink pointer for byte tracking
    tcpSink = DynamicCast<PacketSink>(ftpSinkApp.Get(0));

    // Create 5 CBR applications using UDP
    uint16_t cbrPort = 10;
    double cbrStarts[] = {0.3, 0.5, 0.7, 0.8, 1.0};
    double cbrStops[] = {2.0, 2.0, 1.1, 1.5, 1.6};

    for (int i = 0; i < 5; i++)
    {
        OnOffHelper cbr("ns3::UdpSocketFactory",
                       InetSocketAddress(interfaces.GetAddress(1), cbrPort + i));
        cbr.SetAttribute("DataRate", StringValue("500Kbps"));
        cbr.SetAttribute("PacketSize", UintegerValue(512));
        cbr.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        cbr.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer cbrApp = cbr.Install(nodes.Get(0));
        cbrApp.Start(Seconds(cbrStarts[i]));
        cbrApp.Stop(Seconds(cbrStops[i]));

        // Create packet sink for CBR
        PacketSinkHelper cbrSinkHelper("ns3::UdpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), cbrPort + i));
        ApplicationContainer cbrSinkApp = cbrSinkHelper.Install(nodes.Get(1));
        cbrSinkApp.Start(Seconds(0.0));
        cbrSinkApp.Stop(Seconds(2.5));
    }

    // Open output files
    cwndFile.open("cwnd_" + tcpVariant + ".dat");
    dropFile.open("drops_" + tcpVariant + ".dat");
    bytesFile.open("bytes_" + tcpVariant + ".dat");

    if (!cwndFile.is_open() || !dropFile.is_open() || !bytesFile.is_open())
    {
        NS_FATAL_ERROR("Failed to open output files");
    }

    // Reset counters
    totalDrops = 0;
    totalTcpBytes = 0;

    // Schedule CWND tracing - try multiple socket IDs
    Simulator::Schedule(Seconds(0.001), &TraceCwnd, 0, 0);
    Simulator::Schedule(Seconds(0.05), &TraceCwnd, 0, 0);
    Simulator::Schedule(Seconds(0.01), &TraceCwnd, 0, 0);

    // Connect queue disc drop trace
    qdiscs.Get(0)->TraceConnectWithoutContext("Drop", MakeCallback(&QueueDiscDrop));

    // Start periodic byte tracking
    Simulator::Schedule(Seconds(0.001), &TrackBytes);

    // Install FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(2.0));
    Simulator::Run();

    // Get flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n=== " << tcpVariant << " Statistics ===" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    uint32_t tcpFlowCount = 0;
    double totalTcpThroughput = 0.0;
    uint64_t totalTcpRxBytes = 0;
    uint32_t totalTcpLostPackets = 0;
    
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); 
         i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        
        if (t.protocol == 6 && t.destinationPort == ftpPort) // TCP FTP flow (data direction)
        {
            tcpFlowCount++;
            std::cout << "\nTCP Flow (FTP Data):" << std::endl;
            std::cout << "  Direction:     " << t.sourceAddress << " → " << t.destinationAddress << std::endl;
            std::cout << "  Tx Packets:    " << i->second.txPackets << std::endl;
            std::cout << "  Tx Bytes:      " << i->second.txBytes << std::endl;
            std::cout << "  Rx Packets:    " << i->second.rxPackets << std::endl;
            std::cout << "  Rx Bytes:      " << i->second.rxBytes << std::endl;
            std::cout << "  Lost Packets:  " << i->second.lostPackets << std::endl;
            std::cout << "  Throughput:    " << i->second.rxBytes * 8.0 / 2.0 / 1000000.0 << " Mbps" << std::endl;
            
            totalTcpThroughput += i->second.rxBytes * 8.0 / 2.0 / 1000000.0;
            totalTcpRxBytes += i->second.rxBytes;
            totalTcpLostPackets += i->second.lostPackets;
        }
    }
    
    std::cout << "\n" << std::string(60, '-') << std::endl;
    std::cout << "Summary:" << std::endl;
    std::cout << "  Total TCP Flows:        " << tcpFlowCount << std::endl;
    std::cout << "  Total TCP Throughput:   " << totalTcpThroughput << " Mbps" << std::endl;
    std::cout << "  Total TCP Rx Bytes:     " << totalTcpRxBytes << " bytes" << std::endl;
    std::cout << "  Total TCP Lost Packets: " << totalTcpLostPackets << std::endl;
    std::cout << "  Total TCP Drops (Queue):" << totalDrops << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    // Close files
    cwndFile.close();
    dropFile.close();
    bytesFile.close();

    // Reset sink pointer
    tcpSink = 0;

    Simulator::Destroy();
}

int
main(int argc, char* argv[])
{
    // Disable ns-3 logging unless explicitly needed
    // LogComponentEnable("TcpCongestionControl", LOG_LEVEL_INFO);

    // List of TCP variants to test
    std::vector<std::string> tcpVariants = {
        "TcpNewReno",
        "TcpVeno",
        "TcpHybla",
        "TcpHighSpeed",
        "TcpVegas",
        "TcpWestwood"
    };

    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "TCP CONGESTION CONTROL COMPARISON SIMULATION" << std::endl;
    std::cout << "CS342 Assignment 3 - Problem 2" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    std::cout << "\nSimulation Parameters:" << std::endl;
    std::cout << "  Link Bandwidth:  1 Mbps" << std::endl;
    std::cout << "  Link Delay:      10 ms" << std::endl;
    std::cout << "  Queue Size:      10 packets" << std::endl;
    std::cout << "  Simulation Time: 2.0 seconds" << std::endl;
    std::cout << "  FTP Traffic:     0.0s - 2.0s" << std::endl;
    std::cout << "  CBR Flows:       5 flows @ 500 Kbps each" << std::endl;
    std::cout << std::string(70, '=') << std::endl;

    // Run simulation for each TCP variant
    for (const auto& variant : tcpVariants)
    {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "Testing: " << variant << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        RunSimulation(variant);
        
        // Small delay between simulations
        std::cout << std::endl;
    }

    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "ALL SIMULATIONS COMPLETED SUCCESSFULLY!" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    std::cout << "\nGenerated Output Files:" << std::endl;
    std::cout << "  ├─ cwnd_*.dat   - Congestion window traces" << std::endl;
    std::cout << "  ├─ drops_*.dat  - Cumulative TCP packet drops" << std::endl;
    std::cout << "  └─ bytes_*.dat  - Cumulative bytes received" << std::endl;
    std::cout << "\nNext Step: Run plotting script to generate graphs" << std::endl;
    std::cout << "  python3 plot_tcp_results.py" << std::endl;
    std::cout << std::string(70, '=') << std::endl << std::endl;

    return 0;
}