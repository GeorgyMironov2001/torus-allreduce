import sst

NET = dict(
    link_bw="400Gb/s", xbar_bw="3200Gb/s", flitSize="256B",
    input_latency="150ns", output_latency="150ns", link_lat="100ns",
    in_buf="64KB", out_buf="64KB",
)

N = 8           # 1xN тор
BYTES = 12          # 12 или 12000
MODE = "duplex"    # "duplex" или "pingpong"


def mk_router(i):
    r = sst.Component(f"r{i}", "merlin.hr_router")
    r.addParams({
        "id": str(i),
        "num_ports": 3,                          # 2 сетевых + 1 локальный
        "xbar_bw": NET["xbar_bw"],
        "link_bw": NET["link_bw"],
        "flit_size": NET["flitSize"],
        "input_buf_size": NET["in_buf"],
        "output_buf_size": NET["out_buf"],
        "input_latency": NET["input_latency"],
        "output_latency": NET["output_latency"],
    })
    topo = r.setSubComponent("topology", "merlin.torus")  # Merlin torus
    topo.addParams({
        "shape": f"1x{N}",     # тор 1×N
        "width": "1x1",        # по одной «полосе» на измерение
        "local_ports": 1,      # один локальный порт на роутер
        "local_port": 0,       # NIC сидит на port0
    })
    return r


eng = []
nic = []
for r in (0, 1):
    e = sst.Component(f"e{r}", "ember.EmberEngine")
    e.addParams({
        "motif_count": 1,
        "motif_0": "ember.duplexpp",
        "motif_0.count_bytes": BYTES,
        "motif_0.mode": MODE,
        "motif_0.datatype": "FLOAT",
        "rank": r, "size": 2,
        "os.module": "firefly.hades",
        "api.0.module": "firefly.hadesMP",
    })
    os = e.setSubComponent("OS", "firefly.hades")
    proto = os.setSubComponent("proto", "firefly.CtrlMsgProto")
    process = proto.setSubComponent("process", "firefly.ctrlMsg")
    process.addParams({
        "shortMsgLength": 1048576,          # 2**20
        "matchDelay_ns": 1,
        "sendAckDelay_ns": 0,

        "txSetupMod": "firefly.LatencyMod",
        "rxSetupMod": "firefly.LatencyMod",
        "txMemcpyMod": "firefly.LatencyMod",
        "rxMemcpyMod": "firefly.LatencyMod",

        "txSetupModParams.base": "1ns",
        "rxSetupModParams.base": "0ns",
        "txMemcpyModParams.op": "Mult",
        "txMemcpyModParams.range.0": "0-:0ps",
        "rxMemcpyModParams.range.0": "0-:0ps",
    })
    loop = sst.Link(f"loop{r}")
    loop.connect((process, "loop", "1ns"), (process, "loop", "1ns"))

    n = e.setSubComponent("nic", "merlin.linkcontrol")
    n.addParams({
        "module": "merlin.linkcontrol",
        "link_bw": NET["link_bw"],
        "input_buf_size": NET["in_buf"],
        "output_buf_size": NET["out_buf"],
        "flitSize": NET["flitSize"],
        "packetSize": "8192B",
    })
    eng.append(e)
    nic.append(n)

# Роутеры тора 1×N (Merlin torus topology внутри)
rtrs = [mk_router(i) for i in range(N)]

# Подключаем NIC к локальным портам крайних роутеров
sst.Link("l_e0").connect(
    (nic[0], "rtr_port", "1ns"), (rtrs[0],    "port0", "1ns"))
sst.Link("l_e1").connect(
    (nic[1], "rtr_port", "1ns"), (rtrs[N-1], "port0", "1ns"))

# Сетевые связи (соседние и замыкающая) — по портам 1 и 2


def L(name, A, pa, B, pb):
    sst.Link(name).connect((A, pa, NET["link_lat"]), (B, pb, NET["link_lat"]))


for i in range(N):
    L(f"l{i}_{(i+1)%N}", rtrs[i], "port1", rtrs[(i+1) % N], "port2")

sst.setProgramOption("timebase", "1ps")
