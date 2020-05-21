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
  *
  */
 
// Network topology
//
//            n0 ---+        +--- n2
//                  |        |
//                 n4 ----- n5
//                  |        |
//            n1 ---+        +--- n3
//
// - Access Link:       1000Mbps, 10ms
// - Bottleneck Link:      1Mbps, 480ms
// - TCP in total send 10000000 bytes, need 80s to send if 1Mbps.
 
 #include <iostream>
 #include <fstream>
 #include <string>
 #include <stdlib.h>
 #include <time.h>
 #include <vector>
 
 #include "ns3/core-module.h"
 #include "ns3/applications-module.h"
 #include "ns3/network-module.h"
 #include "ns3/internet-module.h"
 #include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
 #include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-apps-module.h"
 #include "ns3/traffic-control-module.h"
 #include "mytag.h"
 #include "rr_queue_disc.h"
 
 using namespace ns3;
 
 NS_LOG_COMPONENT_DEFINE ("MySimu10");
 

 //static const uint32_t totalTxBytes = 200000000;
 //static uint32_t currentTxBytes = 0;
 static const uint32_t writeSize = 1040;
 uint8_t data[writeSize]; 
 void StartFlow (Ptr<Socket>, Ipv4Address, uint16_t);
 void WriteUntilBufferFull (Ptr<Socket>, uint32_t);

 uint64_t rxBytes[20000];
 std::string congesCont[20000];
 uint32_t totalTxBytes[20000];
 uint32_t currentTxBytes[20000];

 int algoNum = 12;
 int flowPerAlgo = 100;

 



void query_fin_time (FlowMonitorHelper* flowmon, Ptr<FlowMonitor> monitor, double myTime, std::ofstream* f1, int flowNum){
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon->GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        
        *f1 << t.sourceAddress << "," << t.destinationPort << "," << i->second.timeLastRxPacket  << "," << i->second.timeFirstTxPacket << std::endl;
        

    }
}

void QueueMeasurement(Ptr<OutputStreamWrapper> stream, uint32_t n_packets_old, uint32_t n_packets)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << ',' << n_packets << std::endl;
}


void PktQueueMeasurement(Ptr<OutputStreamWrapper> streamwrapper, Ptr<Packet const> original_pkt)
{
    auto stream = streamwrapper->GetStream();
    auto pkt = original_pkt->Copy(); // We need to remove headers.
    PppHeader eth;
    pkt->RemoveHeader(eth);
    
    if (eth.GetProtocol() != 0x0021)
    {
        return; // We are only interested in the IP packets.
    }
    Ipv4Header ip;
    pkt->RemoveHeader(ip);
    auto prot = ip.GetProtocol();
    if (prot != 6)
    {
        return; // We only care about TCP flows.
    }
    TcpHeader tcp;
    pkt->PeekHeader(tcp);
    // Print Data: t, 5-tuple, seq_nr, ack_nr
    
    *stream << Simulator::Now().GetSeconds() << ',';
    ip.GetSource().Print(*stream);

    *stream << std::endl;
}

