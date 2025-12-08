#ifndef _H_EMBER_ALLTOALLALLREDUCE_MOTIF
#define _H_EMBER_ALLTOALLALLREDUCE_MOTIF

#include "mpi/embermpigen.h"

using namespace SST::Ember;

class EmberAlltoallAllreduce : public EmberMessagePassingGenerator {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
      EmberAlltoallAllreduce, "ember", "AlltoallAllreduceMotif",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Alltoall vs Allreduce micro-benchmark (float only)",
      SST::Ember::EmberGenerator)

  SST_ELI_DOCUMENT_PARAMS({"arg.count",
                           "Payload size in number of float elements", "1"},
                          {"arg.validate",
                           "When 1, the result will be validated", "0"}, )

  EmberAlltoallAllreduce(ComponentId_t id, Params &params)
      : EmberMessagePassingGenerator(id, params, "AlltoallAllreduce"),
        m_dtype(FLOAT), m_comm(GroupWorld) {

    // Основной параметр — число float-элементов
    m_count = (uint32_t)params.find<uint32_t>("arg.count", 1);
    m_validate = (bool)params.find<bool>("arg.validate", false);
    m_p = 64;
    m_block_size = m_count / m_p;
    m_reqs.resize(m_p);
    m_sends.resize(m_p);
    can_continue = false;
    m_state = REDUCE_SCATTER;

    m_send_data.resize(m_p);
    m_recv_data.resize(m_p);
    if (m_validate) {
      memSetBacked();
      m_data = (float *)memAlloc(sizeofDataType(FLOAT) * m_count);
      m_data_validation_send =
          (float *)memAlloc(sizeofDataType(FLOAT) * m_count);
      m_data_validation_recv =
          (float *)memAlloc(sizeofDataType(FLOAT) * m_count);
      for (size_t i = 0; i < m_count; i++) {
        // m_data[i] = rand() % 1024;
        m_data[i] = 1;
        m_data_validation_send[i] = m_data[i];
      }
      m_validation_reduce_executed = false;
    }
  }
  void printStats() {
    uint64_t rank_time = m_t1 - m_t0;
    uint64_t bytes = m_count * sizeofDataType(FLOAT);
    uint64_t data_moved =
        2 * (m_count / m_p) * sizeofDataType(FLOAT) * (m_p - 1);
    double bw = (double)8 * data_moved / rank_time;
    double gbw = bw * m_p;
    int m_r = rank();
    // printf("Size %d - Start %" PRIu64 " - Stop %" PRIu64 " - Diff %" PRIu64 "
    // - Count %d - JobId %d\n", m_gen.size(), m_start_time, m_stop_time,
    // m_stop_time - m_start_time, m_count, m_gen.getJobId());
    printf("TIME %d start_time %" PRIu64 " stop_time %" PRIu64
           " rank_time %" PRIu64 " bytes %" PRIu64 " data_moved %" PRIu64
           " bw %lf gbw %lf\n",
           m_r, m_t0, m_t1, rank_time, bytes, data_moved, bw, gbw);
  }

  ~EmberAlltoallAllreduce() { printStats(); }

  bool generate(std::queue<EmberEvent *> &evQ) override {
    if (m_state == REDUCE_SCATTER && m_i == 0) {
      enQ_getTime(evQ, &m_t0);
    }
    if (need_stop) {
      //   printf("Rank %d, Time: %lu\n", rank(), m_t1 - m_t0);

      if (m_validate) {
        if (!m_validation_reduce_executed) {
          enQ_allreduce(evQ, m_data_validation_send, m_data_validation_recv,
                        m_count, FLOAT, Hermes::MP::SUM, GroupWorld);
          m_validation_reduce_executed = true;
          return false;
        } else {
          bool valid = true;
          for (size_t i = 0; i < m_count; i++) {
            if (m_data[i] != m_data_validation_recv[i]) {
              fprintf(stderr,
                      "Validation error on rank %d at index %d (%f vs. %f)\n",
                      rank(), i, m_data[i], m_data_validation_recv[i]);
              valid = false;
            }
          }
          if (valid) {
            printf("[Rank %d] Validation succeeded.\n", rank());
          }
        }
      }

      return true;
    }
    if (m_i > 0) {
      if (!can_continue) {
        return false;
      } else if (m_state == REDUCE_SCATTER) {
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
      m_send_data[peer].assign(m_count, 0);
      m_recv_data[peer].assign(m_count, 0);
      if (m_validate) {
        for (size_t i = 0; i < m_count; i++) {
          // if (m_state == REDUCE_SCATTER) {
          //   m_send_data[peer][i] = m_data[i + peer * m_block_size];
          // } else {
          //   m_send_data[peer][i] = m_data[i + rank() * m_block_size];
          // }
          m_send_data[peer][i] = m_data[i];
        }
      }
      enQ_isend(evQ, m_send_data[peer].data(), m_count, FLOAT, peer, m_tag1,
                m_comm, &m_sends[id]);
      enQ_irecv(evQ, m_recv_data[peer].data(), m_count, FLOAT, peer, m_tag1,
                m_comm, &m_reqs[id]);
    }
    enQ_waitall(evQ, m_p - 1, m_reqs.data(), nullptr);
    enQ_compute(evQ, [this]() {
      can_continue = true;
      if (m_validate) {
        for (int peer = 0; peer < m_p; ++peer) {
          if (peer == rank()) {
            continue;
          }
          for (size_t i = 0; i < m_count; i++) {
            // if (m_state == REDUCE_SCATTER) {
            //   m_data[i + rank() * m_block_size] += m_recv_data[peer][i];
            // } else {
            //   m_data[i + peer * m_block_size] = m_recv_data[peer][i];
            // }
            m_data[i] += m_recv_data[peer][i];
          }
        }
      }
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
  Communicator m_comm{GroupWorld};
  int m_p;
  MessageRequest m_s1{}, m_r1{}, m_s2{}, m_r2{};
  std::vector<MessageRequest> m_reqs;
  std::vector<MessageRequest> m_sends;
  bool can_continue;
  int send_num;
  uint64_t m_t0{}, m_t1{};
  std::vector<std::vector<float>> m_send_data;
  std::vector<std::vector<float>> m_recv_data;
  bool m_validate;
  float *m_data;
  float *m_data_validation_send;
  float *m_data_validation_recv;
  bool m_validation_reduce_executed;
};

#endif