 /* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

/*

 * Go-Back-N Protocol Implementation in NS-3

 */


#include "ns3/core-module.h"

#include "ns3/network-module.h"

#include "ns3/internet-module.h"

#include "ns3/point-to-point-module.h"

#include "ns3/applications-module.h"

#include "ns3/error-model.h"

#include <fstream>

#include <deque>

#include <vector> // <-- added


using namespace ns3;


NS_LOG_COMPONENT_DEFINE("GoBackNProtocol");


// Custom packet header for GBN

class GbnHeader : public Header

{

public:

    GbnHeader();

    virtual ~GbnHeader();


    void SetSeqNum(uint32_t seq);

    uint32_t GetSeqNum() const;

    void SetAck(bool isAck);

    bool IsAck() const;

    void SetChecksum(uint32_t checksum);

    uint32_t GetChecksum() const;


    static TypeId GetTypeId();

    virtual TypeId GetInstanceTypeId() const;

    virtual void Print(std::ostream &os) const;

    virtual uint32_t GetSerializedSize() const;

    virtual void Serialize(Buffer::Iterator start) const;

    virtual uint32_t Deserialize(Buffer::Iterator start);


private:

    uint32_t m_seqNum;

    uint32_t m_checksum;

    bool m_isAck;

};


GbnHeader::GbnHeader()

    : m_seqNum(0), m_checksum(0), m_isAck(false)

{

}


GbnHeader::~GbnHeader()

{

}


void GbnHeader::SetSeqNum(uint32_t seq)

{

    m_seqNum = seq;

}


uint32_t GbnHeader::GetSeqNum() const

{

    return m_seqNum;

}


void GbnHeader::SetAck(bool isAck)

{

    m_isAck = isAck;

}


bool GbnHeader::IsAck() const

{

    return m_isAck;

}


void GbnHeader::SetChecksum(uint32_t checksum)

{

    m_checksum = checksum;

}


uint32_t GbnHeader::GetChecksum() const

{

    return m_checksum;

}


TypeId GbnHeader::GetTypeId()

{

    static TypeId tid = TypeId("ns3::GbnHeader")

        .SetParent<Header>()

        .AddConstructor<GbnHeader>();

    return tid;

}


TypeId GbnHeader::GetInstanceTypeId() const

{

    return GetTypeId();

}


void GbnHeader::Print(std::ostream &os) const

{

    os << "SeqNum=" << m_seqNum << " IsAck=" << m_isAck << " Checksum=" << m_checksum;

}


uint32_t GbnHeader::GetSerializedSize() const

{

    return 9; // 4 bytes seqnum, 4 bytes checksum, 1 byte isAck

}


void GbnHeader::Serialize(Buffer::Iterator start) const

{

    start.WriteHtonU32(m_seqNum);

    start.WriteHtonU32(m_checksum);

    start.WriteU8(m_isAck ? 1 : 0);

}


uint32_t GbnHeader::Deserialize(Buffer::Iterator start)

{

    m_seqNum = start.ReadNtohU32();

    m_checksum = start.ReadNtohU32();

    m_isAck = start.ReadU8() == 1;

    return GetSerializedSize();

}


// GBN Sender Application

class GbnSender : public Application

{

public:

    GbnSender();

    virtual ~GbnSender();


    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, 

               uint32_t nPackets, DataRate dataRate, uint32_t windowSize);


private:

    virtual void StartApplication();

    virtual void StopApplication();


    void SendPackets();

    void ReceiveAck(Ptr<Socket> socket);

    void HandleTimeout();

    uint32_t CalculateChecksum(Ptr<Packet> packet);


    Ptr<Socket> m_socket;

    Address m_peer;

    uint32_t m_packetSize;

    uint32_t m_nPackets;

    DataRate m_dataRate;

    uint32_t m_windowSize;

    

    EventId m_sendEvent;

    EventId m_timeoutEvent;

    bool m_running;

    

    uint32_t m_base;           // Base of the window

    uint32_t m_nextSeqNum;     // Next sequence number to send

    uint32_t m_totalPackets;   // Total packets to send

    

    std::deque<Ptr<Packet>> m_sentPackets;

    Time m_timeout;

    uint32_t m_retransmissions;

    uint32_t m_goodput;

    uint32_t m_totalBytesSent;

    Time m_startTime;


    std::ofstream m_windowFile;

    std::ofstream m_lossFile;

    std::ofstream m_goodputFile;

    std::ofstream m_retransFile;

};


