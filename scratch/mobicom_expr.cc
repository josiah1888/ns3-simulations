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

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gpsr-module.h"
#include "ns3/aodv-module.h"
#include "ns3/random-variable-stream.h"
#include <fstream>

#include "ns3/netanim-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/flow-monitor.h"
#include <vector>
#include <utility>
#include <climits>
#include <cmath>
#include <sstream>

//#include "myapp.h"

NS_LOG_COMPONENT_DEFINE ("MyExpr");

#define MIN_NODES 38

using namespace ns3;

// We want to look at changes in the ns-3 TCP congestion window.  We need
// to crank up a flow and hook the CongestionWindow attribute on the socket
// of the sender.  Normally one would use an on-off application to generate a
// flow, but this has a couple of problems.  First, the socket of the on-off
// application is not created until Application Start time, so we wouldn't be
// able to hook the socket (now) at configuration time.  Second, even if we
// could arrange a call after start time, the socket is not public so we
// couldn't get at it.
//
// So, we can cook up a simple version of the on-off application that does what
// we want.  On the plus side we don't need all of the complexity of the on-off
// application.  On the minus side, we don't have a helper, so we have to get
// a little more involved in the details, but this is trivial.
//
// So first, we create a socket and do the trace connect on it; then we pass
// this socket into the constructor of our simple application which we then
// install in the source node.
// ===========================================================================
//

/**  Terminal commands
 *   clear && ./waf --run "mobicom_expr --RoutingModel=GPSR"
 *   clear && ./waf --run "mobicom_expr --RoutingModel=HWMP"
 *   git add scratch/hwmp-mesh.cc scratch/mobicom_expr.cc
 *   git ls-files | xargs git add
*/

class MyApp : public Application
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);
  //std::cout << Simulator::Now ().GetSeconds () << "\t" << "one packet has been sent! PacketSize="<<m_packetSize<<"\n";
  //std::cout << "TxSent\n";
  ScheduleTx ();
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

static void
SetNodePos(Ptr<Node> node, Vector pos)
{
	Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
	mobility->SetPosition(pos);
}

static void
SetNodeFailure (Ptr<Node> node, Time FailureTime)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  Vector oldPos = mobility->GetPosition();
  Vector failurePos (10000,0,0); //to simulate failure move node out of its neighbors range

  Simulator::Schedule (Simulator::Now(), &SetNodePos, node, failurePos);
  Simulator::Schedule (Simulator::Now() + FailureTime - Seconds(0.1), &SetNodePos,node, oldPos);
}

static void
DecideOnNodesFailure (NodeContainer nodes, Time FailureTime, double FailureProb, Time StopTime)
{
	Ptr<UniformRandomVariable> rvar = CreateObject<UniformRandomVariable>();
	NodeContainer::Iterator i;
    for (i = nodes.Begin(); !(i == nodes.End()); i++) {
		if (rvar->GetValue (0, 1) < FailureProb)
		{
			SetNodeFailure(*i, FailureTime);
			std::cout<< Simulator::Now ().GetSeconds () <<" sec, Node["<<(*i)->GetId()<<"] failed!"<<std::endl;
		}
	}

   	Simulator::Schedule (FailureTime, &DecideOnNodesFailure, nodes, FailureTime, FailureProb, StopTime);
}

static void
CourseChange (std::string foo, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition ();
  std::cout << Simulator::Now ().GetSeconds () << ", Paramedic has changed location -> POS: (" << pos.x << ", " << pos.y << ")." << std::endl;
}

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  //NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}

static void
SetupGPSR (NodeContainer& nodes, NetDeviceContainer& devices, YansWifiPhyHelper& wifiPhy, uint16_t RepulsionMode, std::string phyMode, Vector holePosition, double holeRadius) {
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue(phyMode),
                                "ControlMode",StringValue(phyMode));

  GpsrHelper gpsr;
  gpsr.Set("RepulsionMode", UintegerValue(RepulsionMode));

  OlsrHelper olsr;
  Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (olsr, 10);

  InternetStackHelper internet;
  internet.SetRoutingHelper (gpsr);
  internet.Install (nodes);

  gpsr.Install (holePosition, holeRadius); // install on all nodes

  devices = wifi.Install (wifiPhy, wifiMac, nodes);
}

static void
SetupHWMP (NodeContainer& nodes, NetDeviceContainer& devices, YansWifiPhyHelper& wifiPhy) {
  std::string root = "ff:ff:ff:ff:ff:ff";

  MeshHelper mesh = MeshHelper::Default();
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
  mesh.SetNumberOfInterfaces (1);

  InternetStackHelper internetStack;
  internetStack.Install (nodes);

  devices = mesh.Install (wifiPhy, nodes);
}


