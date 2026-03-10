// Copyright 2009-2025 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2025, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// of the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#include "emberalltoall.h"
#include <sst_config.h>

using namespace SST::Ember;

EmberAlltoallGenerator::EmberAlltoallGenerator(SST::ComponentId_t id,
                                               Params &params)
    : EmberMessagePassingGenerator(id, params, "Alltoall"), m_loopIndex(0) {
  uint dimensions = (uint)params.find("arg.dimensions", 1);
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
      size_t index =
          dimensions - i - 1; // Dimensions are numbered in the reverse order
      dimensions_sizes[index] = std::stoul(tmp);
      ++i;
    }
    std::cout << "Dimensions: ";
    for (int i = dimensions - 1; i >= 0; i--) {
      std::cout << dimensions_sizes[i] << " ";
    }
    std::cout << std::endl;
  }
  m_p = dimensions_sizes[0] * dimensions_sizes[1];
  m_iterations = (uint32_t)params.find("arg.iterations", 2);
  m_compute = (uint32_t)params.find("arg.compute", 0);
  m_count = (uint32_t)params.find("arg.count", 1);
  //   jobId = (int)params.find<int>("_jobId"); // NetworkSim
  m_sendBuf = NULL;
  m_recvBuf = NULL;
}
void EmberAlltoallGenerator::printStats() {
  uint64_t rank_time = m_stopTime - m_startTime;
  uint64_t bytes = m_count * sizeofDataType(FLOAT);
  uint64_t data_moved = m_count * sizeofDataType(FLOAT) * (m_p - 1);
  double bw = (double)8 * data_moved / rank_time;
  double gbw = bw * m_p;
  int m_r = rank();
  // printf("Size %d - Start %" PRIu64 " - Stop %" PRIu64 " - Diff %" PRIu64 "
  // - Count %d - JobId %d\n", m_gen.size(), m_start_time, m_stop_time,
  // m_stop_time - m_start_time, m_count, m_gen.getJobId());
  printf("TIME %d start_time %" PRIu64 " stop_time %" PRIu64
         " rank_time %" PRIu64 " bytes %" PRIu64 " data_moved %" PRIu64
         " bw %lf gbw %lf\n",
         m_r, m_startTime, m_stopTime, rank_time, bytes, data_moved, bw, gbw);
}
bool EmberAlltoallGenerator::generate(std::queue<EmberEvent *> &evQ) {

  if (m_loopIndex == m_iterations) {
    printStats();
    return true;
  }
  if (0 == m_loopIndex) {
    enQ_getTime(evQ, &m_startTime);
  }

  enQ_compute(evQ, m_compute);
  int block_size = m_count / m_p;
  enQ_alltoall(evQ, m_sendBuf, block_size, FLOAT, m_recvBuf, block_size, FLOAT,
               GroupWorld);

  if (++m_loopIndex == m_iterations) {
    enQ_getTime(evQ, &m_stopTime);
  }
  return false;
}
