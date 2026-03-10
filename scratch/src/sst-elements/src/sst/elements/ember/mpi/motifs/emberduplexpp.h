#ifndef _H_EMBER_DUPLEXPP_MOTIF
#define _H_EMBER_DUPLEXPP_MOTIF

#include "mpi/embermpigen.h"

using namespace SST::Ember;
using namespace SST::Hermes;

class EmberDuplexPP : public EmberMessagePassingGenerator {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(EmberDuplexPP, "ember", "DuplexPPMotif",
                                SST_ELI_ELEMENT_VERSION(1, 0, 0),
                                "Rank 0 sends 1 packet to ranks 1 and 2",
                                SST::Ember::EmberGenerator)

  SST_ELI_DOCUMENT_PARAMS({"arg.count",
                           "Payload size in number of FLOAT elements", "1"})

  EmberDuplexPP(ComponentId_t id, Params &params)
      : EmberMessagePassingGenerator(id, params, "DuplexPP"), m_dtype(FLOAT) {
    // число float-элементов в одном сообщении
    m_count = (uint32_t)params.find<uint32_t>("arg.count", 1);
    m_send_num = (uint32_t)params.find<uint32_t>("arg.send_num", 2);
    m_state = INIT;

    m_send.assign(100, 1.0f);
    m_recv.assign(100, 0.0f);

    m_reqs.resize(2); // два запроса на isend
  }

  bool generate(std::queue<EmberEvent *> &evQ) override {

    if (m_state == DONE)
      return true;
    int packet_size = 8192;
    int packet_float_count = 2048;
    // Тест рассчитан на ранги 0,1,2; остальные ничего не делают
    int r = rank();
    if (r != 0 && r != 1 && r != 2)
      return true;

    if (m_state == INIT) {

      // Приёмники 1 и 2 меряют свою латентность
      if (r == 1 || r == 2) {
        enQ_getTime(evQ, &m_t0);

        enQ_recv(evQ,
                 m_recv.data(),                   // буфер
                 packet_float_count * m_send_num, // число элементов
                 m_dtype,                         // тип
                 /*src=*/0,                       // источник
                 m_tag, m_comm, &m_resp);

        enQ_getTime(evQ, &m_t1);

        enQ_compute(evQ, [=] {
          uint64_t dt = m_t1 - m_t0;
          printf("[DuplexPP] 0->%d: count=%u FLOATs, latency=%" PRIu64 " ns\n",
                 r, m_count, dt);
          fflush(stdout);
          return 0;
        });
      }
      // Отправитель 0: две неблокирующие отправки + ожидание их завершения
      else if (r == 0) {
        // isend в ранг 1
        enQ_isend(evQ, m_send.data(), packet_float_count * m_send_num, m_dtype,
                  /*dst=*/1, m_tag, m_comm, &m_reqs[0]);

        // isend в ранг 2
        enQ_isend(evQ, m_send.data(), packet_float_count * m_send_num, m_dtype,
                  /*dst=*/2, m_tag, m_comm, &m_reqs[1]);

        // дождаться окончания обеих отправок
        enQ_wait(evQ, &m_reqs[0]);
        enQ_wait(evQ, &m_reqs[1]);
      }

      m_state = DONE;
      return false;
    }

    // m_state == DONE
    return true;
  }

private:
  enum State { INIT, DONE };

  State m_state;
  int m_send_num;
  uint32_t m_count;
  const PayloadDataType m_dtype;

  Communicator m_comm{GroupWorld};
  int m_tag{42};

  uint64_t m_t0{}, m_t1{};
  MessageResponse m_resp{};

  std::vector<MessageRequest> m_reqs; // два запроса isend на ранге 0
  std::vector<float> m_send;
  std::vector<float> m_recv;
};

#endif