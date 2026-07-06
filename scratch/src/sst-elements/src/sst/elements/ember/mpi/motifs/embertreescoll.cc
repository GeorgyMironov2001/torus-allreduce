#include "embertreescoll.h"
#include <algorithm>
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

static std::pair<int, int> up(std::pair<int, int> coord, int n) {
  auto [x, y] = coord;
  return std::make_pair(x, mod(y + 1, n));
}
static std::pair<int, int> down(std::pair<int, int> coord, int n) {
  auto [x, y] = coord;
  return std::make_pair(x, mod(y - 1, n));
}
static std::pair<int, int> left(std::pair<int, int> coord, int n) {
  auto [x, y] = coord;
  return std::make_pair(mod(x - 1, n), y);
}
static std::pair<int, int> right(std::pair<int, int> coord, int n) {
  auto [x, y] = coord;
  return std::make_pair(mod(x + 1, n), y);
}
static uint getRankFromCoord(const std::vector<int> &coord, int n, int d) {
  int res = 0;
  int factor = 1;
  for (int i = d - 1; i >= 0; i--) {
    res += coord[i] * factor;
    factor *= n;
  }
  return res;
  // return coord.first * n + coord.second;
}
static std::vector<int> getCoordFromRank(int rank, int n, int d = 2) {
  std::vector<int> coord(d);
  int nnodes = (int)std::pow(n, d);
  for (int i = 0; i < d; i++) {
    nnodes = nnodes / n;
    coord[i] = rank / nnodes;
    rank = rank % nnodes;
  }
  return coord;
}

int getMainCenterId(int n, int d = 2) {
  std::vector<int> coord(d);
  for (int i = 0; i < d; i++) {
    coord[i] = (n - 1) / 2;
  }
  return getRankFromCoord(coord, n, d);
}
static int getDistanceManhattan(std::pair<int, int> coord1,
                                std::pair<int, int> coord2, int n) {
  auto [x, y] = coord1;
  auto [Ox, Oy] = coord2;
  int x_diff =
      mod(x - Ox, n) > mod(Ox - x, n) ? mod(Ox - x, n) : -mod(x - Ox, n);
  int y_diff =
      mod(y - Oy, n) > mod(Oy - y, n) ? mod(Oy - y, n) : -mod(y - Oy, n);
  return std::abs(x_diff) + std::abs(y_diff);
}
static int getDistanceScatter(std::pair<int, int> m_r,
                              std::pair<int, int> center, int n) {
  auto [x, y] = m_r;
  auto [Ox, Oy] = center;
  if (x == Ox && y == Oy) {
    return n;
  }
  int x_diff =
      mod(x - Ox, n) > mod(Ox - x, n) ? mod(Ox - x, n) : -mod(x - Ox, n);
  int y_diff =
      mod(y - Oy, n) > mod(Oy - y, n) ? mod(Oy - y, n) : -mod(y - Oy, n);
  int m = (n - 1) / 2;

  if (x_diff >= 0 && y_diff < 0) {
    return x_diff == 0 ? n + y_diff : m + 1 - x_diff;
  } else if (x_diff < 0 && y_diff <= 0) {
    return y_diff == 0 ? n + x_diff : m + 1 + y_diff;
  } else if (x_diff <= 0 && y_diff > 0) {
    return x_diff == 0 ? n - y_diff : m + 1 + x_diff;
  } else if (x_diff > 0 && y_diff >= 0) {
    return y_diff == 0 ? n - x_diff : m + 1 - y_diff;
  }
}

