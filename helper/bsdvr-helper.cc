/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "bsdvr-helper.h"
#include "ns3/bsdvr.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-list-routing.h"

namespace ns3 {

  BsdvrHelper::BsdvrHelper() : 
  Ipv4RoutingHelper ()
  {
  m_agentFactory.SetTypeId ("ns3::bsdvr::RoutingProtocol");
  }

  BsdvrHelper::~BsdvrHelper ()
  {
  }

  BsdvrHelper* 
  BsdvrHelper::Copy (void) const 
  {
   return new BsdvrHelper (*this); 
  } 

  Ptr<Ipv4RoutingProtocol> 
  BsdvrHelper::Create (Ptr<Node> node) const
  {
    Ptr<bsdvr::RoutingProtocol> agent = m_agentFactory.Create<bsdvr::RoutingProtocol> ();
    node->AggregateObject (agent);
    return agent;
  }

  void 
  BsdvrHelper::Set (std::string name, const AttributeValue &value)
  {
    m_agentFactory.Set (name, value);
  }

  int64_t
  BsdvrHelper::AssignStreams (NodeContainer c, int64_t stream)
  {
    int64_t currentStream = stream;
    Ptr<Node> node;
    for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
      {
        node = (*i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
        NS_ASSERT_MSG (ipv4, "Ipv4 not installed on node");
        Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol ();
        NS_ASSERT_MSG (proto, "Ipv4 routing not installed on node");
        Ptr<bsdvr::RoutingProtocol> bsdvr = DynamicCast<bsdvr::RoutingProtocol> (proto);
        if (bsdvr)
          {
            currentStream += bsdvr->AssignStreams (currentStream);
            continue;
          }
        // Bsdvr may also be in a list
        Ptr<Ipv4ListRouting> list = DynamicCast<Ipv4ListRouting> (proto);
        if (list)
          {
            int16_t priority;
            Ptr<Ipv4RoutingProtocol> listProto;
            Ptr<bsdvr::RoutingProtocol> listBsdvr;
            for (uint32_t i = 0; i < list->GetNRoutingProtocols (); i++)
              {
                listProto = list->GetRoutingProtocol (i, priority);
                listBsdvr = DynamicCast<bsdvr::RoutingProtocol> (listProto);
                if (listBsdvr)
                  {
                    currentStream += listBsdvr->AssignStreams (currentStream);
                    break;
                  }
              }
          }
      }
    return (currentStream - stream);
  }

}

