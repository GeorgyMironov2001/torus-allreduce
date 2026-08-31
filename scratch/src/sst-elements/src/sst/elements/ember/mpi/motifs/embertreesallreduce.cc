#include "embertreesallreduce.h"
#include "embertrees/emberbdmstrees_16_16d4.h"
#include "embertrees/emberbdmstrees_8_4d4.h"
#include "embertrees/emberbdmstrees_8_8d1.h"
#include "embertrees/emberbdmstrees_8_8d2.h"
#include "embertrees/emberbdmstrees_8_8d3.h"
#include "embertrees/emberbdmstrees_8_8d4.h"
#include "embertrees/emberbdmstrees_8_8d5.h"
#include "embertrees/emberbdmstrees_8_8d6.h"
#include "embertrees/emberbdmstrees_8_8d7.h"
#include "embertrees/emberbdmstrees_8_8d8.h"
#include "embertrees/emberbdmstrees_hyperx_8_8_2.h"
#include "embertrees/emberbdmstrees_hyperx_64_64_2.h"
#include "embertrees/embercycleopt_16_16_4.h"
#include "embertrees/embercycleopt_16_16_6.h"
#include "embertrees/embercycleopt_16_16_8.h"
#include "embertrees/embercycleopt_8_8_4.h"
#include "embertrees/embercycleopt_8_8_6.h"
#include "embertrees/embertrees_64_64.h"
#include "embertrees/embertrees_8_8_8.h"
#include "embertrees/embertrees_small_cases.h"
#include "embertreescoll.h"
#include "embershortmsgcheck.h"
#include <cinttypes>
#include <sst_config.h>
using namespace SST::Ember;

std::pair<std::vector<std::vector<int>>, std::map<std::pair<int, int>, int>>
EmberTreesAllreduceGenerator::parse_route_table(std::string route_table_file) {
  std::vector<std::vector<int>> route_table;
  std::map<std::pair<int, int>, int> route_table_map;

  std::ifstream file(route_table_file);
  nlohmann::json j;
  file >> j;
  for (const auto &entry : j) {
    std::vector<int> path;
    for (int rank : entry) {
      path.push_back(rank);
    }
    route_table.push_back(path);
  }
  return std::make_pair(route_table, route_table_map);
}

EmberTreesAllreduceGenerator::EmberTreesAllreduceGenerator(
    SST::ComponentId_t id, Params &params)
    : EmberTreesCollGenerator(id, params, "TreesAllreduce") {

  double aggregation_cost_ns =
      (double)params.find("arg.aggregation_cost_ns", 0.01);
  uint32_t recvcount = (uint32_t)params.find("arg.count", 1);
  bool blocking = (bool)params.find("arg.blocking", true);
  int validate = (int)params.find("arg.validate", 0);
  uint ports = (uint)params.find("arg.ports", 1);
  bool latency_optimal = (bool)params.find("arg.latency_optimal", false);
  bool sync = (bool)params.find("arg.sync", true);
  uint dimensions = (uint)params.find("arg.dimensions", 1);
  int multiplicator = (int)params.find("arg.multiplicator", 1);
  std::string dimensions_sizes_s =
      params.find<std::string>("arg.dimensions_sizes", "");
  uint *dimensions_sizes = NULL;
  // Split the dimensions_sizes string into the value of each dimensions
  if (dimensions_sizes_s != "") {
    dimensions_sizes = (uint *)malloc(sizeof(uint) * dimensions);
    std::string tmp;
    std::stringstream ss(dimensions_sizes_s);
    uint i = 0;
    while (getline(ss, tmp, ',')) {
      if (i >= dimensions) {
        std::cerr << "Too many dimensions sizes specified" << std::endl;
      }
      // size_t index =
      //     dimensions - i - 1; // Dimensions are numbered in the reverse order
      size_t index = i;
      dimensions_sizes[index] = std::stoul(tmp);
      ++i;
    }
    // std::cout << "Dimensions: ";
    // for (int i = dimensions - 1; i >= 0; i--) {
    //   std::cout << dimensions_sizes[i] << " ";
    // }
    // std::cout << std::endl;
  }

  m_validate = validate;
  m_recvcount = recvcount;
  m_data = NULL;
  if (m_validate) {
    memSetBacked();
    m_data = (float *)memAlloc(sizeofDataType(FLOAT) * recvcount);
    m_data_validation_send =
        (float *)memAlloc(sizeofDataType(FLOAT) * recvcount);
    m_data_validation_recv =
        (float *)memAlloc(sizeofDataType(FLOAT) * recvcount);
    printf("[%d] Validation data: ", rank());
    if (rank() == 39) {
      int q = 1;
    }
    for (size_t i = 0; i < recvcount; i++) {
      m_data[i] = rand() % 1024;
      // m_data[i] = 1.0;
      // m_data[i] = rand() % 10;
      m_data_validation_send[i] = m_data[i];
      // printf("%f ", m_data[i]);
    }
    printf("\n");
    m_validation_reduce_executed = false;
  }

  // Same as Overlay/Swing: read valueShort that Firefly uses for short path.
  setValueShort(emberLoadValueShort());
  if (rank() == 0) {
    printf("[TreesAllreduce] valueShort=%" PRIu64 "\n", valueShort());
  }

  std::string route_table_file =
      params.find<std::string>("arg.route_table_file", "");
  std::vector<std::vector<int>> route_table;
  std::map<std::pair<int, int>, int> route_table_map;
  if (route_table_file != "") {
    std::pair<std::vector<std::vector<int>>, std::map<std::pair<int, int>, int>>
        p = parse_route_table(route_table_file);
    route_table = p.first;
    route_table_map = p.second;
  }
  // bdms_tree_specs_bdms_16x16_4_quadruple
  // bdms_tree_specs_8x4_4_double
  // bdms_tree_specs_bdms_hyperx_8x8_2_quadruple
  m_allreduce = new TreesCollective(
      *this, dimensions, ports, recvcount, rank(), size(), GroupWorld,
      bdms_tree_specs_bdms_hyperx_8x8_2_quadruple, route_table, route_table_map,
      aggregation_cost_ns, !blocking, sync, TREES_ALLREDUCE, m_data,
      dimensions_sizes, latency_optimal);
}

EmberTreesAllreduceGenerator::~EmberTreesAllreduceGenerator() {
  m_allreduce->printStats();
  delete m_allreduce;
}

bool EmberTreesAllreduceGenerator::generate(std::queue<EmberEvent *> &evQ) {
  if (!m_allreduce->progress(evQ)) {
    return false;
  }
  // Allreduce over, we can return true
  // If we need to validate, we run a standard allreduce
  if (m_validate) {
    if (!m_validation_reduce_executed) {
      enQ_allreduce(evQ, m_data_validation_send, m_data_validation_recv,
                    m_recvcount, FLOAT, Hermes::MP::SUM, GroupWorld);
      m_validation_reduce_executed = true;
      return false;
    } else {
      bool valid = true;
      for (size_t i = 0; i < m_recvcount; i++) {
        if (m_data[i] != m_data_validation_recv[i]) {
          fprintf(stderr,
                  "Validation error on rank %d at index %d (%f vs. %f)\n",
                  rank(), i, m_data[i], m_data_validation_recv[i]);
          printf("Validation error on rank %d at index %d (%f vs. %f)\n",
                 rank(), i, m_data[i], m_data_validation_recv[i]);
          fflush(stdout);
          valid = false;
        }
      }
      if (valid) {
        printf("[Rank %d] Validation succeeded.\n", rank());
        fflush(stdout);
      }
    }
  }
  return true;
}