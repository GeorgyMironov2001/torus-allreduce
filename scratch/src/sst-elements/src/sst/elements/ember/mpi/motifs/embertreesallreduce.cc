#include "embertreesallreduce.h"
#include "embershortmsgcheck.h"
#include "embertrees/emberbdmstrees_128_8d68.h"
#include "embertrees/emberbdmstrees_16_16d4.h"
#include "embertrees/emberbdmstrees_32_32d1.h"
#include "embertrees/emberbdmstrees_32_32d2.h"
#include "embertrees/emberbdmstrees_32_32d3.h"
#include "embertrees/emberbdmstrees_32_32d4.h"
#include "embertrees/emberbdmstrees_32_32d5.h"
#include "embertrees/emberbdmstrees_32_32d6.h"
#include "embertrees/emberbdmstrees_32_32d7.h"
#include "embertrees/emberbdmstrees_32_32d8.h"
#include "embertrees/emberbdmstrees_32_32d9.h"
#include "embertrees/emberbdmstrees_32_32d10.h"
#include "embertrees/emberbdmstrees_32_32d11.h"
#include "embertrees/emberbdmstrees_32_32d12.h"
#include "embertrees/emberbdmstrees_32_32d13.h"
#include "embertrees/emberbdmstrees_32_32d14.h"
#include "embertrees/emberbdmstrees_32_32d15.h"
#include "embertrees/emberbdmstrees_32_32d16.h"
#include "embertrees/emberbdmstrees_32_32d17.h"
#include "embertrees/emberbdmstrees_32_32d18.h"
#include "embertrees/emberbdmstrees_32_32d19.h"
#include "embertrees/emberbdmstrees_32_32d20.h"
#include "embertrees/emberbdmstrees_32_32d21.h"
#include "embertrees/emberbdmstrees_32_32d22.h"
#include "embertrees/emberbdmstrees_32_32d23.h"
#include "embertrees/emberbdmstrees_32_32d24.h"
#include "embertrees/emberbdmstrees_32_32d25.h"
#include "embertrees/emberbdmstrees_32_32d26.h"
#include "embertrees/emberbdmstrees_32_32d27.h"
#include "embertrees/emberbdmstrees_32_32d28.h"
#include "embertrees/emberbdmstrees_32_32d29.h"
#include "embertrees/emberbdmstrees_32_32d30.h"
#include "embertrees/emberbdmstrees_32_32d31.h"
#include "embertrees/emberbdmstrees_32_32d32.h"
#include "embertrees/emberbdmstrees_256_4d130.h"
#include "embertrees/emberbdmstrees_64_16d40.h"
#include "embertrees/emberbdmstrees_64_64d1.h"
#include "embertrees/emberbdmstrees_64_64d2.h"
#include "embertrees/emberbdmstrees_64_64d3.h"
#include "embertrees/emberbdmstrees_64_64d4.h"
#include "embertrees/emberbdmstrees_64_64d5.h"
#include "embertrees/emberbdmstrees_64_64d6.h"
#include "embertrees/emberbdmstrees_64_64d7.h"
#include "embertrees/emberbdmstrees_64_64d8.h"
#include "embertrees/emberbdmstrees_64_64d9.h"
#include "embertrees/emberbdmstrees_64_64d10.h"
#include "embertrees/emberbdmstrees_64_64d11.h"
#include "embertrees/emberbdmstrees_64_64d12.h"
#include "embertrees/emberbdmstrees_64_64d13.h"
#include "embertrees/emberbdmstrees_64_64d14.h"
#include "embertrees/emberbdmstrees_64_64d15.h"
#include "embertrees/emberbdmstrees_64_64d16.h"
#include "embertrees/emberbdmstrees_64_64d17.h"
#include "embertrees/emberbdmstrees_64_64d18.h"
#include "embertrees/emberbdmstrees_64_64d19.h"
#include "embertrees/emberbdmstrees_64_64d20.h"
#include "embertrees/emberbdmstrees_64_64d21.h"
#include "embertrees/emberbdmstrees_64_64d22.h"
#include "embertrees/emberbdmstrees_64_64d23.h"
#include "embertrees/emberbdmstrees_64_64d24.h"
#include "embertrees/emberbdmstrees_64_64d25.h"
#include "embertrees/emberbdmstrees_64_64d26.h"
#include "embertrees/emberbdmstrees_64_64d27.h"
#include "embertrees/emberbdmstrees_64_64d28.h"
#include "embertrees/emberbdmstrees_64_64d29.h"
#include "embertrees/emberbdmstrees_64_64d30.h"
#include "embertrees/emberbdmstrees_64_64d31.h"
#include "embertrees/emberbdmstrees_64_64d32.h"
#include "embertrees/emberbdmstrees_64_64d33.h"
#include "embertrees/emberbdmstrees_64_64d34.h"
#include "embertrees/emberbdmstrees_64_64d35.h"
#include "embertrees/emberbdmstrees_64_64d36.h"
#include "embertrees/emberbdmstrees_64_64d37.h"
#include "embertrees/emberbdmstrees_64_64d38.h"
#include "embertrees/emberbdmstrees_64_64d39.h"
#include "embertrees/emberbdmstrees_64_64d40.h"
#include "embertrees/emberbdmstrees_64_64d41.h"
#include "embertrees/emberbdmstrees_64_64d42.h"
#include "embertrees/emberbdmstrees_64_64d43.h"
#include "embertrees/emberbdmstrees_64_64d44.h"
#include "embertrees/emberbdmstrees_64_64d45.h"
#include "embertrees/emberbdmstrees_64_64d46.h"
#include "embertrees/emberbdmstrees_64_64d47.h"
#include "embertrees/emberbdmstrees_64_64d48.h"
#include "embertrees/emberbdmstrees_64_64d49.h"
#include "embertrees/emberbdmstrees_64_64d50.h"
#include "embertrees/emberbdmstrees_64_64d51.h"
#include "embertrees/emberbdmstrees_64_64d52.h"
#include "embertrees/emberbdmstrees_64_64d53.h"
#include "embertrees/emberbdmstrees_64_64d54.h"
#include "embertrees/emberbdmstrees_64_64d55.h"
#include "embertrees/emberbdmstrees_64_64d56.h"
#include "embertrees/emberbdmstrees_64_64d57.h"
#include "embertrees/emberbdmstrees_64_64d58.h"
#include "embertrees/emberbdmstrees_64_64d59.h"
#include "embertrees/emberbdmstrees_64_64d60.h"
#include "embertrees/emberbdmstrees_64_64d61.h"
#include "embertrees/emberbdmstrees_64_64d62.h"
#include "embertrees/emberbdmstrees_64_64d63.h"
#include "embertrees/emberbdmstrees_64_64d64.h"
#include "embertrees/emberbdmstrees_8_4d4.h"
#include "embertrees/emberbdmstrees_8_8d1.h"
#include "embertrees/emberbdmstrees_8_8d2.h"
#include "embertrees/emberbdmstrees_8_8d3.h"
#include "embertrees/emberbdmstrees_8_8d4.h"
#include "embertrees/emberbdmstrees_8_8d5.h"
#include "embertrees/emberbdmstrees_8_8d6.h"
#include "embertrees/emberbdmstrees_8_8d7.h"
#include "embertrees/emberbdmstrees_8_8d8.h"
#include "embertrees/emberbdmstrees_hyperx_32_32_budget_b16.h"
#include "embertrees/emberbdmstrees_hyperx_32_32_budget_b32.h"
#include "embertrees/emberbdmstrees_hyperx_32_32_budget_b4.h"
#include "embertrees/emberbdmstrees_hyperx_32_32_budget_b64.h"
#include "embertrees/emberbdmstrees_hyperx_32_32_budget_b8.h"
#include "embertrees/emberbdmstrees_hyperx_64_64_budget_b16.h"
#include "embertrees/emberbdmstrees_hyperx_8_8_2.h"
#include "embertrees/embercycleopt_16_16_4.h"
#include "embertrees/embercycleopt_16_16_6.h"
#include "embertrees/embercycleopt_16_16_8.h"
#include "embertrees/embercycleopt_8_8_4.h"
#include "embertrees/embercycleopt_8_8_6.h"
#include "embertrees/embertrees_64_64.h"
#include "embertrees/embertrees_8_8_8.h"
#include "embertrees/embertrees_small_cases.h"
#include "embertreescoll.h"
#include <cinttypes>
#include <sst_config.h>
using namespace SST::Ember;