GbnSender::GbnSender()

    : m_socket(0),

      m_peer(),

      m_packetSize(0),

      m_nPackets(0),

      m_windowSize(4),

      m_sendEvent(),

      m_timeoutEvent(),

      m_running(false),

      m_base(0),

      m_nextSeqNum(0),

      m_totalPackets(0),

      m_timeout(Seconds(2.0)),

      m_retransmissions(0),

      m_goodput(0),

      m_totalBytesSent(0)

{

}


GbnSender::~GbnSender()

{

    m_socket = 0;

}


void GbnSender::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, 

                      uint32_t nPackets, DataRate dataRate, uint32_t windowSize)

{

    m_socket = socket;

    m_peer = address;

    m_packetSize = packetSize;

    m_nPackets = nPackets;

    m_totalPackets = nPackets;

    m_dataRate = dataRate;

    m_windowSize = windowSize;

}


void GbnSender::StartApplication()

{

    m_running = true;

    m_base = 0;

    m_nextSeqNum = 0;

    m_startTime = Simulator::Now();


    m_socket->Bind();

    m_socket->Connect(m_peer);

    m_socket->SetRecvCallback(MakeCallback(&GbnSender::ReceiveAck, this));


    m_windowFile.open("gbn_window.dat");

    m_lossFile.open("gbn_loss.dat");

    m_goodputFile.open("gbn_goodput.dat");

    m_retransFile.open("gbn_retransmissions.dat");


    SendPackets();

}


void GbnSender::StopApplication()

{

    m_running = false;


    // Use IsPending() on EventId as per ns-3.46 API

    if (m_sendEvent.IsPending())

    {

        Simulator::Cancel(m_sendEvent);

    }

    if (m_timeoutEvent.IsPending())

    {

        Simulator::Cancel(m_timeoutEvent);

    }

    if (m_socket)

    {

        m_socket->Close();

    }


    m_windowFile.close();

    m_lossFile.close();

    m_goodputFile.close();

    m_retransFile.close();


    NS_LOG_INFO("Total retransmissions: " << m_retransmissions);

    NS_LOG_INFO("Total goodput bytes: " << m_goodput);

}


uint32_t GbnSender::CalculateChecksum(Ptr<Packet> packet)

{

    uint32_t checksum = 0;

    // avoid VLAs: use vector

    std::vector<uint8_t> buffer(m_packetSize);

    packet->CopyData(buffer.data(), m_packetSize);

    for (uint32_t i = 0; i < m_packetSize; i++)

    {

        checksum += buffer[i];

    }

    return checksum;

}


void GbnSender::SendPackets()

{

    if (!m_running)

    {

        return;

    }


    // Send packets within window

    while (m_nextSeqNum < m_base + m_windowSize && m_nextSeqNum < m_totalPackets)

    {

        Ptr<Packet> packet = Create<Packet>(m_packetSize);

        GbnHeader gbnHeader;

        gbnHeader.SetSeqNum(m_nextSeqNum);

        gbnHeader.SetAck(false);

        gbnHeader.SetChecksum(CalculateChecksum(packet));

        packet->AddHeader(gbnHeader);


        m_sentPackets.push_back(packet->Copy());

        m_socket->Send(packet);

        m_totalBytesSent += m_packetSize;


        NS_LOG_INFO("Sent packet with SeqNum: " << m_nextSeqNum);

        m_nextSeqNum++;


        // Record window utilization

        double elapsed = (Simulator::Now() - m_startTime).GetSeconds();

        double windowUtil = static_cast<double>(m_nextSeqNum - m_base) / m_windowSize;

        m_windowFile << elapsed << " " << windowUtil << std::endl;

    }


    // Start timer if not already running and there are unacked packets

    if (m_base == m_nextSeqNum)

{

    if (m_timeoutEvent.IsPending())

        Simulator::Cancel(m_timeoutEvent);

}

else

{

    if (m_timeoutEvent.IsPending())

        Simulator::Cancel(m_timeoutEvent);

    m_timeoutEvent = Simulator::Schedule(m_timeout, &GbnSender::HandleTimeout, this);

}


}


