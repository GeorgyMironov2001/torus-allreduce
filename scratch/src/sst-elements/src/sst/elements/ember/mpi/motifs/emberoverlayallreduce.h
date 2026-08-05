#ifndef _H_EMBER_OVERLAYALLREDUCE_MOTIF
#define _H_EMBER_OVERLAYALLREDUCE_MOTIF

#include "emberoverlaycoll.h"

namespace SST {
namespace Ember {

class EmberOverlayAllreduceGenerator : public EmberOverlayCollGenerator {

public:
  SST_ELI_REGISTER_SUBCOMPONENT(
      EmberOverlayAllreduceGenerator, "ember", "OverlayAllreduceMotif",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Topology-agnostic AllReduce over an explicit set of overlay trees "
      "(FLOAT, SUM)",
      SST::Ember::EmberGenerator)

  SST_ELI_DOCUMENT_PARAMS(
      {"arg.aggregation_cost_ns", "Cost to sum two floats", "0.01"},
      {"arg.count", "Total number of FLOAT elements (split across trees)", "1"},
      {"arg.trees_file", "JSON schedule with explicit overlay trees", ""},
      {"arg.mode", "Optional override: reduce_bcast | rs_ag", ""},
      {"arg.blocking", "Blocking vs non-blocking", "true"},
      {"arg.sync",
       "If true, wait for all concurrent tree engines before next step",
       "true"},
      {"arg.ports",
       "Number of OverlayCollectiveEngine ports. 1 = single engine (all "
       "edges). Multi-port (e.g. S spine uplinks + 1 intra-leaf) is reserved "
       "for later; currently only 1 is supported.",
       "1"},
      {"arg.validate", "When 1, validate against MPI_Allreduce", "0"}, )

public:
  EmberOverlayAllreduceGenerator(SST::ComponentId_t, Params &params);
  ~EmberOverlayAllreduceGenerator();
  bool generate(std::queue<EmberEvent *> &evQ);

private:
  OverlayCollective *m_allreduce;
  int m_recvcount;
  int m_validate;
  float *m_data;
  float *m_data_validation_send;
  float *m_data_validation_recv;
  bool m_validation_reduce_executed;
};

} // namespace Ember
} // namespace SST

#endif
