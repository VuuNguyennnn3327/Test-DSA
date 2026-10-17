#ifndef GORIDE_DSA_DIJKSTRA_HPP
#define GORIDE_DSA_DIJKSTRA_HPP

#include "road_graph.hpp"
#include <string>
#include <utility>
#include <vector>

namespace goride {
namespace dsa {

struct DijkstraResult {
    bool found{false};
    NodeId source{0};
    NodeId target{0};
    double total_distance{0.0};
    std::vector<NodeId> path_nodes;                      // Sequence of node IDs: [source, ..., target]
    std::vector<Edge> path_edges;                        // Sequence of directed edges traversed
    std::vector<std::pair<double, double>> path_geometry;// Sequence of coordinates: [(lat, lon)]
    size_t nodes_visited{0};                             // Number of settled nodes during search
    std::string error_message;

    bool is_empty() const { return !found || path_nodes.empty(); }
};

class DijkstraRouter {
public:
    explicit DijkstraRouter(const RoadGraph& graph) : graph_(graph) {}

    // Find shortest path between source and target using Dijkstra's algorithm
    DijkstraResult find_shortest_path(NodeId source, NodeId target) const;

    // Static convenience method
    static DijkstraResult find_shortest_path(const RoadGraph& graph, NodeId source, NodeId target) {
        DijkstraRouter router(graph);
        return router.find_shortest_path(source, target);
    }

private:
    const RoadGraph& graph_;
};

} // namespace dsa
} // namespace goride

#endif // GORIDE_DSA_DIJKSTRA_HPP
