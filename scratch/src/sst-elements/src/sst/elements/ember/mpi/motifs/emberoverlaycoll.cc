#include "emberoverlaycoll.h"
#include "embershortmsgcheck.h"
#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sst_config.h>
#include <stdexcept>
// #define DEBUG
using namespace SST::Ember;

#ifdef DEBUG
#define DPRINTF(...) printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

/******* schedule IO *******/

OverlayMode EmberOverlayCollGenerator::parseMode(const std::string &mode) {
  if (mode == "rs_ag" || mode == "RS_AG") {
    return OverlayMode::RS_AG;
  }
  if (mode == "reduce_bcast" || mode == "REDUCE_BCAST" || mode.empty()) {
    return OverlayMode::REDUCE_BCAST;
  }
  fprintf(stderr, "Unknown overlay mode '%s'\n", mode.c_str());
  exit(1);
  return OverlayMode::REDUCE_BCAST;
}

OverlaySchedule
EmberOverlayCollGenerator::loadSchedule(const std::string &path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("Cannot open trees_file: " + path);
  }
  nlohmann::json j;
  file >> j;

  OverlaySchedule sched;
  sched.version = j.value("version", 1);
  sched.n = j.at("n").get<int>();
  sched.mode = parseMode(j.value("mode", "reduce_bcast"));
  sched.chunking = j.value("chunking", "equal_by_tree");

  std::vector<double> top_shares;
  if (j.contains("shares")) {
    top_shares = j.at("shares").get<std::vector<double>>();
  }

  for (const auto &tj : j.at("trees")) {
    OverlayTree tree;
    tree.id = tj.value("id", static_cast<int>(sched.trees.size()));
    tree.root = tj.at("root").get<int>();
    if (tj.contains("share")) {
      tree.share = tj.at("share").get<double>();
    }
    for (const auto &ej : tj.at("edges")) {
      OverlayEdge e;
      e.from = ej.at("from").get<int>();
      e.to = ej.at("to").get<int>();
      e.rsStage = ej.value("rsStage", ej.value("stage_up", 0));
      e.agStage = ej.value("agStage", ej.value("stage_down", 0));
      e.route_class = ej.value("route_class", ej.value("route_id", -1));
      tree.edges.push_back(e);
    }
    tree.buildAdjacency(sched.n);
    sched.trees.push_back(std::move(tree));
  }

  const int K = (int)sched.trees.size();
  if (!top_shares.empty() && (int)top_shares.size() != K) {
    fprintf(stderr,
            "Overlay schedule: top-level shares length %zu != trees %d\n",
            top_shares.size(), K);
    exit(1);
  }

  bool any_share = false;
  bool all_share = true;
  for (int t = 0; t < K; ++t) {
    if (std::isnan(sched.trees[t].share)) {
      all_share = false;
      if (!top_shares.empty()) {
        sched.trees[t].share = top_shares[(size_t)t];
        any_share = true;
      }
    } else {
      any_share = true;
    }
  }

  if (!any_share) {
    // No per-tree or top-level shares: equal_by_tree.
    const double eq = 1.0 / (double)K;
    double assigned = 0.0;
    for (int t = 0; t < K; ++t) {
      if (t < K - 1) {
        sched.trees[(size_t)t].share = eq;
        assigned += eq;
      } else {
        sched.trees[(size_t)t].share = 1.0 - assigned;
      }
    }
    if (sched.chunking.empty()) {
      sched.chunking = "equal_by_tree";
    }
  } else {
    for (int t = 0; t < K; ++t) {
      if (std::isnan(sched.trees[t].share)) {
        fprintf(stderr,
                "Overlay schedule: tree %d missing share while others set "
                "(or provide top-level shares)\n",
                sched.trees[t].id);
        exit(1);
      }
    }
    if (sched.chunking == "equal_by_tree" && !all_share) {
      // Mixed / top-level shares: mark as weighted for logs.
      sched.chunking = "weighted";
    } else if (sched.chunking == "equal_by_tree") {
      // Explicit equal shares still fine; keep label unless unequal.
      bool equal = true;
      const double eq = 1.0 / (double)K;
      for (const auto &tree : sched.trees) {
        if (std::fabs(tree.share - eq) > 1e-12) {
          equal = false;
          break;
        }
      }
      if (!equal) {
        sched.chunking = "weighted";
      }
    }
  }

  return sched;
}

