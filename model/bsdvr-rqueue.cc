#include "bsdvr-rqueue.h"
#include <algorithm>
#include <functional>
#include "ns3/ipv4-route.h"
#include "ns3/socket.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BsdvrRequestQueue");

namespace bsdvr {

//-----------------------------------------------------------------------------

/*
 BSDVR Pending Reply Queue Functions
 */

u_int32_t
BsdvrPendingReplyQueue::GetSize ()
{
  return m_prqueue.size ();
}

bool
BsdvrPendingReplyQueue::Enqueue (PendingReplyEntry & pr_entry)
{
  NS_LOG_FUNCTION ("Enqueing entry for " << pr_entry.GetNeighbor () << "for destination " << pr_entry.GetDestination ());
  Purge ();
  for (std::vector<PendingReplyEntry>::const_iterator i = m_prqueue.begin (); 
       i != m_prqueue.end (); ++i)
    {
      if (i->GetNeighbor () == pr_entry.GetNeighbor ()
          && i->GetDestination() == pr_entry.GetDestination ())
        {
          return false;
        }
    }
  pr_entry.SetPendingTime (m_timeout);
  if (m_prqueue.size () == m_maxLen)
    {
      Drop (m_prqueue.front (), "Drop the most aged entry"); // Drop the most aged entry
      m_prqueue.erase (m_prqueue.begin ());
    }
  m_prqueue.push_back(pr_entry);
  return true;
}
void
BsdvrPendingReplyQueue::DropEntryWithNeighbor (Ipv4Address ne)
{
  NS_LOG_FUNCTION (this << ne);
  /// FIXME: purge call removed here due to presence of callback
  //Purge ();
  for (std::vector<PendingReplyEntry>::iterator i = m_prqueue.begin ();
       i != m_prqueue.end (); ++i)
    {
      if (i->GetNeighbor () == ne)
        {
          Drop (*i, "Droppoing entries for given neighbor");
        }
    }
  auto new_end = std::remove_if (m_prqueue.begin (), m_prqueue.end (),
  [&](const PendingReplyEntry& en) {return en.GetNeighbor () == ne; });
  m_prqueue.erase (new_end, m_prqueue.end ());
}
bool
BsdvrPendingReplyQueue::Dequeue (Ipv4Address ne, PendingReplyEntry & pr_entry)
{
  /// FIXME: purge call removed here due to presence of callback
  //Purge ();
  for (std::vector<PendingReplyEntry>::iterator i = m_prqueue.begin ();
       i != m_prqueue.end (); ++i)
    {
      if (i->GetNeighbor () == ne)
        {
          pr_entry = *i;
          m_prqueue.erase (i);
          return true;
        }
    }
  return false;
}
bool
BsdvrPendingReplyQueue::Find (Ipv4Address ne)
{
  for (std::vector<PendingReplyEntry>::const_iterator i = m_prqueue.begin ();
       i != m_prqueue.end (); ++i)
    {
      if (i->GetNeighbor () == ne)
        {
          return true;
        }
    }
  return false;
}

/**
 * \brief IsExpired structure
 */
struct IsExpired
{
  bool
  /**
   * Check if the entry is expired
   *
   * \param p PendingReplyEntry entry
   * \return true if expired, false otherwise
   */
  operator() (PendingReplyEntry const & p) const
  {
    return (p.GetPendingTime () < Seconds (0));
  }
};
void
BsdvrPendingReplyQueue::Purge ()
{
  if (m_prqueue.empty ())
    {
      return;
    }

  IsExpired pred;
  if (!m_handlePRTimeout.IsNull ())
  {
    for (std::vector<PendingReplyEntry>::iterator i = m_prqueue.begin (); 
       i != m_prqueue.end (); ++i)
      {
        if (pred (*i))
          {
            Drop (*i, "Pending reply entry timer expired ");
            m_handlePRTimeout (*i);
          }
      }
  }
  m_prqueue.erase (std::remove_if (m_prqueue.begin (), m_prqueue.end (), pred), m_prqueue.end ());
}
void
BsdvrPendingReplyQueue::Drop (PendingReplyEntry en, std::string reason)
{
  NS_LOG_LOGIC (reason << en.GetNeighbor () << " " << en.GetDestination ());
  return;
}

//-----------------------------------------------------------------------------

/*
 BSDVR Queue Functions
 */

uint32_t
BsdvrQueue::GetSize ()
{
  return m_queue.size ();
}

bool
BsdvrQueue::Enqueue (QueueEntry & entry)
{
  NS_LOG_FUNCTION ("Enqueing packet destined for" << entry.GetIpv4Header ().GetDestination ());
  for (std::vector<QueueEntry>::const_iterator i = m_queue.begin (); i 
       != m_queue.end (); i++)
  {
    if ((i->GetPacket ()->GetUid () == entry.GetPacket ()->GetUid ())
        && (i->GetIpv4Header ().GetDestination ()
            == entry.GetIpv4Header ().GetDestination ()))
      {
        return false;
      }
  }
  if (m_queue.size () == m_maxLen)
    {
      QueueEntry drp;
      if (DropPolicy (drp))
        {
          Drop (drp, "Drop the least priority packet");
        }
      else
        {
          return false;
        }
    }
  m_queue.push_back (entry);
  return true;
}
bool
BsdvrQueue::Dequeue (Ipv4Address dst, QueueEntry &entry, u_int32_t sval)
{
  for (std::vector<QueueEntry>::iterator i = m_queue.begin (); i 
       != m_queue.end (); i++)
    {
      // if (i->GetIpv4Header ().GetDestination () == dst)
      //   {
      //     entry = *i;
      //     m_queue.erase (i);
      //     return true;
      //   }
      if (sval == 2) // Active Route
        {
          if (i->GetIpv4Header ().GetDestination () == dst)
            {
              if (i->GetStatus () == BSDVRTYPE_NOT_FORWARDED || 
                  i->GetStatus () == BSDVRTYPE_INACTIVE_FORWARDED)
                {
                  i->SetStatus (BSDVRTYPE_ACTIVE_FORWARDED);
                  entry = *i;
                  return true;
                }
            }
        }
      else if (sval == 1) // Inactive Route
        {
          if (i->GetIpv4Header ().GetDestination () == dst && i->GetStatus () == BSDVRTYPE_NOT_FORWARDED)
            {
              i->SetStatus (BSDVRTYPE_INACTIVE_FORWARDED);
              entry = *i;
              return true;
            }
        }
    }
  return false;
}
void
BsdvrQueue::DropPacketWithDst (Ipv4Address dst)
{
  NS_LOG_FUNCTION ("Dropping packet to " << dst);
  for (std::vector<QueueEntry>::const_iterator i = m_queue.begin (); i 
       != m_queue.end (); i++)
    {
      if (i->GetIpv4Header ().GetDestination () == dst)
        {
          Drop(*i, "DropPacketWithDst");
        }
    }
  auto new_end = std::remove_if (m_queue.begin (), m_queue.end (), [&](const QueueEntry& en)
                                { return en.GetIpv4Header ().GetDestination () == dst;});
  m_queue.erase (new_end, m_queue.end ());
}
bool
BsdvrQueue::Find (Ipv4Address dst)
{
  for (std::vector<QueueEntry>::const_iterator i = m_queue.begin (); i 
       != m_queue.end (); i++)
    {
      if (i->GetIpv4Header ().GetDestination () == dst)
        {
          NS_LOG_DEBUG ("Find");
          return true;
        }
    }
  return false;
}
///Note: Add IsExpired structure here for pending reply queue
void
BsdvrQueue::Drop (QueueEntry en, std::string reason)
{
  NS_LOG_LOGIC (reason << en.GetPacket ()->GetUid () << " " << en.GetIpv4Header ().GetDestination ());
  return;
}
bool
BsdvrQueue::DropPolicy (QueueEntry &en)
{
  ///TODO: Add drop policy function over here
  std::vector<QueueEntry>::iterator onf = m_queue.end ();  //oldest not forwarded
  std::vector<QueueEntry>::iterator oaf = m_queue.end ();  //oldest active forwarded
  std::vector<QueueEntry>::iterator oif = m_queue.end ();  //oldest inactive forwarded
  for (std::vector<QueueEntry>::iterator i = m_queue.begin (); i
       != m_queue.end (); ++i)
    {
      if (i->GetStatus () == BSDVRTYPE_NOT_FORWARDED && onf == m_queue.end ())
        {
          onf = i;
        }
      else if (i->GetStatus () == BSDVRTYPE_INACTIVE_FORWARDED && oif == m_queue.end ())
        {
          oif = i;
        }
      else if (i->GetStatus () == BSDVRTYPE_ACTIVE_FORWARDED && oaf == m_queue.end ())
        {
          oaf = i;
        }
    }
    if (oaf != m_queue.end ()) // active forwarded first
      {
        m_queue.erase (oaf);
        return true;
      }
    else if (oif != m_queue.end ()) // inactive forwarded second
      {
        m_queue.erase (oif);
        return true;
      }
    else if (onf != m_queue.end ()) // not forwarded last
      {
        m_queue.erase (onf);
        return true;
      }
  return false; 
}

}  // namespace bsdvr
}  // namespace ns3