static void
SetConstantMobilityGrid(NodeContainer& nodes, double minx, double miny, double deltax, double deltay, uint gridWidth)
{
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (minx),
                                 "MinY", DoubleValue (miny),
                                 "DeltaX", DoubleValue (deltax),
                                 "DeltaY", DoubleValue (deltay),
                                 "GridWidth", UintegerValue (gridWidth),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);
}

static void
SetConstantPositionMobility(Ptr<Node> node, Vector v)
{
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject <ListPositionAllocator>();
  positionAlloc ->Add(v);
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(node);
}

static void SetupWaypointMobility(Ptr<Node> node, Vector startingLocation)
{
  MobilityHelper mobilitySrc;
  Ptr<ListPositionAllocator> positionAllocSrc = CreateObject <ListPositionAllocator>();
  positionAllocSrc ->Add(startingLocation);
  mobilitySrc.SetPositionAllocator(positionAllocSrc);
  mobilitySrc.SetMobilityModel ("ns3::WaypointMobilityModel", "InitialPositionIsWaypoint", BooleanValue (true));
  mobilitySrc.Install(node);
}

static void SetWaypoints(Ptr<Node> node, std::vector<std::pair<Time, Vector> > waypoints)
{
  // Pass in vector of tuples of destinations and time to get there
  Ptr<WaypointMobilityModel> model = node->GetObject<WaypointMobilityModel>();
  Time elapsed = Seconds (1);
  for (std::vector<std::pair<Time, Vector> >::iterator it=waypoints.begin() ; it < waypoints.end(); it++) {
    std::pair<Time, Vector> waypoint = *it;
    elapsed += waypoint.first;
    Waypoint w (elapsed, waypoint.second);
    model->AddWaypoint(w);
  }
}

