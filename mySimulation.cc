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
 
 #include "ns3/core-module.h"
 #include "ns3/applications-module.h"
 #include "ns3/network-module.h"
 #include "ns3/internet-module.h"
 #include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
 #include "ns3/ipv4-global-routing-helper.h"
 
 using namespace ns3;
 
 NS_LOG_COMPONENT_DEFINE ("FlowFin");
 

 //static const uint32_t totalTxBytes = 200000000;
 //static uint32_t currentTxBytes = 0;
 static const uint32_t writeSize = 1040;
 uint8_t data[writeSize]; 
 void StartFlow (Ptr<Socket>, Ipv4Address, uint16_t);
 void WriteUntilBufferFull (Ptr<Socket>, uint32_t);

 uint64_t rxBytes[100];
 std::string congesCont[100];
 uint32_t totalTxBytes[100];
 uint32_t currentTxBytes[100];

 



void query_throughput (FlowMonitorHelper* flowmon, Ptr<FlowMonitor> monitor, double myTime, double dur, std::ofstream* f1, int flowNum){
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon->GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        for (int j = 0; j < flowNum; j++){
            if (t.sourceAddress == Ipv4Address(("10.1."+std::to_string(j)+".1").c_str() ) ) {
                uint64_t nowRxBytes = i->second.rxBytes - rxBytes[j];
                rxBytes[j] = i->second.rxBytes;
                *f1 << t.sourceAddress << ","  << congesCont[j] << "," << myTime <<"," <<nowRxBytes * 8.0 /dur/1024/1024 << std::endl;
            }
        }
    }

}

