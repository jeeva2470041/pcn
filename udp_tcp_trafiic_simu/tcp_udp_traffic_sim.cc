/*
 * NS-3 Simulation: Performance Analysis of TCP and UDP Traffic
 * Course: Principles of Computer Networks (PCN) - Experiment 8
 * Author: Arivuchezhiyan E (Reg: 3122247001006)
 * Topology: 4 End Nodes (n0, n1, n2, n3) via 2 Routers (r1, r2)
 *
 *   n0 (TCP Client)  \                               / n3 (TCP Server)
 *                     r1 =======(Bottleneck)======= r2
 *   n1 (UDP Client)  /                               \ n2 (UDP Server)
 *
 * Link Parameters:
 *   - Access Links (n0-r1, n1-r1, r2-n3, r2-n2): 10 Mbps, 2 ms delay
 *   - Bottleneck Link (r1-r2): 2 Mbps (configurable), 10 ms delay (configurable)
 *   - Bottleneck Queue: DropTail (MaxPackets = 50, configurable)
 *
 * Traffic Flows:
 *   - Flow 1 (TCP): n0 -> n3 (Packet Size: 1024 Bytes, BulkSend)
 *   - Flow 2 (UDP): n1 -> n2 (Packet Size: 512 Bytes, DataRate: 1 Mbps, OnOff)
 *   - Simulation Duration: 20 seconds
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpUdpTrafficSim");

// Global counters for tracing
uint32_t g_tcpRetransmissions = 0;
uint32_t g_queueDrops = 0;

void
CwndTracer(uint32_t oldval, uint32_t newval)
{
    NS_LOG_DEBUG("At time " << Simulator::Now().GetSeconds() << "s cwnd changed: " << oldval << " -> " << newval);
}

void
RxDropTracer(Ptr<const Packet> p)
{
    g_queueDrops++;
}

void
RtoTracer(Time oldval, Time newval)
{
    g_tcpRetransmissions++;
}

int main(int argc, char *argv[])
{
    // Default Simulation Configuration Parameters
    std::string bottleneckBw = "2Mbps";
    std::string bottleneckDelay = "10ms";
    std::string accessBw = "10Mbps";
    std::string accessDelay = "2ms";
    std::string udpRate = "1Mbps";
    uint32_t queueSize = 50;
    double simTime = 20.0;
    bool enablePcap = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("bottleneckBw", "Bottleneck link bandwidth", bottleneckBw);
    cmd.AddValue("bottleneckDelay", "Bottleneck link propagation delay", bottleneckDelay);
    cmd.AddValue("accessBw", "Access link bandwidth", accessBw);
    cmd.AddValue("accessDelay", "Access link propagation delay", accessDelay);
    cmd.AddValue("udpRate", "UDP application data rate", udpRate);
    cmd.AddValue("queueSize", "Bottleneck DropTail queue capacity in packets", queueSize);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    // Global Configuration
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(1));
    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize",
                       QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));

    std::cout << "================================================================" << std::endl;
    std::cout << "  NS-3 TCP & UDP TRAFFIC SIMULATION (EXPERIMENT 8)             " << std::endl;
    std::cout << "  Host: STARK (IPv4: 10.59.74.253) | Reg: 3122247001006         " << std::endl;
    std::cout << "================================================================" << std::endl;
    std::cout << " [Configuration Parameters]" << std::endl;
    std::cout << "  - Access Links Bandwidth/Delay    : " << accessBw << " / " << accessDelay << std::endl;
    std::cout << "  - Bottleneck Bandwidth/Delay      : " << bottleneckBw << " / " << bottleneckDelay << std::endl;
    std::cout << "  - Bottleneck Queue Size (Packets) : " << queueSize << " packets (DropTail)" << std::endl;
    std::cout << "  - UDP Offered Data Rate           : " << udpRate << " (Packet: 512 B)" << std::endl;
    std::cout << "  - TCP Segment Size                : 1024 Bytes" << std::endl;
    std::cout << "  - Total Simulation Time           : " << simTime << " seconds" << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;

    // Create 4 End Nodes and 2 Intermediate Routers
    NodeContainer endNodes;
    endNodes.Create(4); // n0, n1, n2, n3

    NodeContainer routers;
    routers.Create(2);  // r1, r2

    // Helper PointToPoint setups
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue(accessBw));
    accessLink.SetChannelAttribute("Delay", StringValue(accessDelay));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckBw));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    bottleneckLink.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize",
                            QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));

    // Connect Nodes: n0-r1, n1-r1, r1-r2 (bottleneck), r2-n3, r2-n2
    NetDeviceContainer devN0R1 = accessLink.Install(endNodes.Get(0), routers.Get(0));
    NetDeviceContainer devN1R1 = accessLink.Install(endNodes.Get(1), routers.Get(0));
    NetDeviceContainer devR1R2 = bottleneckLink.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer devR2N3 = accessLink.Install(routers.Get(1), endNodes.Get(3));
    NetDeviceContainer devR2N2 = accessLink.Install(routers.Get(1), endNodes.Get(2));

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(endNodes);
    stack.Install(routers);

    // Assign IP Addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0"); // n0 - r1
    Ipv4InterfaceContainer ifN0R1 = ipv4.Assign(devN0R1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0"); // n1 - r1
    Ipv4InterfaceContainer ifN1R1 = ipv4.Assign(devN1R1);

    ipv4.SetBase("10.1.3.0", "255.255.255.0"); // r1 - r2 (bottleneck)
    Ipv4InterfaceContainer ifR1R2 = ipv4.Assign(devR1R2);

    ipv4.SetBase("10.1.4.0", "255.255.255.0"); // r2 - n3
    Ipv4InterfaceContainer ifR2N3 = ipv4.Assign(devR2N3);

    ipv4.SetBase("10.1.5.0", "255.255.255.0"); // r2 - n2
    Ipv4InterfaceContainer ifR2N2 = ipv4.Assign(devR2N2);

    // Populate Global Routing Tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // -------------------------------------------------------------
    // Application Setup
    // Flow 1: TCP Flow from Node 0 (n0) to Node 3 (n3)
    // -------------------------------------------------------------
    uint16_t tcpSinkPort = 8080;
    Address sinkAddressTcp(InetSocketAddress(ifR2N3.GetAddress(1), tcpSinkPort));
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), tcpSinkPort));
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(endNodes.Get(3));
    tcpSinkApp.Start(Seconds(0.5));
    tcpSinkApp.Stop(Seconds(simTime));

    BulkSendHelper tcpSourceHelper("ns3::TcpSocketFactory", sinkAddressTcp);
    tcpSourceHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    tcpSourceHelper.SetAttribute("SendSize", UintegerValue(1024));
    ApplicationContainer tcpSourceApp = tcpSourceHelper.Install(endNodes.Get(0));
    tcpSourceApp.Start(Seconds(1.0));
    tcpSourceApp.Stop(Seconds(simTime - 0.5));

    // -------------------------------------------------------------
    // Flow 2: UDP Flow from Node 1 (n1) to Node 2 (n2)
    // -------------------------------------------------------------
    uint16_t udpSinkPort = 9090;
    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), udpSinkPort));
    ApplicationContainer udpSinkApp = udpSinkHelper.Install(endNodes.Get(2));
    udpSinkApp.Start(Seconds(0.5));
    udpSinkApp.Stop(Seconds(simTime));

    OnOffHelper udpSourceHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(ifR2N2.GetAddress(1), udpSinkPort));
    udpSourceHelper.SetConstantRate(DataRate(udpRate), 512);
    ApplicationContainer udpSourceApp = udpSourceHelper.Install(endNodes.Get(1));
    udpSourceApp.Start(Seconds(1.0));
    udpSourceApp.Stop(Seconds(simTime - 0.5));

    if (enablePcap)
    {
        bottleneckLink.EnablePcapAll("tcp-udp-sim");
    }

    // Install FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    std::cout << "[*] Starting NS-3 discrete-event simulation..." << std::endl;
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Gather Flow Statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n================================================================" << std::endl;
    std::cout << "                 NS-3 FLOW MONITOR RESULTS                      " << std::endl;
    std::cout << "================================================================" << std::endl;

    double tcpThroughput = 0.0, udpThroughput = 0.0;
    double tcpDelay = 0.0, udpDelay = 0.0;
    uint32_t tcpLost = 0, udpLost = 0;
    uint32_t tcpTx = 0, tcpRx = 0, udpTx = 0, udpRx = 0;

    for (auto i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        double flowDuration = (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds());
        if (flowDuration <= 0) flowDuration = simTime - 1.5;
        double throughputKbps = (i->second.rxBytes * 8.0) / (flowDuration * 1000.0);
        double meanDelayMs = (i->second.rxPackets > 0) ? (i->second.delaySum.GetSeconds() * 1000.0 / i->second.rxPackets) : 0.0;
        double lossPercent = (i->second.txPackets > 0) ? (100.0 * i->second.lostPackets / i->second.txPackets) : 0.0;

        std::cout << "Flow ID: " << i->first 
                  << " (" << t.sourceAddress << ":" << t.sourcePort 
                  << " -> " << t.destinationAddress << ":" << t.destinationPort << ")" << std::endl;
        std::cout << "  Protocol              : " << ((t.protocol == 6) ? "TCP" : "UDP") << std::endl;
        std::cout << "  Tx Packets / Rx Packets: " << i->second.txPackets << " / " << i->second.rxPackets << std::endl;
        std::cout << "  Lost Packets (Drop %) : " << i->second.lostPackets << " (" << std::fixed << std::setprecision(2) << lossPercent << " %)" << std::endl;
        std::cout << "  Total Rx Bytes        : " << i->second.rxBytes << " Bytes" << std::endl;
        std::cout << "  Average Throughput    : " << std::fixed << std::setprecision(3) << throughputKbps / 1000.0 << " Mbps (" << throughputKbps << " Kbps)" << std::endl;
        std::cout << "  Mean End-to-End Delay : " << std::fixed << std::setprecision(3) << meanDelayMs << " ms" << std::endl;
        std::cout << "----------------------------------------------------------------" << std::endl;

        if (t.protocol == 6 && t.destinationPort == tcpSinkPort) {
            tcpThroughput = throughputKbps / 1000.0;
            tcpDelay = meanDelayMs;
            tcpLost = i->second.lostPackets;
            tcpTx = i->second.txPackets;
            tcpRx = i->second.rxPackets;
        } else if (t.protocol == 17 && t.destinationPort == udpSinkPort) {
            udpThroughput = throughputKbps / 1000.0;
            udpDelay = meanDelayMs;
            udpLost = i->second.lostPackets;
            udpTx = i->second.txPackets;
            udpRx = i->second.rxPackets;
        }
    }

    std::cout << "\n================================================================" << std::endl;
    std::cout << "                 EXPERIMENT SUMMARY METRICS                     " << std::endl;
    std::cout << "================================================================" << std::endl;
    std::cout << std::left << std::setw(15) << "Metric" 
              << std::setw(25) << "TCP Flow (n0 -> n3)" 
              << std::setw(25) << "UDP Flow (n1 -> n2)" << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;
    std::cout << std::left << std::setw(15) << "Offered Rate" 
              << std::setw(25) << "Saturating (Greedy)" 
              << std::setw(25) << udpRate << std::endl;
    std::cout << std::left << std::setw(15) << "Packet Size" 
              << std::setw(25) << "1024 Bytes" 
              << std::setw(25) << "512 Bytes" << std::endl;
    std::cout << std::left << std::setw(15) << "Throughput" 
              << std::setw(25) << (std::to_string(tcpThroughput).substr(0, 5) + " Mbps") 
              << std::setw(25) << (std::to_string(udpThroughput).substr(0, 5) + " Mbps") << std::endl;
    std::cout << std::left << std::setw(15) << "End-to-End Delay" 
              << std::setw(25) << (std::to_string(tcpDelay).substr(0, 6) + " ms") 
              << std::setw(25) << (std::to_string(udpDelay).substr(0, 6) + " ms") << std::endl;
    std::cout << std::left << std::setw(15) << "Packet Loss" 
              << std::setw(25) << (std::to_string(tcpLost) + " packets (" + std::to_string(100.0 * tcpLost / (tcpTx ? tcpTx : 1)).substr(0, 4) + "%)")
              << std::setw(25) << (std::to_string(udpLost) + " packets (" + std::to_string(100.0 * udpLost / (udpTx ? udpTx : 1)).substr(0, 4) + "%)") << std::endl;
    std::cout << "================================================================" << std::endl;

    Simulator::Destroy();
    return 0;
}
