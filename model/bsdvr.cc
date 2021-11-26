/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#define NS_LOG_APPEND_CONTEXT                                   \
  if (m_ipv4) { std::clog << "[node " << m_ipv4->GetObject<Node> ()->GetId () << "] "; }

#include "bsdvr.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/random-variable-stream.h"
#include "ns3/inet-socket-address.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/udp-l4-protocol.h"
#include "ns3/udp-header.h"
#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/wifi-mac-queue-item.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include <algorithm>
#include <limits>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BsdvrRoutingProtocol");

namespace bsdvr{

NS_OBJECT_ENSURE_REGISTERED (RoutingProtocol);

/// UDP Port for BSDVR control traffic
const uint32_t RoutingProtocol::BSDVR_PORT = 653;

/**
* \ingroup bsdvr
* \brief Tag used by BSDVR implementation
*/
class DeferredRouteOutputTag : public Tag
{

public:
  /**
   * \brief Constructor
   * \param o the output interface
   */
  DeferredRouteOutputTag (int32_t o = -1) : Tag (),
                                            m_oif (o)
  {
  }

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ()
  {
    static TypeId tid = TypeId ("ns3::bsdvr::DeferredRouteOutputTag")
      .SetParent<Tag> ()
      .SetGroupName ("Bsdvr")
      .AddConstructor<DeferredRouteOutputTag> ()
    ;
    return tid;
  }

  TypeId  GetInstanceTypeId () const
  {
    return GetTypeId ();
  }

  /**
   * \brief Get the output interface
   * \return the output interface
   */
  int32_t GetInterface () const
  {
    return m_oif;
  }

  /**
   * \brief Set the output interface
   * \param oif the output interface
   */
  void SetInterface (int32_t oif)
  {
    m_oif = oif;
  }

  uint32_t GetSerializedSize () const
  {
    return sizeof(int32_t);
  }

  void  Serialize (TagBuffer i) const
  {
    i.WriteU32 (m_oif);
  }

  void  Deserialize (TagBuffer i)
  {
    m_oif = i.ReadU32 ();
  }

  void  Print (std::ostream &os) const
  {
    os << "DeferredRouteOutputTag: output interface = " << m_oif;
  }

private:
  /// Positive if output device is fixed in RouteOutput
  int32_t m_oif;
};

NS_OBJECT_ENSURE_REGISTERED (DeferredRouteOutputTag);

//-----------------------------------------------------------------------------
RoutingProtocol::RoutingProtocol ()
  : m_enableHello (false),
    m_helloInterval (Seconds (5)),
    m_nb (m_helloInterval),
    m_maxQueueLen (64),
    m_queue (m_maxQueueLen),
    m_htimer (Timer::CANCEL_ON_DESTROY),
    m_lastBcastTime (Seconds (0))
{
  m_nb.SetCallback (MakeCallback (&RoutingProtocol::SendUpdateOnLinkFailure, this));
}

TypeId
RoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::bsdvr::RoutingProtocol")
    //FIXME: Add attributes for data plane packet buffer and control plane pending reply buffer
    .SetParent<Ipv4RoutingProtocol> ()
    .SetGroupName ("Bsdvr")
    .AddConstructor<RoutingProtocol> ()
    .AddAttribute ("HelloInterval", "HELLO messages emission interval.",
                   TimeValue (Seconds (1)),
                   MakeTimeAccessor (&RoutingProtocol::m_helloInterval),
                   MakeTimeChecker ())
    .AddAttribute ("MaxQueueLen", "Maximum number of packets that we allow a routing protocol to buffer.",
                   UintegerValue (64),
                   MakeUintegerAccessor (&RoutingProtocol::SetMaxQueueLen,
                                         &RoutingProtocol::GetMaxQueueLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("EnableHello", "Indicates whether a hello messages enable.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetHelloEnable,
                                        &RoutingProtocol::GetHelloEnable),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableBroadcast", "Indicates whether a broadcast data packets forwarding enable.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetBroadcastEnable,
                                        &RoutingProtocol::GetBroadcastEnable),
                   MakeBooleanChecker ())
    .AddAttribute ("UniformRv",
                   "Access to the underlying UniformRandomVariable",
                   StringValue ("ns3::UniformRandomVariable"),
                   MakePointerAccessor (&RoutingProtocol::m_uniformRandomVariable),
                   MakePointerChecker<UniformRandomVariable> ())
    ;
  return tid;
}

void
RoutingProtocol::SetMaxQueueLen (uint32_t len)
{
  m_maxQueueLen = len;
  m_queue.SetMaxQueueLen (len);
}
RoutingProtocol::~RoutingProtocol ()
{
}

void
RoutingProtocol::DoDispose ()
{
  m_ipv4 = 0;
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::iterator iter = 
       m_socketAddresses.begin (); iter != m_socketAddresses.end (); iter++)
     {
       iter->first->Close ();
     }
  m_socketAddresses.clear ();
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::iterator iter =
         m_socketSubnetBroadcastAddresses.begin (); iter != m_socketSubnetBroadcastAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketSubnetBroadcastAddresses.clear ();
  Ipv4RoutingProtocol::DoDispose ();
}

void 
RoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream () << "Node: " << m_ipv4->GetObject<Node> ()->GetId ()
                        << "; Time: " << Now ().As (unit)
                        << ", Local time: " << m_ipv4->GetObject<Node> ()->GetLocalTime ().As (unit)
                        << ", BSDVR Routing table" << std::endl;
  std::map<Ipv4Address, RoutingTableEntry> ft = m_routingTable.GetForwardingTablePrint ();                 
  m_routingTable.Print (&ft, stream, unit);
  *stream->GetStream () << std::endl;
}

int64_t 
RoutingProtocol::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uniformRandomVariable->SetStream (stream);
  return 1;
}

void
RoutingProtocol::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  uint32_t startTime;
  if (m_enableHello)
    {
      m_htimer.SetFunction (&RoutingProtocol::HelloTimerExpire, this);
      startTime = m_uniformRandomVariable->GetInteger (0, 100);
      NS_LOG_DEBUG ("Starting at time " << startTime << "ms");
      m_htimer.Schedule (MilliSeconds (startTime));
    }
  Ipv4RoutingProtocol::DoInitialize ();
}