void GbnSender::ReceiveAck(Ptr<Socket> socket)

{

    Ptr<Packet> packet;

    Address from;

    while ((packet = socket->RecvFrom(from)))

    {

        GbnHeader gbnHeader;

        packet->RemoveHeader(gbnHeader);


        if (gbnHeader.IsAck())

        {

            uint32_t ackNum = gbnHeader.GetSeqNum();

            NS_LOG_INFO("Received cumulative ACK: " << ackNum);


            if (ackNum >= m_base && ackNum < m_nextSeqNum)

            {

                // Update base to ackNum + 1

                uint32_t packetsAcked = ackNum - m_base + 1;

                m_base = ackNum + 1;

                m_goodput += packetsAcked * m_packetSize;


                // Remove acknowledged packets from buffer

                for (uint32_t i = 0; i < packetsAcked && !m_sentPackets.empty(); i++)

                {

                    m_sentPackets.pop_front();

                }


                // Record goodput

                double elapsed = (Simulator::Now() - m_startTime).GetSeconds();
double goodputRate = (m_goodput * 8.0) / (elapsed * 1000000.0); // Mbps
m_goodputFile << elapsed << " " << goodputRate << std::endl;



                // Cancel timer if window is empty

                if (m_base == m_nextSeqNum)

                {

                    if (m_timeoutEvent.IsPending())

                    {

                        Simulator::Cancel(m_timeoutEvent);

                    }

                }

                else

                {

                    // Restart timer

                    if (m_timeoutEvent.IsPending())

                    {

                        Simulator::Cancel(m_timeoutEvent);

                    }

                    m_timeoutEvent = Simulator::Schedule(m_timeout, &GbnSender::HandleTimeout, this);

                }


                // Send more packets if possible

                SendPackets();

            }

            else{

                NS_LOG_INFO("Ignoring duplicate ACK: " << ackNum);

            }

        }

    }

}


// void GbnSender::HandleTimeout()

// {

//     NS_LOG_INFO("Timeout occurred, retransmitting from SeqNum: " << m_base);

//     m_retransmissions++;


//     double elapsed = (Simulator::Now() - m_startTime).GetSeconds();

//     m_retransFile << elapsed << " " << m_retransmissions << std::endl;


//     // Calculate packet loss rate

//     if (m_nextSeqNum > 0)

//     {

//         double lossRate = static_cast<double>(m_retransmissions) / m_nextSeqNum;

//         m_lossFile << Simulator::Now().GetSeconds() << " " << lossRate << std::endl;

//     }


//     // Retransmit all packets in window

//     for (uint32_t i = 0; i < m_sentPackets.size(); ++i)

// {

//     m_socket->Send(m_sentPackets[i]->Copy());

//     NS_LOG_INFO("Retransmitted packet with SeqNum: " << (m_base + i));

// }



//     // Restart timer

//     m_timeoutEvent = Simulator::Schedule(m_timeout, &GbnSender::HandleTimeout, this);

