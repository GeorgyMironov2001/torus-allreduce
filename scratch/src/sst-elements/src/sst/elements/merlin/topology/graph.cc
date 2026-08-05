#include <cstdio>
#include <sst_config.h>

#include "graph.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <queue>
#include <sstream>
#include <utility>

#include "sst/core/rng/xorshift.h"

using namespace SST::Merlin;

namespace {

std::string trim(const std::string &s) {
  size_t start = 0;
  while (start < s.size() &&
         std::isspace(static_cast<unsigned char>(s[start]))) {
    ++start;
  }
  size_t end = s.size();
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
    --end;
  }
  return s.substr(start, end - start);
}

bool parse_router_pair_key(const std::string &key, int &src, int &dst) {
  size_t comma = key.find(',');
  if (comma == std::string::npos) {
    return false;
  }
  try {
    src = std::stoi(trim(key.substr(0, comma)));
    dst = std::stoi(trim(key.substr(comma + 1)));
  } catch (const std::exception &) {
    return false;
  }
  return true;
}

bool looks_like_ecmp_paths(const nlohmann::json &j) {
  if (!j.is_object() || j.empty()) {
    return false;
  }
  for (const auto &entry : j.items()) {
    int src = -1;
    int dst = -1;
    if (!parse_router_pair_key(entry.key(), src, dst)) {
      return false;
    }
    if (!entry.value().is_array()) {
      return false;
    }
  }
  return true;
}

std::vector<int> json_to_path(const nlohmann::json &path_json) {
  if (!path_json.is_array()) {
    throw nlohmann::json::type_error::create(
        302, "ECMP path must be an array of router ids", &path_json);
  }
  std::vector<int> path;
  path.reserve(path_json.size());
  for (const auto &node : path_json) {
    path.push_back(static_cast<int>(node.get<int64_t>()));
  }
  return path;
}

void append_ecmp_paths_for_dst(std::vector<std::vector<int>> &paths,
                               const nlohmann::json &value) {
  if (value.is_array() && !value.empty() && value.front().is_number()) {
    paths.push_back(json_to_path(value));
    return;
  }
  if (!value.is_array()) {
    throw nlohmann::json::type_error::create(
        302, "ECMP entry must be an array of paths", &value);
  }
  for (const auto &path_json : value) {
    paths.push_back(json_to_path(path_json));
  }
}

} // namespace

void topo_graph::parse_route_table_array(const nlohmann::json &j) {
  int id = 0;
  for (const auto &entry : j) {
    std::pair<int, int> key = std::make_pair(entry[0][0], entry[0][1]);
    route_table_map[key] = id++;
    all_paths.push_back(json_to_path(entry[1]));
  }
}

void topo_graph::parse_ecmp_paths(const nlohmann::json &paths_obj) {
  if (!paths_obj.is_object()) {
    output.fatal(CALL_INFO, -1,
                 "ECMP route_table_file must contain an object of paths\n");
  }

  all_shortest_paths.assign(total_routers, std::vector<std::vector<int>>());

  for (const auto &entry : paths_obj.items()) {
    int src = -1;
    int dst = -1;
    if (!parse_router_pair_key(entry.key(), src, dst)) {
      continue;
    }
    if (src != router_id || dst == router_id) {
      continue;
    }
    if (dst < 0 || dst >= total_routers) {
      output.fatal(CALL_INFO, -1, "Invalid ECMP destination %d for router %d\n",
                   dst, router_id);
    }

    std::vector<std::vector<int>> paths;
    try {
      append_ecmp_paths_for_dst(paths, entry.value());
    } catch (const nlohmann::json::exception &e) {
      output.fatal(CALL_INFO, -1,
                   "Invalid ECMP paths for pair %s at router %d: %s\n",
                   entry.key().c_str(), router_id, e.what());
    }
    all_shortest_paths[dst] = std::move(paths);
  }
}

