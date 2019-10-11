/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-stream-helper.h"
#include "ns3/tcp-stream-interface.h"

// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1   n2   n3   n4
//    point-to-point  |    |    |    |
//                    ================
//                      LAN 10.1.2.0


template <typename T>
std::string ToString(T val)
{
    std::stringstream stream;
    stream << val;
    return stream.str();
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SecondScriptExample");

int
main (int argc, char *argv[])
{
  uint64_t segmentDuration;
  // The simulation id is used to distinguish log file results from potentially multiple consequent simulation runs.
  uint32_t simulationId = 1;
  uint32_t numberOfClients = 1;
  std::string adaptationAlgo;
  std::string segmentSizeFilePath;
  std::string videoFilePath;
  std::string fifoPath;
  int abrPort = 8001;
  double bandwidthPerPlayer = 2.5; //mbps
  std::string logDir = dashLogDirectory();
  uint64_t playbackStartDifference = 20000; //20 ms

  uint64_t p2pSpeed = 5*1000000; //5mbps



  CommandLine cmd;
  cmd.Usage ("Simulation of streaming with DASH.\n");
  cmd.AddValue ("simulationId", "The simulation's index (for logging purposes)", simulationId);
  cmd.AddValue ("numberOfClients", "The number of clients", numberOfClients);
  cmd.AddValue ("segmentDuration", "The duration of a video segment in microseconds", segmentDuration);
  cmd.AddValue ("adaptationAlgo", "The adaptation algorithm that the client uses for the simulation", adaptationAlgo);
  cmd.AddValue ("segmentSizeFile", "The relative path (from ns-3.x directory) to the file containing the segment sizes in bytes", segmentSizeFilePath);
  cmd.AddValue ("videoFile", "The relative path (from ns-3.x directory) to the file containing the segment sizes in bytes", videoFilePath);
  cmd.AddValue ("startInterval", "playbackStartDifference", playbackStartDifference);
  cmd.AddValue ("fifoPath", "FifoPath for remote", fifoPath);
  cmd.AddValue ("abrPort", "abrPort for remote", abrPort);
  cmd.AddValue ("bwPerPlayer", "bandwidthPerPlayer", bandwidthPerPlayer);
  cmd.AddValue ("logDir", "Dash log dir", logDir);

  cmd.Parse (argc,argv);

  dashLogDirectory(logDir);
//   dashLogDirectory() = logDir;


  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (524288));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (524288));


  numberOfClients = numberOfClients == 0 ? 1 : numberOfClients;

  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  NodeContainer csmaNodes;
  csmaNodes.Add (p2pNodes.Get (1));
  csmaNodes.Create (numberOfClients);

  std::vector <std::pair <Ptr<Node>, std::string> > clients;
  for (NodeContainer::Iterator i = csmaNodes.Begin () + 1; i != csmaNodes.End (); ++i)
    {
      std::pair <Ptr<Node>, std::string> client (*i, adaptationAlgo);
      clients.push_back (client);
    }

  p2pSpeed = bandwidthPerPlayer*numberOfClients*1000*1000;

  std::cout << p2pSpeed << std::endl;

  PointToPointHelper pointToPoint;
  //pointToPoint.SetChannelAttribute ("DataRate", DataRateValue (DataRate (p2pSpeed)));
  pointToPoint.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (p2pSpeed)));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  InternetStackHelper stack;
  stack.Install (p2pNodes.Get (0));
  stack.Install (csmaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);


  // create folder so we can log the positions of the clients
  std::string tmpdashLogDirectory = logDir;
  const char * mylogsDir = tmpdashLogDirectory.c_str();
  mkdir (mylogsDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  tmpdashLogDirectory += "/" + adaptationAlgo;
  const char * tobascoDir = tmpdashLogDirectory.c_str ();
  mkdir (tobascoDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  tmpdashLogDirectory += "/" + ToString (numberOfClients);
  const char * simdir = tmpdashLogDirectory.c_str();
  mkdir(simdir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  tmpdashLogDirectory += "/sim" + ToString (simulationId);
  const char * dir = tmpdashLogDirectory.c_str();
  mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);


  TcpStreamServerHelper streamServer (9); //TODO replace with http stream server
  ApplicationContainer serverApps = streamServer.Install (p2pNodes.Get (0));
  serverApps.Start (Seconds (1.0));
//   serverApps.Stop (Seconds (10.0));

  TcpStreamClientHelper streamClient (p2pInterfaces.GetAddress (0), 9);
  streamClient.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
  streamClient.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));
  streamClient.SetAttribute ("VideoFilePath", StringValue (videoFilePath));
  streamClient.SetAttribute ("NumberOfClients", UintegerValue(numberOfClients));
  streamClient.SetAttribute ("SimulationId", UintegerValue (simulationId));
  streamClient.SetAttribute ("fifoPath", StringValue (fifoPath));
  streamClient.SetAttribute ("AbrPort", IntegerValue(abrPort));

  ApplicationContainer clientApps = streamClient.Install (clients);
  for (uint i=0; i<clientApps.GetN(); i++)
  {
	  clientApps.Get(i)->SetStartTime (MicroSeconds (2000000 + i * playbackStartDifference));
  }



  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

//   csma.EnablePcapAll ("second");
//   pointToPoint.EnablePcap ("second", csmaDevices.Get (0), true);

  NS_LOG_INFO ("Run Simulation.");
  NS_LOG_INFO ("Sim: " << simulationId << "Clients: " << numberOfClients);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