void TimePrint(int i){
  std::cout << i << std::endl;
}
 
 
 int main (int argc, char *argv[])
 {
   // Users may find it convenient to turn on explicit debugging
   // for selected modules; the below lines suggest how to do this
   //  LogComponentEnable("TcpL4Protocol", LOG_LEVEL_ALL);
   //  LogComponentEnable("TcpSocketImpl", LOG_LEVEL_ALL);
   //  LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
   //  LogComponentEnable("TcpLargeTransfer", LOG_LEVEL_ALL);
    //LogComponentEnable("TcpVegas", LOG_LEVEL_LOGIC);
    //LogComponentEnable("TcpCongestionOps", LOG_LEVEL_ALL);

    std::string fHead = "test";
    std::vector<std::string> cc_vector = {"ns3::TcpBic","ns3::TcpNewReno","ns3::TcpHighSpeed"
                                            ,"ns3::TcpHtcp","ns3::TcpHybla","ns3::TcpIllinois"
                                            ,"ns3::TcpLedbat","ns3::TcpScalable","ns3::TcpVegas"
                                            ,"ns3::TcpVeno","ns3::TcpWestwood","ns3::TcpYeah"};
    std::string access_bandwidth = "1000Mbps";
    std::string access_delay = "20ms";
    std::string bottle_bandwidth = "100Mbps";
    std::string bottle_delay = "10ms";
    bool udpFeature = 0;
    std::string udpOnTime = "1";
    std::string udpOffTime = "0";
    bool enable_trace_queue = 0;

    
    for (int i = 0; i < 5000; i++){
        rxBytes[i] = 0;
        congesCont[i] = "";
        totalTxBytes[i] = 0;
        currentTxBytes[i] = 0;
    }


    CommandLine cmd;
    cmd.AddValue ("algoNum", "algoNum", algoNum);
    cmd.AddValue ("flowPerAlgo", "flowPerAlgo", flowPerAlgo);
    cmd.AddValue ("fHead", "output file head name", fHead);
    cmd.AddValue ("access_bandwidth", "access_bandwidth", access_bandwidth);
    cmd.AddValue ("access_delay", "access_delay", access_delay);
    cmd.AddValue ("bottle_bandwidth", "bottle_bandwidth", bottle_bandwidth);
    cmd.AddValue ("bottle_delay", "bottle_delay", bottle_delay);
    cmd.AddValue ("udpOnTime", "udpOnTime", udpOnTime);
    cmd.AddValue ("udpOffTime", "udpOffTime", udpOffTime);
    cmd.AddValue ("udpFeature", "0 disable, 1 enable", udpFeature);
    cmd.AddValue ("enable_trace_queue","enable_trace_queue",enable_trace_queue);
    cmd.Parse (argc, argv);


    std::ofstream finfile("output/"+fHead+"_finT.txt");
    finfile << "sourceAddress,destinationPort,finTime,startTime" << std::endl;

    Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 22));
    Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 22));

    for (int i =0; i < algoNum*flowPerAlgo; i++){
      totalTxBytes[i] = 500000;
    }

    
 
    // initialize the tx buffer.
    for(uint32_t i = 0; i < writeSize; ++i)
    {
        char m = toascii (97 + i % 26);
        data[i] = m;
    }
 
    //
    // Explicility create the nodes required by the topology shown above.
    //
    NS_LOG_INFO ("Create nodes.");
    NodeContainer c;
    c.Create(2+algoNum*2);

    NodeContainer udpNodes;
    udpNodes.Create(2);


    //
    // Install Internet Stack
    //
    InternetStackHelper internet;
    internet.InstallAll ();


    //
    // Create channels
    //
    NS_LOG_INFO ("Create channels.");
    PointToPointHelper LocalLink;
    LocalLink.SetDeviceAttribute ("DataRate", StringValue(access_bandwidth));
    LocalLink.SetChannelAttribute ("Delay", StringValue(access_delay));
    LocalLink.SetQueue ("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize ("10p")));

    


    Ptr<Node> gateWay1 = c.Get (2*algoNum);
    Ptr<Node> gateWay2 = c.Get (2*algoNum+1);
    NetDeviceContainer devCon = NetDeviceContainer();
    for (int i = 0; i < algoNum; i++){
      devCon.Add (LocalLink.Install (c.Get (i), gateWay1));
    }
    for (int i = 0; i < algoNum; i++){
        devCon.Add (LocalLink.Install (c.Get (algoNum+i), gateWay2));
    }


    


    PointToPointHelper BottleLink;
    BottleLink.SetDeviceAttribute ("DataRate", StringValue(bottle_bandwidth));
    BottleLink.SetChannelAttribute ("Delay", StringValue(bottle_delay));
    BottleLink.SetQueue ("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize ("1p")));
    
    
    NetDeviceContainer tchDevice = BottleLink.Install (gateWay1, gateWay2);
    devCon.Add (tchDevice);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc ("ns3::RRQueueDisc");
    tch.Install (tchDevice.Get (0));
    
    


    NetDeviceContainer udpd0g1 = LocalLink.Install (udpNodes.Get (0), gateWay1);
    NetDeviceContainer udpd1g2 = LocalLink.Install (udpNodes.Get (1), gateWay2);
 

    //
    // Add IP addresses
    //
    NS_LOG_INFO ("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer intCon = Ipv4InterfaceContainer ();
    ipv4.SetBase ("10.1.0.0", "255.255.255.0");
    for (int i = 0; i < algoNum; i++){
        NetDeviceContainer tmpDevCon = NetDeviceContainer ();
        tmpDevCon.Add (devCon.Get (2*i));
        tmpDevCon.Add (devCon.Get (2*i+1));
        intCon.Add (ipv4.Assign (tmpDevCon));
        ipv4.NewNetwork ();
    }
    for (int i = 0; i < algoNum; i++){
        NetDeviceContainer tmpDevCon = NetDeviceContainer ();
        tmpDevCon.Add (devCon.Get (2*algoNum+2*i));
        tmpDevCon.Add (devCon.Get (2*algoNum+2*i+1));
        intCon.Add (ipv4.Assign (tmpDevCon));
        ipv4.NewNetwork ();
    }
    NetDeviceContainer tmpDevCon = NetDeviceContainer ();
    tmpDevCon.Add (devCon.Get (4*algoNum));
    tmpDevCon.Add (devCon.Get (4*algoNum+1));
    intCon.Add (ipv4.Assign (tmpDevCon));

    ipv4.NewNetwork ();
    Ipv4InterfaceContainer udpi0g1 = ipv4.Assign (udpd0g1);
    ipv4.NewNetwork ();
    Ipv4InterfaceContainer udpi1g2 = ipv4.Assign (udpd1g2);


   
    //
    // Turn on global static routing
    //
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    //
    // Create Applications
    //

    NS_LOG_INFO ("Create Applications.");

    uint16_t servPort = 0;
    ApplicationContainer sinkApps = ApplicationContainer ();
    for (uint16_t j = 1; j < (flowPerAlgo)+1; j++){
      servPort = j;
      PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), servPort));
      for (int i = 0; i < algoNum; i++){
          sinkApps.Add (sink.Install(c.Get(algoNum+i)));
      }
    }
    
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (10000.0));


    std::srand((unsigned)time(NULL));

    TypeId tid = TypeId::LookupByName (cc_vector[0]);
    for (int i = 0; i < algoNum; i++){
        
        tid = TypeId::LookupByName (cc_vector[i%12]);
        congesCont[i] = cc_vector[i%12];
        std::string specificNode = "/NodeList/" + std::to_string(i) + "/$ns3::TcpL4Protocol/SocketType";
        Config::Set (specificNode, TypeIdValue (tid));

        for (uint16_t j = 1; j < (flowPerAlgo)+1; j++){
          servPort = j;
          Ptr<Socket> localSocket = Socket::CreateSocket (c.Get (i), TcpSocketFactory::GetTypeId ());
          localSocket->Bind ();
          double start_time = std::rand()%1;
          //std::cout << start_time << std::endl;
          Simulator::Schedule (Seconds(start_time),&StartFlow, localSocket, intCon.GetAddress (2*algoNum+2*i), servPort);
        }
        
    }

    for (int i = 0; i < 100; i++){
      Simulator::Schedule (Seconds(i), &TimePrint, i);
    }


    if (udpFeature){
      uint16_t udpPort = 4000;
      Address udpSinkAddress (InetSocketAddress (Ipv4Address::GetAny (), udpPort));
      PacketSinkHelper udpSinkHelper ("ns3::UdpSocketFactory", udpSinkAddress);
      ApplicationContainer udpSink = udpSinkHelper.Install (udpNodes.Get (1));
      udpSink.Start (Seconds (0));
      udpSink.Stop (Seconds (2000));

      OnOffHelper udpClientHelper ("ns3::UdpSocketFactory", Address (InetSocketAddress (udpi1g2.GetAddress (0), udpPort)));
      udpClientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=10]"));
      udpClientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=10]"));
      udpClientHelper.SetAttribute("DataRate", DataRateValue(DataRate("800kb/s")));


      ApplicationContainer udpClientApp = udpClientHelper.Install (udpNodes.Get (0));
      udpClientApp.Start (Seconds (0));
      udpSink.Stop (Seconds (2000));
    }
    

    

    //
    // Calculate Throughput using Flowmonitor
    //
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();


    Simulator::Schedule(Seconds(9999), &query_fin_time, &flowmon, monitor, 9999, &finfile, flowPerAlgo*algoNum);

    if (enable_trace_queue){
      AsciiTraceHelper asciiTraceHelper;
      auto enqueuefile = asciiTraceHelper.CreateFileStream("output/"+fHead+"_enqueue.csv");
      *enqueuefile->GetStream() << "time" << ',' << "srcAddr"  << std::endl;
      auto dev = DynamicCast<PointToPointNetDevice>(devCon.Get(4*algoNum));
      //dev->GetQueue()->TraceConnectWithoutContext(
      //    "PacketsInQueue", MakeBoundCallback(&QueueMeasurement, queuefile)); 

 
      dev->GetQueue()->TraceConnectWithoutContext(
          "Enqueue", MakeBoundCallback(&PktQueueMeasurement, enqueuefile));  


      auto dequeuefile = asciiTraceHelper.CreateFileStream("output/"+fHead+"_dequeue.csv");
      *dequeuefile->GetStream() << "time" << ',' << "srcAddr"  << std::endl; 
      dev->GetQueue()->TraceConnectWithoutContext(
          "Dequeue", MakeBoundCallback(&PktQueueMeasurement, dequeuefile));  


      auto queuesizefile = asciiTraceHelper.CreateFileStream("output/"+fHead+"_queuesize.csv");
      *queuesizefile->GetStream() << "time" << ',' << "srcAddr"  << std::endl; 
      dev->GetQueue()->TraceConnectWithoutContext(
          "PacketsInQueue", MakeBoundCallback(&QueueMeasurement, queuesizefile)); 

      


    }


    //BottleLink.EnablePcapAll ("mylab3");

    Simulator::Stop (Seconds (10000));

    Simulator::Run ();
    Simulator::Destroy ();

    finfile.close();


