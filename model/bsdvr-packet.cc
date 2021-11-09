#include "ns3/packet.h"
#include "bsdvr-packet.h"
#include "ns3/address-utils.h"

namespace ns3{
namespace bsdvr{

NS_OBJECT_ENSURE_REGISTERED (TypeHeader);

TypeHeader::TypeHeader (MessageType t)
  : m_type (t),
    m_valid (true)
{
}

TypeId
TypeHeader::GetTypeId ()
{
  static TypeId tid = TypeId("ns3::bsdvr-ns3::TypeHeader")
    .SetParent<Header> ()
    .SetGroupName ("Bsdvr")
    .AddConstructor<TypeHeader> ()
  ;
  return tid;
}

TypeId
TypeHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
TypeHeader::GetSerializedSize () const
{
    return 1;
}

void
TypeHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU8 ((uint8_t) m_type);
}

uint32_t
TypeHeader::Deserialize (Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    uint8_t type = i.ReadU8 ();
    m_valid = true;
    switch (type)
      {
      case BSDVRTYPE_HELLO:
      case BSDVRTYPE_UPDATE:
        {
          m_type = (MessageType) type;
          break;
        }
      default:
        m_valid = false;
      }
    uint32_t dist = i.GetDistanceFrom (start);
    NS_ASSERT (dist == GetSerializedSize ());
    return dist;
}

void
TypeHeader::Print (std::ostream &os) const
{
    switch (m_type)
    {
    case BSDVRTYPE_HELLO:
      {
        os << "HELLO";
        break;
      }
    case BSDVRTYPE_UPDATE:
      {
        os << "UPDATE";
        break;
      }
    default:
      os << "UNKNOWN_TYPE";
    }
}

bool
TypeHeader::operator== (TypeHeader const & o) const
{
  return (m_type == o.m_type && m_valid == o.m_valid);
}

std::ostream &
operator<< (std::ostream & os, TypeHeader const & h)
{
  h.Print (os);
  return os;
}

//-----------------------------------------------------------------------------
// UPDATE
//-----------------------------------------------------------------------------

UpdateHeader::UpdateHeader (Ipv4Address origin, Ipv4Address dst, uint32_t hopcount, uint32_t state)
  : m_origin (origin),
    m_dst (dst),
    m_hopCount (hopcount),
    m_binaryState (state)
{
}

NS_OBJECT_ENSURE_REGISTERED (UpdateHeader);

TypeId
UpdateHeader::GetTypeId ()
{
  static TypeId tid = TypeId("ns3::bsdvr-ns3::UpdateHeader")
    .SetParent<Header> ()
    .SetGroupName ("Bsdvr")
    .AddConstructor<UpdateHeader> ()
  ;
  return tid;
}

TypeId
UpdateHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
UpdateHeader::GetSerializedSize () const
{
  return 16;
}

void
UpdateHeader::Serialize (Buffer::Iterator i) const
{
  WriteTo (i, m_origin);
  WriteTo (i, m_dst);
  i.WriteHtonU32 (m_hopCount);
  i.WriteHtonU32 (m_binaryState);
}

uint32_t
UpdateHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  ReadFrom (i, m_origin);
  ReadFrom (i, m_dst);
  m_hopCount = i.ReadNtohU32 ();
  m_binaryState = i.ReadNtohU32 ();

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
UpdateHeader::Print (std::ostream &os) const
{
  os << "SourceIpv4: " << m_origin
     << "DestinationIpv4: " << m_dst
     << "Hopcount: " << m_hopCount
     << "State: " << m_binaryState;
}

bool
UpdateHeader::operator== (UpdateHeader const & o) const
{
  return(m_origin == o.m_origin && m_dst == o.m_dst
         && m_hopCount == o.m_hopCount && m_binaryState == o.m_binaryState);
}

std::ostream &
operator<< (std::ostream & os, UpdateHeader const & h)
{
  h.Print (os);
  return os;
}

//-----------------------------------------------------------------------------
// HELLO
//-----------------------------------------------------------------------------

HelloHeader::HelloHeader (Ipv4Address origin, Ipv4Address dst)
  : m_origin (origin),
    m_dst (dst)
{
}

NS_OBJECT_ENSURE_REGISTERED (HelloHeader);

TypeId
HelloHeader::GetTypeId ()
{
  static TypeId tid = TypeId("ns3::bsdvr-ns3::HelloHeader")
    .SetParent<Header> ()
    .SetGroupName ("Bsdvr")
    .AddConstructor<HelloHeader> ()
  ;
  return tid;
}

TypeId
HelloHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
HelloHeader::GetSerializedSize () const
{
  return 8;
}

void
HelloHeader::Serialize (Buffer::Iterator i) const
{
  WriteTo (i, m_origin);
  WriteTo (i, m_dst);
}

uint32_t
HelloHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  ReadFrom (i, m_origin);
  ReadFrom (i, m_dst);

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
HelloHeader::Print (std::ostream &os) const
{
  os << "SourceIpv4: " << m_origin
     << "DestinationIpv4: " << m_dst;
}

bool
HelloHeader::operator== (HelloHeader const & o) const
{
  return(m_origin == o.m_origin && m_dst == o.m_dst);
}

std::ostream &
operator<< (std::ostream & os, HelloHeader const & h)
{
  h.Print (os);
  return os;
}

}  // namespace bsdvr
}  // namespace ns3