#ifndef _H_EMBER_TORUSCOLL_MOTIF
#define _H_EMBER_TORUSCOLL_MOTIF

#include "mpi/embermpigen.h"

namespace SST {
namespace Ember {

class EmberTorusCollGenerator : public EmberMessagePassingGenerator {

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
  enum CollType { TORUS_ALLREDUCE = 0, TORUS_REDUCE_SCATTER, TORUS_ALLGATHER };
  enum ring_allreduce_state_t { REDUCE_SCATTER = 0, ALL_GATHER, FINI };
  enum MessageRequestType { RECV = 0, SEND };
  class TorusCollectiveRunner;
  class TorusCollective;
  class TorusCollectiveEngine {

  public:
    TorusCollectiveEngine(EmberTorusCollGenerator &gen, CollType coll_type,
                          uint *dimensions, uint dimensions_num, float *dst,
                          uint32_t count, uint32_t vrank, uint32_t numproc,
                          double aggregation_cost_ns, Communicator comm,
                          bool validate, bool latency_optimal, int port_id,
                          TorusCollective *runner);
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
    std::vector<std::map<int, std::vector<int>>> scatter_port_send;
    std::vector<std::map<int, std::vector<int>>> scatter_port_recv;
    std::vector<std::map<int, std::vector<int>>> allgather_port_send;
    std::vector<std::map<int, std::vector<int>>> allgather_port_recv;
    std::vector<std::map<int, std::vector<int>>> scatter_peers_send;
    std::vector<std::map<int, std::vector<int>>> allgather_peers_send;
    std::vector<std::map<int, std::vector<int>>> scatter_peers_recv;
    std::vector<std::map<int, std::vector<int>>> allgather_peers_recv;

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

  private:
    uint64_t m_data_reduced;
    TorusCollective *m_runner;
    EmberTorusCollGenerator &m_gen;
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

  class TorusCollectiveRunner {
  public:
    TorusCollectiveRunner(EmberTorusCollGenerator &gen, int num_allreduces,
                          uint32_t count, uint32_t rank, uint32_t comm_size,
                          bool nonblocking, bool sync);
    virtual bool progress(std::queue<EmberEvent *> &evQ) = 0;
    virtual void reset() = 0;
    bool progress_phase(std::queue<EmberEvent *> &evQ);
    void printStats();

  protected:
    std::vector<TorusCollectiveEngine> m_allreduces;
    std::vector<int> m_reduce_scatters_idx;
    std::vector<std::pair<int, bool>> m_allgathers_idx;
    std::vector<int> m_allgathers_permutation;

    std::vector<MessageRequest> m_active_handles;
    std::vector<std::tuple<TorusCollectiveEngine *, std::pair<int, int>>>
        m_active_allreduce_ptrs;
    // std::vector<TorusCollectiveEngine *> m_active_allreduce_ptrs;

    EmberTorusCollGenerator &m_gen;
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
  class TorusCollective : public TorusCollectiveRunner {
  public:
    TorusCollective(EmberTorusCollGenerator &gen, uint dimensions_num,
                    uint ports, uint32_t count, uint32_t rank,
                    uint32_t comm_size, Communicator comm,
                    double aggregation_cost_ns = 0, bool nb = false,
                    bool sync = true, CollType coll_type = TORUS_ALLREDUCE,
                    float *data = NULL, uint *dimensions = NULL,
                    bool latency_optimal = false);
    ~TorusCollective();
    bool progress(std::queue<EmberEvent *> &evQ) override;
    void reset() override;
    void addWaitingCenters(int stage, std::vector<int> &centers,
                           CollType coll_type);

    std::vector<std::map<int, int>> stages_waiting_centers;

  private:
    uint *m_dimensions;
    enum allreduce_state_t { INIT, PHASE_1, FINI };

  private:
    allreduce_state_t m_state;
    CollType m_coll_type;
  };

  // // Improved alltoall
  // class EmberImprovedAlltoall {
  // public:
  //   EmberImprovedAlltoall(EmberSwingCollGenerator &gen, uint32_t sendcount,
  //                         uint32_t rank, uint32_t comm_size, Communicator
  //                         comm);
  //   ~EmberImprovedAlltoall();
  //   bool progress(std::queue<EmberEvent *> &evQ);
  //   int mod(int a, int b);
  //   void reset();

  // private:
  //   EmberSwingCollGenerator &m_gen;
  //   int m_r, m_p, m_sendcount, m_tag;
  //   Communicator m_comm;

  //   uint64_t m_time_debug;
  //   uint32_t m_loopIndex = 0;
  //   uint32_t m_iterations = 1;
  //   uint32_t m_messageSize;
  //   uint64_t m_startTime;
  //   uint64_t m_stopTime;

  //   MessageRequest *msgRequests;
  //   int nextMRIndex = 0;

  //   MessageResponse m_resp;
  //   void *m_sendBuf;
  //   void *m_recvBuf;
  // };

public:
  SST_ELI_REGISTER_SUBCOMPONENT(EmberTorusCollGenerator, "ember",
                                "EmberTorusMotif",
                                SST_ELI_ELEMENT_VERSION(1, 0, 0),
                                "EmberTorusMotif parent motif -- this is meant "
                                "to be a superclass of EmberTorusMotif motifs",
                                SST::Ember::EmberGenerator)

public:
  EmberTorusCollGenerator(SST::ComponentId_t, Params &params);
  EmberTorusCollGenerator(SST::ComponentId_t, Params &params, std::string name);
  bool generate(std::queue<EmberEvent *> &evQ);

  uint32_t getNextTag() { return m_tag++; }

private:
  uint32_t m_tag = 0;
};

} // namespace Ember
} // namespace SST

#endif
