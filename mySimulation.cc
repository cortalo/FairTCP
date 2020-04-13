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
// - Access Link:       1000Mbps, 450ms
// - Bottleneck Link:      1Mbps,  50ms
// - Access Link has 10x times buffer than bottleneck link.
 
 #include <iostream>
 #include <fstream>
 #include <string>
 
 #include "ns3/core-module.h"
 #include "ns3/applications-module.h"
 #include "ns3/network-module.h"
 #include "ns3/internet-module.h"
 #include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
 #include "ns3/ipv4-global-routing-helper.h"
 
 using namespace ns3;
 
 NS_LOG_COMPONENT_DEFINE ("MyLab3");
 

 static const uint32_t totalTxBytes = 10000000;
 static uint32_t currentTxBytes = 0;
 static const uint32_t writeSize = 1040;
 uint8_t data[writeSize]; 
 void StartFlow (Ptr<Socket>, Ipv4Address, uint16_t);
 void WriteUntilBufferFull (Ptr<Socket>, uint32_t);

 uint64_t rxBytes[100];
 std::string congesCont[100];



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

 
 
 int main (int argc, char *argv[])
 {
   // Users may find it convenient to turn on explicit debugging
   // for selected modules; the below lines suggest how to do this
   //  LogComponentEnable("TcpL4Protocol", LOG_LEVEL_ALL);
   //  LogComponentEnable("TcpSocketImpl", LOG_LEVEL_ALL);
   //  LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
   //  LogComponentEnable("TcpLargeTransfer", LOG_LEVEL_ALL);
    //LogComponentEnable("TcpVegas", LOG_LEVEL_LOGIC);
    //LogComponentEnable("TcpCongestionOps", LOG_LEVEL_LOGIC);

    std::string fHead = "test";
    std::string cc1 = "ns3::TcpNewReno";
    std::string cc2 = "ns3::TcpNewReno";
    std::string access_bandwidth = "1000Mbps";
    std::string access_delay = "450ms";
    std::string bottle_bandwidth = "1Mbps";
    std::string bottle_delay = "50ms";
    double bufferFactor = 1;

    int flowNum = 2;
    for (int i = 0; i < 100; i++){
        rxBytes[i] = 0;
        congesCont[i] = "";
    }


    CommandLine cmd;
    cmd.AddValue ("cc1", "cc algo 1", cc1);
    cmd.AddValue ("cc2", "cc algo 2", cc2);
    cmd.AddValue ("flowNum", "flowNum", flowNum);
    cmd.AddValue ("fHead", "output file head name", fHead);
    cmd.AddValue ("bufferFactor", "bufferFactor", bufferFactor);
    cmd.Parse (argc, argv);

    std::ofstream h1file("output/"+fHead+".txt");

    h1file << "sourceAddress,ccAlgo,time,throughput" << std::endl;
    DataRate bottle_b (bottle_bandwidth);
    Time access_d (access_delay);
    Time bottle_d (bottle_delay);
    uint32_t bufferSize = static_cast<uint32_t> ( (bottle_b.GetBitRate () / 8) * ((access_d + bottle_d)*2).GetSeconds () );
    bufferSize = bufferSize * bufferFactor;
 
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
    LocalLink.SetQueue ("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, 10*bufferSize)));
    LocalLink.SetDeviceAttribute ("DataRate", StringValue(access_bandwidth));
    LocalLink.SetChannelAttribute ("Delay", StringValue(access_delay));
    Ptr<Node> gateWay1 = c.Get (2*flowNum);
    Ptr<Node> gateWay2 = c.Get (2*flowNum+1);
    NetDeviceContainer devCon = NetDeviceContainer();
    for (int i = 0; i < flowNum; i++){
        devCon.Add (LocalLink.Install (c.Get (i), gateWay1));
    }
    for (int i = 0; i < flowNum; i++){
        devCon.Add (LocalLink.Install (c.Get (flowNum+i), gateWay2));
    }


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
    sinkApps.Stop (Seconds (1000.0));




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
        Simulator::ScheduleNow (&StartFlow, localSocket, intCon.GetAddress (2*flowNum+2*i), servPort);
    }
    
    

    //
    // Calculate Throughput using Flowmonitor
    //
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    for (int i = 0; i < 200; i ++){
        double myTime = 1 * i;
        double dur = 1;
        Simulator::Schedule(Seconds(myTime), &query_throughput, &flowmon, monitor, myTime, dur , &h1file, flowNum);
    }

    Simulator::Stop (Seconds (1000));

    Simulator::Run ();
    Simulator::Destroy ();
    h1file.close();

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
   while (currentTxBytes < totalTxBytes && localSocket->GetTxAvailable () > 0) 
     {
       uint32_t left = totalTxBytes - currentTxBytes;
       uint32_t dataOffset = currentTxBytes % writeSize;
       uint32_t toWrite = writeSize - dataOffset;
       toWrite = std::min (toWrite, left);
       toWrite = std::min (toWrite, localSocket->GetTxAvailable ());
       int amountSent = localSocket->Send (&data[dataOffset], toWrite, 0);
       if(amountSent < 0)
         {
           // we will be called again when new tx space becomes available.
           return;
         }
       currentTxBytes += amountSent;
     }
   if (currentTxBytes >= totalTxBytes)
     {
       // std::cout << "Transit Finish!" << std::endl;
       localSocket->Close ();
     }
 }
