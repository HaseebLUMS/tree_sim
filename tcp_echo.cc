
/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <ns3/applications-module.h>
#include <ns3/core-module.h>
#include <ns3/internet-module.h>
#include <ns3/network-module.h>
#include <ns3/point-to-point-module.h>
#include <vector>
#include <fstream>

// #include "timestamp_tag.h"

// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1
//    point-to-point
//

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpExample");

class CustomTcpClient : public Application
{
public:
    CustomTcpClient() : m_socket(0), m_connected(false) {}
    virtual ~CustomTcpClient() { m_socket = nullptr; }

    void Setup(Address serverAddress, uint32_t packetSize, uint32_t rate, uint32_t duration)
    {
        m_serverAddress = serverAddress;
        m_packetSize = packetSize;
        m_interval = Seconds(1.0 / rate); // Calculate inter-packet interval
        m_numPackets = rate * duration;  // Calculate total packets to send
    }

protected:
    virtual void StartApplication() override
    {
        m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
        m_socket->Connect(m_serverAddress);
        m_socket->SetConnectCallback(
            MakeCallback(&CustomTcpClient::ConnectionSucceeded, this),
            MakeCallback(&CustomTcpClient::ConnectionFailed, this));
    }

    virtual void StopApplication() override
    {
        if (m_socket)
        {
            m_socket->Close();
            m_socket = nullptr;
        }
    }

private:
    void ConnectionSucceeded(Ptr<Socket> socket)
    {
        NS_LOG_INFO("Connection Succeeded");
        m_connected = true;
        SendPacket();
    }

    void ConnectionFailed(Ptr<Socket> socket)
    {
        NS_LOG_INFO("Connection Failed");
    }

    void SendPacket()
    {
        if (!m_socket)
        {
            NS_LOG_WARN("Socket is null. Cannot send packet.");
            return;
        }

        if (m_packetsSent < m_numPackets)
        {
            // Serialize the timestamp into the payload
            Time sendTime = Simulator::Now();
            uint64_t timestampNs = sendTime.GetNanoSeconds();
            Ptr<Packet> packet = Create<Packet>((uint8_t *)&timestampNs, sizeof(timestampNs)); // Include timestamp as payload

            NS_LOG_INFO("Sending packet " << m_packetsSent + 1 << " at " << sendTime);
            m_socket->Send(packet);

            ++m_packetsSent;
            Simulator::Schedule(m_interval, &CustomTcpClient::SendPacket, this);
        }
    }

    Ptr<Socket> m_socket;
    Address m_serverAddress;
    uint32_t m_packetSize;
    uint32_t m_numPackets;
    Time m_interval;
    uint32_t m_packetsSent = 0;
    bool m_connected = false;
};


// Global container to store packet latencies
std::vector<double> latencies;

void PacketReceived(Ptr<const Packet> packet, const Address &from)
{
    if (packet->GetSize() >= sizeof(uint64_t)) // Ensure payload is large enough to contain timestamp
    {
        uint64_t timestampNs;
        packet->CopyData((uint8_t *)&timestampNs, sizeof(timestampNs)); // Extract timestamp
        Time sendTime = NanoSeconds(timestampNs);
        Time receiveTime = Simulator::Now();
        double latency = (receiveTime - sendTime).GetSeconds();
        latencies.push_back(latency);

        NS_LOG_INFO("Packet received from: " << InetSocketAddress::ConvertFrom(from).GetIpv4()
                     << " | Size: " << packet->GetSize() << " bytes"
                     << " | Sent at: " << sendTime
                     << " | Received at: " << receiveTime
                     << " | Latency: " << latency << " seconds");
    }
    else
    {
        NS_LOG_WARN("Packet received with insufficient size. Ignoring...");
    }
}

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);
    std::cout << "Running..." << std::endl;

    Time::SetResolution(Time::NS);
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("TcpExample", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create a point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("30us"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install TCP server on node 1
    uint16_t port = 50000; // TCP port
    Address serverAddress = InetSocketAddress(interfaces.GetAddress(1), port);
    PacketSinkHelper tcpServerHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApps = tcpServerHelper.Install(nodes.Get(1));
    serverApps.Start(Seconds(1));
    serverApps.Stop(Seconds(12));

    // Attach callback to record packet latencies
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApps.Get(0));
    sink->TraceConnectWithoutContext("Rx", MakeCallback(&PacketReceived));

    // Install custom TCP client on node 0
    Ptr<CustomTcpClient> clientApp = CreateObject<CustomTcpClient>();
    clientApp->Setup(serverAddress, 100, 10, 10);
    // clientApp->Setup(serverAddress, 100, 1000000, 10);
    nodes.Get(0)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(2));
    clientApp->SetStopTime(Seconds(20));

    // Run simulation
    Simulator::Run();

    // Save latencies to file
    std::ofstream latencyFile("./scratch/assets/latencies1.txt");
    for (double latency : latencies)
    {
        latencyFile << latency << std::endl;
    }
    latencyFile.close();

    Simulator::Destroy();

    return 0;
}
