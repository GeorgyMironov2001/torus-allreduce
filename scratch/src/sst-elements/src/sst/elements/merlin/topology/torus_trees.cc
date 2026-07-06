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

#include "torus_trees.h"
#include "torus.h"
#include <sst_config.h>

#include <algorithm>

#include <stdlib.h>

using namespace SST::Merlin;
void topo_torus_trees::parse_route_table(Params &params) {
  std::string route_table_file = params.find<std::string>("route_table_file");

  std::ifstream file(route_table_file);
  nlohmann::json j;
  file >> j;
  int id = 0;
  for (const auto &entry : j) {
    std::pair<int, int> key = std::make_pair(entry[0][0], entry[0][1]);
    route_table_map[key] = id++;
    std::vector<int> path;
    for (int rank : entry[1]) {
      path.push_back(rank);
    }
    route_table.push_back(path);
  }
}

topo_torus_trees::topo_torus_trees(ComponentId_t cid, Params &params,
                                   int num_ports, int rtr_id, int num_vns)
    : Topology(cid), router_id(rtr_id), num_vns(num_vns) {

  route_table.clear();
  route_table_map.clear();
  // Get the various parameters
  std::string shape;
  shape = params.find<std::string>("shape");
  if (!shape.compare("")) {
  }

  // Need to parse the shape string to get the number of dimensions
  // and the size of each dimension
  dimensions = std::count(shape.begin(), shape.end(), 'x') + 1;

  dim_size = new int[dimensions];
  dim_width = new int[dimensions];
  port_start = new int[dimensions][2];

  parseDimString(shape, dim_size);

  std::string width = params.find<std::string>("width", "");
  if (width.compare("") == 0) {
    for (int i = 0; i < dimensions; i++)
      dim_width[i] = 1;
  } else {
    parseDimString(width, dim_width);
  }

  int next_port = 0;
  for (int d = 0; d < dimensions; d++) {
    for (int i = 0; i < 2; i++) {
      port_start[d][i] = next_port;
      next_port += dim_width[d];
    }
  }

  num_local_ports = params.find<int>("local_ports", 1);

  // int n_vc = params.find<int>("num_vcs");
  // if ( n_vc < 2 || (n_vc & 1) ) {
  //     output.fatal(CALL_INFO, -1, "Number of VC's must be a multiple of two
  //     for a torus\n");
  // }

  int needed_ports = 0;
  for (int i = 0; i < dimensions; i++) {
    needed_ports += 2 * dim_width[i];
  }

  if (num_ports < (needed_ports + num_local_ports)) {
    output.fatal(CALL_INFO, -1,
                 "Number of ports should be %d for this configuration\n",
                 needed_ports + num_local_ports);
  }

  local_port_start = needed_ports; // Local delivery is on the last ports

  id_loc = new int[dimensions];
  idToLocation(router_id, id_loc);

  parse_route_table(params);
}

topo_torus_trees::~topo_torus_trees() {
  delete[] id_loc;
  delete[] dim_size;
  delete[] dim_width;
  delete[] port_start;
}

