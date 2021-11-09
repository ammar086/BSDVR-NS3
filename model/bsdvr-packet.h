#ifndef BSDVRPACKET_H
#define BSDVRPACKET_H

#include <map>
#include <iostream>
#include "ns3/enum.h"
#include "ns3/header.h"
#include "ns3/nstime.h"
#include "ns3/ipv4-address.h"

namespace ns3{
namespace bsdvr{

// use hop-count as the metric for Bellman Ford
// Assume out of order delivery unlikely due to discrete even simulation
// Fixed sized packets sending DV one at a time initially and during updates

// LT -> DV -> FT
enum MessageType
{   
    BSDVRTYPE_HELLO  = 1,
    BSDVRTYPE_UPDATE = 2
};
/**
* \ingroup bsdvr
* \brief BSDVR types
*/

class TypeHeader : public Header
{
public:
  /**
    * constructor
    * \param t the BSDVR UPDATE type
    */
  TypeHeader (MessageType t = BSDVRTYPE_UPDATE);

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  /**
   * \returns the type
   */
  MessageType Get () const
  {
    return m_type;
  }
  /**
   * Check that type if valid
   * \returns true if the type is valid
   */
  bool IsValid () const
  {
    return m_valid;
  }
  /**
   * \brief Comparison operator
   * \param o header to compare
   * \return true if the headers are equal
   */
  bool operator== (TypeHeader const & o) const;
private:
  MessageType m_type; ///< type of the message
  bool m_valid; ///< Indicates if the message is valid
};

/**
  * \brief Stream output operator
  * \param os output stream
  * \return updated stream
  */
std::ostream & operator<< (std::ostream & os, TypeHeader const & h);

/**
 * \ingroup bsdvr
 * \brief BSDVR Update Message Format
 * \verbatim
 |      0        |      1        |      2        |       3       |
  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                      Originator Address                       |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                      Destination Address                      |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           HopCount                            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                            State                              |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 */
class UpdateHeader : public Header
{
public:
  /**
   * constructor
   *
   * \param origin the origin IP address
   * \param dst the destination IP address
   * \param hopCount the hop count
   * \param state the binary state of the route
   */
  UpdateHeader (Ipv4Address origin = Ipv4Address (), Ipv4Address dst = Ipv4Address (), uint32_t hopcount = 0, uint32_t state = 0);
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  // Fields
  /**
   * \brief Set the origin address
   * \param a the origin address
   */
  void SetOrigin (Ipv4Address a)
  {
    m_origin = a;
  }
  /**
   * \brief Get the origin address
   * \return the origin address
   */
  Ipv4Address GetOrigin () const
  {
    return m_origin;
  }
  /**
   * \brief Set the destination address
   * \param a the destination address
   */
  void SetDst (Ipv4Address a)
  {
    m_dst = a;
  }
  /**
   * \brief Get the destination address
   * \return the destination address
   */
  Ipv4Address GetDst () const
  {
    return m_dst;
  }
  /**
   * \brief Set the hop count
   * \param count the hop count
   */
  void SetHopCount (uint32_t count)
  {
    m_hopCount = count;
  }
  /**
   * \brief Get the hop count
   * \return the hop count
   */
  uint32_t GetHopCount () const
  {
    return m_hopCount;
  }
  /**
   * \brief Set the binary state
   * \param s the binary state
   */
  void SetBinaryState (uint32_t s)
  {
    m_binaryState = s;
  }
  /**
   * \brief Get the hop count
   * \return the hop count
   */
  uint32_t GetBinaryState () const
  {
    return m_binaryState;
  }
  /**
   * \brief Comparison operator
   * \param o UPDATE header to compare
   * \return true if the UPDATE headers are equal
   */
  bool operator== (UpdateHeader const & o) const;
private:
  Ipv4Address    m_origin;         ///< Originator IP Address
  Ipv4Address    m_dst;            ///< Destination IP Address
  uint32_t       m_hopCount;       ///< Number of Hops
  uint32_t       m_binaryState;    ///< Binary State
};

/**
  * \brief Stream output operator
  * \param os output stream
  * \return updated stream
  */
std::ostream & operator<< (std::ostream & os, UpdateHeader const &);

/**
 * \ingroup bsdvr
 * \brief HELLO Message Format
 * \verbatim
|      0        |      1        |      2        |       3       |
 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                Origin Neighbor Interface Address              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|              Destination Neighbor Interface Address           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* \endverbatim
*/
class HelloHeader : public Header
{
public:
  /**
   * constructor
   *
   * \param origin the origin IP address
   * \param dst the destination IP address
   */
  HelloHeader (Ipv4Address origin = Ipv4Address (), Ipv4Address dst = Ipv4Address ());
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  // Fields
  /**
   * \brief Set the origin address
   * \param a the origin address
   */
  void SetOrigin (Ipv4Address a)
  {
    m_origin = a;
  }
  /**
   * \brief Get the origin address
   * \return the origin address
   */
  Ipv4Address GetOrigin () const
  {
    return m_origin;
  }
  /**
   * \brief Set the destination address
   * \param a the destination address
   */
  void SetDst (Ipv4Address a)
  {
    m_dst = a;
  }
  /**
   * \brief Get the destination address
   * \return the destination address
   */
  Ipv4Address GetDst () const
  {
    return m_dst;
  }
  /**
   * \brief Comparison operator
   * \param o HELLO header to compare
   * \return true if the HELLO headers are equal
   */
  bool operator== (HelloHeader const & o) const;
private:
  Ipv4Address    m_origin;         ///< Originator IP Address
  Ipv4Address    m_dst;            ///< Destination IP Address
};

/**
  * \brief Stream output operator
  * \param os output stream
  * \return updated stream
  */
std::ostream & operator<< (std::ostream & os, HelloHeader const &);

}  // namespace bsdvr
}  // namespace ns3

#endif /*BSDVRPACKET_H*/