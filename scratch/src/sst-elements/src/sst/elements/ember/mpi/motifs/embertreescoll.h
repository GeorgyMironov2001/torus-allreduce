#ifndef _H_EMBER_TREESCOLL_MOTIF
#define _H_EMBER_TREESCOLL_MOTIF

#include "mpi/embermpigen.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
namespace SST {
namespace Ember {

// TreeEdge: от кого -> к кому, и стадии для RS/AG (-1 если не участвует)
struct TreeEdge {
  int from;    // источник
  int to;      // приёмник
  int rsStage; // стадия Reduce-Scatter (0..L-1 или -1)
  int agStage; // стадия Allgather     (0..L-1 или -1)
  int route_class{-1};
};

struct TreeSpec {
  int center = -1;
  std::vector<TreeEdge> edges;
  int dim_size;
  int dimension;
  std::vector<uint> dimensions;
  std::vector<std::vector<TreeEdge>> inside_peers;
  std::vector<std::vector<TreeEdge>> outside_peers;
  auto get_peers(int center_id, int rank);
  explicit TreeSpec(int center_, int dim_size,
                    const std::vector<TreeEdge> &edges_, int D = 2)
      : center(center_), dim_size(dim_size), dimension(D), edges(edges_) {

    int m_p = (int)std::pow(dim_size, dimension);
    dimensions.assign(dimension, dim_size);
    inside_peers.assign(m_p, std::vector<TreeEdge>());
    outside_peers.assign(m_p, std::vector<TreeEdge>());
    for (const auto &e : edges) {
      inside_peers[e.from].push_back(e);
      outside_peers[e.to].push_back(e);
    }
  }
  explicit TreeSpec(int center_, std::vector<uint> &dimensions_,
                    const std::vector<TreeEdge> &edges_)
      : center(center_), dimensions(dimensions_), edges(edges_),
        dimension(dimensions_.size()), dim_size(-1) {
    int m_p = 1;
    for (uint dim : dimensions_) {
      m_p *= dim;
    }
    inside_peers.assign(m_p, std::vector<TreeEdge>());
    outside_peers.assign(m_p, std::vector<TreeEdge>());
    for (const auto &e : edges) {
      inside_peers[e.from].push_back(e);
      outside_peers[e.to].push_back(e);
    }
  }
  TreeSpec() : center(-1), dim_size(0), inside_peers(0), outside_peers(0) {}
};

class EmberTreesCollGenerator : public EmberMessagePassingGenerator {

private:
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
  enum CollType { TREES_ALLREDUCE = 0, TREES_REDUCE_SCATTER, TREES_ALLGATHER };
  enum ring_allreduce_state_t { REDUCE_SCATTER = 0, ALL_GATHER, FINI };
  enum MessageRequestType { RECV = 0, SEND };
  class TreesCollectiveRunner;
  class TreesCollective;
  class TreesCollectiveEngine {

  public:
    TreesCollectiveEngine(EmberTreesCollGenerator &gen, CollType coll_type,
                          uint *dimensions, uint dimensions_num, float *dst,
                          uint32_t count, uint32_t vrank, uint32_t numproc,
                          double aggregation_cost_ns, Communicator comm,
                          bool validate, bool latency_optimal, int port_id,
                          TreesCollective *runner,
                          std::vector<std::vector<int>> &route_table,
                          std::map<std::pair<int, int>, int> &route_table_map);
    bool progress(std::queue<EmberEvent *> &evQ);
    bool hasPendingRecv();
    bool hasPendingSend();
    std::pair<std::vector<MessageRequest>, std::vector<std::pair<int, int>>>
    getRecvHandle();
    std::pair<std::vector<MessageRequest>, std::vector<int>> getSendHandle();
    void notifyRecv();
    void notifyRecv(std::pair<int, int> chunk_id,
                    std::queue<EmberEvent *> &evQ);
    void notifySend();
    void notifySend(int chunk_id);
    void setBuff(float *new_dest);
    void reset();
    float *getBuff();
    uint64_t getMovedBytes();
    uint64_t getDataReduced() { return m_data_reduced; }
    void setEnable(bool enable);
    bool isEnabled();
    uint32_t getStep() { return m_i; }
    uint32_t getMr() { return m_r; }
    // uint getCenterId() { return m_center_id; }
    ring_allreduce_state_t getState() { return m_state; }
    std::vector<int> getRouteTablePath(int from, int to, int route_class);