// }
void GbnSender::HandleTimeout()
{
    NS_LOG_INFO("Timeout occurred, retransmitting from SeqNum: " << m_base);

    // Retransmit all packets in the current window
    for (uint32_t i = 0; i < m_sentPackets.size(); ++i)
    {
        Ptr<Packet> packet = m_sentPackets[i]->Copy();
        m_socket->Send(packet);

        // Update retransmission count
        m_retransmissions++;

        // Record the retransmission in file with timestamp and sequence number
        double elapsed = (Simulator::Now() - m_startTime).GetSeconds();
        m_retransFile << elapsed << " " << (m_base + i) << std::endl;
        m_retransFile.flush();  // ensure it writes immediately

        NS_LOG_INFO("Retransmitted packet with SeqNum: " << (m_base + i));
    }

    // Update packet loss rate in the loss file
    if (m_nextSeqNum > 0)
    {
        double elapsed = (Simulator::Now() - m_startTime).GetSeconds();
        double lossRate = static_cast<double>(m_retransmissions) / m_nextSeqNum;
        m_lossFile << elapsed << " " << lossRate << std::endl;
        m_lossFile.flush();
    }

    // Restart the timeout timer
    if (m_timeoutEvent.IsPending() || m_timeoutEvent.IsExpired())
    {
        Simulator::Cancel(m_timeoutEvent);
    }
    m_timeoutEvent = Simulator::Schedule(m_timeout, &GbnSender::HandleTimeout, this);
}



// GBN Receiver Application

class GbnReceiver : public Application

{

public:

    GbnReceiver();

    virtual ~GbnReceiver();


    void Setup(Ptr<Socket> socket);


private:

    virtual void StartApplication();

    virtual void StopApplication();


    void HandleRead(Ptr<Socket> socket);

    void SendAck(uint32_t seqNum,Address to);

    uint32_t CalculateChecksum(Ptr<Packet> packet, uint32_t dataSize);


    Ptr<Socket> m_socket;

    uint32_t m_expectedSeqNum;

    uint32_t m_packetsReceived;

};


GbnReceiver::GbnReceiver()

    : m_socket(0),

      m_expectedSeqNum(0),

      m_packetsReceived(0)

{

}


GbnReceiver::~GbnReceiver()

{

    m_socket = 0;

}


void GbnReceiver::Setup(Ptr<Socket> socket)

{

    m_socket = socket;

}


void GbnReceiver::StartApplication()

{

    m_socket->Bind();

    m_socket->SetRecvCallback(MakeCallback(&GbnReceiver::HandleRead, this));

}


void GbnReceiver::StopApplication()

{

    if (m_socket)

    {

        m_socket->Close();

        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());

    }

    NS_LOG_INFO("Total packets received correctly: " << m_packetsReceived);

}


uint32_t GbnReceiver::CalculateChecksum(Ptr<Packet> packet, uint32_t dataSize)

{

    uint32_t checksum = 0;

    // avoid VLA: use vector

    std::vector<uint8_t> buffer(dataSize);

    packet->CopyData(buffer.data(), dataSize);

    for (uint32_t i = 0; i < dataSize; i++)

    {

        checksum += buffer[i];

    }

    return checksum;

}

void GbnReceiver::HandleRead(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        GbnHeader gbnHeader;
        packet->RemoveHeader(gbnHeader);

        if (!gbnHeader.IsAck())
        {
            uint32_t dataSize = packet->GetSize();
            uint32_t receivedChecksum = gbnHeader.GetChecksum();
            uint32_t calculatedChecksum = CalculateChecksum(packet, dataSize);
            uint32_t seqNum = gbnHeader.GetSeqNum();

            // Check if the packet is not corrupt and is the one we expect
            if (receivedChecksum == calculatedChecksum && seqNum == m_expectedSeqNum)
            {
                NS_LOG_INFO("Received in-order packet with SeqNum: " << seqNum);
                m_packetsReceived++;
                
                // *** THE FIX ***
                // Send a cumulative ACK for the sequence number we just received.
                SendAck(m_expectedSeqNum, from);
                
                // Increment the expected sequence number for the next packet
                m_expectedSeqNum++;
            }
            else
            {
                if (receivedChecksum != calculatedChecksum)
                {
                    NS_LOG_INFO("Received corrupted packet");
                }
                else if (seqNum < m_expectedSeqNum)
                {
                    NS_LOG_INFO("Received duplicate packet with SeqNum: " << seqNum);
                }
                else // seqNum > m_expectedSeqNum
                {
                    NS_LOG_INFO("Received out-of-order packet with SeqNum: " << seqNum
                                << ", expected: " << m_expectedSeqNum);
                }
                
                // For any other case (duplicate, out-of-order, corrupted),
                // GBN requires resending the ACK for the last correctly received packet.
                if (m_expectedSeqNum > 0)
                {
                    SendAck(m_expectedSeqNum - 1, from);
                }
            }
        }
    }
}


