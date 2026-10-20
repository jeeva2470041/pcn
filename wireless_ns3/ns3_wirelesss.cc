#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/sixlowpan-module.h"
#include <iomanip>
#include <iostream>
#include <cmath>

using namespace ns3;

struct SimResult {
    std::string tech;
    double speed;
    double rxPower;
    double throughput;
    double loss;
    double delay;
};

// Runs one experiment for a given wireless technology and mobility speed
SimResult RunSimulation(std::string tech, double speed) {
    NodeContainer mobile, gateway;
    mobile.Create(3);
    gateway.Create(1);

    // 1. Mobility: Fixed Gateway at (50, 50), Mobile nodes with RandomWalk2d
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
    pos->Add(Vector(50.0, 50.0, 0.0));
    mobility.SetPositionAllocator(pos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gateway);

    MobilityHelper mobMobile;
    mobMobile.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=20|Max=80]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=20|Max=80]"));
    mobMobile.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Mode", StringValue("Time"), "Time", TimeValue(Seconds(2.0)),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(speed) + "]"),
        "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobMobile.Install(mobile);

    double txPower = (tech == "wifi") ? 16.0 : ((tech == "bluetooth") ? 4.0 : 0.0);
    uint16_t port = (tech == "wifi") ? 5000 : ((tech == "bluetooth") ? 7000 : 6000);
    uint32_t pktSize = (tech == "wifi") ? 512 : ((tech == "bluetooth") ? 200 : 100);
    std::string rate = (tech == "wifi") ? "1Mbps" : ((tech == "bluetooth") ? "100Kbps" : "250Kbps");

    ApplicationContainer sinkApp, srcApp;

    // 2. Wireless Channel & Stack Setup
    if (tech == "wifi" || tech == "bluetooth") {
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());
        phy.Set("TxPowerStart", DoubleValue(txPower));
        phy.Set("TxPowerEnd", DoubleValue(txPower));

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211g);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
            "DataMode", StringValue("ErpOfdmRate6Mbps"),
            "ControlMode", StringValue("ErpOfdmRate6Mbps"));

        WifiMacHelper mac;
        Ssid ssid = Ssid("Campus-" + tech);
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDev = wifi.Install(phy, mac, mobile);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDev = wifi.Install(phy, mac, gateway);

        InternetStackHelper internet;
        internet.Install(mobile);
        internet.Install(gateway);

        Ipv4AddressHelper address;
        address.SetBase("10.1.0.0", "255.255.255.0");
        NetDeviceContainer allDev;
        allDev.Add(staDev);
        allDev.Add(apDev);
        Ipv4InterfaceContainer ifaces = address.Assign(allDev);

        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        sinkApp = sink.Install(gateway.Get(0));

        OnOffHelper src("ns3::UdpSocketFactory", InetSocketAddress(ifaces.GetAddress(3), port));
        src.SetAttribute("DataRate", DataRateValue(DataRate(rate)));
        src.SetAttribute("PacketSize", UintegerValue(pktSize));
        srcApp = src.Install(mobile);
    } else { // Zigbee (IEEE 802.15.4 LR-WPAN + 6LoWPAN)
        NodeContainer allNodes;
        allNodes.Add(mobile);
        allNodes.Add(gateway);

        LrWpanHelper lrWpan;
        NetDeviceContainer lrDev = lrWpan.Install(allNodes);
        for (uint32_t i = 0; i < lrDev.GetN(); i++) {
            Ptr<lrwpan::LrWpanNetDevice> d = DynamicCast<lrwpan::LrWpanNetDevice>(lrDev.Get(i));
            if (d) {
                d->GetMac()->SetPanId(0);
                d->GetMac()->SetShortAddress(Mac16Address(static_cast<uint16_t>(i + 1)));
            }
        }

        SixLowPanHelper sixlowpan;
        NetDeviceContainer sixDev = sixlowpan.Install(lrDev);

        InternetStackHelper internet;
        internet.Install(allNodes);

        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer ifaces6 = ipv6.Assign(sixDev);

        for (uint32_t i = 0; i < allNodes.GetN(); i++) {
            Ptr<Ipv6> p = allNodes.Get(i)->GetObject<Ipv6>();
            if (p) for (uint32_t j = 0; j < p->GetNInterfaces(); j++) p->SetUp(j);
        }

        PacketSinkHelper sink("ns3::UdpSocketFactory", Inet6SocketAddress(Ipv6Address::GetAny(), port));
        sinkApp = sink.Install(gateway.Get(0));

        OnOffHelper src("ns3::UdpSocketFactory", Inet6SocketAddress(ifaces6.GetAddress(3, 1), port));
        src.SetAttribute("DataRate", DataRateValue(DataRate(rate)));
        src.SetAttribute("PacketSize", UintegerValue(pktSize));
        srcApp = src.Install(mobile);
    }

    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(20.0));
    srcApp.Start(Seconds(2.0));
    srcApp.Stop(Seconds(20.0));

    // 3. FlowMonitor for Throughput, Packet Loss, Delay
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    auto stats = monitor->GetFlowStats();

    uint64_t totalTx = 0, totalRx = 0;
    double totalDelay = 0;
    for (auto const &f : stats) {
        totalTx += f.second.txPackets;
        totalRx += f.second.rxPackets;
        totalDelay += f.second.delaySum.GetSeconds();
    }

    // 4. Signal Strength (Friis Free-Space Path Loss)
    double distSum = 0;
    for (uint32_t i = 0; i < mobile.GetN(); i++) {
        Vector p1 = mobile.Get(i)->GetObject<MobilityModel>()->GetPosition();
        Vector p2 = gateway.Get(0)->GetObject<MobilityModel>()->GetPosition();
        double d = std::max(0.1, sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2)));
        distSum += d;
    }
    double avgDist = distSum / mobile.GetN();
    double pathLoss = 20.0 * log10(avgDist) + 20.0 * log10(2.4e9) - 147.55;
    double rxPower = txPower - pathLoss;

    double throughput = (totalRx > 0) ? (totalRx * pktSize * 8.0 / 18.0 / 1000.0) : 0.0; // Kbps
    double loss = (totalTx > 0) ? ((double)(totalTx - totalRx) / totalTx * 100.0) : 0.0;
    double avgDelay = (totalRx > 0) ? (totalDelay / totalRx * 1000.0) : 0.0; // ms

    Simulator::Destroy();
    return SimResult{tech, speed, rxPower, throughput, loss, avgDelay};
}