static void SetupMobility(std::string mobilityScene, 
NodeContainer& c1, NodeContainer& c2, NodeContainer& c3, NodeContainer& c4, NodeContainer& sinkSrc,
Vector& holePosition, double& holeRadius, double scale, int nNodes
)
{
  // node container counts: 14, 14, 5, 5, 2
  Time locationTime = Seconds(120.0);
  double srcSpeed = 2.8;

  if (mobilityScene == "static-grid") {
    holePosition = Vector(200, 350, 0);
    holeRadius = 150;

    SetConstantMobilityGrid(c1, 50, 0, 50, 50, 1);
    SetConstantMobilityGrid(c2, 350, 0, 50, 50, 1);
    SetConstantMobilityGrid(c3, 100, 0, 50, 50, 5);
    SetConstantMobilityGrid(c4, 100, 650, 50, 50, 5);

    SetupWaypointMobility(sinkSrc.Get(0), Vector(0,0,0));
    std::vector<std::pair<Time, Vector> > waypoints;
    waypoints.push_back (std::make_pair(locationTime, Vector(0,0,0)));
    waypoints.push_back (std::make_pair(Seconds (325/srcSpeed), Vector(0,325,0)));
    waypoints.push_back (std::make_pair(locationTime, Vector(0,325,0)));
    waypoints.push_back (std::make_pair(Seconds (325/srcSpeed), Vector(0,0,0)));
    SetWaypoints(sinkSrc.Get (0), waypoints);

    SetConstantPositionMobility(sinkSrc.Get(1), Vector(400, 325, 0));
  } else if (mobilityScene == "mobile-nodes") {
    SetConstantMobilityGrid(c2, 350, 0, 50, 50, 1);
    SetConstantMobilityGrid(c3, 100, 0, 50, 50, 5);
    SetConstantMobilityGrid(c4, 100, 650, 50, 50, 5);

    for (int i = 0; i < c1.GetN (); i++) {
      SetupWaypointMobility(c1.Get (i), Vector(50, 50 * i,0));
      std::vector<std::pair<Time, Vector> > waypoints;
      waypoints.push_back (std::make_pair(Seconds (3), Vector(50 + 22 * i, 150 + 25 * i, 0)));
      SetWaypoints(c1.Get (i), waypoints);
    }

    SetupWaypointMobility(sinkSrc.Get(0), Vector(0,0,0));
    std::vector<std::pair<Time, Vector> > waypoints;
    waypoints.push_back (std::make_pair(locationTime, Vector(0,0,0)));
    waypoints.push_back (std::make_pair(Seconds (325/srcSpeed), Vector(0,325,0)));
    waypoints.push_back (std::make_pair(locationTime, Vector(0,325,0)));
    waypoints.push_back (std::make_pair(Seconds (325/srcSpeed), Vector(0,0,0)));
    SetWaypoints(sinkSrc.Get (0), waypoints);

    SetConstantPositionMobility(sinkSrc.Get(1), Vector(400, 325, 0));
  } else if (mobilityScene == "dense-mesh") {

    NodeContainer allNodes(c1, c2, c3, c4);
    int i = 0;

    SetConstantPositionMobility(allNodes.Get(i++), Vector(400, 250, 0));
    SetConstantPositionMobility(allNodes.Get(i++), Vector(200, 300, 0));
    SetConstantPositionMobility(allNodes.Get(i++), Vector(300, 350, 0));
    SetConstantPositionMobility(allNodes.Get(i++), Vector(200, 100, 0));
    SetConstantPositionMobility(allNodes.Get(i++), Vector(300, 150, 0));
    SetConstantPositionMobility(allNodes.Get(i++), Vector(50, 150, 0));
    SetConstantPositionMobility(allNodes.Get(i++), Vector(150, 150, 0));
    SetConstantPositionMobility(allNodes.Get(i++), Vector(100, 250, 0));
    SetConstantPositionMobility(allNodes.Get(i++), Vector(150, 300, 0));
    SetConstantPositionMobility(allNodes.Get(i++), Vector(50, 50, 0));
    SetConstantPositionMobility(allNodes.Get(i++), Vector(150, 50, 0));

    for (; i < allNodes.GetN (); i++) {
      SetConstantPositionMobility(allNodes.Get(i), Vector(1000, 1000, 0));
    }

    SetConstantPositionMobility(sinkSrc.Get(1), Vector(100, 50, 0));

    SetupWaypointMobility(sinkSrc.Get(0), Vector(100,100,0));
    std::vector<std::pair<Time, Vector> > waypoints;
    waypoints.push_back (std::make_pair(Seconds (325/srcSpeed), Vector(50,200,0)));
    waypoints.push_back (std::make_pair(locationTime, Vector(50,200,0)));
    waypoints.push_back (std::make_pair(Seconds (325/srcSpeed), Vector(100,350,0)));
    waypoints.push_back (std::make_pair(locationTime, Vector(100,350,0)));
    waypoints.push_back (std::make_pair(Seconds (325/srcSpeed), Vector(400,300,0)));
    waypoints.push_back (std::make_pair(locationTime, Vector(400,300,0)));
    SetWaypoints(sinkSrc.Get (0), waypoints);
  } else if (mobilityScene == "city-block") {
/*
   100m  |
         |
   100m  |   292m
_______ x x x x x x x
        x           x
        x           x 261m
        x           x
        x           x
        x x x x x x x
*/

    NodeContainer allNodes(c1, c2, c3, c4);

    if (nNodes < 16) {
      scale = scale * (nNodes / 16.0);
    }

    double a = 292 * scale, b = 261 * scale , offset = 100 * scale;
    double perimeter = 2 * (a + b);

    holePosition = Vector(offset + a / 2, offset + b / 2, 0);
    holeRadius = (a + b) / 4;

    for (double i = 0; i < allNodes.GetN (); i++) {
      double currentPosition = i / (double)allNodes.GetN () * perimeter;

      if (currentPosition < a) {
        SetConstantPositionMobility(allNodes.Get(i), Vector(offset + currentPosition, offset, 0));
      } else if (currentPosition < a + b) {
        SetConstantPositionMobility(allNodes.Get(i), Vector(offset + a, offset + currentPosition - a, 0));
      } else if (currentPosition < 2 * a + b) {
        SetConstantPositionMobility(allNodes.Get(i), Vector(offset + 2 * a - currentPosition + b, offset + b, 0));
      } else {
        SetConstantPositionMobility(allNodes.Get(i), Vector(offset, offset + 2 * (a + b) - currentPosition, 0));
      }
    }

    SetConstantPositionMobility(sinkSrc.Get(1), Vector(offset + .2 * a, offset + .2 * b, 0));

    Vector current = Vector(offset * 1.5 + a, offset + 2.0 / 3.0 * b, 0), next;
    SetupWaypointMobility(sinkSrc.Get(0), current);
    std::vector<std::pair<Time, Vector> > waypoints;

    next = Vector(offset * .5 + a, offset * .5 + b ,0);
    waypoints.push_back (std::make_pair(Seconds (CalculateDistance(current, next) / srcSpeed), next));
    waypoints.push_back (std::make_pair(locationTime, next));
    
    current = next;
    next = Vector(offset + 2.0 / 3.0 * a, offset * 1.5 + b,0);
    waypoints.push_back (std::make_pair(Seconds (CalculateDistance(current, next) / srcSpeed), next));
    waypoints.push_back (std::make_pair(locationTime, next));
    
    SetWaypoints(sinkSrc.Get (0), waypoints);

    std::cout << "Space between nodes: " << 1.0 / (double)allNodes.GetN () * perimeter << "m" << std::endl;
  } else if (mobilityScene == "full-grid") {

    NodeContainer allNodes(c1, c2, c3, c4);
    NodeContainer gridNodes;
    double offset = 100 * scale;
    double spaceBetweenNodes = 50 * scale;
    double nColumns = sqrt(nNodes);
    
    if (nColumns * nColumns != nNodes) {
      std::cout << "To make a square grid, let nNodes be a perfect square" << std::endl;
    }
    
    for (int i = 0; i < allNodes.GetN (); i++) {
      if (i < nColumns * nColumns) {
        gridNodes.Add (allNodes.Get (i));
      } else {
        SetConstantPositionMobility(allNodes.Get (i), Vector(0, offset + nColumns * spaceBetweenNodes * 1.5, 0));
      }
    }

    SetConstantMobilityGrid(gridNodes, offset, offset, spaceBetweenNodes, spaceBetweenNodes, nColumns);
    SetConstantPositionMobility(sinkSrc.Get(1), Vector(offset + 25, offset + 25, 0));
    //SetConstantPositionMobility(sinkSrfc.Get(0), Vector(offset + spaceBetweenNodes * (nColumns - 1) + 50, offset + spaceBetweenNodes * (nColumns - 1) + 50, 0));

    Vector current = Vector(offset + spaceBetweenNodes * (nColumns - 1) + 25 , offset + 2.0 / 3.0 * spaceBetweenNodes * (nColumns - 1), 0), next;
    SetupWaypointMobility(sinkSrc.Get(0), current);
    std::vector<std::pair<Time, Vector> > waypoints;
    waypoints.push_back (std::make_pair(locationTime, current));

    next = Vector(offset + spaceBetweenNodes * (nColumns - 1) - 25, offset + spaceBetweenNodes * (nColumns - 1) - 25 ,0);
    waypoints.push_back (std::make_pair(Seconds (CalculateDistance(current, next) / srcSpeed), next));
    waypoints.push_back (std::make_pair(locationTime, next));
    
    current = next;
    next = Vector(offset + 2.0 / 3.0 * spaceBetweenNodes * (nColumns - 1), offset + spaceBetweenNodes * (nColumns - 1) + 25,0);
    waypoints.push_back (std::make_pair(Seconds (CalculateDistance(current, next) / srcSpeed), next));
    waypoints.push_back (std::make_pair(locationTime, next));
    
    SetWaypoints(sinkSrc.Get (0), waypoints);
  } else if (mobilityScene == "straight-line") {
    // todo, make straight line mobility scene

    NodeContainer allNodes(c1, c2, c3, c4);
    double spaceBetweenNodes = 100 * scale;
    double x = 0;

    SetConstantPositionMobility(sinkSrc.Get(1), Vector(x += spaceBetweenNodes, 0, 0));

    for (int i = 0; i < allNodes.GetN (); i++) {
      SetConstantPositionMobility(allNodes.Get (i), Vector(x += spaceBetweenNodes, 0, 0));
    }

    SetConstantPositionMobility(sinkSrc.Get(0), Vector(x += spaceBetweenNodes, 0, 0));
  }
}

