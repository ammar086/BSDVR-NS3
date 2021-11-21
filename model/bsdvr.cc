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
  DeferredRouteOutputTag (int32_t o = -1) 
    : Tag (),
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
<<<<<<< Updated upstream

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

TypeId
RoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::bsdvr::RoutingProtocol");
  return tid;
}

RoutingProtocol::RoutingProtocol ()
  : m_helloInterval (Seconds (1)),
    m_nb (m_helloInterval),
    m_maxQueueLen (64),
    m_htimer (Timer::CANCEL_ON_DESTROY)
{
}

RoutingProtocol::~RoutingProtocol ()
{
}

void
RoutingProtocol::DoDispose ()
{
}

Ptr<Ipv4Route> 
RoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
  Ptr<Ipv4Route> route;
  return route;
}
bool 
RoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, 
                   MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
{
  return true;
}
void 
RoutingProtocol::NotifyInterfaceUp (uint32_t interface)
{
}
void 
RoutingProtocol::NotifyInterfaceDown (uint32_t interface)
{
}
void 
RoutingProtocol::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
}
void 
RoutingProtocol::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
}
void 
RoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
}
void 
RoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
}
void
RoutingProtocol::DoInitialize (void)
{
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
  //FIXME: Make sure the getter returns a pointer to actual rtable to allow insert and removal of entries
  std::map<Ipv4Address, RoutingTableEntry> ft = m_routingTable.GetForwardingTable();
  for (std::map<Ipv4Address, RoutingTableEntry>::iterator i = ft.begin (); i != ft.end (); i++)
    {
      curr_dst = i->first;
      curr_nxtHp = i->second.GetNextHop ();
      curr_state = i->second.GetRouteState ();
      if (curr_state == ACTIVE && rt.GetRouteState () == INACTIVE)
        {
          if (nxtHp == curr_nxtHp && dst == curr_dst)
            {
              fake_dsts.push_back(curr_dst);
            }
          // TODO: Confirm if neighbor check works right
          // if (nxtHp == dst && bsdvr::Neighbors::IsNeighbor (dst)){
        }
    }
}

void 
RoutingProtocol::UpdateDistanceVectorTable (Ipv4Address nxtHp, RoutingTableEntry & rt){}
=======

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

TypeId
RoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::bsdvr::RoutingProtocol");
  return tid;
}

RoutingProtocol::RoutingProtocol ()
  : m_helloInterval (Seconds (1)),
    m_nb (m_helloInterval),
    m_maxQueueLen (64),
    m_htimer (Timer::CANCEL_ON_DESTROY)
{
}

RoutingProtocol::~RoutingProtocol ()
{
}

void
RoutingProtocol::DoDispose ()
{
}

Ptr<Ipv4Route> 
RoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
  Ptr<Ipv4Route> route;
  return route;
}
bool 
RoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, 
                   MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
{
  return true;
}
void 
RoutingProtocol::NotifyInterfaceUp (uint32_t interface)
{
}
void 
RoutingProtocol::NotifyInterfaceDown (uint32_t interface)
{
}
void 
RoutingProtocol::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
}
void 
RoutingProtocol::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
}
void 
RoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
}
void 
RoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
}
void
RoutingProtocol::DoInitialize (void)
{
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
  //FIXME: Make sure the getter returns a pointer to actual rtable to allow insert and removal of entries
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
  //FIXME: Improve search in neighbor vector
  for (n = m_neighbors.begin (); n != m_neighbors.end (); n++)
    {
      if (n->m_neighborAddress == nxtHp)
        {
          break;
        }
    }
  n_dvt = (*dvt).find (nxtHp);
  if (n != m_neighbors.end () && n_dvt != (*dvt).end ())
    {
      //NOTE: Assuming all neighbor hopCounts to be 1 so entries won't change will link quality
      // Do nothing
    }
  else
    {
      //NOTE: As link quality is assumed constant, no total-cost calc. performed and
      //check against THRESHOLD value to skip total-cost calc.
      std::map<Ipv4Address, RoutingTableEntry>* n_dvt_entries = (*dvt)[nxtHp];
      (*n_dvt_entries)[dst] = rt;
    } 
  

}
>>>>>>> Stashed changes

void
RoutingProtocol::RefreshForwardingTable (Ipv4Address dst, Ipv4Address nxtHp){}

std::list<RoutingTableEntry> 
RoutingProtocol::ComputeForwardingTable ()
{
  std::list<RoutingTableEntry> changes;
  return changes;
}

}  // namespace bsdvr
}  // namespace ns3

