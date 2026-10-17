#include "dsa/dijkstra.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>

namespace goride {
namespace dsa {

namespace {

struct PQElement {
    NodeId u{0};
    double dist{0.0};
};

struct CompareDist {
    bool operator()(const PQElement& a, const PQElement& b) const {
        return a.dist > b.dist; // Min-heap: smallest distance has highest priority
    }
};

} // namespace

DijkstraResult DijkstraRouter::find_shortest_path(NodeId source, NodeId target) const {
    DijkstraResult res;
    res.source = source;
    res.target = target;

    // 1. Validate source and target presence in graph
    if (!graph_.has_node(source)) {
        res.found = false;
        res.error_message = "Source node " + std::to_string(source) + " does not exist in graph.";
        return res;
    }
    if (!graph_.has_node(target)) {
        res.found = false;
        res.error_message = "Target node " + std::to_string(target) + " does not exist in graph.";
        return res;
    }

    // 2. Trivial path: source is target
    if (source == target) {
        res.found = true;
        res.total_distance = 0.0;
        res.path_nodes.push_back(source);
        const Node* n = graph_.get_node(source);
        if (n) {
            res.path_geometry.push_back({n->lat, n->lon});
        }
        res.nodes_visited = 1;
        return res;
    }

    // 3. Dijkstra algorithm with min-heap priority queue
    std::unordered_map<NodeId, double> dist;
    std::unordered_map<NodeId, std::pair<NodeId, Edge>> predecessor;
    std::priority_queue<PQElement, std::vector<PQElement>, CompareDist> pq;

    dist[source] = 0.0;
    pq.push({source, 0.0});

    size_t settled_count = 0;
    bool reached = false;

    while (!pq.empty()) {
        auto [u, d] = pq.top();
        pq.pop();

        // Lazy deletion: skip if this entry is stale
        auto it_u = dist.find(u);
        if (it_u == dist.end() || d > it_u->second) {
            continue;
        }

        ++settled_count;

        // Target reached: can break early in single-pair shortest path
        if (u == target) {
            reached = true;
            break;
        }

        for (const auto& edge : graph_.get_outgoing_edges(u)) {
            // Strictly guard against invalid, non-positive or non-finite weights
            if (!std::isfinite(edge.weight) || edge.weight <= 0.0) {
                continue;
            }

            NodeId v = edge.v;
            double alt = d + edge.weight;

            auto it_v = dist.find(v);
            if (it_v == dist.end() || alt < it_v->second) {
                dist[v] = alt;
                predecessor[v] = {u, edge};
                pq.push({v, alt});
            }
        }
    }

    res.nodes_visited = settled_count;

    // 4. Check if target was reached
    if (!reached && dist.find(target) == dist.end()) {
        res.found = false;
        res.total_distance = std::numeric_limits<double>::infinity();
        res.error_message = "No directed path exists from " + std::to_string(source) +
                            " to " + std::to_string(target) + " under one-way restrictions.";
        return res;
    }

    // 5. Reconstruct path from predecessors: backtrack target -> source
    std::vector<NodeId> rev_nodes;
    std::vector<Edge> rev_edges;
    NodeId curr = target;
    rev_nodes.push_back(curr);

    while (curr != source) {
        auto pred_it = predecessor.find(curr);
        if (pred_it == predecessor.end()) {
            res.found = false;
            res.error_message = "Path reconstruction failed: broken predecessor chain at " + std::to_string(curr);
            return res;
        }
        rev_edges.push_back(pred_it->second.second);
        curr = pred_it->second.first;
        rev_nodes.push_back(curr);
    }

    std::reverse(rev_nodes.begin(), rev_nodes.end());
    std::reverse(rev_edges.begin(), rev_edges.end());

    // 6. Verify total weight strictly equals the sum of chosen edge weights
    double edge_sum = 0.0;
    for (const auto& e : rev_edges) {
        edge_sum += e.weight;
    }

    // 7. Assemble geometry in strict source -> target order
    std::vector<std::pair<double, double>> geometry;
    const Node* src_node = graph_.get_node(source);
    if (src_node) {
        geometry.push_back({src_node->lat, src_node->lon});
    }
    for (const auto& e : rev_edges) {
        const Node* v_node = graph_.get_node(e.v);
        if (v_node) {
            geometry.push_back({v_node->lat, v_node->lon});
        }
    }

    res.found = true;
    res.total_distance = edge_sum;
    res.path_nodes = std::move(rev_nodes);
    res.path_edges = std::move(rev_edges);
    res.path_geometry = std::move(geometry);

    return res;
}

} // namespace dsa
} // namespace goride