void 
RoutingProtocol::Start ()
{
  NS_LOG_FUNCTION (this);
  if (m_enableHello)
    {
      m_nb.ScheduleTimer ();
    }
}

Ptr<Ipv4Route> 
RoutingProtocol::LoopbackRoute (const Ipv4Header & hdr, Ptr<NetDevice> oif) const
{
  NS_LOG_FUNCTION (this << hdr);
  NS_ASSERT (m_lo != 0);
  Ptr<Ipv4Route> rt = Create<Ipv4Route> ();
  rt->SetDestination (hdr.GetDestination ());
  //
  // Source address selection here is tricky.  The loopback route is
  // returned when BSDVR does not have a route; this causes the packet
  // to be looped back and handled (cached) in RouteInput() method
  // while a route is found. However, connection-oriented protocols
  // like TCP need to create an endpoint four-tuple (src, src port,
  // dst, dst port) and create a pseudo-header for checksumming.  So,
  // BSDVR needs to guess correctly what the eventual source address
  // will be.
  //
  // For single interface, single address nodes, this is not a problem.
  // When there are possibly multiple outgoing interfaces, the policy
  // implemented here is to pick the first available BSDVR interface.
  // If RouteOutput() caller specified an outgoing interface, that
  // further constrains the selection of source address
  //
  std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin ();
  if (oif)
    {
      // Iterate to find an address on the oif device
      for (j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
        {
          Ipv4Address addr = j->second.GetLocal ();
          int32_t interface = m_ipv4->GetInterfaceForAddress (addr);
          if (oif == m_ipv4->GetNetDevice (static_cast<uint32_t> (interface)))
            {
              rt->SetSource (addr);
              break;
            }
        }
    }
  else
    {
      rt->SetSource (j->second.GetLocal ());
    }
  NS_ASSERT_MSG (rt->GetSource () != Ipv4Address (), "Valid BSDVR source address not found");
  rt->SetGateway (Ipv4Address ("127.0.0.1"));
  rt->SetOutputDevice (m_lo);
  return rt;
}

Ptr<Ipv4Route> 
RoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, 
                              Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << header << (oif ? oif->GetIfIndex () : 0));
  if (!p)
    {
      NS_LOG_DEBUG ("Packet is == 0");
      return LoopbackRoute (header, oif);
    }
  if (m_socketAddresses.empty ())
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
      NS_LOG_LOGIC ("No bsdvr interfaces");
      Ptr<Ipv4Route> route;
      return route;
    }
  sockerr = Socket::ERROR_NOTERROR;
  Ptr<Ipv4Route> route;
  Ipv4Address dst = header.GetDestination ();
  RoutingTableEntry rt;
  std::map<Ipv4Address, RoutingTableEntry> *ft = m_routingTable.GetForwardingTable (); 
  if (m_routingTable.LookupRoute(dst, rt, ft))
    {
      route = rt.GetRoute ();
      NS_ASSERT (route != 0);
      NS_LOG_DEBUG ("Exist route to " << route->GetDestination () << " from interface " << route->GetSource ());
      if (oif != 0 && route->GetOutputDevice () != oif)
        {
          NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
          sockerr = Socket::ERROR_NOROUTETOHOST;
          return Ptr<Ipv4Route> ();
        }
      return route;
    }
  // Valid route not found, in this case we return loopback.
  // routed to loopback, received from loopback and passed to RouteInput (see below)
  uint32_t iif = (oif ? m_ipv4->GetInterfaceForDevice (oif) : -1);
  DeferredRouteOutputTag tag (iif);
  NS_LOG_DEBUG ("Valid Route not found");
  if (!p->PeekPacketTag (tag))
    {
      p->AddPacketTag (tag);
    }
  return LoopbackRoute (header, oif);
}

void 
RoutingProtocol::DeferredRouteOutput (Ptr<const Packet> p, const Ipv4Header & header, 
                     UnicastForwardCallback ucb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header);
  NS_ASSERT (p != 0 && p != Ptr<Packet> ());
  QueueEntry newEntry (p, Status(), header, ucb, ecb);
  bool result = m_queue.Enqueue (newEntry);
  if (result)
    {
      NS_LOG_LOGIC ("Add packet " << p->GetUid () << " to queue. Protocol " << (uint16_t) header.GetProtocol ());
    }
}

