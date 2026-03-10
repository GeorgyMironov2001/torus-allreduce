#include "embertoruscoll.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits.h>
#include <sst/core/rng/xorshift.h>
#include <sst_config.h>
#include <tuple>
#include <utility>
#define DEBUG

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
static uint getRankFromCoord(std::pair<int, int> coord, int n) {
  return coord.first * n + coord.second;
}
static std::pair<int, int> getCoordFromRank(int rank, int n) {
  int coord[2];
  int nnodes = n * n;
  for (int i = 0; i < 2; i++) {
    nnodes = nnodes / n;
    coord[i] = rank / nnodes;
    rank = rank % nnodes;
  }
  return std::make_pair(coord[0], coord[1]);
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

static std::pair<std::vector<int>, std::vector<int>> get_peers(int center_id,
                                                               int m_r, int n) {
  int m = (n - 1) / 2;
  std::vector<int> outside_peers;
  std::vector<int> inside_peers;
  auto [Ox, Oy] = getCoordFromRank(center_id, n);
  auto center_coord = std::make_pair(Ox, Oy);
  auto [x, y] = getCoordFromRank(m_r, n);
  auto m_point = std::make_pair(x, y);
  if (x == Ox && y == Oy) {
    outside_peers = {getRankFromCoord(right(m_point, n), n),
                     getRankFromCoord(up(m_point, n), n),
                     getRankFromCoord(left(m_point, n), n),
                     getRankFromCoord(down(m_point, n), n)};
    return std::make_pair(inside_peers, outside_peers);
  }
  int x_diff =
      mod(x - Ox, n) > mod(Ox - x, n) ? mod(Ox - x, n) : -mod(x - Ox, n);
  int y_diff =
      mod(y - Oy, n) > mod(Oy - y, n) ? mod(Oy - y, n) : -mod(y - Oy, n);
  if (x_diff == 0 && y_diff == -m) {
    inside_peers = {getRankFromCoord(down(m_point, n), n)};
    outside_peers = {getRankFromCoord(left(m_point, n), n)};
    return std::make_pair(inside_peers, outside_peers);
  } else if (x_diff == 0 && y_diff == m) {
    inside_peers = {getRankFromCoord(up(m_point, n), n)};
    outside_peers = {getRankFromCoord(right(m_point, n), n)};
    return std::make_pair(inside_peers, outside_peers);
  } else if (x_diff == m && y_diff == 0) {
    inside_peers = {getRankFromCoord(right(m_point, n), n)};
    outside_peers = {getRankFromCoord(down(m_point, n), n)};
    return std::make_pair(inside_peers, outside_peers);
  } else if (x_diff == -m && y_diff == 0) {
    inside_peers = {getRankFromCoord(left(m_point, n), n)};
    outside_peers = {getRankFromCoord(up(m_point, n), n)};
    return std::make_pair(inside_peers, outside_peers);
  }

  if (x_diff >= 0 && y_diff < 0) {
    if (x_diff == m) {
      inside_peers = {getRankFromCoord(right(m_point, n), n)};
    } else if (m > x_diff && x_diff > 0) {
      inside_peers = {getRankFromCoord(right(m_point, n), n)};
      outside_peers = {getRankFromCoord(left(m_point, n), n)};
    } else if (x_diff == 0) {
      inside_peers = {getRankFromCoord(down(m_point, n), n)};
      outside_peers = {getRankFromCoord(up(m_point, n), n),
                       getRankFromCoord(left(m_point, n), n)};
    }

  } else if (x_diff < 0 && y_diff <= 0) {
    if (y_diff == -m) {
      inside_peers = {getRankFromCoord(down(m_point, n), n)};
    } else if (-m < y_diff && y_diff < 0) {
      inside_peers = {getRankFromCoord(down(m_point, n), n)};
      outside_peers = {getRankFromCoord(up(m_point, n), n)};
    } else if (y_diff == 0) {
      inside_peers = {getRankFromCoord(left(m_point, n), n)};
      outside_peers = {getRankFromCoord(right(m_point, n), n),
                       getRankFromCoord(up(m_point, n), n)};
    }
  } else if (x_diff <= 0 && y_diff > 0) {
    if (x_diff == -m) {
      inside_peers = {getRankFromCoord(left(m_point, n), n)};
    } else if (-m < x_diff && x_diff < 0) {
      inside_peers = {getRankFromCoord(left(m_point, n), n)};
      outside_peers = {getRankFromCoord(right(m_point, n), n)};
    } else if (x_diff == 0) {
      inside_peers = {getRankFromCoord(up(m_point, n), n)};
      outside_peers = {getRankFromCoord(down(m_point, n), n),
                       getRankFromCoord(right(m_point, n), n)};
    }
  } else if (x_diff > 0 && y_diff >= 0) {
    if (y_diff == m) {
      inside_peers = {getRankFromCoord(up(m_point, n), n)};
    } else if (m > y_diff && y_diff > 0) {
      inside_peers = {getRankFromCoord(up(m_point, n), n)};
      outside_peers = {getRankFromCoord(down(m_point, n), n)};
    } else if (y_diff == 0) {
      inside_peers = {getRankFromCoord(right(m_point, n), n)};
      outside_peers = {getRankFromCoord(left(m_point, n), n),
                       getRankFromCoord(down(m_point, n), n)};
    }
  }
  return std::make_pair(inside_peers, outside_peers);
}

EmberTorusCollGenerator::TorusCollectiveEngine::TorusCollectiveEngine(
    EmberTorusCollGenerator &gen, CollType coll_type, uint *dimensions,
    uint dimensions_num, float *dst, uint32_t count, uint32_t vrank,
    uint32_t numproc, double aggregation_cost_ns, Communicator comm,
    bool validate, bool latency_optimal, int port_id, TorusCollective *runner)
    : m_gen(gen), m_count(count), m_dst(dst), m_r(vrank), m_p(numproc),
      m_aggregation_cost_ns(aggregation_cost_ns), m_data_sent(0), m_comm(comm),
      m_dimensions(dimensions), m_dimensions_num(dimensions_num),
      m_validate(validate), m_enabled(true), m_latency_optimal(latency_optimal),
      m_port_id(port_id), m_runner(runner), m_data_reduced(0) {

  uint32_t block_size = (m_count >= m_p) ? m_count / m_p : m_count;
  // printf("Count1 is %d - m_p is %d - block size %d\n", count, m_p,
  // block_size);
  fflush(stdout);

  // TODO What if m_count < m_p or not divisible for m_p ???

  if (m_validate) {
    // m_tmp = (float *)malloc(gen.sizeofDataType(FLOAT) *
    //                         m_count);
    // m_tmp.resize(m_count);
    // Even though at most each rank can
    // receive a block of m_count/2
    // assert(m_tmp != NULL);
    // m_send_tmp = (float *)malloc(
    //     gen.sizeofDataType(FLOAT) *
    //     m_count); // Even though at most each rank can send a block of
    //     m_count/2
    // assert(m_send_tmp != NULL);
  } else {
    // m_tmp = NULL;
    // m_send_tmp = NULL;
  }

  // m_blocks_bitmap_s = (uint8_t *)malloc(m_p * sizeof(uint8_t));
  // m_blocks_bitmap_r = (uint8_t *)malloc(m_p * sizeof(uint8_t));

  switch (coll_type) {
  case TORUS_ALLREDUCE:
    m_do_reduce_scatter = true;
    if (m_count >= m_p && !m_latency_optimal) {
      m_do_allgather = true;
    } else {
      if (m_r == 0) {
        DPRINTF(
            "[%d] Skipping allgather (msg too small, or latency optimal "
            "required, \"reducescatter\" will be enough to do allreduce).\n",
            m_r);
      }
      m_do_allgather = false; // No need to do allgather for too small messages
                              // (all the data will be sent at each step)
    }
    break;
  case TORUS_REDUCE_SCATTER:
    m_do_reduce_scatter = true;
    m_do_allgather = false;
    break;
  case TORUS_ALLGATHER:
    m_do_reduce_scatter = false;
    m_do_allgather = true;
    break;
  default:
    assert(0);
  }

  int n = m_dimensions[0]; // assuming torus (2m+1)X(2m+1)
  int stages_num = n - 1;
  int m = (n - 1) / 2;

  auto m_coord = getCoordFromRank(m_r, n);

  scatter_peers_send.resize(stages_num);
  scatter_peers_recv.resize(stages_num);
  allgather_peers_send.resize(stages_num);
  allgather_peers_recv.resize(stages_num);

  scatter_port_send.resize(stages_num);
  scatter_port_recv.resize(stages_num);
  allgather_port_send.resize(stages_num);
  allgather_port_recv.resize(stages_num);

  // std::vector<std::vector<std::map<int, std::vector<int>>>>
  //     all_steps_scatter_send(
  //         n * n, std::vector<std::map<int, std::vector<int>>>(n - 1));
  // std::vector<std::vector<std::map<int, std::vector<int>>>>
  //     all_steps_allgather_send(
  //         n * n, std::vector<std::map<int, std::vector<int>>>(n - 1));

  for (int Ox = 0; Ox < n; ++Ox) {
    for (int Oy = 0; Oy < n; ++Oy) {
      for (int x = 0; x < n; ++x) {
        for (int y = 0; y < n; ++y) {
          if (x == Ox && y == Oy) {
            continue;
          }
          std::pair<int, int> point = std::make_pair(x, y);
          std::pair<int, int> center = std::make_pair(Ox, Oy);
          int scatter_stage = getDistanceScatter(point, center, n) - 1;
          int allgather_stage = getDistanceManhattan(point, center, n) - 1;
          int x_diff = mod(x - Ox, n) > mod(Ox - x, n) ? mod(Ox - x, n)
                                                       : -mod(x - Ox, n);
          int y_diff = mod(y - Oy, n) > mod(Oy - y, n) ? mod(Oy - y, n)
                                                       : -mod(y - Oy, n);
          auto next_point = point;
          if (x_diff >= 0 && y_diff < 0) {
            if (x_diff > 0) {
              next_point = right(point, n);
            } else {
              next_point = down(point, n);
            }
          } else if (x_diff < 0 && y_diff <= 0) {
            if (y_diff < 0) {
              next_point = down(point, n);
            } else {
              next_point = left(point, n);
            }
          } else if (x_diff <= 0 && y_diff > 0) {
            if (x_diff < 0) {
              next_point = left(point, n);
            } else {
              next_point = up(point, n);
            }
          } else if (x_diff > 0 && y_diff >= 0) {
            if (y_diff > 0) {
              next_point = up(point, n);
            } else {
              next_point = right(point, n);
            }
          }
          int point_id = getRankFromCoord(point, n);
          int next_point_id = getRankFromCoord(next_point, n);
          int center_id = getRankFromCoord(center, n);
          if (point_id == m_r) {
            scatter_peers_send[scatter_stage][next_point_id].push_back(
                center_id);
            if (next_point_id == m_port_id) {
              scatter_port_send[scatter_stage][m_port_id].push_back(center_id);
            }
            allgather_peers_recv[allgather_stage][next_point_id].push_back(
                center_id);
            if (allgather_stage ==
                stages_num - 1) { // && next_point_id == m_port_id) {
              allgather_port_recv[allgather_stage][m_port_id].push_back(
                  center_id);
            }
          }
          if (next_point_id == m_r) {
            scatter_peers_recv[scatter_stage][point_id].push_back(center_id);
            if (scatter_stage ==
                stages_num - 1) { // && point_id == m_port_id) {
              scatter_port_recv[scatter_stage][point_id].push_back(center_id);
            }
            allgather_peers_send[allgather_stage][point_id].push_back(
                center_id);
            if (point_id == m_port_id) {
              allgather_port_send[allgather_stage][m_port_id].push_back(
                  center_id);
            }
          }
        }
      }
    }
  }

  for (int stage = 0; stage < stages_num - 1; ++stage) {
    for (int neighbour_id :
         std::array<int, 4>{getRankFromCoord(left(m_coord, n), n),
                            getRankFromCoord(right(m_coord, n), n),
                            getRankFromCoord(up(m_coord, n), n),
                            getRankFromCoord(down(m_coord, n), n)}) {

      for (int center_id : scatter_peers_recv[stage][neighbour_id]) {
        auto [inside_peers, outside_peers] = get_peers(center_id, m_r, n);
        assert(inside_peers.size() == 1);
        int inside_peer = inside_peers[0];
        if (inside_peer == m_port_id) {
          scatter_port_recv[stage][neighbour_id].push_back(center_id);
        }
      }
    }
  }

  for (int stage = 0; stage < stages_num - 1; ++stage) {
    for (int neighbour_id :
         std::array<int, 4>{getRankFromCoord(left(m_coord, n), n),
                            getRankFromCoord(right(m_coord, n), n),
                            getRankFromCoord(up(m_coord, n), n),
                            getRankFromCoord(down(m_coord, n), n)}) {
      for (int center_id : allgather_peers_recv[stage][neighbour_id]) {
        auto [inside_peers, outside_peers] = get_peers(center_id, m_r, n);
        if (std::find(outside_peers.begin(), outside_peers.end(), m_port_id) !=
            outside_peers.end()) {
          allgather_port_recv[stage][neighbour_id].push_back(center_id);
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

  reset();
}

// uint8_t **EmberTorusCollGenerator::TorusCollectiveEngine::getBitmaps(int
// rank) {
//   uint num_steps = ceil(log2(m_p));
//   uint8_t **bitmaps = (uint8_t **)malloc(sizeof(uint8_t *) * num_steps);
//   // Bit vector that says if rank reached another node
//   uint8_t *m_reached_step = (uint8_t *)malloc(sizeof(uint8_t) * m_p);
//   memset(m_reached_step, num_steps,
//          sizeof(uint8_t) *
//              m_p); // Init with num_steps to denote it didn't reach
//   for (size_t step = 0; step < num_steps; step++) {
//     bitmaps[step] = (uint8_t *)malloc(sizeof(uint8_t) * m_p);
//     memset(bitmaps[step], 0, sizeof(uint8_t) * m_p);
//     int dest = m_peers[rank][step];
//     bitmaps[step][dest] = 1; // I'll send its block
//     computeBlocksBitmap(
//         dest, step + 1,
//         bitmaps[step]);      // ... plus those it will send in the next
//         steps.
//     bitmaps[step][rank] = 0; // I can never reach myself (this could happen
//                              // sometimes in the non-power-of-2 case)
//     for (size_t i = 0; i < m_p; i++) {
//       if (bitmaps[step][i]) {
//         // Is there any peer I already reached before? If so, delete the
//         earlier
//         // reach
//         if (m_reached_step[i] != num_steps) {
//           int prev_reached_step = m_reached_step[i];
//           bitmaps[prev_reached_step][i] = 0;
//         }
//         m_reached_step[i] = step;
//       }
//     }
//   }
//   free(m_reached_step);
//   return bitmaps;
// }

void EmberTorusCollGenerator::TorusCollectiveEngine::setEnable(bool enable) {
  m_enabled = enable;
}

bool EmberTorusCollGenerator::TorusCollectiveEngine::isEnabled() {
  return m_enabled;
}

bool EmberTorusCollGenerator::TorusCollectiveEngine::progress(
    std::queue<EmberEvent *> &evQ) {
  if (m_r == 1 && m_state == ALL_GATHER) {
    int q = 1;
  }
  switch (m_state) {
  case REDUCE_SCATTER:
    if (collective(evQ, TORUS_REDUCE_SCATTER)) {
      m_state = ALL_GATHER;
      // return false;
    } else {
      return false;
    }
  case ALL_GATHER:
    if (collective(evQ, TORUS_ALLGATHER)) {
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

uint64_t EmberTorusCollGenerator::TorusCollectiveEngine::getMovedBytes() {
  return m_data_sent;
}

void EmberTorusCollGenerator::TorusCollectiveEngine::setBuff(float *new_dest) {
  m_dst = new_dest;
}

float *EmberTorusCollGenerator::TorusCollectiveEngine::getBuff() {
  return m_dst;
}

bool EmberTorusCollGenerator::TorusCollectiveEngine::hasPendingRecv() {
  if (m_i == 0) {
    return false;
  }
  int n = m_dimensions[0];
  int global_stage = m_i + (m_state == REDUCE_SCATTER ? 0 : n - 1);
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
bool EmberTorusCollGenerator::TorusCollectiveEngine::hasPendingSend() {
  return waiting_send() && m_ready_to_send;
}

std::pair<std::vector<MessageRequest>, std::vector<std::pair<int, int>>>
EmberTorusCollGenerator::TorusCollectiveEngine::getRecvHandle() {
  std::vector<MessageRequest> active_received_handlers(0);
  std::vector<std::pair<int, int>> active_received_handlers_ids(0);
  int n = m_dimensions[0];
  int global_stage = m_i + (m_state == REDUCE_SCATTER ? 0 : n - 1);
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
// EmberTorusCollGenerator::TorusCollectiveEngine::getSendHandle() {
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
void EmberTorusCollGenerator::TorusCollectiveEngine::reset() {
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
  int stages_num = n - 1;
  m_req_recv.resize(2 * stages_num);
  m_req_send.resize(2 * stages_num);
  m_recv_epoch.resize(2 * stages_num);
  // m_req_recv.assign(4, MessageRequest());
  // m_req_send.assign(4, MessageRequest());
}

void EmberTorusCollGenerator::TorusCollectiveEngine::notifyRecv() {
  // m_waiting_recv.assign(m_waiting_recv.size(), false);
  // m_ready_to_recv = false;
  // // m_req_recv = 0;
  // m_req_recv.assign(m_req_recv.size(), MessageRequest());
}
void EmberTorusCollGenerator::TorusCollectiveEngine::notifyRecv(
    std::pair<int, int> chunk, std::queue<EmberEvent *> &evQ) {
  assert(m_i > 0);
  int n = m_dimensions[0];
  auto [chunk_stage, chunk_id] = chunk;
  CollType coll_type =
      chunk_stage < n - 1 ? TORUS_REDUCE_SCATTER : TORUS_ALLGATHER;
  auto &recv_peers = coll_type == TORUS_REDUCE_SCATTER ? scatter_peers_recv
                                                       : allgather_peers_recv;
  // int global_stage =
  //     chunk_stage + (coll_type == TORUS_REDUCE_SCATTER ? 0 : n - 1);
  // m_waiting_recv[chunk_id] = false;
  if (m_r == 5 && m_port_id == 0 && chunk_stage == 2) {
    int q = 1;
  }
  if (m_r == 0 && m_port_id == 4 && chunk_stage == 1) {
    int q = 1;
  }
  for (int come_center_id :
       recv_peers[(chunk_stage < n - 1 ? chunk_stage : chunk_stage - (n - 1))]
                 [m_port_id]) {
    // if (m_r == 0 && global_stage == 3) {
    //   int q = 1;
    // }
    m_runner->stages_waiting_centers[chunk_stage][come_center_id]--;
  }
  // m_ready_to_recv = waiting_receive();
  // m_req_recv = 0;
  m_req_recv[chunk_stage][chunk_id] = 0;
  // processReceivedData(
  //     evQ, coll_type,
  //     (chunk_stage < n - 1 ? chunk_stage : chunk_stage - (n - 1)), chunk_id);
  processReceivedData(evQ, coll_type, chunk_stage, chunk_id);
}

// void EmberTorusCollGenerator::TorusCollectiveEngine::notifySend(int chunk_id)
// {
//   m_waiting_send[chunk_id] = false;
//   m_ready_to_send = waiting_send();
//   // m_req_send = 0;
//   m_req_send[chunk_id] = 0;
// }
void EmberTorusCollGenerator::TorusCollectiveEngine::processReceivedData(
    std::queue<EmberEvent *> &evQ, CollType coll_type, int chunk_stage,
    int chunk_id) {
  auto &recv_peers = coll_type == TORUS_REDUCE_SCATTER ? scatter_peers_recv
                                                       : allgather_peers_recv;
  if (coll_type == TORUS_REDUCE_SCATTER) {
    m_data_reduced += m_recv_size * m_gen.sizeofDataType(FLOAT);
  }
  int n = m_dimensions[0];
  int small_stage = chunk_stage < n - 1 ? chunk_stage : chunk_stage - (n - 1);
  if (m_validate) {
    int peer_id = 0;
    for (auto &[peer, centers] : recv_peers[small_stage]) {
      if (peer != m_port_id) {
        continue;
      }
      auto chunk = m_recv_epoch[chunk_stage].chunks[peer_id++];
      std::vector<float> received_data(chunk.ptr, chunk.ptr + chunk.size);
      for (int i = 0; i < centers.size(); i++) {
        int center_id = centers[i];
        uint32_t recv_block_offset = getBlockOffset(center_id);
        uint32_t recv_block_size = getBlockSize(center_id);

        for (uint32_t j = recv_block_offset;
             j < recv_block_offset + recv_block_size; j++) {
          if (coll_type == TORUS_REDUCE_SCATTER) {
            m_dst[j] +=
                received_data[j - recv_block_offset + i * recv_block_size];
          } else {
            m_dst[j] =
                received_data[j - recv_block_offset + i * recv_block_size];
          }
        }
      }
    }
    // for (auto chunk : m_recv_epoch.chunks) {
    //   std::vector<float> received_data(chunk.ptr, chunk.ptr + chunk.size);
    //   int block_id = recv_peers[m_i-1]
    // }
  } else {
    if (m_aggregation_cost_ns != 0 && coll_type == TORUS_REDUCE_SCATTER) {
      m_gen.enQ_compute(evQ, m_aggregation_cost_ns * m_recv_size);
      return;
    }
  }
}

uint32_t EmberTorusCollGenerator::TorusCollectiveEngine::getBlockOffset(
    uint32_t block_idx) {
  if (m_count < m_p)
    return 0;

  uint32_t block_size = m_count / m_p;
  return block_idx * block_size;
}

uint32_t EmberTorusCollGenerator::TorusCollectiveEngine::getBlockSize(
    uint32_t block_idx) {
  if (m_count < m_p)
    return m_count;

  uint32_t block_size = m_count / m_p;
  uint32_t num_blocks = m_p;
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

// Convert a rank id into a list of d-dimensional coordinates (adapted from
// MPICH code --
// https://github.com/pmodels/mpich/blob/94b1cd6f060cafbf68d6d83ea551a8bcc8fcecd4/src/mpi/topo/topo_impl.c)
void EmberTorusCollGenerator::TorusCollectiveEngine::getCoordFromId(
    int id, int *coord) {
  int nnodes = m_p;
  for (int i = 0; i < m_dimensions_num; i++) {
    nnodes = nnodes / m_dimensions[i];
    coord[i] = id / nnodes;
    id = id % nnodes;
  }
  /*
  if(m_dimensions_num == 1){
      coord[0] = id;
  }else if(m_dimensions_num == 2){
      coord[0] = id / m_dimensions[1];
      coord[1] = id % m_dimensions[1];
  }else if(m_dimensions_num == 3){
      coord[0] = (id / m_dimensions[1]) % m_dimensions[0];
      coord[1] = id % m_dimensions[1];
      coord[2] = id / (m_dimensions[0]*m_dimensions[1]);
  }
  */
}

// Convert d-dimensional coordinates into a rank id (adapted from MPICH code
// --
// https://github.com/pmodels/mpich/blob/94b1cd6f060cafbf68d6d83ea551a8bcc8fcecd4/src/mpi/topo/topo_impl.c)
int EmberTorusCollGenerator::TorusCollectiveEngine::getIdFromCoord(
    int *coords, uint *dimensions, uint dimensions_num) {
  int rank = 0;
  int multiplier = 1;
  int coord;
  for (int i = dimensions_num - 1; i >= 0; i--) {
    coord = coords[i];
    if (/*cart_ptr->topo.cart.periodic[i]*/ 1) {
      if (coord >= dimensions[i])
        coord = coord % dimensions[i];
      else if (coord < 0) {
        coord = coord % dimensions[i];
        if (coord)
          coord = dimensions[i] + coord;
      }
    }
    rank += multiplier * coord;
    multiplier *= dimensions[i];
  }
  return rank;
  /*
  if(dimensions_num == 1){
      return coord[0];
  }else if(dimensions_num == 2){
      return coord[0]*dimensions[1] + coord[1];
  }else if(dimensions_num == 3){
      return int(coord[2]*(dimensions[0]*dimensions[1]) +
  getIdFromCoord(coord, dimensions, dimensions_num - 1)); }else{ return -1;
  }
  */
}

// Gets the peer of rank 'sender' at the step-th step on a
// dimension-dimensional network, considering the next_direction int
// EmberTorusCollGenerator::TorusCollectiveEngine::getPeer(
//     int sender, int step, CollType collective) {
//   // std::cout << "Sender " << sender << " Port " << m_port << " Peers: "
//   <<
//   // m_peers[sender][0] << " " << m_peers[sender][1] << " " <<
//   // m_peers[sender][2] << " " << m_peers[sender][3] << " " <<
//   // m_peers[sender][4] << " " << m_peers[sender][5] << std::endl;

//   // At this point we now in which direction is the peer and at which step
//   // relative to that dimension we are.
//   size_t index;
//   if (collective == TORUS_REDUCE_SCATTER) {
//     index = step;
//   } else {
//     index = ceil(log2(m_p)) - step - 1;
//   }
//   uint32_t id = m_peers[sender][index];
//   DPRINTF("Sender %d Port %d Step %d peer %d coll %d\n", sender, m_port,
//   step,
//           id, collective);
//   return id;
// }

// void EmberTorusCollGenerator::TorusCollectiveEngine::computeBlocksBitmap(
//     int sender, int step, uint8_t *blocks_bitmap) {
//   if (step >= ceil(log2(m_p))) { // Base case
//     return;
//   } else {
//     for (size_t s = step; s < ceil(log2(m_p)); s++) {
//       int peer = m_peers[sender][s];
//       blocks_bitmap[peer] = 1;
//       computeBlocksBitmap(peer, s + 1, blocks_bitmap);
//     }
//     return;
//   }
// }
// void EmberTorusCollGenerator::TorusCollectiveEngine::scatter(
//     std::queue<EmberEvent *> &evQ) {
//   int n = m_dimensions[0]; // assuming torus (2m+1)X(2m+1)
//   int stages_num = n - 1;
//   int m = (n - 1) / 2;
// #ifdef DEBUG
//   DPRINTF("[%d] Blocks Bitmap (Send) at step %d: ", m_r, m_i);
//   for (size_t i = 0; i < m_p; i++) {
//     DPRINTF("%d ", m_blocks_bitmap_s[i]);
//   }
//   DPRINTF("\n");

//   DPRINTF("[%d] Blocks Bitmap (Recv) at step %d: ", m_r, m_i);
//   for (size_t i = 0; i < m_p; i++) {
//     DPRINTF("%d ", m_blocks_bitmap_r[i]);
//   }
//   DPRINTF("\n");
// #endif

//   /********/
//   /* Recv */
//   /********/

//   m_recv_epoch.chunks.clear();
//   size_t total = 0;
//   for (auto &[prev_peer, prev_centers] : m_peers_scatter_recv[m_r][m_i]) {
//     for (int prev_center : prev_centers) {
//       uint32_t recv_block_size = getBlockSize(prev_center);
//       total += recv_block_size;
//     }
//   }
//   m_recv_epoch.slab.resize(total);
//   m_recv_size = 0;
//   for (auto &[prev_peer, prev_centers] : m_peers_scatter_recv[m_r][m_i]) {
//     size_t curr_recv_size = 0;
//     for (int prev_center : prev_centers) {
//       uint32_t recv_block_size = getBlockSize(prev_center);
//       curr_recv_size += recv_block_size;
//       if (curr_recv_size == m_count) {
//         break;
//       }
//     }
//     m_recv_epoch.chunks.emplace_back(m_recv_epoch.slab.data() +
//     m_recv_size,
//                                      curr_recv_size, prev_peer, {});
//     m_gen.enQ_irecv(evQ, m_recv_epoch.chunks.back().ptr, curr_recv_size,
//     FLOAT,
//                     prev_peer, tag, m_comm, &m_recv_epoch.chunks.req);
//     m_recv_size += curr_recv_size;

//     DPRINTF("[%d] Receiving %d elements from %d with tag %d on port %d\n",
//             m_gen.rank(), curr_recv_size, prev_peer, tag, m_port);
//     // Posting irecv
//     // m_gen.enQ_irecv(evQ, &m_tmp[0], m_recv_size, FLOAT, prev_peer, tag,
//     //                 m_comm, &m_req_recv);
//   }
//   m_waiting_recv = true;

//   /********/
//   /* Send */
//   /********/

//   m_send_epoch.chunks.clear();
//   m_send_epoch.slab.resize(total);
//   m_send_size = 0;
//   int pos = 0;
//   for (auto &[next_peer, next_centers] : m_peers_scatter_send[m_r][m_i]) {
//     size_t curr_send_size = 0;
//     for (int next_center : next_centers) {
//       uint32_t send_buff_size = getBlockSize(next_center);
//       uint32_t send_buff_offset = getBlockOffset(next_center);
//       // if (m_send_size == m_count) {
//       //   break;
//       // }
//       memcpy(m_send_epoch.slab.data() + m_send_size, m_dst +
//       send_buff_offset,
//              send_buff_size * m_gen.sizeofDataType(FLOAT));

//       m_send_size += send_buff_size;
//       curr_send_size += send_buff_size;
//     }
//     m_send_epoch.chunks.emplace_back(m_send_epoch.slab.data() + pos,
//                                      curr_send_size, next_peer, {});
//     m_gen.enQ_isend(evQ, m_send_epoch.chunks.back().ptr, curr_send_size,
//     FLOAT,
//                     next_peer, tag, m_comm,
//                     &m_send_epoch.chunks.back().req);
//     pos += curr_send_size;
//   }
// }
bool EmberTorusCollGenerator::TorusCollectiveEngine::waiting_receive(
    CollType coll_type) {
  auto &recv_peers = (coll_type == TORUS_REDUCE_SCATTER) ? scatter_port_recv
                                                         : allgather_port_recv;
  int n = m_dimensions[0];
  assert(m_i > 0);
  if (m_r == 5 && m_i == 2 && coll_type == TORUS_REDUCE_SCATTER &&
      m_port_id == 10) {
    int q = 1;
  }
  if (m_r == 5 && m_i == 3 && coll_type == TORUS_REDUCE_SCATTER &&
      m_port_id == 10) {
    int q = 1;
  }
  if (m_r == 0 && m_port_id == 5 && m_i == 2) {
    int q = 1;
  }
  int global_stage = m_i - 1 + (coll_type == TORUS_REDUCE_SCATTER ? 0 : n - 1);
  for (auto &[prev_peer, prev_peer_centers] : recv_peers[m_i - 1]) {
    for (int center_id : prev_peer_centers) {
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
  return false;
  // for (auto waiting_recv : m_waiting_recv) {
  //   if (waiting_recv) {
  //     return true;
  //   }
  // }
  // return false;
}
bool EmberTorusCollGenerator::TorusCollectiveEngine::waiting_send() {
  for (auto waiting_send : m_waiting_send) {
    if (waiting_send) {
      return true;
    }
  }
  return false;
}
bool EmberTorusCollGenerator::TorusCollectiveEngine::collective(
    std::queue<EmberEvent *> &evQ, CollType coll_type) {

  auto tag = coll_type == TORUS_REDUCE_SCATTER ? m_tag_reduce : m_tag_broadcast;
  std::string coll_type_str =
      coll_type == TORUS_REDUCE_SCATTER ? "reducescatter" : "allgather";
  auto &send_peers = coll_type == TORUS_REDUCE_SCATTER ? scatter_peers_send
                                                       : allgather_peers_send;
  auto &recv_peers = coll_type == TORUS_REDUCE_SCATTER ? scatter_peers_recv
                                                       : allgather_peers_recv;

  auto &recv_ports = coll_type == TORUS_REDUCE_SCATTER ? scatter_port_recv
                                                       : allgather_port_recv;
  int n = m_dimensions[0];
  int m = (n - 1) / 2;
  int global_stage = m_i + (coll_type == TORUS_REDUCE_SCATTER ? 0 : n - 1);
  DPRINTF("Starting step %d on rank %d, %s\n", m_i, m_r, coll_type_str.c_str());
  fflush(stdout);
  if (m_i == 1 && m_r == 0 && coll_type == TORUS_ALLGATHER) {
    int q = 1;
  }

  if (m_i > 0 && waiting_receive(coll_type)) {
    m_ready_to_recv = true;
    return false;
  }

  auto current_time = m_gen.getCurrentSimTimeNano();
  double send_size_time = m_send_size * m_gen.sizeofDataType(FLOAT) / 400.0;
  DPRINTF("[%d] port %d, step %d, phase %s, current time %" PRIu64 " "
          "send_size_time %f \n",
          m_r, m_port_id, m_i,
          coll_type == TORUS_REDUCE_SCATTER ? "reduce-scatter" : "allgather",
          current_time, send_size_time);
  fflush(stdout);
  if (m_i == 0) {
    // recv//
    int total = 0;
    int recv_block_size = getBlockSize(0);
    for (auto &[prev_peer, prev_centers] : recv_peers[m_i]) {
      if (prev_peer != m_port_id) {
        continue;
      }
      for (int prev_center : prev_centers) {
        total += getBlockSize(prev_center);
      }
    }
    m_recv_epoch[global_stage].chunks.clear();
    m_recv_epoch[global_stage].slab.resize(total);
    // m_waiting_recv.assign(recv_peers[m_i].size(), true);
    m_waiting_recv_centers = recv_peers[m_i][m_port_id];
    m_runner->addWaitingCenters(m_i, recv_peers[m_i][m_port_id], coll_type);
    // m_req_recv.assign(recv_peers[m_i].size(), MessageRequest());
    m_req_recv[global_stage].assign(1, MessageRequest());
    int prev_peer_counter = -1;
    m_recv_size = 0;
    for (auto &[prev_peer, prev_centers] : recv_peers[m_i]) {
      if (prev_peer != m_port_id) {
        continue;
      }
      prev_peer_counter++;
      m_recv_epoch[global_stage].chunks.push_back(
          {m_recv_epoch[global_stage].slab.data() + m_recv_size,
           recv_block_size * prev_centers.size(), prev_peer});
      m_gen.enQ_irecv(evQ, m_recv_epoch[global_stage].chunks.back().ptr,
                      recv_block_size * prev_centers.size(), FLOAT, prev_peer,
                      tag, m_comm,
                      &m_req_recv[global_stage][prev_peer_counter]);
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

    // send//
    if (m_r == 3 && m_i == 0 && coll_type == TORUS_ALLGATHER &&
        m_port_id == 0) {
      int q = 1;
    }
    total = 0;
    int send_block_size = getBlockSize(0);
    for (auto &[next_peer, next_centers] : send_peers[m_i]) {
      if (next_peer != m_port_id) {
        continue;
      }
      for (int next_center : next_centers) {
        total += getBlockSize(next_center);
      }
    }
    m_send_epoch.chunks.clear();
    m_send_epoch.slab.resize(total);
    // m_req_send[m_i].assign(send_peers[m_i].size(), MessageRequest());
    m_req_send[global_stage].assign(1, MessageRequest());
    m_send_size = 0;
    size_t peer_idx = 0;
    for (auto &[next_peer, next_centers] : send_peers[m_i]) {
      if (next_peer != m_port_id) {
        continue;
      }
      const int count = send_block_size * next_centers.size();
      float *out = m_send_epoch.slab.data() + m_send_size;

      if (m_validate) {
        float *cur = out;
        for (int center : next_centers) {
          if (center == 0) {
            std::string phase_str = coll_type == TORUS_REDUCE_SCATTER
                                        ? "reduce-scatter"
                                        : "allgather";
            DPRINTF("[%d] Sending block 0 to peer %d at step %d on phase %s \n",
                    m_r, next_peer, m_i, phase_str.c_str());
            fflush(stdout);
          }

          std::memcpy(cur, m_dst + getBlockOffset(center),
                      send_block_size * m_gen.sizeofDataType(FLOAT));
          cur += send_block_size;
        }
      }

      m_send_epoch.chunks.emplace_back(out, count, next_peer);
      m_gen.enQ_isend(evQ, out, count, FLOAT, next_peer, tag, m_comm,
                      &m_req_send[global_stage][peer_idx++]);
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
  } else {
    if (m_r == 5 && m_port_id == 10 && coll_type == TORUS_REDUCE_SCATTER &&
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

    if (m_i < n - 1) {
      // recv//
      if (m_r == 0 && m_port_id == 4 && m_i == 1) {
        int q = 1;
      }
      int total = 0;
      int recv_block_size = getBlockSize(0);
      for (auto &[prev_peer, prev_centers] : recv_peers[m_i]) {
        if (prev_peer != m_port_id) {
          continue;
        }
        for (int prev_center : prev_centers) {
          total += getBlockSize(prev_center);
        }
      }
      m_recv_epoch[global_stage].chunks.clear();
      m_recv_epoch[global_stage].slab.resize(total);
      // m_waiting_recv.assign(recv_peers[m_i].size(), true);
      m_waiting_recv_centers = recv_peers[m_i][m_port_id];
      m_runner->addWaitingCenters(m_i, recv_peers[m_i][m_port_id], coll_type);
      // m_req_recv.assign(recv_peers[m_i].size(), MessageRequest());
      m_req_recv[global_stage].assign(1, MessageRequest());
      int prev_peer_counter = 0;
      m_recv_size = 0;
      for (auto &[prev_peer, prev_centers] : recv_peers[m_i]) {
        if (prev_peer != m_port_id) {
          continue;
        }
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

        DPRINTF("[%d] Receiving %d elements %s from %d with tag %d colltype "
                "= %s\n",
                m_r, recv_block_size * prev_centers.size(), peers_str.c_str(),
                prev_peer, tag, coll_type_str.c_str());
        fflush(stdout);
      }

      // send//
      if (m_r == 6 && m_port_id == 0 && coll_type == TORUS_ALLGATHER &&
          m_i == 1) {
        int q = 1;
      }
      if (m_r == 20 && m_port_id == 21 && coll_type == TORUS_ALLGATHER &&
          m_i == 1) {
        int q = 1;
      }
      if (m_r == 4 && m_port_id == 0 && m_i == 1) {
        int q = 1;
      }
      total = 0;
      int send_block_size = getBlockSize(0);
      for (auto &[next_peer, next_centers] : send_peers[m_i]) {
        if (next_peer != m_port_id) {
          continue;
        }
        for (int next_center : next_centers) {
          total += getBlockSize(next_center);
        }
      }
      m_send_epoch.chunks.clear();
      m_send_epoch.slab.resize(total);
      // m_req_send[m_i].assign(send_peers[m_i].size(), MessageRequest());
      m_req_send[global_stage].assign(1, MessageRequest());
      m_send_size = 0;
      size_t peer_idx = 0;
      for (auto &[next_peer, next_centers] : send_peers[m_i]) {
        if (next_peer != m_port_id) {
          continue;
        }
        const int count = send_block_size * next_centers.size();
        float *out = m_send_epoch.slab.data() + m_send_size;

        if (m_validate) {
          float *cur = out;
          for (int center : next_centers) {
            if (center == 0) {
              std::string phase_str = coll_type == TORUS_REDUCE_SCATTER
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

        m_send_epoch.chunks.emplace_back(out, count, next_peer);
        m_gen.enQ_isend(evQ, out, count, FLOAT, next_peer, tag, m_comm,
                        &m_req_send[global_stage][peer_idx++]);
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
  }

  m_i++;
  if (m_i < n)
    return false;
  else {
    m_i = 0;
    // for (int i = 0; i < n-1; i++) {
    //   m_req_recv[i].clear();
    //   m_req_send[i].clear();
    // }
    return true;
  }
}

// bool EmberTorusCollGenerator::TorusCollectiveEngine::collective(
//     std::queue<EmberEvent *> &evQ, CollType coll_type) {
//   if (m_p <= 1) {
//     return true;
//   }
//   int n = m_dimensions[0]; // assuming torus (2m+1)X(2m+1)
//   int stages_num = n - 1;
//   int m = (n - 1) / 2;
//   assert(coll_type == TORUS_REDUCE_SCATTER || coll_type ==
//   TORUS_ALLGATHER); uint32_t tag; if (coll_type == TORUS_REDUCE_SCATTER) {
//     tag = m_tag1;
//   } else {
//     tag = m_tag2;
//   }
//   auto &receive_peers =
//       coll_type == TORUS_REDUCE_SCATTER ? m_outside_peers : m_inside_peers;
//   auto &send_peers =
//       coll_type == TORUS_REDUCE_SCATTER ? m_inside_peers : m_outside_peers;

//   if (m_i == 0) {
//     m_waiting_recv.assign(receive_peers.size(), false);
//   }
//   DPRINTF("[%d] Starting step %d\n", m_r, m_i);
//   // If we only have one rank, we do not need to do anything

//   if (waiting_receive()) {
//     m_ready_to_recv = true;
//     return false;
//   }
//   if (m_i == 1 && coll_type == TORUS_ALLGATHER && receive_peers.empty()) {
//     m_gen.enQ_waitall(evQ, m_req_send.size(), m_req_send.data(), NULL);
//     return true;
//   }
//   // Not to be done on first iteration
//   if (m_i == 1) {
//     m_waiting_recv.assign(receive_peers.size(), false);
//     processReceivedData(evQ, coll_type);
//   }
//   if (m_i == 0 && receive_peers.empty()) {
//     if (send_peers.size() > 0) {
//       uint32_t send_block_size = getBlockSize(m_center_id);
//       m_send_epoch.chunks.clear();
//       m_req_send.assign(send_peers.size(), MessageRequest());
//       m_send_epoch.slab.resize(send_block_size * send_peers.size());

//       m_send_size = 0;
//       for (int i = 0; i < send_peers.size(); i++) {
//         if (m_validate) {
//           std::memcpy(m_send_epoch.slab.data() + m_send_size,
//                       m_dst + getBlockOffset(m_center_id),
//                       send_block_size * m_gen.sizeofDataType(FLOAT));
//         }
//         m_send_epoch.chunks.push_back({m_send_epoch.slab.data() +
//         m_send_size,
//                                        send_block_size, send_peers[i],
//                                        MessageRequest()});
//         m_gen.enQ_isend(evQ, m_send_epoch.chunks.back().ptr,
//         send_block_size,
//                         FLOAT, send_peers[i], -m_center_id, m_comm,
//                         &m_req_send[i]);
//         m_send_size += send_block_size;
//         DPRINTF("[%d] Sending %d elements to %d with tag %d on port %d\n",
//                 m_gen.rank(), m_send_size, send_peers[i], -m_center_id,
//                 m_center_id);
//       }
//       m_data_sent += m_send_size;
//     }
//     if (coll_type == TORUS_ALLGATHER) {
//       m_i++;
//       return false;
//     } else if (coll_type == TORUS_REDUCE_SCATTER) {
//       m_i = 0;
//       return true;
//     }
//   } else if (m_i == 0 && receive_peers.size() > 0) {
//     if (coll_type == TORUS_ALLGATHER) {
//       m_gen.enQ_waitall(evQ, m_req_send.size(), m_req_send.data(), NULL);
//     }
//     m_req_recv.assign(receive_peers.size(), MessageRequest());
//     uint32_t recv_block_size = getBlockSize(m_center_id);
//     m_recv_epoch.chunks.clear();
//     m_recv_epoch.slab.resize(recv_block_size * receive_peers.size());
//     m_recv_size = 0;
//     for (int i = 0; i < receive_peers.size(); i++) {
//       m_recv_epoch.chunks.push_back({m_recv_epoch.slab.data() +
//       m_recv_size,
//                                      recv_block_size, receive_peers[i],
//                                      MessageRequest()});
//       m_gen.enQ_irecv(evQ, m_recv_epoch.chunks.back().ptr, recv_block_size,
//                       FLOAT, receive_peers[i], -m_center_id, m_comm,
//                       &m_req_recv[i]);
//       m_recv_size += recv_block_size;
//       m_waiting_recv[i] = true;
//       DPRINTF("[%d] Receiving %d elements from %d with tag %d on port
//       %d\n",
//               m_gen.rank(), recv_block_size, receive_peers[i], tag,
//               m_center_id);
//     }
//     m_i++;
//     return false;
//   }
//   if (m_i == 1) {
//     if (send_peers.size() > 0) {
//       uint32_t send_block_size = getBlockSize(m_center_id);
//       m_send_epoch.chunks.clear();
//       m_req_send.assign(send_peers.size(), MessageRequest());
//       m_send_epoch.slab.resize(send_block_size * send_peers.size());

//       m_send_size = 0;
//       for (int i = 0; i < send_peers.size(); i++) {
//         if (m_validate) {
//           std::memcpy(m_send_epoch.slab.data() + m_send_size,
//                       m_dst + getBlockOffset(m_center_id),
//                       send_block_size * m_gen.sizeofDataType(FLOAT));
//         }
//         m_send_epoch.chunks.push_back({m_send_epoch.slab.data() +
//         m_send_size,
//                                        send_block_size, send_peers[i],
//                                        MessageRequest()});
//         m_gen.enQ_isend(evQ, m_send_epoch.chunks.back().ptr,
//         send_block_size,
//                         FLOAT, send_peers[i], -m_center_id, m_comm,
//                         &m_req_send[i]);
//         m_send_size += send_block_size;
//         DPRINTF("[%d] Sending %d elements to %d with tag %d on port %d\n",
//                 m_gen.rank(), m_send_size, send_peers[i], -m_center_id,
//                 m_center_id);
//       }
//       m_data_sent += m_send_size;
//     }
//     if (coll_type == TORUS_ALLGATHER && send_peers.empty()) {
//       return true;
//     }
//     if (coll_type == TORUS_ALLGATHER) {
//       m_i++;
//       return false;
//     } else if (coll_type == TORUS_REDUCE_SCATTER) {
//       m_i = 0;
//       return true;
//     }
//   }
//   if (m_i == 2) {
//     if (coll_type == TORUS_ALLGATHER) {
//       m_gen.enQ_waitall(evQ, m_req_send.size(), m_req_send.data(), NULL);
//       return true;
//     }
//   }
//   /********************************************************************/
//   /* To be done on all iterations except the last, posting send/recv. */
//   /********************************************************************/

//   // #ifdef DEBUG
//   //   DPRINTF("[%d] Blocks (Send) in allreduce %d: ", m_r, m_center_id);
//   //   for (int next_peer : send_peers) {
//   //     DPRINTF("%d ", next_peer);
//   //   }
//   //   DPRINTF("\n");

//   //   DPRINTF("[%d] Blocks (Recv) in allreduce %d: ", m_r, m_center_id);
//   //   for (int prev_peer : receive_peers) {
//   //     DPRINTF("%d ", prev_peer);
//   //   }
//   //   DPRINTF("\n");
//   // #endif
//   /********/
//   /* Recv */
//   /********/
//   // if (receive_peers.size() > 0) {
//   //   m_req_recv.assign(receive_peers.size(), MessageRequest());
//   //   uint32_t recv_block_size = getBlockSize(m_center_id);
//   //   m_recv_epoch.chunks.clear();
//   //   m_recv_epoch.slab.resize(recv_block_size * receive_peers.size());
//   //   m_recv_size = 0;
//   //   for (int i = 0; i < receive_peers.size(); i++) {
//   //     m_recv_epoch.chunks.push_back({m_recv_epoch.slab.data() +
//   m_recv_size,
//   //                                    recv_block_size, receive_peers[i],
//   //                                    MessageRequest()});
//   //     m_gen.enQ_irecv(evQ, m_recv_epoch.chunks.back().ptr,
//   recv_block_size,
//   //                     FLOAT, receive_peers[i], tag, m_comm,
//   &m_req_recv[i]);
//   //     m_recv_size += recv_block_size;
//   //     m_waiting_recv[i] = true;
//   //     DPRINTF("[%d] Receiving %d elements from %d with tag %d on port
//   %d\n",
//   //             m_gen.rank(), recv_block_size, receive_peers[i], tag,
//   //             m_center_id);
//   //   }
//   // }

//   // /********/
//   // /* Send */
//   // /********/

//   // if (send_peers.size() > 0) {
//   //   uint32_t send_block_size = getBlockSize(m_center_id);
//   //   m_send_epoch.chunks.clear();
//   //   m_req_send.assign(send_peers.size(), MessageRequest());
//   //   m_send_epoch.slab.resize(send_block_size * send_peers.size());

//   //   m_send_size = 0;
//   //   for (int i = 0; i < send_peers.size(); i++) {
//   //     if (m_validate) {
//   //       std::memcpy(m_send_epoch.slab.data() + m_send_size,
//   //                   m_dst + getBlockOffset(m_center_id),
//   //                   send_block_size * m_gen.sizeofDataType(FLOAT));
//   //     }
//   //     m_send_epoch.chunks.push_back({m_send_epoch.slab.data() +
//   m_send_size,
//   //                                    send_block_size, send_peers[i],
//   //                                    MessageRequest()});
//   //     m_gen.enQ_isend(evQ, m_send_epoch.chunks.back().ptr,
//   send_block_size,
//   //                     FLOAT, send_peers[i], tag, m_comm,
//   &m_req_send[i]);
//   //     m_send_size += send_block_size;
//   //     DPRINTF("[%d] Sending %d elements to %d with tag %d on port %d\n",
//   //             m_gen.rank(), m_send_size, send_peers[i], tag,
//   m_center_id);
//   //   }
//   //   m_data_sent += m_send_size;
//   // }

//   // m_i++;
//   // if (coll_type == TORUS_REDUCE_SCATTER) {
//   //   return true;
//   // }
//   // return false;
// }

/******* CollectiveBase  *******/

EmberTorusCollGenerator::TorusCollectiveRunner::TorusCollectiveRunner(
    EmberTorusCollGenerator &gen, int num_allreduces, uint32_t count,
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

bool EmberTorusCollGenerator::TorusCollectiveRunner::progress_phase(
    std::queue<EmberEvent *> &evQ) {
  if (!m_active_handles.empty() && m_has_new) {
    auto [allreduce_ptr, active_handlers_id] = m_active_allreduce_ptrs[m_idx];
    DPRINTF("[%d] Notifying recv, time %" PRIu64 "\n", m_r, m_notify_time);
    fflush(stdout);
    allreduce_ptr->notifyRecv(active_handlers_id, evQ);
  } else if (!m_active_handles.empty() && !m_has_new) {
    return false;
  }

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
    // m_gen.enQ_waitall(evQ, m_active_handles.size(), &m_active_handles[0],
    // NULL); m_gen.enQ_compute(evQ, [&]() {
    //   for (auto &allreduce : m_active_allreduce_ptrs) {
    //     DPRINTF("[%d] Notifying\n", m_r);
    //     fflush(stdout);
    //     allreduce->notifyRecv();
    //   }
    //   return 0;
    // });
  }
  return all_completed;

  // if (m_active_handles.size() > 0 && m_has_new) {
  //   assert(m_idx >= 0 && m_idx < (int)m_active_handles.size());
  //   auto [allreduce_ptr, active_handlers_id] =
  //   m_active_allreduce_ptrs[m_idx];
  //   allreduce_ptr->notifyRecv(active_handlers_id);
  //   DPRINTF("Notifying recv from %d done, coll_type = %s\n", m_r,
  //           allreduce_ptr->getCollTypeStr().c_str());
  // }

  // // progress allreduces
  // m_has_new = false;
  // m_active_handles.clear();
  // m_active_allreduce_ptrs.clear();
  // bool all_completed = true;
  // for (auto &allreduce : m_allreduces) {
  //   // we don't progress disabled allreduces (they look as completed)
  //   bool this_completed = allreduce.progress(evQ);
  //   all_completed = all_completed && this_completed;

  //   if (allreduce.hasPendingRecv()) {
  //     assert(!this_completed);

  //     auto [active_handlers, active_handlers_ids] =
  //     allreduce.getRecvHandle();
  //     m_active_handles.insert(m_active_handles.end(),
  //     active_handlers.begin(),
  //                             active_handlers.end());
  //     for (int handler_id : active_handlers_ids) {
  //       m_active_allreduce_ptrs.push_back({&allreduce, handler_id});
  //     }
  //   }
  // }

  // if (m_active_handles.size() > 0) {
  //   m_has_new = true;
  //   m_gen.enQ_waitany(evQ, m_active_handles.size(), &m_active_handles[0],
  //                     &m_idx, &m_resp_recv);
  //   // m_gen.enQ_compute(evQ, [&]() {
  //   //   m_has_new = true;
  //   //   return 1;
  //   // });
  //   return false;
  // }
  // return all_completed;
}
// size_t write = 0;
// for (size_t read = 0; read < m_reduce_scatters_idx.size(); ++read) {
//   int scatter_id = m_reduce_scatters_idx[read];

//   bool this_completed = m_allreduces[scatter_id].progress(evQ);
//   all_completed = all_completed && this_completed;
//   bool remove = (m_allreduces[scatter_id].getStats() == ALL_GATHER);
//   if (remove) {
//     m_allgathers_idx[m_allgathers_permutation[scatter_id]].second = true;
//     continue; // не копируем — тем самым “удаляем”
//   }
//   m_reduce_scatters_idx[write++] = scatter_id;
// }
// m_reduce_scatters_idx.resize(write);

// for (auto &[gather_id, enabled] : m_allgathers_idx) {
//   if (enabled) {
//     bool this_completed = m_allreduces[gather_id].progress(evQ);
//     all_completed = all_completed && this_completed;
//   } else {
//     all_completed = false;
//     break;
//   }
// }

// return all_completed;
// if (m_sync) {
//   m_active_recv_handles.clear();
//   m_active_allreduce_ptrs.clear();
//   bool all_completed = true;
//   for (auto &allreduce : m_allreduces) {
//     // we don't progress disabled allreduces (they look as completed)
//     bool this_completed = !allreduce.isEnabled() ||
//     allreduce.progress(evQ); all_completed = all_completed &&
//     this_completed; if (allreduce.hasPendingRecv()) {
//       DPRINTF("[%d] Found one pending recv\n", m_r);
//       assert(!this_completed);
//       // we need to do this because SST doesn't like invalid recv handles
//       in
//       // the waitany :(
//       // printf("[%d] allreduce %p waiting for %p\n", m_gen.rank(),
//       // &allreduce, allreduce.getRecvHandle());
//       auto [active_handlers, active_handlers_ids] =
//       allreduce.getRecvHandle(); int left = m_active_recv_handles.size();
//       int right = left + active_handlers.size();
//       m_active_recv_handles.insert(m_active_recv_handles.end(),
//                                    active_handlers.begin(),
//                                    active_handlers.end());
//       m_active_allreduce_ptrs[left] = {&allreduce, active_handlers_ids};

//     } else {
//       DPRINTF("[%d] Found one non-pending recv\n", m_r);
//     }
//   }
//   if (m_active_recv_handles.size() > 0) {
//     DPRINTF("[%d] Waiting for %d recvs\n", m_r,
//     m_active_recv_handles.size()); m_gen.enQ_waitall(evQ,
//     m_active_recv_handles.size(),
//                       &m_active_recv_handles[0], NULL);
//     m_gen.enQ_compute(evQ, [&]() {
//       for (auto &[_, allreduce_ptr] : m_active_allreduce_ptrs) {
//         DPRINTF("[%d] Notifying\n", m_r);
//         allreduce_ptr.first->notifyRecv();
//       }
//       return 0;
//     });
//   }
//   return all_completed;
// } else {
//   // notify allreduces that recevied a message
//   if (m_active_recv_handles.size() > 0 && m_has_new_recv) {
//     auto &[left, allreduce_ptr_and_ids] =
//         *(--m_active_allreduce_ptrs.upper_bound(m_recv_idx));
//     auto &[allreduce_ptr, active_handlers_ids] = allreduce_ptr_and_ids;
//     allreduce_ptr->notifyRecv(active_handlers_ids[m_recv_idx - left]);
//   }

//   // progress allreduces
//   m_handle_idx = 0;
//   m_has_new_recv = false;
//   bool all_completed = true;
//   for (auto &allreduce : m_allreduces) {
//     // we don't progress disabled allreduces (they look as completed)
//     bool this_completed = false;
//     if (allreduce.isEnabled()) {
//       this_completed = allreduce.progress(evQ);
//     } else {
//       this_completed = true;
//     }
//     all_completed = all_completed && this_completed;

//     if (allreduce.hasPendingRecv()) {
//       assert(!this_completed);
//       // we need to do this because SST doesn't like invalid recv handles
//       in
//       // the waitany :(
//       // printf("[%d] allreduce %p waiting for %p\n", m_gen.rank(),
//       // &allreduce, allreduce.getRecvHandle());
//       auto [active_handlers, active_handlers_ids] =
//       allreduce.getRecvHandle(); int left = m_active_recv_handles.size();
//       int right = left + active_handlers.size();
//       m_active_recv_handles.insert(m_active_recv_handles.end(),
//                                    active_handlers.begin(),
//                                    active_handlers.end());
//       m_active_allreduce_ptrs[left] =
//           std::make_pair(&allreduce, active_handlers_ids);
//     }
//   }

//   if (m_active_recv_handles.size() > 0) {
//     if (m_nb) {
//       // printf("[%d] testany (m_handle_idx: %d; t: %" PRIu64 ")\n",
//       // m_gen.rank(), m_handle_idx, m_gen.getCurrentSimTimeNano());

//       // this is needed to advance simtime. Testany doesn't advance it by
//       // itself :(
//       m_gen.enQ_compute(evQ, 10);
//       m_gen.enQ_testany(evQ, m_active_recv_handles.size(),
//                         &m_active_recv_handles[0], &m_recv_idx,
//                         &m_has_new_recv, &m_resp_recv);
//     } else {
//       m_has_new_recv = true;
//       m_gen.enQ_waitany(evQ, m_active_recv_handles.size(),
//                         &m_active_recv_handles[0], &m_recv_idx,
//                         &m_resp_recv);
//     }

//     return false;
//   }

//   return all_completed;
// }

void EmberTorusCollGenerator::TorusCollectiveRunner::printStats() {
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

/******* TorusCollective  *******/
EmberTorusCollGenerator::TorusCollective::TorusCollective(
    EmberTorusCollGenerator &gen, uint dimensions_num, uint ports,
    uint32_t count, uint32_t rank, uint32_t comm_size, Communicator comm,
    double aggregation_cost_ns, bool nb, bool sync, CollType coll_type,
    float *data, uint *dimensions, bool latency_optimal)
    : TorusCollectiveRunner(gen, 4, count, rank, comm_size, nb, sync),
      m_state(INIT), m_coll_type(coll_type), m_dimensions(dimensions) {
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
  stages_waiting_centers.resize(2 * (n - 1));
  auto m_point_coord = getCoordFromRank(m_r, n);
  for (auto port_id : {getRankFromCoord(left(m_point_coord, n), n),
                       getRankFromCoord(right(m_point_coord, n), n),
                       getRankFromCoord(up(m_point_coord, n), n),
                       getRankFromCoord(down(m_point_coord, n), n)}) {
    m_allreduces.push_back(TorusCollectiveEngine(
        m_gen, coll_type, dimensions, dimensions_num, data, m_count, m_r, m_p,
        aggregation_cost_ns, comm, data != NULL, latency_optimal, port_id,
        this));
  }
}
void EmberTorusCollGenerator::TorusCollective::addWaitingCenters(
    int stage, std::vector<int> &centers, CollType coll_type) {
  int n = m_dimensions[0];
  // assert(stage > 0);
  int global_stage = stage + (coll_type == TORUS_REDUCE_SCATTER ? 0 : n - 1);
  for (int center_id : centers) {
    stages_waiting_centers[global_stage][center_id]++;
    // if (stages_waiting_centers[global_stage].find(center_id) ==
    //     stages_waiting_centers[global_stage].end()) {
    //   stages_waiting_centers[global_stage][center_id] = true;
    // }
  }
  // stages_waiting_centers[stage].insert(centers.begin(), centers.end());
}
bool EmberTorusCollGenerator::TorusCollective::progress(
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

void EmberTorusCollGenerator::TorusCollective::reset() {
  m_state = INIT;
  for (auto &allreduce : m_allreduces)
    allreduce.reset();
}

EmberTorusCollGenerator::TorusCollective::~TorusCollective() { ; }

/******* EmberTorusCollGenerator (parent Ember motif) *******/
EmberTorusCollGenerator::EmberTorusCollGenerator(SST::ComponentId_t id,
                                                 Params &params)
    : EmberMessagePassingGenerator(id, params, "None"), m_tag(0) {
  assert(0);
}

EmberTorusCollGenerator::EmberTorusCollGenerator(SST::ComponentId_t id,
                                                 Params &params,
                                                 std::string name)
    : EmberMessagePassingGenerator(id, params, name), m_tag(0) {
  ;
}

bool EmberTorusCollGenerator::generate(std::queue<EmberEvent *> &evQ) {
  assert(0);
}

/******* AllToAllImproved *******/
// int EmberTorusCollGenerator::EmberImprovedAlltoall::mod(int a, int b) {
//   int tmp = ((a % b) + b) % b;
//   return tmp;
// }

// EmberTorusCollGenerator::EmberImprovedAlltoall::~EmberImprovedAlltoall()
// {
//   free(msgRequests);
// }

// void EmberTorusCollGenerator::EmberImprovedAlltoall::reset() {
//   m_loopIndex = 0;
//   nextMRIndex = 0;
//   m_tag = m_gen.getNextTag();
// }

// EmberTorusCollGenerator::EmberImprovedAlltoall::EmberImprovedAlltoall(
//     EmberTorusCollGenerator &gen, uint32_t sendcount, uint32_t rank,
//     uint32_t comm_size, Communicator comm)
//     : m_gen(gen), m_r(rank), m_p(comm_size), m_sendcount(sendcount),
//       m_comm(comm), m_tag(0) {
//   m_sendBuf = NULL;
//   m_recvBuf = NULL;

//   m_tag = m_gen.getNextTag();

//   nextMRIndex = 0;
//   msgRequests =
//       (MessageRequest *)malloc(sizeof(MessageRequest) * (comm_size) * 2);
// }

// bool EmberTorusCollGenerator::EmberImprovedAlltoall::progress(
//     std::queue<EmberEvent *> &evQ) {
//   if (m_loopIndex == m_iterations) {
//     m_gen.enQ_waitall(evQ, nextMRIndex, msgRequests, NULL);
//     return true;
//   }

//   const bool participate = true;
//   if (participate) {

//     for (int i = 1; i <= m_p; i++) {
//       if (mod(m_r - i, m_p) != m_r) {
//         m_gen.enQ_irecv(evQ, NULL, m_sendcount, FLOAT, mod(m_r - i, m_p),
//         m_tag,
//                         m_comm, &msgRequests[nextMRIndex++]);
//       }
//     }

//     for (int i = 1; i <= m_p; i++) {
//       if (mod(m_r + i, m_p) != m_r) {
//         m_gen.enQ_isend(evQ, NULL, m_sendcount, FLOAT, mod(m_r + i, m_p),
//         m_tag,
//                         m_comm, &msgRequests[nextMRIndex++]);
//       }
//     }
//     m_loopIndex++;
//   }
//   return false;
// }