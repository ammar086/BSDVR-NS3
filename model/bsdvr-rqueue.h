#ifndef BSDVR_RQUEUE_H
#define BSDVR_RQUEUE_H

#include <vector>
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/simulator.h"

namespace ns3 {
namespace bsdvr {

enum ForwardingStatus
{   
    BSDVRTYPE_NOT_FORWARDED  = 0,
    BSDVRTYPE_INACTIVE_FORWARDED  = 1,
    BSDVRTYPE_ACTIVE_FORWARDED  = 2
};

class Status
{
public:
  /**
    * constructor
    * \param s the packet forwarding status
    */
  Status (ForwardingStatus s = BSDVRTYPE_NOT_FORWARDED)
  {  
  }
  void Print (std::ostream &os) const;
  /**
   * \returns the status
   */
  ForwardingStatus Get ()
  {
    return p_status;
  }
  /**
   * \param s the new forwarding status
   */
  void Set (ForwardingStatus s)
  {
    p_status = s;
  }
  /**
   * Check that type if valid
   * \returns true if the type is valid
   */
  bool IsValid () const
  {
    return p_valid;
  }
  /**
   * \brief Comparison operator
   * \param s Status to compare against
   * \return true if the headers are equal
   */
  bool operator== (Status const & s) const
  {
    return (p_status == s.p_status && p_valid == s.p_valid);
  }
private:
  ForwardingStatus p_status;  /// forwarding status of the packet
  bool p_valid;  /// Indicates if the packet is valid
};

/**
  * \brief Stream output operator
  * \param os output stream
  * \return updated stream
  */
std::ostream & operator<< (std::ostream & os, Status const & s);

/**
 * \ingroup bsdvr
 * \brief BSDVR Pending Reply Entry
 */
class PendingReplyEntry
{
public:
  /**
   * constructor
   *
   * \param ne neighbor ip
   * \param dst destination ip
   * \param wait the pending time
   */
  PendingReplyEntry (Ipv4Address ne = Ipv4Address (), Ipv4Address dst = Ipv4Address (),
                     Time wait = Simulator::Now ())
    : m_ne (ne),
      m_dst (dst),
      m_time (wait + Simulator::Now ())
  {
  }
  /**
   * \brief Compare reply entries
   * \param o PendingReplyEntry to compare
   * \return true if equal
   */
  bool operator== (PendingReplyEntry const & o) const
  {
    return ((m_ne == o.m_ne) && (m_dst == o.m_dst) && (m_time == o.m_time));
  }
  // Fields
  /**
   * Get IPv4 address for neighbor
   * \returns the IPv4 address
   */
  Ipv4Address GetNeighbor () const
  {
    return m_ne;
  }
  /**
   * Set IPv4 address for neighbor
   * \param ip the IPv4 address
   */
  void SetNeighbor (Ipv4Address ip)
  {
    m_ne = ip;
  }
  /**
   * Get IPv4 address for destination
   * \returns the IPv4 address
   */
  Ipv4Address GetDestination () const
  {
    return m_dst;
  }
  /**
   * Set IPv4 address for destination
   * \param ip the IPv4 address
   */
  void SetDestination (Ipv4Address ip)
  {
    m_dst = ip;
  }
  /**
   * Set pending reply time
   * \param wait the pending time
   */
  void SetPendingTime (Time wait)
  {
    m_time = wait + Simulator::Now ();
  }
  /**
   * Get remaining pending reply timer
   * \returns the remaining pending time
   */
  Time GetPendingTime () const
  {
    return m_time - Simulator::Now ();
  }
private:
  /// Neighbor ip
  Ipv4Address m_ne;
  /// Destination ip
  Ipv4Address m_dst;
  /// Pending reply time
  Time m_time;
};
/**
 * \ingroup bsdvr
 * \brief BSDVR Queue Entry
 */
class QueueEntry
{
public:
  /// IPv4 routing unicast forward callback typedef
  typedef Ipv4RoutingProtocol::UnicastForwardCallback UnicastForwardCallback;
  /// IPv4 routing error callback typedef
  typedef Ipv4RoutingProtocol::ErrorCallback ErrorCallback;
  /**
   * constructor
   *
   * \param pa the packet to add to the queue
   * \param fs the forwarding state flag of the packet
   * \param h the Ipv4Header
   * \param ucb the UnicastForwardCallback function
   * \param ecb the ErrorCallback function
   * \param exp the expiration time
   */
  QueueEntry (Ptr<const Packet> pa = 0, Status fs = Status (), Ipv4Header const & h = Ipv4Header (),
              UnicastForwardCallback ucb = UnicastForwardCallback (), ErrorCallback ecb = ErrorCallback ())
    : m_packet (pa),
      m_status (fs),
      m_header (h),
      m_ucb (ucb),
      m_ecb (ecb)
  {
  }
  /**
   * \brief Compare queue entries
   * \param o QueueEntry to compare
   * \return true if equal
   */
  bool operator== (QueueEntry const & o) const
  {
    return ((m_packet == o.m_packet) && (m_status == o.m_status) && (m_header.GetDestination () == o.m_header.GetDestination ()));
  }
  // Fields
  /**
   * Get unicast forward callback function
   * \returns the unicast forward callback
   */
  UnicastForwardCallback GetUnicastForwardCallback () const
  {
    return m_ucb;
  }
  /**
   * Set unicast forward callback
   * \param ucb The unicast callback
   */
  void SetUnicastForwardCallback (UnicastForwardCallback ucb)
  {
    m_ucb = ucb;
  }
  /**
   * Get error callback
   * \returns the error callback
   */
  ErrorCallback GetErrorCallback () const
  {
    return m_ecb;
  }
  /**
   * Set error callback
   * \param ecb The error callback
   */
  void SetErrorCallback (ErrorCallback ecb)
  {
    m_ecb = ecb;
  }
  /**
   * Get packet from entry
   * \returns the packet
   */
  Ptr<const Packet> GetPacket () const
  {
    return m_packet;
  }
  /**
   * Set packet in entry
   * \param p The packet
   */
  void SetPacket (Ptr<const Packet> p)
  {
    m_packet = p;
  }
  /**
   * Get packet status
   * \returns the packet status
   */
  Status GetStatus () const
  {
    return m_status;
  }
  /**
   * Set packet status
   * \param s the new packet status
   */
  void SetStatus (Status s)
  {
    m_status = s;
  }
  /**
   * Get IPv4 header
   * \returns the IPv4 header
   */
  Ipv4Header GetIpv4Header () const
  {
    return m_header;
  }
  /**
   * Set IPv4 header
   * \param h the IPv4 header
   */
  void SetIpv4Header (Ipv4Header h)
  {
    m_header = h;
  }

private:
  /// Data packet
  Ptr<const Packet> m_packet;
  /// Data packet forwarding status
  Status m_status;
  /// IP header
  Ipv4Header m_header;
  /// Unicast forward callback
  UnicastForwardCallback m_ucb;
  /// Error callback
  ErrorCallback m_ecb;
};
/**
 * \ingroup bsdvr
 * \brief BSDVR Pending Reply Queue
 * A queue used by the routing layer to keep hold off sending UPDATE messages to restore lost
 * neighbor entries in order to avoid count-to-infinty loops setup by upstream node failures
 */
class BsdvrPendingReplyQueue
{
public:
  /**
   * constructor
   *
   * \param maxLen the maximum length
   * \param timeout the route to queue timeout
   */
  BsdvrPendingReplyQueue (uint32_t maxLen, Time timeout)
    : m_maxLen (maxLen),
      m_timeout (timeout)
  {
  }
  /**
   * Push entry in queue, if there is no entry with the same neighbor and destination address in queue.
   * \param pr_entry the queue entry
   * \returns true if the entry is queued
   */
  bool Enqueue (PendingReplyEntry & pr_entry);
  /**
   * Return first found (the oldest) entry for a given neighbor
   * 
   * \param ne the neighbor IP address
   * \param pr_entry the queue entry
   * \returns true if the entry is dequeued
   */
  bool Dequeue (Ipv4Address ne, PendingReplyEntry & pr_entry);
  /**
   * Remove all entries with neighbor IP address ne
   * \param ne the destination IP address
   */
  void DropEntryWithNeighbor (Ipv4Address ne);
  /**
   * Finds whether an entry with neighbor ne exists in the queue
   * 
   * \param ne the neighbor IP address
   * \returns true if an entry with the IP address is found
   */
  bool Find (Ipv4Address ne);
  /**
   * \returns the number of entries
   */
  uint32_t GetSize ();