void query_fin_time (FlowMonitorHelper* flowmon, Ptr<FlowMonitor> monitor, double myTime, std::ofstream* f1, int flowNum){
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon->GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        for (int j = 0; j < flowNum; j++){
            if (t.sourceAddress == Ipv4Address(("10.1."+std::to_string(j)+".1").c_str() ) ) {
                *f1 << t.sourceAddress << "," << congesCont[j] << "," << i->second.timeLastRxPacket  << "," << i->second.timeFirstTxPacket << std::endl;
                //uint64_t nowRxBytes = i->second.rxBytes - rxBytes[j];
                //rxBytes[j] = i->second.rxBytes;
                //*f1 << t.sourceAddress << ","  << congesCont[j] << "," << myTime <<"," <<nowRxBytes * 8.0 /dur/1024/1024 << std::endl;
            }
        }
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
    
    /*if (eth.GetProtocol() != 0x0800)
    {
        return; // We are only interested in the IP packets.
    }*/
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
    std::string cc1 = "ns3::TcpNewReno";
    std::string cc2 = "ns3::TcpNewReno";
    std::string access_bandwidth = "1000Mbps";
    std::string access_delay_1 = "200ms";
    std::string access_delay_2 = "50ms";
    std::string bottle_bandwidth = "1Mbps";
    std::string bottle_delay = "100ms";
    double bufferFactor = 1;
    bool udpFeature = 0;
    std::string udpOnTime = "1";
    std::string udpOffTime = "0";
    bool enable_query_throughput = 0;
    bool enable_trace_queue = 0;

    int flowNum = 2;
    for (int i = 0; i < 100; i++){
        rxBytes[i] = 0;
        congesCont[i] = "";
        totalTxBytes[i] = 0;
        currentTxBytes[i] = 0;
    }


    CommandLine cmd;
    cmd.AddValue ("cc1", "cc algo 1", cc1);
    cmd.AddValue ("cc2", "cc algo 2", cc2);
    cmd.AddValue ("flowNum", "flowNum", flowNum);
    cmd.AddValue ("fHead", "output file head name", fHead);
    cmd.AddValue ("bufferFactor", "bufferFactor", bufferFactor);
    cmd.AddValue ("access_bandwidth", "access_bandwidth", access_bandwidth);
    cmd.AddValue ("access_delay_1", "access_delay_1", access_delay_1);
    cmd.AddValue ("access_delay_2", "access_delay_2", access_delay_2);
    cmd.AddValue ("bottle_bandwidth", "bottle_bandwidth", bottle_bandwidth);
    cmd.AddValue ("bottle_delay", "bottle_delay", bottle_delay);
    cmd.AddValue ("udpOnTime", "udpOnTime", udpOnTime);
    cmd.AddValue ("udpOffTime", "udpOffTime", udpOffTime);
    cmd.AddValue ("udpFeature", "0 disable, 1 enable", udpFeature);
    cmd.AddValue ("enable_query_throughput","enable_query_throughput",enable_query_throughput);
    cmd.AddValue ("enable_trace_queue","enable_trace_queue",enable_trace_queue);
    cmd.Parse (argc, argv);

    std::ofstream h1file("output/"+fHead+"_thput.txt");
    h1file << "sourceAddress,ccAlgo,time,throughput" << std::endl;

    std::ofstream finfile("output/"+fHead+"_finT.txt");
    finfile << "sourceAddress,ccAlgo,finTime,startTime" << std::endl;

    Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 22));
    Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 22));

    for (int i =0; i < flowNum; i++){
      totalTxBytes[i] = 50000000/flowNum;
    }

    DataRate bottle_b (bottle_bandwidth);
    Time access_d (access_delay_1);
    Time bottle_d (bottle_delay);
    uint32_t bufferSize = static_cast<uint32_t> ( (bottle_b.GetBitRate () / 8) * ((access_d + bottle_d + access_d)*2).GetSeconds () );
    bufferSize = bufferSize * bufferFactor;
    double transTTR = ((access_d + access_d + bottle_d)*2).GetSeconds ();
 
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
    c.Create(2+flowNum*2);

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
    PointToPointHelper LocalLink_1;
    LocalLink_1.SetDeviceAttribute ("DataRate", StringValue(access_bandwidth));
    LocalLink_1.SetChannelAttribute ("Delay", StringValue(access_delay_1));
    LocalLink_1.SetQueue ("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, 1500)));

    PointToPointHelper LocalLink_2;
    LocalLink_2.SetDeviceAttribute ("DataRate", StringValue(access_bandwidth));
    LocalLink_2.SetChannelAttribute ("Delay", StringValue(access_delay_2));
    LocalLink_2.SetQueue ("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, 1500)));


    Ptr<Node> gateWay1 = c.Get (2*flowNum);
    Ptr<Node> gateWay2 = c.Get (2*flowNum+1);
    NetDeviceContainer devCon = NetDeviceContainer();
    for (int i = 0; i < flowNum; i++){
        if (i%2 == 0){
          devCon.Add (LocalLink_1.Install (c.Get (i), gateWay1));
        }

        if (i%2 == 1){
          devCon.Add (LocalLink_2.Install (c.Get (i), gateWay1));
        }
        
    }
    for (int i = 0; i < flowNum; i++){
        if (i%2 == 0){
          devCon.Add (LocalLink_1.Install (c.Get (flowNum+i), gateWay2));
        }
        if (i%2 == 1){
          devCon.Add (LocalLink_2.Install (c.Get (flowNum+i), gateWay2));
        }
    }


    NetDeviceContainer udpd0g1 = LocalLink_1.Install (udpNodes.Get (0), gateWay1);
    NetDeviceContainer udpd1g2 = LocalLink_1.Install (udpNodes.Get (1), gateWay2);


    PointToPointHelper BottleLink;
    BottleLink.SetDeviceAttribute ("DataRate", StringValue(bottle_bandwidth));
    BottleLink.SetChannelAttribute ("Delay", StringValue(bottle_delay));
    BottleLink.SetQueue ("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, bufferSize)));
    devCon.Add (BottleLink.Install (gateWay1, gateWay2));
 

    //
    // Add IP addresses
    //
    NS_LOG_INFO ("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer intCon = Ipv4InterfaceContainer ();
    ipv4.SetBase ("10.1.0.0", "255.255.255.0");
    for (int i = 0; i < flowNum; i++){
        NetDeviceContainer tmpDevCon = NetDeviceContainer ();
        tmpDevCon.Add (devCon.Get (2*i));
        tmpDevCon.Add (devCon.Get (2*i+1));
        intCon.Add (ipv4.Assign (tmpDevCon));
        ipv4.NewNetwork ();
    }
    for (int i = 0; i < flowNum; i++){
        NetDeviceContainer tmpDevCon = NetDeviceContainer ();
        tmpDevCon.Add (devCon.Get (2*flowNum+2*i));
        tmpDevCon.Add (devCon.Get (2*flowNum+2*i+1));
        intCon.Add (ipv4.Assign (tmpDevCon));
        ipv4.NewNetwork ();
    }
    NetDeviceContainer tmpDevCon = NetDeviceContainer ();
    tmpDevCon.Add (devCon.Get (4*flowNum));
    tmpDevCon.Add (devCon.Get (4*flowNum+1));
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

    uint16_t servPort = 50000;
    PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), servPort));
    ApplicationContainer sinkApps = ApplicationContainer ();
    for (int i = 0; i < flowNum; i++){
        sinkApps.Add (sink.Install(c.Get(flowNum+i)));
    }
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (4000.0));


    std::srand((unsigned)time(NULL));

    TypeId tid = TypeId::LookupByName (cc1);
    std::stringstream nodeId;
    for (int i = 0; i < flowNum; i++){
        if (i%2 == 0){
            tid = TypeId::LookupByName (cc1);
            congesCont[i] = cc1;
        }else{
            tid = TypeId::LookupByName (cc2);
            congesCont[i] = cc2;
        }

        std::string specificNode = "/NodeList/" + std::to_string(i) + "/$ns3::TcpL4Protocol/SocketType";
        Config::Set (specificNode, TypeIdValue (tid));
        Ptr<Socket> localSocket = Socket::CreateSocket (c.Get (i), TcpSocketFactory::GetTypeId ());
        localSocket->Bind ();
        double start_time = std::rand()%500;
        //std::cout << start_time << std::endl;
        Simulator::Schedule (Seconds(start_time),&StartFlow, localSocket, intCon.GetAddress (2*flowNum+2*i), servPort);
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
    


/*    Address udpServerAddress = Address (udpi1g2.GetAddress (0));

    uint16_t udpPort = 4000;
    UdpServerHelper udpServer (udpPort);
    ApplicationContainer udpApps = udpServer.Install (udpNodes.Get (1));
    udpApps.Start (Seconds (300.0));
    udpApps.Stop (Seconds (700.0));

    uint32_t udpMaxPacketSize = 1024;
    Time udpInterPacketInterval = Seconds (0.02);
    uint32_t udpMaxPacketCount = 20000;
    UdpClientHelper udpClient (udpServerAddress, udpPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (udpMaxPacketCount));
    udpClient.SetAttribute ("Interval", TimeValue (udpInterPacketInterval));
    udpClient.SetAttribute ("PacketSize", UintegerValue (udpMaxPacketSize));
    udpApps = udpClient.Install (udpNodes.Get (0));
    udpApps.Start (Seconds (300.0));
    udpApps.Stop (Seconds (700.0));*/
    
    

    //
    // Calculate Throughput using Flowmonitor
    //
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    if (enable_query_throughput){
      for (int i = 0; i < (2000/transTTR/4); i ++){
          double myTime = 4 * i * transTTR;
          double dur = 4 * transTTR;
          Simulator::Schedule(Seconds(myTime), &query_throughput, &flowmon, monitor, myTime, dur , &h1file, flowNum);
      }
    }

    Simulator::Schedule(Seconds(3999), &query_fin_time, &flowmon, monitor, 3999, &finfile, flowNum);

    if (enable_trace_queue){
      AsciiTraceHelper asciiTraceHelper;
      auto enqueuefile = asciiTraceHelper.CreateFileStream("output/"+fHead+"_enqueue.csv");
      *enqueuefile->GetStream() << "time" << ',' << "srcAddr"  << std::endl;
      auto dev = DynamicCast<PointToPointNetDevice>(devCon.Get(4*flowNum));
      //dev->GetQueue()->TraceConnectWithoutContext(
      //    "PacketsInQueue", MakeBoundCallback(&QueueMeasurement, queuefile)); 

 
      dev->GetQueue()->TraceConnectWithoutContext(
          "Enqueue", MakeBoundCallback(&PktQueueMeasurement, enqueuefile));  


      auto dequeuefile = asciiTraceHelper.CreateFileStream("output/"+fHead+"_dequeue.csv");
      *dequeuefile->GetStream() << "time" << ',' << "srcAddr"  << std::endl; 
      dev->GetQueue()->TraceConnectWithoutContext(
          "Dequeue", MakeBoundCallback(&PktQueueMeasurement, dequeuefile));  

      auto dropqueuefile = asciiTraceHelper.CreateFileStream("output/"+fHead+"_dropqueue.csv");
      *dropqueuefile->GetStream() << "time" << ',' << "srcAddr"  << std::endl; 
      dev->GetQueue()->TraceConnectWithoutContext(
          "DropBeforeEnqueue", MakeBoundCallback(&PktQueueMeasurement, dropqueuefile)); 
      dev->GetQueue()->TraceConnectWithoutContext(
          "DropAfterDequeue", MakeBoundCallback(&PktQueueMeasurement, dropqueuefile)); 
      dev->GetQueue()->TraceConnectWithoutContext(
          "Drop", MakeBoundCallback(&PktQueueMeasurement, dropqueuefile));


    }


    //BottleLink.EnablePcapAll ("mylab3");

    Simulator::Stop (Seconds (4000));

    Simulator::Run ();
    Simulator::Destroy ();

    h1file.close();

    finfile.close();


    for (int i = 0 ; i < flowNum; i++){
        Ptr<PacketSink> pSink = DynamicCast<PacketSink> (sinkApps.Get (i));
        std::cout << "Total Bytes Received by flow "+std::to_string(i)+"   :" << pSink->GetTotalRx () << std::endl;
    }
    
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
   uint32_t flowIndex = thisNode->GetId ();
   //std::cout << flowIndex << std::endl;
   while (currentTxBytes[flowIndex] < totalTxBytes[flowIndex] && localSocket->GetTxAvailable () > 0) 
     {
       uint32_t left = totalTxBytes[flowIndex] - currentTxBytes[flowIndex];
       uint32_t dataOffset = currentTxBytes[flowIndex] % writeSize;
       uint32_t toWrite = writeSize - dataOffset;
       toWrite = std::min (toWrite, left);
       toWrite = std::min (toWrite, localSocket->GetTxAvailable ());
       int amountSent = localSocket->Send (&data[dataOffset], toWrite, 0);
       if(amountSent < 0)
         {
           // we will be called again when new tx space becomes available.
           return;
         }
       currentTxBytes[flowIndex] += amountSent;

     }
   if (currentTxBytes[flowIndex] >= totalTxBytes[flowIndex])
     {
       localSocket->Close ();
     }
 }
