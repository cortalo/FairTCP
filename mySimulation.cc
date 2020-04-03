/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Network topology
//
//            n0 ---+        +--- n2
//                  |        |
//                 n4 ----- n5
//                  |        |
//            n1 ---+        +--- n3
//
// - All links are p2p with 500kb/s and 2ms
// - TCP flow from n0 to n2
// - TCP flow from n1 to n3

#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MyLab2");


int main (int argc, char *argv[])
{
    std::string lat = "2ms";
    std::string rate = "500kb/s";
    uint32_t maxBytes = 3000000;

    CommandLine cmd;
    cmd.AddValue ("latency", "P2P link latency in miliseconds", lat);
    cmd.AddValue ("rate", "P2P data rate in bps", rate);
    cmd.AddValue ("maxBytes", "Total number of bytes for application to send", maxBytes);

    cmd.Parse (argc, argv);

    
    //
    // Explicility create the nodes required by the topology shown above.
    //
    NS_LOG_INFO ("Create nodes.");
    NodeContainer c;
    c.Create(6);

    NodeContainer n0n4 = NodeContainer (c.Get (0), c.Get (4));
    NodeContainer n1n4 = NodeContainer (c.Get (1), c.Get (4));
    NodeContainer n2n5 = NodeContainer (c.Get (2), c.Get (5));
    NodeContainer n3n5 = NodeContainer (c.Get (3), c.Get (5));
    NodeContainer n4n5 = NodeContainer (c.Get (4), c.Get (5));

    //
    // Select TCP variant
    //
    TypeId tid_0 = TypeId::LookupByName ("ns3::TcpVeno");
    std::string specificNode_0 = "/NodeList/0/$ns3::TcpL4Protocol/SocketType";
    Config::Set (specificNode_0, TypeIdValue (tid_0));

    TypeId tid_1 = TypeId::LookupByName ("ns3::TcpBic");
    std::string specificNode_1 = "/NodeList/1/$ns3::TcpL4Protocol/SocketType";
    Config::Set (specificNode_1, TypeIdValue (tid_1));
    

    //
    // Install Internet Stack
    //
    InternetStackHelper internet;
    internet.Install (c);

    //
    // Create channels
    //
    NS_LOG_INFO ("Create channels.");
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue (rate));
    p2p.SetChannelAttribute ("Delay", StringValue (lat));
    NetDeviceContainer d0d4 = p2p.Install (n0n4);
    NetDeviceContainer d1d4 = p2p.Install (n1n4);
    NetDeviceContainer d4d5 = p2p.Install (n4n5);
    NetDeviceContainer d2d5 = p2p.Install (n2n5);
    NetDeviceContainer d3d5 = p2p.Install (n3n5);
    //
    // Add IP addresses
    //
    NS_LOG_INFO ("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i4 = ipv4.Assign (d0d4);

    ipv4.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i4 = ipv4.Assign (d1d4);

    ipv4.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i4i5 = ipv4.Assign (d4d5);

    ipv4.SetBase ("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i5 = ipv4.Assign (d2d5);

    ipv4.SetBase ("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i5 = ipv4.Assign (d3d5);

    NS_LOG_INFO ("Enable static global routing.");
    //
    // Turn on global static routing
    //
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    NS_LOG_INFO ("Create Applications.");

    // TCP connections

    uint16_t port = 9;

    BulkSendHelper source_1 ("ns3::TcpSocketFactory", InetSocketAddress (i2i5.GetAddress (0), port));
    source_1.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
    ApplicationContainer sourceApps = source_1.Install (c.Get (0));
    BulkSendHelper source_2 ("ns3::TcpSocketFactory", InetSocketAddress (i3i5.GetAddress (0), port));
    source_2.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
    sourceApps.Add (source_2.Install (c.Get (1)));
    sourceApps.Start (Seconds (1.0));
    sourceApps.Stop (Seconds (10.0));

    PacketSinkHelper sink_1 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApps = sink_1.Install (c.Get (2));
    PacketSinkHelper sink_2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    sinkApps.Add (sink_2.Install (c.Get (3)));
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (10.0));

    //
    // Set up pcap tracing
    //
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll (ascii.CreateFileStream ("mylab_2.tr"));
    p2p.EnablePcapAll ("mylab_2", false);

    //
    // Calculate Throughput using Flowmonitor
    //
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    //
    // Now, do the actual simulation
    //
    NS_LOG_INFO ("Run Simulation.");
    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " ->" << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:    " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:    " << i->second.rxBytes << "\n";
        std::cout << "  Throughput:  " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024 << " Mbps\n";
    }

    Simulator::Destroy ();

    NS_LOG_INFO ("Done.");

    Ptr<PacketSink> pSink_1 = DynamicCast<PacketSink> (sinkApps.Get (0));
    Ptr<PacketSink> pSink_2 = DynamicCast<PacketSink> (sinkApps.Get (1));
    std::cout << "Total Bytes Received by n2:  " << pSink_1->GetTotalRx () << std::endl;
    std::cout << "Total Bytes Received by n3:  " << pSink_2->GetTotalRx () << std::endl;

    
}
