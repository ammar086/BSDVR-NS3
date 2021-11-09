/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/bsdvr-packet.h"
#include "ns3/address-utils.h"
#include "ns3/v4ping-helper.h"
#include "ns3/network-module.h"
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
  std::cout << tHeader1 << tHeader1.IsValid()<<std::endl;
  ns3::bsdvr::TypeHeader tHeader2 (ns3::bsdvr::BSDVRTYPE_UPDATE);
  std::cout << tHeader2 << tHeader2.IsValid()<<std::endl;
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}


