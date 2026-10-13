#include "embertreescoll.h"
#include "embershortmsgcheck.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits.h>
#include <sst/core/rng/xorshift.h>
#include <sst_config.h>
#include <tuple>
#include <utility>
// #define DEBUG

#define NO_PEER -1
#define MAX_SUPPORTED_DIMENSIONS 8 // We support up to 8D torus

using namespace SST::Ember;
using namespace SST::RNG;

extern "C" {
#include "hxmesh_dims.h"
}

#ifdef DEBUG
#define DPRINTF(...) printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

static int mod(int a, int b) {
  int r = a % b;
  return r < 0 ? r + b : r;
}

// static std::pair<int, int> up(std::pair<int, int> coord, int n) {
//   auto [x, y] = coord;
//   return std::make_pair(x, mod(y + 1, n));
// }
// static std::pair<int, int> down(std::pair<int, int> coord, int n) {
//   auto [x, y] = coord;
//   return std::make_pair(x, mod(y - 1, n));
// }
// static std::pair<int, int> left(std::pair<int, int> coord, int n) {
//   auto [x, y] = coord;
//   return std::make_pair(mod(x - 1, n), y);
// }
// static std::pair<int, int> right(std::pair<int, int> coord, int n) {
//   auto [x, y] = coord;
//   return std::make_pair(mod(x + 1, n), y);
// }
static uint getRankFromCoord(const std::vector<int> &coord, int n, int d) {
  int res = 0;
  int factor = 1;
  for (int i = 0; i < d; i++) {
    res += coord[i] * factor;
    factor *= n;
  }
  return res;
  // return coord.first * n + coord.second;
}

static int getRankFromCoord(const std::vector<int> &coords, uint *dimensions,
                            uint dimensions_num) {
  int res = 0;
  int factor = 1;
  for (int i = 0; i < dimensions_num; i++) {
    res += coords[i] * factor;
    factor *= dimensions[i];
  }
  return res;
}
static std::vector<int> getCoordFromRank(int rank, int n, int d = 2) {
  std::vector<int> coord(d);
  for (int i = d - 1; i > 0; i--) {
    int div = 1;
    for (int j = 0; j < i; ++j) {
      div *= n;
    }
    int value = (rank / div);
    coord[i] = value;
    rank -= (value * div);
  }
  coord[0] = rank;
  return coord;
}

static std::vector<int> getCoordFromRank(int rank, uint *dimensions,
                                         uint dimensions_num) {
  std::vector<int> coord(dimensions_num);
  for (int i = dimensions_num - 1; i > 0; i--) {
    int div = 1;
    for (int j = 0; j < i; ++j) {
      div *= dimensions[j];
    }
    int value = (rank / div);
    coord[i] = value;
    rank -= (value * div);
  }
  coord[0] = rank;
  return coord;

  // int nnodes = 1;
  // for (int i = 0; i < dimensions_num; i++) {
  //   nnodes *= dimensions[i];
  // }
  // for (int i = 0; i < dimensions_num; i++) {
  //   nnodes = nnodes / dimensions[i];
  //   coord[i] = rank / nnodes;
  //   rank = rank % nnodes;
  // }
  // return coord;
}
static int getMainCenterId(int n, int d = 2) {
  std::vector<int> coord(d);
  for (int i = 0; i < d; i++) {
    coord[i] = (n - 1) / 2;
  }
  return getRankFromCoord(coord, n, d);
}
static int getMainCenterId(uint *dimensions, uint dimensions_num) {
  std::vector<int> coord(dimensions_num);
  for (int i = 0; i < dimensions_num; i++) {
    coord[i] = (dimensions[i] - 1) / 2;
  }
  return getRankFromCoord(coord, dimensions, dimensions_num);
}
// static int getDistanceManhattan(std::pair<int, int> coord1,
//                                 std::pair<int, int> coord2, int n) {
//   auto [x, y] = coord1;
//   auto [Ox, Oy] = coord2;
//   int x_diff =
//       mod(x - Ox, n) > mod(Ox - x, n) ? mod(Ox - x, n) : -mod(x - Ox, n);
//   int y_diff =
//       mod(y - Oy, n) > mod(Oy - y, n) ? mod(Oy - y, n) : -mod(y - Oy, n);
//   return std::abs(x_diff) + std::abs(y_diff);
// }
// static int getDistanceScatter(std::pair<int, int> m_r,
//                               std::pair<int, int> center, int n) {
//   auto [x, y] = m_r;
//   auto [Ox, Oy] = center;
//   if (x == Ox && y == Oy) {
//     return n;
//   }
//   int x_diff =
//       mod(x - Ox, n) > mod(Ox - x, n) ? mod(Ox - x, n) : -mod(x - Ox, n);
//   int y_diff =
//       mod(y - Oy, n) > mod(Oy - y, n) ? mod(Oy - y, n) : -mod(y - Oy, n);
//   int m = (n - 1) / 2;

//   if (x_diff >= 0 && y_diff < 0) {
//     return x_diff == 0 ? n + y_diff : m + 1 - x_diff;
//   } else if (x_diff < 0 && y_diff <= 0) {
//     return y_diff == 0 ? n + x_diff : m + 1 + y_diff;
//   } else if (x_diff <= 0 && y_diff > 0) {
//     return x_diff == 0 ? n - y_diff : m + 1 + x_diff;
//   } else if (x_diff > 0 && y_diff >= 0) {
//     return y_diff == 0 ? n - x_diff : m + 1 - y_diff;
//   }
// }

static void fillCoordFromRank(int rank, const uint *dimensions, int D,
                              int *coord) {
  for (int i = D - 1; i > 0; i--) {
    int div = 1;
    for (int j = 0; j < i; ++j) {
      div *= (int)dimensions[j];
    }
    int value = rank / div;
    coord[i] = value;
    rank -= value * div;
  }
  coord[0] = rank;
}

static int rankFromCoord(const int *coords, const uint *dimensions, int D) {
  int res = 0;
  int factor = 1;
  for (int i = 0; i < D; i++) {
    res += coords[i] * factor;
    factor *= (int)dimensions[i];
  }
  return res;
}

static int shiftRankByDelta(int node, const int *delta, const uint *dimensions,
                            int D) {
  int coord[MAX_SUPPORTED_DIMENSIONS];
  fillCoordFromRank(node, dimensions, D, coord);
  for (int i = 0; i < D; i++) {
    coord[i] = (coord[i] + delta[i]) % (int)dimensions[i];
  }
  return rankFromCoord(coord, dimensions, D);
}

std::pair<std::vector<TreeEdge>, std::vector<TreeEdge>>
TreeSpec::get_peers(int center_id, int rank_id) const {
  const int D = this->dimension;
  const uint *dimensions = this->dimensions.data();

  int r_coord[MAX_SUPPORTED_DIMENSIONS];
  int C_coord[MAX_SUPPORTED_DIMENSIONS];
  int O_coord[MAX_SUPPORTED_DIMENSIONS];
  fillCoordFromRank(rank_id, dimensions, D, r_coord);
  fillCoordFromRank(center_id, dimensions, D, C_coord);
  fillCoordFromRank(center, dimensions, D, O_coord);

  // shift(x) = x + C - O; shift_back(x) = x - C + O
  int delta[MAX_SUPPORTED_DIMENSIONS];
  int delta_back[MAX_SUPPORTED_DIMENSIONS];
  for (int i = 0; i < D; i++) {
    const int dim = (int)dimensions[i];
    delta[i] = (C_coord[i] - O_coord[i] + dim) % dim;
    delta_back[i] = (O_coord[i] - C_coord[i] + dim) % dim;
  }

  int shifted_back[MAX_SUPPORTED_DIMENSIONS];
  for (int i = 0; i < D; i++) {
    shifted_back[i] = (r_coord[i] + delta_back[i]) % (int)dimensions[i];
  }
  const int rank = rankFromCoord(shifted_back, dimensions, D);

  std::vector<TreeEdge> new_inside_peers = inside_peers[rank];
  std::vector<TreeEdge> new_outside_peers = outside_peers[rank];
  for (auto &e : new_inside_peers) {
    e.from = shiftRankByDelta(e.from, delta, dimensions, D);
    e.to = shiftRankByDelta(e.to, delta, dimensions, D);
  }
  for (auto &e : new_outside_peers) {
    e.from = shiftRankByDelta(e.from, delta, dimensions, D);
    e.to = shiftRankByDelta(e.to, delta, dimensions, D);
  }
  return {std::move(new_inside_peers), std::move(new_outside_peers)};
}
int sign(int x) { return x > 0 ? 1 : x < 0 ? -1 : 0; }