int main (int argc, char *argv[])
{
  bool enableFlowMonitor = false;
  //ErpOfdmRate54Mbps
  //DsssRate11Mbps
  std::string phyMode ("ErpOfdmRate54Mbps");
  uint16_t RepulsionMode = (uint16_t) 0;
  Time FailureTime = Seconds(30.0);
  double FailureProb = 0.0;
  Time StopTime = Seconds(780.0);
  Time LocationTime = Seconds(10.0); // 180.0
  double SrcSpeed = 2.8; //[m/s] speed of jogging
  std::string RoutingModel = "GPSR";
  bool EnableNetAnim = false;
  double NodeDataRate = 5.0;
  std::string MobilityScene = "static-grid";
  double scale = 1;
  int nNodes = MIN_NODES;
  std::string protocol = "tcp";
  
  // Not sure how much we should implement this
  Vector holePosition = Vector(-1000, 2, 0);
  double holeRadius = 5.0;


  CommandLine cmd;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("RepulsionMode", "Enable Repulsion during Greedy Forwarding", RepulsionMode);
  cmd.AddValue ("FailureTime", "Nodes Failure Time", FailureTime);
  cmd.AddValue ("FailureProb", "Nodes Failure Probability", FailureProb);
  cmd.AddValue ("StopTime", "Time to Stop Simulation", StopTime);
  cmd.AddValue ("LocationTime", "Time src spends at each location", LocationTime);
  cmd.AddValue ("SrcSpeed", "Speed of the paramedic who acts as a src between locations", SrcSpeed);
  cmd.AddValue ("RoutingModel", "Routing algorithm to use. Can be 'HWMP' or 'GPSR'", RoutingModel);
  cmd.AddValue ("EnableNetAnim", "Enable simulation recording for NetAnim", EnableNetAnim);
  cmd.AddValue ("NodeDataRate", "Data rate for nodes", NodeDataRate);
  cmd.AddValue ("NNodes", "Number of nodes.", nNodes);
  cmd.AddValue ("Scale", "Scale of the scene.", scale);
  cmd.AddValue ("Protocol", "Which protocol to use: udp or tcp", protocol);
  cmd.AddValue ("MobilityScene", "Which mobility scene to use of ['city-block', 'full-grid', 'straight-line']", MobilityScene);
  cmd.Parse (argc, argv);

  std::cout << "Scale: " << scale;

//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer c1; // sink and source
  c1.Create (nNodes);
  NodeContainer c2;
  //c2.Create(14);
  NodeContainer c3;
  //c3.Create(5);
  NodeContainer c4;
  //c4.Create(nNodes > MIN_NODES ? 5 + nNodes - MIN_NODES : 5);
  

  NodeContainer allTransmissionNodes;
  allTransmissionNodes.Add(c1);
  allTransmissionNodes.Add(c2);
  allTransmissionNodes.Add(c3);
  allTransmissionNodes.Add(c4);

  NodeContainer sinkSrc;
  sinkSrc.Create(2);

  //add to one container
  NodeContainer c;
  c.Add(sinkSrc); //add sink and source
  c.Add(allTransmissionNodes);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);

  YansWifiChannelHelper wifiChannel ;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
	  	  	  	  	  	  	  	    "SystemLoss", DoubleValue(1),
		  	  	  	  	  	  	    "HeightAboveZ", DoubleValue(1.5));

  // For range near 250m
  wifiPhy.Set ("TxPowerStart", DoubleValue(20));
  wifiPhy.Set ("TxPowerEnd", DoubleValue(20));
  wifiPhy.Set ("TxPowerLevels", UintegerValue(1));
  wifiPhy.Set ("TxGain", DoubleValue(6));
  wifiPhy.Set ("RxGain", DoubleValue(0));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue(-68.8));//-64.5));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue(-71.8));//-67.5));

  wifiPhy.SetChannel (wifiChannel.Create ());


  NetDeviceContainer devices;
  MeshHelper mesh;
  // todo: should probably return NetDeviceContainer devices from setup methods

  if (RoutingModel == "GPSR") {
    std::cout << "using GPSR\n";
    SetupGPSR(c, devices, wifiPhy, RepulsionMode, phyMode, holePosition, holeRadius);
  } else if (RoutingModel == "HWMP") {
    std::cout << "using HWMP\n";
    SetupHWMP(c, devices, wifiPhy);
  }

  // Set up Addresses
  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifcont = ipv4.Assign (devices);

  SetupMobility(MobilityScene, c1, c2, c3, c4, sinkSrc, holePosition, holeRadius, scale, nNodes);

  std::cout << "hole pos: " << holePosition << " hole radius: " << holeRadius << std::endl;

  //setup applications
  NS_LOG_INFO ("Create Applications.");


  if (protocol == "tcp") {

    std::cout << "Using tcp protocol" << std::endl;

    uint16_t sinkPort = 8080;
    Address sinkAddress (InetSocketAddress (ifcont.GetAddress (1), sinkPort));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install (sinkSrc.Get (1));
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (StopTime);

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (sinkSrc.Get (0), TcpSocketFactory::GetTypeId ());
    // ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));

    Ptr<MyApp> app = CreateObject<MyApp> ();
    app->Setup (ns3TcpSocket, sinkAddress, 1448, DataRate ("5Mbps"));
    sinkSrc.Get (0)->AddApplication (app);
    app->SetStartTime (Seconds (1.0));
    app->SetStopTime (StopTime);

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("cwnd_" + RoutingModel + ".txt");
    ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream));
  } else {

    std::cout << "Using udp protocol" << std::endl;

    UdpServerHelper server (9);
    ApplicationContainer serverApp = server.Install (sinkSrc.Get (0));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (StopTime);

    UdpClientHelper client (ifcont.GetAddress (0), 9);
    client.SetAttribute ("MaxPackets", UintegerValue ((uint32_t)(StopTime.GetSeconds () * pow(2, 20) * 5)));
    client.SetAttribute ("Interval", TimeValue (Seconds (0.01))); // 5Mbps =0.0001953125
    client.SetAttribute ("PacketSize", UintegerValue (32));


      // client.SetAttribute ("MaxPackets", UintegerValue ((uint32_t)(m_totalTime*(1/m_packetInterval))));
      //client.SetAttribute ("Interval", TimeValue (Seconds (m_packetInterval)));
      //client.SetAttribute ("PacketSize", UintegerValue (m_packetSize));

    ApplicationContainer clientApp = client.Install(sinkSrc.Get(1));
    clientApp.Start (Seconds (1.0));
    clientApp.Stop (StopTime);  

    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  }

  std::cout <<"src ip="<<ifcont.GetAddress (0, 0)<<" id="<<sinkSrc.Get (0)->GetId() <<"; sink ip="<<ifcont.GetAddress(1, 0)<<" id="<<sinkSrc.Get (1)->GetId() <<std::endl;

  std::ostringstream temp;  //temp as in temporary
  temp << sinkSrc.Get (0) ->GetId ();

  //log only src movements
  Config::Connect ("/NodeList/" + temp.str() + "/$ns3::MobilityModel/CourseChange",
                      MakeCallback (&CourseChange));

  // Trace devices (pcap)

  if (RoutingModel == "HWMP") {
    wifiPhy.EnablePcapAll ("mp-");
  } else {
    wifiPhy.EnablePcap ("mp-", devices.Get(0)); //save pcap file for src
    wifiPhy.EnablePcap ("mp-", devices.Get(1)); //save pcap file for sink
    // should we really trace throughput for the sink? It should only send ack messages
  }
  
  // Now, do the actual simulation.
  NS_LOG_INFO ("Run Simulation.");

  DecideOnNodesFailure (allTransmissionNodes, FailureTime, FailureProb, StopTime); // simulate node failures by moving them far from others

  Simulator::Stop (StopTime);
  // Doesn't seem to work unless it's literally right here. Can't place in an IF statement

  /* Animation */

  // Gives SIGSEGV
  // NodeContainer holeNodeContainer;
  // holeNodeContainer.Create(1);
  // SetConstantPositionMobility(holeNodeContainer.Get (0), holePosition);

  AnimationInterface anim ("mobicom_expr_animation_" + RoutingModel + ".xml");
  anim.SkipPacketTracing ();
  anim.UpdateNodeColor (sinkSrc.Get (0), 0, 255, 0);
  anim.UpdateNodeSize (sinkSrc.Get (0) -> GetId(), 5 * scale, 5 * scale);
  anim.UpdateNodeColor (sinkSrc.Get (1), 0, 0, 255);
  anim.UpdateNodeSize (sinkSrc.Get (1) -> GetId(), 5 * scale, 5 * scale);
  //anim.UpdateNodeColor (holeNodeContainer.Get (0), 0, 0, 255);
  anim.SetMaxPktsPerTraceFile (ULLONG_MAX);
  if (MobilityScene == "city-block") {
    anim.SetBackgroundImage ("/Users/muitprogram/Google Drive/joplin-city-block-cropped.jpg",
                          50.0 * scale, 50.0 * scale, scale / 4, scale / 4, 1);
  }

  /* End Animation */

  Simulator::Run ();

  // if (RoutingModel == "HWMP") {
  //   std::ostringstream os;
  //   os << "mp-report.xml";
  //   std::ofstream of;
  //   of.open (os.str ().c_str ());
  //   if (!of.is_open ())
  //   {
  //     std::cerr << "Error: Can't open file " << os.str () << "\n";
  //   }
  //   mesh.Report (devices.Get (0), of);
  //   of << "\n\n\n<NextDevice>";
  //   mesh.Report (devices.Get (1), of);
  //   of << "</NextDevice>\n";
  //   of.close ();
  // }

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