    std::vector<std::map<std::pair<int, int>, std::vector<int>>>
        scatter_port_send;
    std::vector<std::map<std::pair<int, int>, std::vector<int>>>
        scatter_port_recv;
    std::vector<std::map<std::pair<int, int>, std::vector<int>>>
        allgather_port_send;
    std::vector<std::map<std::pair<int, int>, std::vector<int>>>
        allgather_port_recv;
    std::vector<std::map<std::pair<int, int>, std::vector<int>>>
        scatter_peers_send;
    std::vector<std::map<std::pair<int, int>, std::vector<int>>>
        allgather_peers_send;
    std::vector<std::map<std::pair<int, int>, std::vector<int>>>
        scatter_peers_recv;
    std::vector<std::map<std::pair<int, int>, std::vector<int>>>
        allgather_peers_recv;

    std::vector<std::vector<int>> route_table;
    std::map<std::pair<int, int>, int> route_table_map;

  private:
    bool waiting_receive(CollType coll_type);
    bool waiting_send();
    void getCoordFromId(int id, int *coord);
    int getIdFromCoord(int *coord, uint *dimensions, uint dimensions_num);
    void computeBlocksBitmap(int sender, int step, uint8_t *blocks_bitmap);
    int getPeer(int sender, int step, CollType collective);
    bool collective(std::queue<EmberEvent *> &evQ, CollType coll_type);
    int recv_scatter_started, send_scatter_started, recv_allgather_started,
        send_allgather_started;
    // bool reduce(std::queue<EmberEvent *> &evQ);
    // bool broadcast(std::queue<EmberEvent *> &evQ);
    bool pack(std::queue<EmberEvent *> &evQ);
    void processReceivedData(std::queue<EmberEvent *> &evQ, CollType coll_type,
                             int chunk_stage, int chunk_id);
    uint8_t **getBitmaps(int rank);
    uint32_t getBlockOffset(uint32_t block_idx);
    uint32_t getBlockSize(uint32_t block_idx);
    uint32_t messageTag(CollType coll_type, int route_class) const;

  private:
    int m_stages_num;
    uint64_t m_prev_time = 0;
    uint64_t m_data_reduced;
    TreesCollective *m_runner;
    EmberTreesCollGenerator &m_gen;
    float *m_dst;
    std::vector<EpochRecv> m_recv_epoch;
    EpochSend m_send_epoch;
    ring_allreduce_state_t m_state;
    // std::vector<float> m_tmp;
    // std::vector<float> m_send_tmp;
    std::vector<uint32_t> m_inside_peers;
    std::vector<uint32_t> m_outside_peers;
    uint32_t m_p;
    uint32_t m_r;
    uint32_t m_i;
    uint32_t m_tag_reduce, m_tag_broadcast;
    uint32_t m_count;
    std::vector<int> m_waiting_recv_centers;
    std::vector<bool> m_waiting_send;
    // std::vector<bool> m_ready_to_recv;
    bool m_ready_to_recv;
    bool m_ready_to_send;
    double m_aggregation_cost_ns;
    std::vector<std::vector<MessageRequest>> m_req_recv;
    std::vector<std::vector<MessageRequest>> m_req_send;
    uint64_t m_data_sent;
    Communicator m_comm;
    uint8_t *m_blocks_bitmap_s; // Bitmap representing the blocks I sent
    uint8_t *m_blocks_bitmap_r; // Bitmap representing the blocks I received

    uint8_t **m_my_blocks_matrix;
    // For each rank and for each step I have a bitmap of blocks rank 'rank'
    // sends at step 'step': [rank][step][blocks]. From those I can get also
    // blocks to recv etc
    std::vector<std::vector<std::vector<uint>>> m_blocks;

