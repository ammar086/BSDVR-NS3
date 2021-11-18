#include "ns3/log.h"
#include <algorithm>
#include "bsdvr-rtable.h"
#include "ns3/simulator.h"

namespace ns3{

NS_LOG_COMPONENT_DEFINE ("BsdvrRoutingTable");

namespace bsdvr{

/*
 Routing Table Entry
 */
RoutingTableEntry::RoutingTableEntry (Ptr<NetDevice> dev, Ipv4Address dst, Ipv4InterfaceAddress iface, 
                                      uint32_t  hops, Ipv4Address nextHop, bool changedEntries)
  : m_hops (hops),
    m_iface (iface),
    m_state (ACTIVE),
    m_entriesChanged (changedEntries)
{
  m_ipv4Route = Create<Ipv4Route> ();
  m_ipv4Route->SetDestination (dst);
  m_ipv4Route->SetGateway (nextHop);
  m_ipv4Route->SetSource (m_iface.GetLocal ());
  m_ipv4Route->SetOutputDevice (dev);
}
RoutingTableEntry::~RoutingTableEntry ()
{ 
}
void
RoutingTableEntry::Print (Ptr<OutputStreamWrapper> stream, Time::Unit unit /* = Time::S */) const
{
  std::ostream* os = stream->GetStream ();
  // Copy the current ostream state
  std::ios oldState (nullptr);
  oldState.copyfmt (*os);

  *os << std::resetiosflags (std::ios::adjustfield) << std::setiosflags (std::ios::left);

  std::ostringstream dest, gw, iface;
  dest << m_ipv4Route->GetDestination ();
  gw << m_ipv4Route->GetGateway ();
  iface << m_iface.GetLocal ();

  *os << std::setw (16) << dest.str ();
  *os << std::setw (16) << gw.str ();
  *os << std::setw (16) << iface.str ();
  *os << std::setw (16);
  switch (m_state)
    {
    case ACTIVE:
      {
        *os << "ACTIVE";
        break;
      }
    case INACTIVE:
      {
        *os << "INACTIVE";
        break;
      }  
    }
  *os << std::setw (16) << m_hops << std::endl;
  // Restore the previous ostream state
  (*os).copyfmt (oldState);
}

/*
 Routing Table
 */
RoutingTable::RoutingTable ()
{
}
bool
RoutingTable::LookupRoute (Ipv4Address id, RoutingTableEntry & rt, std::map<Ipv4Address, RoutingTableEntry> & map)
{
  if (map.empty ())
    {
      NS_LOG_LOGIC ("Route to " << id << " not found; table is empty");
      return false;
    }
  std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = map.find (id);
  if (i == map.end ())
    {
      NS_LOG_LOGIC ("Route to " << id << " not found");
      return false;
    }
  rt = i->second;
  NS_LOG_LOGIC ("Route to " << id << " found");
  return true;
}
bool
RoutingTable::DeleteRoute (Ipv4Address dst, std::map<Ipv4Address, RoutingTableEntry> & map)
{
  if (map.erase (dst) != 0)
    {
      NS_LOG_LOGIC ("Route deletion to " << dst << " successful");
      return true;
    }
  NS_LOG_LOGIC ("Route deletion to " << dst << " not successful");
  return false;
}
bool
RoutingTable::AddRoute (RoutingTableEntry & rt, std::map<Ipv4Address, RoutingTableEntry> & map)
{
  NS_LOG_FUNCTION (this);
  std::pair<std::map<Ipv4Address, RoutingTableEntry>::iterator, bool> result =
    map.insert (std::make_pair (rt.GetDestination (), rt));
  return result.second;
}
bool
RoutingTable::Update (RoutingTableEntry & rt, std::map<Ipv4Address, RoutingTableEntry> & map)
{
  NS_LOG_FUNCTION (this);
  std::map<Ipv4Address, RoutingTableEntry>::iterator i = map.find(rt.GetDestination ());
  if (i == map.end ())
    {
      NS_LOG_LOGIC ("Route update to " << rt.GetDestination () << " fails; not found");
      return false;
    }
  i->second = rt;
  NS_LOG_LOGIC ("Route update to " << rt.GetDestination () << " passed");
  return true;
}
bool
RoutingTable::SetEntryState (Ipv4Address id, RouteState state, std::map<Ipv4Address, RoutingTableEntry> & map)
{
  NS_LOG_FUNCTION (this);
  std::map<Ipv4Address, RoutingTableEntry>::iterator i = map.find(id);
  if (i == map.end ())
    {
      NS_LOG_LOGIC ("Route set entry state to " << id << " fails; not found");
      return false;
    }
  i->second.SetRouteState (state);
  NS_LOG_LOGIC ("Route set entry state to " << id << ": new state is " << state);
  return true;
}
void
RoutingTable::DeleteAllRoutesFromInterface (Ipv4InterfaceAddress iface, std::map<Ipv4Address, RoutingTableEntry> & map)
{
  NS_LOG_FUNCTION (this);
  if (map.empty ())
    {
      return;
    }
  for (std::map<Ipv4Address, RoutingTableEntry>::iterator i = map.begin (); i != map.end ();)
    {
      if (i->second.GetInterface () == iface)
        {
          std::map<Ipv4Address, RoutingTableEntry>::iterator tmp = i;
          ++i;
          map.erase (tmp);
        }
      else
        {
          ++i;
        }
    }
}
void
RoutingTable::Print (std::map<Ipv4Address, RoutingTableEntry> & map, Ptr<OutputStreamWrapper> stream, Time::Unit unit /* = Time::S */) const
{
  std::ostream* os = stream->GetStream ();
  // Copy the current ostream state
  std::ios oldState (nullptr);
  oldState.copyfmt (*os);

  *os << std::resetiosflags (std::ios::adjustfield) << std::setiosflags (std::ios::left);
  
  *os << "\nBSDVR Routing table\n";
  *os << std::setw (16) << "Destination";
  *os << std::setw (16) << "Gateway";
  *os << std::setw (16) << "Interface";
  *os << std::setw (16) << "State";
  *os << std::setw (16) << "Hops" << std::endl;
  for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = map.begin (); i
       != map.end (); ++i)
    {
      i->second.Print (stream, unit);
    }
  *os << std::endl;
  // Restore the previous ostream state
  (*os).copyfmt (oldState);

}

}  // namespace bsdvr
}  // namespace ns3