void topo_graph::parse_route_table(Params &params) {
  std::string route_table_file = params.find<std::string>("route_table_file");

  std::ifstream file(route_table_file);
  if (!file.is_open()) {
    output.fatal(CALL_INFO, -1, "Unable to open route_table_file: %s\n",
                 route_table_file.c_str());
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  std::string json_text = content;
  std::string first_line;
  {
    std::istringstream iss(content);
    std::getline(iss, first_line);
    first_line = trim(first_line);
  }

  if (first_line == "ecmp" || first_line == "ECMP") {
    use_ecmp_routing = true;
    size_t newline = content.find('\n');
    if (newline == std::string::npos) {
      output.fatal(CALL_INFO, -1,
                   "ECMP route_table_file must contain JSON after header\n");
    }
    json_text = content.substr(newline + 1);
  }

  nlohmann::json j;
  try {
    j = nlohmann::json::parse(json_text);
  } catch (const nlohmann::json::exception &e) {
    output.fatal(CALL_INFO, -1, "Failed to parse route_table_file: %s\n",
                 e.what());
  }

  if (j.is_array()) {
    parse_route_table_array(j);
    return;
  }

  if (!j.is_object()) {
    output.fatal(CALL_INFO, -1, "Unsupported route_table_file format\n");
  }

  if (j.contains("type")) {
    std::string route_type = j["type"].get<std::string>();
    if (route_type == "ecmp" || route_type == "ECMP") {
      use_ecmp_routing = true;
    }
  }

  if (j.contains("all_shortest_paths")) {
    use_ecmp_routing = true;
    parse_ecmp_paths(j["all_shortest_paths"]);
    return;
  }

  if (use_ecmp_routing) {
    parse_ecmp_paths(j);
    return;
  }

  if (looks_like_ecmp_paths(j)) {
    use_ecmp_routing = true;
    parse_ecmp_paths(j);
    return;
  }

  output.fatal(CALL_INFO, -1,
               "Unsupported route_table_file object format for router %d\n",
               router_id);
}

topo_graph::topo_graph(ComponentId_t cid, Params &params, int num_ports,
                       int rtr_id, int num_vns)
    : Topology(cid), router_id(rtr_id), num_ports(num_ports), num_vns(num_vns),
      use_ecmp_routing(false), rng(new RNG::XORShiftRNG(router_id + 1)) {
  graph_file = params.find<std::string>("graph_file");
  hosts_per_router = params.find<int>("hosts_per_router");
  total_routers = params.find<int>("total_routers", 0);
  num_vcs = params.find<int>("num_vcs", 1);

  if (graph_file.empty()) {
    output.fatal(CALL_INFO, -1, "graph topology requires graph_file\n");
  }
  if (hosts_per_router <= 0) {
    output.fatal(CALL_INFO, -1, "hosts_per_router must be positive\n");
  }
  if (num_vcs <= 0) {
    output.fatal(CALL_INFO, -1, "num_vcs must be positive\n");
  }

  loadGraph();

  if (router_id < 0 || router_id >= total_routers) {
    output.fatal(CALL_INFO, -1,
                 "router id %d is outside graph with %d routers\n", router_id,
                 total_routers);
  }

  int needed_ports = hosts_per_router + adjacency[router_id].size();
  if (num_ports < needed_ports) {
    output.fatal(CALL_INFO, -1, "Router %d needs at least %d ports, got %d\n",
                 router_id, needed_ports, num_ports);
  }

  neighbor_to_port.assign(total_routers, -1);
  for (size_t i = 0; i < adjacency[router_id].size(); ++i) {
    neighbor_to_port[adjacency[router_id][i]] = hosts_per_router + i;
  }
  auto route_table_file = params.find<std::string>("route_table_file", "");
  if (!route_table_file.empty()) {
    parse_route_table(params);
  }
  initRouteTable();
  initBroadcastTree();

  const char *routing_mode = "bfs";
  if (!route_table_file.empty()) {
    routing_mode = use_ecmp_routing ? "ecmp" : "routing_table";
  }
  if (router_id == 0) {
    output.output(
        "graph topology: routing=%s graph_file=%s route_table_file=%s\n",
        routing_mode, graph_file.c_str(),
        route_table_file.empty() ? "<none>" : route_table_file.c_str());
  }
}

topo_graph::~topo_graph() { delete rng; }

void topo_graph::loadGraph() {
  std::ifstream fp(graph_file.c_str());
  if (!fp.is_open()) {
    output.fatal(CALL_INFO, -1, "Unable to open graph_file: %s\n",
                 graph_file.c_str());
  }

  std::string line;
  int expected_edges = -1;
  bool header_seen = false;
  std::vector<std::pair<int, int>> edges;

  while (std::getline(fp, line)) {
    std::string trimmed = line;
    size_t comment = trimmed.find('#');
    if (comment != std::string::npos) {
      trimmed = trimmed.substr(0, comment);
    }

    std::istringstream iss(trimmed);
    int u, v;
    if (!(iss >> u >> v)) {
      continue;
    }

    if (!header_seen) {
      total_routers = (total_routers > 0) ? total_routers : u;
      expected_edges = v;
      header_seen = true;
      continue;
    }

    if (u < 0 || v < 0 || u >= total_routers || v >= total_routers) {
      output.fatal(CALL_INFO, -1,
                   "Invalid edge (%d, %d) for graph with %d routers\n", u, v,
                   total_routers);
    }
    if (u == v) {
      continue;
    }

    if (u > v)
      std::swap(u, v);
    edges.push_back(std::make_pair(u, v));
  }

  if (!header_seen || total_routers <= 0) {
    output.fatal(CALL_INFO, -1,
                 "graph_file must start with '<num_routers> <num_edges>'\n");
  }

  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

  if (expected_edges >= 0 && expected_edges != static_cast<int>(edges.size())) {
    output.verbose(
        CALL_INFO, 1, 0,
        "graph_file declared %d edges, loaded %zu unique non-self edges\n",
        expected_edges, edges.size());
  }

  adjacency.assign(total_routers, std::vector<int>());
  for (auto const &edge : edges) {
    adjacency[edge.first].push_back(edge.second);
    adjacency[edge.second].push_back(edge.first);
  }

  for (auto &neighbors : adjacency) {
    std::sort(neighbors.begin(), neighbors.end());
  }
}

void topo_graph::initRouteTable() {
  route_table.assign(total_routers, -1);

  std::vector<int> seen(total_routers, 0);
  std::queue<int> q;

  seen[router_id] = 1;
  q.push(router_id);

  while (!q.empty()) {
    int cur = q.front();
    q.pop();

    for (int next : adjacency[cur]) {
      if (seen[next])
        continue;

      seen[next] = 1;
      if (cur == router_id) {
        route_table[next] = getPortForNeighbor(next);
      } else {
        route_table[next] = route_table[cur];
      }
      q.push(next);
    }
  }

  for (int dst = 0; dst < total_routers; ++dst) {
    if (dst == router_id)
      continue;
    if (route_table[dst] < 0) {
      output.fatal(CALL_INFO, -1, "No route from router %d to router %d\n",
                   router_id, dst);
    }
  }
}

void topo_graph::initBroadcastTree() {
  broadcast_parent.assign(total_routers, -1);
  broadcast_children.assign(total_routers, std::vector<int>());

  std::vector<int> seen(total_routers, 0);
  std::queue<int> q;

  seen[0] = 1;
  q.push(0);

  while (!q.empty()) {
    int cur = q.front();
    q.pop();

    for (int next : adjacency[cur]) {
      if (seen[next])
        continue;

      seen[next] = 1;
      broadcast_parent[next] = cur;
      broadcast_children[cur].push_back(next);
      q.push(next);
    }
  }
}
void topo_graph::route_packet(int port, int vc, internal_router_event *ev) {
  int dest_router = getRouterID(ev->getDest());
  if (dest_router == router_id) {
    topo_graph_init_event *graph_ev = static_cast<topo_graph_init_event *>(ev);
    if (!graph_ev->route_path.empty()) {
      const auto &path = graph_ev->route_path;

      output.verbose(CALL_INFO, 1, 0,
                     "graph route_path: router=%d dest_router=%d path[0]=%d "
                     "path[-1]=%d\n",
                     router_id, dest_router, path[0], path[path.size()-1]);
      fflush(stdout);
      if (path[0]==63 && path[path.size()-1]==3) {
        int q = 1;
      }
    }
    ev->setNextPort(getDestLocalPort(ev->getDest()));
    ev->setVC(0);
    return;
  }

  topo_graph_init_event *graph_ev = static_cast<topo_graph_init_event *>(ev);
  if (graph_ev->route_path.empty()) {
    route_packet_bfs(port, vc, ev);
    return;
  }

  assert(router_id == graph_ev->route_path[graph_ev->route_port_id]);
  auto &path = graph_ev->route_path;
  int next_router = path[graph_ev->route_port_id + 1];
  graph_ev->setNextPort(getPortForNeighbor(next_router));
  graph_ev->route_port_id++;
  // graph_ev->setVC(vc);
}

void topo_graph::route_packet_bfs(int port, int vc, internal_router_event *ev) {
  int dest_router = getRouterID(ev->getDest());

  if (dest_router < 0 || dest_router >= total_routers) {
    output.fatal(CALL_INFO, -1, "Invalid destination endpoint %d\n",
                 ev->getDest());
  }

  if (dest_router == router_id) {
    ev->setNextPort(getDestLocalPort(ev->getDest()));
    ev->setVC(0);
    return;
  }

  ev->setNextPort(route_table[dest_router]);
  // ev->setVC(vc==0?1:0);
  // ev->setVC(vc);
}

internal_router_event *topo_graph::process_input(RtrEvent *ev) {
  topo_graph_init_event *graph_ev = new topo_graph_init_event();
  graph_ev->setEncapsulatedEvent(ev);

  int route_id = ev->getRoute_id();
  int dest_router = getRouterID(ev->getDest());

  if (use_ecmp_routing) {
    if (dest_router != router_id) {
      const auto &paths = all_shortest_paths[dest_router];
      if (paths.empty()) {
        output.fatal(CALL_INFO, -1,
                     "No ECMP paths from router %d to router %d\n", router_id,
                     dest_router);
      }
      if (route_id >= 0 && route_id < static_cast<int>(paths.size())) {
        graph_ev->setRoute_path(paths[route_id]);
      } else {
        graph_ev->clearRoute_path();
      }
    } else {
      graph_ev->clearRoute_path();
    }
  } else if (route_id >= 0) {
    graph_ev->setRoute_path(all_paths[route_id]);
  } else {
    graph_ev->clearRoute_path();
  }

  // graph_ev->setVC(graph_ev->getVN() * num_vcs);
  graph_ev->setVC(0);
  return graph_ev;
}

void topo_graph::routeUntimedData(int port, internal_router_event *ev,
                                  std::vector<int> &outPorts) {
  if (ev->getDest() == UNTIMED_BROADCAST_ADDR) {
    topo_graph_init_event *graph_ev = static_cast<topo_graph_init_event *>(ev);

    if (graph_ev->phase == 0 && router_id != 0) {
      outPorts.push_back(getPortForNeighbor(broadcast_parent[router_id]));
      return;
    }

    graph_ev->phase = 1;

    for (int p = 0; p < hosts_per_router; ++p) {
      if (p != port)
        outPorts.push_back(p);
    }

    for (int child : broadcast_children[router_id]) {
      int child_port = getPortForNeighbor(child);
      if (child_port != port)
        outPorts.push_back(child_port);
    }
    return;
  }

  route_packet(port, 0, ev);
  outPorts.push_back(ev->getNextPort());
}

internal_router_event *topo_graph::process_UntimedData_input(RtrEvent *ev) {
  return new topo_graph_init_event(ev);
}

Topology::PortState topo_graph::getPortState(int port) const {
  if (port >= 0 && port < hosts_per_router)
    return R2N;
  if (port >= hosts_per_router &&
      port < hosts_per_router + static_cast<int>(adjacency[router_id].size()))
    return R2R;
  return UNCONNECTED;
}

std::string topo_graph::getPortLogicalGroup(int port) const {
  if (port >= 0 && port < hosts_per_router) {
    return "host";
  } else if (port >= hosts_per_router &&
             port < hosts_per_router +
                        static_cast<int>(adjacency[router_id].size())) {
    return "graph";
  } else {
    return "unconn";
  }
}

int topo_graph::getEndpointID(int port) {
  if (port < 0 || port >= hosts_per_router)
    return -1;
  return router_id * hosts_per_router + port;
}

int topo_graph::getRouterID(int endpoint) const {
  return endpoint / hosts_per_router;
}

int topo_graph::getDestLocalPort(int endpoint) const {
  return endpoint % hosts_per_router;
}

int topo_graph::getPortForNeighbor(int neighbor) const {
  if (neighbor < 0 || neighbor >= total_routers ||
      neighbor_to_port[neighbor] < 0) {
    output.fatal(CALL_INFO, -1, "Router %d has no port for neighbor %d\n",
                 router_id, neighbor);
  }
  return neighbor_to_port[neighbor];
}
