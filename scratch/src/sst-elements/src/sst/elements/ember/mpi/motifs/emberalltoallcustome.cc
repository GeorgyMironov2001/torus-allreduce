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

#include "emberalltoallcustome.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>

#include <sst_config.h>

using namespace SST::Ember;

namespace {

size_t elemSize() { return sizeof(float); }

bool parseRouterPairKey(const std::string &key, int &src, int &dst) {
  size_t comma = key.find(',');
  if (comma == std::string::npos) {
    return false;
  }
  try {
    src = std::stoi(key.substr(0, comma));
    dst = std::stoi(key.substr(comma + 1));
  } catch (const std::exception &) {
    return false;
  }
  return true;
}

int countEcmpPaths(const nlohmann::json &value) {
  if (!value.is_array() || value.empty()) {
    return 0;
  }
  if (value.front().is_number()) {
    return 1;
  }
  return static_cast<int>(value.size());
}

bool looksLikeEcmpPaths(const nlohmann::json &j) {
  if (!j.is_object() || j.empty()) {
    return false;
  }
  for (const auto &entry : j.items()) {
    int src = -1;
    int dst = -1;
    if (!parseRouterPairKey(entry.key(), src, dst)) {
      return false;
    }
    if (!entry.value().is_array()) {
      return false;
    }
  }
  return true;
}

std::string readRouteTableJsonText(const std::string &route_table_file) {
  std::ifstream file(route_table_file);
  if (!file.is_open()) {
    return "";
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  std::string first_line;
  {
    std::istringstream iss(content);
    std::getline(iss, first_line);
    first_line.erase(0, first_line.find_first_not_of(" \t\r\n"));
    first_line.erase(first_line.find_last_not_of(" \t\r\n") + 1);
  }

  if (first_line == "ecmp" || first_line == "ECMP") {
    size_t newline = content.find('\n');
    if (newline == std::string::npos) {
      return "";
    }
    return content.substr(newline + 1);
  }
  return content;
}

} // namespace

EmberAlltoallCustomeGenerator::EmberRoutedAlltoallv::EmberRoutedAlltoallv(
    EmberAlltoallCustomeGenerator &gen, void *sendBuf, void *recvBuf,
    const AlltoallvPattern &pattern)
    : m_gen(gen), m_sendBuf(sendBuf), m_recvBuf(recvBuf), m_r(gen.rank()),
      m_p(gen.size()), m_comm(GroupWorld), m_pat(pattern), m_tag(0), m_step(0),
      m_req_count(0), m_reqs(NULL), m_bytes_sent(0) {
  m_tag = m_gen.getNextTag();
  m_reqs = (MessageRequest *)malloc(sizeof(MessageRequest) * 2 * m_p);
}

EmberAlltoallCustomeGenerator::EmberRoutedAlltoallv::~EmberRoutedAlltoallv() {
  free(m_reqs);
}

void EmberAlltoallCustomeGenerator::EmberRoutedAlltoallv::reset() {
  m_step = 0;
  m_req_count = 0;
  m_bytes_sent = 0;
  m_tag = m_gen.getNextTag();
}

int EmberAlltoallCustomeGenerator::getRouteId(int dst) {
  if (m_ecmp_mode) {
    auto it = m_ecmp_path_counts.find(std::make_pair(rank(), dst));
    if (it == m_ecmp_path_counts.end() || it->second <= 0) {
      return -1;
    }
    std::uniform_int_distribution<int> dist(0, it->second - 1);
    return dist(m_ecmp_rng);
  }

  auto it = m_route_table_map.find(std::make_pair(rank(), dst));
  if (it != m_route_table_map.end()) {
    return it->second;
  }
  return -1;
}

void *
EmberAlltoallCustomeGenerator::EmberRoutedAlltoallv::sendPtr(int displ) const {
  if (!m_sendBuf) {
    return NULL;
  }
  return (char *)m_sendBuf + displ * elemSize();
}

void *
EmberAlltoallCustomeGenerator::EmberRoutedAlltoallv::recvPtr(int displ) const {
  if (!m_recvBuf) {
    return NULL;
  }
  return (char *)m_recvBuf + displ * elemSize();
}

bool EmberAlltoallCustomeGenerator::EmberRoutedAlltoallv::progress(
    std::queue<EmberEvent *> &evQ) {
  if (m_step == 0) {
    m_gen.queueIterationStart(evQ);
    m_req_count = 0;
    m_bytes_sent = 0;

    for (int src = 0; src < m_p; ++src) {
      if (src == m_r || m_pat.recv_counts[src] == 0) {
        continue;
      }
      m_gen.enQ_irecv(evQ, recvPtr(m_pat.recv_displs[src]),
                      m_pat.recv_counts[src], FLOAT, src, m_tag, m_comm,
                      &m_reqs[m_req_count++]);
    }

    for (int dst = 0; dst < m_p; ++dst) {
      if (dst == m_r) {
        if (m_sendBuf && m_recvBuf && m_pat.send_counts[dst] > 0) {
          std::memcpy(recvPtr(m_pat.recv_displs[dst]),
                      sendPtr(m_pat.send_displs[dst]),
                      m_pat.send_counts[dst] * elemSize());
        }
        continue;
      }
      if (m_pat.send_counts[dst] == 0) {
        continue;
      }
      m_bytes_sent += m_pat.send_counts[dst] * elemSize();
      m_gen.enQ_isend(evQ, sendPtr(m_pat.send_displs[dst]),
                      m_pat.send_counts[dst], FLOAT, dst, m_tag, m_comm,
                      &m_reqs[m_req_count++], m_gen.getRouteId(dst));
    }

    m_step = 1;
    return false;
  }

  if (m_step == 1) {
    if (m_req_count > 0) {
      m_gen.enQ_waitall(evQ, m_req_count, m_reqs, NULL);
      m_step = 2;
      return false;
    }
    m_step = 2;
    return true;
  }

  if (m_step == 2) {
    return true;
  }

  return true;
}

std::pair<std::vector<std::vector<int>>, std::map<std::pair<int, int>, int>>
EmberAlltoallCustomeGenerator::parseRouteTable(
    const std::string &route_table_file) {
  std::vector<std::vector<int>> route_table;
  std::map<std::pair<int, int>, int> route_table_map;

  std::ifstream file(route_table_file);
  if (!file.is_open()) {
    return std::make_pair(route_table, route_table_map);
  }

  nlohmann::json j;
  file >> j;
  int id = 0;
  for (const auto &entry : j) {
    std::pair<int, int> key = std::make_pair(entry[0][0], entry[0][1]);
    route_table_map[key] = id++;
    std::vector<int> path;
    for (int node : entry[1]) {
      path.push_back(node);
    }
    route_table.push_back(path);
  }
  return std::make_pair(route_table, route_table_map);
}

std::map<std::pair<int, int>, int>
EmberAlltoallCustomeGenerator::parseEcmpPathCounts(
    const std::string &route_table_file) {
  std::map<std::pair<int, int>, int> path_counts;

  std::string json_text = readRouteTableJsonText(route_table_file);
  if (json_text.empty()) {
    return path_counts;
  }

  nlohmann::json j;
  try {
    j = nlohmann::json::parse(json_text);
  } catch (const nlohmann::json::exception &) {
    return path_counts;
  }

  const nlohmann::json *paths_obj = &j;
  if (j.is_object() && j.contains("all_shortest_paths")) {
    paths_obj = &j["all_shortest_paths"];
  }

  if (!paths_obj->is_object()) {
    return path_counts;
  }

  for (const auto &entry : paths_obj->items()) {
    int src = -1;
    int dst = -1;
    if (!parseRouterPairKey(entry.key(), src, dst)) {
      continue;
    }
    int path_count = countEcmpPaths(entry.value());
    if (path_count > 0) {
      path_counts[std::make_pair(src, dst)] = path_count;
    }
  }
  return path_counts;
}

EmberAlltoallCustomeGenerator::VolumeMode
EmberAlltoallCustomeGenerator::parseMode(const std::string &mode) const {
  if (mode == "fixed") {
    return FIXED;
  }
  if (mode == "fixed_file") {
    return FIXED_FILE;
  }
  if (mode == "random") {
    return RANDOM;
  }
  return EQUAL;
}

std::vector<int>
EmberAlltoallCustomeGenerator::parseCsvInts(const std::string &csv,
                                            int expected) {
  std::vector<int> values;
  std::stringstream ss(csv);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (token.empty()) {
      continue;
    }
    values.push_back(std::stoi(token));
  }
  if (expected > 0 && static_cast<int>(values.size()) != expected) {
    fprintf(stderr, "expected %d counts, got %zu in '%s'\n", expected,
            values.size(), csv.c_str());
  }
  return values;
}