bool same_direction(int from, int to, int port_id, uint *dimensions,
                    uint dimensions_num) {
  // return port_id == to;
  if (dimensions_num != 2) {
    return port_id == to;
  }
  auto [fx, fy] =
      std::array{getCoordFromRank(from, dimensions, dimensions_num)[0],
                 getCoordFromRank(from, dimensions, dimensions_num)[1]};
  auto [tx, ty] =
      std::array{getCoordFromRank(to, dimensions, dimensions_num)[0],
                 getCoordFromRank(to, dimensions, dimensions_num)[1]};
  auto [px, py] =
      std::array{getCoordFromRank(port_id, dimensions, dimensions_num)[0],
                 getCoordFromRank(port_id, dimensions, dimensions_num)[1]};
  if (!((fx == tx && tx == px) || (fy == ty && ty == py))) {
    return false;
  }
  if (fx == tx && tx == px) {
    int to_vec = ((ty - fy + dimensions[1]) % dimensions[1] >
                  (fy - ty + dimensions[1]) % dimensions[1])
                     ? -1
                     : 1;
    int port_vec = ((py - fy + dimensions[1]) % dimensions[1] >
                    (fy - py + dimensions[1]) % dimensions[1])
                       ? -1
                       : 1;
    return to_vec == port_vec;
  }
  if (fy == ty && ty == py) {
    int to_vec = ((tx - fx + dimensions[0]) % dimensions[0] >
                  (fx - tx + dimensions[0]) % dimensions[0])
                     ? -1
                     : 1;
    int port_vec = ((px - fx + dimensions[0]) % dimensions[0] >
                    (fx - px + dimensions[0]) % dimensions[0])
                       ? -1
                       : 1;
    return to_vec == port_vec;
  }
  return false;
}
bool same_direction(int from, int to, int port_id, int n, int d) {

  // return port_id == to;
  if (d != 2) {
    return port_id == to;
  }

  auto [fx, fy] = std::array{getCoordFromRank(from, n, d)[0],
                             getCoordFromRank(from, n, d)[1]};
  auto [tx, ty] =
      std::array{getCoordFromRank(to, n, d)[0], getCoordFromRank(to, n, d)[1]};
  auto [px, py] = std::array{getCoordFromRank(port_id, n, d)[0],
                             getCoordFromRank(port_id, n, d)[1]};
  if (!((fx == tx && tx == px) || (fy == ty && ty == py))) {
    return false;
  }
  if (fx == tx && tx == px) {
    int to_vec = ((ty - fy + n) % n > (fy - ty + n) % n) ? -1 : 1;
    int port_vec = ((py - fy + n) % n > (fy - py + n) % n) ? -1 : 1;
    return to_vec == port_vec;
  }
  if (fy == ty && ty == py) {
    int to_vec = ((tx - fx + n) % n > (fx - tx + n) % n) ? -1 : 1;
    int port_vec = ((px - fx + n) % n > (fx - px + n) % n) ? -1 : 1;
    return to_vec == port_vec;
  }
  return false;
}

std::vector<int>
EmberTreesCollGenerator::TreesCollectiveEngine::getRouteTablePath(
    int from, int to, int route_class) {
  if (route_class == -1) {
    return std::vector<int>();
  }
  int D = m_dimensions_num;
  auto &dimensions = m_dimensions;
  int n = m_dimensions[0];
  // auto from_coord = getCoordFromRank(from, n, D);
  auto from_coord = getCoordFromRank(from, dimensions, D);

  auto &etalon_path = route_table[route_class];
  // auto etalon_start_coord = getCoordFromRank(etalon_path[0], n, D);
  auto etalon_start_coord = getCoordFromRank(etalon_path[0], dimensions, D);

  auto shift = [=](std::vector<int> point) {
    std::vector<int> new_coord(D);
    for (int i = 0; i < D; i++) {
      new_coord[i] =
          (point[i] - etalon_start_coord[i] + from_coord[i] + dimensions[i]) %
          dimensions[i];
    }
    return new_coord;
  };
  std::vector<int> shifted_path(etalon_path.size());
  for (int i = 0; i < etalon_path.size(); i++) {
    // shifted_path[i] =
    //     getRankFromCoord(shift(getCoordFromRank(etalon_path[i], n, D)), n,
    //     D);
    shifted_path[i] = getRankFromCoord(
        shift(getCoordFromRank(etalon_path[i], dimensions, D)), dimensions, D);
  }
  return shifted_path;
}

EmberTreesCollGenerator::TreesCollectiveEngine::TreesCollectiveEngine(
    EmberTreesCollGenerator &gen, CollType coll_type, uint *dimensions,
    uint dimensions_num, float *dst, uint32_t count, uint32_t vrank,
    uint32_t numproc, double aggregation_cost_ns, Communicator comm,
    bool validate, bool latency_optimal, int port_id, TreesCollective *runner,
    std::vector<std::vector<int>> &route_table,
    std::map<std::pair<int, int>, int> &route_table_map)
    : m_gen(gen), m_count(count), m_dst(dst), m_r(vrank), m_p(numproc),
      m_aggregation_cost_ns(aggregation_cost_ns), m_data_sent(0), m_comm(comm),
      m_dimensions(dimensions), m_dimensions_num(dimensions_num),
      m_validate(validate), m_enabled(true), m_latency_optimal(latency_optimal),
      m_port_id(port_id), m_runner(runner), m_data_reduced(0),
      route_table(route_table), route_table_map(route_table_map) {

  uint32_t block_size = (m_count >= m_p) ? m_count / m_p : m_count;

  int D = m_dimensions_num;
  int n = m_dimensions[0]; // assuming torus (2m+1)X(2m+1)
  // m_stages_num = (n % 2 == 1) ? n - 1 : (n / 2) * D;
  m_stages_num = 0;
  for (const auto &edge : m_runner->allreduce_trees[0].edges) {
    m_stages_num = std::max(m_stages_num, edge.rsStage + 1);
  }
  assert(!m_latency_optimal || m_stages_num == 1);
  m_do_reduce_scatter = (coll_type != TREES_ALLGATHER);
  m_do_allgather =
      (coll_type == TREES_ALLGATHER) ||
      (coll_type == TREES_ALLREDUCE && m_count >= m_p && !m_latency_optimal);
  // Latency L1: one port-engine owns all peers (star edges are not always
  // axis-aligned, so same_direction would drop many).
  // if (m_latency_optimal) {
  //   auto oc = getCoordFromRank(m_r, m_dimensions, D);
  //   oc[0] = (oc[0] + 1) % m_dimensions[0];
  //   if (m_port_id != getRankFromCoord(oc, m_dimensions, D))
  //     m_enabled = false;
  // }
  int m = (n - 1) / 2;

  // auto m_coord = getCoordFromRank(m_r, n, D);
  auto m_coord = getCoordFromRank(m_r, m_dimensions, D);

  scatter_peers_send.resize(m_stages_num);
  scatter_peers_recv.resize(m_stages_num);
  allgather_peers_send.resize(m_stages_num);
  allgather_peers_recv.resize(m_stages_num);

  scatter_port_send.resize(m_stages_num);
  scatter_port_recv.resize(m_stages_num);
  allgather_port_send.resize(m_stages_num);
  allgather_port_recv.resize(m_stages_num);

  // Peers are precomputed once per rank in TreesCollective::peers_cache
  // (independent of port_id) and reused here + in phase-2 loops below.
  const auto &peers_cache = m_runner->peers_cache;
  const int num_trees = (int)m_runner->allreduce_trees.size();

  for (int center_id = 0; center_id < (int)m_p; center_id++) {
    if (m_r == 27 && center_id == 27) {
      int q = 1;
    }
    for (int id = 0; id < num_trees; id++) {
      int center_block_id = center_id * num_trees + id;
      const auto &inside_peers = peers_cache[center_block_id].first;
      const auto &outside_peers = peers_cache[center_block_id].second;
      for (const auto &e : inside_peers) {
        int route_class = e.route_class;
        scatter_peers_send[e.rsStage][std::make_pair(e.to, route_class)]
            .push_back(center_block_id);
        allgather_peers_recv[e.agStage][std::make_pair(e.to, route_class)]
            .push_back(center_block_id);

        int rid = route_class;
        std::vector<int> path_shifted =
            getRouteTablePath(e.from, e.to, route_class);
        const bool take = (rid == -1 && same_direction(e.from, e.to, m_port_id,
                                                       m_dimensions, D)) ||
                          (rid != -1 && m_port_id == path_shifted[1]);
        if (take) {
          scatter_port_send[e.rsStage][std::make_pair(e.to, route_class)]
              .push_back(center_block_id);
        }

        if (outside_peers.empty() && take) {
          allgather_port_recv[e.agStage][std::make_pair(e.to, route_class)]
              .push_back(center_block_id);
        }
      }
      for (const auto &e : outside_peers) {
        int route_class = e.route_class;
        scatter_peers_recv[e.rsStage][std::make_pair(e.from, route_class)]
            .push_back(center_block_id);
        allgather_peers_send[e.agStage][std::make_pair(e.from, route_class)]
            .push_back(center_block_id);
        if (inside_peers.empty()) {
          scatter_port_recv[e.rsStage][std::make_pair(e.from, route_class)]
              .push_back(center_block_id);
        }

        int rid = route_class;
        std::vector<int> path_shifted =
            getRouteTablePath(e.from, e.to, route_class);
        const bool take =
            (rid == -1 &&
             same_direction(e.to, e.from, m_port_id, m_dimensions, D)) ||
            (rid != -1 && m_port_id == path_shifted[path_shifted.size() - 2]);
        if (take) {
          allgather_port_send[e.agStage][std::make_pair(e.from, route_class)]
              .push_back(center_block_id);
        }
      }
    }
  }

  for (int stage = 0; stage < m_stages_num - 1; ++stage) {
    for (auto &[neighbour_id, center_block_ids] : scatter_peers_recv[stage]) {
      for (int center_block_id : center_block_ids) {
        const auto &inside_peers = peers_cache[center_block_id].first;
        for (const auto &e : inside_peers) {
          int rid = e.route_class;
          std::vector<int> path_shifted = getRouteTablePath(e.from, e.to, rid);
          const bool take =
              (rid == -1 &&
               same_direction(e.from, e.to, m_port_id, m_dimensions, D)) ||
              (rid != -1 && m_port_id == path_shifted[1]);
          if (take) {
            scatter_port_recv[stage][neighbour_id].push_back(center_block_id);
            break;
          }
        }
      }
    }
  }

  for (int stage = 0; stage < m_stages_num - 1; ++stage) {
    for (auto &[neighbour_id, center_block_ids] : allgather_peers_recv[stage]) {
      for (int center_block_id : center_block_ids) {
        const auto &outside_peers = peers_cache[center_block_id].second;
        for (const auto &e : outside_peers) {
          int rid = e.route_class;
          std::vector<int> path_shifted = getRouteTablePath(e.from, e.to, rid);
          const bool take =
              (rid == -1 &&
               same_direction(e.to, e.from, m_port_id, m_dimensions, D)) ||
              (rid != -1 && m_port_id == path_shifted[path_shifted.size() - 2]);
          if (take) {
            allgather_port_recv[stage][neighbour_id].push_back(center_block_id);
            break;
          }
        }
      }
    }
  }

  auto sort_centers = [](auto &port_map) {
    // for (auto& [stage, ports_centers] : port_map) {
    for (int stage = 0; stage < port_map.size(); stage++) {
      auto &ports_centers = port_map[stage];
      for (const auto &[port, _] : ports_centers) {
        std::sort(ports_centers[port].begin(), ports_centers[port].end());
      }
    }
  };

  sort_centers(scatter_port_send), sort_centers(scatter_peers_send);
  sort_centers(allgather_port_send), sort_centers(allgather_peers_send);
  sort_centers(scatter_port_recv), sort_centers(scatter_peers_recv);
  sort_centers(allgather_port_recv), sort_centers(allgather_peers_recv);
  if (m_r == 11) {
    int q = 1;
  }
  reset();
}

