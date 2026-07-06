// -*- mode: c++ -*-

#ifndef COMPONENTS_MERLIN_TOPOLOGY_GRAPH_H
#define COMPONENTS_MERLIN_TOPOLOGY_GRAPH_H

#include <sst/core/event.h>
#include <sst/core/link.h>
#include <sst/core/params.h>

#include <string>
#include <vector>
#include <map>
#include "sst/elements/merlin/router.h"
#include <nlohmann/json.hpp>
#include <sst/core/rng/rng.h>


namespace SST {
namespace Merlin {

class topo_graph_init_event : public internal_router_event {
public:
  int phase;
  std::vector<int> route_path;
  int route_port_id;

  topo_graph_init_event()
      : internal_router_event(), phase(0), route_port_id(0) {}
  topo_graph_init_event(RtrEvent *ev, int p = 0)
      : internal_router_event(ev), phase(p), route_port_id(0) {}

  virtual internal_router_event *clone(void) override {
    return new topo_graph_init_event(*this);
  }

  void setRoute_path(const std::vector<int> &path) { route_path = path; }
  void clearRoute_path() { route_path.clear(); }
  void serialize_order(SST::Core::Serialization::serializer &ser) override {
    internal_router_event::serialize_order(ser);
    SST_SER(phase);
    SST_SER(route_path);
    SST_SER(route_port_id);
  }

private:
  ImplementSerializable(SST::Merlin::topo_graph_init_event)
};

class topo_graph : public Topology {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
      topo_graph, "merlin", "graph", SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Topology loaded from an undirected edge-list graph file",
      SST::Merlin::Topology)

  SST_ELI_DOCUMENT_PARAMS(
      {"graph_file",
       "Path to graph file. Format: first non-comment line is '<num_routers> "
       "<num_edges>'; following lines are '<src> <dst>'."},
      {"route_table_file", "Path to route table file."},
      {"hosts_per_router", "Number of endpoints attached to each router."},
      {"total_routers",
       "Number of routers in the graph. Usually read from graph_file.", "0"},
      {"num_vcs", "Virtual channels per virtual network.", "1"})

private:
  int router_id;
  int num_ports;
  int num_vns;
  int num_vcs;
  int hosts_per_router;
  int total_routers;

  std::string graph_file;

  std::vector<std::vector<int>> adjacency;
  std::vector<int> route_table;
  std::vector<int> neighbor_to_port;

  std::vector<int> broadcast_parent;
  std::vector<std::vector<int>> broadcast_children;

  std::vector<std::vector<int>> all_paths;
  std::map<std::pair<int, int>, int> route_table_map;
  std::vector<std::vector<std::vector<int>>> all_shortest_paths;
  bool use_ecmp_routing;
  RNG::Random *rng;

public:
  topo_graph(ComponentId_t cid, Params &params, int num_ports, int rtr_id,
             int num_vns);
  ~topo_graph();

  virtual void route_packet(int port, int vc,
                            internal_router_event *ev) override;
  virtual internal_router_event *process_input(RtrEvent *ev) override;

  virtual void routeUntimedData(int port, internal_router_event *ev,
                                std::vector<int> &outPorts) override;
  virtual internal_router_event *
  process_UntimedData_input(RtrEvent *ev) override;

  virtual PortState getPortState(int port) const override;
  virtual std::string getPortLogicalGroup(int port) const override;
  virtual int getEndpointID(int port) override;

  virtual void getVCsPerVN(std::vector<int> &vcs_per_vn) override {
    for (int i = 0; i < num_vns; ++i) {
      vcs_per_vn[i] = num_vcs;
    }
  }

private:
  void loadGraph();
  void initRouteTable();
  void initBroadcastTree();
  void parse_route_table(Params &params);
  void parse_route_table_array(const nlohmann::json &j);
  void parse_ecmp_paths(const nlohmann::json &paths_obj);
  void route_packet_bfs(int port, int vc, internal_router_event *ev);

  int getRouterID(int endpoint) const;
  int getDestLocalPort(int endpoint) const;
  int getPortForNeighbor(int neighbor) const;
};

} // namespace Merlin
} // namespace SST

#endif // COMPONENTS_MERLIN_TOPOLOGY_GRAPH_H
