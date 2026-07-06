// -*- mode: c++ -*-

// Copyright 2009-2025 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2025, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// of the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#ifndef COMPONENTS_MERLIN_TOPOLOGY_TORUS_H
#define COMPONENTS_MERLIN_TOPOLOGY_TORUS_H

#include <sst/core/event.h>
#include <sst/core/link.h>
#include <sst/core/params.h>

#include <string.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include "sst/elements/merlin/router.h"

namespace SST {
namespace Merlin {

class topo_torus_trees_event : public internal_router_event {
public:
  int dimensions;
  int routing_dim;
  int *dest_loc;
  std::vector<int> route_path;
  int route_port_id;

  topo_torus_trees_event() : route_port_id{0} {}
  topo_torus_trees_event(int dim) : route_port_id{0} {
    dimensions = dim;
    routing_dim = 0;
    dest_loc = new int[dim];
    route_path.resize(dim);
  }
  ~topo_torus_trees_event() { delete[] dest_loc; }
  virtual internal_router_event *clone(void) override {
    topo_torus_trees_event *ttte = new topo_torus_trees_event(*this);
    ttte->dest_loc = new int[dimensions];
    memcpy(ttte->dest_loc, dest_loc, dimensions * sizeof(int));
    return ttte;
  }

  void setRoute_path(const std::vector<int> &path) { route_path = path; }
  void clearRoute_path() { route_path.clear(); }
  void serialize_order(SST::Core::Serialization::serializer &ser) override {
    internal_router_event::serialize_order(ser);
    SST_SER(dimensions);
    SST_SER(routing_dim);
    SST_SER(route_port_id);
    SST_SER(route_path);
    if (ser.mode() == SST::Core::Serialization::serializer::UNPACK) {
      dest_loc = new int[dimensions];
    }

    for (int i = 0; i < dimensions; i++) {
      SST_SER(dest_loc[i]);
    }
  }

private:
  ImplementSerializable(SST::Merlin::topo_torus_trees_event)
};

class topo_torus_trees : public Topology {

public:
  SST_ELI_REGISTER_SUBCOMPONENT(topo_torus_trees, "merlin", "torus_trees",
                                SST_ELI_ELEMENT_VERSION(1, 0, 0),
                                "Multi-dimensional torus trees topology object",
                                SST::Merlin::Topology)

  SST_ELI_DOCUMENT_PARAMS(
      // Parameters needed for use with old merlin python module
      {"torus.shape", "Shape of the torus specified as the number of routers "
                      "in each dimension, where each "
                      "dimension is separated by an x.  For example, 4x4x2x2.  "
                      "Any number of dimensions is supported."},
      {"torus.width", "Number of links between routers in each dimension, "
                      "specified in same manner as for shape.  "
                      "For example, 2x2x1 denotes 2 links in the x and y "
                      "dimensions and one in the z dimension."},
      {"torus.local_ports", "Number of endpoints attached to each router."},
      {"torus.route_table_file",
       "File containing the route table for the torus trees topology."},

      {"shape", "Shape of the torus specified as the number of routers in each "
                "dimension, where each dimension is "
                "separated by an x.  For example, 4x4x2x2.  Any number of "
                "dimensions is supported."},
      {"width", "Number of links between routers in each dimension, specified "
                "in same manner as for shape.  For "
                "example, 2x2x1 denotes 2 links in the x and y dimensions and "
                "one in the z dimension."},
      {"local_ports", "Number of endpoints attached to each router."}, )

private:
  int router_id;
  int *id_loc;
  // std::vector<int> id_loc;

  int dimensions;
  int *dim_size;
  int *dim_width;

  int (*port_start)[2]; // port_start[dim][direction: 0=pos, 1=neg]

  int num_local_ports;
  int local_port_start;

  int num_vns;

  std::vector<std::vector<int>> route_table;
  std::map<std::pair<int, int>, int> route_table_map;

public:
  topo_torus_trees(ComponentId_t cid, Params &params, int num_ports, int rtr_id,
                   int num_vns);
  ~topo_torus_trees();

  virtual void route_packet(int port, int vc, internal_router_event *ev);
  virtual internal_router_event *process_input(RtrEvent *ev);

  virtual void routeUntimedData(int port, internal_router_event *ev,
                                std::vector<int> &outPorts);
  virtual internal_router_event *process_UntimedData_input(RtrEvent *ev);

  virtual PortState getPortState(int port) const;
  virtual std::string getPortLogicalGroup(int port) const override;
  virtual int getEndpointID(int port);

  virtual void getVCsPerVN(std::vector<int> &vcs_per_vn) {
    for (int i = 0; i < num_vns; ++i) {
      vcs_per_vn[i] = 2;
    }
  }

protected:
  //   virtual int choose_multipath(int start_port, int num_ports, int
  //   dest_dist);

private:
  void idToLocation(int id, int *location) const;
  int LocationToId(int *location) const;
  void parseDimString(const std::string &shape, int *output) const;
  int get_dest_router(int dest_id) const;
  int get_dest_local_port(int dest_id) const;

  void parse_route_table(Params &params);
};

} // namespace Merlin
} // namespace SST

#endif // COMPONENTS_MERLIN_TOPOLOGY_TORUS_H