void EmberAlltoallCustomeGenerator::buildDisplacements(
    const std::vector<int> &counts, std::vector<int> &displs) {
  displs.resize(counts.size());
  int offset = 0;
  for (size_t i = 0; i < counts.size(); ++i) {
    displs[i] = offset;
    offset += counts[i];
  }
}

AlltoallvPattern EmberAlltoallCustomeGenerator::buildEqualPattern() {
  AlltoallvPattern pat;
  int p = size();
  int block = m_count;
  pat.send_counts.assign(p, block);
  pat.recv_counts = pat.send_counts;
  buildDisplacements(pat.send_counts, pat.send_displs);
  buildDisplacements(pat.recv_counts, pat.recv_displs);
  return pat;
}

AlltoallvPattern EmberAlltoallCustomeGenerator::buildFixedPattern(
    const std::string &send_counts_s, const std::string &recv_counts_s) {
  AlltoallvPattern pat;
  pat.send_counts = parseCsvInts(send_counts_s, size());
  if (recv_counts_s.empty()) {
    pat.recv_counts = pat.send_counts;
  } else {
    pat.recv_counts = parseCsvInts(recv_counts_s, size());
  }
  buildDisplacements(pat.send_counts, pat.send_displs);
  buildDisplacements(pat.recv_counts, pat.recv_displs);
  return pat;
}