namespace {
const std::vector<TreeSpec> *const kTorus64x64TreePacks[] = {
    &bdms_tree_specs_bdms_64x64_1_quadruple,
    &bdms_tree_specs_bdms_64x64_2_quadruple,
    &bdms_tree_specs_bdms_64x64_3_quadruple,
    &bdms_tree_specs_bdms_64x64_4_quadruple,
    &bdms_tree_specs_bdms_64x64_5_quadruple,
    &bdms_tree_specs_bdms_64x64_6_quadruple,
    &bdms_tree_specs_bdms_64x64_7_quadruple,
    &bdms_tree_specs_bdms_64x64_8_quadruple,
    &bdms_tree_specs_bdms_64x64_9_quadruple,
    &bdms_tree_specs_bdms_64x64_10_quadruple,
    &bdms_tree_specs_bdms_64x64_11_quadruple,
    &bdms_tree_specs_bdms_64x64_12_quadruple,
    &bdms_tree_specs_bdms_64x64_13_quadruple,
    &bdms_tree_specs_bdms_64x64_14_quadruple,
    &bdms_tree_specs_bdms_64x64_15_quadruple,
    &bdms_tree_specs_bdms_64x64_16_quadruple,
    &bdms_tree_specs_bdms_64x64_17_quadruple,
    &bdms_tree_specs_bdms_64x64_18_quadruple,
    &bdms_tree_specs_bdms_64x64_19_quadruple,
    &bdms_tree_specs_bdms_64x64_20_quadruple,
    &bdms_tree_specs_bdms_64x64_21_quadruple,
    &bdms_tree_specs_bdms_64x64_22_quadruple,
    &bdms_tree_specs_bdms_64x64_23_quadruple,
    &bdms_tree_specs_bdms_64x64_24_quadruple,
    &bdms_tree_specs_bdms_64x64_25_quadruple,
    &bdms_tree_specs_bdms_64x64_26_quadruple,
    &bdms_tree_specs_bdms_64x64_27_quadruple,
    &bdms_tree_specs_bdms_64x64_28_quadruple,
    &bdms_tree_specs_bdms_64x64_29_quadruple,
    &bdms_tree_specs_bdms_64x64_30_quadruple,
    &bdms_tree_specs_bdms_64x64_31_quadruple,
    &bdms_tree_specs_bdms_64x64_32_quadruple,
    &bdms_tree_specs_bdms_64x64_33_quadruple,
    &bdms_tree_specs_bdms_64x64_34_quadruple,
    &bdms_tree_specs_bdms_64x64_35_quadruple,
    &bdms_tree_specs_bdms_64x64_36_quadruple,
    &bdms_tree_specs_bdms_64x64_37_quadruple,
    &bdms_tree_specs_bdms_64x64_38_quadruple,
    &bdms_tree_specs_bdms_64x64_39_quadruple,
    &bdms_tree_specs_bdms_64x64_40_quadruple,
    &bdms_tree_specs_bdms_64x64_41_quadruple,
    &bdms_tree_specs_bdms_64x64_42_quadruple,
    &bdms_tree_specs_bdms_64x64_43_quadruple,
    &bdms_tree_specs_bdms_64x64_44_quadruple,
    &bdms_tree_specs_bdms_64x64_45_quadruple,
    &bdms_tree_specs_bdms_64x64_46_quadruple,
    &bdms_tree_specs_bdms_64x64_47_quadruple,
    &bdms_tree_specs_bdms_64x64_48_quadruple,
    &bdms_tree_specs_bdms_64x64_49_quadruple,
    &bdms_tree_specs_bdms_64x64_50_quadruple,
    &bdms_tree_specs_bdms_64x64_51_quadruple,
    &bdms_tree_specs_bdms_64x64_52_quadruple,
    &bdms_tree_specs_bdms_64x64_53_quadruple,
    &bdms_tree_specs_bdms_64x64_54_quadruple,
    &bdms_tree_specs_bdms_64x64_55_quadruple,
    &bdms_tree_specs_bdms_64x64_56_quadruple,
    &bdms_tree_specs_bdms_64x64_57_quadruple,
    &bdms_tree_specs_bdms_64x64_58_quadruple,
    &bdms_tree_specs_bdms_64x64_59_quadruple,
    &bdms_tree_specs_bdms_64x64_60_quadruple,
    &bdms_tree_specs_bdms_64x64_61_quadruple,
    &bdms_tree_specs_bdms_64x64_62_quadruple,
    &bdms_tree_specs_bdms_64x64_63_quadruple,
    &bdms_tree_specs_bdms_64x64_64_quadruple,
};
constexpr int kTorus64x64TreePackCount =
    sizeof(kTorus64x64TreePacks) / sizeof(kTorus64x64TreePacks[0]);

// tree_set 0 -> d1 ... tree_set 31 -> d32.
const std::vector<TreeSpec> *const kTorus32x32TreePacks[] = {
    &bdms_tree_specs_bdms_32x32_1_quadruple,
    &bdms_tree_specs_bdms_32x32_2_quadruple,
    &bdms_tree_specs_bdms_32x32_3_quadruple,
    &bdms_tree_specs_bdms_32x32_4_quadruple,
    &bdms_tree_specs_bdms_32x32_5_quadruple,
    &bdms_tree_specs_bdms_32x32_6_quadruple,
    &bdms_tree_specs_bdms_32x32_7_quadruple,
    &bdms_tree_specs_bdms_32x32_8_quadruple,
    &bdms_tree_specs_bdms_32x32_9_quadruple,
    &bdms_tree_specs_bdms_32x32_10_quadruple,
    &bdms_tree_specs_bdms_32x32_11_quadruple,
    &bdms_tree_specs_bdms_32x32_12_quadruple,
    &bdms_tree_specs_bdms_32x32_13_quadruple,
    &bdms_tree_specs_bdms_32x32_14_quadruple,
    &bdms_tree_specs_bdms_32x32_15_quadruple,
    &bdms_tree_specs_bdms_32x32_16_quadruple,
    &bdms_tree_specs_bdms_32x32_17_quadruple,
    &bdms_tree_specs_bdms_32x32_18_quadruple,
    &bdms_tree_specs_bdms_32x32_19_quadruple,
    &bdms_tree_specs_bdms_32x32_20_quadruple,
    &bdms_tree_specs_bdms_32x32_21_quadruple,
    &bdms_tree_specs_bdms_32x32_22_quadruple,
    &bdms_tree_specs_bdms_32x32_23_quadruple,
    &bdms_tree_specs_bdms_32x32_24_quadruple,
    &bdms_tree_specs_bdms_32x32_25_quadruple,
    &bdms_tree_specs_bdms_32x32_26_quadruple,
    &bdms_tree_specs_bdms_32x32_27_quadruple,
    &bdms_tree_specs_bdms_32x32_28_quadruple,
    &bdms_tree_specs_bdms_32x32_29_quadruple,
    &bdms_tree_specs_bdms_32x32_30_quadruple,
    &bdms_tree_specs_bdms_32x32_31_quadruple,
    &bdms_tree_specs_bdms_32x32_32_quadruple,
};
constexpr int kTorus32x32TreePackCount =
    sizeof(kTorus32x32TreePacks) / sizeof(kTorus32x32TreePacks[0]);
} // namespace

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
  std::string tree_pack = params.find<std::string>("arg.tree_pack", "default");
  int tree_set = (int)params.find("arg.tree_set", 0);

  const std::vector<SST::Ember::TreeSpec> *selected_pack = nullptr;
  const char *selected_name = nullptr;
  std::string selected_name_storage;
  if (tree_pack == "torus" && dimensions == 2 && dimensions_sizes != nullptr &&
      dimensions_sizes[0] == 8 && dimensions_sizes[1] == 8) {
    switch (tree_set) {
    case 0:
      selected_pack = &bdms_tree_specs_bdms_8x8_1_quadruple;
      selected_name = "torus_8x8_d1";
      break;
    case 1:
      selected_pack = &bdms_tree_specs_bdms_8x8_2_quadruple;
      selected_name = "torus_8x8_d2";
      break;
    case 2:
      selected_pack = &bdms_tree_specs_bdms_8x8_3_quadruple;
      selected_name = "torus_8x8_d3";
      break;
    case 3:
      selected_pack = &bdms_tree_specs_bdms_8x8_4_quadruple;
      selected_name = "torus_8x8_d4";
      break;
    case 4:
      selected_pack = &bdms_tree_specs_bdms_8x8_5_quadruple;
      selected_name = "torus_8x8_d5";
      break;
    case 5:
      selected_pack = &bdms_tree_specs_bdms_8x8_6_quadruple;
      selected_name = "torus_8x8_d6";
      break;
    case 6:
      selected_pack = &bdms_tree_specs_bdms_8x8_7_quadruple;
      selected_name = "torus_8x8_d7";
      break;
    case 7:
      selected_pack = &bdms_tree_specs_bdms_8x8_8_quadruple;
      selected_name = "torus_8x8_d8";
      break;
    default:
      fatal(CALL_INFO, -1,
            "TreesAllreduce: torus 8x8 tree_set must be in 0..7; got %d\n",
            tree_set);
    }
  } else if (tree_pack == "torus" && dimensions == 2 &&
             dimensions_sizes != nullptr && dimensions_sizes[0] == 64 &&
             dimensions_sizes[1] == 64) {
    if (tree_set < 0 || tree_set >= kTorus64x64TreePackCount) {
      fatal(CALL_INFO, -1,
            "TreesAllreduce: torus 64x64 tree_set must be in 0..63; got %d\n",
            tree_set);
    } else {
      selected_pack = kTorus64x64TreePacks[tree_set];
      selected_name_storage = "torus_64x64_d" + std::to_string(tree_set + 1);
      selected_name = selected_name_storage.c_str();
    }
  } else if (tree_pack == "torus" && dimensions == 2 &&
             dimensions_sizes != nullptr && dimensions_sizes[0] == 32 &&
             dimensions_sizes[1] == 32) {
    if (tree_set < 0 || tree_set >= kTorus32x32TreePackCount) {
      fatal(CALL_INFO, -1,
            "TreesAllreduce: torus 32x32 tree_set must be in 0..31; got %d\n",
            tree_set);
    } else {
      selected_pack = kTorus32x32TreePacks[tree_set];
      selected_name_storage = "torus_32x32_d" + std::to_string(tree_set + 1);
      selected_name = selected_name_storage.c_str();
    }
  } else if (tree_pack == "hyperx" && size() == 64) {
    selected_pack = &bdms_tree_specs_bdms_hyperx_8x8_2_quadruple;
    selected_name = "hyperx_8x8_2";
  } else if (tree_pack == "hyperx" && size() == 1024) {
    switch (tree_set) {
    case 5:
      selected_pack = &bdms_tree_specs_bdms_hyperx_32x32_budget_b4_quadruple;
      selected_name = "hyperx_32x32_budget_b4";
      break;
    case 6:
      selected_pack = &bdms_tree_specs_bdms_hyperx_32x32_budget_b8_quadruple;
      selected_name = "hyperx_32x32_budget_b8";
      break;
    case 7:
      selected_pack = &bdms_tree_specs_bdms_hyperx_32x32_budget_b16_quadruple;
      selected_name = "hyperx_32x32_budget_b16";
      break;
    case 8:
      selected_pack = &bdms_tree_specs_bdms_hyperx_32x32_budget_b32_quadruple;
      selected_name = "hyperx_32x32_budget_b32";
      break;
    case 9:
      selected_pack = &bdms_tree_specs_bdms_hyperx_32x32_budget_b64_quadruple;
      selected_name = "hyperx_32x32_budget_b64";
      break;
    default:
      fatal(CALL_INFO, -1,
            "TreesAllreduce: HyperX 32x32 budget packs are tree_set "
            "5=b4, 6=b8, 7=b16, 8=b32, 9=b64; got %d\n",
            tree_set);
    }
  } else if (tree_pack == "hyperx" && size() == 4096) {
    if (tree_set != 7) {
      fatal(CALL_INFO, -1,
            "TreesAllreduce: HyperX 64x64 uses only budget_b16; expected "
            "tree_set=7, got %d\n",
            tree_set);
    }
    selected_pack = &bdms_tree_specs_bdms_hyperx_64x64_budget_b16_quadruple;
    selected_name = "hyperx_64x64_budget_b16";
  } else if (tree_pack == "rect" && dimensions == 2 &&
             dimensions_sizes != nullptr) {
    if (dimensions_sizes[0] == 64 && dimensions_sizes[1] == 16) {
      selected_pack = &bdms_tree_specs_64x16_40_double;
      selected_name = "torus_64x16_40";
    } else if (dimensions_sizes[0] == 128 && dimensions_sizes[1] == 8) {
      selected_pack = &bdms_tree_specs_128x8_68_double;
      selected_name = "torus_128x8_68";
    } else if (dimensions_sizes[0] == 256 && dimensions_sizes[1] == 4) {
      selected_pack = &bdms_tree_specs_256x4_130_double;
      selected_name = "torus_256x4_130";
    }
  } else if (tree_pack == "default" && size() == 64) {
    selected_pack = &bdms_tree_specs_bdms_hyperx_8x8_2_quadruple;
    selected_name = "legacy_hyperx_8x8_2";
  }

  if (selected_pack == nullptr) {
    fatal(CALL_INFO, -1,
          "TreesAllreduce: unsupported tree_pack=%s, dimensions=%u, "
          "dimensions_sizes=%s, ranks=%d, tree_set=%d\n",
          tree_pack.c_str(), dimensions, dimensions_sizes_s.c_str(), size(),
          tree_set);
  }
  if (rank() == 0) {
    printf("[TreesAllreduce] selected_pack=%s tree_set=%d\n", selected_name,
           tree_set);
  }
  m_allreduce = new TreesCollective(
      *this, dimensions, ports, recvcount, rank(), size(), GroupWorld,
      *selected_pack, route_table, route_table_map, aggregation_cost_ns,
      !blocking, sync, TREES_ALLREDUCE, m_data, dimensions_sizes,
      latency_optimal);
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