void EmberOverlayCollGenerator::validateSchedule(const OverlaySchedule &sched,
                                                 int world_size) {
  if (sched.n != world_size) {
    fprintf(stderr, "Overlay schedule n=%d does not match MPI world size=%d\n",
            sched.n, world_size);
    exit(1);
  }
  if (sched.trees.empty()) {
    fprintf(stderr, "Overlay schedule has no trees\n");
    exit(1);
  }
  int stages_num = 0;
  double share_sum = 0.0;
  for (const auto &tree : sched.trees) {
    if (tree.root < 0 || tree.root >= sched.n) {
      fprintf(stderr, "Tree %d has invalid root %d\n", tree.id, tree.root);
      exit(1);
    }
    if (tree.edges.empty()) {
      fprintf(stderr, "Tree %d has no edges\n", tree.id);
      exit(1);
    }
    if (std::isnan(tree.share) || !(tree.share > 0.0)) {
      fprintf(stderr, "Tree %d has invalid share %g (must be > 0)\n", tree.id,
              tree.share);
      exit(1);
    }
    share_sum += tree.share;
    if ((int)tree.outgoing.size() != sched.n ||
        (int)tree.incoming.size() != sched.n) {
      fprintf(stderr, "Tree %d adjacency not built (call buildAdjacency(n))\n",
              tree.id);
      exit(1);
    }
    for (const auto &e : tree.edges) {
      if (e.from < 0 || e.from >= sched.n || e.to < 0 || e.to >= sched.n ||
          e.from == e.to || e.rsStage < 0 || e.agStage < 0) {
        fprintf(stderr, "Tree %d: invalid edge %d->%d\n", tree.id, e.from,
                e.to);
        exit(1);
      }
      stages_num = std::max(stages_num, e.rsStage + 1);
      stages_num = std::max(stages_num, e.agStage + 1);
    }
  }
  if (std::fabs(share_sum - 1.0) > 1e-9) {
    fprintf(stderr, "Overlay schedule share sum=%g (expected 1.0)\n",
            share_sum);
    exit(1);
  }
  if (stages_num <= 0) {
    fprintf(stderr, "Overlay schedule has no stages (stages_num=%d)\n",
            stages_num);
    exit(1);
  }
}

/******* OverlayCollectiveEngine *******/

EmberOverlayCollGenerator::OverlayCollectiveEngine::OverlayCollectiveEngine(
    EmberOverlayCollGenerator &gen, CollType coll_type, float *dst,
    uint32_t count, uint32_t rank, uint32_t numproc, double aggregation_cost_ns,
    Communicator comm, bool validate, int port_id, OverlayCollective *runner)
    : m_gen(gen), m_count(count), m_dst(dst), m_r(rank), m_p(numproc),
      m_aggregation_cost_ns(aggregation_cost_ns), m_data_sent(0), m_comm(comm),
      m_validate(validate), m_enabled(true), m_port_id(port_id),
      m_runner(runner), m_data_reduced(0), m_recv_size(0), m_send_size(0) {

  // Stages from the explicit tree set (no torus shifts).
  m_stages_num = 0;
  int max_rc = -1;
  for (const auto &tree : m_runner->allreduce_trees) {
    for (const auto &e : tree.edges) {
      m_stages_num = std::max(m_stages_num, e.rsStage + 1);
      m_stages_num = std::max(m_stages_num, e.agStage + 1);
      if (e.route_class > max_rc) {
        max_rc = e.route_class;
      }
    }
  }
  assert(m_stages_num > 0);
  // Slots: {-1} ∪ {0..max_rc} → size max_rc+2 (at least 1 for all -1).
  m_num_rc_slots = (uint32_t)(max_rc + 2);
  assert(m_num_rc_slots >= 1);

  m_do_reduce_scatter = (coll_type != OVERLAY_ALLGATHER);
  // reduce_bcast: RS+AG with full chunk (latency path); rs_ag later.
  m_do_allgather =
      (coll_type == OVERLAY_ALLGATHER) || (coll_type == OVERLAY_ALLREDUCE);

  buildTreeLayout();

  scatter_peers_send.resize(m_stages_num);
  scatter_peers_recv.resize(m_stages_num);
  allgather_peers_send.resize(m_stages_num);
  allgather_peers_recv.resize(m_stages_num);

  // Fill peer maps from this rank's incident edges only.
  // tree_id is the waiting / chunk key (replaces torus center_block_id).
  for (int tree_id = 0; tree_id < (int)m_runner->allreduce_trees.size();
       ++tree_id) {
    const auto &tree = m_runner->allreduce_trees[tree_id];
    for (const auto &e : tree.edgesFrom((int)m_r)) {
      // UP: I send toward root; DOWN: I recv from parent.
      const auto key_to = std::make_pair(e.to, e.route_class);
      scatter_peers_send[e.rsStage][key_to].push_back(tree_id);
      allgather_peers_recv[e.agStage][key_to].push_back(tree_id);
    }
    for (const auto &e : tree.edgesTo((int)m_r)) {
      // UP: I recv from child; DOWN: I send to child.
      const auto key_from = std::make_pair(e.from, e.route_class);
      scatter_peers_recv[e.rsStage][key_from].push_back(tree_id);
      allgather_peers_send[e.agStage][key_from].push_back(tree_id);
    }
  }

  auto sort_tree_ids = [](auto &stage_map) {
    for (auto &ports : stage_map) {
      for (auto &[key, ids] : ports) {
        (void)key;
        std::sort(ids.begin(), ids.end());
      }
    }
  };
  sort_tree_ids(scatter_peers_send);
  sort_tree_ids(scatter_peers_recv);
  sort_tree_ids(allgather_peers_send);
  sort_tree_ids(allgather_peers_recv);

  reset();
  if (m_r == 0) {
    int q = 1;
  }
}

void EmberOverlayCollGenerator::OverlayCollectiveEngine::setEnable(
    bool enable) {
  m_enabled = enable;
}