  // Fields
  /**
   * Get maximum queue length
   * \returns the maximum queue length
   */
  uint32_t GetMaxQueueLen () const
  {
    return m_maxLen;
  }
  /**
   * Set maximum queue length
   * \param len The maximum queue length
   */
  void SetMaxQueueLen (uint32_t len)
  {
    m_maxLen = len;
  }
  /**
   * Get queue timeout
   * \returns the queue timeout
   */
  Time GetQueueTimeout () const
  {
    return m_timeout;
  }
  /**
   * Set queue timeout
   * \param t The queue timeout
   */
  void SetQueueTimeout (Time t)
  {
    m_timeout = t;
  }
  /**
   * Set entry timeout callback
   * \param cb the callback function
   */
  void SetCallback (Callback<void, PendingReplyEntry> cb)
  {
    m_handlePRTimeout = cb;
  }
  /**
   * Get entry timeout callback
   * \returns the callback
   */
  Callback<void, PendingReplyEntry> GetCallback () const
  {
    return m_handlePRTimeout;
  }

private:
  /// pending reply timeout callback
  Callback<void, PendingReplyEntry> m_handlePRTimeout;
  /// the queue
  std::vector<PendingReplyEntry> m_prqueue;
  /// send all expired entries and remove them from queue
  void Purge ();
  /**
   * Notify that entry is dropped from queue by timeout
   * \param en the queue entry to drop
   * \param reason the reason to drop the entry
   */
  void Drop (PendingReplyEntry en, std::string reason);
  /// the maximum number of pending reply entries allowed to be buffered
  uint32_t m_maxLen;
  /// the maximum period of time for which a pending reply can be buffered
  Time m_timeout;
};
/**
 * \ingroup bsdvr
 * \brief BSDVR Packet queue
 * A "drop-front" queue used by the routing layer with binary state precedence rules to 
 * buffer packets to which it does not have a route.
 */
class BsdvrQueue
{
public:
  /**
   * constructor
   *
   * \param maxLen the maximum length
   */
  BsdvrQueue (uint32_t maxLen)
    : m_maxLen (maxLen)
  {
  }
  /**
   * Push entry in queue, if there is no entry with the same packet and destination address in queue.
   * \param entry QueueEntry to compare
   * \return true if successful
   */
  bool Enqueue (QueueEntry & entry);
  /**
   * Return first found (the earliest) entry for given destination
   * 
   * \param dst the destination IP address
   * \param entry the queue entry
   * \returns true if the entry is dequeued
   */
  bool Dequeue (Ipv4Address dst, QueueEntry & entry, u_int32_t sval);
  /**
   * Remove all packets with destination IP address dst
   * \param dst the destination IP address
   */
  void DropPacketWithDst (Ipv4Address dst);
  /**
   * Finds whether a packet with destination dst exists in the queue
   * 
   * \param dst the destination IP address
   * \returns true if an entry with the IP address is found
   */
  bool Find (Ipv4Address dst);
  /**
   * \returns the number of entries
   */
  uint32_t GetSize ();
  /**
   * Get maximum queue length
   * \returns the maximum queue length
   */
  uint32_t GetMaxQueueLen () const
  {
    return m_maxLen;
  }
  /**
   * Set maximum queue length
   * \param len The maximum queue length
   */
  void SetMaxQueueLen (uint32_t len)
  {
    m_maxLen = len;
  }


private:
  /// The queue
  std::vector<QueueEntry> m_queue;
  /// The maximum number of packets that we allow a routing protocol to buffer.
  uint32_t m_maxLen;
  /**
   * Notify that packet is dropped from queue by timeout
   * \param en the queue entry to drop
   * \param reason the reason to drop the entry
   */
  void Drop (QueueEntry en, std::string reason);
  /**
   * Apply BSDVR precedence based drop policy
   * \param en retrieves the dropped entry
   */
  bool DropPolicy (QueueEntry &en);
};

}  // namespace bsdvr
}  // namespace ns3

#endif /* BSDVR_RQUEUE_H */