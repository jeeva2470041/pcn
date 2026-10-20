/*
 * NS-3 Simulation: Comparative Evaluation of Routing Protocols (RIP, OSPF/Link-State, AODV)
 * Scenario: Multi-Department Enterprise Network under Normal, Congested, and Link-Failure Conditions
 * Course: Principles of Computer Networks (PCN) - Experiment 10
 * Author: Arivuchezhiyan E (Reg: 3122247001006)
 *
 * Topology:
 *             [Dept A (Source)]
 *                    | (10.1.1.0/24)
 *                  ( R1 ) ---------- (Primary Link: 10.1.3.0/24) ---------- ( R2 )
 *                    |                                                        |
 *   (Backup Link:    |                                                        | (10.1.5.0/24)
 *    10.1.4.0/24)    |                                                        |
 *                  ( R3 ) ---------- (Backup Path: 10.1.6.0/24) ----------- ( R4 )
 *                                                                             |
 *                                                                       (10.1.2.0/24)
 *                                                                             |
 *                                                                    [Dept B (Destination)]
 *
 * Evaluated Conditions:
 *   1. Normal Condition     : 1 Mbps CBR UDP stream
 *   2. Congested Condition  : Multiple simultaneous 4 Mbps cross-department streams
 *   3. Link-Failure Condition: Primary R1-R2 link disabled at t = 10.0s (Dynamic Re-routing)
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/rip-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnterpriseRoutingEvaluation");

// Dynamic link failure simulator
void TearDownLink(Ptr<Node> nodeA, Ptr<Node> nodeB, uint32_t devIndexA, uint32_t devIndexB)
{
    std::cout << "[!] Time " << Simulator::Now().GetSeconds() << "s: >>> PRIMARY LINK FAILURE EVENT TRIGGERED (R1 <-> R2 DOWN) <<<" << std::endl;
    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4>();
    ipv4A->SetDown(ipv4A->GetInterfaceForDevice(nodeA->GetDevice(devIndexA)));
    ipv4B->SetDown(ipv4B->GetInterfaceForDevice(nodeB->GetDevice(devIndexB)));
}

int main(int argc, char *argv[])
{
    std::string protocol = "OSPF"; // RIP, OSPF, AODV
    std::string condition = "normal"; // normal, congestion, link_failure
    double simTime = 30.0;
    double failTime = 10.0;
    std::string appRate = "1Mbps";
    uint32_t packetSize = 512;

    CommandLine cmd(__FILE__);
    cmd.AddValue("protocol", "Routing protocol (RIP, OSPF, AODV)", protocol);
    cmd.AddValue("condition", "Network condition (normal, congestion, link_failure)", condition);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("failTime", "Link failure trigger time", failTime);
    cmd.Parse(argc, argv);

    if (condition == "congestion") {
        appRate = "4Mbps";
    }

    std::cout << "==========================================================================" << std::endl;
    std::cout << "  NS-3 ROUTING PROTOCOLS EVALUATION (EXPERIMENT 10: ENTERPRISE TOPOLOGY)  " << std::endl;
    std::cout << "  Host: STARK (IPv4: 10.59.74.253) | Reg: 3122247001006                   " << std::endl;
    std::cout << "==========================================================================" << std::endl;
    std::cout << " [Configuration Parameters]" << std::endl;
    std::cout << "  - Evaluated Protocol          : " << protocol << std::endl;
    std::cout << "  - Network Operating Condition : " << condition << std::endl;
    std::cout << "  - Primary Offered Traffic Rate: " << appRate << " (Packet Size: " << packetSize << " Bytes)" << std::endl;
    std::cout << "  - Total Simulation Time       : " << simTime << " seconds" << std::endl;
    if (condition == "link_failure") {
        std::cout << "  - Scheduled Link Teardown     : R1-R2 Link at t = " << failTime << "s" << std::endl;
    }
    std::cout << "--------------------------------------------------------------------------" << std::endl;

    // Create 4 Core Routers (r1, r2, r3, r4) and 2 Departmental Host Nodes (deptA, deptB)
    NodeContainer routers;
    routers.Create(4); // r1=0, r2=1, r3=2, r4=3

    NodeContainer hosts;
    hosts.Create(2);   // deptA=0, deptB=1

    // Setup Point-to-Point Links
    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pAccess.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pCorePrimary;
    p2pCorePrimary.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pCorePrimary.SetChannelAttribute("Delay", StringValue("5ms"));

    PointToPointHelper p2pCoreBackup;
    p2pCoreBackup.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pCoreBackup.SetChannelAttribute("Delay", StringValue("8ms"));

    // Install NetDevices
    NetDeviceContainer devHostA_R1 = p2pAccess.Install(hosts.Get(0), routers.Get(0));
    NetDeviceContainer devHostB_R4 = p2pAccess.Install(hosts.Get(1), routers.Get(3));

    NetDeviceContainer devR1_R2 = p2pCorePrimary.Install(routers.Get(0), routers.Get(1)); // Primary
    NetDeviceContainer devR1_R3 = p2pCoreBackup.Install(routers.Get(0), routers.Get(2));  // Backup branch 1
    NetDeviceContainer devR2_R4 = p2pCorePrimary.Install(routers.Get(1), routers.Get(3)); // Primary exit
    NetDeviceContainer devR3_R4 = p2pCoreBackup.Install(routers.Get(2), routers.Get(3));  // Backup branch 2

    // Protocol Stack Selection
    InternetStackHelper stack;
    if (protocol == "RIP") {
        RipHelper ripHelper;
        Ipv4ListRoutingHelper list;
        list.Add(ripHelper, 100);
        stack.SetRoutingHelper(list);
        stack.Install(routers);
        stack.Install(hosts);
    } else if (protocol == "AODV") {
        AodvHelper aodvHelper;
        Ipv4ListRoutingHelper list;
        list.Add(aodvHelper, 100);
        stack.SetRoutingHelper(list);
        stack.Install(routers);
        stack.Install(hosts);
    } else { // OSPF / Link-State Global Dijkstra
        stack.Install(routers);
        stack.Install(hosts);
    }

    // IP Subnet Assignment
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifHostA_R1 = ipv4.Assign(devHostA_R1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifHostB_R4 = ipv4.Assign(devHostB_R4);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifR1_R2 = ipv4.Assign(devR1_R2);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer ifR1_R3 = ipv4.Assign(devR1_R3);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ifR2_R4 = ipv4.Assign(devR2_R4);

    ipv4.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer ifR3_R4 = ipv4.Assign(devR3_R4);

    if (protocol == "OSPF") {
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    // Dynamic Link Failure Scheduling
    if (condition == "link_failure") {
        Simulator::Schedule(Seconds(failTime), &TearDownLink, routers.Get(0), routers.Get(1), 1, 0);
        if (protocol == "OSPF") {
            // Recompute global SPF tables after failure event
            Simulator::Schedule(Seconds(failTime + 0.1), &Ipv4GlobalRoutingHelper::RecomputeRoutingTables);
        }
    }

    // Install Traffic Applications
    uint16_t sinkPort = 9000;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(hosts.Get(1)); // Dept B
    sinkApp.Start(Seconds(0.5));
    sinkApp.Stop(Seconds(simTime));

    OnOffHelper clientHelper("ns3::UdpSocketFactory",
                             InetSocketAddress(ifHostB_R4.GetAddress(0), sinkPort));
    clientHelper.SetConstantRate(DataRate(appRate), packetSize);
    ApplicationContainer clientApp = clientHelper.Install(hosts.Get(0)); // Dept A
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime - 0.5));

    // Install FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    std::cout << "[*] Executing discrete-event simulation for " << protocol << " (" << condition << ")..." << std::endl;
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n==========================================================================" << std::endl;
    std::cout << "         EMPIRICAL METRICS (" << protocol << " - " << condition << ")    " << std::endl;
    std::cout << "==========================================================================" << std::endl;

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationPort == sinkPort) {
            double duration = i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds();
            if (duration <= 0) duration = simTime - 1.5;
            double tputMbps = (i->second.rxBytes * 8.0) / (duration * 1000000.0);
            double meanDelayMs = (i->second.rxPackets > 0) ? (i->second.delaySum.GetSeconds() * 1000.0 / i->second.rxPackets) : 0.0;
            double pdrPercent = (i->second.txPackets > 0) ? (100.0 * i->second.rxPackets / i->second.txPackets) : 0.0;

            std::cout << "Flow (Dept A -> Dept B):" << std::endl;
            std::cout << "  - Transmitted Packets (Tx)   : " << i->second.txPackets << " packets" << std::endl;
            std::cout << "  - Received Packets (Rx)      : " << i->second.rxPackets << " packets" << std::endl;
            std::cout << "  - Lost / Dropped Packets     : " << i->second.lostPackets << " packets" << std::endl;
            std::cout << "  - Packet Delivery Ratio (PDR): " << std::fixed << std::setprecision(2) << pdrPercent << " %" << std::endl;
            std::cout << "  - Average End-to-End Delay   : " << std::fixed << std::setprecision(2) << meanDelayMs << " ms" << std::endl;
            std::cout << "  - Effective Throughput       : " << std::fixed << std::setprecision(3) << tputMbps << " Mbps" << std::endl;
        }
    }
    std::cout << "==========================================================================" << std::endl;

    Simulator::Destroy();
    return 0;
}
