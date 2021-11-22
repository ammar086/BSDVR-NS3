/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef BSDVR_H
#define BSDVR_H

#include "bsdvr-constants.h"
#include "bsdvr-rtable.h"
#include "bsdvr-rqueue.h"
#include "bsdvr-packet.h"
#include "bsdvr-neighbor.h"
#include "ns3/node.h"
#include "ns3/random-variable-stream.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-l3-protocol.h"

namespace ns3 {

class WifiMacQueueItem;
enum WifiMacDropReason : uint8_t; // opaque enum declaration

namespace bsdvr {
/**
 * \ingroup bsdvr
 *
 * \brief BSDVR Routing Protocol
 */
class RoutingProtocol : public Ipv4RoutingProtocol
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  static const u_int32_t BSDVR_PORT;

  /// constructor
  RoutingProtocol ();
  virtual ~RoutingProtocol ();
  virtual void DoDispose ();

  // Inherited from Ipv4RoutingProtocol
  Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
  bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, 
                   MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;
  
  /**
   * Get the maximum queue length
   * \returns the maximum queue length
   */
  uint32_t GetMaxQueueLen () const
  {
    return m_maxQueueLen;
  }
  /**
   * Set the maximum queue length
   * \param len the maximum queue length
   */
  void SetMaxQueueLen (uint32_t len);
  /**
   * Set hello enable
   * \param f the hello enable flag
   */
  void SetHelloEnable (bool f)
  {
    m_enableHello = f;
  }
  /**
   * Get hello enable flag
   * \returns the enable hello flag
   */
  bool GetHelloEnable () const
  {
    return m_enableHello;
  }

  /**
   * Assign a fixed random variable stream number to the random variables
   * used by this model.  Return the number of streams (possibly zero) that
   * have been assigned.
   *
   * \param stream first stream index to use
   * \return the number of stream indices assigned by this model
   */
  int64_t AssignStreams (int64_t stream);

  bool isBetterRoute2 (RoutingTableEntry & r1, RoutingTableEntry & r2)
  {
    return isBetterRoute (r1,r2);
  }

  void RefreshForwardingTable2 (Ipv4Address dst, Ipv4Address nxtHp)
  {
    RefreshForwardingTable (dst, nxtHp);
  }

protected:
  virtual void DoInitialize (void);
private:
  /**
   * Notify that an MPDU was dropped.
   *
   * \param reason the reason why the MPDU was dropped
   * \param mpdu the dropped MPDU
   */
  void NotifyTxError (WifiMacDropReason reason, Ptr<const WifiMacQueueItem> mpdu);

  // Protocol parameters
  /// Nodes IP address
  /// NOTE: Verify if m_mainAddress is really required
  Ipv4Address m_mainAddress;
  /// IP protocol
  Ptr<Ipv4> m_ipv4;
  /// Raw unicast socket per each IP interface, map socket -> iface address (IP + mask)
  std::map< Ptr<Socket>, Ipv4InterfaceAddress > m_socketAddresses;
  /// Raw subnet directed broadcast socket per each IP interface, map socket -> iface address (IP + mask)
  //std::map< Ptr<Socket>, Ipv4InterfaceAddress > m_socketSubnetBroadcastAddresses;
  /// Loopback device used to defer route requests until a route is found
  Ptr<NetDevice> m_lo;
  /// Routing table
  RoutingTable m_routingTable;
  /// Indicates whether a hello messages enable
  bool m_enableHello;
  /**
   * Every HelloInterval the node checks whether it has sent a broadcast  within the last HelloInterval.
   * If it has not, it MAY broadcast a  Hello message
   */
  Time m_helloInterval;
  /// Handle neighbors
  Neighbors m_nb;
  /// The maximum number of packets that we allow a routing protocol to buffer
  uint32_t m_maxQueueLen;
  /// A "drop-front" queue used by the routing layer with binary state precedence rules to buffer packets to which it does not have a route.
  BsdvrQueue m_queue;

private:
  /// Start protocol operation
  void Start ();
  /**
   * Queue packet until we find a route
   * \param p the packet to route
   * \param header the Ipv4Header
   * \param ucb the UnicastForwardCallback function
   * \param ecb the ErrorCallback function
   */
  void DeferredRouteOutput (Ptr<const Packet> p, const Ipv4Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);
  /**
   * If route exists and is valid, forward packet.
   * \param p the packet to route
   * \param header the IP header
   * \param ucb the UnicastForwardCallback function
   * \param ecb the ErrorCallback function
   * \returns true if forwarded
   */ 
  bool Forwarding (Ptr<const Packet> p, const Ipv4Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);
  /**
   * Test whether the provided address is assigned to an interface on this node
   * \param src the source IP address
   * \returns true if the IP address is the node's IP address
   */
  bool IsMyOwnAddress (Ipv4Address src);
  /**
   * Find unicast socket with local interface address iface
   *
   * \param iface the interface
   * \returns the socket associated with the interface
   */
  Ptr<Socket> FindSocketWithInterfaceAddress (Ipv4InterfaceAddress iface) const;
  /**
   * Find subnet directed broadcast socket with local interface address iface
   *
   * \param iface the interface
   * \returns the socket associated with the interface
   */
  //Ptr<Socket> FindSubnetBroadcastSocketWithInterfaceAddress (Ipv4InterfaceAddress iface) const;
  /**
   * Process hello message
   * 
   * \param helloHeader RREP message header
   * \param receiverIfaceAddr receiver interface IP address
   */
  void ProcessHello (HelloHeader const & helloHeader, Ipv4Address receiverIfaceAddr);
  /**
   * Create loopback route for given header
   *
   * \param header the IP header
   * \param oif the device
   * \returns the route
   */
  Ptr<Ipv4Route> LoopbackRoute (const Ipv4Header & header, Ptr<NetDevice> oif) const;
  
