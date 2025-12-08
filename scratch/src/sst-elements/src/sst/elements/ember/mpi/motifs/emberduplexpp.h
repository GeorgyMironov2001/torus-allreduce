#ifndef _H_EMBER_DUPLEXPP_MOTIF
#define _H_EMBER_DUPLEXPP_MOTIF

#include "mpi/embermpigen.h"

using namespace SST::Ember;

class EmberDuplexPP : public EmberMessagePassingGenerator {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
      EmberDuplexPP, "ember", "DuplexPPMotif", SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Duplex vs PingPong micro-benchmark (float only)",
      SST::Ember::EmberGenerator)

  SST_ELI_DOCUMENT_PARAMS({"arg.count",
                           "Payload size in number of float elements", "1"},
                          {"arg.mode", "duplex | pingpong", "duplex"})

  EmberDuplexPP(ComponentId_t id, Params &params)
      : EmberMessagePassingGenerator(id, params, "DuplexPP"), m_dtype(FLOAT),
        m_peer(0), m_comm(GroupWorld) {

    // Основной параметр — число float-элементов
    m_count = (uint32_t)params.find<uint32_t>("arg.count", 1);

    m_mode = params.find<std::string>("arg.mode", "duplex");
    send_num = params.find<int>("arg.send_num", 1);
    m_p = 64;
    m_block_size = m_count / m_p;
    m_reqs.resize(m_p);
    m_sends.resize(m_p);
    can_continue = false;
    // Напарник (рассчитано на 2 ранга)
    m_state = REDUCE_SCATTER;
  }

  bool generate(std::queue<EmberEvent *> &evQ) override {
    if (m_state == REDUCE_SCATTER && m_i == 0) {
      enQ_getTime(evQ, &m_t0);
    }
    if (need_stop) {
      printf("Rank %d, Time: %lu\n", rank(), m_t1 - m_t0);
      return true;
    }
    if (m_i > 0) {
      if (!can_continue) {
        return false;
      } else if (m_state == ALLGATHER) {
        enQ_getTime(evQ, &m_t1);
        need_stop = true;
        return false;
      } else {
        m_state = ALLGATHER;
        m_i = 0;
      }
    }
    can_continue = false;
    int id = -1;
    for (int peer = 0; peer < m_p; ++peer) {
      if (peer == rank()) {
        continue;
      }
      id++;
      enQ_isend(evQ, m_send, m_block_size, FLOAT, peer, m_tag1, m_comm,
                &m_sends[id]);
      enQ_irecv(evQ, m_recv, m_block_size, FLOAT, peer, m_tag1, m_comm,
                &m_reqs[id]);
    }
    enQ_waitall(evQ, m_p - 1, m_reqs.data(), nullptr);
    enQ_compute(evQ, [this]() {
      can_continue = true;
      return 0;
    });
    m_i++;
    return false;
  }

private:
  enum AllreduceState { REDUCE_SCATTER, ALLGATHER };
  AllreduceState m_state;
  int m_i{0};
  uint32_t m_count;
  std::string m_mode;
  int m_tag1{11}, m_tag2{12};
  int m_block_size{0};
  // Жёстко: только FLOAT
  bool need_stop = false;
  const PayloadDataType m_dtype;
  int m_peer;
  Communicator m_comm{GroupWorld};
  int m_p;
  MessageRequest m_s1{}, m_r1{}, m_s2{}, m_r2{};
  std::vector<MessageRequest> m_reqs;
  std::vector<MessageRequest> m_sends;
  bool can_continue;
  int send_num;
  uint64_t m_t0{}, m_t1{};
  void *m_send{nullptr};
  void *m_recv{nullptr};
};

#endif