void EmberTreesCollGenerator::TreesCollectiveEngine::setEnable(bool enable) {
  m_enabled = enable;
}

bool EmberTreesCollGenerator::TreesCollectiveEngine::isEnabled() {
  return m_enabled;
}

bool EmberTreesCollGenerator::TreesCollectiveEngine::progress(
    std::queue<EmberEvent *> &evQ) {
  if (m_r == 1 && m_state == ALL_GATHER) {
    int q = 1;
  }
  switch (m_state) {
  case REDUCE_SCATTER:
    if (!m_do_reduce_scatter || collective(evQ, TREES_REDUCE_SCATTER)) {
      m_state = ALL_GATHER;
      // return false;
    } else {
      return false;
    }
  case ALL_GATHER:
    if (!m_do_allgather || collective(evQ, TREES_ALLGATHER)) {
      m_state = FINI;
      // return false;
    } else {
      return false;
    }
  case FINI:
    return true;
  default:
    assert(0);
  }
  assert(0);
}

uint64_t EmberTreesCollGenerator::TreesCollectiveEngine::getMovedBytes() {
  return m_data_sent;
}

void EmberTreesCollGenerator::TreesCollectiveEngine::setBuff(float *new_dest) {
  m_dst = new_dest;
}

float *EmberTreesCollGenerator::TreesCollectiveEngine::getBuff() {
  return m_dst;
}

bool EmberTreesCollGenerator::TreesCollectiveEngine::hasPendingRecv() {
  if (m_i == 0) {
    return false;
  }
  int n = m_dimensions[0];
  // int stages_num = n % 2 == 1 ? n - 1 : n;
  int global_stage = m_i + (m_state == REDUCE_SCATTER ? 0 : m_stages_num);
  for (int stage = 0; stage < global_stage; stage++) {
    for (auto &req : m_req_recv[stage]) {
      if (req != 0) {
        return true;
      }
    }
  }
  return false;
  // return waiting_receive() && m_ready_to_recv;
}
bool EmberTreesCollGenerator::TreesCollectiveEngine::hasPendingSend() {
  return waiting_send() && m_ready_to_send;
}

std::pair<std::vector<MessageRequest>, std::vector<std::pair<int, int>>>
EmberTreesCollGenerator::TreesCollectiveEngine::getRecvHandle() {
  std::vector<MessageRequest> active_received_handlers(0);
  std::vector<std::pair<int, int>> active_received_handlers_ids(0);
  int n = m_dimensions[0];
  // int stages_num = n % 2 == 1 ? n - 1 : n;
  int global_stage = m_i + (m_state == REDUCE_SCATTER ? 0 : m_stages_num);
  for (int stage = 0; stage < global_stage; stage++) {
    for (int i = 0; i < m_req_recv[stage].size(); i++) {
      // if (m_waiting_recv[i] && m_req_recv[i] != 0) {
      //   active_received_handlers.push_back(m_req_recv[i]);
      //   active_received_handlers_ids.push_back(i);
      // }
      if (m_req_recv[stage][i] != 0) {
        active_received_handlers.push_back(m_req_recv[stage][i]);
        active_received_handlers_ids.push_back(std::make_pair(stage, i));
      }
    }
  }
  return std::make_pair(active_received_handlers, active_received_handlers_ids);
}

// std::pair<std::vector<MessageRequest>, std::vector<int>>
// EmberTreesCollGenerator::TreesCollectiveEngine::getSendHandle() {
//   std::vector<MessageRequest> active_sent_handlers(0);
//   std::vector<int> active_sent_handlers_ids(0);
//   for (int i = 0; i < m_req_send.size(); i++) {
//     if (m_waiting_send[i] && m_req_send[i] != 0) {
//       active_sent_handlers.push_back(m_req_send[i]);
//       active_sent_handlers_ids.push_back(i);
//     }
//   }
//   return std::make_pair(active_sent_handlers, active_sent_handlers_ids);
// }
void EmberTreesCollGenerator::TreesCollectiveEngine::reset() {
  recv_scatter_started = 0, send_scatter_started = 0,
  recv_allgather_started = 0, send_allgather_started = 0;
  m_ready_to_recv = false;
  m_ready_to_send = false;
  m_i = 0;
  m_state = REDUCE_SCATTER;
  // m_tag_reduce = m_gen.getNextTag();
  // m_tag_broadcast = m_gen.getNextTag();
  m_tag_reduce = 0;
  m_tag_broadcast = 1;
  int n = m_dimensions[0]; // assuming torus (2m+1)X(2m+1)
  // int stages_num = n % 2 == 1 ? n - 1 : n;
  m_req_recv.resize(2 * m_stages_num);
  m_req_send.resize(2 * m_stages_num);
  m_recv_epoch.resize(2 * m_stages_num);
  // m_req_recv.assign(4, MessageRequest());
  // m_req_send.assign(4, MessageRequest());
}