bool 
RoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, 
                             Ptr<const NetDevice> idev, UnicastForwardCallback ucb, 
                             MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << " received packet " << p->GetUid ()
                        << " from " << header.GetSource ()
                        << " on interface " << idev->GetAddress ()
                        << " to destination " << header.GetDestination ());
  if (m_socketAddresses.empty ())
    {
      NS_LOG_LOGIC ("No Bsdvr interfaces");
      return false;
    }
  NS_ASSERT (m_ipv4 != 0);
  NS_ASSERT (p != 0);
  // Check if input device supports IP
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
  int32_t iif = m_ipv4->GetInterfaceForDevice (idev);

  Ipv4Address dst = header.GetDestination ();
  Ipv4Address origin = header.GetSource ();

  // Deferred route request
  if (idev == m_lo)
    {
      DeferredRouteOutputTag tag;
      if (p->PeekPacketTag (tag))
        {
          DeferredRouteOutput (p, header, ucb, ecb);
          return true;
        }
    }
  // Duplicate of own packet
  if (IsMyOwnAddress (origin))
    {
      return true;
    }
  // BSDVR is not a multicast routing protocol
  if (dst.IsMulticast ())
    {
      return false;
    }
  
  // Broadcast local delivery/forwarding
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv4InterfaceAddress iface = j->second;
      if (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()) == iif)
        {
          if (dst == iface.GetBroadcast () || dst.IsBroadcast ())
            {
              Ptr<Packet> packet = p->Copy ();
              if (lcb.IsNull () == false)
                {
                  NS_LOG_LOGIC ("Broadcast local delivery to " << iface.GetLocal ());
                  lcb (p, header, iif);
                  // Fall through to additional processing
                }
              else
                {
                  NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
                  ecb (p, header, Socket::ERROR_NOROUTETOHOST);
                }
              if (!m_enableBroadcast)
                {
                  return true;
                }
              if (header.GetProtocol () == UdpL4Protocol::PROT_NUMBER)
                {
                  UdpHeader udpHeader;
                  p->PeekHeader (udpHeader);
                  if (udpHeader.GetDestinationPort () == BSDVR_PORT)
                    {
                      // AODV packets sent in broadcast are already managed
                      return true;
                    }
                }
              if (header.GetTtl () > 1)
                {
                  NS_LOG_LOGIC ("Forward broadcast. TTL " << (uint16_t) header.GetTtl ());
                  RoutingTableEntry toBroadcast;
                  std::map<Ipv4Address, ns3::bsdvr::RoutingTableEntry>* ft = m_routingTable.GetForwardingTable ();
                  if (m_routingTable.LookupRoute (dst, toBroadcast, ft))
                    {
                      Ptr<Ipv4Route> route = toBroadcast.GetRoute ();
                      ucb (route, packet, header);
                    }
                  else
                    {
                      NS_LOG_DEBUG ("No route to forward broadcast. Drop packet " << p->GetUid ());
                    }
                }
              else
                {
                  NS_LOG_DEBUG ("TTL exceeded. Drop packet " << p->GetUid ());
                }
              return true;
            }
        }
    }
  // Unicast local delivery
  if (m_ipv4->IsDestinationAddress (dst, iif))
    {
      /// NOTE: Confirm if neighbors Update () is required here
      if (lcb.IsNull () == false)
        {
          NS_LOG_LOGIC ("Unicast local delivery to " << dst);
          lcb (p, header, iif);
        }
      else
        {
          NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
          ecb (p, header, Socket::ERROR_NOROUTETOHOST);
        }
      return true;
    }
  // Check if input device supports IP forwarding
  if (m_ipv4->IsForwarding (iif) == false)
    {
      NS_LOG_LOGIC ("Forwarding disabled for this interface");
      ecb (p, header, Socket::ERROR_NOROUTETOHOST);
      return true;
    }

  // Forwarding
  return Forwarding (p, header, ucb, ecb);
}
bool 
RoutingProtocol::Forwarding (Ptr<const Packet> p, const Ipv4Header & header,
                             UnicastForwardCallback ucb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this);
  Ipv4Address dst = header.GetDestination ();
  RoutingTableEntry toDst;
  std::map<Ipv4Address, ns3::bsdvr::RoutingTableEntry>* ft = m_routingTable.GetForwardingTable ();
  if (m_routingTable.LookupRoute (dst, toDst, ft))
    {
      Ptr<Ipv4Route> route = toDst.GetRoute ();
      NS_LOG_LOGIC (route->GetSource () << "is forwarding packet " << p->GetUid ()
                                        << " to " << dst
                                        << " from " << header.GetSource ()
                                        << " via nexthop neighbor " << toDst.GetNextHop ());
      
      /// NOTE: Confirm if neighbors Update () is required here

      ucb (route, p, header);
      return true;
    }
  NS_LOG_DEBUG ("Drop packet " << p->GetUid () << " because no route to forward it.");
  return false;
}
void 
RoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_ASSERT (ipv4 != 0);
  NS_ASSERT (m_ipv4 == 0);

  m_ipv4 = ipv4;
  
  // Create lo route. It is asserted that the only one interface up for now is loopback
  NS_ASSERT (m_ipv4->GetNInterfaces () == 1 && m_ipv4->GetAddress (0, 0).GetLocal () == Ipv4Address ("127.0.0.1"));
  m_lo = m_ipv4->GetNetDevice (0);
  NS_ASSERT (m_lo != 0);
  // Remember lo route
  RoutingTableEntry rt(/*device=*/ m_lo, /*dst=*/ Ipv4Address::GetLoopback (), 
                       /*iface=*/ Ipv4InterfaceAddress (Ipv4Address::GetLoopback (), Ipv4Mask ("255.0.0.0")), 
                       /*hops=*/ 1, /*next hop=*/ Ipv4Address::GetLoopback (), /*changedEntries*/ false);
  std::map<Ipv4Address, ns3::bsdvr::RoutingTableEntry>* ft = m_routingTable.GetForwardingTable ();
  m_routingTable.AddRoute (rt, ft);
  Simulator::ScheduleNow (&RoutingProtocol::Start,this);
}
void 
RoutingProtocol::NotifyInterfaceUp (uint32_t i)
{
  NS_LOG_FUNCTION (this << m_ipv4->GetAddress (i, 0).GetLocal ());
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  if (l3->GetNAddresses (i) > 1)
    {
      NS_LOG_WARN ("BSDVR does not work with more then one address per each interface.");
    }
  Ipv4InterfaceAddress iface = l3->GetAddress (i,0);
  if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
    {
      return;
    }
  // Create a socket to listen only on this interface
  Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                             UdpSocketFactory::GetTypeId ());
  NS_ASSERT (socket != 0);
  socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvBsdv, this));
  socket->BindToNetDevice (l3->GetNetDevice (i));
  socket->Bind (InetSocketAddress (iface.GetLocal (), BSDVR_PORT));
  socket->SetAllowBroadcast (true);
  socket->SetIpRecvTtl (true);
  m_socketAddresses.insert (std::make_pair (socket, iface));
  
  /// NOTE: See if subnet broadcast socket required here
  socket = Socket::CreateSocket (GetObject<Node> (),
                                 UdpSocketFactory::GetTypeId ());
  NS_ASSERT (socket != 0);
  socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvBsdv, this));
  socket->BindToNetDevice (l3->GetNetDevice (i));
  socket->Bind (InetSocketAddress (iface.GetBroadcast (), BSDVR_PORT));
  socket->SetAllowBroadcast (true);
  socket->SetIpRecvTtl (true);
  m_socketSubnetBroadcastAddresses.insert (std::make_pair (socket, iface));

  // Add local broadcast record to the routing table
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
  RoutingTableEntry rt(/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*iface=*/ iface, 
                       /*hops=*/ 1, /*next hop=*/ iface.GetBroadcast (), /*changedEntries*/ false);
  std::map<Ipv4Address, ns3::bsdvr::RoutingTableEntry>* ft = m_routingTable.GetForwardingTable ();
  m_routingTable.AddRoute (rt, ft);
  if (m_mainAddress == Ipv4Address ())
    {
      m_mainAddress = iface.GetLocal ();
    }
  NS_ASSERT (m_mainAddress != Ipv4Address ());

  if (l3->GetInterface (i)->GetArpCache ())
    {
      m_nb.AddArpCache (l3->GetInterface (i)->GetArpCache ());
    }
   // Allow neighbor manager use this interface for layer 2 feedback if possible
   Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
   if (wifi == 0)
    {
      return;
    }
   Ptr<WifiMac> mac = wifi->GetMac ();
   if (mac == 0)
    {
      return;
    }

  mac->TraceConnectWithoutContext ("DroppedMpdu", MakeCallback (&RoutingProtocol::NotifyTxError, this));
}
void 
RoutingProtocol::NotifyTxError (WifiMacDropReason reason, Ptr<const WifiMacQueueItem> mpdu)
{
  m_nb.GetTxErrorCallback ()(mpdu->GetHeader ());
}
void 
RoutingProtocol::NotifyInterfaceDown (uint32_t i)
{
  NS_LOG_FUNCTION (this << m_ipv4->GetAddress (i, 0).GetLocal ());

  //Disable layer 2 link state monitoring (if possible)
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  Ptr<NetDevice> dev = l3->GetNetDevice (i);
  Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
  if (wifi != 0)
    {
      Ptr<WifiMac> mac = wifi->GetMac ()->GetObject<AdhocWifiMac> ();
      if (mac != 0)
        {
          mac->TraceDisconnectWithoutContext ("DroppedMpdu",
                                              MakeCallback (&RoutingProtocol::NotifyTxError, this));
          m_nb.DelArpCache (l3->GetInterface (i)->GetArpCache ());
        }
    }

  // Close socket
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (m_ipv4->GetAddress (i, 0));
  NS_ASSERT (socket);
  socket->Close ();
  m_socketAddresses.erase (socket);
  
  /// NOTE: See if subnet broadcast socket required here
  // Close socket
  socket = FindSubnetBroadcastSocketWithInterfaceAddress (m_ipv4->GetAddress (i, 0));
  NS_ASSERT (socket);
  socket->Close ();
  m_socketSubnetBroadcastAddresses.erase (socket);

  if (m_socketAddresses.empty ())
    {
      NS_LOG_LOGIC ("No bsdvr interfaces");
      m_htimer.Cancel ();
      m_nb.Clear ();
      m_routingTable.Clear (); // clears forwarding table
      return;
    }
  std::map<Ipv4Address, RoutingTableEntry>* ft = m_routingTable.GetForwardingTable ();
  m_routingTable.DeleteAllRoutesFromInterface (m_ipv4->GetAddress (i, 0), ft);
  /// NOTE: Add DeleteAllRoutesFromInterface for DVT over here
}
void 
RoutingProtocol::NotifyAddAddress (uint32_t i, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << " interface " << i << " address " << address);
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  if (!l3->IsUp (i))
    {
      return;
    }
  if (l3->GetNAddresses (i) == 1)
    {
      Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (iface);
      if (!socket)
        {
          if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
            {
              return;
            }
          // Create a socket to listen only on this interface
          Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (), 
                                                     UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvBsdv,this));
          socket->BindToNetDevice (l3->GetNetDevice (i));
          socket->Bind (InetSocketAddress (iface.GetLocal (), BSDVR_PORT));
          socket->SetAllowBroadcast (true);
          m_socketAddresses.insert (std::make_pair (socket, iface));
          
          /// NOTE: See if subnet broadcast socket required here
          // create also a subnet directed broadcast socket
          socket = Socket::CreateSocket (GetObject<Node> (),
                                         UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvBsdv, this));
          socket->BindToNetDevice (l3->GetNetDevice (i));
          socket->Bind (InetSocketAddress (iface.GetBroadcast (), BSDVR_PORT));
          socket->SetAllowBroadcast (true);
          socket->SetIpRecvTtl (true);
          m_socketSubnetBroadcastAddresses.insert (std::make_pair (socket, iface));

          // Add local broadcast record to the routing table
          Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
          RoutingTableEntry rt(/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*iface=*/ iface, 
                              /*hops=*/ 1, /*next hop=*/ iface.GetBroadcast (), /*changedEntries*/ false);
          std::map<Ipv4Address, ns3::bsdvr::RoutingTableEntry>* ft = m_routingTable.GetForwardingTable ();
          m_routingTable.AddRoute (rt, ft);

          ///NOTE: assuming this is the point a new connection is setup between two nodes to 
          ///      perform the initial exchange of distance vectors. (SYN + SYN-ACK)
          SendTriggeredUpdateToNeighbor (rt.GetDestination ());
        }
    }
  else
    {
      NS_LOG_LOGIC ("BSDVR does not work with more then one address per each interface. Ignore added address");
    }
}
void 
RoutingProtocol::NotifyRemoveAddress (uint32_t i, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (address);
  if (socket)
    {
      std::map<Ipv4Address, ns3::bsdvr::RoutingTableEntry>* ft = m_routingTable.GetForwardingTable ();
      m_routingTable.DeleteAllRoutesFromInterface (address, ft);

      /// NOTE: Add DeleteAllRoutesFromInterface for DVT over here

      socket->Close ();
      m_socketAddresses.erase (socket);

      /// NOTE: See if subnet broadcast socket required here
      Ptr<Socket> unicastSocket = FindSubnetBroadcastSocketWithInterfaceAddress (address);
      if (unicastSocket)
        {
          unicastSocket->Close ();
          m_socketAddresses.erase (unicastSocket);
        }

      Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
      if (l3->GetNAddresses (i))
        {
          Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);
          // Create a socket to listen only on this interface
          Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (), 
                                                     UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvBsdv, this));
          // Bind to any IP address so that broadcasts can be received
          socket->BindToNetDevice (l3->GetNetDevice (i));
          socket->Bind (InetSocketAddress (iface.GetLocal (), BSDVR_PORT));
          socket->SetAllowBroadcast (true);
          m_socketAddresses.insert (std::make_pair (socket,iface));
          
          /// NOTE: See if subnet broadcast socket required here
          socket = Socket::CreateSocket (GetObject<Node> (),
                                         UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvBsdv, this));
          socket->BindToNetDevice (l3->GetNetDevice (i));
          socket->Bind (InetSocketAddress (iface.GetBroadcast (), BSDVR_PORT));
          socket->SetAllowBroadcast (true);
          socket->SetIpRecvTtl (true);
          m_socketSubnetBroadcastAddresses.insert (std::make_pair (socket, iface));

          // Add local broadcast record to the routing table
          Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
          RoutingTableEntry rt(/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*iface=*/ iface, 
                              /*hops=*/ 1, /*next hop=*/ iface.GetBroadcast (), /*changedEntries*/ false);
          std::map<Ipv4Address, ns3::bsdvr::RoutingTableEntry>* ft = m_routingTable.GetForwardingTable ();
          m_routingTable.AddRoute (rt, ft);
        }
      if (m_socketAddresses.empty ())
        {
          NS_LOG_LOGIC ("No bsdvr interfaces");
          m_htimer.Cancel ();
          m_nb.Clear ();
          m_routingTable.Clear ();
          return;
        }
    }
  else
    {
      NS_LOG_LOGIC ("Remove address not participating in BSDVR operation");
    }
}
bool
RoutingProtocol::IsMyOwnAddress (Ipv4Address src)
{
  NS_LOG_FUNCTION (this << src);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv4InterfaceAddress iface = j->second;
      if (src == iface.GetLocal ())
        {
          return true;
        }
    }
  return false;
}
void
RoutingProtocol::HelloTimerExpire ()
{
  NS_LOG_FUNCTION (this);
  Time offset = Time (Seconds (0));
  if (m_lastBcastTime > Time (Seconds (0)))
    {
      offset = Simulator::Now () - m_lastBcastTime;
      NS_LOG_DEBUG ("Hello deferred due to last bcast at:" << m_lastBcastTime);
    }
  else
    {
      SendHello ();
    }
  m_htimer.Cancel ();
  Time diff = m_helloInterval - offset;
  m_htimer.Schedule (std::max (Time (Seconds (0)), diff));
  m_lastBcastTime = Time (Seconds (0));
}
Ptr<Socket> 
RoutingProtocol::FindSocketWithInterfaceAddress (Ipv4InterfaceAddress addr) const
{
  NS_LOG_FUNCTION (this << addr);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      if (iface == addr)
        {
          return socket;
        }
    }
  Ptr<Socket> socket;
  return socket;
}
Ptr<Socket>
RoutingProtocol::FindSubnetBroadcastSocketWithInterfaceAddress (Ipv4InterfaceAddress addr ) const
{
  NS_LOG_FUNCTION (this << addr);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketSubnetBroadcastAddresses.begin (); j != m_socketSubnetBroadcastAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      if (iface == addr)
        {
          return socket;
        }
    }
  Ptr<Socket> socket;
  return socket;
}

