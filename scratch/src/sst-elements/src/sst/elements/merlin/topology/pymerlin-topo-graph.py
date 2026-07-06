#!/usr/bin/env python

import sst
from sst.merlin.base import *


class topoGraph(Topology):
    """
    Generic undirected graph topology.

    graph_file format:
        # comments and blank lines are ignored
        <num_routers> <num_edges>
        <src_router> <dst_router>
        <src_router> <dst_router>
        ...

    Port layout must match topology/graph.cc:
        ports [0, hosts_per_router) are endpoint ports
        remaining ports are router-router links ordered by sorted adjacency
    """

    def __init__(self):
        Topology.__init__(self)
        self._declareClassVariables(["link_latency", "host_link_latency", "bundleEndpoints"])
        self._declareParams("main", ["graph_file", "hosts_per_router", "total_routers", "num_vcs"])
        self.total_routers = 0
        self.num_vcs = 1
        self._subscribeToPlatformParamSet("topology")

    def getName(self):
        return "Graph"

    def getRouterNameForId(self, rtr_id):
        return "rtr_%d" % rtr_id

    def findRouterById(self, rtr_id):
        return sst.findComponentByName(self.getRouterNameForId(rtr_id))

    def getNumNodes(self):
        num_routers, _edges = self._load_graph()
        return num_routers * int(self.hosts_per_router)

    def _load_graph(self):
        records = []

        with open(self.graph_file) as fp:
            for line in fp:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue

                parts = line.split()
                if len(parts) < 2:
                    continue

                records.append((int(parts[0]), int(parts[1])))

        if not records:
            raise Exception("topoGraph: graph_file is empty")

        num_routers, declared_edges = records[0]
        if num_routers <= 0:
            raise Exception("topoGraph: first line must be '<num_routers> <num_edges>'")

        edges = []
        for u, v in records[1:]:
            if u < 0 or v < 0 or u >= num_routers or v >= num_routers:
                raise Exception("topoGraph: invalid edge (%d, %d) for %d routers" % (u, v, num_routers))
            if u == v:
                continue
            if u > v:
                u, v = v, u
            edges.append((u, v))

        edges = sorted(set(edges))
        if declared_edges != len(edges):
            print("topoGraph: declared %d edges, loaded %d unique non-self edges" % (declared_edges, len(edges)))

        return num_routers, edges

    def _build_adjacency(self, num_routers, edges):
        adj = [[] for _ in range(num_routers)]
        for u, v in edges:
            adj[u].append(v)
            adj[v].append(u)

        for neighbors in adj:
            neighbors.sort()

        return adj

    def _build_impl(self, endpoint):
        if self.host_link_latency is None:
            self.host_link_latency = self.link_latency

        num_routers, edges = self._load_graph()
        hosts_per_router = int(self.hosts_per_router)
        adj = self._build_adjacency(num_routers, edges)

        self.total_routers = num_routers

        routers = {}
        for rtr_id in range(num_routers):
            radix = hosts_per_router + len(adj[rtr_id])
            rtr = self._instanceRouter(radix, rtr_id)

            topo = rtr.setSubComponent(self.router.getTopologySlotName(), "merlin.graph")
            self._applyStatisticsSettings(topo)
            topo.addParams(self._getGroupParams("main"))

            routers[rtr_id] = rtr

        for rtr_id in range(num_routers):
            for local_id in range(hosts_per_router):
                node_id = (rtr_id * hosts_per_router) + local_id
                ep, port_name = endpoint.build(node_id, {})

                if ep:
                    nic_link = sst.Link("nic_%d_%d" % (rtr_id, local_id))
                    if self.bundleEndpoints:
                        nic_link.setNoCut()
                    nic_link.connect(
                        (ep, port_name, self.host_link_latency),
                        (routers[rtr_id], "port%d" % local_id, self.host_link_latency)
                    )

        neighbor_index = [{} for _ in range(num_routers)]
        for rtr_id in range(num_routers):
            for idx, neighbor in enumerate(adj[rtr_id]):
                neighbor_index[rtr_id][neighbor] = idx

        for u, v in edges:
            port_u = hosts_per_router + neighbor_index[u][v]
            port_v = hosts_per_router + neighbor_index[v][u]

            link = sst.Link("link_%d_%d" % (u, v))
            link.connect(
                (routers[u], "port%d" % port_u, self.link_latency),
                (routers[v], "port%d" % port_v, self.link_latency)
            )