bool EmberOverlayCollGenerator::OverlayCollectiveEngine::isEnabled() {
  return m_enabled;
}

uint64_t EmberOverlayCollGenerator::OverlayCollectiveEngine::getMovedBytes() {
  return m_data_sent;
}

void EmberOverlayCollGenerator::OverlayCollectiveEngine::setBuff(
    float *new_dest) {
  m_dst = new_dest;
}

float *EmberOverlayCollGenerator::OverlayCollectiveEngine::getBuff() {
  return m_dst;
}

uint32_t EmberOverlayCollGenerator::OverlayCollectiveEngine::getTreeSize(
    int tree_id) const {
  assert(tree_id >= 0 && tree_id < (int)m_tree_size.size());
  return m_tree_size[(size_t)tree_id];
}

uint32_t EmberOverlayCollGenerator::OverlayCollectiveEngine::getTreeOffset(
    int tree_id) const {
  assert(tree_id >= 0 && tree_id < (int)m_tree_offset.size());
  return m_tree_offset[(size_t)tree_id];
}

void EmberOverlayCollGenerator::OverlayCollectiveEngine::buildTreeLayout() {
  const int K = (int)m_runner->allreduce_trees.size();
  assert(K > 0);
  m_tree_size.assign((size_t)K, 0);
  m_tree_offset.assign((size_t)K, 0);

  // Prefix-round layout: off[t] = round(M * sum_{i<t} w_i), off[K] = M.
  // Guarantees contiguous non-overlapping shares summing to m_count.
  std::vector<uint32_t> off((size_t)K + 1, 0);
  double prefix = 0.0;
  off[0] = 0;
  for (int t = 0; t < K; ++t) {
    const double w = m_runner->allreduce_trees[(size_t)t].share;
    assert(w > 0.0 && !std::isnan(w));
    prefix += w;
    if (t + 1 < K) {
      const double rounded = std::round((double)m_count * prefix);
      off[(size_t)t + 1] =
          (uint32_t)std::max(0.0, std::min((double)m_count, rounded));
    }
  }
  off[(size_t)K] = m_count;

  // Enforce non-decreasing offsets (rounding can theoretically stall).
  for (int t = 1; t <= K; ++t) {
    if (off[(size_t)t] < off[(size_t)t - 1]) {
      off[(size_t)t] = off[(size_t)t - 1];
    }
  }
  off[(size_t)K] = m_count;

  uint32_t sum = 0;
  for (int t = 0; t < K; ++t) {
    m_tree_offset[(size_t)t] = off[(size_t)t];
    m_tree_size[(size_t)t] = off[(size_t)t + 1] - off[(size_t)t];
    sum += m_tree_size[(size_t)t];
  }
  assert(sum == m_count);

  if (m_r == 0) {
    double wsum = 0.0;
    printf("[Overlay] tree layout count=%u K=%d sizes=[", m_count, K);
    for (int t = 0; t < K; ++t) {
      wsum += m_runner->allreduce_trees[(size_t)t].share;
      printf("%u", m_tree_size[(size_t)t]);
      if (t + 1 < K)
        printf(",");
    }
    printf("] shares=[");
    for (int t = 0; t < K; ++t) {
      printf("%g", m_runner->allreduce_trees[(size_t)t].share);
      if (t + 1 < K)
        printf(",");
    }
    printf("] share_sum=%g\n", wsum);
  }
}

uint32_t EmberOverlayCollGenerator::OverlayCollectiveEngine::messageTag(
    CollType coll_type, int route_class, int from_rank, int to_rank) const {
  // Layout: [phase | route_class_slot | ordered_pair(from,to)]
  //
  // Ordered (from,to): A→B and B→A get different tags. Needed when two trees
  // exchange opposite directions on the same host pair in one stage
  // (DBTree: 37→38 tree0 and 38→37 tree1). Sender and receiver of one message
  // both pass the same (from,to) in the edge direction.
  assert(from_rank >= 0 && to_rank >= 0);
  assert((uint32_t)from_rank < m_p && (uint32_t)to_rank < m_p);
  assert(from_rank != to_rank);

  const uint32_t pair = (uint32_t)from_rank * m_p + (uint32_t)to_rank;

  const uint32_t rc_slot = route_class < 0 ? 0u : (uint32_t)route_class + 1u;
  assert(rc_slot < m_num_rc_slots);

  const uint32_t phase = (coll_type == OVERLAY_REDUCE_SCATTER) ? 0u : 1u;
  const uint32_t pair_span = m_p * m_p;
  return (phase * m_num_rc_slots + rc_slot) * pair_span + pair;
}

void EmberOverlayCollGenerator::OverlayCollectiveEngine::reset() {
  m_ready_to_recv = false;
  m_ready_to_send = false;
  m_i = 0;
  m_state = REDUCE_SCATTER;
  m_req_recv.resize(2 * m_stages_num);
  m_req_send.resize(2 * m_stages_num);
  m_recv_epoch.resize(2 * m_stages_num);
  m_waiting_send.clear();
}