void 
RoutingProtocol::ProcessHello (HelloHeader const & hlHeader, Ipv4Address receiver)
{
  Ipv4Address origin =  hlHeader.GetOrigin ();
  NS_LOG_FUNCTION (this << "from " << origin);
  /*
   *  Whenever a node receives a Hello message from a neighbor, the node
   * SHOULD make sure that it has an active route to the neighbor, and
   * create one if necessary in dvt.
   */
  RoutingTableEntry toNeighbor;
  std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>* >* dvt = m_routingTable.GetDistanceVectorTable ();
  /// NOTE: use of find () instead of [] handles index errors better, change at end
  std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>* >::iterator dvt_iter;
  for (dvt_iter = dvt->begin (); dvt_iter != dvt->end(); ++dvt_iter)
    {
      if (dvt_iter->first == receiver)
        {
          break;
        }
    }
  std::map<Ipv4Address, RoutingTableEntry>* dv = new std::map<Ipv4Address, RoutingTableEntry> ();
  if (dvt_iter != dvt->end ())
    {
      dv = (*dvt)[receiver]; // receiver node's dv
    }
  if (!m_routingTable.LookupRoute (origin, toNeighbor, dv))
    {
      Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver));
      RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ origin, 
                                  /*iface=*/ m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (receiver), 0),
                                  /*hops=*/ 1, /*next hop=*/ origin, /*changedEntries*/ false);
      m_routingTable.AddRoute (newEntry, dv);
    }
  else
    {
      toNeighbor.SetOutputDevice (m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver)));
      toNeighbor.SetInterface (m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (receiver), 0));
      toNeighbor.SetHop (1);
      toNeighbor.SetNextHop (origin);
      m_routingTable.Update(toNeighbor, dv);
    }
  if (m_enableHello)
    {
      m_nb.Update (origin, Time (m_helloInterval));
    }
}
//-----------------------------------------------------------------------------