  ///\name Receive control packets
  //\{
  /**
   * Receive and process control packet (Wrapper)
   * \param socket input socket
   */
  void RecvBsdv (Ptr<Socket> socket);
  /**
   * Receive Update
   * \param p packet
   * \param my destination address
   * \param src sender address
   */
  void RecvUpdate (Ptr<Packet> p, Ipv4Address my, Ipv4Address src);
  /**
   * Receive Hello
   * \param p packet
   * \param my destination address
   * \param src sender address
   */
  void RecvHello (Ptr<Packet> p, Ipv4Address my, Ipv4Address src);
  //\}

  ///\name Send
  //\{
  /** Forward packet from route request queue
   * \param dst destination address
   * \param route route to use
   */
  void SendPacketFromQueue (Ipv4Address dst, Ptr<Ipv4Route> route);    
  /// Send hello
  void SendHello ();
  /** Send Update
   * \param updtHeader route update header
   * \param toOrigin routing table entry to originator
   */
  void SendUpdate (UpdateHeader const & updtHeader, RoutingTableEntry const & toOrigin);
  /// @}
  
  /**
   * Send packet to destination socket (Wrapper)
   * \param socket - destination node socket
   * \param packet - packet to send
   * \param destination - destination node IP address
   */
  void SendTo (Ptr<Socket> socket, Ptr<Packet> packet, Ipv4Address destination);  

  /// Hello timer
  Timer m_htimer;
  /// Schedule next send of hello message
  void HelloTimerExpire ();
  /**
   * Find if a route to a destination is better than an alternative route
   * \param rt1 routing entry for a given destination
   * \param rt2 alternative routing entry for the same destination
   * \return true in success
   */

  /// BSDVR Control Plane Functions

  bool isBetterRoute (RoutingTableEntry & r1, RoutingTableEntry & r2);
  /**
   * Remove alternative routes from DVT to avoid fake routes - [doesnot remove direct neighbor routes]
   * \param nxtHp nexthop's address
   * \param rt  new entry with destination address dst
   */
  void RemoveFakeRoutes (Ipv4Address nxtHp, RoutingTableEntry & rt); 
  /**
   * Update existing routes in DVT or add new routes
   * \param nxtHp nexthop's address
   * \param rt  new entry with destination address dst
   */
  void UpdateDistanceVectorTable (Ipv4Address nxtHp, RoutingTableEntry & rt); 
  /**
   * Update changes in existing routes from updated DVT
   * \param dst destination address
   * \param nxtHp nexthop's address
   */
  void RefreshForwardingTable (Ipv4Address dst, Ipv4Address nxtHp);
   /**
   * Replace existing routes with by alternative routes from updated DVT if any
   * \returns a list of newly installed routes in FT to broadcast to neighbors
   */
  std::list<Ipv4Address> ComputeForwardingTable ();

  /// Provides uniform random variables.
  Ptr<UniformRandomVariable> m_uniformRandomVariable;

};
}  // namespace bsdvr
}  // namespace ns3

#endif /* BSDVR_H */

