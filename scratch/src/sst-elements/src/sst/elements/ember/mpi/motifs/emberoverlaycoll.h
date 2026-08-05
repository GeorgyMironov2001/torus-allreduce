#ifndef _H_EMBER_OVERLAYCOLL_MOTIF
#define _H_EMBER_OVERLAYCOLL_MOTIF

#include "mpi/embermpigen.h"
#include <map>
#include <string>
#include <vector>

namespace SST {
namespace Ember {

// Edge of an explicit overlay tree (already placed on physical ranks).
// from -> to is the reduce / RS direction (toward root).
struct OverlayEdge {
  int from = -1;
  int to = -1;
  int rsStage = 0;   // Reduce-Scatter / reduce-up stage
  int agStage = 0;   // Allgather / bcast-down stage
  int route_class = -1; // opaque fabric class; Spine-Leaf: spine index
};

struct OverlayTree {
  int id = -1;
  int root = -1;
  std::vector<OverlayEdge> edges;
  // Indexed by rank: edges with from==v / to==v (RS direction toward root).
  std::vector<std::vector<OverlayEdge>> outgoing;
  std::vector<std::vector<OverlayEdge>> incoming;

  void buildAdjacency(int n) {
    outgoing.assign(n, {});
    incoming.assign(n, {});
    for (const auto &e : edges) {
      outgoing[e.from].push_back(e);
      incoming[e.to].push_back(e);
    }
  }

  const std::vector<OverlayEdge> &edgesFrom(int v) const { return outgoing[v]; }
  const std::vector<OverlayEdge> &edgesTo(int v) const { return incoming[v]; }
};

enum class OverlayMode { REDUCE_BCAST = 0, RS_AG };

struct OverlaySchedule {
  int version = 1;
  int n = 0;
  OverlayMode mode = OverlayMode::RS_AG;
  std::string chunking = "equal_by_tree";
  std::vector<OverlayTree> trees;
};

// Topology-agnostic overlay collective.
// Mirrors embertreescoll structure: Runner + single Engine + peer maps +
// stages_waiting_centers. With m_sync=true one Engine is enough.
class EmberOverlayCollGenerator : public EmberMessagePassingGenerator {
public:
  struct RecvChunk {
    float *ptr;
    uint32_t size;
    int peer;
    RecvChunk(float *ptr, uint32_t size, int peer)
        : ptr(ptr), size(size), peer(peer) {}
  };

  struct EpochRecv {
    std::vector<float> slab;
    std::vector<RecvChunk> chunks;
    void reset() {
      slab.clear();
      chunks.clear();
    }
  };

  struct SendChunk {
    float *ptr;
    uint32_t size;
    int peer;
    SendChunk(float *ptr, uint32_t size, int peer)
        : ptr(ptr), size(size), peer(peer) {}
  };

  struct EpochSend {
    std::vector<float> slab;
    std::vector<SendChunk> chunks;
  };

  enum CollType {
    OVERLAY_ALLREDUCE = 0,
    OVERLAY_REDUCE_SCATTER,
    OVERLAY_ALLGATHER
  };
  enum phase_state_t { REDUCE_SCATTER = 0, ALL_GATHER, FINI };

  class OverlayCollectiveRunner;
  class OverlayCollective;
  class OverlayCollectiveEngine;

  // ------------------------------------------------------------------
  // Engine bound to a port_id.
  //
  // ports == 1 (current default):
  //   single engine, port_id = -1, owns all edges.
  //
  // Future multi-port (topo-specific ownership):
  //   - Torus-like: port = neighbor / exit toward another router.
  //   - Spine-Leaf candidate: ports = S uplink engines (route_class == spine)
  //     + 1 extra engine for intra-leaf (route_class == -1).
  //   Edge ownership later: resolve path from (peer, route_class) and check
  //   that the local hop matches this engine's port (as in embertreescoll).
  // ------------------------------------------------------------------
  class OverlayCollectiveEngine {
  public:
    OverlayCollectiveEngine(EmberOverlayCollGenerator &gen, CollType coll_type,
                            float *dst, uint32_t count, uint32_t rank,
                            uint32_t numproc, double aggregation_cost_ns,
                            Communicator comm, bool validate, int port_id,
                            OverlayCollective *runner);

    bool progress(std::queue<EmberEvent *> &evQ);
    bool hasPendingRecv();
    std::pair<std::vector<MessageRequest>, std::vector<std::pair<int, int>>>
    getRecvHandle();
    void notifyRecv(std::pair<int, int> chunk_id,
                    std::queue<EmberEvent *> &evQ);
    void setBuff(float *new_dest);
    void reset();
    float *getBuff();
    uint64_t getMovedBytes();
    uint64_t getDataReduced() { return m_data_reduced; }
    void setEnable(bool enable);
    bool isEnabled();
    uint32_t getStep() { return m_i; }
    phase_state_t getState() { return m_state; }
    int getPortId() const { return m_port_id; }

    // stage -> ((peer, route_class) -> [tree_id, ...])
    std::vector<std::map<std::pair<int, int>, std::vector<int>>>
        scatter_peers_send;
    std::vector<std::map<std::pair<int, int>, std::vector<int>>>
        scatter_peers_recv;
    std::vector<std::map<std::pair<int, int>, std::vector<int>>>
        allgather_peers_send;
    std::vector<std::map<std::pair<int, int>, std::vector<int>>>
        allgather_peers_recv;