/*
 BSDVR Recv Functions
 */

void 
RoutingProtocol::RecvBsdv (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Address sourceAddress;
  Ptr<Packet> packet = socket->RecvFrom (sourceAddress);
  InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
  Ipv4Address sender = inetSourceAddr.GetIpv4 ();
  Ipv4Address receiver;
  if (m_socketAddresses.find (socket) != m_socketAddresses.end ())
    {
      receiver = m_socketAddresses[socket].GetLocal ();
    }
  else if (m_socketSubnetBroadcastAddresses.find (socket) != m_socketSubnetBroadcastAddresses.end ())
    {
      receiver = m_socketSubnetBroadcastAddresses[socket].GetLocal ();
    }
  else
    {
      NS_ASSERT_MSG (false, "Received a packet from an unknown socket");
    }
  uint32_t packetSize = packet->GetSize ();
  NS_LOG_DEBUG ("BSDVR node " << this << " received a BSDVR packet from " 
                              << sender << " to " << receiver
                              << " of size " << packetSize
                              << " and id " << packet->GetUid ());
  TypeHeader tHeader (BSDVRTYPE_UPDATE);
  packet->RemoveHeader (tHeader);
  if (!tHeader.IsValid ())
    {
      NS_LOG_DEBUG ("BSDVR message " << packet->GetUid () << " with unknown type received: " << tHeader.Get () << ". Drop");
      return; // drop
    }
  switch (tHeader.Get ())
    {
      case BSDVRTYPE_HELLO:
        {
          RecvHello (packet, receiver, sender);
          break;
        }
      case BSDVRTYPE_UPDATE:
        {
          RecvUpdate (packet, receiver, sender);
          break;
        }
    }
}
void 
RoutingProtocol::RecvHello (Ptr<Packet> p, Ipv4Address my, Ipv4Address src)
{
  NS_LOG_FUNCTION (this << " src " << src);
  HelloHeader hlHeader;
  p->RemoveHeader (hlHeader);
  NS_LOG_LOGIC ("HELLO destination " << my << " HELLO origin " << hlHeader.GetOrigin ());
  // Confirming HELLO message
  if (hlHeader.GetDst () == hlHeader.GetOrigin ())
  {
    ProcessHello (hlHeader, my);
  }
  return;
}
void 
RoutingProtocol::RecvUpdate (Ptr<Packet> p, Ipv4Address my, Ipv4Address src)
{
  NS_LOG_FUNCTION (this << " src " << src);
  UpdateHeader uptHeader;
  std::list<Ipv4Address> nex;
  p->RemoveHeader (uptHeader);
  Ipv4Address dst = uptHeader.GetDst ();
  NS_LOG_LOGIC ("UPDATE destination " << dst << " UPDATE origin " << uptHeader.GetOrigin ());
  uint8_t hop = uptHeader.GetHopCount () + 1;
  uptHeader.SetHopCount (hop);
  /*
   * If the route table entry to the destination is created or updated :
   * - the route is added/updated in the distance vector table <= UpdateDistanceVectorTable ()
   * - the best routes are computed and added/updated in the forwarding table <= ComputeForwardingTable ()
   * - the new changes in forwarding table are extracted and broadcasted to current neighbors <= BroadcastFTChanges ()
   * 
   * If UPDATE message is INACTIVE and not on primary path:
   * - initiate pending reply timer <= PendingReplyEnqueue ()
   */
  uint32_t state = uptHeader.GetBinaryState ();
  RouteState rs = (state == 1) ? ACTIVE : INACTIVE;
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (my));
  RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ dst, 
                        /*iface=*/ m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (my), 0),
                        /*hops=*/ hop, /*next hop=*/ src, /*changedEntries*/ false);
  rt.SetRouteState (rs);
  UpdateDistanceVectorTable (src, rt);
  std::list<Ipv4Address> changes = ComputeForwardingTable ();
  /// NOTE: Add Broadcast changes function here
  SendTriggeredUpdateChangesToNeighbors (changes, nex);
  // std::cout << changes.size () << std::endl;
  /// NOTE: Add Re-Transmit current entry function here
  /// NOTE: Send buffered packets
  std::map<Ipv4Address, RoutingTableEntry>::iterator ft_entry;
  std::map<Ipv4Address, RoutingTableEntry>* ft = m_routingTable.GetForwardingTable ();
  for (std::list<Ipv4Address>::const_iterator i = changes.begin (); i != changes.end (); ++i)
    {
      ft_entry = ft->find(*i);
      if (ft_entry != ft->end())
        {
          SendPacketFromQueue (*i, ft_entry->second.GetRoute ());
        }
    } 

}

