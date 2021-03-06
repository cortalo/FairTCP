/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 Universita' degli Studi di Napoli Federico II
 *
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
 * Authors:  Stefano Avallone <stavallo@unina.it>
 */

#ifndef RR_QUEUE_DISC_H
#define RR_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/pointer.h"
#include "ns3/socket.h"
#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include <iostream>

#include <algorithm>
#include <iterator>
#include <memory.h>

#include <stdlib.h> 
#include <time.h> 

namespace ns3 {


//###########RRQueueDisc


NS_LOG_COMPONENT_DEFINE ("RRQueueDisc");

class RRQueueDisc : public QueueDisc {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief RRQueueDisc constructor
   *
   * Creates a queue with a depth of 1000 packets by default
   */
  RRQueueDisc ();

  virtual ~RRQueueDisc();

  // Reasons for dropping packets
  static constexpr const char* LIMIT_EXCEEDED_DROP = "Queue disc limit exceeded";  //!< Packet dropped due to queue disc limit exceeded

private:
  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  virtual Ptr<const QueueDiscItem> DoPeek (void);
  virtual bool CheckConfig (void);
  virtual void InitializeParams (void);

  uint32_t rr_queue_num;
  uint32_t now_queue_num;
};



NS_OBJECT_ENSURE_REGISTERED (RRQueueDisc);



TypeId RRQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RRQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<RRQueueDisc> ()
    
  ;
  return tid;
}


RRQueueDisc::RRQueueDisc ()
  : QueueDisc (QueueDiscSizePolicy::NO_LIMITS)
{
  NS_LOG_FUNCTION (this);
}

RRQueueDisc::~RRQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

bool
RRQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{

  NS_LOG_FUNCTION (this << item);
  MyTag algoTag;
  uint32_t algoNum = 0;

  if (item->GetPacket ()->PeekPacketTag (algoTag))
  {
  	algoNum = algoTag.GetSimpleValue ();
  }else{
  	algoNum = 0;
  }

  //std::cout << (uint32_t)algoNum << ",";
  
  bool retval = GetQueueDiscClass (algoNum)->GetQueueDisc ()->Enqueue (item);


  return retval;

	
}

Ptr<QueueDiscItem>
RRQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<QueueDiscItem> item;

  
  int prob = std::rand()%10;


  if (prob == 0){
    //First dequeue rr-queue sets.
    for (uint32_t i = 0; i < rr_queue_num; i++){
        now_queue_num = (now_queue_num+1)%rr_queue_num;
      if ((item = GetQueueDiscClass (now_queue_num + 1)->GetQueueDisc ()->Dequeue ()) != 0)
      {
        return item;
      }
    }
    //Try dequeue main queue.
    if ((item = GetQueueDiscClass (0)->GetQueueDisc ()->Dequeue ()) != 0)
    {
      return item;
    }

  }
  else{
    //First dequeue main queue.
    if ((item = GetQueueDiscClass (0)->GetQueueDisc ()->Dequeue ()) != 0)
    {
      return item;
    }
    //Try dequeue rr-queue sets.
    for (uint32_t i = 0; i < rr_queue_num; i++){
      now_queue_num = (now_queue_num+1)%rr_queue_num;
      if ((item = GetQueueDiscClass (now_queue_num + 1)->GetQueueDisc ()->Dequeue ()) != 0)
      {
        return item;
      }
    }

  }

  


  return item;

	
}

Ptr<const QueueDiscItem>
RRQueueDisc::DoPeek (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<const QueueDiscItem> item;

  std::srand( (unsigned)time( NULL ) );
  int prob = std::rand()%10;

  if (prob == 0){
    //First dequeue rr-queue sets.
    for (uint32_t i = 0; i < rr_queue_num; i++){
        now_queue_num = (now_queue_num+1)%rr_queue_num;
      if ((item = GetQueueDiscClass (now_queue_num + 1)->GetQueueDisc ()->Peek ()) != 0)
      {
        return item;
      }
    }
    //Try dequeue main queue.
    if ((item = GetQueueDiscClass (0)->GetQueueDisc ()->Peek ()) != 0)
    {
      return item;
    }

  }
  else{
    //First dequeue main queue.
    if ((item = GetQueueDiscClass (0)->GetQueueDisc ()->Peek ()) != 0)
    {
      return item;
    }
    //Try dequeue rr-queue sets.
    for (uint32_t i = 0; i < rr_queue_num; i++){
      now_queue_num = (now_queue_num+1)%rr_queue_num;
      if ((item = GetQueueDiscClass (now_queue_num + 1)->GetQueueDisc ()->Peek ()) != 0)
      {
        return item;
      }
    }

  }


  return item;

}

bool
RRQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  std::srand( (unsigned)time( NULL ) );
  
  ObjectFactory factory;
  factory.SetTypeId ("ns3::FifoQueueDisc");
  for (uint32_t i = 0; i < 2000; i++)
  {
  	Ptr<QueueDisc> qd = factory.Create<QueueDisc> ();
  	qd->Initialize ();
    if (i == 0){
      qd->SetMaxSize (QueueSize("10800p"));
    }else{
      qd->SetMaxSize (QueueSize("100p"));
    }
  	
  	Ptr<QueueDiscClass> c = CreateObject<QueueDiscClass> ();
  	c->SetQueueDisc (qd);
  	AddQueueDiscClass (c);
  }

  rr_queue_num = 12;
  now_queue_num = 0;



  return true;
}

void
RRQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
}


} // namespace ns3

#endif /* RR_QUEUE_DISC_H */
