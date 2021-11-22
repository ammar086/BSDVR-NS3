#ifndef BSDVR_RTABLE_H
#define BSDVR_RTABLE_H

#include <map>
#include <cassert>
#include <stdint.h>
#include "ns3/ipv4.h"
#include "ns3/timer.h"
#include <sys/types.h>
#include "ns3/ipv4-route.h"
#include "ns3/net-device.h"
#include "ns3/output-stream-wrapper.h"

namespace ns3 {
namespace bsdvr {

/**
 * \ingroup bsdvr
 * \brief Route record states
 */
enum RouteState
{
  INACTIVE = 0,      //!< INACTIVE
  ACTIVE = 1,        //!< ACTIVE
};

/**
 * \ingroup bsdvr
 * \brief Routing table entry
 */
class RoutingTableEntry
{
public:
  /**
   * constructor
   *
   * \param dev the net device
   * \param dst the destination IP address
   * \param iface the interface
   * \param hops the number of hops
   * \param nextHop the IP address of the next hop
   * \param state the binary state of the entry
   * \param changedEntries flag for changed entries
   */
  RoutingTableEntry (Ptr<NetDevice> dev = 0,Ipv4Address dst = Ipv4Address (), Ipv4InterfaceAddress iface = Ipv4InterfaceAddress (), 
                     uint32_t  hops = 0, Ipv4Address nextHop = Ipv4Address (), bool changedEntries = false);

  ~RoutingTableEntry ();