  private:
    bool waiting_receive(CollType coll_type);
    bool waiting_send();
    bool collective(std::queue<EmberEvent *> &evQ, CollType coll_type);
    void processReceivedData(std::queue<EmberEvent *> &evQ, CollType coll_type,
                             int chunk_stage, int chunk_id);
    // Tag axes: phase × route_class × unordered {from,to}.
    // Same route_class in one stage but different host pairs → different tags.
    uint32_t messageTag(CollType coll_type, int route_class, int from_rank,
                        int to_rank) const;
    // Elements / offset for the data share of tree_id (equal_by_tree for now;
    // later: non-uniform weights via chunk layout).
    uint32_t getTreeSize(int tree_id) const;
    uint32_t getTreeOffset(int tree_id) const;

    int m_stages_num;
    uint64_t m_prev_time = 0;
    // route_class==-1 → slot 0; route_class in [0,S) → slot route_class+1.
    uint32_t m_num_rc_slots;
    uint64_t m_data_reduced;
    OverlayCollective *m_runner;
    EmberOverlayCollGenerator &m_gen;
    float *m_dst;
    std::vector<EpochRecv> m_recv_epoch;
    EpochSend m_send_epoch;
    phase_state_t m_state;
    uint32_t m_p;
    uint32_t m_r;
    uint32_t m_i;
    uint32_t m_count;
    bool m_ready_to_recv;
    bool m_ready_to_send;
    double m_aggregation_cost_ns;
    std::vector<std::vector<MessageRequest>> m_req_recv;
    std::vector<std::vector<MessageRequest>> m_req_send;
    uint64_t m_data_sent;
    Communicator m_comm;
    bool m_validate;
    bool m_enabled;
    bool m_do_reduce_scatter;
    bool m_do_allgather;
    int m_port_id; // -1 = owns all (ports==1); else topo-specific port key
    std::vector<bool> m_waiting_send;
    uint32_t m_recv_size, m_send_size;
  };

  // ------------------------------------------------------------------
  // Runner: progress_phase + stats (same role as TreesCollectiveRunner)
  // ------------------------------------------------------------------
  class OverlayCollectiveRunner {
  public:
    OverlayCollectiveRunner(EmberOverlayCollGenerator &gen, int num_engines,
                            uint32_t count, uint32_t rank, uint32_t comm_size,
                            bool nonblocking, bool sync);
    virtual bool progress(std::queue<EmberEvent *> &evQ) = 0;
    virtual void reset() = 0;
    bool progress_phase(std::queue<EmberEvent *> &evQ);
    void printStats();

  protected:
    std::vector<OverlayCollectiveEngine> m_allreduces;
    std::vector<MessageRequest> m_active_handles;
    std::vector<std::tuple<OverlayCollectiveEngine *, std::pair<int, int>>>
        m_active_allreduce_ptrs;

    EmberOverlayCollGenerator &m_gen;
    int m_handle_idx;
    int m_idx;
    bool m_nb, m_sync;
    int m_has_new;
    MessageResponse m_resp_recv;
    uint64_t m_stop_time, m_start_time;
    uint32_t m_count;
    uint32_t m_r, m_p;
    uint64_t m_notify_time;
  };

  // ------------------------------------------------------------------
  // Collective: owns trees + stages_waiting_centers, drives the engine
  // ------------------------------------------------------------------
  class OverlayCollective : public OverlayCollectiveRunner {
  public:
    OverlayCollective(EmberOverlayCollGenerator &gen, OverlaySchedule schedule,
                      uint32_t count, uint32_t rank, uint32_t comm_size,
                      Communicator comm, double aggregation_cost_ns = 0,
                      bool nb = false, bool sync = true, uint ports = 1,
                      CollType coll_type = OVERLAY_ALLREDUCE,
                      float *data = nullptr);
    ~OverlayCollective();
    bool progress(std::queue<EmberEvent *> &evQ) override;
    void reset() override;
    void addWaitingCenters(int stage, std::vector<int> &tree_ids,
                           CollType coll_type);

    // global_stage -> (tree_id -> outstanding recv count)
    std::vector<std::map<int, int>> stages_waiting_centers;
    std::vector<OverlayTree> allreduce_trees;
    OverlayMode mode = OverlayMode::RS_AG;

  private:
    int m_stages_num;
    uint m_ports;
    enum allreduce_state_t { INIT, PHASE_1, FINI };
    allreduce_state_t m_state;
    CollType m_coll_type;
  };

public:
  SST_ELI_REGISTER_SUBCOMPONENT(
      EmberOverlayCollGenerator, "ember", "EmberOverlayCollMotif",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Parent motif for topology-agnostic overlay tree collectives",
      SST::Ember::EmberGenerator)

  EmberOverlayCollGenerator(SST::ComponentId_t, Params &params);
  EmberOverlayCollGenerator(SST::ComponentId_t, Params &params,
                            std::string name);
  bool generate(std::queue<EmberEvent *> &evQ);

  uint32_t getNextTag() { return m_tag++; }

  static OverlaySchedule loadSchedule(const std::string &path);
  static OverlayMode parseMode(const std::string &mode);
  static void validateSchedule(const OverlaySchedule &sched, int world_size);

private:
  uint32_t m_tag = 0;
};

} // namespace Ember
} // namespace SST

#endif