auto TreeSpec::get_peers(int center_id, int rank_id) {
  // auto [rx, ry] = getCoordFromRank(rank_id, dim_size, dim_sizes.size());
  // auto [Cx, Cy] = getCoordFromRank(center_id, dim_size, dim_sizes.size());
  // auto [Ox, Oy] = getCoordFromRank(center, dim_size, dim_sizes.size());
  int D = dimension;
  auto r_coord = getCoordFromRank(rank_id, dim_size, D);
  auto C_coord = getCoordFromRank(center_id, dim_size, D);
  auto O_coord = getCoordFromRank(center, dim_size, D);
  auto shift = [=](std::vector<int> point) {
    std::vector<int> new_coord(D);
    // int new_x = (point.first + Cx - Ox + dim_size) % dim_size;
    // int new_y = (point.second + Cy - Oy + dim_size) % dim_size;
    for (int i = 0; i < D; i++) {
      new_coord[i] = (point[i] + C_coord[i] - O_coord[i] + dim_size) % dim_size;
    }
    return new_coord;
  };
  auto shift_back = [=](std::vector<int> point) {
    std::vector<int> new_coord(D);
    for (int i = 0; i < D; i++) {
      new_coord[i] = (point[i] - C_coord[i] + O_coord[i] + dim_size) % dim_size;
    }
    return new_coord;
    // int new_x = (point.first - Cx + Ox + dim_size) % dim_size;
    // int new_y = (point.second - Cy + Oy + dim_size) % dim_size;
    // return std::make_pair(new_x, new_y);
  };
  auto r_shifted_back_coord = shift_back(r_coord);
  int rank = getRankFromCoord(r_shifted_back_coord, dim_size, D);
  auto new_inside_peers = inside_peers[rank];
  auto new_outside_peers = outside_peers[rank];
  for (auto &e : new_inside_peers) {
    e.from = getRankFromCoord(shift(getCoordFromRank(e.from, dim_size, D)),
                              dim_size, D);
    e.to = getRankFromCoord(shift(getCoordFromRank(e.to, dim_size, D)),
                            dim_size, D);
  }
  for (auto &e : new_outside_peers) {
    e.from = getRankFromCoord(shift(getCoordFromRank(e.from, dim_size, D)),
                              dim_size, D);
    e.to = getRankFromCoord(shift(getCoordFromRank(e.to, dim_size, D)),
                            dim_size, D);
  }
  return std::make_pair(new_inside_peers, new_outside_peers);
}
int sign(int x) { return x > 0 ? 1 : x < 0 ? -1 : 0; }

