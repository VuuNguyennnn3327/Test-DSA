#ifndef GORIDE_DSA_ROAD_GRAPH_HPP
#define GORIDE_DSA_ROAD_GRAPH_HPP

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace goride {
namespace dsa {

using NodeId = int64_t;

struct Node {
    NodeId id{0};
    double lat{0.0};
    double lon{0.0};

    bool is_valid() const {
        return id != 0 && std::isfinite(lat) && std::isfinite(lon) &&
               lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
    }
};

struct Edge {
    NodeId u{0};               // Source node ID
    NodeId v{0};               // Target node ID
    double weight{0.0};        // Positive distance in meters
    int64_t way_id{0};         // Parent OSM way ID
    std::string name;          // Street name
    std::string highway;       // Highway classification (e.g. primary, secondary)
    bool oneway{false};        // Whether parent segment was one-way
    std::vector<std::pair<double, double>> geometry; // Coordinates [(lat, lon)] along the segment

    bool is_valid() const {
        return u != 0 && v != 0 && std::isfinite(weight) && weight > 0.0;
    }
};

struct GraphStats {
    size_t node_count{0};
    size_t edge_count{0};
    size_t oneway_edges{0};
    size_t twoway_edges{0};
};

struct LoadResult {
    bool success{false};
    size_t nodes_loaded{0};
    size_t edges_loaded{0};
    size_t malformed_records{0};
    size_t missing_node_references{0};
    size_t non_positive_weight_edges{0};
    std::vector<std::string> error_messages;
};

class RoadGraph {
public:
    RoadGraph() = default;

    void clear();

    // Node management
    bool add_node(NodeId id, double lat, double lon);
    bool add_node(const Node& node);
    bool has_node(NodeId id) const;
    const Node* get_node(NodeId id) const;
    const std::unordered_map<NodeId, Node>& nodes() const { return nodes_; }

    // Edge management (directed)
    bool add_edge(const Edge& edge);
    bool add_edge(NodeId u, NodeId v, double weight, int64_t way_id = 0,
                  std::string name = "", std::string highway = "", bool oneway = false,
                  std::vector<std::pair<double, double>> geometry = {});
    const std::vector<Edge>& get_outgoing_edges(NodeId id) const;
    const std::unordered_map<NodeId, std::vector<Edge>>& adjacency_list() const { return adj_; }

    // Graph metrics
    size_t node_count() const { return nodes_.size(); }
    size_t edge_count() const { return edge_count_; }
    GraphStats get_stats() const;

    // JSON file ingestion conforming to Phase 2A.1 schema
    LoadResult load_from_json_file(const std::string& filepath);
    LoadResult load_from_json_string(const std::string& json_str);

private:
    std::unordered_map<NodeId, Node> nodes_;
    std::unordered_map<NodeId, std::vector<Edge>> adj_;
    size_t edge_count_{0};
    static const std::vector<Edge> empty_edges_;
};

} // namespace dsa
} // namespace goride

#endif // GORIDE_DSA_ROAD_GRAPH_HPP