  // Fields
  /**
   * Get destination address function
   * \returns the IPv4 destination address
   */
  Ipv4Address GetDestination () const
  {
    return m_ipv4Route->GetDestination ();
  }
  /**
   * Get route function
   * \returns The IPv4 route
   */
  Ptr<Ipv4Route> GetRoute () const
  {
    return m_ipv4Route;
  }
  /**
   * Set route function
   * \param route the IPv4 route
   */
  void SetRoute (Ptr<Ipv4Route> route)
  {
    m_ipv4Route = route;
  }
  /**
   * Set next hop address
   * \param nextHop the next hop IPv4 address
   */
  void SetNextHop (Ipv4Address nextHop)
  {
    m_ipv4Route->SetGateway (nextHop);
  }
  /**
   * Get next hop address
   * \returns the next hop address
   */
  Ipv4Address GetNextHop () const
  {
    return m_ipv4Route->GetGateway ();
  }
  /**
   * Set output device
   * \param dev The output device
   */
  void SetOutputDevice (Ptr<NetDevice> dev)
  {
    m_ipv4Route->SetOutputDevice (dev);
  }
  /**
   * Get output device
   * \returns the output device
   */
  Ptr<NetDevice> GetOutputDevice () const
  {
    return m_ipv4Route->GetOutputDevice ();
  }
  /**
   * Get the Ipv4InterfaceAddress
   * \returns the Ipv4InterfaceAddress
   */
  Ipv4InterfaceAddress GetInterface () const
  {
    return m_iface;
  }
  /**
   * Set the Ipv4InterfaceAddress
   * \param iface The Ipv4InterfaceAddress
   */
  void SetInterface (Ipv4InterfaceAddress iface)
  {
    m_iface = iface;
  }
  /**
   * Set the number of hops
   * \param hop the number of hops
   */
  void SetHop (uint32_t hop)
  {
    m_hops = hop;
  }
  /**
   * Get the number of hops
   * \returns the number of hops
   */
  uint32_t GetHop () const
  {
    return m_hops;
  }
  /**
   * Set the route binary state
   * \param state the route state
   */
  void SetRouteState (RouteState state)
  {
    m_state = state;
  }
  /**
   * Get the route binary state
   * \returns the route state
   */
  RouteState GetRouteState () const
  {
    return m_state;
  }
  /**
   * Set entries changed indicator
   * \param entriesChanged
   */
  void SetEntriesChanged (bool entriesChanged)
  {
    m_entriesChanged = entriesChanged;
  }
  /**
   * Get entries changed
   * \returns the entries changed flag
   */
  bool GetEntriesChanged () const
  {
    return m_entriesChanged;
  }
  /**
   * \brief Compare destination address
   * \param dst IP address to compare
   * \return true if equal
   */
  bool operator== (Ipv4Address const  dst) const
  {
    return (m_ipv4Route->GetDestination () == dst);
  }
  /**
   * Print routing table entry
   * \param stream the output stream
   */
  void Print (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;

private:
  /// Hop Count (number of hops needed to reach destination)
  uint32_t m_hops;
  /** Ip route, include
   *   - destination address
   *   - source address
   *   - next hop address (gateway)
   *   - output device
   */
  Ptr<Ipv4Route> m_ipv4Route;
  /// Output interface address
  Ipv4InterfaceAddress m_iface;
  /// Routing state: active or inactive
  RouteState m_state;
  /// Flag to show if any of the routing table entries were changed with the routing update.
  bool m_entriesChanged;
};

/**
 * \ingroup bsdvr
 * \brief The Routing table used by BSDVR protocol
 */
class RoutingTable
{
public:
  RoutingTable ();
  /**
   * Get forwarding table
   * \returns the forwarding table
   */
  std::map<Ipv4Address, RoutingTableEntry> GetForwardingTablePrint () const
  {
    return &m_ForwardingTable;
  }
  /**
   * Get forwarding table
   * \returns the forwarding table
   */
  std::map<Ipv4Address, RoutingTableEntry>* GetForwardingTable ()
  {
    return &m_ForwardingTable;
  }
  /**
   * Get forwarding table
   * \returns the forwarding table
   */
  std::map<Ipv4Address, RoutingTableEntry>* GetForwardingTable ()
  {
    return &m_ForwardingTable;
  }
  /**
   * Get forwarding table
   * \returns the forwarding table
   */
  std::map<Ipv4Address, RoutingTableEntry>* GetForwardingTable ()
  {
    return &m_ForwardingTable;
  }
  /**
   * Get forwarding table
   * \returns the forwarding table
   */
  std::map<Ipv4Address, RoutingTableEntry>* GetForwardingTable ()
  {
    return &m_ForwardingTable;
  }
  /**
   * Get distance vector table
   * \returns the distance vector table
   */
  std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>* >* GetDistanceVectorTable ()
  {
    return &m_DistanceVectorTable;
  }
  /**
   * Add routing table entry if it doesn't yet exist in routing table
   * \param r routing table entry
   * \param map Ipv4 address to entry map
   * \return true in success
   */
  bool AddRoute (RoutingTableEntry & r, std::map<Ipv4Address, RoutingTableEntry>* map);
  /**
   * Delete routing table entry with destination address dst, if it exists.
   * \param dst destination address
   * \param map Ipv4 address to entry map
   * \return true on success
   */
  bool DeleteRoute (Ipv4Address dst, std::map<Ipv4Address, RoutingTableEntry> & map);
  /**
   * Lookup routing table entry with destination address dst
   * \param dst destination address
   * \param rt entry with destination address dst, if exists
   * \param map Ipv4 address to entry map
   * \return true on success
   */
  bool LookupRoute (Ipv4Address dst, RoutingTableEntry & rt, std::map<Ipv4Address, RoutingTableEntry>* map);
  /**
   * Updating the routing Table with routing table entry rt
   * \param rt routing table entry
   * \param map Ipv4 address to entry map
   * \return true on success
   */
  bool Update (RoutingTableEntry & rt, std::map<Ipv4Address, RoutingTableEntry> & map);
  /**
   * Set routing table entry flags
   * \param dst destination address
   * \param state the routing flags
   * \param map Ipv4 address to entry map
   * \return true on success
   */
  bool SetEntryState (Ipv4Address dst, RouteState state, std::map<Ipv4Address, RoutingTableEntry> & map);
  /**
   * Delete all route from interface with address iface
   * \param iface the interface
   * \param map Ipv4 address to entry map
   */
  void DeleteAllRoutesFromInterface (Ipv4InterfaceAddress iface, std::map<Ipv4Address, RoutingTableEntry> & map);
  /// Delete all entries from routing table
  void Clear () 
  { 
    m_ForwardingTable.clear (); 
  }
  /**
   * Print routing table
   * \param stream the output stream
   * \param unit The time unit to use (default Time::S)
   * \param map Ipv4 address to entry map
   */
  void Print (std::map<Ipv4Address, RoutingTableEntry>* map, Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;

private:
  /// The forwarding table (main routing table)
  std::map<Ipv4Address, RoutingTableEntry> m_ForwardingTable;
  /// The distance vector table (alternative entries)
  std::map<Ipv4Address, std::map<Ipv4Address, RoutingTableEntry>* > m_DistanceVectorTable;
};

}  // namespace bsdvr
}  // namespace ns3

#endif /* BSDVR_RTABLE_H */