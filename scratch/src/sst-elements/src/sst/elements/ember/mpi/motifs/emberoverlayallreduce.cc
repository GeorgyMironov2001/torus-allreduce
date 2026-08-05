#include "emberoverlayallreduce.h"
#include <sst_config.h>

using namespace SST::Ember;

EmberOverlayAllreduceGenerator::EmberOverlayAllreduceGenerator(
    SST::ComponentId_t id, Params &params)
    : EmberOverlayCollGenerator(id, params, "OverlayAllreduce") {

  double aggregation_cost_ns =
      (double)params.find("arg.aggregation_cost_ns", 0.01);
  uint32_t recvcount = (uint32_t)params.find("arg.count", 1);
  bool blocking = (bool)params.find("arg.blocking", true);
  bool sync = (bool)params.find("arg.sync", true);
  uint ports = (uint)params.find("arg.ports", 1);
  int validate = (int)params.find("arg.validate", 0);
  std::string trees_file = params.find<std::string>("arg.trees_file", "");
  std::string mode_override = params.find<std::string>("arg.mode", "");

  if (trees_file.empty()) {
    fatal(CALL_INFO, -1, "OverlayAllreduceMotif requires arg.trees_file\n");
  }
  if (ports < 1) {
    fatal(CALL_INFO, -1, "OverlayAllreduceMotif: arg.ports must be >= 1\n");
  }

  // JSON → OverlaySchedule: EmberOverlayCollGenerator::loadSchedule
  OverlaySchedule schedule = loadSchedule(trees_file);
  if (!mode_override.empty()) {
    schedule.mode = parseMode(mode_override);
  }

  m_validate = validate;
  m_recvcount = recvcount;
  m_data = nullptr;
  m_data_validation_send = nullptr;
  m_data_validation_recv = nullptr;
  m_validation_reduce_executed = false;

  if (m_validate) {
    memSetBacked();
    m_data = (float *)memAlloc(sizeofDataType(FLOAT) * recvcount);
    m_data_validation_send =
        (float *)memAlloc(sizeofDataType(FLOAT) * recvcount);
    m_data_validation_recv =
        (float *)memAlloc(sizeofDataType(FLOAT) * recvcount);
    for (uint32_t i = 0; i < recvcount; i++) {
      m_data[i] = static_cast<float>(rand() % 1024);
      m_data_validation_send[i] = m_data[i];
    }
  }

  m_allreduce = new OverlayCollective(
      *this, std::move(schedule), recvcount, rank(), size(), GroupWorld,
      aggregation_cost_ns, !blocking, sync, ports, OVERLAY_ALLREDUCE, m_data);
}

EmberOverlayAllreduceGenerator::~EmberOverlayAllreduceGenerator() {
  if (m_allreduce) {
    m_allreduce->printStats();
    delete m_allreduce;
    m_allreduce = nullptr;
  }
}

bool EmberOverlayAllreduceGenerator::generate(std::queue<EmberEvent *> &evQ) {
  if (!m_allreduce->progress(evQ)) {
    return false;
  }

  if (m_validate) {
    if (!m_validation_reduce_executed) {
      enQ_allreduce(evQ, m_data_validation_send, m_data_validation_recv,
                    m_recvcount, FLOAT, Hermes::MP::SUM, GroupWorld);
      m_validation_reduce_executed = true;
      return false;
    }
    bool valid = true;
    for (int i = 0; i < m_recvcount; i++) {
      if (m_data[i] != m_data_validation_recv[i]) {
        fprintf(stderr,
                "Validation error on rank %d at index %d (%f vs. %f)\n",
                rank(), i, m_data[i], m_data_validation_recv[i]);
        valid = false;
      }
    }
    if (valid) {
      printf("[Rank %d] Overlay validation succeeded.\n", rank());
      fflush(stdout);
    }
  }
  return true;
}