//-----------------------------------------------------------------------------

/*
 BSDVR Send Functions
 */
void 
RoutingProtocol::SendHello ()
{
  NS_LOG_FUNCTION (this);
  /* Broadcast a Hello message with TTL = 1 */
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin ();
       j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      HelloHeader hlHeader (iface.GetLocal (), iface.GetLocal ());
      Ptr<Packet> packet = Create<Packet> ();
      SocketIpTtlTag tag;
      tag.SetTtl (1);
      packet->AddPacketTag (tag);
      packet->AddHeader (hlHeader);
      TypeHeader tHeader (BSDVRTYPE_HELLO);
      packet->AddHeader (tHeader);
      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ipv4Address destination;
      if (iface.GetMask () == Ipv4Mask::GetOnes ())
        {
          destination = Ipv4Address ("255.255.255.255");
        }
      else
        {
          /// NOTE: confirm the broadcast is working as intended
          destination = iface.GetBroadcast ();
        }
      Time jitter = Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10)));
      Simulator::Schedule (jitter, &RoutingProtocol::SendTo, this, socket, packet, destination);
    }
}
void 
RoutingProtocol::SendPacketFromQueue (Ipv4Address dst, Ptr<Ipv4Route> route)
{
  NS_LOG_FUNCTION (this);
  QueueEntry queueEntry;
  while (m_queue.Dequeue (dst, queueEntry))
    {
      DeferredRouteOutputTag tag;
      Ptr<Packet> p = ConstCast<Packet> (queueEntry.GetPacket ());
      if (p->RemovePacketTag (tag)
          && tag.GetInterface () != -1
          && tag.GetInterface () != m_ipv4->GetInterfaceForDevice (route->GetOutputDevice ()))
        {
          NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
          return;
        }
      UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback ();
      Ipv4Header header = queueEntry.GetIpv4Header ();
      header.SetSource (route->GetSource ());
      header.SetTtl (header.GetTtl () + 1); // compensate extra TTL decrement by fake loopback routing
      ucb (route, p, header);
    }
}
void 
RoutingProtocol::SendUpdate (/*ft entry=*/ RoutingTableEntry const & rt, /*neighbor*/Ipv4Address const & ne)
{
  NS_LOG_FUNCTION (this << rt.GetDestination ());
  ///NOTE: set packet header value over here
  u_int32_t hops = rt.GetHop ();
  Ipv4Address dst = rt.GetDestination ();
  Ipv4Address origin = rt.GetInterface ().GetLocal ();
  uint32_t state = (rt.GetRouteState () == ACTIVE) ? 1 : 0;

  UpdateHeader uptHeader (/*origin*/origin, /*dst*/dst, /*hops*/hops, /*state*/state);
  
  Ptr<Packet> packet =  Create<Packet> ();
  SocketIpTtlTag tag;
  tag.SetTtl (rt.GetHop ());
  packet->AddPacketTag (tag);
  packet->AddHeader (uptHeader);
  TypeHeader tHeader (BSDVRTYPE_UPDATE);
  packet->AddHeader (tHeader);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (rt.GetInterface ());
  NS_ASSERT (socket);
  socket->SendTo (packet, 0, InetSocketAddress (ne, BSDVR_PORT));
}
void 
RoutingProtocol::SendUpdateOnLinkFailure (Ipv4Address ne)
{
  std::list<Ipv4Address> nex;
  std::list<Ipv4Address> changes;
  std::map<Ipv4Address, RoutingTableEntry>::iterator n_dvt_entry;
  std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>*>::iterator n_dvt;
  std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>* > *dvt = m_routingTable.GetDistanceVectorTable ();
  for (n_dvt = dvt->begin (); n_dvt != dvt->end (); ++n_dvt)
    {
      if (n_dvt->first == ne)
        {
          break;
        }
    }
  if (n_dvt != dvt->end ())
    {
      n_dvt_entry = (*dvt)[ne]->find(ne);
      if (n_dvt_entry != (*dvt)[ne]->end ())
        {
          nex.push_back (ne);
          RoutingTableEntry rt = n_dvt_entry->second;
          rt.SetRouteState (INACTIVE);
          UpdateDistanceVectorTable (ne, rt);
          changes = ComputeForwardingTable ();
          SendTriggeredUpdateChangesToNeighbors (changes, nex);
        }
    }
}
void 
RoutingProtocol::SendTriggeredUpdateToNeighbor (Ipv4Address ne)
{
  std::map<Ipv4Address, RoutingTableEntry>* ft = m_routingTable.GetForwardingTable ();
  for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = ft->begin ();
       i != ft->end (); ++i)
    {
      if (i->first != m_mainAddress && i->first != ne)
        {
          SendUpdate (i->second, ne);
        }
    }
}
void 
RoutingProtocol::SendTriggeredUpdateChangesToNeighbors (std::list<Ipv4Address> changes, std::list<Ipv4Address> nex)
{
  std::list<Ipv4Address>::iterator n;
  std::map<Ipv4Address, RoutingTableEntry>::iterator ft_entry;
  std::vector<Neighbors::Neighbor> neighbors = m_nb.GetNeighbors ();
  std::map<Ipv4Address, RoutingTableEntry>* ft = m_routingTable.GetForwardingTable ();
  for (std::vector<Neighbors::Neighbor>::const_iterator i = neighbors.begin ();
       i != neighbors.end (); ++i)
    {
      Ipv4Address ne = i->m_neighborAddress;
      /// FIXME: Improve search in neighbor vector
      for (n = nex.begin (); n != nex.end (); ++n)
        {
          if (*n == ne)
            {
              break;
            }
        }
      if (n == nex.end ())
        {
          for (std::list<Ipv4Address>::const_iterator j = changes.begin ();
               j != changes.end (); ++j)
            {
              ft_entry = ft->find(*j);
              if (ft_entry != ft->end ())
                {
                  SendUpdate (ft_entry->second, ne);
                }
            }
        }
    }
}
void 
RoutingProtocol::SendTo (Ptr<Socket> socket, Ptr<Packet> packet, Ipv4Address destination)
{
  socket->SendTo (packet, 0, InetSocketAddress (destination, BSDVR_PORT)); 
}