bool same_direction(int from, int to, int port_id, int n, int d) {
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

std::pair<int, std::vector<int>>
EmberTreesCollGenerator::TreesCollectiveEngine::getRouteTableId(
    int from, int to, int center_block_id) {
  int D = m_dimensions_num;
  int n = m_dimensions[0];
  int center_id = center_block_id / m_runner->allreduce_trees.size();
  int main_center_id = getMainCenterId(n, D);

  auto O_coord = getCoordFromRank(main_center_id, n, D);
  auto C_coord = getCoordFromRank(center_id, n, D);
  auto shift = [=](std::vector<int> point) {
    std::vector<int> new_coord(D);
    for (int i = 0; i < D; i++) {
      new_coord[i] = (point[i] - C_coord[i] + O_coord[i] + n) % n;
    }
    return new_coord;
  };

  auto backward_shift = [=](std::vector<int> point) {
    std::vector<int> new_coord(D);
    for (int i = 0; i < D; i++) {
      new_coord[i] = (point[i] + C_coord[i] - O_coord[i] + n) % n;
    }
    return new_coord;
  };
  int from_shifted =
      getRankFromCoord(shift(getCoordFromRank(from, n, D)), n, D);
  int to_shifted = getRankFromCoord(shift(getCoordFromRank(to, n, D)), n, D);
  if (route_table_map.find(std::make_pair(from_shifted, to_shifted)) !=
      route_table_map.end()) {
    int rid = route_table_map[std::make_pair(from_shifted, to_shifted)];
    std::vector<int> path_shifted;
    for (int rank_id : route_table[rid]) {
      path_shifted.push_back(getRankFromCoord(
          backward_shift(getCoordFromRank(rank_id, n, D)), n, D));
    }
    // return route_table_map[std::make_pair(from_shifted, to_shifted)];
    return std::make_pair(rid, path_shifted);
  }
  return std::make_pair(-1, std::vector<int>());
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
  for (auto edge : m_runner->allreduce_trees[0].edges) {
    m_stages_num = std::max(m_stages_num, edge.rsStage + 1);
  }
  int m = (n - 1) / 2;

  auto m_coord = getCoordFromRank(m_r, n, D);

  scatter_peers_send.resize(m_stages_num);
  scatter_peers_recv.resize(m_stages_num);
  allgather_peers_send.resize(m_stages_num);
  allgather_peers_recv.resize(m_stages_num);

  scatter_port_send.resize(m_stages_num);
  scatter_port_recv.resize(m_stages_num);
  allgather_port_send.resize(m_stages_num);
  allgather_port_recv.resize(m_stages_num);

  // std::vector<std::vector<std::map<int, std::vector<int>>>>
  //     all_steps_scatter_send(
  //         n * n, std::vector<std::map<int, std::vector<int>>>(n - 1));
  // std::vector<std::vector<std::map<int, std::vector<int>>>>
  //     all_steps_allgather_send(
  //         n * n, std::vector<std::map<int, std::vector<int>>>(n - 1));

  for (int center_id = 0; center_id < m_p; center_id++) {
    if (m_r == 27 && center_id == 27) {
      int q = 1;
    }
    // int center_id = getRankFromCoord(std::make_pair(Ox, Oy), n);
    for (int id = 0; id < m_runner->allreduce_trees.size(); id++) {
      int center_block_id = center_id * m_runner->allreduce_trees.size() + id;
      auto &allreduce_tree = m_runner->allreduce_trees[id];
      auto [inside_peers, outside_peers] =
          allreduce_tree.get_peers(center_id, m_r);
      for (auto e : inside_peers) {
        scatter_peers_send[e.rsStage][e.to].push_back(center_block_id);
        allgather_peers_recv[e.agStage][e.to].push_back(center_block_id);
        // if (e.to == m_port_id) {
        //   scatter_port_send[e.rsStage][m_port_id].push_back(center_block_id);
        // }

        auto p = getRouteTableId(e.from, e.to, center_block_id);
        int rid = p.first;
        std::vector<int> path_shifted = p.second;
        const bool take =
            (rid == -1 && same_direction(e.from, e.to, m_port_id, n, D)) ||
            (rid != -1 && m_port_id == path_shifted[1]);
        if (take) {
          scatter_port_send[e.rsStage][e.to].push_back(center_block_id);
        }

        // if (same_direction(e.from, e.to, m_port_id, n, D)) {
        //   scatter_port_send[e.rsStage][e.to].push_back(center_block_id);
        // }

        // if (outside_peers.empty() && e.to == m_port_id) {
        //   allgather_port_recv[e.agStage][m_port_id].push_back(
        //       center_block_id);
        // }
        if (outside_peers.empty() && take) {
          allgather_port_recv[e.agStage][e.to].push_back(center_block_id);
        }
        // if (outside_peers.empty() &&
        //     same_direction(e.from, e.to, m_port_id, n, D)) {
        //   allgather_port_recv[e.agStage][e.to].push_back(center_block_id);
        // }
      }
      for (auto e : outside_peers) {
        scatter_peers_recv[e.rsStage][e.from].push_back(center_block_id);
        allgather_peers_send[e.agStage][e.from].push_back(center_block_id);
        if (inside_peers.empty()) {
          scatter_port_recv[e.rsStage][e.from].push_back(center_block_id);
        }
        // if (e.from == m_port_id) {
        //   allgather_port_send[e.agStage][m_port_id].push_back(
        //       center_block_id);
        // }
        auto p = getRouteTableId(e.from, e.to, center_block_id);
        int rid = p.first;
        std::vector<int> path_shifted = p.second;
        const bool take =
            (rid == -1 && same_direction(e.to, e.from, m_port_id, n, D)) ||
            (rid != -1 && m_port_id == path_shifted[path_shifted.size() - 2]);
        if (take) {
          allgather_port_send[e.agStage][e.from].push_back(center_block_id);
        }
        // if (same_direction(e.to, e.from, m_port_id, n, D)) {
        //   allgather_port_send[e.agStage][e.from].push_back(center_block_id);
        // }
      }
    }
  }

  for (int stage = 0; stage < m_stages_num - 1; ++stage) {
    // for (int neighbour_id :
    //      std::array<int, 4>{getRankFromCoord(left(m_coord, n), n),
    //                         getRankFromCoord(right(m_coord, n), n),
    //                         getRankFromCoord(up(m_coord, n), n),
    //                         getRankFromCoord(down(m_coord, n), n)}) {

    for (auto &[neighbour_id, center_block_ids] : scatter_peers_recv[stage]) {
      for (int center_block_id : center_block_ids) {
        int center_id = center_block_id / m_runner->allreduce_trees.size();
        int center_tree_id = center_block_id % m_runner->allreduce_trees.size();

        auto [inside_peers, outside_peers] =
            m_runner->allreduce_trees[center_tree_id].get_peers(center_id, m_r);
        // assert(inside_peers.size() == 1);
        // int inside_peer = inside_peers[0].to;
        for (auto e : inside_peers) {
          // if (e.to == m_port_id) {
          //   scatter_port_recv[stage][neighbour_id].push_back(center_block_id);
          //   break;
          // }
          auto p = getRouteTableId(e.from, e.to, center_block_id);
          int rid = p.first;
          std::vector<int> path_shifted = p.second;
          const bool take =
              (rid == -1 && same_direction(e.from, e.to, m_port_id, n, D)) ||
              (rid != -1 && m_port_id == path_shifted[1]);
          if (take) {
            scatter_port_recv[stage][neighbour_id].push_back(center_block_id);
            break;
          }
          // if (same_direction(e.from, e.to, m_port_id, n, D)) {
          //   scatter_port_recv[stage][neighbour_id].push_back(center_block_id);
          //   break;
          // }
        }
      }
    }
    // }
  }

  for (int stage = 0; stage < m_stages_num - 1; ++stage) {
    // for (int neighbour_id :
    //      std::array<int, 4>{getRankFromCoord(left(m_coord, n), n),
    //                         getRankFromCoord(right(m_coord, n), n),
    //                         getRankFromCoord(up(m_coord, n), n),
    //                         getRankFromCoord(down(m_coord, n), n)}) {

    for (auto &[neighbour_id, center_block_ids] : allgather_peers_recv[stage]) {
      for (int center_block_id : center_block_ids) {
        int center_id = center_block_id / m_runner->allreduce_trees.size();
        int center_tree_id = center_block_id % m_runner->allreduce_trees.size();
        auto [inside_peers, outside_peers] =
            m_runner->allreduce_trees[center_tree_id].get_peers(center_id, m_r);
        for (auto e : outside_peers) {
          // if (e.from == m_port_id) {
          //   allgather_port_recv[stage][neighbour_id].push_back(center_block_id);
          //   break;
          // }
          auto p = getRouteTableId(e.from, e.to, center_block_id);
          int rid = p.first;
          std::vector<int> path_shifted = p.second;
          const bool take =
              (rid == -1 && same_direction(e.to, e.from, m_port_id, n, D)) ||
              (rid != -1 && m_port_id == path_shifted[path_shifted.size() - 2]);
          if (take) {
            allgather_port_recv[stage][neighbour_id].push_back(center_block_id);
            break;
          }

          // if (same_direction(e.to, e.from, m_port_id, n, D)) {
          //   allgather_port_recv[stage][neighbour_id].push_back(center_block_id);
          //   break;
          // }
        }
      }
    }
    // }
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
  if (m_r == 27) {
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
    if (collective(evQ, TREES_REDUCE_SCATTER)) {
      m_state = ALL_GATHER;
      // return false;
    } else {
      return false;
    }
  case ALL_GATHER:
    if (collective(evQ, TREES_ALLGATHER)) {
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
    int rid = -1;
    bool skip = false;
    if (coll_type == TREES_REDUCE_SCATTER) {
      auto p = getRouteTableId(prev_peer, m_r, prev_centers[0]);
      rid = p.first;
      std::vector<int> path_shifted = p.second;
      skip = (rid == -1 && !same_direction(m_r, prev_peer, m_port_id, n,
                                           m_dimensions_num)) ||
             (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
    } else {
      auto p = getRouteTableId(m_r, prev_peer, prev_centers[0]);
      rid = p.first;
      std::vector<int> path_shifted = p.second;
      skip = (rid == -1 && !same_direction(m_r, prev_peer, m_port_id, n,
                                           m_dimensions_num)) ||
             (rid != -1 && m_port_id != path_shifted[1]);
    }
    if (skip) {
      continue;
    }
    // if (!same_direction(m_r, prev_peer, m_port_id, n, m_dimensions_num)) {
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
      int rid = -1;
      bool skip = false;
      if (coll_type == TREES_REDUCE_SCATTER) {
        auto p = getRouteTableId(peer, m_r, centers[0]);
        rid = p.first;
        std::vector<int> path_shifted = p.second;
        skip =
            (rid == -1 &&
             !same_direction(m_r, peer, m_port_id, n, m_dimensions_num)) ||
            (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
      } else {
        auto p = getRouteTableId(m_r, peer, centers[0]);
        rid = p.first;
        std::vector<int> path_shifted = p.second;
        skip = (rid == -1 &&
                !same_direction(m_r, peer, m_port_id, n, m_dimensions_num)) ||
               (rid != -1 && m_port_id != path_shifted[1]);
      }
      if (skip) {
        continue;
      }
      // if (!same_direction(m_r, peer, m_port_id, n, m_dimensions_num)) {
      //   continue;
      // }
      if (chunk_id != peer_id) {
        peer_id++;
        continue;
      }
      if (m_r == 27) {
        int q = 1;
      }
      auto chunk = m_recv_epoch[chunk_stage].chunks[chunk_id];
      std::vector<float> received_data(chunk.ptr, chunk.ptr + chunk.size);
      for (int i = 0; i < centers.size(); i++) {
        int center_id = centers[i];
        uint32_t recv_block_offset = getBlockOffset(center_id);
        uint32_t recv_block_size = getBlockSize(center_id);

        for (uint32_t j = recv_block_offset;
             j < recv_block_offset + recv_block_size; j++) {
          if (coll_type == TREES_REDUCE_SCATTER) {
            m_dst[j] +=
                received_data[j - recv_block_offset + i * recv_block_size];
          } else {
            m_dst[j] =
                received_data[j - recv_block_offset + i * recv_block_size];
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

  auto tag = coll_type == TREES_REDUCE_SCATTER ? m_tag_reduce : m_tag_broadcast;
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
  int n = m_dimensions[0];
  // int stages_num = n % 2 == 1 ? n - 1 : n;
  int m = (n - 1) / 2;
  int global_stage =
      m_i + (coll_type == TREES_REDUCE_SCATTER ? 0 : m_stages_num);
  DPRINTF("Starting step %d on rank %d, on port %d, %s\n", m_i, m_r, m_port_id,
          coll_type_str.c_str());
  fflush(stdout);

  if (m_i > 0 && waiting_receive(coll_type)) {
    m_ready_to_recv = true;
    return false;
  }
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
  if (m_i == 0) {
    // recv//
    int total = 0;
    int recv_block_size = getBlockSize(0);
    int total_prev_peers = 0;
    for (auto &[prev_peer, prev_centers] : recv_peers[m_i]) {
      // if (prev_peer != m_port_id) {
      //   continue;
      // }

      int rid = -1;
      bool skip = false;
      if (coll_type == TREES_REDUCE_SCATTER) {
        auto p = getRouteTableId(prev_peer, m_r, prev_centers[0]);
        rid = p.first;
        std::vector<int> path_shifted = p.second;
        skip =
            (rid == -1 &&
             !same_direction(m_r, prev_peer, m_port_id, n, m_dimensions_num)) ||
            (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
      } else {
        auto p = getRouteTableId(m_r, prev_peer, prev_centers[0]);
        rid = p.first;
        std::vector<int> path_shifted = p.second;
        skip = (rid == -1 && !same_direction(m_r, prev_peer, m_port_id, n,
                                             m_dimensions_num)) ||
               (rid != -1 && m_port_id != path_shifted[1]);
      }
      if (skip) {
        continue;
      }

      // if (!same_direction(m_r, prev_peer, m_port_id, n, m_dimensions_num)) {
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
      // m_req_recv.assign(recv_peers[m_i].size(), MessageRequest());
      int prev_peer_counter = 0;
      m_recv_size = 0;
      for (auto &[prev_peer, prev_centers] : recv_peers[m_i]) {
        // if (prev_peer != m_port_id) {
        //   continue;
        // }

        int rid = -1;
        bool skip = false;
        if (coll_type == TREES_REDUCE_SCATTER) {
          auto p = getRouteTableId(prev_peer, m_r, prev_centers[0]);
          rid = p.first;
          std::vector<int> path_shifted = p.second;
          skip =
              (rid == -1 && !same_direction(m_r, prev_peer, m_port_id, n,
                                            m_dimensions_num)) ||
              (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
        } else {
          auto p = getRouteTableId(m_r, prev_peer, prev_centers[0]);
          rid = p.first;
          std::vector<int> path_shifted = p.second;
          skip = (rid == -1 && !same_direction(m_r, prev_peer, m_port_id, n,
                                               m_dimensions_num)) ||
                 (rid != -1 && m_port_id != path_shifted[1]);
        }
        if (skip) {
          continue;
        }

        // if (!same_direction(m_r, prev_peer, m_port_id, n, m_dimensions_num))
        // {
        //   continue;
        // }
        m_recv_epoch[global_stage].chunks.push_back(
            {m_recv_epoch[global_stage].slab.data() + m_recv_size,
             recv_block_size * prev_centers.size(), prev_peer});
        m_gen.enQ_irecv(evQ, m_recv_epoch[global_stage].chunks.back().ptr,
                        recv_block_size * prev_centers.size(), FLOAT, prev_peer,
                        tag, m_comm,
                        &m_req_recv[global_stage][prev_peer_counter++]);
        m_recv_size += recv_block_size * prev_centers.size();
        std::string peers_str = "[";
        for (size_t k = 0; k < prev_centers.size(); ++k) {
          peers_str += std::to_string(prev_centers[k]);
          if (k + 1 < prev_centers.size())
            peers_str += " ";
        }
        peers_str += "]";

        DPRINTF(
            "[%d] Receiving %d elements %s from %d with tag %d colltype = %s\n",
            m_r, recv_block_size * prev_centers.size(), peers_str.c_str(),
            prev_peer, tag, coll_type_str.c_str());
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

      int rid = -1;
      bool skip = false;
      if (coll_type == TREES_REDUCE_SCATTER) {
        auto p = getRouteTableId(m_r, next_peer, next_centers[0]);
        rid = p.first;
        std::vector<int> path_shifted = p.second;
        skip = (rid == -1 && !same_direction(m_r, next_peer, m_port_id, n,
                                             m_dimensions_num)) ||
               (rid != -1 && m_port_id != path_shifted[1]);
      } else {
        auto p = getRouteTableId(next_peer, m_r, next_centers[0]);
        rid = p.first;
        std::vector<int> path_shifted = p.second;
        skip =
            (rid == -1 &&
             !same_direction(m_r, next_peer, m_port_id, n, m_dimensions_num)) ||
            (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
      }
      if (skip) {
        continue;
      }

      // if (!same_direction(m_r, next_peer, m_port_id, n, m_dimensions_num)) {
      //   continue;
      // }
      total_next_peers++;
      for (int next_center : next_centers) {
        total += getBlockSize(next_center);
      }
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

        int rid = -1;
        bool skip = false;
        if (coll_type == TREES_REDUCE_SCATTER) {
          auto p = getRouteTableId(m_r, next_peer, next_centers[0]);
          rid = p.first;
          std::vector<int> path_shifted = p.second;
          skip = (rid == -1 && !same_direction(m_r, next_peer, m_port_id, n,
                                               m_dimensions_num)) ||
                 (rid != -1 && m_port_id != path_shifted[1]);
        } else {
          auto p = getRouteTableId(next_peer, m_r, next_centers[0]);
          rid = p.first;
          std::vector<int> path_shifted = p.second;
          skip =
              (rid == -1 && !same_direction(m_r, next_peer, m_port_id, n,
                                            m_dimensions_num)) ||
              (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
        }
        if (skip) {
          continue;
        }

        // if (!same_direction(m_r, next_peer, m_port_id, n, m_dimensions_num))
        // {
        //   continue;
        // }
        const int count = send_block_size * next_centers.size();
        float *out = m_send_epoch.slab.data() + m_send_size;

        if (m_validate) {
          float *cur = out;
          for (int center : next_centers) {
            if (center == 0) {
              std::string phase_str = coll_type == TREES_REDUCE_SCATTER
                                          ? "reduce-scatter"
                                          : "allgather";
              DPRINTF(
                  "[%d] Sending block 0 to peer %d at step %d on phase %s \n",
                  m_r, next_peer, m_i, phase_str.c_str());
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
        m_send_epoch.chunks.emplace_back(out, count, next_peer);
        m_gen.enQ_isend(evQ, out, count, FLOAT, next_peer, tag, m_comm,
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
        DPRINTF("[%d] Sending %d elements %s to %d with tag %d colltype = %s\n",
                m_r, send_block_size * next_centers.size(), peers_str.c_str(),
                next_peer, tag, coll_type_str.c_str());
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

        int rid = -1;
        bool skip = false;
        if (coll_type == TREES_REDUCE_SCATTER) {
          auto p = getRouteTableId(prev_peer, m_r, prev_centers[0]);
          rid = p.first;
          std::vector<int> path_shifted = p.second;
          skip =
              (rid == -1 && !same_direction(m_r, prev_peer, m_port_id, n,
                                            m_dimensions_num)) ||
              (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
        } else {
          auto p = getRouteTableId(m_r, prev_peer, prev_centers[0]);
          rid = p.first;
          std::vector<int> path_shifted = p.second;
          skip = (rid == -1 && !same_direction(m_r, prev_peer, m_port_id, n,
                                               m_dimensions_num)) ||
                 (rid != -1 && m_port_id != path_shifted[1]);
        }
        if (skip) {
          continue;
        }

        // if (!same_direction(m_r, prev_peer, m_port_id, n, m_dimensions_num))
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

          int rid = -1;
          bool skip = false;
          if (coll_type == TREES_REDUCE_SCATTER) {
            auto p = getRouteTableId(prev_peer, m_r, prev_centers[0]);
            rid = p.first;
            std::vector<int> path_shifted = p.second;
            skip = (rid == -1 && !same_direction(m_r, prev_peer, m_port_id, n,
                                                 m_dimensions_num)) ||
                   (rid != -1 &&
                    m_port_id != path_shifted[path_shifted.size() - 2]);
          } else {
            auto p = getRouteTableId(m_r, prev_peer, prev_centers[0]);
            rid = p.first;
            std::vector<int> path_shifted = p.second;
            skip = (rid == -1 && !same_direction(m_r, prev_peer, m_port_id, n,
                                                 m_dimensions_num)) ||
                   (rid != -1 && m_port_id != path_shifted[1]);
          }
          if (skip) {
            continue;
          }

          // if (!same_direction(m_r, prev_peer, m_port_id, n,
          // m_dimensions_num)) {
          //   continue;
          // }
          m_recv_epoch[global_stage].chunks.push_back(
              {m_recv_epoch[global_stage].slab.data() + m_recv_size,
               recv_block_size * prev_centers.size(), prev_peer});
          m_gen.enQ_irecv(evQ, m_recv_epoch[global_stage].chunks.back().ptr,
                          recv_block_size * prev_centers.size(), FLOAT,
                          prev_peer, tag, m_comm,
                          &m_req_recv[global_stage][prev_peer_counter++]);
          m_recv_size += recv_block_size * prev_centers.size();
          std::string peers_str = "[";
          for (size_t k = 0; k < prev_centers.size(); ++k) {
            peers_str += std::to_string(prev_centers[k]);
            if (k + 1 < prev_centers.size())
              peers_str += " ";
          }
          peers_str += "]";

          DPRINTF("[%d] Receiving %d elements %s from %d with tag %d colltype "
                  "= %s\n",
                  m_r, recv_block_size * prev_centers.size(), peers_str.c_str(),
                  prev_peer, tag, coll_type_str.c_str());
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

        int rid = -1;
        bool skip = false;
        if (coll_type == TREES_REDUCE_SCATTER) {
          auto p = getRouteTableId(m_r, next_peer, next_centers[0]);
          rid = p.first;
          std::vector<int> path_shifted = p.second;
          skip = (rid == -1 && !same_direction(m_r, next_peer, m_port_id, n,
                                               m_dimensions_num)) ||
                 (rid != -1 && m_port_id != path_shifted[1]);
        } else {
          auto p = getRouteTableId(next_peer, m_r, next_centers[0]);
          rid = p.first;
          std::vector<int> path_shifted = p.second;
          skip =
              (rid == -1 && !same_direction(m_r, next_peer, m_port_id, n,
                                            m_dimensions_num)) ||
              (rid != -1 && m_port_id != path_shifted[path_shifted.size() - 2]);
        }
        if (skip) {
          continue;
        }

        // if (!same_direction(m_r, next_peer, m_port_id, n, m_dimensions_num))
        // {
        //   continue;
        // }
        total_next_peers++;
        for (int next_center : next_centers) {
          total += getBlockSize(next_center);
        }
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

          int rid = -1;
          bool skip = false;
          if (coll_type == TREES_REDUCE_SCATTER) {
            auto p = getRouteTableId(m_r, next_peer, next_centers[0]);
            rid = p.first;
            std::vector<int> path_shifted = p.second;
            skip = (rid == -1 && !same_direction(m_r, next_peer, m_port_id, n,
                                                 m_dimensions_num)) ||
                   (rid != -1 && m_port_id != path_shifted[1]);
          } else {
            auto p = getRouteTableId(next_peer, m_r, next_centers[0]);
            rid = p.first;
            std::vector<int> path_shifted = p.second;
            skip = (rid == -1 && !same_direction(m_r, next_peer, m_port_id, n,
                                                 m_dimensions_num)) ||
                   (rid != -1 &&
                    m_port_id != path_shifted[path_shifted.size() - 2]);
          }
          if (skip) {
            continue;
          }

          // if (!same_direction(m_r, next_peer, m_port_id, n,
          // m_dimensions_num)) {
          //   continue;
          // }
          const int count = send_block_size * next_centers.size();
          float *out = m_send_epoch.slab.data() + m_send_size;

          if (m_validate) {
            float *cur = out;
            for (int center : next_centers) {
              if (center == 0) {
                std::string phase_str = coll_type == TREES_REDUCE_SCATTER
                                            ? "reduce-scatter"
                                            : "allgather";
                DPRINTF(
                    "[%d] Sending block 0 to peer %d at step %d on phase %s \n",
                    m_r, next_peer, m_i, phase_str.c_str());
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
          m_send_epoch.chunks.emplace_back(out, count, next_peer);
          m_gen.enQ_isend(evQ, out, count, FLOAT, next_peer, tag, m_comm,
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
          DPRINTF(
              "[%d] Sending %d elements %s to %d with tag %d colltype = %s\n",
              m_r, send_block_size * next_centers.size(), peers_str.c_str(),
              next_peer, tag, coll_type_str.c_str());

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
    std::vector<TreeSpec> tree_specs,
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

  int max_ports = std::min(count, ports);
  if (m_r == 0) {
    DPRINTF("[%d] Count is %d, thus we will use %d ports\n", m_r, m_count,
            max_ports);
  }

  int n = dimensions[0];
  int D = dimensions_num;
  m_stages_num = n % 2 == 1 ? n - 1 : (n / 2) * D;
  stages_waiting_centers.resize(2 * m_stages_num);
  auto m_point_coord = getCoordFromRank(m_r, n, D);

  // for (auto port_id : {getRankFromCoord(left(m_point_coord, n), n),
  //                      getRankFromCoord(right(m_point_coord, n), n),
  //                      getRankFromCoord(up(m_point_coord, n), n),
  //                      getRankFromCoord(down(m_point_coord, n), n)}) {

  //   m_allreduces.push_back(TreesCollectiveEngine(
  //       m_gen, coll_type, dimensions, dimensions_num, data, m_count, m_r,
  //       m_p, aggregation_cost_ns, comm, data != NULL, latency_optimal,
  //       port_id, this));
  // }
  for (int i = 0; i < D; ++i) {
    for (int step : {1, -1}) {
      auto port_coord = m_point_coord;
      port_coord[i] = (port_coord[i] + step + n) % n;
      int port_id = getRankFromCoord(port_coord, n, D);
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
