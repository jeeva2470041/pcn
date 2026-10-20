#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <iomanip>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpAssignment");

int main(int argc, char *argv[]) {
  // ------------------------------------------------------------
  // Simulation parameters
  // ------------------------------------------------------------
  double latency = 2.0;       // Link latency in milliseconds
  double rate1 = 5.0;         // Flow 1 data rate in Mbps
  double rate2 = 5.0;         // Flow 2 data rate in Mbps
  uint32_t packetSize = 1024; // UDP packet size in bytes
  double simulationTime = 10.0;
  bool enableFlow2 = true;    // Set to false for single-flow testing

  CommandLine cmd(__FILE__);
  cmd.AddValue("latency", "Point-to-point link latency in milliseconds", latency);
  cmd.AddValue("rate1", "Data rate of UDP Flow 1 (Mbps)", rate1);
  cmd.AddValue("rate2", "Data rate of UDP Flow 2 (Mbps)", rate2);
  cmd.AddValue("enableFlow2", "Enable second UDP flow on Port 6000", enableFlow2);
  cmd.Parse(argc, argv);

  // ------------------------------------------------------------
  // 1. Create two nodes
  // ------------------------------------------------------------
  NodeContainer nodes;
  nodes.Create(2);

  // ------------------------------------------------------------
  // 2. Create Point-to-Point link
  // ------------------------------------------------------------
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
  pointToPoint.SetChannelAttribute("Delay", TimeValue(MilliSeconds(latency)));
  NetDeviceContainer devices = pointToPoint.Install(nodes);

  // ------------------------------------------------------------
  // 3. Install Internet stack
  // ------------------------------------------------------------
  InternetStackHelper internet;
  internet.Install(nodes);

  // ------------------------------------------------------------
  // 4. Assign IP addresses
  // ------------------------------------------------------------
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Node 0 = 10.1.1.1
  // Node 1 = 10.1.1.2

  // ------------------------------------------------------------
  // 5. UDP Server 1 - Port 5000
  // ------------------------------------------------------------
  uint16_t port1 = 5000;
  UdpServerHelper server1(port1);
  ApplicationContainer serverApp1 = server1.Install(nodes.Get(1));
  serverApp1.Start(Seconds(1.0));
  serverApp1.Stop(Seconds(simulationTime));

  // ------------------------------------------------------------
  // 6. UDP Client 1 - Port 5000
  // ------------------------------------------------------------
  UdpClientHelper client1(interfaces.GetAddress(1), port1);
  client1.SetAttribute("MaxPackets", UintegerValue(1000000));
  client1.SetAttribute("Interval", TimeValue(Seconds((packetSize * 8.0) / (rate1 * 1000000.0))));
  client1.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApp1 = client1.Install(nodes.Get(0));
  clientApp1.Start(Seconds(1.0));
  clientApp1.Stop(Seconds(simulationTime));

  // ------------------------------------------------------------
  // 7. UDP Server & Client 2 - Port 6000 (Optional second flow)
  // ------------------------------------------------------------
  ApplicationContainer serverApp2;
  ApplicationContainer clientApp2;
  if (enableFlow2) {
    uint16_t port2 = 6000;
    UdpServerHelper server2(port2);
    serverApp2 = server2.Install(nodes.Get(1));
    serverApp2.Start(Seconds(1.0));
    serverApp2.Stop(Seconds(simulationTime));

    UdpClientHelper client2(interfaces.GetAddress(1), port2);
    client2.SetAttribute("MaxPackets", UintegerValue(1000000));
    client2.SetAttribute("Interval", TimeValue(Seconds((packetSize * 8.0) / (rate2 * 1000000.0))));
    client2.SetAttribute("PacketSize", UintegerValue(packetSize));
    clientApp2 = client2.Install(nodes.Get(0));
    clientApp2.Start(Seconds(1.0));
    clientApp2.Stop(Seconds(simulationTime));
  }

  // ------------------------------------------------------------
  // 8. Enable PCAP tracing
  // ------------------------------------------------------------
  pointToPoint.EnablePcapAll("udp-assignment", true);

  // ------------------------------------------------------------
  // 9. Run simulation
  // ------------------------------------------------------------
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // ------------------------------------------------------------
  // 10. Performance Metrics Calculation
  //     (Latency, RTT, Throughput, PDR)
  // ------------------------------------------------------------
  double activeTime = simulationTime - 1.0; // 9.0 seconds active transmission
  double rtt = 2.0 * latency;              // Theoretical RTT = 2 * one-way latency

  // Flow 1 Metrics
  Ptr<UdpServer> serverPtr1 = DynamicCast<UdpServer>(serverApp1.Get(0));
  uint64_t received1 = serverPtr1->GetReceived();
  uint32_t lost1 = serverPtr1->GetLost();
  uint64_t totalSent1 = received1 + lost1;
  double pdr1 = (totalSent1 > 0) ? (static_cast<double>(received1) * 100.0 / totalSent1) : 0.0;
  double throughput1 = (received1 * packetSize * 8.0) / activeTime / 1000000.0;

  // ------------------------------------------------------------
  // 11. Print Performance Metrics Report
  // ------------------------------------------------------------
  std::cout << "\n=======================================================\n";
  std::cout << "             UDP PERFORMANCE METRICS REPORT            \n";
  std::cout << "=======================================================\n";
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Link Latency (One-Way) : " << latency << " ms\n";
  std::cout << "Round Trip Time (RTT)  : " << rtt << " ms (Theoretical: 2 * latency)\n";
  std::cout << "Active Simulation Time : " << activeTime << " s\n";
  std::cout << "-------------------------------------------------------\n";

  std::cout << "[Flow 1 - Port 5000]\n";
  std::cout << "  Offered Rate         : " << rate1 << " Mbps\n";
  std::cout << "  Packets Received     : " << received1 << "\n";
  std::cout << "  Packets Lost         : " << lost1 << "\n";
  std::cout << "  Packet Delivery Ratio: " << pdr1 << " %\n";
  std::cout << "  Throughput           : " << std::setprecision(5) << throughput1 << " Mbps\n";

  if (enableFlow2) {
    Ptr<UdpServer> serverPtr2 = DynamicCast<UdpServer>(serverApp2.Get(0));
    uint64_t received2 = serverPtr2->GetReceived();
    uint32_t lost2 = serverPtr2->GetLost();
    uint64_t totalSent2 = received2 + lost2;
    double pdr2 = (totalSent2 > 0) ? (static_cast<double>(received2) * 100.0 / totalSent2) : 0.0;
    double throughput2 = (received2 * packetSize * 8.0) / activeTime / 1000000.0;

    uint64_t totalRx = received1 + received2;
    uint32_t totalLost = lost1 + lost2;
    double overallPdr = ((totalRx + totalLost) > 0) ? (static_cast<double>(totalRx) * 100.0 / (totalRx + totalLost)) : 0.0;
    double totalThroughput = throughput1 + throughput2;

    std::cout << "\n[Flow 2 - Port 6000]\n";
    std::cout << std::setprecision(2);
    std::cout << "  Offered Rate         : " << rate2 << " Mbps\n";
    std::cout << "  Packets Received     : " << received2 << "\n";
    std::cout << "  Packets Lost         : " << lost2 << "\n";
    std::cout << "  Packet Delivery Ratio: " << pdr2 << " %\n";
    std::cout << "  Throughput           : " << std::setprecision(5) << throughput2 << " Mbps\n";

    std::cout << "-------------------------------------------------------\n";
    std::cout << "[Combined Network Summary]\n";
    std::cout << "  Total Packets Received: " << totalRx << "\n";
    std::cout << "  Overall PDR           : " << std::setprecision(2) << overallPdr << " %\n";
    std::cout << "  Total Throughput      : " << std::setprecision(5) << totalThroughput << " Mbps\n";
  }

  std::cout << "=======================================================\n\n";

  Simulator::Destroy();
  return 0;
}