//-----------------------------------------------------------------------------

/*
 BSDVR Control Plane Functions
 */

bool 
RoutingProtocol::isBetterRoute (RoutingTableEntry & r1, RoutingTableEntry & r2)
{
  u_int32_t new_hopCount = r2.GetHop ();
  u_int32_t curr_hopCount = r1.GetHop ();
  RouteState new_state = r2.GetRouteState ();
  RouteState curr_state  = r1.GetRouteState ();

  switch (new_state)
    {
    case ACTIVE:
      {
        if (curr_state == ACTIVE)
          {
            return (curr_hopCount <= new_hopCount) ? false : true; 
          }
        else if (curr_state == INACTIVE)
          {
            return (new_hopCount < bsdvr::constants::BSDVR_THRESHOLD) ? true : false;
          }
        break;
      }
    case INACTIVE:
      {
        if (curr_state == ACTIVE)
          {
            return (curr_hopCount <= bsdvr::constants::BSDVR_THRESHOLD) ? false : true; 
          }
        else if (curr_state == INACTIVE)
          {
            return (curr_hopCount <= new_hopCount) ? false : true;
          }
        break;
      }  
    }
   
  return false;
}

void 
RoutingProtocol::RemoveFakeRoutes (Ipv4Address nxtHp, RoutingTableEntry & rt)
{
  Ipv4Address curr_dst;
  Ipv4Address curr_nxtHp;
  RouteState curr_state;
  std::list<Ipv4Address> fake_dsts;
  Ipv4Address dst = rt.GetDestination ();
  /// FIXME: Make sure the getter returns a pointer to actual rtable to allow insert and removal of entries
  std::map<Ipv4Address, RoutingTableEntry> *ft = m_routingTable.GetForwardingTable ();
  for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = ft->begin (); i != ft->end (); i++)
    {
      curr_dst = i->first;
      curr_nxtHp = i->second.GetNextHop ();
      curr_state = i->second.GetRouteState ();
      if (curr_state == ACTIVE && rt.GetRouteState () == INACTIVE)
        {
          if (nxtHp == curr_nxtHp && dst == curr_dst)
            {
              fake_dsts.push_back (curr_dst);
            }
          // TODO: Confirm if neighbor check works right
          if (nxtHp == dst && m_nb.IsNeighbor (nxtHp))
            {
            if (curr_nxtHp == nxtHp && dst != curr_dst)
              {
                fake_dsts.push_back (curr_dst);
              }
            }
        }
    }
    //FIXME: Make sure the getter returns a pointer to actual rtable to allow insert and removal of entries
    std::vector<Neighbors::Neighbor> m_neighbors =  m_nb.GetNeighbors();
    std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>* > *dvt = m_routingTable.GetDistanceVectorTable ();
    for (std::vector<Neighbors::Neighbor>::iterator i = m_neighbors.begin ();
       i != m_neighbors.end (); ++i)
       {
         std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>* >::iterator n_dvt = dvt->find (i->m_neighborAddress);
         if (n_dvt != dvt->end ())
           {
             std::map<Ipv4Address, RoutingTableEntry>* n_dvt_entries = (*dvt)[i->m_neighborAddress];
             for (std::map<Ipv4Address, RoutingTableEntry>::iterator j = n_dvt_entries->begin (); j != n_dvt_entries->end (); j++)
                {
                    for (std::list<Ipv4Address>::iterator k = fake_dsts.begin (); k != fake_dsts.end (); k++)
                       {
                         if (*k != j->first)
                           {
                             curr_nxtHp = (*ft)[j->first].GetNextHop ();
                             if (i->m_neighborAddress != curr_nxtHp)
                               {
                                 (*dvt)[i->m_neighborAddress]->erase(j->first);
                               }
                           }
                       }
                }
           }
       }   
}