bool EmberOverlayCollGenerator::OverlayCollectiveEngine::progress(
    std::queue<EmberEvent *> &evQ) {
  switch (m_state) {
  case REDUCE_SCATTER:
    if (!m_do_reduce_scatter || collective(evQ, OVERLAY_REDUCE_SCATTER)) {
      m_state = ALL_GATHER;
    } else {
      return false;
    }
    // fallthrough
  case ALL_GATHER:
    if (!m_do_allgather || collective(evQ, OVERLAY_ALLGATHER)) {
      m_state = FINI;
    } else {
      return false;
    }
    // fallthrough
  case FINI:
    return true;
  default:
    assert(0);
  }
  assert(0);
}

bool EmberOverlayCollGenerator::OverlayCollectiveEngine::hasPendingRecv() {
  if (m_i == 0) {
    return false;
  }
  int global_stage = m_i + (m_state == REDUCE_SCATTER ? 0 : m_stages_num);
  for (int stage = 0; stage < global_stage; stage++) {
    for (auto &req : m_req_recv[stage]) {
      if (req != 0) {
        return true;
      }
    }
  }
  return false;
}

std::pair<std::vector<MessageRequest>, std::vector<std::pair<int, int>>>
EmberOverlayCollGenerator::OverlayCollectiveEngine::getRecvHandle() {
  std::vector<MessageRequest> active_handlers;
  std::vector<std::pair<int, int>> active_ids;
  int global_stage = m_i + (m_state == REDUCE_SCATTER ? 0 : m_stages_num);
  for (int stage = 0; stage < global_stage; stage++) {
    for (int i = 0; i < (int)m_req_recv[stage].size(); i++) {
      if (m_req_recv[stage][i] != 0) {
        active_handlers.push_back(m_req_recv[stage][i]);
        active_ids.push_back(std::make_pair(stage, i));
      }
    }
  }
  return std::make_pair(active_handlers, active_ids);
}

// Same idea as TreesCollectiveEngine::waiting_receive:
// do not enter stage m_i until every tree_id expected from previous-stage
// recvs has been delivered (stages_waiting_centers count == 0).
bool EmberOverlayCollGenerator::OverlayCollectiveEngine::waiting_receive(
    CollType coll_type) {
  auto &recv_peers = (coll_type == OVERLAY_REDUCE_SCATTER)
                         ? scatter_peers_recv
                         : allgather_peers_recv;
  assert(m_i > 0);
  int global_stage =
      m_i - 1 + (coll_type == OVERLAY_REDUCE_SCATTER ? 0 : m_stages_num);
  for (auto &[prev_peer, tree_ids] : recv_peers[m_i - 1]) {
    (void)prev_peer;
    for (int tree_id : tree_ids) {
      auto &stage_map = m_runner->stages_waiting_centers[global_stage];
      auto it = stage_map.find(tree_id);
      if (it == stage_map.end()) {
        return true; // not registered yet / still outstanding
      }
      if (it->second > 0) {
        return true;
      }
    }
  }
  return false;
}

bool EmberOverlayCollGenerator::OverlayCollectiveEngine::waiting_send() {
  for (bool w : m_waiting_send) {
    if (w) {
      return true;
    }
  }
  return false;
}

void EmberOverlayCollGenerator::OverlayCollectiveEngine::notifyRecv(
    std::pair<int, int> chunk, std::queue<EmberEvent *> &evQ) {
  assert(m_i > 0);
  auto [chunk_stage, chunk_id] = chunk;
  CollType coll_type =
      chunk_stage < m_stages_num ? OVERLAY_REDUCE_SCATTER : OVERLAY_ALLGATHER;
  auto &recv_peers = coll_type == OVERLAY_REDUCE_SCATTER ? scatter_peers_recv
                                                         : allgather_peers_recv;
  int stage =
      (chunk_stage < m_stages_num ? chunk_stage : chunk_stage - m_stages_num);

  int prev_peer_counter = 0;
  for (auto &[prev_peer, tree_ids] : recv_peers[stage]) {
    (void)prev_peer;
    if (chunk_id != prev_peer_counter) {
      prev_peer_counter++;
      continue;
    }
    for (int tree_id : tree_ids) {
      m_runner->stages_waiting_centers[chunk_stage][tree_id]--;
    }
    break;
  }

  m_req_recv[chunk_stage][chunk_id] = 0;
  processReceivedData(evQ, coll_type, chunk_stage, chunk_id);
}