static std::string readRankCountsLine(const std::string &counts_file, int rank) {
  std::ifstream fp(counts_file.c_str());
  if (!fp.is_open()) {
    return "";
  }

  std::string line;
  int target_rank = 0;
  while (std::getline(fp, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    if (target_rank == rank) {
      return line;
    }
    ++target_rank;
  }
  return "";
}

AlltoallvPattern EmberAlltoallCustomeGenerator::buildFixedFilePattern(
    const std::string &counts_file, const std::string &recv_counts_file) {
  std::string send_line = readRankCountsLine(counts_file, rank());
  if (send_line.empty()) {
    fprintf(stderr, "[%d] counts_file %s has no line for rank %d\n", rank(),
            counts_file.c_str(), rank());
    return buildEqualPattern();
  }

  std::string recv_line;
  if (!recv_counts_file.empty()) {
    recv_line = readRankCountsLine(recv_counts_file, rank());
    if (recv_line.empty()) {
      fprintf(stderr,
              "[%d] recv_counts_file %s has no line for rank %d, using send "
              "counts\n",
              rank(), recv_counts_file.c_str(), rank());
    }
  }
  return buildFixedPattern(send_line, recv_line);
}

AlltoallvPattern EmberAlltoallCustomeGenerator::buildRandomPattern() {
  AlltoallvPattern pat;
  int p = size();
  pat.send_counts.assign(p, 0);

  std::mt19937 rng(m_seed + rank());

  int remaining = m_total_count;
  for (int dst = 0; dst < p - 1; ++dst) {
    int max_here =
        std::min(m_max_count, remaining - m_min_count * (p - dst - 1));
    int min_here = m_min_count;
    if (max_here < min_here) {
      max_here = min_here;
    }
    std::uniform_int_distribution<int> bounded(min_here, max_here);
    pat.send_counts[dst] = bounded(rng);
    remaining -= pat.send_counts[dst];
  }
  pat.send_counts[p - 1] = std::max(0, remaining);

  pat.recv_counts = pat.send_counts;
  buildDisplacements(pat.send_counts, pat.send_displs);
  buildDisplacements(pat.recv_counts, pat.recv_displs);
  return pat;
}

AlltoallvPattern EmberAlltoallCustomeGenerator::buildPattern() {
  switch (m_mode) {
  case FIXED:
    return buildFixedPattern(m_send_counts_s, m_recv_counts_s);
  case FIXED_FILE:
    return buildFixedFilePattern(m_counts_file, m_recv_counts_file);
  case RANDOM:
    return buildRandomPattern();
  case EQUAL:
  default:
    return buildEqualPattern();
  }
}

int patternTotalElems(const std::vector<int> &counts,
                      const std::vector<int> &displs) {
  if (counts.empty()) {
    return 0;
  }
  return displs.back() + counts.back();
}

void EmberAlltoallCustomeGenerator::initSendData() {
  float *send = (float *)m_sendBuf;
  for (int dst = 0; dst < size(); ++dst) {
    for (int k = 0; k < m_pattern.send_counts[dst]; ++k) {
      int idx = m_pattern.send_displs[dst] + k;
      send[idx] = (float)(rank() * 1000000 + dst * 1000 + k);
    }
  }
}

void EmberAlltoallCustomeGenerator::initBuffers(
    const AlltoallvPattern &pattern) {
  int send_elems = patternTotalElems(pattern.send_counts, pattern.send_displs);
  int recv_elems = patternTotalElems(pattern.recv_counts, pattern.recv_displs);
  m_buf_elems = std::max(send_elems, recv_elems);

  if (m_sendBuf) {
    memFree(m_sendBuf);
    m_sendBuf = NULL;
  }
  if (m_recvBuf) {
    memFree(m_recvBuf);
    m_recvBuf = NULL;
  }
  if (m_ref_recvBuf) {
    memFree(m_ref_recvBuf);
    m_ref_recvBuf = NULL;
  }

  m_sendBuf = memAlloc(m_buf_elems * elemSize());
  m_recvBuf = memAlloc(m_buf_elems * elemSize());
  m_ref_recvBuf = memAlloc(m_buf_elems * elemSize());
  initSendData();
}

void EmberAlltoallCustomeGenerator::releaseBuffers() {
  if (m_sendBuf) {
    memFree(m_sendBuf);
    m_sendBuf = NULL;
  }
  if (m_recvBuf) {
    memFree(m_recvBuf);
    m_recvBuf = NULL;
  }
  if (m_ref_recvBuf) {
    memFree(m_ref_recvBuf);
    m_ref_recvBuf = NULL;
  }
  m_buf_elems = 0;
}

void EmberAlltoallCustomeGenerator::applyPattern(
    const AlltoallvPattern &pattern) {
  m_pattern = pattern;
  delete m_alltoall;
  m_alltoall = NULL;

  if (m_validate) {
    initBuffers(pattern);
  } else {
    releaseBuffers();
  }

  m_alltoall = new EmberRoutedAlltoallv(*this, m_sendBuf, m_recvBuf, m_pattern);
}

bool EmberAlltoallCustomeGenerator::patternIsEqualBlocks(
    const AlltoallvPattern &pattern) const {
  if (pattern.send_counts.empty()) {
    return true;
  }
  int block = pattern.send_counts[0];
  for (int c : pattern.send_counts) {
    if (c != block) {
      return false;
    }
  }
  for (int c : pattern.recv_counts) {
    if (c != block) {
      return false;
    }
  }
  return true;
}

void EmberAlltoallCustomeGenerator::enqueueReferenceAlltoall(
    std::queue<EmberEvent *> &evQ) {
  if (patternIsEqualBlocks(m_pattern) && !m_pattern.send_counts.empty()) {
    int block = m_pattern.send_counts[0];
    enQ_alltoall(evQ, m_sendBuf, block, FLOAT, m_ref_recvBuf, block, FLOAT,
                 GroupWorld);
    return;
  }

  enQ_alltoallv(evQ, m_sendBuf, &m_pattern.send_counts[0],
                &m_pattern.send_displs[0], FLOAT, m_ref_recvBuf,
                &m_pattern.recv_counts[0], &m_pattern.recv_displs[0], FLOAT,
                GroupWorld);
}

bool EmberAlltoallCustomeGenerator::compareResults() {
  float *custom = (float *)m_recvBuf;
  float *ref = (float *)m_ref_recvBuf;
  bool valid = true;

  for (int src = 0; src < size(); ++src) {
    for (int k = 0; k < m_pattern.recv_counts[src]; ++k) {
      int idx = m_pattern.recv_displs[src] + k;
      float expected = (float)(src * 1000000 + rank() * 1000 + k);
      if (custom[idx] != ref[idx]) {
        fprintf(stderr,
                "Validation error on rank %d from src %d at offset %d: "
                "custom=%f ref=%f expected=%f\n",
                rank(), src, idx, custom[idx], ref[idx], expected);
        valid = false;
      }
    }
  }

  if (valid) {
    printf("[Rank %d] Alltoall validation succeeded.\n", rank());
  } else {
    printf("[Rank %d] Alltoall validation FAILED.\n", rank());
  }
  fflush(stdout);
  return valid;
}

EmberAlltoallCustomeGenerator::EmberAlltoallCustomeGenerator(
    SST::ComponentId_t id, Params &params)
    : EmberMessagePassingGenerator(id, params, "AlltoallCustome"),
      m_alltoall(NULL), m_validate(false), m_validation_ref_executed(false),
      m_ref_recvBuf(NULL), m_buf_elems(0), m_loopIndex(0),
      m_iterationStarted(false), m_lastBytesSent(0), m_startTime(0),
      m_stopTime(0), m_sendBuf(NULL), m_recvBuf(NULL), m_tag(UINT_MAX),
      m_ecmp_mode(false), m_ecmp_rng(1) {
  m_iterations = (uint32_t)params.find("arg.iterations", 1);
  m_compute = (uint64_t)params.find("arg.compute", 0);
  m_count = (int)params.find("arg.count", 1);
  m_total_count = (int)params.find("arg.total_count", m_count);
  m_min_count = (int)params.find("arg.min_count", 1);
  m_max_count = (int)params.find("arg.max_count", 1);
  m_seed = (uint32_t)params.find("arg.seed", 1);
  m_regenerate_random = (bool)params.find("arg.regenerate_random", false);
  m_validate = (bool)params.find("arg.validate", 0);

  if (m_validate) {
    memSetBacked();
  }

  m_mode = parseMode(params.find<std::string>("arg.mode", "equal"));
  m_send_counts_s = params.find<std::string>("arg.send_counts", "");
  m_recv_counts_s = params.find<std::string>("arg.recv_counts", "");
  m_counts_file = params.find<std::string>("arg.counts_file", "");
  m_recv_counts_file = params.find<std::string>("arg.recv_counts_file", "");

  std::string route_table_file =
      params.find<std::string>("arg.route_table_file", "");
  if (!route_table_file.empty()) {
    std::string json_text = readRouteTableJsonText(route_table_file);
    nlohmann::json j;
    try {
      j = nlohmann::json::parse(json_text);
    } catch (const nlohmann::json::exception &) {
      j = nlohmann::json();
    }

    if (j.is_array()) {
      m_route_table_map = parseRouteTable(route_table_file).second;
    } else if (looksLikeEcmpPaths(j) ||
               (j.is_object() && j.contains("all_shortest_paths"))) {
      m_ecmp_mode = true;
      m_ecmp_path_counts = parseEcmpPathCounts(route_table_file);
    } else if (j.is_object() && !j.empty()) {
      fprintf(stderr,
              "[%d] route_table_file %s: unsupported object format\n",
              rank(), route_table_file.c_str());
    }
  }

  m_ecmp_rng.seed(m_seed + rank());

  applyPattern(buildPattern());
}

EmberAlltoallCustomeGenerator::~EmberAlltoallCustomeGenerator() {
  delete m_alltoall;
  releaseBuffers();
}

void EmberAlltoallCustomeGenerator::queueIterationStart(
    std::queue<EmberEvent *> &evQ) {
  if (m_iterationStarted) {
    return;
  }
  enQ_getTime(evQ, &m_startTime);
  enQ_compute(evQ, m_compute);
  m_iterationStarted = true;
}

void EmberAlltoallCustomeGenerator::completeIteration(
    std::queue<EmberEvent *> &evQ) {
  m_lastBytesSent = m_alltoall->bytesSent();
  if (++m_loopIndex == m_iterations) {
    enQ_getTime(evQ, &m_stopTime);
  } else {
    m_iterationStarted = false;
  }
  m_alltoall->reset();
}

void EmberAlltoallCustomeGenerator::printStats() {
  uint64_t rank_time = m_stopTime - m_startTime;
  uint64_t bytes = m_lastBytesSent;
  double bw = rank_time > 0 ? (double)8 * bytes / rank_time : 0.0;
  double gbw = bw * size();
  printf("TIME %d start_time %" PRIu64 " stop_time %" PRIu64
         " rank_time %" PRIu64 " bytes_sent %" PRIu64 " bw %lf gbw %lf\n",
         rank(), m_startTime, m_stopTime, rank_time, bytes, bw, gbw);
  fflush(stdout);
}

bool EmberAlltoallCustomeGenerator::generate(std::queue<EmberEvent *> &evQ) {
  if (m_loopIndex == m_iterations) {
    printStats();
    return true;
  }

  if (m_loopIndex > 0 && m_mode == RANDOM && m_regenerate_random) {
    applyPattern(buildPattern());
    m_validation_ref_executed = false;
  }

  if (m_validation_ref_executed) {
    compareResults();
    m_validation_ref_executed = false;
    completeIteration(evQ);
    return false;
  }

  if (!m_alltoall->progress(evQ)) {
    return false;
  }

  if (m_validate) {
    enqueueReferenceAlltoall(evQ);
    m_validation_ref_executed = true;
    return false;
  }
  
  completeIteration(evQ);
  return false;
}
