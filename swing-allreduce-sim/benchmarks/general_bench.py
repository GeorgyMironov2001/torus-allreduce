import json
import math
import subprocess
import sys
from argparse import ArgumentParser
import shutil
import os
import re
import time

# Change this to Path later
logs_folder = "logs"
sbatch_folder = "sbatch"
ember_load_folder = "/home/mipt-user/torus-allreduce/scratch/src/sst-elements/src/sst/elements/ember/test/"
sstsim_path = "/home/mipt-user/torus-allreduce/local/sstcore-15.0.0/libexec/sstsim.x"

EMBERTREES_SPINELEAF = (
    "/home/mipt-user/torus-allreduce/scratch/src/sst-elements/src/sst/elements/"
    "ember/mpi/motifs/embertrees/spineleaf"
)
EMBERTREES_DRAGONFLY = (
    "/home/mipt-user/torus-allreduce/scratch/src/sst-elements/src/sst/elements/"
    "ember/mpi/motifs/embertrees/dragonfly"
)
EMBERTREES_RECT = (
    "/home/mipt-user/torus-allreduce/scratch/src/sst-elements/src/sst/elements/"
    "ember/mpi/motifs/embertrees/rect"
)
EMBERTREES_ROUTETABLES = (
    "/home/mipt-user/torus-allreduce/scratch/src/sst-elements/src/sst/elements/"
    "ember/mpi/motifs/emberroutingtables"
)

# Number of allreduce trees used by TreesAllreduce for a given job size.
TREES_BY_JOB_SIZE = {
    "8x4": 2,
    "8x8": 4,
    "16x16": 4,
    "32x32": 4,
    "64x64": 4,
}

# Static audit of torus 8x8 quadruples. Keys are arg.tree_set
# (d = tree_set + 1); values are center blocks in the largest isend.
TORUS_8x8_TREE_MAX_PACKED_BLOCKS = {
    0: 4,    # d1
    1: 43,   # d2
    2: 38,   # d3
    3: 27,   # d4
    4: 21,   # d5
    5: 19,   # d6
    6: 16,   # d7
    7: 16,   # d8
}

# Static audit of the largest (peer, stage, route_class) send group in every
# torus 64x64 quadruple. Keys are arg.tree_set (d = tree_set + 1); values are
# center blocks packed into one TreesAllreduce isend.
TORUS_64x64_TREE_MAX_PACKED_BLOCKS = {
    0: 4,      # d1
    1: 617,    # d2
    2: 2220,   # d3
    3: 2603,   # d4
    4: 2256,   # d5
    5: 1983,   # d6
    6: 1751,   # d7
    7: 1573,   # d8
    8: 1443,   # d9
    9: 1252,   # d10
    10: 1093,  # d11
    11: 960,   # d12
    12: 897,   # d13
    13: 905,   # d14
    14: 831,   # d15
    15: 828,   # d16
    16: 735,   # d17
    17: 724,   # d18
    18: 623,   # d19
    19: 704,   # d20
    20: 622,   # d21
    21: 575,   # d22
    22: 585,   # d23
    23: 531,   # d24
    24: 513,   # d25
    25: 550,   # d26
    26: 548,   # d27
    27: 489,   # d28
    28: 484,   # d29
    29: 473,   # d30
    30: 481,   # d31
    31: 251,   # d32
    32: 308,   # d33
    33: 301,   # d34
    34: 303,   # d35
    35: 299,   # d36
    36: 695,   # d37
    37: 729,   # d38
    38: 673,   # d39
    39: 715,   # d40
    40: 645,   # d41
    41: 745,   # d42
    42: 624,   # d43
    43: 683,   # d44
    44: 624,   # d45
    45: 624,   # d46
    46: 622,   # d47
    47: 561,   # d48
    48: 621,   # d49
    49: 621,   # d50
    50: 500,   # d51
    51: 532,   # d52
    52: 500,   # d53
    53: 559,   # d54
    54: 451,   # d55
    55: 497,   # d56
    56: 376,   # d57
    57: 348,   # d58
    58: 374,   # d59
    59: 373,   # d60
    60: 225,   # d61
    61: 219,   # d62
    62: 160,   # d63
    63: 128,   # d64
}


def num_ranks_from_shape(shape):
    # torus/hyperx: "8x8"; spineleaf Merlin: "4,4:16"; dragonfly: "2:4:1:9"
    if ":" in shape:
        parts = shape.split(":")
        if len(parts) == 4:
            p, a, _n, g = (int(x) for x in parts)
            return p * a * g
        # S,S:L → S * L hosts (2-level SL)
        left, leaves = shape.split(":")
        spines = int(left.split(",")[0])
        return spines * int(leaves)
    num_ranks = 1
    for part in shape.replace(",", "x").split("x"):
        if part:
            num_ranks *= int(part)
    return num_ranks


def num_trees_for_bench(bench, shape):
    if bench == "Trees":
        if shape not in TREES_BY_JOB_SIZE:
            print(
                f"Warning: unknown num_trees for job_size {shape}, using 4"
            )
        return TREES_BY_JOB_SIZE.get(shape, 4)
    if bench in ("Overlay", "DBTree"):
        return 1
    return 1


def _split_with_last_remainder(total, parts):
    if parts <= 0:
        raise ValueError(f"parts must be positive, got {parts}")
    quotient, remainder = divmod(total, parts)
    return [quotient] * (parts - 1) + [quotient + remainder]


def _max_ring_block(count, ranks):
    if ranks <= 0:
        raise ValueError(f"ranks must be positive, got {ranks}")
    if count < ranks:
        return count
    block_count, remainder = divmod(count, ranks)
    return block_count + remainder