    uint32_t m_recv_size, m_send_size;
    uint *m_dimensions;
    uint m_dimensions_num;
    bool m_validate;
    // uint m_center_id;
    // uint ***m_peers_scatter_send;
    // std::vector<std::vector<std::map<int, std::vector<int>>>>
    //     m_peers_scatter_send;
    // std::vector<std::vector<std::map<int, std::vector<int>>>>
    //     m_peers_scatter_recv;
    // std::vector<std::vector<std::map<int, std::vector<int>>>>
    //     m_peers_gather_send;
    // std::vector<std::vector<std::map<int, std::vector<int>>>>
    //     m_peers_gather_recv;
    bool m_enabled;
    bool m_latency_optimal;

    bool m_do_reduce_scatter;
    bool m_do_allgather;
    int m_port_id;
  };

  class TreesCollectiveRunner {
  public:
    TreesCollectiveRunner(EmberTreesCollGenerator &gen, int num_allreduces,
                          uint32_t count, uint32_t rank, uint32_t comm_size,
                          bool nonblocking, bool sync);
    virtual bool progress(std::queue<EmberEvent *> &evQ) = 0;
    virtual void reset() = 0;
    bool progress_phase(std::queue<EmberEvent *> &evQ);
    void printStats();

  protected:
    std::vector<TreesCollectiveEngine> m_allreduces;
    std::vector<int> m_reduce_scatters_idx;
    std::vector<std::pair<int, bool>> m_allgathers_idx;
    std::vector<int> m_allgathers_permutation;

    std::vector<MessageRequest> m_active_handles;
    std::vector<std::tuple<TreesCollectiveEngine *, std::pair<int, int>>>
        m_active_allreduce_ptrs;
    // std::vector<TorusCollectiveEngine *> m_active_allreduce_ptrs;

    EmberTreesCollGenerator &m_gen;
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

public:
  // TorusCollective
  // This is a torus collective on a d-dimensional torus.
  // It is 'Single' in the sense that there is a single collective running on
  // m_count elements.
  class TreesCollective : public TreesCollectiveRunner {
  public:
    TreesCollective(EmberTreesCollGenerator &gen, uint dimensions_num,
                    uint ports, uint32_t count, uint32_t rank,
                    uint32_t comm_size, Communicator comm,
                    std::vector<TreeSpec> tree_specs,
                    std::vector<std::vector<int>> &route_table,
                    std::map<std::pair<int, int>, int> &route_table_map,
                    double aggregation_cost_ns = 0, bool nb = false,
                    bool sync = true, CollType coll_type = TREES_ALLREDUCE,
                    float *data = NULL, uint *dimensions = NULL,
                    bool latency_optimal = false);
    ~TreesCollective();
    bool progress(std::queue<EmberEvent *> &evQ) override;
    void reset() override;
    void addWaitingCenters(int stage, std::vector<int> &centers,
                           CollType coll_type);

    std::vector<std::map<int, int>> stages_waiting_centers;
    std::vector<TreeSpec> allreduce_trees;

  private:
    int m_stages_num;
    uint *m_dimensions;
    enum allreduce_state_t { INIT, PHASE_1, FINI };

  private:
    allreduce_state_t m_state;
    CollType m_coll_type;
  };

public:
  SST_ELI_REGISTER_SUBCOMPONENT(
      EmberTreesCollGenerator, "ember", "EmberTreesCollMotif",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "EmberTreesCollMotif parent motif -- this is meant "
      "to be a superclass of EmberTreesCollMotif motifs",
      SST::Ember::EmberGenerator)

public:
  EmberTreesCollGenerator(SST::ComponentId_t, Params &params);
  EmberTreesCollGenerator(SST::ComponentId_t, Params &params, std::string name);
  bool generate(std::queue<EmberEvent *> &evQ);

  uint32_t getNextTag() { return m_tag++; }

  // From defaultParams.py valueShort (bytes); set by TreesAllreduce.
  uint64_t valueShort() const { return m_value_short; }
  void setValueShort(uint64_t v) { m_value_short = v; }

private:
  uint32_t m_tag = 0;
  uint64_t m_value_short = 0;
};

} // namespace Ember
} // namespace SST

#endif
