/* abp.cc
   Alternating Bit Protocol (Stop-and-Wait) example for ns-3.
   Place in ns-3/scratch/ and run with waf.

   Command line args:
     --SimTime (seconds)
     --Loss (packet error rate for RatesErrorModel; e.g. 0.01)
     --Payload (payload bytes per data packet)
     --Interval (time between new application requests; sender will attempt to send one every Interval if previous ACK received)
     --Timeout (retransmit timeout in seconds)
*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include <fstream>
#include <deque>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ABPExample");

// Simple header with type (DATA/ACK) and seq
class ABPHeader : public Header
{
public:
  enum Type : uint8_t { DATA = 0, ACK = 1 };
  ABPHeader() : m_type(DATA), m_seq(0), m_payloadSize(0) {}
  void SetType(Type t) { m_type = t; }
  void SetSeq(uint8_t s) { m_seq = s; }
  void SetPayloadSize(uint32_t s) { m_payloadSize = s; }

  Type GetType() const { return m_type; }
  uint8_t GetSeq() const { return m_seq; }
  uint32_t GetPayloadSize() const { return m_payloadSize; }

  static Type ToType(uint8_t b) { return (b == 0 ? DATA : ACK); }

  // ns-3 Header interface
  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const override;
  virtual uint32_t GetSerializedSize(void) const override { return 1 + 1 + 4; }

  virtual void Serialize(Buffer::Iterator start) const override
  {
    start.WriteU8((uint8_t)m_type);
    start.WriteU8(m_seq);
    start.WriteHtonU32(m_payloadSize);
  }

  virtual uint32_t Deserialize(Buffer::Iterator start) override
  {
    m_type = (Type)start.ReadU8();
    m_seq = start.ReadU8();
    m_payloadSize = start.ReadNtohU32();
    return GetSerializedSize();
  }

  virtual void Print(std::ostream &os) const override
  {
    os << "ABPHeader(type=" << (int)m_type << " seq=" << (int)m_seq << " payload=" << m_payloadSize << ")";
  }

private:
  Type m_type;
  uint8_t m_seq;
  uint32_t m_payloadSize;
};

// Definition of static GetTypeId and instance type id
TypeId ABPHeader::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::ABPHeader").SetParent<Header>().AddConstructor<ABPHeader>();
  return tid;
}
TypeId ABPHeader::GetInstanceTypeId(void) const { return GetTypeId(); }

// Sender application - implements Stop-and-Wait ABP
class ABPSender : public Application
{
public:
  ABPSender() : m_socket(0), m_peer(), m_seq(0), m_waitingAck(false), m_payloadSize(1000),
                m_interval(0.05), m_timeout(0.5), m_retxCount(0), m_bytesAcked(0) {}
  virtual ~ABPSender() { m_socket = nullptr; }

  void Setup(Address peer, uint32_t payloadSize, double interval, double timeout, std::string outRetransFile, std::string outThroughputFile)
  {
    m_peer = peer;
    m_payloadSize = payloadSize;
    m_interval = Seconds(interval);
    m_timeout = Seconds(timeout);
    m_outRetrans = outRetransFile;
    m_outThroughput = outThroughputFile;
  }

  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("ABPSender").SetParent<Application>();
    return tid;
  }

private:
  virtual void StartApplication() override
  {
    if (!m_socket)
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(m_peer);
    m_socket->SetRecvCallback(MakeCallback(&ABPSender::HandleRead, this));
    // open files
    m_retxFs.open(m_outRetrans.c_str());
    m_throughFs.open(m_outThroughput.c_str());
    m_retxFs << "time,retransmissions\n";
    m_throughFs << "time,bytes_acked\n";
    // schedule first send
    ScheduleSend(Seconds(0.1));
    // schedule throughput sampling every 0.1s
    m_sampler = Simulator::Schedule(Seconds(0.1), &ABPSender::SampleThroughput, this);
  }

  virtual void StopApplication() override
  {
    if (m_socket) m_socket->Close();

    // correct EventId cancellation for ns-3.46: use IsPending() then Cancel()
    if (m_sendEvent.IsPending()) Simulator::Cancel(m_sendEvent);
    if (m_retxEvent.IsPending()) Simulator::Cancel(m_retxEvent);
    if (m_sampler.IsPending()) Simulator::Cancel(m_sampler);

    if (m_retxFs.is_open()) m_retxFs.close();
    if (m_throughFs.is_open()) m_throughFs.close();
  }

  void ScheduleSend(Time when)
  {
    // Use IsPending() to check scheduled event (ns-3 EventId API)
    if (m_sendEvent.IsPending()) return;
    m_sendEvent = Simulator::Schedule(when, &ABPSender::SendPacket, this);
  }

  void SendPacket()
  {
    if (m_waitingAck)
    {
      // can't send new data while waiting — Stop-and-Wait
      return;
    }
    // build packet with header
    Ptr<Packet> p = Create<Packet>(m_payloadSize);
    ABPHeader h;
    h.SetType(ABPHeader::DATA);
    h.SetSeq(m_seq);
    h.SetPayloadSize(m_payloadSize);
    p->AddHeader(h);
    m_socket->Send(p);
    NS_LOG_INFO("Sent DATA seq=" << (int)m_seq << " at " << Simulator::Now().GetSeconds());
    // start retransmit timer
    m_waitingAck = true;
    m_retxEvent = Simulator::Schedule(m_timeout, &ABPSender::Retransmit, this);
    m_totalSent++;
  }

  void Retransmit()
  {
    m_retxCount++;
    m_retxFs << Simulator::Now().GetSeconds() << "," << m_retxCount << "\n";
    NS_LOG_INFO("Timeout. Retransmitting seq=" << (int)m_seq << " at " << Simulator::Now().GetSeconds());
    // build same packet
    Ptr<Packet> p = Create<Packet>(m_payloadSize);
    ABPHeader h;
    h.SetType(ABPHeader::DATA);
    h.SetSeq(m_seq);
    h.SetPayloadSize(m_payloadSize);
    p->AddHeader(h);
    m_socket->Send(p);
    // restart timer
    m_retxEvent = Simulator::Schedule(m_timeout, &ABPSender::Retransmit, this);
    m_totalSent++;
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> p;
    Address from;
    while ((p = socket->RecvFrom(from)))
    {
      ABPHeader h;
      if (p->GetSize() < (int)h.GetSerializedSize()) continue;
      p->RemoveHeader(h);
      if (h.GetType() == ABPHeader::ACK)
      {
        uint8_t ackSeq = h.GetSeq();
        NS_LOG_INFO("Received ACK " << (int)ackSeq << " at " << Simulator::Now().GetSeconds());
        if (m_waitingAck && ackSeq == m_seq)
        {
          // good ack
          m_waitingAck = false;
          if (m_retxEvent.IsPending()) Simulator::Cancel(m_retxEvent);
          m_bytesAcked += h.GetPayloadSize();
          // flip seq
          m_seq = 1 - m_seq;
          // schedule next send after interval
          ScheduleSend(m_interval);
        }
      }
    }
  }

  void SampleThroughput()
  {
    m_throughFs << Simulator::Now().GetSeconds() << "," << m_bytesAcked << "\n";
    m_sampler = Simulator::Schedule(Seconds(0.1), &ABPSender::SampleThroughput, this);
  }

private:
  Ptr<Socket> m_socket;
  Address m_peer;
  uint8_t m_seq;
  bool m_waitingAck;
  uint32_t m_payloadSize;
  Time m_interval;
  Time m_timeout;
  EventId m_sendEvent;
  EventId m_retxEvent;
  EventId m_sampler;
  uint64_t m_retxCount;
  uint64_t m_totalSent = 0;
  uint64_t m_bytesAcked;
  std::ofstream m_retxFs;
  std::ofstream m_throughFs;
  std::string m_outRetrans;
  std::string m_outThroughput;
};

// Receiver app
class ABPReceiver : public Application
{
public:
  ABPReceiver() : m_socket(0), m_expectedSeq(0), m_bytesDelivered(0) {}
  virtual ~ABPReceiver() { m_socket = nullptr; }

  void Setup(Address local, std::string outGoodputFile)
  {
    m_local = local;
    m_outGoodput = outGoodputFile;
  }

  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("ABPReceiver").SetParent<Application>();
    return tid;
  }

private:
  virtual void StartApplication() override
  {
    if (!m_socket)
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress addr = InetSocketAddress(Ipv4Address::GetAny(), 8080);
    m_socket->Bind(addr);
    m_socket->SetRecvCallback(MakeCallback(&ABPReceiver::HandleRead, this));
    m_goodputFs.open(m_outGoodput.c_str());
    m_goodputFs << "time,bytes_delivered\n";
  }

  virtual void StopApplication() override
  {
    if (m_socket) m_socket->Close();
    if (m_goodputFs.is_open()) m_goodputFs.close();
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> p;
    Address from;
    while ((p = socket->RecvFrom(from)))
    {
      ABPHeader h;
      if (p->GetSize() < (int)h.GetSerializedSize()) continue;
      p->RemoveHeader(h);
      if (h.GetType() == ABPHeader::DATA)
      {
        uint8_t seq = h.GetSeq();
        if (seq == m_expectedSeq)
        {
          // accept and deliver
          m_bytesDelivered += h.GetPayloadSize();
          NS_LOG_INFO("Receiver accepted seq=" << (int)seq << " at " << Simulator::Now().GetSeconds());
          m_expectedSeq = 1 - m_expectedSeq;
        }
        else
        {
          NS_LOG_INFO("Receiver got out-of-order seq=" << (int)seq << " expected=" << (int)m_expectedSeq);
          // duplicate or old packet: ignore
        }
        // always send ACK for last in-order received (cumulative)
        // new: ACK carries the payload size of the data packet that was just processed
ABPHeader ack;
ack.SetType(ABPHeader::ACK);
ack.SetSeq((uint8_t)(1 - m_expectedSeq)); // last received successfully
ack.SetPayloadSize(h.GetPayloadSize());   // <<-- carry the data packet payload size
Ptr<Packet> ackp = Create<Packet>(0);
ackp->AddHeader(ack);
socket->SendTo(ackp, 0, from);

        // log goodput instant (simple)
        m_goodputFs << Simulator::Now().GetSeconds() << "," << m_bytesDelivered << "\n";
      }
    }
  }

private:
  Ptr<Socket> m_socket;
  Address m_local;
  uint8_t m_expectedSeq;
  uint64_t m_bytesDelivered;
  std::ofstream m_goodputFs;
  std::string m_outGoodput;
};

// main
int main(int argc, char *argv[])
{
  double simTime = 20.0;
  double loss = 0.0;
  uint32_t payload = 500;
  double interval = 0.05;
  double timeout = 0.5;

  CommandLine cmd;
  cmd.AddValue("SimTime", "Simulation time (s)", simTime);
  cmd.AddValue("Loss", "Packet error rate", loss);
  cmd.AddValue("Payload", "Payload size bytes", payload);
  cmd.AddValue("Interval", "Interval between new sends", interval);
  cmd.AddValue("Timeout", "Retransmission timeout (s)", timeout);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices = p2p.Install(nodes);

  // Attach simple error model to the device 1 (receiver side) to simulate loss
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
if (loss > 0.0)
{
  em->SetAttribute("ErrorRate", DoubleValue(loss));
  // use the correct attribute name and enum for packet unit
  em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
}
devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));


  InternetStackHelper internet;
  internet.Install(nodes);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifs = ipv4.Assign(devices);

  // Create sender on node 0, receiver on node 1
  Ptr<ABPSender> sender = CreateObject<ABPSender>();
  Ptr<ABPReceiver> receiver = CreateObject<ABPReceiver>();

  nodes.Get(0)->AddApplication(sender);
  nodes.Get(1)->AddApplication(receiver);

  InetSocketAddress peer = InetSocketAddress(ifs.GetAddress(1), 8080);
  sender->Setup(peer, payload, interval, timeout, "abp_retrans.csv", "abp_throughput.csv");
  receiver->Setup(InetSocketAddress(ifs.GetAddress(1), 8080), "abp_goodput.csv");

  sender->SetStartTime(Seconds(0.5));
  sender->SetStopTime(Seconds(simTime));
  receiver->SetStartTime(Seconds(0.0));
  receiver->SetStopTime(Seconds(simTime));

  Simulator::Stop(Seconds(simTime + 0.1));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