void GbnReceiver::SendAck(uint32_t seqNum,Address to)

{

    Ptr<Packet> ackPacket = Create<Packet>(1);

    GbnHeader ackHeader;

    ackHeader.SetSeqNum(seqNum);

    ackHeader.SetAck(true);

    ackHeader.SetChecksum(0);

    ackPacket->AddHeader(ackHeader);

   // m_socket->Send(ackPacket);

    m_socket->SendTo(ackPacket, 0, to);

    NS_LOG_INFO("Sent cumulative ACK for SeqNum: " << seqNum);

}


// Main simulation

int main(int argc, char *argv[])

{

    uint32_t packetSize = 1024;

    uint32_t nPackets = 100;

    uint32_t windowSize = 4;

    double lossRate = 0.1;

    double corruptionRate = 0.05;


    CommandLine cmd;

    cmd.AddValue("packetSize", "Size of packets", packetSize);

    cmd.AddValue("nPackets", "Number of packets", nPackets);

    cmd.AddValue("windowSize", "Window size for GBN", windowSize);

    cmd.AddValue("lossRate", "Packet loss rate", lossRate);

    cmd.AddValue("corruptionRate", "Packet corruption rate", corruptionRate);

    cmd.Parse(argc, argv);


    LogComponentEnable("GoBackNProtocol", LOG_LEVEL_INFO);


    // Create nodes

    NodeContainer nodes;

    nodes.Create(2);


    // Create point-to-point link

    PointToPointHelper pointToPoint;

    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));

    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));


    NetDeviceContainer devices;

    devices = pointToPoint.Install(nodes);


    // Install error model

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();

    em->SetAttribute("ErrorRate", DoubleValue(lossRate));

    // use enum value for ErrorUnit (ns-3.46)

    em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));


    // Install Internet stack

    InternetStackHelper stack;

    stack.Install(nodes);


    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);


    // Create sockets

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

    Ptr<Socket> senderSocket = Socket::CreateSocket(nodes.Get(0), tid);

    Ptr<Socket> receiverSocket = Socket::CreateSocket(nodes.Get(1), tid);


    // Setup sender

    Ptr<GbnSender> sender = CreateObject<GbnSender>();

    InetSocketAddress senderAddress(interfaces.GetAddress(1), 9);

    sender->Setup(senderSocket, senderAddress, packetSize, nPackets, DataRate("500Kbps"), windowSize);

    nodes.Get(0)->AddApplication(sender);

    sender->SetStartTime(Seconds(1.0));

    sender->SetStopTime(Seconds(20.0));


    // Setup receiver

    Ptr<GbnReceiver> receiver = CreateObject<GbnReceiver>();

    InetSocketAddress receiverAddress(Ipv4Address::GetAny(), 9);

    receiverSocket->Bind(receiverAddress);

    receiver->Setup(receiverSocket);

    nodes.Get(1)->AddApplication(receiver);

    receiver->SetStartTime(Seconds(0.0));

    receiver->SetStopTime(Seconds(20.0));


    Simulator::Stop(Seconds(20.0));

    Simulator::Run();

    Simulator::Destroy();


    return 0;

} 