/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include <iostream>
#include <fstream> 
#include "ns3/core-module.h"
#include "ns3/bsdvr-packet.h"
#include "ns3/bsdvr-rtable.h"
#include "ns3/address-utils.h"
#include "ns3/v4ping-helper.h"
#include "ns3/network-module.h"
#include "ns3/bsdvr-neighbor.h"
#include "ns3/bsdvr-constants.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;


int 
main (int argc, char *argv[])
{
  bool verbose = true;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);

  cmd.Parse (argc,argv);

  /* ... */
  ns3::bsdvr::TypeHeader tHeader1 (ns3::bsdvr::BSDVRTYPE_HELLO);
  std::cout << tHeader1 << " : " << tHeader1.IsValid() << std::endl;
  ns3::bsdvr::TypeHeader tHeader2 (ns3::bsdvr::BSDVRTYPE_UPDATE);
  std::cout << tHeader2 << " : " << tHeader2.IsValid() << std::endl;
  /* ... */
  ns3::bsdvr::Neighbors links(Seconds (35));
  links.Update(Ipv4Address (), Seconds(30));
  std::cout << links.GetExpireTime(Ipv4Address ()) << std::endl;
  /* ... */
  ns3::bsdvr::RoutingTableEntry entry (0, Ipv4Address (), Ipv4InterfaceAddress (), 7, Ipv4Address (), false);
  std::filebuf fb;
  fb.open ("test.txt",std::ios::out);
  std::ostream os(&fb);
  OutputStreamWrapper fs(&os);
  entry.Print (&fs, Time::Unit ());
  /* ... */
  ns3::bsdvr::RoutingTable table;
  std::map<Ipv4Address, ns3::bsdvr::RoutingTableEntry> tmp;
  tmp = table.GetForwardingTable ();
  table.AddRoute(entry,tmp);
  table.Print (tmp, &fs, Time::Unit ());
  /* ... */
  ns3::bsdvr::RoutingTableEntry r1 (0, Ipv4Address (), Ipv4InterfaceAddress (), 7, Ipv4Address (), false);
  ns3::bsdvr::RoutingTableEntry r2 (0, Ipv4Address (), Ipv4InterfaceAddress (), 4, Ipv4Address (), false);
  // r2.SetRouteState (ns3::bsdvr::ACTIVE);
  r1.Print (&fs, Time::Unit ());
  r2.Print (&fs, Time::Unit ());
  std::cout << "Threshold for hopCount is: " << bsdvr::constants::THRESHOLD << std::endl;
  // std::cout<< "r1 is better than r2: " << table.isBetterRoute(r1, r2) << std::endl;
  /* ... */

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}