void topo_torus_trees::route_packet(int port, int vc,
                                    internal_router_event *ev) {
  int dest_router = get_dest_router(ev->getDest());
  if (dest_router == router_id) {
    ev->setNextPort(get_dest_local_port(ev->getDest()));
    return;
  }

  topo_torus_trees_event *ttt_ev = static_cast<topo_torus_trees_event *>(ev);
  int current_route_id = ttt_ev->getEncapsulatedEvent()->getRoute_id();

  if (!ttt_ev->route_path.empty()) {
    assert(router_id == ttt_ev->route_path[ttt_ev->route_port_id]);
  }

  std::vector<int> current_coord(dimensions, 0);
  std::vector<int> next_coord(dimensions, 0);
  idToLocation(router_id, current_coord.data());

  if (ttt_ev->route_path.empty()) {
    idToLocation(dest_router, next_coord.data());
  } else {
    idToLocation(ttt_ev->route_path[ttt_ev->route_port_id + 1],
                 next_coord.data());
  }

  int shift_coord = 0;
  for (int i = 0; i < dimensions; i++) {
    if (current_coord[i] != next_coord[i]) {
      shift_coord = i;
      break;
    }
  }

  if (!ttt_ev->route_path.empty() && ttt_ev->route_port_id > 0) {
    std::vector<int> prev_coord(dimensions, 0);
    idToLocation(ttt_ev->route_path[ttt_ev->route_port_id - 1],
                 prev_coord.data());
    if (prev_coord[shift_coord] == current_coord[shift_coord]) {
      ttt_ev->setVC(vc & (~1)); // Reset the VC
    }
  }

  if (std::abs(current_coord[shift_coord] - next_coord[shift_coord]) > 1) {
    int new_vc = vc ^ 1;
    ttt_ev->setVC(new_vc); // Toggle VC
  }
  int reverse_shift_coord = dimensions - shift_coord - 1;
  if (next_coord[shift_coord] % dim_size[reverse_shift_coord] ==
      (current_coord[shift_coord] + 1) % dim_size[reverse_shift_coord]) {
    ttt_ev->setNextPort(port_start[reverse_shift_coord][0]);
  } else {
    ttt_ev->setNextPort(port_start[reverse_shift_coord][1]);
  }
  if (!ttt_ev->route_path.empty()) {
    ttt_ev->route_port_id++;
  }
}

internal_router_event *topo_torus_trees::process_input(RtrEvent *ev) {
  // assert(ev->getRouteVN() == 0);
  // output.output("Torus::process_input VN=%d\n", ev->getRouteVN());

  //   output.output("Torus::process_input Route ID=%d\n", ev->getRoute_id());

  topo_torus_trees_event *tt_ev = new topo_torus_trees_event(dimensions);
  tt_ev->setEncapsulatedEvent(ev);
  tt_ev->setVC(tt_ev->getVN() * 2);
  int route_id = ev->getRoute_id();
  if (-1 != route_id) {
    int path_number = route_table.size();
    std::vector<int> path;
    if (route_id >= path_number) {
      int path_size = route_table[route_id - path_number].size();
      path.assign(path_size, 0);
      for (int i = 0; i < path_size; i++) {
        path[i] = route_table[route_id - path_number][path_size - i - 1];
      }
      // tt_ev->setRoute_path(path);
    } else {
      path = route_table[route_id];
      // tt_ev->setRoute_path(route_table[route_id]);
    }
    std::vector<int> current_coord(dimensions, 0);
    std::vector<int> first_path_coord(dimensions, 0);
    idToLocation(router_id, current_coord.data());
    idToLocation(path[0], first_path_coord.data());

    auto shift = [=](std::vector<int> point) {
      std::vector<int> new_coord(dimensions);
      for (int i = 0; i < dimensions; i++) {
        new_coord[i] =
            (point[i] + current_coord[i] - first_path_coord[i] + dim_size[i]) %
            dim_size[i];
      }
      return new_coord;
    };

    for (int i = 0; i < (int)path.size(); i++) {
      std::vector<int> path_coord(dimensions, 0);
      idToLocation(path[i], path_coord.data());
      path[i] = LocationToId(shift(path_coord).data());
    }
    tt_ev->setRoute_path(path);

  } else {
    tt_ev->clearRoute_path();
  }
  // Need to figure out what the torus address is for easier
  // routing.
  int run_id = get_dest_router(tt_ev->getDest());
  idToLocation(run_id, tt_ev->dest_loc);

  return tt_ev;
}

