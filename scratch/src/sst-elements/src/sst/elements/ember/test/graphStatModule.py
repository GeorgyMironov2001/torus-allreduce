"""Enable maximum Merlin / Firefly / Ember statistics for graph alltoall runs.

Usage (from ember/test cwd):
  sstsim.x --model-options="... --statsModule=graphStatModule --statsFile=/path/stats.csv" emberLoad.py

Collects every registered statistic on all simulation components (including
subcomponents such as merlin.linkcontrol, firefly.ctrlMsg, router ports).
"""

import sst

_ACCUM = {"type": "sst.AccumulatorStatistic", "rate": "0ns"}

# Explicit per-type lists (from registerStatistic in Merlin/Firefly sources).
# Used in addition to enableAllStatistics* so nothing is missed when SST
# filters by load level.
_HR_ROUTER_STATS = (
    "send_packet_count",
    "send_bit_count",
    "output_port_stalls",
    "idle_time",
    "width_adj_count",
    "xbar_stalls",
)

_LINK_STATS = (
    "packet_latency",
    "send_bit_count",
    "send_packet_count",
    "output_port_stalls",
    "idle_time",
)

_FIREFLY_NIC_STATS = (
    "sentByteCount",
    "rcvdByteCount",
    "sentPkts",
    "rcvdPkts",
    "networkStall",
    "hostStall",
    "recvStreamPending",
    "sendStreamPending",
)

_FIREFLY_CTRL_STATS = (
    "posted_receive_list",
    "received_msg_list",
)

# Top-level component types present in emberLoad graph runs.
_COMPONENT_TYPES = (
    "merlin.hr_router",
    "merlin.reorderlinkcontrol",
    "merlin.linkcontrol",
    "firefly.nic",
    "firefly.loopBack",
    "ember.EmberEngine",
)


def _enable_stat(comp_type, stat_name):
    sst.enableStatisticForComponentType(comp_type, stat_name, _ACCUM, 1)


def _enable_stats(comp_type, stats):
    for stat in stats:
        _enable_stat(comp_type, stat)


def init(output_file):
    sst.setStatisticLoadLevel(9)
    sst.setStatisticOutput("sst.statOutputCSV")
    sst.setStatisticOutputOptions(
        {
            "filepath": output_file,
            "separator": ", ",
        }
    )

    # Catch every registered stat, including dynamic subcomponent names.
    sst.enableAllStatisticsForAllComponents(_ACCUM)

    # Explicit enable for known Merlin router / link counters.
    _enable_stats("merlin.hr_router", _HR_ROUTER_STATS)
    _enable_stats("merlin.reorderlinkcontrol", _LINK_STATS)
    _enable_stats("merlin.linkcontrol", _LINK_STATS)

    # Firefly host/NIC and MPI queue stats.
    _enable_stats("firefly.nic", _FIREFLY_NIC_STATS)
    for stat in _FIREFLY_CTRL_STATS:
        _enable_stat("firefly.ctrlMsg", stat)

    # Per component type, recurse into subcomponents (linkcontrol, ctrlMsg, …).
    for comp_type in _COMPONENT_TYPES:
        sst.enableAllStatisticsForComponentType(comp_type, _ACCUM, 1)
