#ifndef GPSR_PTABLE_H
#define GPSR_PTABLE_H
#define _DEFINE_DEPRECATED_HASH_CLASSES 0
#include <map>
#include <set>
#include <cassert>
#include <stdint.h>
#include "ns3/ipv4.h"
#include "ns3/timer.h"
#include <sys/types.h>
#include "ns3/node.h"
#include "ns3/node-list.h"
#include "ns3/mobility-model.h"
#include "ns3/vector.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/random-variable-stream.h"
#include <complex>

namespace ns3 {
namespace gpsr {

/*
 * \ingroup gpsr
 * \brief Position table used by GPSR
 */
class PositionTable
{
public:
  /// c-tor
  PositionTable();
  PositionTable (Vector, double);
  PositionTable operator()(Vector, double);

  /**
   * \brief Gets the last time the entry was updated
   * \param id Ipv4Address to get time of update from
   * \return Time of last update to the position
   */
  Time GetEntryUpdateTime (Ipv4Address id);

  /**
   * \brief Adds entry in position table
   */
  void AddEntry (Ipv4Address id, Vector position);

  /**
   * \brief Deletes entry in position table
   */
  void DeleteEntry (Ipv4Address id);

  /**
   * \brief Gets position from position table
   * \param id Ipv4Address to get position from
   * \return Position of that id or NULL if not known
   */
  Vector GetPosition (Ipv4Address id);

  /**
   * \brief Checks if a node is a neighbour
   * \param id Ipv4Address of the node to check
   * \return True if the node is neighbour, false otherwise
   */
  bool isNeighbour (Ipv4Address id);

  /**
   * \brief remove entries with expired lifetime
   */
  void Purge ();

  /**
   * \brief clears all entries
   */
  void Clear ();

  /**
   * \Get Callback to ProcessTxError
   */
  Callback<void, WifiMacHeader const &> GetTxErrorCallback () const
  {
    return m_txErrorCallback;
  }

  void PrintNeighbors (std::ostream &os);

  //Planarizes Graph by RNG
  void PlanarizeNeighbors(Vector nodePos);

  /**
   * \brief Gets next hop according to GPSR protocol
   * \param position the position of the destination node
   * \param nodePos the position of the node that has the packet
   * \return Ipv4Address of the next hop, Ipv4Address::GetZero () if no nighbour was found in greedy mode
   */
  Ipv4Address BestNeighbor (Vector position, Vector nodePos);

  /**
   * \brief Gets next hop according to Electrostatics based Greedy Forwarding
   * \param position the position of the destination node
   * \param nodePos the position of the node that has the packet
   * \return Ipv4Address of the next hop, Ipv4Address::GetZero () if no nighbour was found in Repulsion mode
   */
  Ipv4Address ElectrostaticBestNeighbor (Vector position, Vector nodePos);

  bool IsInSearch (Ipv4Address id);

  bool HasPosition (Ipv4Address id);

  static Vector GetInvalidPosition ()
  {
    return Vector (-1, -1, 0);
  }

  /**
   * \brief Gets next hop according to GPSR recovery-mode protocol (right hand rule)
   * \param previousHop the position of the node that sent the packet to this node
   * \param nodePos the position of the destination node
   * \return Ipv4Address of the next hop, Ipv4Address::GetZero () if no nighbour was found in greedy mode
   */
  Ipv4Address BestAngle (Vector previousHop, Vector nodePos);

  //Gives angle between the vector CentrePos-Refpos to the vector CentrePos-node counterclockwise
  double GetAngle (Vector centrePos, Vector refPos, Vector node);


private:
  Time m_entryLifeTime;
  std::map<Ipv4Address, std::pair<Vector, Time> > m_table;
  std::set<Ipv4Address> m_planarized_neighbors; //keeps set of prohibited neighbors IP addresses due graph planarization
  // TX error callback
  Callback<void, WifiMacHeader const &> m_txErrorCallback;
  // Process layer 2 TX error notification
  void ProcessTxError (WifiMacHeader const&);

  // New stuff
  Vector m_holePosition;
  double m_holeRadius;


};

}   // gpsr
} // ns3
#endif /* GPSR_PTABLE_H */