void 
RoutingProtocol::UpdateDistanceVectorTable (Ipv4Address nxtHp, RoutingTableEntry & rt)
{
  Ipv4Address curr_dst;
  Ipv4Address curr_nxtHp;
  Ipv4Address dst = rt.GetDestination ();
  // Tables
  std::vector<Neighbors::Neighbor> m_neighbors =  m_nb.GetNeighbors();
  std::map<Ipv4Address, RoutingTableEntry> *ft = m_routingTable.GetForwardingTable ();
  std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>* > *dvt = m_routingTable.GetDistanceVectorTable ();
  // Iterators
  std::vector<Neighbors::Neighbor>::iterator n;
  std::map<Ipv4Address, RoutingTableEntry>::iterator ft_entry;
  std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>* >::iterator n_dvt;

  ft_entry = ft->find (dst);
  if (ft_entry != ft->end ())
    {
      try
      {
        RemoveFakeRoutes (nxtHp, rt);
      }
      catch(const std::exception& e)
      {
        std::cerr << e.what() << '\n';
      }
      
    }
  /// FIXME: Improve search in neighbor vector
  for (n = m_neighbors.begin (); n != m_neighbors.end (); ++n)
    {
      if (n->m_neighborAddress == nxtHp)
        {
          break;
        }
    }
  n_dvt = (*dvt).find (nxtHp);
  if (n != m_neighbors.end () && n_dvt != (*dvt).end ())
    {
      /// NOTE: Assuming all neighbor hopCounts to be 1 so entries won't change will link quality
      // Do nothing
    }
  else
    {
      /// NOTE: As link quality is assumed constant, no total-cost calc. performed and
      //check against THRESHOLD value to skip total-cost calc.
      std::map<Ipv4Address, RoutingTableEntry>* n_dvt_entries = (*dvt)[nxtHp];
      (*n_dvt_entries)[dst] = rt;
    } 
}

void
RoutingProtocol::RefreshForwardingTable (Ipv4Address dst, Ipv4Address nxtHp)
{
  Ipv4Address curr_nxtHp;
  // Tables
  std::map<Ipv4Address, RoutingTableEntry> *ft = m_routingTable.GetForwardingTable ();
  std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>* > *dvt = m_routingTable.GetDistanceVectorTable ();
  // Iterators
  std::map<Ipv4Address, RoutingTableEntry>::iterator ft_entry;
  std::map<Ipv4Address, RoutingTableEntry>::iterator n_dvt_entry;
  std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>* >::iterator n_dvt;

  n_dvt = (*dvt).find (nxtHp);
  if (n_dvt != dvt->end ())
    {
      std::map<Ipv4Address, RoutingTableEntry>* n_dvt_entries = (*dvt)[nxtHp];
      n_dvt_entry = n_dvt_entries->find (dst);
      if (n_dvt_entry != n_dvt_entries->end ())
        {
          (*ft)[dst] = n_dvt_entry->second;
        }
    }
  else
    {
      (*ft)[dst].SetRouteState (INACTIVE);
    }
}

std::list<Ipv4Address> 
RoutingProtocol::ComputeForwardingTable ()
{
  Ipv4Address curr_nxtHp;
  RoutingTableEntry old_entry;
  RoutingTableEntry new_entry;
  RoutingTableEntry curr_entry;
  std::list<Ipv4Address> changes;
  // Tables
  std::vector<Neighbors::Neighbor> m_neighbors =  m_nb.GetNeighbors();
  std::map<Ipv4Address, RoutingTableEntry> *ft = m_routingTable.GetForwardingTable ();
  std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>* > *dvt = m_routingTable.GetDistanceVectorTable ();
  // Iterators
  std::list<Ipv4Address>::iterator c;
  std::vector<Neighbors::Neighbor>::iterator n;
  std::map<Ipv4Address, RoutingTableEntry>::iterator ft_entry;
  std::map<Ipv4Address, RoutingTableEntry>::iterator n_dvt_entry;

  for (std::vector<Neighbors::Neighbor>::iterator i = m_neighbors.begin ();
       i != m_neighbors.end (); ++i)
      {
        std::map<Ipv4Address, RoutingTableEntry>* n_dvt_entries = (*dvt)[i->m_neighborAddress];
        for (n_dvt_entry = n_dvt_entries->begin (); n_dvt_entry != n_dvt_entries->end (); n_dvt_entry++)
        {
          ft_entry = ft->find (n_dvt_entry->first);
          if (ft_entry != ft->end ())
            {
              try
              {
                curr_nxtHp = (*ft)[n_dvt_entry->first].GetNextHop ();
                old_entry = (*ft)[n_dvt_entry->first];
                RefreshForwardingTable (n_dvt_entry->first, curr_nxtHp);
                new_entry = (*(*dvt)[i->m_neighborAddress])[n_dvt_entry->first];
                curr_entry = (*ft)[n_dvt_entry->first];
                if (isBetterRoute (new_entry, curr_entry))
                  {
                    (*ft)[n_dvt_entry->first] = new_entry;
                    c = std::find(changes.begin (), changes.end (), n_dvt_entry->first);
                    if (c != changes.end ())
                      {
                        changes.push_back (n_dvt_entry->first);
                      }
                  }
                else if ((curr_entry.GetHop () != old_entry.GetHop ()) || (curr_entry.GetRouteState () != old_entry.GetRouteState ()))
                  {
                    c = std::find(changes.begin (), changes.end (), n_dvt_entry->first);
                    if (c != changes.end ())
                      {
                        changes.push_back (n_dvt_entry->first);
                      }
                  }
              }
              catch(const std::exception& e)
              {
                std::cerr << e.what() << '\n';
              }
              
            }
          else
            {
              new_entry = (*(*dvt)[i->m_neighborAddress])[n_dvt_entry->first];
              (*ft)[n_dvt_entry->first] = new_entry;
              changes.push_back (n_dvt_entry->first);
            }
        }
      }
  
  changes.remove (m_mainAddress);
  return changes;
}

}  // namespace bsdvr
}  // namespace ns3