void topo_torus_trees::routeUntimedData(int port, internal_router_event *ev,
                                        std::vector<int> &outPorts) {
  if (ev->getDest() == UNTIMED_BROADCAST_ADDR) {
    /* For broadcast, use dest_loc as src_loc */
    topo_torus_trees_event *tt_ev = static_cast<topo_torus_trees_event *>(ev);
    /*
     * Find dimension came in on
     * Send in positive direction in all dimensions that level and higher
     * (unless at end)
     */
    int inc_dim = 0;
    for (; inc_dim < dimensions; inc_dim++) {
      if (port == port_start[inc_dim][1]) {
        break;
      }
    }
    if (inc_dim >= dimensions)
      inc_dim = 0; // A new message

    for (int dim = inc_dim; dim < dimensions; dim++) {
      if (((id_loc[dim] + 1) % dim_size[dim]) != tt_ev->dest_loc[dim]) {
        outPorts.push_back(port_start[dim][0]);
      }
    }

    // Also, send to hosts
    for (int p = 0; p < num_local_ports; p++) {
      if ((local_port_start + p) != port) {
        outPorts.push_back(local_port_start + p);
      }
    }

  } else {
    route_packet(port, 0, ev);
    outPorts.push_back(ev->getNextPort());
  }
}

internal_router_event *
topo_torus_trees::process_UntimedData_input(RtrEvent *ev) {
  topo_torus_trees_event *tt_ev = new topo_torus_trees_event(dimensions);
  tt_ev->setEncapsulatedEvent(ev);
  if (tt_ev->getDest() == UNTIMED_BROADCAST_ADDR) {
    /* For broadcast, use dest_loc as src_loc */
    for (int i = 0; i < dimensions; i++) {
      tt_ev->dest_loc[i] = id_loc[i];
    }
  } else {
    int rtr_id = get_dest_router(tt_ev->getDest());
    idToLocation(rtr_id, tt_ev->dest_loc);
  }
  return tt_ev;
}

Topology::PortState topo_torus_trees::getPortState(int port) const {
  if (port >= local_port_start) {
    if (port < (local_port_start + num_local_ports))
      return R2N;
    return UNCONNECTED;
  }
  return R2R;
}
std::string topo_torus_trees::getPortLogicalGroup(int port) const {
  if (port >= local_port_start && port < (local_port_start + num_local_ports)) {
    return "host"; // локальные порты к EmberEngine/NIC
  } else if (port < local_port_start) {
    return "torus"; // межроутерные линки
  } else {
    return "unconn"; // неиспользуемые порты
  }
}
void topo_torus_trees::idToLocation(int run_id, int *location) const {
  for (int i = dimensions - 1; i > 0; i--) {
    int div = 1;
    for (int j = 0; j < i; j++) {
      div *= dim_size[j];
    }
    int value = (run_id / div);
    // location[i] = value;
    location[dimensions - i - 1] = value;
    run_id -= (value * div);
  }
  // location[0] = run_id;
  location[dimensions - 1] = run_id;
}

int topo_torus_trees::LocationToId(int *location) const {
  int res = 0;
  int factor = 1;
  for (int i = dimensions - 1; i >= 0; i--) {
    res += location[i] * factor;
    factor *= dim_size[i];
  }
  return res;
}

void topo_torus_trees::parseDimString(const std::string &shape,
                                      int *output) const {
  size_t start = 0;
  size_t end = 0;
  for (int i = 0; i < dimensions; i++) {
    end = shape.find('x', start);
    size_t length = end - start;
    std::string sub = shape.substr(start, length);
    output[i] = strtol(sub.c_str(), NULL, 0);
    start = end + 1;
  }
}

int topo_torus_trees::get_dest_router(int dest_id) const {
  return dest_id / num_local_ports;
}

int topo_torus_trees::get_dest_local_port(int dest_id) const {
  return local_port_start + (dest_id % num_local_ports);
}

// int topo_torus_trees::choose_multipath(int start_port, int num_ports,
//                                        int dest_dist) {
//   if (num_ports == 1) {
//     return start_port;
//   } else {
//     return start_port + (dest_dist % num_ports);
//   }
// }

int topo_torus_trees::getEndpointID(int port) {
  if (!isHostPort(port))
    return -1;
  return (router_id * num_local_ports) + (port - local_port_start);
}