/*    for (int i = 0 ; i < flowPerAlgo*algoNum; i++){
        Ptr<PacketSink> pSink = DynamicCast<PacketSink> (sinkApps.Get (i));
        std::cout << "Total Bytes Received by flow "+std::to_string(i)+"   :" << pSink->GetTotalRx () << std::endl;
    }*/

    
 }
 
 
 //-----------------------------------------------------------------------------
 //-----------------------------------------------------------------------------
 //-----------------------------------------------------------------------------
 //begin implementation of sending "Application"
 void StartFlow (Ptr<Socket> localSocket,
                 Ipv4Address servAddress,
                 uint16_t servPort)
 {
   NS_LOG_LOGIC ("Starting flow at time " <<  Simulator::Now ().GetSeconds ());
   localSocket->Connect (InetSocketAddress (servAddress, servPort)); //connect
 
   // tell the tcp implementation to call WriteUntilBufferFull again
   // if we blocked and new tx buffer space becomes available
   localSocket->SetSendCallback (MakeCallback (&WriteUntilBufferFull));
   WriteUntilBufferFull (localSocket, localSocket->GetTxAvailable ());
 }
 
 void WriteUntilBufferFull (Ptr<Socket> localSocket, uint32_t txSpace)
 {
   Ptr<Node> thisNode = localSocket->GetNode ();
   uint32_t algoID = thisNode->GetId ();
   
   Address* tmpAddress = new Address ();
   localSocket->GetPeerName (*tmpAddress);
   auto _tmpAddress = InetSocketAddress::ConvertFrom(*tmpAddress);
   uint16_t portNum = _tmpAddress.GetPort();
   uint16_t flowIndex = algoID*flowPerAlgo + portNum-1;
   
   uint32_t tag_value = 0;
   if (portNum <= 10){
    tag_value = algoID*1 + 1-1 + 1;
   }

   //std::cout << flowIndex << std::endl;
   while (currentTxBytes[flowIndex] < totalTxBytes[flowIndex] && localSocket->GetTxAvailable () > 0) 
     {
       uint32_t left = totalTxBytes[flowIndex] - currentTxBytes[flowIndex];
       uint32_t dataOffset = currentTxBytes[flowIndex] % writeSize;
       uint32_t toWrite = writeSize - dataOffset;
       toWrite = std::min (toWrite, left);
       toWrite = std::min (toWrite, localSocket->GetTxAvailable ());
       Ptr<Packet> pkt = Create<Packet> (&data[dataOffset], toWrite);
       MyTag tag; tag.SetSimpleValue (tag_value);
       pkt->AddPacketTag (tag);
       int amountSent = localSocket->Send (pkt, 0);

       if(amountSent < 0)
         {
           // we will be called again when new tx space becomes available.
           return;
         }else{
            currentTxBytes[flowIndex] += amountSent;
         }
       
     }
   if (currentTxBytes[flowIndex] >= totalTxBytes[flowIndex])
     {
       localSocket->Close ();
     }
 }