void EmberOverlayCollGenerator::OverlayCollectiveEngine::processReceivedData(
    std::queue<EmberEvent *> &evQ, CollType coll_type, int chunk_stage,
    int chunk_id) {
  auto &recv_peers = coll_type == OVERLAY_REDUCE_SCATTER ? scatter_peers_recv
                                                         : allgather_peers_recv;
  int small_stage =
      chunk_stage < m_stages_num ? chunk_stage : chunk_stage - m_stages_num;
  if (m_validate) {
    int peer_idx = 0;
    for (auto &[peer, tree_ids] : recv_peers[small_stage]) {
      (void)peer;
      if (chunk_id != peer_idx) {
        peer_idx++;
        continue;
      }

      assert(chunk_id < (int)m_recv_epoch[chunk_stage].chunks.size());
      auto chunk = m_recv_epoch[chunk_stage].chunks[chunk_id];

      if (coll_type == OVERLAY_REDUCE_SCATTER) {
        m_data_reduced += (uint64_t)chunk.size * m_gen.sizeofDataType(FLOAT);
      }

      if (m_validate) {
        // Same layout as send pack: tree shares concatenated in tree_ids order.
        float *src = chunk.ptr;
        for (int tid : tree_ids) {
          const uint32_t sz = getTreeSize(tid);
          const uint32_t off = getTreeOffset(tid);
          if (coll_type == OVERLAY_REDUCE_SCATTER) {
            for (uint32_t j = 0; j < sz; j++) {
              m_dst[off + j] += src[j];
            }
          } else {
            std::memcpy(m_dst + off, src,
                        (size_t)sz * m_gen.sizeofDataType(FLOAT));
          }
          src += sz;
        }
      } else if (m_aggregation_cost_ns != 0 &&
                 coll_type == OVERLAY_REDUCE_SCATTER) {
        m_gen.enQ_compute(evQ, m_aggregation_cost_ns * chunk.size);
      }
      break;
    }
  } else {
    if (m_aggregation_cost_ns != 0 && coll_type == OVERLAY_REDUCE_SCATTER) {
      m_gen.enQ_compute(evQ, m_aggregation_cost_ns * m_recv_size);
      return;
    }
  }
}

