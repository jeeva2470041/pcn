/*
 * NS-3 Simulation: Performance Analysis of TCP Congestion Control (Reno, Cubic, BBR)
 * Scenario: Intelligent Transportation System (VANET Telemetry & Camera Data)
 * Course: Principles of Computer Networks (PCN) - Experiment 9
 * Author: Arivuchezhiyan E (Reg: 3122247001006)
 *
 * Topology:
 *   Vehicle 1 (10.1.1.1) \
 *   Vehicle 2 (10.1.2.1)  \
 *   Vehicle 3 (10.1.3.1) --- Roadside Router (RSU) ===(Internet Bottleneck)=== Traffic Server (10.1.6.2)
 *   Vehicle 4 (10.1.4.1)  /
 *   Vehicle 5 (10.1.5.1) /
 *
 * Link Parameters:
 *   - Vehicle-to-RSU Access Links: 10 Mbps, 2 ms delay
 *   - RSU-to-Server Bottleneck: 5 Mbps, 25 ms delay (Queue: DropTail 50 packets)
 *
 * Evaluated Congestion Control Variants:
 *   - TCP Reno (AIMD)
 *   - TCP Cubic (Polynomial Growth, RFC 8312)
 *   - TCP BBR (Bottleneck Bandwidth and RTT Model, Cardwell et al.)
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
#include <vector>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpVanetCongestionSim");

int main(int argc, char *argv[])
{
    std::string tcpVariant = "TcpCubic"; // TcpNewReno, TcpCubic, TcpBbr
    std::string accessBw = "10Mbps";
    std::string accessDelay = "2ms";
    std::string bottleneckBw = "5Mbps";
    std::string bottleneckDelay = "25ms";
    uint32_t queueSize = 50;
    double simTime = 25.0;
    uint32_t numVehicles = 5;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpVariant", "TCP Congestion Control Variant (TcpNewReno, TcpCubic, TcpBbr)", tcpVariant);
    cmd.AddValue("bottleneckBw", "Internet bottleneck bandwidth", bottleneckBw);
    cmd.AddValue("bottleneckDelay", "Internet bottleneck delay", bottleneckDelay);
    cmd.AddValue("queueSize", "DropTail queue size in packets", queueSize);
    cmd.AddValue("simTime", "Total simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    // Set TCP Congestion Control Algorithm TypeId
    std::string typeIdStr = "ns3::" + tcpVariant;
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(typeIdStr)));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(1));
    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize",
                       QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));

    std::cout << "==========================================================================" << std::endl;
    std::cout << "  NS-3 TCP CONGESTION CONTROL SIMULATION (EXPERIMENT 9: VANET ITS SCENARIO)" << std::endl;
    std::cout << "  Host: STARK (IPv4: 10.59.74.253) | Reg: 3122247001006                   " << std::endl;
    std::cout << "==========================================================================" << std::endl;
    std::cout << " [Configuration Parameters]" << std::endl;
    std::cout << "  - Active TCP Variant          : " << tcpVariant << std::endl;
    std::cout << "  - Number of Vehicles (Nodes)  : " << numVehicles << " (V1 - V5)" << std::endl;
    std::cout << "  - Vehicle-to-RSU Access Links : " << accessBw << " / " << accessDelay << std::endl;
    std::cout << "  - RSU-to-Server Bottleneck    : " << bottleneckBw << " / " << bottleneckDelay << std::endl;
    std::cout << "  - Bottleneck Queue Capacity   : " << queueSize << " packets (DropTail)" << std::endl;
    std::cout << "  - Simulation Duration         : " << simTime << " seconds" << std::endl;
    std::cout << "--------------------------------------------------------------------------" << std::endl;

    // Create Nodes
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    NodeContainer rsuRouter;
    rsuRouter.Create(1);

    NodeContainer trafficServer;
    trafficServer.Create(1);

    // Create Links
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue(accessBw));
    accessLink.SetChannelAttribute("Delay", StringValue(accessDelay));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckBw));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    bottleneckLink.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize",
                            QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));

    // Install Devices
    std::vector<NetDeviceContainer> vehicleDevs(numVehicles);
    for (uint32_t i = 0; i < numVehicles; i++)
    {
        vehicleDevs[i] = accessLink.Install(vehicles.Get(i), rsuRouter.Get(0));
    }
    NetDeviceContainer serverDev = bottleneckLink.Install(rsuRouter.Get(0), trafficServer.Get(0));

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(vehicles);
    stack.Install(rsuRouter);
    stack.Install(trafficServer);

    // Assign IP Addresses
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> vehicleIfs(numVehicles);
    for (uint32_t i = 0; i < numVehicles; i++)
    {
        std::string subnet = "10.1." + std::to_string(i + 1) + ".0";
        ipv4.SetBase(subnet.c_str(), "255.255.255.0");
        vehicleIfs[i] = ipv4.Assign(vehicleDevs[i]);
    }
    ipv4.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer serverIf = ipv4.Assign(serverDev);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Application Installation: Traffic Server Sinks & Vehicle BulkSend Sources
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < numVehicles; i++)
    {
        uint16_t port = 8001 + i;
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        serverApps.Add(sinkHelper.Install(trafficServer.Get(0)));

        Address serverAddress(InetSocketAddress(serverIf.GetAddress(1), port));
        BulkSendHelper sourceHelper("ns3::TcpSocketFactory", serverAddress);
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));
        sourceHelper.SetAttribute("SendSize", UintegerValue(1024));
        // Stagger vehicle transmission start times slightly to simulate real-world arrival
        ApplicationContainer app = sourceHelper.Install(vehicles.Get(i));
        app.Start(Seconds(1.0 + (i * 0.2)));
        app.Stop(Seconds(simTime - 0.5));
        clientApps.Add(app);
    }

    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(simTime));

    // Flow Monitoring
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    std::cout << "[*] Running discrete-event VANET ITS simulation for " << tcpVariant << "..." << std::endl;
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n==========================================================================" << std::endl;
    std::cout << "                 PER-FLOW PERFORMANCE METRICS (" << tcpVariant << ")      " << std::endl;
    std::cout << "==========================================================================" << std::endl;

    std::vector<double> throughputs;
    double totalThroughput = 0.0;
    double sumDelay = 0.0;
    uint32_t totalLost = 0;
    uint32_t totalTx = 0;
    uint32_t validFlowCount = 0;

    for (auto i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationPort >= 8001 && t.destinationPort <= 8005)
        {
            double duration = (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds());
            if (duration <= 0) duration = simTime - 2.0;
            double tputMbps = (i->second.rxBytes * 8.0) / (duration * 1000000.0);
            double meanDelayMs = (i->second.rxPackets > 0) ? (i->second.delaySum.GetSeconds() * 1000.0 / i->second.rxPackets) : 0.0;
            double lossPercent = (i->second.txPackets > 0) ? (100.0 * i->second.lostPackets / i->second.txPackets) : 0.0;

            throughputs.push_back(tputMbps);
            totalThroughput += tputMbps;
            sumDelay += meanDelayMs;
            totalLost += i->second.lostPackets;
            totalTx += i->second.txPackets;
            validFlowCount++;

            uint32_t vId = t.destinationPort - 8000;
            std::cout << "Vehicle " << vId << " (" << t.sourceAddress << " -> " << t.destinationAddress << ":" << t.destinationPort << "):" << std::endl;
            std::cout << "  Throughput: " << std::fixed << std::setprecision(3) << tputMbps << " Mbps"
                      << " | Delay: " << std::fixed << std::setprecision(2) << meanDelayMs << " ms"
                      << " | Lost: " << i->second.lostPackets << " (" << std::fixed << std::setprecision(2) << lossPercent << "%)"
                      << " | Rx: " << i->second.rxPackets << " pkts" << std::endl;
        }
    }

    // Calculate Jain's Fairness Index: J = (sum(x_i))^2 / (n * sum(x_i^2))
    double sumX = 0.0;
    double sumXSquare = 0.0;
    for (double tp : throughputs)
    {
        sumX += tp;
        sumXSquare += (tp * tp);
    }
    double jainsIndex = (sumXSquare > 0 && throughputs.size() > 0) ? (sumX * sumX) / (throughputs.size() * sumXSquare) : 0.0;
    double avgDelay = (validFlowCount > 0) ? (sumDelay / validFlowCount) : 0.0;
    double overallLossPct = (totalTx > 0) ? (100.0 * totalLost / totalTx) : 0.0;

    std::cout << "--------------------------------------------------------------------------" << std::endl;
    std::cout << " [OVERALL AGGREGATE RESULTS - " << tcpVariant << "]" << std::endl;
    std::cout << "  - Aggregate Bottleneck Throughput : " << std::fixed << std::setprecision(3) << totalThroughput << " Mbps (" << (totalThroughput / 5.0 * 100.0) << "% of 5Mbps capacity)" << std::endl;
    std::cout << "  - Average Vehicle Delay (Latency) : " << std::fixed << std::setprecision(2) << avgDelay << " ms" << std::endl;
    std::cout << "  - Overall Packet Loss Rate        : " << totalLost << " packets (" << std::fixed << std::setprecision(2) << overallLossPct << "%)" << std::endl;
    std::cout << "  - Jain's Fairness Index (J)       : " << std::fixed << std::setprecision(4) << jainsIndex << " (Scale: 0.0 - 1.0)" << std::endl;
    std::cout << "==========================================================================" << std::endl;

    Simulator::Destroy();
    return 0;
}