void EmberTreesCollGenerator::TreesCollectiveEngine::notifyRecv() {
  // m_waiting_recv.assign(m_waiting_recv.size(), false);
  // m_ready_to_recv = false;
  // // m_req_recv = 0;
  // m_req_recv.assign(m_req_recv.size(), MessageRequest());
}
void EmberTreesCollGenerator::TreesCollectiveEngine::notifyRecv(
    std::pair<int, int> chunk, std::queue<EmberEvent *> &evQ) {
  assert(m_i > 0);
  int n = m_dimensions[0];
  // int stages_num = n % 2 == 1 ? n - 1 : n;
  auto [chunk_stage, chunk_id] = chunk;
  CollType coll_type =
      chunk_stage < m_stages_num ? TREES_REDUCE_SCATTER : TREES_ALLGATHER;
  auto &recv_peers = coll_type == TREES_REDUCE_SCATTER ? scatter_peers_recv
                                                       : allgather_peers_recv;

  if (m_r == 9 && chunk_stage == 6) {
    int q = 1;
  }
  // int global_stage =
  //     chunk_stage + (coll_type == TORUS_REDUCE_SCATTER ? 0 : n - 1);
  // m_waiting_recv[chunk_id] = false;
  if (m_r == 5 && m_port_id == 0 && chunk_stage == 2) {
    int q = 1;
  }
  if (m_r == 0 && m_port_id == 4 && chunk_stage == 1) {
    int q = 1;
  }
  int stage =
      (chunk_stage < m_stages_num ? chunk_stage : chunk_stage - (m_stages_num));
  int prev_peer_counter = 0;
  for (auto &[prev_peer, prev_centers] : recv_peers[stage]) {
    int prev_peer_id = prev_peer.first;
    int route_class = prev_peer.second;
    int rid = route_class;
    // int rid = -1;
    bool skip = false;
    if (coll_type == TREES_REDUCE_SCATTER) {
      // auto p = getRouteTablePath(prev_peer, m_r, prev_centers[0]);
      // rid = p.first;
      std::vector<int> path_shifted =
          getRouteTablePath(prev_peer_id, m_r, route_class);
      skip = (rid == -1 && !same_direction(m_r, prev_peer_id, m_port_id,
                                           m_dimensions, m_dimensions_num)) ||
             (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
    } else {
      // auto p = getRouteTablePath(m_r, prev_peer, prev_centers[0]);
      // rid = p.first;
      std::vector<int> path_shifted =
          getRouteTablePath(m_r, prev_peer_id, route_class);
      skip = (rid == -1 && !same_direction(m_r, prev_peer_id, m_port_id,
                                           m_dimensions, m_dimensions_num)) ||
             (rid != -1 && m_port_id != path_shifted[1]);
    }
    if (skip) {
      continue;
    }
    // if (!m_latency_optimal && !same_direction(m_r, prev_peer, m_port_id, n,
    // m_dimensions_num)) {
    //   continue;
    // }
    if (chunk_id != prev_peer_counter) {
      prev_peer_counter++;
      continue;
    }
    for (int come_center_id : prev_centers) {
      m_runner->stages_waiting_centers[chunk_stage][come_center_id]--;
    }
    break;
  }
  // for (int come_center_id :
  //      recv_peers[(chunk_stage < m_stages_num ? chunk_stage
  //                                             : chunk_stage -
  //                                             (m_stages_num))]
  //                [m_port_id]) {
  //   // if (m_r == 0 && global_stage == 3) {
  //   //   int q = 1;
  //   // }
  //   m_runner->stages_waiting_centers[chunk_stage][come_center_id]--;
  // }
  // m_ready_to_recv = waiting_receive();
  // m_req_recv = 0;
  m_req_recv[chunk_stage][chunk_id] = 0;
  processReceivedData(evQ, coll_type, chunk_stage, chunk_id);
}

// void EmberTorusCollGenerator::TorusCollectiveEngine::notifySend(int chunk_id)
// {
//   m_waiting_send[chunk_id] = false;
//   m_ready_to_send = waiting_send();
//   // m_req_send = 0;
//   m_req_send[chunk_id] = 0;
// }
void EmberTreesCollGenerator::TreesCollectiveEngine::processReceivedData(
    std::queue<EmberEvent *> &evQ, CollType coll_type, int chunk_stage,
    int chunk_id) {
  auto &recv_peers = coll_type == TREES_REDUCE_SCATTER ? scatter_peers_recv
                                                       : allgather_peers_recv;
  if (coll_type == TREES_REDUCE_SCATTER) {
    m_data_reduced += m_recv_size * m_gen.sizeofDataType(FLOAT);
  }
  // if (m_r == 0 && m_dst[0] != 0.0) {
  //   int q = 1;
  // }
  // if (m_r == 0 && m_i == 7 && m_port_id == 1) {
  //   int q = 1;
  // }
  int n = m_dimensions[0];
  // int stages_num = n % 2 == 1 ? n - 1 : n;
  int small_stage =
      chunk_stage < m_stages_num ? chunk_stage : chunk_stage - (m_stages_num);
  if (m_validate) {
    int peer_id = 0;
    for (auto &[peer, centers] : recv_peers[small_stage]) {
      // if (peer != m_port_id) {
      //   continue;
      // }
      int prev_peer_id = peer.first;
      int route_class = peer.second;
      int rid = route_class;
      bool skip = false;
      if (coll_type == TREES_REDUCE_SCATTER) {
        // auto p = getRouteTablePath(peer, m_r, centers[0]);
        // rid = p.first;
        std::vector<int> path_shifted =
            getRouteTablePath(prev_peer_id, m_r, route_class);
        skip =
            (rid == -1 && !same_direction(m_r, prev_peer_id, m_port_id,
                                          m_dimensions, m_dimensions_num)) ||
            (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
      } else {
        // auto p = getRouteTablePath(m_r, peer, centers[0]);
        // rid = p.first;
        std::vector<int> path_shifted =
            getRouteTablePath(m_r, prev_peer_id, route_class);
        skip = (rid == -1 && !same_direction(m_r, prev_peer_id, m_port_id,
                                             m_dimensions, m_dimensions_num)) ||
               (rid != -1 && m_port_id != path_shifted[1]);
      }
      if (skip) {
        continue;
      }

      if (chunk_id != peer_id) {
        peer_id++;
        continue;
      }
      if (m_r == 27) {
        int q = 1;
      }
      auto chunk = m_recv_epoch[chunk_stage].chunks[chunk_id];
      std::vector<float> received_data(chunk.ptr, chunk.ptr + chunk.size);
      if (m_latency_optimal) {
        // Same order as send pack: unique tree ids from centers as they appear.
        const int num_trees = (int)m_runner->allreduce_trees.size();
        const int tree_chunk = (int)(m_count / num_trees);
        std::vector<int> tree_ids;
        for (int center : centers) {
          const int tid = center % num_trees;
          if (std::find(tree_ids.begin(), tree_ids.end(), tid) ==
              tree_ids.end()) {
            tree_ids.push_back(tid);
          }
        }
        for (size_t i = 0; i < tree_ids.size(); i++) {
          const int tid = tree_ids[i];
          const uint32_t dst_off = (uint32_t)(tid * tree_chunk);
          const uint32_t src_off = (uint32_t)(i * tree_chunk);
          for (int j = 0; j < tree_chunk; j++) {
            if (coll_type == TREES_REDUCE_SCATTER) {
              m_dst[dst_off + j] += received_data[src_off + j];
            } else {
              m_dst[dst_off + j] = received_data[src_off + j];
            }
          }
        }
      } else {
        for (int i = 0; i < centers.size(); i++) {
          int center_id = centers[i];
          uint32_t recv_block_offset = getBlockOffset(center_id);
          uint32_t recv_block_size = getBlockSize(center_id);

          for (uint32_t j = recv_block_offset;
               j < recv_block_offset + recv_block_size; j++) {
            if (coll_type == TREES_REDUCE_SCATTER) {
              if (j == 0 && (m_i == 0 || m_i == 6)) {
                int q = 1;
              }
              m_dst[j] +=
                  received_data[j - recv_block_offset + i * recv_block_size];
            } else {
              m_dst[j] =
                  received_data[j - recv_block_offset + i * recv_block_size];
            }
          }
        }
      }
      break;
    }
    // for (auto chunk : m_recv_epoch.chunks) {
    //   std::vector<float> received_data(chunk.ptr, chunk.ptr + chunk.size);
    //   int block_id = recv_peers[m_i-1]
    // }
  } else {
    if (m_aggregation_cost_ns != 0 && coll_type == TREES_REDUCE_SCATTER) {
      m_gen.enQ_compute(evQ, m_aggregation_cost_ns * m_recv_size);
      return;
    }
  }
  // if (m_r == 0 && m_dst[0] != 0.0) {
  //   int q = 1;
  // }
}

uint32_t EmberTreesCollGenerator::TreesCollectiveEngine::messageTag(
    CollType coll_type, int route_class, uint32_t stage) const {
  const uint32_t route_class_tag_stride =
      static_cast<uint32_t>(route_table.size()) + 1u;
  const uint32_t rc_slot =
      route_class < 0 ? 0u : static_cast<uint32_t>(route_class + 1u);
  const uint32_t phase_slot = coll_type == TREES_REDUCE_SCATTER ? 0u : 1u;
  const uint32_t global_stage =
      phase_slot * static_cast<uint32_t>(m_stages_num) + stage;
  return global_stage * route_class_tag_stride + rc_slot;
}

uint32_t EmberTreesCollGenerator::TreesCollectiveEngine::getBlockOffset(
    uint32_t block_idx) {
  if (m_count < m_p)
    return 0;

  uint32_t block_size = m_count / (m_p * m_runner->allreduce_trees.size());
  return block_idx * block_size;
}

uint32_t EmberTreesCollGenerator::TreesCollectiveEngine::getBlockSize(
    uint32_t block_idx) {
  if (m_count < m_p * m_runner->allreduce_trees.size()) {
    return m_count;
  }

  uint32_t block_size = m_count / (m_p * m_runner->allreduce_trees.size());
  uint32_t num_blocks = m_p * m_runner->allreduce_trees.size();
  assert(block_idx >= 0 && block_idx < num_blocks);
  if (block_idx < num_blocks - 1) {
    // printf("Returning block size1 %d (num_blocks %d)\n", block_size,
    // num_blocks);
    return block_size;
  } else {
    // printf("Returning block size2 %d (num_blocks %d)\n", m_count -
    // (block_size * (num_blocks - 1)), num_blocks);
    return m_count - (block_size * (num_blocks - 1));
  }
}

bool EmberTreesCollGenerator::TreesCollectiveEngine::waiting_receive(
    CollType coll_type) {
  auto &recv_peers = (coll_type == TREES_REDUCE_SCATTER) ? scatter_port_recv
                                                         : allgather_port_recv;
  int n = m_dimensions[0];
  // int stages_num = n % 2 == 1 ? n - 1 : n;
  assert(m_i > 0);
  if (m_r == 5 && m_i == 2 && coll_type == TREES_REDUCE_SCATTER &&
      m_port_id == 10) {
    int q = 1;
  }
  if (m_r == 5 && m_i == 3 && coll_type == TREES_REDUCE_SCATTER &&
      m_port_id == 10) {
    int q = 1;
  }
  if (m_r == 0 && m_port_id == 5 && m_i == 2) {
    int q = 1;
  }
  bool wait_108_center_id = false;
  int global_stage =
      m_i - 1 + (coll_type == TREES_REDUCE_SCATTER ? 0 : m_stages_num);
  for (auto &[prev_peer, prev_peer_centers] : recv_peers[m_i - 1]) {
    for (int center_id : prev_peer_centers) {
      if (center_id == 108) {
        wait_108_center_id = true;
      }
      if (m_runner->stages_waiting_centers[global_stage].find(center_id) ==
          m_runner->stages_waiting_centers[global_stage].end()) {
        return true;
      } else {
        if (m_runner->stages_waiting_centers[global_stage][center_id] > 0) {
          return true;
        }
      }
    }
  }
  if (m_r == 3 && m_port_id == 0 && m_i == 1) {
    int q = 1;
  }
  // if (wait_108_center_id) {
  //   std::string coll_type_str =
  //       coll_type == TREES_REDUCE_SCATTER ? "reduce-scatter" : "allgather";
  //   printf("[Contact Info] Time: %lu ns | Collective type: %s | Global stage:
  //   "
  //          "%d | Rank: %d was waiting for center_id=27 tree\n",
  //          m_gen.getCurrentSimTimeNano(), coll_type_str.c_str(),
  //          global_stage, m_r);
  //   fflush(stdout);
  // }
  return false;
}
bool EmberTreesCollGenerator::TreesCollectiveEngine::waiting_send() {
  for (auto waiting_send : m_waiting_send) {
    if (waiting_send) {
      return true;
    }
  }
  return false;
}
bool EmberTreesCollGenerator::TreesCollectiveEngine::collective(
    std::queue<EmberEvent *> &evQ, CollType coll_type) {

  std::string coll_type_str =
      coll_type == TREES_REDUCE_SCATTER ? "reducescatter" : "allgather";
  auto &send_peers = coll_type == TREES_REDUCE_SCATTER ? scatter_peers_send
                                                       : allgather_peers_send;
  auto &recv_peers = coll_type == TREES_REDUCE_SCATTER ? scatter_peers_recv
                                                       : allgather_peers_recv;
  auto &send_ports = coll_type == TREES_REDUCE_SCATTER ? scatter_port_send
                                                       : allgather_port_send;
  auto &recv_ports = coll_type == TREES_REDUCE_SCATTER ? scatter_port_recv
                                                       : allgather_port_recv;
  // int n = m_dimensions[0];
  // // int stages_num = n % 2 == 1 ? n - 1 : n;
  // int m = (n - 1) / 2;
  if (m_i == 3 && m_r == 9 && m_port_id == 1) {
    int q = 1;
  }
  int global_stage =
      m_i + (coll_type == TREES_REDUCE_SCATTER ? 0 : m_stages_num);
  DPRINTF("Starting step %d on rank %d, on port %d, %s\n", m_i, m_r, m_port_id,
          coll_type_str.c_str());
  fflush(stdout);

  if (m_i > 0 && waiting_receive(coll_type)) {
    m_ready_to_recv = true;
    return false;
  }
  /************************************ */
  // Recv payload is only read from notifyRecv, which has already finished
  // for global_stage-1. Drop that slab before the next stage allocates, or
  // 32 stages of Trees_budget_b16 at count=2**25 keep about 1 TiB resident.
  if (global_stage > 0) {
    auto &done = m_recv_epoch[global_stage - 1];
    done.chunks.clear();
    std::vector<float>().swap(done.slab);
  }
  /**************************************/
  if (m_i == 5 && coll_type == TREES_ALLGATHER) {
    int q = 1;
  }
  if (m_i == 1 && coll_type == TREES_REDUCE_SCATTER) {
    int q = 1;
  }

  if (coll_type == TREES_REDUCE_SCATTER) {
    // printf("[Stage Marker][rank %d][stage %d] Time: %lu ns | Первая стадия "
    //        "reduce-scatter "
    //        "завершена\n",
    //        m_r, m_i, m_gen.getCurrentSimTimeNano());
    // fflush(stdout);
  }
  uint64_t current_time_ns = m_gen.getCurrentSimTimeNano();

  // дельта с прошлого замера (переменная-поле класса)
  uint64_t dt_ns = (m_prev_time != 0 && current_time_ns >= m_prev_time)
                       ? (current_time_ns - m_prev_time)
                       : 0;
  m_prev_time = current_time_ns;

  // если есть фазы/шаги, посчитаем сквозную стадию
  // для деревьев L возьмите из своей логики уровней (или оставьте только
  // state/step)
  const int L = m_stages_num;
  const int phaseIdx = (coll_type == TREES_REDUCE_SCATTER) ? 0 : 1;
  ;
  const int stepInPhase = std::min((int)m_i, L);
  const int curStage = phaseIdx * L + stepInPhase;
  const int totalStages = 2 * L;

  if (m_r == 0 && m_port_id == 1) {
    std::cerr << "[r=0] t=" << current_time_ns << " ns"
              << " dt=" << dt_ns << " ns"
              << " stage=" << curStage << "/" << totalStages << " ("
              << (phaseIdx == 0 ? "RS" : "AG") << " step " << stepInPhase << "/"
              << L << ")" << std::endl;
  }

  auto current_time = m_gen.getCurrentSimTimeNano();
  double send_size_time = m_send_size * m_gen.sizeofDataType(FLOAT) / 400.0;
  DPRINTF("[%d] port %d, step %d, phase %s, current time %" PRIu64 " "
          "send_size_time %f \n",
          m_r, m_port_id, m_i,
          coll_type == TREES_REDUCE_SCATTER ? "reduce-scatter" : "allgather",
          current_time, send_size_time);
  fflush(stdout);
  // if (m_i > 0) {
  //   m_gen.enQ_waitall(evQ, m_req_send[global_stage - 1].size(),
  //                     m_req_send[global_stage - 1].data(), NULL);
  // }
  if (m_i == 0) {
    // recv//
    int total = 0;
    int recv_block_size = getBlockSize(0);
    int total_prev_peers = 0;
    for (auto &[prev_peer, prev_centers] : recv_peers[m_i]) {
      // if (prev_peer != m_port_id) {
      //   continue;
      // }
      int prev_peer_id = prev_peer.first;
      int route_class = prev_peer.second;
      int rid = route_class;
      bool skip = false;
      if (coll_type == TREES_REDUCE_SCATTER) {
        // auto p = getRouteTablePath(prev_peer, m_r, route_class);
        // rid = p.first;
        std::vector<int> path_shifted =
            getRouteTablePath(prev_peer_id, m_r, route_class);
        skip =
            (rid == -1 && !same_direction(m_r, prev_peer_id, m_port_id,
                                          m_dimensions, m_dimensions_num)) ||
            (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
      } else {
        // auto p = getRouteTablePath(m_r, prev_peer, prev_centers[0]);
        // rid = p.first;
        std::vector<int> path_shifted =
            getRouteTablePath(m_r, prev_peer_id, route_class);
        skip = (rid == -1 && !same_direction(m_r, prev_peer_id, m_port_id,
                                             m_dimensions, m_dimensions_num)) ||
               (rid != -1 && m_port_id != path_shifted[1]);
      }
      if (skip) {
        continue;
      }

      total_prev_peers++;
      m_runner->addWaitingCenters(m_i, recv_peers[m_i][prev_peer], coll_type);
      if (m_latency_optimal) {
        const int num_trees = (int)m_runner->allreduce_trees.size();
        std::vector<int> tree_ids;
        for (int center : prev_centers) {
          const int tid = center % num_trees;
          if (std::find(tree_ids.begin(), tree_ids.end(), tid) ==
              tree_ids.end()) {
            tree_ids.push_back(tid);
          }
        }
        total += (int)(m_count / num_trees) * (int)tree_ids.size();
      } else {
        for (int prev_center : prev_centers) {
          total += getBlockSize(prev_center);
        }
      }
    }

    m_recv_epoch[global_stage].chunks.clear();
    m_recv_epoch[global_stage].slab.resize(total);
    m_req_recv[global_stage].assign(total_prev_peers, MessageRequest());
    if (total != 0) {
      // m_req_recv.assign(recv_peers[m_i].size(), MessageRequest());
      int prev_peer_counter = 0;
      m_recv_size = 0;
      for (auto &[prev_peer, prev_centers] : recv_peers[m_i]) {
        // if (prev_peer != m_port_id) {
        //   continue;
        // }
        int prev_peer_id = prev_peer.first;
        int route_class = prev_peer.second;
        int rid = route_class;
        bool skip = false;
        if (coll_type == TREES_REDUCE_SCATTER) {
          // auto p = getRouteTablePath(prev_peer, m_r, prev_centers[0]);

          std::vector<int> path_shifted =
              getRouteTablePath(prev_peer_id, m_r, route_class);
          skip =
              (rid == -1 && !same_direction(m_r, prev_peer_id, m_port_id,
                                            m_dimensions, m_dimensions_num)) ||
              (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
        } else {
          // auto p = getRouteTablePath(m_r, prev_peer, prev_centers[0]);
          std::vector<int> path_shifted =
              getRouteTablePath(m_r, prev_peer_id, route_class);
          skip =
              (rid == -1 && !same_direction(m_r, prev_peer_id, m_port_id,
                                            m_dimensions, m_dimensions_num)) ||
              (rid != -1 && m_port_id != path_shifted[1]);
        }
        if (skip) {
          continue;
        }

        const uint32_t msg_tag = messageTag(coll_type, route_class, m_i);
        int count;
        if (m_latency_optimal) {
          const int num_trees = (int)m_runner->allreduce_trees.size();
          std::vector<int> tree_ids;
          for (int center : prev_centers) {
            const int tid = center % num_trees;
            if (std::find(tree_ids.begin(), tree_ids.end(), tid) ==
                tree_ids.end()) {
              tree_ids.push_back(tid);
            }
          }
          count = (int)(m_count / num_trees) * (int)tree_ids.size();
        } else {
          count = (int)(recv_block_size * prev_centers.size());
        }
        m_recv_epoch[global_stage].chunks.push_back(
            {m_recv_epoch[global_stage].slab.data() + m_recv_size, count,
             prev_peer_id});
        m_gen.enQ_irecv(evQ, m_recv_epoch[global_stage].chunks.back().ptr,
                        count, FLOAT, prev_peer_id, msg_tag, m_comm,
                        &m_req_recv[global_stage][prev_peer_counter++]);
        m_recv_size += count;
        std::string peers_str = "[";
        for (size_t k = 0; k < prev_centers.size(); ++k) {
          peers_str += std::to_string(prev_centers[k]);
          if (k + 1 < prev_centers.size())
            peers_str += " ";
        }
        peers_str += "]";

        DPRINTF("[%d] Receiving %d elements in %d global_stage %s from %d with "
                "tag %u route_class "
                "%d colltype = %s\n",
                m_r, count, global_stage, peers_str.c_str(), prev_peer_id,
                msg_tag, route_class, coll_type_str.c_str());
        fflush(stdout);
      }
    }

    // send//
    if (m_r == 3 && m_i == 0 && coll_type == TREES_ALLGATHER &&
        m_port_id == 0) {
      int q = 1;
    }
    total = 0;
    int total_next_peers = 0;
    int send_block_size = getBlockSize(0);
    for (auto &[next_peer, next_centers] : send_peers[m_i]) {
      // if (next_peer != m_port_id) {
      //   continue;
      // }
      int next_peer_id = next_peer.first;
      int route_class = next_peer.second;
      int rid = route_class;
      bool skip = false;
      if (coll_type == TREES_REDUCE_SCATTER) {
        // auto p = getRouteTablePath(m_r, next_peer, next_centers[0]);
        // rid = p.first;
        std::vector<int> path_shifted =
            getRouteTablePath(m_r, next_peer_id, route_class);
        skip = (rid == -1 && !same_direction(m_r, next_peer_id, m_port_id,
                                             m_dimensions, m_dimensions_num)) ||
               (rid != -1 && m_port_id != path_shifted[1]);
      } else {
        // auto p = getRouteTablePath(next_peer, m_r, next_centers[0]);
        // rid = p.first;
        std::vector<int> path_shifted =
            getRouteTablePath(next_peer_id, m_r, route_class);
        skip =
            (rid == -1 && !same_direction(m_r, next_peer_id, m_port_id,
                                          m_dimensions, m_dimensions_num)) ||
            (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
      }
      if (skip) {
        continue;
      }

      // if (!m_latency_optimal && !same_direction(m_r, next_peer, m_port_id, n,
      // m_dimensions_num)) {
      //   continue;
      // }
      total_next_peers++;
      if (m_latency_optimal) {
        const int num_trees = (int)m_runner->allreduce_trees.size();
        std::vector<int> tree_ids;
        for (int center : next_centers) {
          const int tid = center % num_trees;
          if (std::find(tree_ids.begin(), tree_ids.end(), tid) ==
              tree_ids.end()) {
            tree_ids.push_back(tid);
          }
        }
        total += (int)(m_count / num_trees) * (int)tree_ids.size();
      } else {
        for (int next_center : next_centers) {
          total += getBlockSize(next_center);
        }
      }
    }
    if (m_r == 0 && m_i == 0 && coll_type == TREES_ALLGATHER) {
      int q = 1;
    }
    m_send_epoch.chunks.clear();
    m_send_epoch.slab.resize(total);
    // m_req_send[m_i].assign(send_peers[m_i].size(), MessageRequest());
    m_req_send[global_stage].assign(total_next_peers, MessageRequest());
    if (total != 0) {
      m_send_size = 0;
      size_t next_peer_counter = 0;
      for (auto &[next_peer, next_centers] : send_peers[m_i]) {
        // if (next_peer != m_port_id) {
        //   continue;
        // }
        int next_peer_id = next_peer.first;
        int route_class = next_peer.second;
        int rid = route_class;
        bool skip = false;
        if (coll_type == TREES_REDUCE_SCATTER) {
          // auto p = getRouteTablePath(m_r, next_peer, next_centers[0]);
          // rid = p.first;
          std::vector<int> path_shifted =
              getRouteTablePath(m_r, next_peer_id, route_class);
          skip =
              (rid == -1 && !same_direction(m_r, next_peer_id, m_port_id,
                                            m_dimensions, m_dimensions_num)) ||
              (rid != -1 && m_port_id != path_shifted[1]);
        } else {
          // auto p = getRouteTablePath(next_peer, m_r, next_centers[0]);
          // rid = p.first;
          std::vector<int> path_shifted =
              getRouteTablePath(next_peer_id, m_r, route_class);
          skip =
              (rid == -1 && !same_direction(m_r, next_peer_id, m_port_id,
                                            m_dimensions, m_dimensions_num)) ||
              (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
        }
        if (skip) {
          continue;
        }
        if (m_r == 0 && m_i == 0 && coll_type == TREES_ALLGATHER) {
          int q = 1;
        }
        // if (!m_latency_optimal && !same_direction(m_r, next_peer, m_port_id,
        // n, m_dimensions_num))
        // {
        //   continue;
        // }
        std::vector<int> tree_ids;
        const int num_trees = (int)m_runner->allreduce_trees.size();
        const int tree_chunk = (int)(m_count / num_trees);
        if (m_latency_optimal) {
          for (int center : next_centers) {
            const int tid = center % num_trees;
            if (std::find(tree_ids.begin(), tree_ids.end(), tid) ==
                tree_ids.end()) {
              tree_ids.push_back(tid);
            }
          }
        }
        const int count = m_latency_optimal
                              ? tree_chunk * (int)tree_ids.size()
                              : (int)(send_block_size * next_centers.size());
        // Without validate m_dst is NULL; send slab only (timing path).
        float *out = m_send_epoch.slab.data() + m_send_size;

        if (m_latency_optimal) {
          if (m_validate) {
            float *cur = out;
            for (int tid : tree_ids) {
              std::memcpy(cur, m_dst + tid * tree_chunk,
                          tree_chunk * m_gen.sizeofDataType(FLOAT));
              cur += tree_chunk;
            }
          }
        } else if (m_validate) {
          float *cur = out;
          for (int center : next_centers) {
            if (center == 0) {
              std::string phase_str = coll_type == TREES_REDUCE_SCATTER
                                          ? "reduce-scatter"
                                          : "allgather";
              DPRINTF("[%d] Sending data from block 0 to peer %d at step "
                      "%d on phase %s \n",
                      m_r, next_peer_id, m_i, phase_str.c_str());
              fflush(stdout);
            }

            std::memcpy(cur, m_dst + getBlockOffset(center),
                        send_block_size * m_gen.sizeofDataType(FLOAT));
            cur += send_block_size;
          }
        }
        int route_id = rid;
        if (route_id != -1 && coll_type == TREES_ALLGATHER) {
          route_id += route_table.size();
        }
        const uint32_t msg_tag = messageTag(coll_type, route_class, m_i);
        m_send_epoch.chunks.emplace_back(out, count, next_peer_id);
        emberAssertMsgFitsShort((uint64_t)count * m_gen.sizeofDataType(FLOAT),
                                m_gen.valueShort(), "TreesAllreduce isend");
        m_gen.enQ_isend(evQ, out, count, FLOAT, next_peer_id, msg_tag, m_comm,
                        &m_req_send[global_stage][next_peer_counter++],
                        route_id);
        m_send_size += count;
        std::string peers_str = "[";
        for (size_t k = 0; k < next_centers.size(); ++k) {
          peers_str += std::to_string(next_centers[k]);
          if (k + 1 < next_centers.size())
            peers_str += " ";
        }
        peers_str += "]";
        DPRINTF("[%d] Sending %d elements in %d global_stage %s to %d with tag "
                "%u route_class %d "
                "colltype = %s\n",
                m_r, count, global_stage, peers_str.c_str(), next_peer_id,
                msg_tag, route_class, coll_type_str.c_str());
        fflush(stdout);
      }
      m_data_sent += m_send_size * m_gen.sizeofDataType(FLOAT);
    }
  } else {
    if (m_r == 5 && m_port_id == 10 && coll_type == TREES_REDUCE_SCATTER &&
        m_i == 3) {
      int q = 1;
    }
    // m_waiting_recv.assign(m_waiting_recv.size(), false);
    // processReceivedData(evQ, coll_type);
    // auto m_i_copy = m_i;
    // m_gen.enQ_waitall(evQ, m_req_send[m_i - 1].size(),
    //                   m_req_send[m_i - 1].data(), NULL);
    // m_gen.enQ_compute(evQ, [&, m_i_copy]() {
    //   m_req_send[m_i_copy - 1].clear();
    //   return 0;
    // });

    if (m_i < m_stages_num) {
      // recv//
      if (m_r == 0 && m_port_id == 4 && m_i == 1) {
        int q = 1;
      }
      int total = 0;
      int total_prev_peers = 0;
      int recv_block_size = getBlockSize(0);
      for (auto &[prev_peer, prev_centers] : recv_peers[m_i]) {
        // if (prev_peer != m_port_id) {
        //   continue;
        // }
        int prev_peer_id = prev_peer.first;
        int route_class = prev_peer.second;
        int rid = route_class;
        bool skip = false;
        if (coll_type == TREES_REDUCE_SCATTER) {
          // auto p = getRouteTablePath(prev_peer, m_r, prev_centers[0]);
          // rid = p.first;
          std::vector<int> path_shifted =
              getRouteTablePath(prev_peer_id, m_r, route_class);
          skip =
              (rid == -1 && !same_direction(m_r, prev_peer_id, m_port_id,
                                            m_dimensions, m_dimensions_num)) ||
              (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
        } else {
          // auto p = getRouteTablePath(m_r, prev_peer, prev_centers[0]);
          // rid = p.first;
          std::vector<int> path_shifted =
              getRouteTablePath(m_r, prev_peer_id, route_class);
          skip =
              (rid == -1 && !same_direction(m_r, prev_peer_id, m_port_id,
                                            m_dimensions, m_dimensions_num)) ||
              (rid != -1 && m_port_id != path_shifted[1]);
        }
        if (skip) {
          continue;
        }

        // if (!m_latency_optimal && !same_direction(m_r, prev_peer, m_port_id,
        // n, m_dimensions_num))
        // {
        //   continue;
        // }
        total_prev_peers++;
        m_runner->addWaitingCenters(m_i, recv_peers[m_i][prev_peer], coll_type);
        for (int prev_center : prev_centers) {
          total += getBlockSize(prev_center);
        }
      }
      m_recv_epoch[global_stage].chunks.clear();
      m_recv_epoch[global_stage].slab.resize(total);

      m_req_recv[global_stage].assign(total_prev_peers, MessageRequest());
      if (total != 0) {
        int prev_peer_counter = 0;
        m_recv_size = 0;
        for (auto &[prev_peer, prev_centers] : recv_peers[m_i]) {
          // if (prev_peer != m_port_id) {
          //   continue;
          // }
          int prev_peer_id = prev_peer.first;
          int route_class = prev_peer.second;
          int rid = route_class;
          bool skip = false;
          if (coll_type == TREES_REDUCE_SCATTER) {
            // auto p = getRouteTablePath(prev_peer, m_r, prev_centers[0]);
            // rid = p.first;
            std::vector<int> path_shifted =
                getRouteTablePath(prev_peer_id, m_r, route_class);
            skip = (rid == -1 && !m_latency_optimal &&
                    !same_direction(m_r, prev_peer_id, m_port_id, m_dimensions,
                                    m_dimensions_num)) ||
                   (rid != -1 &&
                    m_port_id != path_shifted[path_shifted.size() - 2]);
          } else {
            // auto p = getRouteTablePath(m_r, prev_peer, prev_centers[0]);
            // rid = p.first;
            std::vector<int> path_shifted =
                getRouteTablePath(m_r, prev_peer_id, route_class);
            skip = (rid == -1 && !m_latency_optimal &&
                    !same_direction(m_r, prev_peer_id, m_port_id, m_dimensions,
                                    m_dimensions_num)) ||
                   (rid != -1 && m_port_id != path_shifted[1]);
          }
          if (skip) {
            continue;
          }

          // if (!m_latency_optimal && !same_direction(m_r, prev_peer,
          // m_port_id, n, m_dimensions_num)) {
          //   continue;
          // }
          const uint32_t msg_tag = messageTag(coll_type, route_class, m_i);
          m_recv_epoch[global_stage].chunks.push_back(
              {m_recv_epoch[global_stage].slab.data() + m_recv_size,
               recv_block_size * prev_centers.size(), prev_peer_id});
          m_gen.enQ_irecv(evQ, m_recv_epoch[global_stage].chunks.back().ptr,
                          recv_block_size * prev_centers.size(), FLOAT,
                          prev_peer_id, msg_tag, m_comm,
                          &m_req_recv[global_stage][prev_peer_counter++]);
          m_recv_size += recv_block_size * prev_centers.size();
          std::string peers_str = "[";
          for (size_t k = 0; k < prev_centers.size(); ++k) {
            peers_str += std::to_string(prev_centers[k]);
            if (k + 1 < prev_centers.size())
              peers_str += " ";
          }
          peers_str += "]";

          DPRINTF("[%d] Receiving %d elements in %d global_stage %s from %d "
                  "with tag %u "
                  "route_class %d colltype "
                  "= %s\n",
                  m_r, recv_block_size * prev_centers.size(), global_stage,
                  peers_str.c_str(), prev_peer_id, msg_tag, route_class,
                  coll_type_str.c_str());
          fflush(stdout);
        }
      }

      // send//
      if (m_r == 6 && m_port_id == 0 && coll_type == TREES_ALLGATHER &&
          m_i == 1) {
        int q = 1;
      }
      if (m_r == 20 && m_port_id == 21 && coll_type == TREES_ALLGATHER &&
          m_i == 1) {
        int q = 1;
      }
      if (m_r == 4 && m_port_id == 0 && m_i == 1) {
        int q = 1;
      }
      total = 0;
      int total_next_peers = 0;
      int send_block_size = getBlockSize(0);
      for (auto &[next_peer, next_centers] : send_peers[m_i]) {
        // if (next_peer != m_port_id) {
        //   continue;
        // }
        int next_peer_id = next_peer.first;
        int route_class = next_peer.second;
        int rid = route_class;
        bool skip = false;
        if (coll_type == TREES_REDUCE_SCATTER) {
          // auto p = getRouteTablePath(m_r, next_peer, next_centers[0]);
          // rid = p.first;
          std::vector<int> path_shifted =
              getRouteTablePath(m_r, next_peer_id, route_class);
          skip = (rid == -1 && !m_latency_optimal &&
                  !same_direction(m_r, next_peer_id, m_port_id, m_dimensions,
                                  m_dimensions_num)) ||
                 (rid != -1 && m_port_id != path_shifted[1]);
        } else {
          // auto p = getRouteTablePath(next_peer, m_r, next_centers[0]);
          // rid = p.first;
          std::vector<int> path_shifted =
              getRouteTablePath(next_peer_id, m_r, route_class);
          skip =
              (rid == -1 && !m_latency_optimal &&
               !same_direction(m_r, next_peer_id, m_port_id, m_dimensions,
                               m_dimensions_num)) ||
              (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
        }
        if (skip) {
          continue;
        }

        // if (!m_latency_optimal && !same_direction(m_r, next_peer, m_port_id,
        // n, m_dimensions_num))
        // {
        //   continue;
        // }
        total_next_peers++;
        for (int next_center : next_centers) {
          total += getBlockSize(next_center);
        }
      }
      if (m_i == 6 && m_r == 57) {
        int q = 1;
      }
      m_send_epoch.chunks.clear();
      m_send_epoch.slab.resize(total);
      // m_req_send[m_i].assign(send_peers[m_i].size(), MessageRequest());
      m_req_send[global_stage].assign(total_next_peers, MessageRequest());
      if (total != 0) {
        m_send_size = 0;
        size_t next_peer_counter = 0;
        if (m_i == 6 && m_r == 20) {
          int q = 1;
        }
        for (auto &[next_peer, next_centers] : send_peers[m_i]) {
          // if (next_peer != m_port_id) {
          //   continue;
          // }

          int next_peer_id = next_peer.first;
          int route_class = next_peer.second;
          int rid = route_class;
          bool skip = false;
          if (coll_type == TREES_REDUCE_SCATTER) {
            // auto p = getRouteTablePath(m_r, next_peer, next_centers[0]);
            // rid = p.first;
            std::vector<int> path_shifted =
                getRouteTablePath(m_r, next_peer_id, route_class);
            skip = (rid == -1 && !m_latency_optimal &&
                    !same_direction(m_r, next_peer_id, m_port_id, m_dimensions,
                                    m_dimensions_num)) ||
                   (rid != -1 && m_port_id != path_shifted[1]);
          } else {
            // auto p = getRouteTablePath(next_peer, m_r, next_centers[0]);
            // rid = p.first;
            std::vector<int> path_shifted =
                getRouteTablePath(next_peer_id, m_r, route_class);
            skip = (rid == -1 && !m_latency_optimal &&
                    !same_direction(m_r, next_peer_id, m_port_id, m_dimensions,
                                    m_dimensions_num)) ||
                   (rid != -1 &&
                    m_port_id != path_shifted[path_shifted.size() - 2]);
          }
          if (skip) {
            continue;
          }

          // if (!m_latency_optimal && !same_direction(m_r, next_peer,
          // m_port_id, n, m_dimensions_num)) {
          //   continue;
          // }
          const int count = send_block_size * next_centers.size();
          float *out = m_send_epoch.slab.data() + m_send_size;
          if (m_i == 6 && m_r == 57 && next_peer_id == 0) {
            int q = 1;
          }
          if (m_validate) {
            float *cur = out;
            for (int center : next_centers) {
              if (center == 0) {
                std::string phase_str = coll_type == TREES_REDUCE_SCATTER
                                            ? "reduce-scatter"
                                            : "allgather";
                DPRINTF("[%d] Sending data from block 0 to peer %d at "
                        "step %d on phase %s \n",
                        m_r, next_peer_id, m_i, phase_str.c_str());
                fflush(stdout);
              }
              std::memcpy(cur, m_dst + getBlockOffset(center),
                          send_block_size * m_gen.sizeofDataType(FLOAT));
              cur += send_block_size;
            }
          }
          int route_id = rid;
          if (route_id != -1 && coll_type == TREES_ALLGATHER) {
            route_id += route_table.size();
          }
          const uint32_t msg_tag = messageTag(coll_type, route_class, m_i);
          m_send_epoch.chunks.emplace_back(out, count, next_peer_id);
          emberAssertMsgFitsShort((uint64_t)count * m_gen.sizeofDataType(FLOAT),
                                  m_gen.valueShort(), "TreesAllreduce isend");
          m_gen.enQ_isend(evQ, out, count, FLOAT, next_peer_id, msg_tag, m_comm,
                          &m_req_send[global_stage][next_peer_counter++],
                          route_id);
          m_send_size += count;
          std::string peers_str = "[";
          for (size_t k = 0; k < next_centers.size(); ++k) {
            peers_str += std::to_string(next_centers[k]);
            if (k + 1 < next_centers.size())
              peers_str += " ";
          }
          peers_str += "]";
          DPRINTF("[%d] Sending %d elements in %d global_stage %s to %d with "
                  "tag %u route_class "
                  "%d colltype = %s\n",
                  m_r, send_block_size * next_centers.size(), global_stage,
                  peers_str.c_str(), next_peer_id, msg_tag, route_class,
                  coll_type_str.c_str());

          fflush(stdout);
        }
        m_data_sent += m_send_size * m_gen.sizeofDataType(FLOAT);
      }
    }
  }

  m_i++;
  if (m_i < m_stages_num + 1) {
    return false;
  } else {
    m_i = 0;
    return true;
  }
}

/******* CollectiveBase  *******/

EmberTreesCollGenerator::TreesCollectiveRunner::TreesCollectiveRunner(
    EmberTreesCollGenerator &gen, int num_allreduces, uint32_t count,
    uint32_t rank, uint32_t comm_size, bool nonblocking, bool sync)
    : m_gen(gen), m_nb(nonblocking), m_sync(sync), m_has_new(false),
      m_count(count), m_r(rank), m_p(comm_size) {
  // tracking active and with-pending-receive allreduces
  // m_active_recv_handles.resize(num_allreduces);
  // m_active_allreduce_ptrs.resize(num_allreduces);

  // counter of posted recvs
  m_handle_idx = 0;

  // index of last matched recv in m_active_recv_handles
  m_idx = 0;
}

bool EmberTreesCollGenerator::TreesCollectiveRunner::progress_phase(
    std::queue<EmberEvent *> &evQ) {
  if (m_sync) {
    m_active_handles.clear();
    m_active_allreduce_ptrs.clear();
    bool all_completed = true;
    for (auto &allreduce : m_allreduces) {
      bool this_completed = !allreduce.isEnabled() || allreduce.progress(evQ);
      all_completed = all_completed && this_completed;
      if (allreduce.hasPendingRecv()) {
        DPRINTF("[%d] Found one pending recv\n", m_r);
        fflush(stdout);
        assert(!this_completed);
        auto [active_handlers, active_handlers_ids] = allreduce.getRecvHandle();
        m_active_handles.insert(m_active_handles.end(), active_handlers.begin(),
                                active_handlers.end());
        for (auto handler_id : active_handlers_ids) {
          m_active_allreduce_ptrs.push_back(
              std::make_tuple(&allreduce, handler_id));
        }
        // m_handle_idx++;
      } else {
        DPRINTF("[%d] Found one non-pending recv\n", m_r);
        fflush(stdout);
      }
    }
    if (!m_active_handles.empty()) {
      m_gen.enQ_waitall(evQ, m_active_handles.size(), &m_active_handles[0],
                        NULL);
      m_gen.enQ_compute(evQ, [&]() {
        for (auto [allreduce_ptr, active_handlers_id] :
             m_active_allreduce_ptrs) {
          DPRINTF("[%d] Notifying recv, time %" PRIu64 "\n", m_r,
                  m_notify_time);
          fflush(stdout);
          allreduce_ptr->notifyRecv(active_handlers_id, evQ);
        }
        return 0;
      });
    } else if (!all_completed) {
      m_gen.enQ_compute(evQ, 0);
    }
    return all_completed;
  } else {
    if (!m_active_handles.empty() && m_has_new) {
      auto [allreduce_ptr, active_handlers_id] = m_active_allreduce_ptrs[m_idx];
      DPRINTF("[%d] Notifying recv, time %" PRIu64 "\n", m_r, m_notify_time);
      fflush(stdout);
      allreduce_ptr->notifyRecv(active_handlers_id, evQ);
    } else if (!m_active_handles.empty() && !m_has_new) {
      return false;
    }

    // TODO: Полное обновление всей информации через shmem???

    m_has_new = false;
    m_active_handles.clear();
    m_active_allreduce_ptrs.clear();
    bool all_completed = true;
    for (auto &allreduce : m_allreduces) {

      // we don't progress disabled allreduces (they look as completed)
      bool this_completed = !allreduce.isEnabled() || allreduce.progress(evQ);
      all_completed = all_completed && this_completed;
      if (allreduce.hasPendingRecv()) {
        DPRINTF("[%d] Found one pending recv\n", m_r);
        fflush(stdout);
        assert(!this_completed);
        auto [active_handlers, active_handlers_ids] = allreduce.getRecvHandle();
        m_active_handles.insert(m_active_handles.end(), active_handlers.begin(),
                                active_handlers.end());
        for (auto handler_id : active_handlers_ids) {
          m_active_allreduce_ptrs.push_back(
              std::make_tuple(&allreduce, handler_id));
        }
        // m_handle_idx++;
      } else {
        DPRINTF("[%d] Found one non-pending recv\n", m_r);
        fflush(stdout);
      }
    }
    if (!m_active_handles.empty()) {
      m_has_new = true;
      DPRINTF("[%d] Waiting for %d recvs\n", m_r, m_active_handles.size());
      m_gen.enQ_waitany(evQ, m_active_handles.size(), &m_active_handles[0],
                        &m_idx, &m_resp_recv);
      m_gen.enQ_getTime(evQ, &m_notify_time);
      return false;
    }
    return all_completed;
  }
}

void EmberTreesCollGenerator::TreesCollectiveRunner::printStats() {
  uint64_t rank_time = m_stop_time - m_start_time;
  uint64_t bytes = m_count * m_gen.sizeofDataType(FLOAT);
  uint64_t data_moved = 0;
  uint64_t data_reduced = 0;
  for (auto &allreduce : m_allreduces) {
    data_moved += allreduce.getMovedBytes();
    data_reduced += allreduce.getDataReduced();
  }
  double bw = (double)8 * data_moved / rank_time;
  double gbw = bw * m_p;
  double reduction_bw = (double)8 * data_reduced / rank_time;

  // printf("Size %d - Start %" PRIu64 " - Stop %" PRIu64 " - Diff %" PRIu64 "
  // - Count %d - JobId %d\n", m_gen.size(), m_start_time, m_stop_time,
  // m_stop_time - m_start_time, m_count, m_gen.getJobId());
  printf("TIME %d start_time %" PRIu64 " stop_time %" PRIu64
         " rank_time %" PRIu64 " bytes %" PRIu64 " data_moved %" PRIu64
         " data_reduced %" PRIu64 " bw %lf gbw %lf\n",
         m_r, m_start_time, m_stop_time, rank_time, bytes, data_moved,
         data_reduced, bw, gbw);
  printf("[%d] Reduction BW %lf\n", m_r, reduction_bw);
}

/******* TreesCollective  *******/
EmberTreesCollGenerator::TreesCollective::TreesCollective(
    EmberTreesCollGenerator &gen, uint dimensions_num, uint ports,
    uint32_t count, uint32_t rank, uint32_t comm_size, Communicator comm,
    const std::vector<TreeSpec> &tree_specs,
    std::vector<std::vector<int>> &route_table,
    std::map<std::pair<int, int>, int> &route_table_map,
    double aggregation_cost_ns, bool nb, bool sync, CollType coll_type,
    float *data, uint *dimensions, bool latency_optimal)
    : TreesCollectiveRunner(gen, 4, count, rank, comm_size, nb, sync),
      m_state(INIT), m_coll_type(coll_type), m_dimensions(dimensions),
      allreduce_trees(tree_specs) {
  if (dimensions == NULL) {
    dimensions = (uint *)malloc(sizeof(uint) * MAX_SUPPORTED_DIMENSIONS);
    if (dimensions_num == 1) {
      dimensions[0] = m_p;
      DPRINTF("Considering a logical topology of size %d\n", dimensions[0]);
    } else if (dimensions_num == 2) {
      dimensions[0] = sqrt(m_p);
      dimensions[1] = sqrt(m_p);
      DPRINTF("Considering a logical topology of size %dx%d\n", dimensions[0],
              dimensions[1]);
    } else if (dimensions_num == 3) {
      dimensions[0] = cbrt(m_p);
      dimensions[1] = cbrt(m_p);
      dimensions[2] = cbrt(m_p);
      DPRINTF("Considering a logical topology of size %dx%dx%d\n",
              dimensions[0], dimensions[1], dimensions[2]);
    } else {
      fprintf(stderr, "%d dimensions not supported.", dimensions_num);
      exit(-1);
    }
  }
  m_dimensions = dimensions;

  int max_ports = std::min(count, ports);
  if (m_r == 0) {
    DPRINTF("[%d] Count is %d, thus we will use %d ports\n", m_r, m_count,
            max_ports);
  }

  int n = dimensions[0];
  int D = dimensions_num;
  // Must match TreesCollectiveEngine: AG global_stage offset uses the same L.
  m_stages_num = 0;
  for (const auto &edge : allreduce_trees[0].edges) {
    m_stages_num = std::max(m_stages_num, edge.rsStage + 1);
  }
  stages_waiting_centers.resize(2 * m_stages_num);
  // auto m_point_coord = getCoordFromRank(m_r, n, D);
  auto m_point_coord = getCoordFromRank(m_r, m_dimensions, D);

  // Point 2: get_peers depends only on (center, tree, m_r), not on port_id.
  // Compute once per rank and share across all port-engines.
  const int num_trees = (int)allreduce_trees.size();
  peers_cache.resize((size_t)m_p * (size_t)num_trees);
  for (int center_id = 0; center_id < (int)m_p; ++center_id) {
    for (int id = 0; id < num_trees; ++id) {
      peers_cache[(size_t)center_id * (size_t)num_trees + (size_t)id] =
          allreduce_trees[id].get_peers(center_id, m_r);
    }
  }

  for (int i = 0; i < D; ++i) {
    for (int step : {1, -1}) {
      auto port_coord = m_point_coord;
      port_coord[i] = (port_coord[i] + step + dimensions[i]) % dimensions[i];
      // int port_id = getRankFromCoord(port_coord, n, D);
      int port_id = getRankFromCoord(port_coord, dimensions, D);
      m_allreduces.push_back(TreesCollectiveEngine(
          m_gen, coll_type, dimensions, dimensions_num, data, m_count, m_r, m_p,
          aggregation_cost_ns, comm, data != NULL, latency_optimal, port_id,
          this, route_table, route_table_map));
    }
  }
}
void EmberTreesCollGenerator::TreesCollective::addWaitingCenters(
    int stage, std::vector<int> &centers, CollType coll_type) {
  int n = m_dimensions[0];
  // int stages_num = n % 2 == 1 ? n - 1 : n;
  // assert(stage > 0);
  int global_stage =
      stage + (coll_type == TREES_REDUCE_SCATTER ? 0 : m_stages_num);
  for (int center_id : centers) {
    stages_waiting_centers[global_stage][center_id]++;
    // if (stages_waiting_centers[global_stage].find(center_id) ==
    //     stages_waiting_centers[global_stage].end()) {
    //   stages_waiting_centers[global_stage][center_id] = true;
    // }
  }
  // stages_waiting_centers[stage].insert(centers.begin(), centers.end());
}
bool EmberTreesCollGenerator::TreesCollective::progress(
    std::queue<EmberEvent *> &evQ) {
  switch (m_state) {
  case INIT:
    m_gen.enQ_getTime(evQ, &m_start_time);
    m_state = PHASE_1;
    // no break/return needed here
  case PHASE_1:
    if (!progress_phase(evQ))
      return false;
    // assert(m_handle_idx == 0);
    m_gen.enQ_getTime(evQ, &m_stop_time);
    m_state = FINI;
    return false;
    // printStats();
  case FINI:
    // we do one more cycle to make sure enQ_getTime completes before
    // somebody calls printStats() printStats();
    // m_state = INIT;
    // for (auto &allreduce : m_allreduces)
    //   allreduce.reset();
    return true;
  default:
    assert(0);
  }
  assert(0);
}

void EmberTreesCollGenerator::TreesCollective::reset() {
  m_state = INIT;
  for (auto &allreduce : m_allreduces)
    allreduce.reset();
}

EmberTreesCollGenerator::TreesCollective::~TreesCollective() { ; }

/******* EmberTreesCollGenerator (parent Ember motif) *******/
EmberTreesCollGenerator::EmberTreesCollGenerator(SST::ComponentId_t id,
                                                 Params &params)
    : EmberMessagePassingGenerator(id, params, "None"), m_tag(0) {
  assert(0);
}

EmberTreesCollGenerator::EmberTreesCollGenerator(SST::ComponentId_t id,
                                                 Params &params,
                                                 std::string name)
    : EmberMessagePassingGenerator(id, params, name), m_tag(0) {
  ;
}
bool EmberTreesCollGenerator::generate(std::queue<EmberEvent *> &evQ) {
  assert(0);
}