// Scaffold of TreesCollectiveEngine::collective control flow.
// Peer maps + waiting_receive are live; isend/irecv body comes next.
bool EmberOverlayCollGenerator::OverlayCollectiveEngine::collective(
    std::queue<EmberEvent *> &evQ, CollType coll_type) {
  (void)evQ;
  std::string coll_type_str =
      coll_type == OVERLAY_REDUCE_SCATTER ? "reducescatter" : "allgather";
  auto &send_peers = coll_type == OVERLAY_REDUCE_SCATTER ? scatter_peers_send
                                                         : allgather_peers_send;
  auto &recv_peers = coll_type == OVERLAY_REDUCE_SCATTER ? scatter_peers_recv
                                                         : allgather_peers_recv;
  (void)send_peers;
  (void)recv_peers;

  int global_stage =
      m_i + (coll_type == OVERLAY_REDUCE_SCATTER ? 0 : m_stages_num);

  if (m_i > 0 && waiting_receive(coll_type)) {
    m_ready_to_recv = true;
    return false;
  }
  uint64_t current_time_ns = m_gen.getCurrentSimTimeNano();
  uint64_t dt_ns = (m_prev_time != 0 && current_time_ns >= m_prev_time)
                       ? (current_time_ns - m_prev_time)
                       : 0;
  m_prev_time = current_time_ns;
  const int L = m_stages_num;
  const int phaseIdx = (coll_type == OVERLAY_REDUCE_SCATTER) ? 0 : 1;
  ;
  const int stepInPhase = std::min((int)m_i, L);
  const int curStage = phaseIdx * L + stepInPhase;
  const int totalStages = 2 * L;

  if (m_r == 0) {
    std::cerr << "[r=0] t=" << current_time_ns << " ns"
              << " dt=" << dt_ns << " ns"
              << " stage=" << curStage << "/" << totalStages << " ("
              << (phaseIdx == 0 ? "RS" : "AG") << " step " << stepInPhase << "/"
              << L << ")" << std::endl;
  }
  // TODO: post irecv for recv_peers[m_i], addWaitingCenters(tree_ids),
  //       pack/isend for send_peers[m_i] with route_class on each edge.
  // Until messaging is wired, just walk stages so the control path is testable.
  (void)global_stage;
  if (m_i == 0) {
    // recv//
    int total = 0;
    int total_prev_peers = 0;
    for (auto &[prev_peer, prev_tree_ids] : recv_peers[m_i]) {
      (void)prev_peer;
      total_prev_peers++;
      m_runner->addWaitingCenters(m_i, recv_peers[m_i][prev_peer], coll_type);
      for (int prev_tree_id : prev_tree_ids) {
        total += (int)getTreeSize(prev_tree_id);
      }
    }
    m_recv_epoch[global_stage].chunks.clear();
    m_recv_epoch[global_stage].slab.resize(total);
    m_req_recv[global_stage].assign(total_prev_peers, MessageRequest());
    if (total != 0) {
      int prev_peer_counter = 0;
      m_recv_size = 0;
      for (auto &[prev_peer, prev_tree_ids] : recv_peers[m_i]) {
        int prev_peer_id = prev_peer.first;
        int route_class = prev_peer.second;
        const uint32_t msg_tag =
            messageTag(coll_type, route_class, prev_peer_id, (int)m_r);
        int count = 0;
        for (int tid : prev_tree_ids) {
          count += (int)getTreeSize(tid);
        }
        m_recv_epoch[global_stage].chunks.push_back(
            {m_recv_epoch[global_stage].slab.data() + m_recv_size, count,
             prev_peer_id});
        m_gen.enQ_irecv(evQ, m_recv_epoch[global_stage].chunks.back().ptr,
                        count, FLOAT, prev_peer_id, msg_tag, m_comm,
                        &m_req_recv[global_stage][prev_peer_counter++]);
        m_recv_size += count;

        std::string peers_str = "[";
        for (size_t k = 0; k < prev_tree_ids.size(); ++k) {
          peers_str += std::to_string(prev_tree_ids[k]);
          if (k + 1 < prev_tree_ids.size())
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
    total = 0;
    int total_next_peers = 0;
    for (auto &[next_peer, next_tree_ids] : send_peers[m_i]) {
      int next_peer_id = next_peer.first;
      int route_class = next_peer.second;
      total_next_peers++;
      for (int next_tree_id : next_tree_ids) {
        total += (int)getTreeSize(next_tree_id);
      }
    }
    m_send_epoch.chunks.clear();
    m_send_epoch.slab.resize(total);
    m_req_send[global_stage].assign(total_next_peers, MessageRequest());
    if (total != 0) {
      m_send_size = 0;
      int next_peer_counter = 0;
      for (auto &[next_peer, next_tree_ids] : send_peers[m_i]) {
        int next_peer_id = next_peer.first;
        int route_class = next_peer.second;
        int count = 0;
        for (int tid : next_tree_ids) {
          count += (int)getTreeSize(tid);
        }
        float *out = m_send_epoch.slab.data() + m_send_size;
        if (m_validate) {
          float *cur = out;
          for (int tid : next_tree_ids) {
            std::memcpy(cur, m_dst + getTreeOffset(tid),
                        getTreeSize(tid) * m_gen.sizeofDataType(FLOAT));
            cur += getTreeSize(tid);
          }
        }

        const uint32_t msg_tag =
            messageTag(coll_type, route_class, (int)m_r, next_peer_id);
        m_send_epoch.chunks.emplace_back(out, count, next_peer_id);
        emberAssertMsgFitsShort(
            (uint64_t)count * m_gen.sizeofDataType(FLOAT), m_gen.valueShort(),
            "OverlayAllreduce isend");
        m_gen.enQ_isend(evQ, out, count, FLOAT, next_peer_id, msg_tag, m_comm,
                        &m_req_send[global_stage][next_peer_counter++],
                        route_class);
        m_send_size += count;
        std::string peers_str = "[";
        for (size_t k = 0; k < next_tree_ids.size(); ++k) {
          peers_str += std::to_string(next_tree_ids[k]);
          if (k + 1 < next_tree_ids.size())
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
    if (m_i < m_stages_num) {
      // recv//
      int total = 0;
      int total_prev_peers = 0;
      for (auto &[prev_peer, prev_tree_ids] : recv_peers[m_i]) {
        (void)prev_peer;
        total_prev_peers++;
        m_runner->addWaitingCenters(m_i, recv_peers[m_i][prev_peer], coll_type);
        for (int prev_tree_id : prev_tree_ids) {
          total += (int)getTreeSize(prev_tree_id);
        }
      }
      m_recv_epoch[global_stage].chunks.clear();
      m_recv_epoch[global_stage].slab.resize(total);
      m_req_recv[global_stage].assign(total_prev_peers, MessageRequest());
      if (total != 0) {
        int prev_peer_counter = 0;
        m_recv_size = 0;
        for (auto &[prev_peer, prev_tree_ids] : recv_peers[m_i]) {
          int prev_peer_id = prev_peer.first;
          int route_class = prev_peer.second;
          const uint32_t msg_tag =
              messageTag(coll_type, route_class, prev_peer_id, (int)m_r);
          int count = 0;
          for (int tid : prev_tree_ids) {
            count += (int)getTreeSize(tid);
          }
          m_recv_epoch[global_stage].chunks.push_back(
              {m_recv_epoch[global_stage].slab.data() + m_recv_size, count,
               prev_peer_id});
          m_gen.enQ_irecv(evQ, m_recv_epoch[global_stage].chunks.back().ptr,
                          count, FLOAT, prev_peer_id, msg_tag, m_comm,
                          &m_req_recv[global_stage][prev_peer_counter++]);
          m_recv_size += count;

          std::string peers_str = "[";
          for (size_t k = 0; k < prev_tree_ids.size(); ++k) {
            peers_str += std::to_string(prev_tree_ids[k]);
            if (k + 1 < prev_tree_ids.size())
              peers_str += " ";
          }
          peers_str += "]";

          DPRINTF(
              "[%d] Receiving %d elements in %d global_stage %s from %d with "
              "tag %u route_class "
              "%d colltype = %s\n",
              m_r, count, global_stage, peers_str.c_str(), prev_peer_id,
              msg_tag, route_class, coll_type_str.c_str());
          fflush(stdout);
        }
      }

      // send//
      total = 0;
      int total_next_peers = 0;
      for (auto &[next_peer, next_tree_ids] : send_peers[m_i]) {
        total_next_peers++;
        for (int next_tree_id : next_tree_ids) {
          total += (int)getTreeSize(next_tree_id);
        }
      }
      m_send_epoch.chunks.clear();
      m_send_epoch.slab.resize(total);
      m_req_send[global_stage].assign(total_next_peers, MessageRequest());
      if (total != 0) {
        m_send_size = 0;
        int next_peer_counter = 0;
        for (auto &[next_peer, next_tree_ids] : send_peers[m_i]) {
          int next_peer_id = next_peer.first;
          int route_class = next_peer.second;
          int count = 0;
          for (int tid : next_tree_ids) {
            count += (int)getTreeSize(tid);
          }
          float *out = m_send_epoch.slab.data() + m_send_size;
          if (m_validate) {
            float *cur = out;
            for (int tid : next_tree_ids) {
              std::memcpy(cur, m_dst + getTreeOffset(tid),
                          getTreeSize(tid) * m_gen.sizeofDataType(FLOAT));
              cur += getTreeSize(tid);
            }
          }

          const uint32_t msg_tag =
              messageTag(coll_type, route_class, (int)m_r, next_peer_id);
          m_send_epoch.chunks.emplace_back(out, count, next_peer_id);
          emberAssertMsgFitsShort(
              (uint64_t)count * m_gen.sizeofDataType(FLOAT), m_gen.valueShort(),
              "OverlayAllreduce isend");
          m_gen.enQ_isend(evQ, out, count, FLOAT, next_peer_id, msg_tag, m_comm,
                          &m_req_send[global_stage][next_peer_counter++],
                          route_class);
          m_send_size += count;
          std::string peers_str = "[";
          for (size_t k = 0; k < next_tree_ids.size(); ++k) {
            peers_str += std::to_string(next_tree_ids[k]);
            if (k + 1 < next_tree_ids.size())
              peers_str += " ";
          }
          peers_str += "]";
          DPRINTF(
              "[%d] Sending %d elements in %d global_stage %s to %d with tag "
              "%u route_class %d "
              "colltype = %s\n",
              m_r, count, global_stage, peers_str.c_str(), next_peer_id,
              msg_tag, route_class, coll_type_str.c_str());
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

/******* OverlayCollectiveRunner *******/

EmberOverlayCollGenerator::OverlayCollectiveRunner::OverlayCollectiveRunner(
    EmberOverlayCollGenerator &gen, int num_engines, uint32_t count,
    uint32_t rank, uint32_t comm_size, bool nonblocking, bool sync)
    : m_gen(gen), m_nb(nonblocking), m_sync(sync), m_has_new(false),
      m_count(count), m_r(rank), m_p(comm_size), m_handle_idx(0), m_idx(0),
      m_notify_time(0) {
  (void)num_engines;
}

bool EmberOverlayCollGenerator::OverlayCollectiveRunner::progress_phase(
    std::queue<EmberEvent *> &evQ) {
  // Prefer sync path (m_sync=true): one Engine, waitall + notifyRecv.
  if (m_r == 63) {
    int q = 1;
  }
  if (m_allreduces[0].getStep() == 1) {
    int q = 1;
  }
  if (m_sync) {
    m_active_handles.clear();
    m_active_allreduce_ptrs.clear();
    bool all_completed = true;
    for (auto &allreduce : m_allreduces) {
      bool this_completed = !allreduce.isEnabled() || allreduce.progress(evQ);
      all_completed = all_completed && this_completed;
      if (allreduce.hasPendingRecv()) {
        assert(!this_completed);
        auto [active_handlers, active_handlers_ids] = allreduce.getRecvHandle();
        m_active_handles.insert(m_active_handles.end(), active_handlers.begin(),
                                active_handlers.end());
        for (auto handler_id : active_handlers_ids) {
          m_active_allreduce_ptrs.push_back(
              std::make_tuple(&allreduce, handler_id));
        }
      }
    }
    if (!m_active_handles.empty()) {
      m_gen.enQ_waitall(evQ, m_active_handles.size(), &m_active_handles[0],
                        NULL);
      m_gen.enQ_compute(evQ, [&]() {
        for (auto [allreduce_ptr, active_handlers_id] :
             m_active_allreduce_ptrs) {
          allreduce_ptr->notifyRecv(active_handlers_id, evQ);
        }
        return 0;
      });
    } else if (!all_completed) {
      m_gen.enQ_compute(evQ, 0);
    }
    return all_completed;
  }

  // Non-sync / waitany path kept for structural parity with Trees.
  if (!m_active_handles.empty() && m_has_new) {
    auto [allreduce_ptr, active_handlers_id] = m_active_allreduce_ptrs[m_idx];
    allreduce_ptr->notifyRecv(active_handlers_id, evQ);
  } else if (!m_active_handles.empty() && !m_has_new) {
    return false;
  }

  m_has_new = false;
  m_active_handles.clear();
  m_active_allreduce_ptrs.clear();
  bool all_completed = true;
  for (auto &allreduce : m_allreduces) {
    bool this_completed = !allreduce.isEnabled() || allreduce.progress(evQ);
    all_completed = all_completed && this_completed;
    if (allreduce.hasPendingRecv()) {
      assert(!this_completed);
      auto [active_handlers, active_handlers_ids] = allreduce.getRecvHandle();
      m_active_handles.insert(m_active_handles.end(), active_handlers.begin(),
                              active_handlers.end());
      for (auto handler_id : active_handlers_ids) {
        m_active_allreduce_ptrs.push_back(
            std::make_tuple(&allreduce, handler_id));
      }
    }
  }
  if (!m_active_handles.empty()) {
    m_has_new = true;
    m_gen.enQ_waitany(evQ, m_active_handles.size(), &m_active_handles[0],
                      &m_idx, &m_resp_recv);
    m_gen.enQ_getTime(evQ, &m_notify_time);
    return false;
  }
  return all_completed;
}

void EmberOverlayCollGenerator::OverlayCollectiveRunner::printStats() {
  uint64_t rank_time = m_stop_time - m_start_time;
  uint64_t bytes = m_count * m_gen.sizeofDataType(FLOAT);
  uint64_t data_moved = 0;
  uint64_t data_reduced = 0;
  for (auto &allreduce : m_allreduces) {
    data_moved += allreduce.getMovedBytes();
    data_reduced += allreduce.getDataReduced();
  }
  double bw = rank_time ? (double)8 * data_moved / rank_time : 0.0;
  double gbw = bw * m_p;
  printf("TIME %d start_time %" PRIu64 " stop_time %" PRIu64
         " rank_time %" PRIu64 " bytes %" PRIu64 " data_moved %" PRIu64
         " data_reduced %" PRIu64 " bw %lf gbw %lf\n",
         m_r, m_start_time, m_stop_time, rank_time, bytes, data_moved,
         data_reduced, bw, gbw);
}

/******* OverlayCollective *******/

EmberOverlayCollGenerator::OverlayCollective::OverlayCollective(
    EmberOverlayCollGenerator &gen, OverlaySchedule schedule, uint32_t count,
    uint32_t rank, uint32_t comm_size, Communicator comm,
    double aggregation_cost_ns, bool nb, bool sync, uint ports,
    CollType coll_type, float *data)
    : OverlayCollectiveRunner(gen, (int)ports, count, rank, comm_size, nb,
                              sync),
      mode(schedule.mode), m_ports(ports), m_state(INIT),
      m_coll_type(coll_type) {

  validateSchedule(schedule, (int)comm_size);
  allreduce_trees = std::move(schedule.trees);

  // Multi-port ownership (S uplink engines + optional intra-router engine)
  // is not wired yet. Keep the knob; reject unsupported values early.
  if (m_ports != 1) {
    fprintf(stderr,
            "[Overlay] arg.ports=%u not supported yet (only ports=1). "
            "Planned: multi-port split by router-to-router links "
            "(Spine-Leaf: S spines) + one engine for intra-router traffic "
            "(route_class=-1).\n",
            m_ports);
    exit(1);
  }

  m_stages_num = 0;
  for (const auto &tree : allreduce_trees) {
    for (const auto &e : tree.edges) {
      m_stages_num = std::max(m_stages_num, e.rsStage + 1);
      m_stages_num = std::max(m_stages_num, e.agStage + 1);
    }
  }
  assert(m_stages_num > 0);
  stages_waiting_centers.resize(2 * m_stages_num);

  // ports==1: one engine, port_id=-1 means "owns all edges".
  m_allreduces.push_back(OverlayCollectiveEngine(
      m_gen, coll_type, data, m_count, m_r, m_p, aggregation_cost_ns, comm,
      data != nullptr, /*port_id=*/-1, this));

  if (m_r == 0) {
    printf("[Overlay] n=%u trees=%zu stages=%d ports=%u mode=%s chunking=%s "
           "sync=%d\n",
           m_p, allreduce_trees.size(), m_stages_num, m_ports,
           mode == OverlayMode::RS_AG ? "rs_ag" : "reduce_bcast",
           schedule.chunking.c_str(), (int)m_sync);
  }
}

EmberOverlayCollGenerator::OverlayCollective::~OverlayCollective() {}

void EmberOverlayCollGenerator::OverlayCollective::addWaitingCenters(
    int stage, std::vector<int> &tree_ids, CollType coll_type) {
  int global_stage =
      stage + (coll_type == OVERLAY_REDUCE_SCATTER ? 0 : m_stages_num);
  for (int tree_id : tree_ids) {
    stages_waiting_centers[global_stage][tree_id]++;
  }
}

bool EmberOverlayCollGenerator::OverlayCollective::progress(
    std::queue<EmberEvent *> &evQ) {
  switch (m_state) {
  case INIT:
    m_gen.enQ_getTime(evQ, &m_start_time);
    m_state = PHASE_1;
    // fallthrough
  case PHASE_1:
    if (!progress_phase(evQ))
      return false;
    m_gen.enQ_getTime(evQ, &m_stop_time);
    m_state = FINI;
    return false;
  case FINI:
    return true;
  default:
    assert(0);
  }
  assert(0);
}

void EmberOverlayCollGenerator::OverlayCollective::reset() {
  m_state = INIT;
  for (auto &allreduce : m_allreduces)
    allreduce.reset();
}

/******* parent generator *******/

EmberOverlayCollGenerator::EmberOverlayCollGenerator(SST::ComponentId_t id,
                                                     Params &params)
    : EmberMessagePassingGenerator(id, params, "None"), m_tag(0) {
  assert(0);
}

EmberOverlayCollGenerator::EmberOverlayCollGenerator(SST::ComponentId_t id,
                                                     Params &params,
                                                     std::string name)
    : EmberMessagePassingGenerator(id, params, name), m_tag(0) {}

bool EmberOverlayCollGenerator::generate(std::queue<EmberEvent *> &evQ) {
  (void)evQ;
  assert(0);
  return true;
}