def _max_half_blocks(count, ranks):
    if ranks <= 0 or ranks % 2:
        raise ValueError(f"ranks must be positive and even, got {ranks}")
    if count < ranks:
        return count
    block_count, remainder = divmod(count, ranks)
    return (ranks // 2) * block_count + remainder


# Static audit of TrivanceCollectiveEngine::getBitmaps(). Values are the
# largest number of equal rank-blocks assigned to one (port, step) isend.
# Keys use the dimensions_sizes order passed by launchAll.py; Trivance reverses
# that order while parsing it, which matters for rectangular jobs.
TRIVANCE_2D_MAX_PACKED_BLOCKS = {
    (8, 8): 24,
    (16, 16): 67,
    (32, 32): 352,
    (64, 64): 1232,
    (64, 16): 300,
    (16, 64): 308,
    (128, 8): 384,
    (8, 128): 344,
    (256, 4): 217,
    (4, 256): 149,
}

# Static audit of BruckCollectiveEngine::getBitmaps(). The motif uses the same
# paired-port split as Trivance, but its peers are at +3^k and +2*3^k.
# Keys use the dimensions_sizes string order; Bruck reverses it internally.
BRUCK_2D_MAX_PACKED_BLOCKS = {
    (8, 8): 24,
    (16, 16): 68,
    (32, 32): 153,
    (64, 64): 1246,
    (64, 16): 299,
    (16, 64): 308,
    (128, 8): 384,
    (8, 128): 384,
    (256, 4): 226,
    (4, 256): 149,
}


def _max_trivance_engine_count(count, num_ranks, ports):
    """Largest per-direction Trivance chunk, in FLOAT elements.

    Trivance creates two engines (opposite directions) per chunk. For large
    messages, ``pairs = min(count, ports) // 2`` chunks split the input; for
    count < num_ranks every engine operates on the whole input.
    """
    if count <= 0:
        raise ValueError(f"Trivance count must be positive, got {count}")
    if num_ranks <= 0:
        raise ValueError(
            f"Trivance num_ranks must be positive, got {num_ranks}"
        )
    active_ports = min(count, ports)
    pairs = active_ports // 2
    if pairs <= 0:
        raise ValueError(
            "Trivance needs at least two active ports; "
            f"got count={count}, ports={ports}"
        )
    if count < num_ranks:
        return count
    return max(_split_with_last_remainder(count, pairs))


def _trivance_fixed_16cube_max_elements(count, num_ranks, ports):
    """Largest bandwidth-mode isend in the optimized 16x16x16 schedule."""
    engine_count = _max_trivance_engine_count(count, num_ranks, ports)
    # Keep the C++ implementation's decimal factor and ceil exactly:
    # get_step_message_count() is largest at relative_step == 0.
    return math.ceil(0.33 * engine_count)


def _trivance_2d_max_elements(
    count, num_ranks, ports, dimensions, max_engine_count
):
    """Largest isend for the audited generic 2D Trivance bitmap schedule."""
    dimensions_key = tuple(dimensions)
    max_packed_blocks = TRIVANCE_2D_MAX_PACKED_BLOCKS.get(dimensions_key)
    if (
        len(dimensions) != 2
        or math.prod(dimensions) != num_ranks
        or ports != 2 * len(dimensions)
        or max_packed_blocks is None
    ):
        return None
    if max_engine_count < num_ranks:
        # getBlockSize() returns the complete engine chunk in this case.
        return max_engine_count
    block_count, remainder = divmod(max_engine_count, num_ranks)
    if remainder:
        # Unequal block sizes change getBitmaps()' greedy load assignment.
        return None
    return max_packed_blocks * block_count


def _bruck_2d_max_elements(
    count, num_ranks, ports, dimensions, max_engine_count
):
    """Largest isend for the audited generic 2D Bruck bitmap schedule."""
    max_packed_blocks = BRUCK_2D_MAX_PACKED_BLOCKS.get(tuple(dimensions))
    if (
        len(dimensions) != 2
        or math.prod(dimensions) != num_ranks
        or ports != 2 * len(dimensions)
        or max_packed_blocks is None
    ):
        return None
    if max_engine_count < num_ranks:
        return max_engine_count
    block_count, remainder = divmod(max_engine_count, num_ranks)
    if remainder:
        return None
    return max_packed_blocks * block_count


def _parse_dimension_sizes(dimensions_sizes):
    if dimensions_sizes is None:
        return []
    if isinstance(dimensions_sizes, str):
        return [
            int(part)
            for part in dimensions_sizes.replace("x", ",").split(",")
            if part
        ]
    return [int(part) for part in dimensions_sizes]


_RECT_PACK_CACHE = {}


def _load_rect_overlay_pack(trees_file):
    """Sidecar for overlay_rect_*.json: per-template shares and pack counts.

    groups[i][t] is how many template-t edges share one (peer, stage) message.
    Built from the shift orbit: one template edge hits every rank exactly once.
    """
    cached = _RECT_PACK_CACHE.get(trees_file)
    if cached is not None:
        return cached
    pack_path = os.path.splitext(trees_file)[0] + ".pack.json"
    if not os.path.isfile(pack_path):
        raise FileNotFoundError(
            f"missing overlay pack audit {pack_path} for {trees_file}"
        )
    with open(pack_path, "r") as f:
        pack = json.load(f)
    _RECT_PACK_CACHE[trees_file] = pack
    return pack


def overlay_rect_max_packed_elements(trees_file, count):
    """Upper bound on the largest Overlay isend, in FLOAT elements.

    Matches Ember's prefix layout (std::round half away from 0). Chunk sizes
    inside one template differ by at most one, so summing each template's
    largest chunk over a packed (peer, stage) is >= every real isend.
    """
    pack = _load_rect_overlay_pack(trees_file)
    template_shares = pack["template_shares"]
    per_template = int(pack["P"])
    ntrees = int(pack["K"])
    if len(template_shares) * per_template != ntrees:
        raise ValueError(
            f"overlay pack {trees_file} has K={ntrees} but "
            f"{len(template_shares)}*{per_template} templates"
        )
    prefix = 0.0
    offset = 0
    seen = 0
    max_size = []
    for share in template_shares:
        local_max = 0
        for _ in range(per_template):
            seen += 1
            prefix += share
            if seen < ntrees:
                nxt = int(count * prefix + 0.5)
                if nxt < 0:
                    nxt = 0
                elif nxt > count:
                    nxt = count
            else:
                nxt = count
            if nxt < offset:
                nxt = offset
            local_max = max(local_max, nxt - offset)
            offset = nxt
        max_size.append(local_max)
    best = 0
    for group in pack["groups"]:
        total = 0
        for copies, size in zip(group, max_size):
            total += copies * size
        if total > best:
            best = total
    return best


def overlay_spineleaf_constructive_max_packed_elements(trees_file, count):
    """Largest packed isend for a constructive Spine-Leaf schedule.

    A same-leaf peer message contains one equal-sized tree chunk per leaf;
    cross-leaf messages contain one chunk.  This is independent of ``border``.
    """
    match = re.fullmatch(
        r"overlay_sl_s(\d+)_l(\d+)_b\d+_constructive\.json",
        os.path.basename(trees_file),
    )
    if not match:
        raise ValueError(f"not a constructive Spine-Leaf file: {trees_file}")
    spines, leaves = map(int, match.groups())
    ranks = spines * leaves
    max_tree_size = (count + ranks - 1) // ranks
    return leaves * max_tree_size


def overlay_spineleaf_radix_max_packed_elements(trees_file, count):
    """Largest packed isend, using the generator's exact structural audit."""
    pack_file = os.path.splitext(trees_file)[0] + ".pack.json"
    with open(pack_file) as f:
        pack = json.load(f)
    ranks = int(pack["n"])
    max_tree_size = (count + ranks - 1) // ranks
    return int(pack["max_packed_chunks"]) * max_tree_size


def compute_short_value(
    bench,
    count,
    num_ranks,
    num_trees=1,
    ring_lanes=1,
    ports=1,
    dimensions_sizes=None,
    tree_pack="",
    tree_set=0,
    trees_file="",
):
    """Firefly shortMsgLength (bytes) patched into defaultParams.py per run.

    Values for bandwidth-oriented algorithms match the largest actual isend.
    Returning None leaves the template default, so every large-message bench
    should have an explicit branch here.
    """
    if bench == "SwingL":
        return 8 * count
    if bench == "SwingB":
        lane_counts = _split_with_last_remainder(count, min(count, ports))
        return 4 * max(
            _max_half_blocks(lane_count, num_ranks)
            for lane_count in lane_counts
        )
    if bench == "RecDoubB":
        return 4 * _max_half_blocks(count, num_ranks)
    if bench in ("Trivance", "TrivanceB", "TrivanceL"):
        dimensions = _parse_dimension_sizes(dimensions_sizes)
        engine_count = _max_trivance_engine_count(
            count, num_ranks, ports
        )
        if bench == "TrivanceL":
            # latency_optimal marks every block for every step, so each engine
            # sends its complete per-direction chunk.
            return 4 * engine_count
        if dimensions == [16, 16, 16]:
            # embertrivancecoll.cc uses its fixed-size schedule only for this
            # shape. Step 0 is the largest: ceil(0.33 * engine_count) FLOATs.
            return 4 * _trivance_fixed_16cube_max_elements(
                count, num_ranks, ports
            )
        max_elements = _trivance_2d_max_elements(
            count, num_ranks, ports, dimensions, engine_count
        )
        if max_elements is not None:
            return 4 * max_elements
        # Physical topology does not affect the isend size, but the logical
        # dimensions do because Trivance builds peers and block bitmaps from
        # dimensions_sizes. Use a safe bound for unaudited shapes/remainders.
        print(
            "Warning: using conservative Trivance shortValue for "
            f"dimensions={dimensions}, count={count}, ports={ports}"
        )
        return 4 * engine_count
    if bench in ("Bruck", "BruckB", "BruckL"):
        dimensions = _parse_dimension_sizes(dimensions_sizes)
        # BruckCollective splits the payload across port pairs exactly as
        # TrivanceCollective does.
        engine_count = _max_trivance_engine_count(
            count, num_ranks, ports
        )
        if bench == "BruckL":
            return 4 * engine_count
        max_elements = _bruck_2d_max_elements(
            count, num_ranks, ports, dimensions, engine_count
        )
        if max_elements is not None:
            return 4 * max_elements
        print(
            "Warning: using conservative Bruck shortValue for "
            f"dimensions={dimensions}, count={count}, ports={ports}"
        )
        return 4 * engine_count
    if bench == "Rings":
        if ring_lanes <= 0 or count % ring_lanes != 0:
            raise ValueError(
                f"Rings count={count} must be divisible by "
                f"ring_lanes={ring_lanes}"
            )
        # RingAllreduce25D splits count over four concurrent rings. Every
        # ring then sends one getBlockSize() block per step. Match that C++
        # implementation exactly, including its enlarged final remainder block.
        per_ring_count = count // ring_lanes
        return 4 * _max_ring_block(per_ring_count, num_ranks)
    if bench == "Bucket":
        dimensions = _parse_dimension_sizes(dimensions_sizes)
        if not dimensions:
            raise ValueError("Bucket requires dimensions_sizes")
        bucket_lanes = 2 * len(dimensions)
        adjusted_count = (
            (count + bucket_lanes - 1) // bucket_lanes
        ) * bucket_lanes
        per_ring_count = adjusted_count // bucket_lanes
        return 4 * _max_ring_block(per_ring_count, min(dimensions))
    if bench == "Trees":
        dimensions = _parse_dimension_sizes(dimensions_sizes)
        if tree_pack == "rect":
            # The rectangular double-tree packs can send up to half of the
            # FLOAT payload in one packed message. This also matches the
            # successful count=2**24 runs (32 MiB shortValue).
            return 2 * count
        if tree_pack == "torus" and dimensions == [8, 8]:
            tree_set_i = int(tree_set)
            max_packed_blocks = TORUS_8x8_TREE_MAX_PACKED_BLOCKS.get(
                tree_set_i
            )
            if max_packed_blocks is None:
                raise ValueError(
                    f"Torus 8x8 Trees tree_set must be in 0..7; "
                    f"got {tree_set_i}"
                )
            total_blocks = num_ranks * num_trees
            block_count, remainder = divmod(count, total_blocks)
            if remainder:
                raise ValueError(
                    f"Trees_{tree_set_i + 1} currently requires count "
                    f"divisible by {total_blocks}; got count={count}"
                )
            # Counts 2**8..2**26 divide exactly into 64 ranks * 4 trees.
            # FLOAT is four bytes.
            return 4 * max_packed_blocks * block_count
        if tree_pack == "torus" and dimensions == [64, 64]:
            tree_set_i = int(tree_set)
            max_packed_blocks = TORUS_64x64_TREE_MAX_PACKED_BLOCKS.get(
                tree_set_i
            )
            if max_packed_blocks is None:
                raise ValueError(
                    "Torus 64x64 Trees supports tree_set 0..63 "
                    "(d1..d64); "
                    f"got {tree_set_i}"
                )
            total_blocks = num_ranks * num_trees
            block_count, remainder = divmod(count, total_blocks)
            if remainder:
                raise ValueError(
                    f"Trees_{tree_set_i + 1} currently requires count "
                    f"divisible by {total_blocks}; got count={count}"
                )
            # Counts 2**14..2**26 divide exactly into
            # 4096 ranks * 4 trees. FLOAT is four bytes.
            return 4 * max_packed_blocks * block_count
        if tree_pack == "torus" and dimensions == [32, 32]:
            tree_set_i = int(tree_set)
            if tree_set_i < 0 or tree_set_i > 31:
                raise ValueError(
                    "Torus 32x32 Trees supports tree_set 0..31 "
                    "(d1..d32); "
                    f"got {tree_set_i}"
                )
            total_blocks = num_ranks * num_trees
            if count % total_blocks:
                raise ValueError(
                    f"Trees_{tree_set_i + 1} currently requires count "
                    f"divisible by {total_blocks}; got count={count}"
                )
            # One packed isend holds at most the whole FLOAT payload.
            # 4 * count bytes is that ceiling, so the short-path assert
            # accepts every Trees_1..32 pack. Counts 2**12..2**26 divide
            # evenly into 1024 ranks * 4 trees.
            return 4 * count
        if tree_pack == "hyperx" and dimensions == [32, 32]:
            tree_set_i = int(tree_set)
            max_packed_blocks = HYPERX_32x32_TREE_MAX_PACKED_BLOCKS.get(
                tree_set_i
            )
            if max_packed_blocks is None:
                raise ValueError(
                    "HyperX 32x32 Trees tree_set must be 5..9 "
                    "(budget_b4/b8/b16/b32/b64); "
                    f"got {tree_set_i}"
                )
            total_blocks = num_ranks * num_trees
            block_count, remainder = divmod(count, total_blocks)
            if remainder:
                raise ValueError(
                    "HyperX 32x32 Trees currently requires count divisible by "
                    f"{total_blocks}; got count={count}"
                )
            return 4 * max_packed_blocks * block_count
        if (
            tree_pack == "hyperx"
            and dimensions == [64, 64]
            and int(tree_set) == 7
        ):
            total_blocks = num_ranks * num_trees
            block_count, remainder = divmod(count, total_blocks)
            if remainder:
                raise ValueError(
                    "Trees_budget_b16 currently requires count divisible by "
                    f"{total_blocks}; got count={count}"
                )
            # Static audit of
            # emberbdmstrees_hyperx_64_64_budget_b16.h:
            # the largest packed isend contains 126 center blocks.
            return 4 * 126 * block_count
        # Existing configuration-specific fallback for other compiled packs.
        return int(count / 2**4)
    if (
        bench == "Overlay"
        and re.fullmatch(
            r"overlay_sl_s\d+_l\d+_b\d+_constructive\.json",
            os.path.basename(trees_file),
        )
    ):
        return 4 * overlay_spineleaf_constructive_max_packed_elements(
            trees_file, count
        )
    if (
        bench == "Overlay"
        and re.fullmatch(
            r"overlay_sl_s\d+_l\d+_radix[0-9x]+_constructive\.json",
            os.path.basename(trees_file),
        )
    ):
        return 4 * overlay_spineleaf_radix_max_packed_elements(
            trees_file, count
        )
    if (
        bench == "Overlay"
        and os.path.basename(trees_file)
        == "overlay_sl_s16_l64_k60x4.json"
    ):
        # Static audit of the k60x4 schedule: 1024 equal-sized tree chunks,
        # with at most 16 chunks packed into one (peer, route_class) message.
        # The ceil keeps the threshold safe for counts not divisible by 1024;
        # the power-of-two experiment counts are exact.
        return 4 * 16 * ((count + 1023) // 1024)
    if (
        bench == "Overlay"
        and os.path.basename(trees_file)
        == "overlay_sl_s16_l32_k1x1.json"
    ):
        # Static audit of the 16s32l k1x1 schedule: 512 equal-sized tree
        # chunks. A same-leaf isend packs one chunk from each of the 32
        # trees that share the sender's root-local; cross-leaf isends are
        # a single chunk. Counts 2**9..2**26 divide evenly, so the largest
        # isend is exactly count/16 FLOATs = count/4 bytes.
        return 4 * 32 * ((count + 511) // 512)
    if (
        bench == "Overlay"
        and os.path.basename(trees_file).startswith("overlay_rect_")
    ):
        # Packed (peer, stage) isend. 4 * count would mark the whole payload
        # short; the rect schedules pack hundreds of chunks into one send.
        return 4 * overlay_rect_max_packed_elements(trees_file, count)
    if bench in (
        "Overlay",
        "DBTree",
        "RecDoubL",
        "RecDoubBM",
        "RecDoubLM",
        "Torus",
        "AlltoallB",
        "AlltoallL",
    ):
        return 4 * count
    return None


def apply_short_value_to_default_params(default_params_path, short_value):
    with open(default_params_path, "r") as f:
        content = f.read()
    new_content, replacements = re.subn(
        r"^(\s*)valueShort\s*=\s*.+$",
        rf"\1valueShort = {short_value}",
        content,
        flags=re.MULTILINE,
    )
    if replacements == 0:
        print(
            f"Warning: could not patch valueShort in {default_params_path}"
        )
        return
    with open(default_params_path, "w") as f:
        f.write(new_content)


def tree_edge_length_from_route_table(route_table_file):
    """Extract tree edge length from ember_bdmst_routing_table_8_6.json -> 6."""
    if not route_table_file:
        return None
    basename = os.path.basename(route_table_file)
    match = re.search(r"ember_bdmst_routing_table_\d+_(\d+)\.json$", basename)
    if match:
        return match.group(1)
    return None


def hyperx_tree_scheme_from_route_table(route_table_file):
    """ember_bdmst_routing_table_hyperx_64_64_7x1.json -> 7x1."""
    if not route_table_file:
        return None
    basename = os.path.basename(route_table_file)
    match = re.search(
        r"ember_bdmst_routing_table_hyperx_\d+_\d+_(.+)\.json$",
        basename,
    )
    if match:
        return match.group(1)
    return None


# HyperX 64x64 tree packs in embertreesallreduce.cc (arg.tree_set index).
HYPERX_64x64_TREE_SCHEMES = (
    "2",
    "k3",
    "k4",
    "k8",
    "k16",
    "budget_b4",
    "budget_b8",
    "budget_b16",
    "budget_b32",
    "budget_b64",
    "cplex_b4_s96",
)

# HyperX 32x32 uses the same budget indices as 64x64. The constructive
# cap changes the stage count; center shifts still put 2*(32-1) blocks
# into the largest (peer, stage) isend.
HYPERX_32x32_BUDGET_TREE_SETS = {
    5: "budget_b4",
    6: "budget_b8",
    7: "budget_b16",
    8: "budget_b32",
    9: "budget_b64",
}
HYPERX_32x32_TREE_MAX_PACKED_BLOCKS = {
    tree_set: 62 for tree_set in HYPERX_32x32_BUDGET_TREE_SETS
}


def resolve_hyperx_64x64_route_table(tree_set, route_table_file=""):
    """Map tree_set to the matching HyperX 64x64 route-table JSON."""
    schemes = HYPERX_64x64_TREE_SCHEMES
    idx = int(tree_set)
    if idx < 0 or idx >= len(schemes):
        raise ValueError(
            f"HyperX 64x64 tree_set must be in 0..{len(schemes) - 1} "
            f"({'/'.join(schemes)}), got {tree_set}"
        )
    scheme = schemes[idx]
    dirname = (
        os.path.dirname(route_table_file)
        if route_table_file
        else EMBERTREES_ROUTETABLES
    )
    if not dirname:
        dirname = EMBERTREES_ROUTETABLES
    return os.path.join(
        dirname, f"ember_bdmst_routing_table_hyperx_64_64_{scheme}.json"
    )


def resolve_hyperx_32x32_route_table(tree_set, route_table_file=""):
    """Map tree_set 5..9 to the matching HyperX 32x32 budget route table."""
    schemes = HYPERX_32x32_BUDGET_TREE_SETS
    idx = int(tree_set)
    if idx not in schemes:
        raise ValueError(
            "HyperX 32x32 tree_set must be "
            + ", ".join(f"{k}={v}" for k, v in schemes.items())
            + f"; got {tree_set}"
        )
    scheme = schemes[idx]
    dirname = (
        os.path.dirname(route_table_file)
        if route_table_file
        else EMBERTREES_ROUTETABLES
    )
    if not dirname:
        dirname = EMBERTREES_ROUTETABLES
    return os.path.join(
        dirname, f"ember_bdmst_routing_table_hyperx_32_32_{scheme}.json"
    )


# VCs/VN for torus_trees turn+dateline bump (from ecdg_check_torus_trees.py).
# num_vc = max_vc + 1 on full Trees_k RS+AG flow set.
NUM_VCS_TREES_16x16 = {
    1: 5,
    2: 4,
    3: 5,
    4: 4,
    5: 4,
    6: 2,
    7: 2,
    8: 3,
    9: 3,
    10: 4,
    11: 4,
    12: 2,
    13: 2,
    14: 2,
    15: 2,
}

NUM_VCS_TREES_32x32 = {
    1: 5,
    2: 5,
    3: 4,
    4: 4,
    5: 4,
    6: 4,
    7: 4,
    8: 3,
    9: 2,
    10: 4,
    11: 4,
    12: 4,
    13: 2,
    14: 2,
    15: 4,
    16: 4,
    17: 5,
    18: 5,
    19: 4,
    20: 2,
    21: 4,
    22: 4,
    23: 4,
    24: 2,
    25: 2,
    26: 2,
    27: 2,
    28: 2,
    29: 2,
    30: 2,
    31: 2,
    32: 1,
}

NUM_VCS_TREES_64x64 = {
    1: 5,
    2: 5,
    3: 5,
    4: 5,
    5: 4,
    6: 5,
    7: 4,
    8: 4,
    9: 4,
    10: 4,
    11: 4,
    12: 4,
    13: 3,
    14: 3,
    15: 4,
    16: 3,
    17: 4,
    18: 4,
    19: 4,
    20: 2,
    21: 3,
    22: 3,
    23: 3,
    24: 4,
    25: 4,
    26: 3,
    27: 4,
    28: 3,
    29: 2,
    30: 2,
    31: 2,
    32: 4,
    33: 4,
    34: 4,
    35: 4,
    36: 4,
    37: 2,
    38: 2,
    39: 3,
    40: 3,
    41: 2,
    42: 2,
    43: 2,
    44: 2,
    45: 2,
    46: 2,
    47: 2,
    48: 2,
    49: 2,
    50: 2,
    51: 2,
    52: 3,
    53: 2,
    54: 2,
    55: 2,
    56: 2,
    57: 2,
    58: 2,
    59: 2,
    60: 2,
    61: 2,
    62: 2,
    63: 2,
    64: 1,
}

NUM_VCS_TREES_BY_SHAPE = {
    "16x16": NUM_VCS_TREES_16x16,
    "32x32": NUM_VCS_TREES_32x32,
    "64x64": NUM_VCS_TREES_64x64,
}


def num_vcs_for_trees_route_table(route_table_file, shape="16x16"):
    """ECDG analytical VC count for a BDMST route-table JSON (not an SST CLI flag).

    Falls back to 5 (worst case on tabulated shapes) if k/shape is unknown.
    """
    k_s = tree_edge_length_from_route_table(route_table_file)
    if k_s is None:
        return 5
    k = int(k_s)
    table = NUM_VCS_TREES_BY_SHAPE.get(shape)
    if table is not None:
        return table.get(k, 5)
    # Other shapes not tabulated yet — keep the safe upper bound.
    return 5


def tree_edge_length_from_tree_set(tree_set):
    """tree_set 0 -> d1, 1 -> d2, ... (k = tree_set + 1)."""
    edge_length = int(tree_set) + 1
    if edge_length < 1 or edge_length > 64:
        assert False, f"Invalid tree_set {tree_set}"
    return str(edge_length)


def parse_tree_sets(spec):
    """Parse tree_set list: '0', '0,2,4', '0-7', '7-0', or 'all'.

    Order is preserved (descending ranges like 7-0 stay reversed).
    """
    if spec is None or str(spec).strip() == "":
        return [0]
    spec = str(spec).strip()
    if spec.lower() == "all":
        return list(range(8))
    tree_sets = []
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            start_s, end_s = part.split("-", 1)
            start, end = int(start_s), int(end_s)
            if start <= end:
                tree_sets.extend(range(start, end + 1))
            else:
                tree_sets.extend(range(start, end - 1, -1))
        else:
            tree_sets.append(int(part))
    # unique, preserve order
    return list(dict.fromkeys(tree_sets))


def parse_counts(spec):
    """Parse message counts: 'All', '21,22', '20-23', or raw sizes."""
    if spec is None or str(spec).strip() == "" or str(spec).strip().lower() == "all":
        return [2**21, 2**22]
    counts = []
    for part in str(spec).split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            a_s, b_s = part.split("-", 1)
            a, b = int(a_s), int(b_s)
            lo, hi = (a, b) if a <= b else (b, a)
            # exponents if both small
            if hi < 40:
                counts.extend(2**e for e in range(lo, hi + 1))
            else:
                counts.extend(range(lo, hi + 1))
        else:
            v = int(part)
            counts.append(2**v if v < 40 else v)
    return list(dict.fromkeys(counts))


def parse_spineleaf_job_size(job_size):
    """'4s16l' -> (spines=4, leaves=16, merlin_shape='4,4:16', nnodes=64)."""
    match = re.match(r"^(\d+)s(\d+)l$", str(job_size).strip(), re.IGNORECASE)
    if not match:
        raise ValueError(
            f"spineleaf job_size must look like '4s16l' or '2s4l', got {job_size!r}"
        )
    spines = int(match.group(1))
    leaves = int(match.group(2))
    merlin_shape = f"{spines},{spines}:{leaves}"
    nnodes = spines * leaves
    label = f"{spines}s{leaves}l"
    return spines, leaves, merlin_shape, nnodes, label


def parse_dragonfly_job_size(job_size):
    """'p2a4g9' or '2:4:1:9' -> (p, a, n, g, merlin_shape, nnodes, label).

    Merlin shape is hosts_per_router:routers_per_group:intergroup_links:num_groups.
    intergroup_links defaults to 1 (balanced p=2,a=4,g=9 → shape 2:4:1:9).
    """
    spec = str(job_size).strip()
    match = re.match(
        r"^p(\d+)a(\d+)(?:n(\d+))?g(\d+)$", spec, re.IGNORECASE
    )
    if match:
        p = int(match.group(1))
        a = int(match.group(2))
        n = int(match.group(3) or 1)
        g = int(match.group(4))
    else:
        parts = spec.split(":")
        if len(parts) == 4 and all(part.isdigit() for part in parts):
            p, a, n, g = (int(x) for x in parts)
        else:
            raise ValueError(
                f"dragonfly job_size must look like 'p2a4g9' or '2:4:1:9', "
                f"got {job_size!r}"
            )
    merlin_shape = f"{p}:{a}:{n}:{g}"
    nnodes = p * a * g
    label = f"p{p}_a{a}_g{g}"
    return p, a, n, g, merlin_shape, nnodes, label


def resolve_route_table_for_tree_set(route_table_file, tree_set, job_size="8x8"):
    """Pick route table JSON matching tree_set (k = tree_set + 1)."""
    edge_length = tree_edge_length_from_tree_set(tree_set)
    if route_table_file:
        match = re.search(
            r"(ember_bdmst_routing_table_)(\d+_)(\d+)(\.json)$",
            os.path.basename(route_table_file),
        )
        if match:
            prefix, job_part, _, suffix = match.groups()
            dirname = os.path.dirname(route_table_file)
            return os.path.join(
                dirname,
                f"{prefix}{job_part}{edge_length}{suffix}",
            )
        return route_table_file
    torus_dim = job_size.split("x")[0]
    return os.path.join(
        EMBERTREES_ROUTETABLES,
        f"ember_bdmst_routing_table_{torus_dim}_{edge_length}.json",
    )


def resolve_overlay_trees_file(
    args,
    spines=None,
    leaves=None,
    topo="spineleaf",
    p=None,
    a=None,
    g=None,
    job_size=None,
):
    """Resolve Overlay / DBTree JSON under embertrees/{spineleaf,dragonfly,rect}/."""
    explicit = getattr(args, "trees_file", "") or ""
    if explicit:
        return explicit
    which = getattr(args, "overlay_kind", "Overlay")
    k = getattr(args, "overlay_k", "") or ""
    if topo == "torus":
        shape = job_size or getattr(args, "job_size", "")
        suffix = "_bfs2" if str(k) == "bfs2" else ""
        return os.path.join(
            EMBERTREES_RECT, f"overlay_rect_{shape}{suffix}.json"
        )
    if topo == "dragonfly":
        if which == "DBTree":
            return os.path.join(
                EMBERTREES_DRAGONFLY,
                f"overlay_nccl_dbtree_p{p}_a{a}_g{g}.json",
            )
        if k != "" and str(k) != "0":
            return os.path.join(
                EMBERTREES_DRAGONFLY,
                f"overlay_df_p{p}_a{a}_g{g}_k{k}.json",
            )
        return os.path.join(
            EMBERTREES_DRAGONFLY,
            f"overlay_df_p{p}_a{a}_g{g}_k3.json",
        )
    if which == "DBTree":
        return os.path.join(
            EMBERTREES_SPINELEAF,
            f"overlay_nccl_dbtree_anneal_s{spines}_l{leaves}.json",
        )
    if k != "" and str(k) != "0":
        return os.path.join(
            EMBERTREES_SPINELEAF,
            f"overlay_sl_s{spines}_l{leaves}_k{k}.json",
        )
    return os.path.join(
        EMBERTREES_SPINELEAF,
        f"overlay_sl_s{spines}_l{leaves}.json",
    )


def output_bench_name(bench, args):
    if bench == "Trees":
        tree_pack = getattr(args, "tree_pack", "") or ""
        if tree_pack == "hyperx":
            scheme = hyperx_tree_scheme_from_route_table(
                getattr(args, "route_table_file", "")
            )
            if scheme:
                return f"Trees_{scheme}"
            return bench
        # Rect: keep plain Trees (no torus k suffix)
        if tree_pack == "rect":
            return bench
        edge_length = tree_edge_length_from_route_table(
            getattr(args, "route_table_file", "")
        )
        if edge_length is None:
            edge_length = tree_edge_length_from_tree_set(
                getattr(args, "tree_set", 0)
            )
        if edge_length:
            return f"Trees_{edge_length}"
    if bench == "Overlay":
        k = getattr(args, "overlay_k", "") or ""
        if k != "" and str(k) != "0":
            if str(k) == "bfs2":
                return "Overlay_bfs2"
            if re.fullmatch(
                r"(?:b\d+|radix[0-9x]+)_constructive", str(k)
            ):
                return f"Overlay_{k}"
            return f"Overlay_k{k}"
    return bench


def resolve_output_path(out_file, bench, args):
    bench_name = output_bench_name(bench, args)
    if bench_name == bench:
        return out_file
    resolved = out_file.replace(f"/{bench}/", f"/{bench_name}/")
    os.makedirs(os.path.dirname(resolved), exist_ok=True)
    return resolved


def allocate_logic(bench, topo):
    # We allocate only for GPT or Cosmo, not needed for other benchmarks
    if "DLRM" in bench and topo == "hx4":
        return "SST_NO_MEM=1 IS_XH4=1"
    if "gpt" in bench.lower() or "cosmo" in bench.lower():
        return "SST_NO_MEM=0"
    else:
        return "SST_NO_MEM=1"


def run_slim(args, launch_string, bench, topo):
    if topo == "dragonfly" and args.nodes > 4:
        node_num = 128
    else:
        node_num = args.nodes
    allocation_policy = allocate_logic(bench, topo)
    slim_string = " time /home/mipt-user/torus-allreduce/local/packages/OpenMPI-4.1.6/bin/mpirun -x {} -mca plm_rsh_no_tree_spawn 1 --map-by slot -mca btl vader,self --hostfile {}  -mca orte_base_help_aggregate 0  --oversubscribe -np {} {}".format(
        allocation_policy, args.hostfile, node_num, sstsim_path
    )
    print(slim_string + " " + launch_string)
    os.system(slim_string + " " + launch_string)


def run_cluster(args, launch_string, size, bench, topo):

    # Special case if we are running on SlimFly
    if args.env == "slimfly":
        return run_slim(args, launch_string, bench, topo)

    allocation_policy = allocate_logic(bench, topo)

    my_sbatch_cont = [
        "#!/bin/bash\n",
        "#SBATCH -N {}\n".format(args.nodes),
        "#SBATCH -n {}\n".format(args.nodes),
        "#SBATCH --time=03:59:59\n",
        "#SBATCH -A g34\n",
        "#SBATCH --mem={}\n".format(args.mem),
        "#SBATCH --cpus-per-task={}\n".format(args.cpus_per_task),
        "#SBATCH -C mc\n",
        "#SBATCH --output=logs/slurm-%A.out\n",
        "# Load the module environment suitable for the job\n",
        "module load openmpi\n",
        "{}\n".format(allocation_policy),
        "# And finally run the job\n",
    ]

    if args.env == "ault":
        my_sbatch_cont = [x for x in my_sbatch_cont if x != "#SBATCH -C mc\n"]
        my_sbatch_cont = [x for x in my_sbatch_cont if x != "#SBATCH -A g34\n"]

    my_sbatch_cont.append(
        "SST_NO_MEM=1 srun --mem={} -N {} -n {} --cpus-per-task={} sstsim.x ".format(
            args.mem, args.nodes, args.nodes, args.cpus_per_task
        )
        + launch_string
    )
    file_name = "launch{}_{}_{}".format(topo, bench, size)
    with open(sbatch_folder + "/" + file_name, "w") as file:
        file.writelines(my_sbatch_cont)

    os.system("sbatch " + sbatch_folder + "/" + file_name)
    time.sleep(2)


def run_sst(
    args,
    topo,
    count,
    bench,
    shape,
    motif_file,
    out_file,
    motif_ports=1,
    motif_dimensions_sizes=None,
):
    ember_load = ember_load_folder + "emberLoad.py"
    out_file = resolve_output_path(out_file, bench, args)

    dimensions = 1
    if topo == "hx4":
        launch_string = '--num-threads={} --model-options="--param="nic:module=merlin.reorderlinkcontrol" --topo=hx --boardShape=4x4 --globalShape={} --fatTreeShape=1:1,64 --hostsPerRtr=1 --netBW={} --loadFile={}" {} > {}'.format(
            str(args.num_threads), shape, args.netBW, motif_file, ember_load, out_file
        )
        dimensions = 2
    elif topo == "hx2":
        launch_string = '--num-threads={} --model-options="--param="nic:module=merlin.reorderlinkcontrol" --topo=hx --boardShape=2x2 --globalShape={} --fatTreeShape=1:1,64 --hostsPerRtr=1 --netBW={} --loadFile={}" {} > {}'.format(
            str(args.num_threads), shape, args.netBW, motif_file, ember_load, out_file
        )
        dimensions = 2
    elif topo == "hx1":
        launch_string = '--num-threads={} --model-options="--param="nic:module=merlin.reorderlinkcontrol" --topo=hx --boardShape=1x1 --globalShape={} --fatTreeShape=1:1,64 --hostsPerRtr=1 --netBW={} --loadFile={}" {} > {}'.format(
            str(args.num_threads), shape, args.netBW, motif_file, ember_load, out_file
        )
        dimensions = 2
    elif topo == "hyperx":
        # host/NIC 1000000Gb/s; fabric hyperx links = netBW (default 400Gb/s).
        # --hostBW=400 нужен только для spineleaf, не для hyperx/torus.
        host_bw = args.hostBW if args.hostBW else "1000000Gb/s"
        model_opts = (
            '--param="nic:module=merlin.reorderlinkcontrol" '
            f'--param="merlin:hyperx.shape={shape}" '
            '--param="merlin:hyperx.width=1x1" '
            '--param="merlin:hyperx.local_ports=1" '
            f'--param="merlin:link_bw:host={host_bw}" '
            f'--param="merlin:link_bw:hyperx={args.netBW}" '
            f'--param="nic:link_bw={host_bw}"'
        )
        launch_string = (
            f'--num-threads={args.num_threads} '
            f'--model-options="{model_opts} '
            f'--topo=hyperx --shape={shape} --hostsPerRtr=1 '
            f'--netBW={args.netBW} --loadFile={motif_file}" '
            f'{ember_load} > {out_file}'
        )
        dimensions = 2
    elif topo == "spineleaf":
        # host↔leaf и leaf↔spine одинаковые: netBW (default 400Gb/s),
        # либо явный --hostBW если нужен другой host-линк.
        host_bw = args.hostBW if args.hostBW else args.netBW
        model_opts = (
            '--param="nic:module=merlin.reorderlinkcontrol" '
            f'--param="merlin:fattree.shape={shape}" '
            f'--param="merlin:link_bw={args.netBW}" '
            f'--param="merlin:link_bw:host={host_bw}" '
            f'--param="merlin:link_bw:network0={args.netBW}" '
            f'--param="nic:link_bw={host_bw}"'
        )
        launch_string = (
            f'--num-threads={args.num_threads} '
            f'--model-options="{model_opts} '
            f'--topo=fattree --shape={shape} --netBW={args.netBW} '
            f'--loadFile={motif_file}" '
            f'{ember_load} > {out_file}'
        )
    elif "fattree" in topo:
        launch_string = '--num-threads={} --model-options="--param="nic:module=merlin.reorderlinkcontrol" --topo=fattree --shape={} --netBW={} --loadFile={}" {} > {}'.format(
            str(args.num_threads), shape, args.netBW, motif_file, ember_load, out_file
        )
    elif topo == "torus":
        model_opts = '--param="nic:module=merlin.reorderlinkcontrol"'
        if args.hostBW or args.torusBW:
            model_opts += f' --param="merlin:torus.shape={shape}"'
            model_opts += f' --param="merlin:torus.width=1x1"'
            model_opts += f' --param="merlin:torus.local_ports=1"'
            if args.hostBW:
                model_opts += f' --param="merlin:link_bw:host={args.hostBW}"'
            if args.torusBW:
                model_opts += f' --param="merlin:link_bw:torus={args.torusBW}"'
        link_lat = getattr(args, "linkLat", "") or ""
        input_latency = getattr(args, "inputLatency", "") or ""
        output_latency = getattr(args, "outputLatency", "") or ""
        if link_lat or input_latency or output_latency:
            if not (link_lat and input_latency and output_latency):
                sys.exit(
                    "torus hop latency requires linkLat, inputLatency "
                    "and outputLatency together"
                )
            # network: is copied into merlin after merlin: overrides.
            model_opts += (
                f' --param="network:link_lat={link_lat}"'
                f' --param="network:input_latency={input_latency}"'
                f' --param="network:output_latency={output_latency}"'
            )
            print(
                f"torus hop latency link_lat={link_lat} "
                f"input_latency={input_latency} "
                f"output_latency={output_latency}"
            )
        # Merlin torus_trees needs the route table at the network layer.
        # Do NOT pass --numVCs: emberLoad.py getopt does not accept it
        # ("option --numVCs not recognized"). torus_trees uses getVCsPerVN()=2.
        route_table = getattr(args, "route_table_file", "") or ""
        if route_table and bench == "Trees":
            model_opts += f' --routeTableFile={route_table}'
            num_vcs = num_vcs_for_trees_route_table(route_table, shape)
            print(
                f"torus_trees route_table={os.path.basename(route_table)} "
                f"(ecdg num_vcs={num_vcs} is analytical only; not an emberLoad flag)"
            )

        launch_string = (
            f'--num-threads={args.num_threads} '
            f'--model-options="{model_opts} '
            f' --topo=torus --shape={shape} --hostsPerRtr=1 '
            f' --netBW={args.netBW} --loadFile={motif_file}" '
            f'{ember_load} > {out_file}'
        )
        dimensions = shape.count("x") + 1
    elif topo == "dragonfly":
        # Dragonfly BW is per logical port group (unlike fattree/torus/hyperx):
        #   host   — NIC ↔ router
        #   group  — intra-group (local router↔router)
        #   global — inter-group
        # hr_router looks up link_bw:<group>; empty flags fall back to netBW.
        if args.num_threads > 8:
            args.num_threads = 8
        host_bw = args.hostBW if getattr(args, "hostBW", "") else args.netBW
        group_bw = getattr(args, "groupBW", "") or args.netBW
        global_bw = getattr(args, "globalBW", "") or args.netBW
        model_opts = (
            '--param="nic:module=merlin.reorderlinkcontrol" '
            '--param="merlin:dragonfly.global_route_mode=relative" '
            f'--param="merlin:link_bw={args.netBW}" '
            f'--param="merlin:link_bw:host={host_bw}" '
            f'--param="merlin:link_bw:group={group_bw}" '
            f'--param="merlin:link_bw:global={global_bw}" '
            f'--param="nic:link_bw={host_bw}"'
        )
        print(
            f"dragonfly BW host={host_bw} group={group_bw} global={global_bw}"
        )
        launch_string = (
            f'--num-threads={args.num_threads} '
            f'--model-options="{model_opts} '
            f'--topo=dragonfly --shape={shape} --netBW={args.netBW} '
            f'--loadFile={motif_file}" '
            f'{ember_load} > {out_file}'
        )
    else:
        print("Error, unknown topo")
        sys.exit(0)

    # Set defaultParams and patch shortMsgLength threshold for this count/bench.
    defaultParamsName = "defaultParams" + str(dimensions) + "D.py"
    default_params_src = ember_load_folder + defaultParamsName
    default_params_dst = ember_load_folder + "defaultParams.py"
    shutil.copyfile(default_params_src, default_params_dst)

    num_ranks = num_ranks_from_shape(shape)
    num_trees = num_trees_for_bench(bench, shape)
    ring_lanes = 4 if bench == "Rings" and dimensions == 2 else 1
    short_value = compute_short_value(
        bench,
        count,
        num_ranks,
        num_trees,
        ring_lanes=ring_lanes,
        ports=motif_ports,
        dimensions_sizes=motif_dimensions_sizes,
        tree_pack=getattr(args, "tree_pack", "") or "",
        tree_set=getattr(args, "tree_set", 0),
        trees_file=getattr(args, "trees_file", "") or "",
    )
    if (
        bench == "Trees"
        and hyperx_tree_scheme_from_route_table(
            getattr(args, "route_table_file", "")
        )
        in {"budget_b4", "cplex_b4_s96"}
        and "hyperx_32_32_"
        not in os.path.basename(getattr(args, "route_table_file", "") or "")
    ):
        # Conservative bound for grouped blocks in the B=4 schedule.
        short_value = max(short_value or 0, (count + 7) // 8)
    if short_value is not None:
        print(
            "shortValue={} (bench={}, count={}, ranks={}, trees={})".format(
                short_value, bench, count, num_ranks, num_trees
            )
        )
        apply_short_value_to_default_params(default_params_dst, short_value)

    if args.env == "local" or args.env == "":
        allocation_policy = allocate_logic(bench, topo)
        print("{} {} ".format(allocation_policy, sstsim_path) + launch_string)
        process = subprocess.Popen(
            ["{} {} ".format(allocation_policy, sstsim_path) + launch_string],
            shell=True,
            stderr=sys.stderr
        )
        process.wait()
    elif (
        args.env == "slurm"
        or args.env == "cluster"
        or args.env == "ault"
        or args.env == "slimfly"
        or args.env == "daint"
    ):
        run_cluster(args, launch_string, count, bench, topo)
    else:
        print("Error, unknown env")
        sys.exit(0)
