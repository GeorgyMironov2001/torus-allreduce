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

#ifndef _H_EMBER_ALLTOALL_CUSTOME_MOTIF
#define _H_EMBER_ALLTOALL_CUSTOME_MOTIF

#include <map>
#include <random>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "mpi/embermpigen.h"

namespace SST {
namespace Ember {

struct AlltoallvPattern {
  std::vector<int> send_counts;
  std::vector<int> send_displs;
  std::vector<int> recv_counts;
  std::vector<int> recv_displs;
};

class EmberAlltoallCustomeGenerator : public EmberMessagePassingGenerator {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
      EmberAlltoallCustomeGenerator, "ember", "AlltoallCustomeMotif",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Alltoall/alltoallv via isend/irecv with per-destination route_id",
      SST::Ember::EmberGenerator)

  SST_ELI_DOCUMENT_PARAMS(
      {"arg.iterations", "Number of alltoall operations to perform", "1"},
      {"arg.compute", "Time spent computing before each alltoall", "0"},
      {"arg.mode", "Volume mode: equal, fixed, fixed_file, or random", "equal"},
      {"arg.count",
       "Float elements sent to each destination in equal mode (block size)", "1"},
      {"arg.send_counts",
       "Comma-separated send counts per destination (fixed mode)", ""},
      {"arg.recv_counts",
       "Comma-separated recv counts per source (fixed mode, optional)", ""},
      {"arg.counts_file",
       "File with one line per rank: P send counts (fixed_file mode)", ""},
      {"arg.recv_counts_file",
       "Optional file with one line per rank: P recv counts (fixed_file mode)",
       ""},
      {"arg.total_count",
       "Total float elements to split randomly across destinations", "1"},
      {"arg.min_count", "Minimum per-destination count in random mode", "1"},
      {"arg.max_count", "Maximum per-destination count in random mode", "1"},
      {"arg.seed", "RNG seed base for random mode", "1"},
      {"arg.regenerate_random", "Regenerate random counts on every iteration",
       "false"},
      {"arg.route_table_file", "JSON route table: [[[src,dst],[path...]], ...]",
       ""},
      {"arg.validate", "When 1, run reference MPI alltoall and compare results",
       "0"}, )

  SST_ELI_DOCUMENT_STATISTICS(
      {"time-Init", "Time spent in Init event", "ns", 0},
      {"time-Finalize", "Time spent in Finalize event", "ns", 0},
      {"time-Isend", "Time spent in Isend event", "ns", 0},
      {"time-Irecv", "Time spent in Irecv event", "ns", 0},
      {"time-Waitall", "Time spent in Waitall event", "ns", 0},
      {"time-Compute", "Time spent in Compute event", "ns", 0},
      {"time-Gettime", "Time spent in Gettime event", "ns", 0}, )

  class EmberRoutedAlltoallv {
  public:
    EmberRoutedAlltoallv(EmberAlltoallCustomeGenerator &gen, void *sendBuf,
                         void *recvBuf, const AlltoallvPattern &pattern);
    ~EmberRoutedAlltoallv();

    bool progress(std::queue<EmberEvent *> &evQ);
    void reset();
    uint64_t bytesSent() const { return m_bytes_sent; }

  private:
    void *sendPtr(int displ) const;
    void *recvPtr(int displ) const;

    EmberAlltoallCustomeGenerator &m_gen;
    void *m_sendBuf;
    void *m_recvBuf;
    int m_r;
    int m_p;
    Communicator m_comm;
    AlltoallvPattern m_pat;

    int m_tag;
    int m_step;
    int m_req_count;
    MessageRequest *m_reqs;
    uint64_t m_bytes_sent;
  };

public:
  EmberAlltoallCustomeGenerator(SST::ComponentId_t id, Params &params);
  ~EmberAlltoallCustomeGenerator();
  bool generate(std::queue<EmberEvent *> &evQ);
  void printStats();
  void queueIterationStart(std::queue<EmberEvent *> &evQ);
  void completeIteration(std::queue<EmberEvent *> &evQ);

  uint32_t getNextTag() { return m_tag--; }
  int getRouteId(int dst);

  static std::pair<std::vector<std::vector<int>>,
                   std::map<std::pair<int, int>, int>>
  parseRouteTable(const std::string &route_table_file);

  static std::map<std::pair<int, int>, int>
  parseEcmpPathCounts(const std::string &route_table_file);

private:
  enum VolumeMode { EQUAL, FIXED, FIXED_FILE, RANDOM };

  AlltoallvPattern buildPattern();
  AlltoallvPattern buildEqualPattern();
  AlltoallvPattern buildFixedPattern(const std::string &send_counts_s,
                                     const std::string &recv_counts_s);
  AlltoallvPattern buildFixedFilePattern(const std::string &counts_file,
                                         const std::string &recv_counts_file);
  AlltoallvPattern buildRandomPattern();

  VolumeMode parseMode(const std::string &mode) const;
  static std::vector<int> parseCsvInts(const std::string &csv, int expected);
  static void buildDisplacements(const std::vector<int> &counts,
                                 std::vector<int> &displs);

  void applyPattern(const AlltoallvPattern &pattern);
  void initBuffers(const AlltoallvPattern &pattern);
  void releaseBuffers();
  void initSendData();
  bool patternIsEqualBlocks(const AlltoallvPattern &pattern) const;
  void enqueueReferenceAlltoall(std::queue<EmberEvent *> &evQ);
  bool compareResults();

  EmberRoutedAlltoallv *m_alltoall;
  AlltoallvPattern m_pattern;
  std::map<std::pair<int, int>, int> m_route_table_map;
  std::map<std::pair<int, int>, int> m_ecmp_path_counts;
  bool m_ecmp_mode;
  std::mt19937 m_ecmp_rng;

  bool m_validate;
  bool m_validation_ref_executed;
  void *m_ref_recvBuf;
  int m_buf_elems;

  VolumeMode m_mode;
  uint32_t m_iterations;
  uint32_t m_loopIndex;
  bool m_iterationStarted;
  uint64_t m_lastBytesSent;
  uint64_t m_compute;
  uint64_t m_startTime;
  uint64_t m_stopTime;

  int m_count;
  int m_total_count;
  int m_min_count;
  int m_max_count;
  uint32_t m_seed;
  bool m_regenerate_random;

  std::string m_send_counts_s;
  std::string m_recv_counts_s;
  std::string m_counts_file;
  std::string m_recv_counts_file;

  void *m_sendBuf;
  void *m_recvBuf;
  uint32_t m_tag;
};

} // namespace Ember
} // namespace SST

#endif
