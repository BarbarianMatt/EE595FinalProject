/*
 * Copyright (c) 2014 Universita' di Firenze
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
 * Author: Tommaso Pecorella <tommaso.pecorella@unifi.it>
 * Modified by Muyuan Shen
 */

#include "bernoulli_packet_socket_client.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet-socket-address.h"
#include "ns3/packet-socket-factory.h"
#include "ns3/packet-socket.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BernoulliPacketSocketClient");

NS_OBJECT_ENSURE_REGISTERED(BernoulliPacketSocketClient);

TypeId
BernoulliPacketSocketClient::GetTypeId()
{
   static TypeId tid =
       TypeId("ns3::BernoulliPacketSocketClient")
           .SetParent<Application>()
           .SetGroupName("Network")
           .AddConstructor<BernoulliPacketSocketClient>()
           .AddAttribute(
               "MaxPackets",
               "The maximum number of packets the application will send (zero means infinite)",
               UintegerValue(100),
               MakeUintegerAccessor(&BernoulliPacketSocketClient::m_maxPackets),
               MakeUintegerChecker<uint32_t>())
           .AddAttribute("TimeSlot",
                          "One slot of time in Bernoulli process",
                          TimeValue(MicroSeconds(9)),
                          MakeTimeAccessor(&BernoulliPacketSocketClient::m_timeSlot),
                          MakeTimeChecker()
                          )
           .AddAttribute("BernoulliPr",
                          "Probability in Bernoulli process",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&BernoulliPacketSocketClient::m_bernoulliPr),
                          MakeDoubleChecker<double>(0.0, 1.0)
                          )
           .AddAttribute("PacketSize",
                         "Size of packets generated (bytes).",
                         UintegerValue(1024),
                         MakeUintegerAccessor(&BernoulliPacketSocketClient::m_size),
                         MakeUintegerChecker<uint32_t>())
           .AddAttribute("Priority",
                         "Priority assigned to the packets generated.",
                         UintegerValue(0),
                         MakeUintegerAccessor(&BernoulliPacketSocketClient::SetPriority,
                                              &BernoulliPacketSocketClient::GetPriority),
                         MakeUintegerChecker<uint8_t>())
           .AddAttribute("OptionalTid", "The another TID (priority). If it is different "
                                        "from m_priority, then the client has a chance to use it.",
                         UintegerValue(0),
                         MakeUintegerAccessor(&BernoulliPacketSocketClient::m_optionalTid),
                         MakeUintegerChecker<uint32_t>(0, 7))
           .AddAttribute("OptionalTidPr",
                         "Probability to use the optional TID",
                         DoubleValue(0),
                         MakeDoubleAccessor(&BernoulliPacketSocketClient::m_optionalTidPr),
                         MakeDoubleChecker<double>(0.0, 1.0)
           )
           .AddTraceSource("Tx",
                           "A packet has been sent",
                           MakeTraceSourceAccessor(&BernoulliPacketSocketClient::m_txTrace),
                           "ns3::Packet::AddressTracedCallback");
   return tid;
}

BernoulliPacketSocketClient::BernoulliPacketSocketClient()
{
   NS_LOG_FUNCTION(this);
   m_sent = 0;
   m_socket = nullptr;
   m_sendEvent = EventId();
   m_peerAddressSet = false;
   m_uniformRngForInterval = CreateObject<UniformRandomVariable> ();
   m_uniformRngForInterval->SetAttribute("Min", DoubleValue (0.0));
   m_uniformRngForInterval->SetAttribute("Max", DoubleValue (1.0));
    m_uniformRngForTid = CreateObject<UniformRandomVariable> ();
    m_uniformRngForTid->SetAttribute("Min", DoubleValue (0.0));
    m_uniformRngForTid->SetAttribute("Max", DoubleValue (1.0));
}

BernoulliPacketSocketClient::~BernoulliPacketSocketClient()
{
   NS_LOG_FUNCTION(this);
}

void
BernoulliPacketSocketClient::SetRemote(PacketSocketAddress addr)
{
   NS_LOG_FUNCTION(this << addr);
   m_peerAddress = addr;
   m_peerAddressSet = true;
}

void
BernoulliPacketSocketClient::DoDispose()
{
   NS_LOG_FUNCTION(this);
   Application::DoDispose();
}

void
BernoulliPacketSocketClient::SetPriority(uint8_t priority)
{
   m_priority = priority;
   if (m_socket)
   {
       m_socket->SetPriority(priority);
   }
}

uint8_t
BernoulliPacketSocketClient::GetPriority() const
{
   return m_priority;
}

void
BernoulliPacketSocketClient::StartApplication()
{
   NS_LOG_FUNCTION(this);
   NS_ASSERT_MSG(m_peerAddressSet, "Peer address not set");

   if (!m_socket)
   {
       TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
       m_socket = Socket::CreateSocket(GetNode(), tid);

       m_socket->Bind(m_peerAddress);
       m_socket->Connect(m_peerAddress);

       if (m_priority)
       {
           m_socket->SetPriority(m_priority);
       }
   }

   m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
   m_sendEvent = Simulator::ScheduleNow(&BernoulliPacketSocketClient::Send, this);
}

void
BernoulliPacketSocketClient::StopApplication()
{
   NS_LOG_FUNCTION(this);
   Simulator::Cancel(m_sendEvent);
   m_socket->Close();
}

void
BernoulliPacketSocketClient::Send()
{
   NS_LOG_FUNCTION(this);
   NS_ASSERT(m_sendEvent.IsExpired());

   Ptr<Packet> p = Create<Packet>(m_size);

   std::stringstream peerAddressStringStream;
   peerAddressStringStream << PacketSocketAddress::ConvertFrom(m_peerAddress);

   if (m_optionalTid != m_priority)
   {
       // can give the optional TID a try, e.g., set the socket's priority
       if (m_uniformRngForTid->GetValue() < m_optionalTidPr)
       {
           // use the optional TID
           m_socket->SetPriority(m_optionalTid);
       }
       else
       {
           // use the original TID
           m_socket->SetPriority(m_priority);
       }
   }

   if ((m_socket->Send(p)) >= 0)
   {
       m_txTrace(p, m_peerAddress);
       NS_LOG_INFO("TraceDelay TX " << m_size << " bytes to " << peerAddressStringStream.str()
                                    << " Uid: " << p->GetUid()
                                    << " Time: " << (Simulator::Now()).GetSeconds());
   }
   else
   {
       NS_LOG_INFO("Error while sending " << m_size << " bytes to "
                                          << peerAddressStringStream.str());
   }
   m_sent++;

    // sample a geometric random number from uniform distribution
   // which is the inter-arrival time
    double uniform = m_uniformRngForInterval->GetValue();
    NS_ASSERT(m_bernoulliPr < 1);
    double numInterval = std::floor(std::log(uniform) / std::log(1 - m_bernoulliPr)) + 1;
    NS_ASSERT(numInterval > 0);
    Time interval = numInterval * m_timeSlot;

   if ((m_sent < m_maxPackets) || (m_maxPackets == 0))
   {
       m_sendEvent = Simulator::Schedule(interval, &BernoulliPacketSocketClient::Send, this);
   }
}

} // Namespace ns3