int main(int argc, char *argv[]) {
    std::string targetTech = "all";
    double targetSpeed = 0.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tech", "Wireless technology: wifi, zigbee, bluetooth, all", targetTech);
    cmd.AddValue("speed", "Mobility speed in m/s: 1, 5, 10 (or 0 for all speeds)", targetSpeed);
    cmd.Parse(argc, argv);

    std::vector<std::string> techs = (targetTech == "all") 
        ? std::vector<std::string>{"wifi", "zigbee", "bluetooth"} 
        : std::vector<std::string>{targetTech};
    std::vector<double> speeds = (targetSpeed > 0) 
        ? std::vector<double>{targetSpeed} 
        : std::vector<double>{1.0, 5.0, 10.0};

    std::cout << "\n=========================================================================\n";
    std::cout << "        HETEROGENEOUS WIRELESS CAMPUS NETWORK PERFORMANCE (NS-3)         \n";
    std::cout << "=========================================================================\n";
    std::cout << std::left << std::setw(12) << "Technology"
              << std::setw(12) << "Speed(m/s)"
              << std::setw(16) << "Rx Power(dBm)"
              << std::setw(18) << "Throughput(Kbps)"
              << std::setw(14) << "Loss(%)"
              << std::setw(14) << "Delay(ms)" << "\n";
    std::cout << "-------------------------------------------------------------------------\n";

    for (const auto &t : techs) {
        for (double s : speeds) {
            SimResult r = RunSimulation(t, s);
            std::cout << std::fixed << std::setprecision(2);
            std::cout << std::left << std::setw(12) << r.tech
                      << std::setw(12) << r.speed
                      << std::setw(16) << r.rxPower
                      << std::setw(18) << r.throughput
                      << std::setw(14) << r.loss
                      << std::setw(14) << r.delay << "\n";
        }
    }
    std::cout << "=========================================================================\n\n";
    return 0;
}
