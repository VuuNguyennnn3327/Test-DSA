#include "dsa/road_graph.hpp"
#include "utils/json.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

namespace goride {
namespace dsa {

const std::vector<Edge> RoadGraph::empty_edges_{};

void RoadGraph::clear() {
    nodes_.clear();
    adj_.clear();
    edge_count_ = 0;
}

bool RoadGraph::add_node(NodeId id, double lat, double lon) {
    Node node{id, lat, lon};
    return add_node(node);
}

bool RoadGraph::add_node(const Node& node) {
    if (!node.is_valid()) {
        return false;
    }
    nodes_[node.id] = node;
    if (adj_.find(node.id) == adj_.end()) {
        adj_[node.id] = {};
    }
    return true;
}

bool RoadGraph::has_node(NodeId id) const {
    return nodes_.find(id) != nodes_.end();
}

const Node* RoadGraph::get_node(NodeId id) const {
    auto it = nodes_.find(id);
    if (it != nodes_.end()) {
        return &(it->second);
    }
    return nullptr;
}

const std::vector<Edge>& RoadGraph::get_outgoing_edges(NodeId id) const {
    auto it = adj_.find(id);
    if (it != adj_.end()) {
        return it->second;
    }
    return empty_edges_;
}

bool RoadGraph::add_edge(const Edge& edge) {
    if (!edge.is_valid()) {
        return false;
    }
    if (!has_node(edge.u) || !has_node(edge.v)) {
        return false;
    }

    Edge directed_edge = edge;
    if (directed_edge.geometry.empty()) {
        const Node* u_node = get_node(edge.u);
        const Node* v_node = get_node(edge.v);
        if (u_node && v_node) {
            directed_edge.geometry.push_back({u_node->lat, u_node->lon});
            directed_edge.geometry.push_back({v_node->lat, v_node->lon});
        }
    }

    adj_[edge.u].push_back(std::move(directed_edge));
    ++edge_count_;
    return true;
}

bool RoadGraph::add_edge(NodeId u, NodeId v, double weight, int64_t way_id,
                         std::string name, std::string highway, bool oneway,
                         std::vector<std::pair<double, double>> geometry) {
    Edge edge;
    edge.u = u;
    edge.v = v;
    edge.weight = weight;
    edge.way_id = way_id;
    edge.name = std::move(name);
    edge.highway = std::move(highway);
    edge.oneway = oneway;
    edge.geometry = std::move(geometry);
    return add_edge(edge);
}

GraphStats RoadGraph::get_stats() const {
    GraphStats stats;
    stats.node_count = nodes_.size();
    stats.edge_count = edge_count_;
    for (const auto& [u, edges] : adj_) {
        for (const auto& e : edges) {
            if (e.oneway) {
                stats.oneway_edges++;
            } else {
                stats.twoway_edges++;
            }
        }
    }
    return stats;
}

LoadResult RoadGraph::load_from_json_string(const std::string& json_str) {
    LoadResult res;
    std::string err;
    auto root = utils::JsonValue::parse(json_str, err);
    if (!err.empty()) {
        res.success = false;
        res.error_messages.push_back("JSON parse error: " + err);
        return res;
    }

    if (!root.is_object()) {
        res.success = false;
        res.error_messages.push_back("Root element is not a JSON object");
        return res;
    }

    clear();

    // 1. Ingest Nodes
    if (root.has_key("nodes")) {
        const auto& nodes_obj = root["nodes"].as_object();
        for (const auto& [nid_str, coord_val] : nodes_obj) {
            try {
                NodeId nid = std::stoll(nid_str);
                if (coord_val.is_array() && coord_val.as_array().size() >= 2) {
                    double lat = coord_val[0].as_double();
                    double lon = coord_val[1].as_double();
                    if (add_node(nid, lat, lon)) {
                        res.nodes_loaded++;
                    } else {
                        res.malformed_records++;
                        res.error_messages.push_back("Invalid coordinates for node: " + nid_str);
                    }
                } else {
                    res.malformed_records++;
                    res.error_messages.push_back("Malformed coordinate array for node: " + nid_str);
                }
            } catch (...) {
                res.malformed_records++;
                res.error_messages.push_back("Failed to parse node ID: " + nid_str);
            }
        }
    } else {
        res.error_messages.push_back("Warning: 'nodes' key not found in JSON");
    }

    // 2. Ingest Edges
    if (root.has_key("edges")) {
        const auto& edges_arr = root["edges"].as_array();
        for (size_t i = 0; i < edges_arr.size(); ++i) {
            const auto& e_val = edges_arr[i];
            if (!e_val.is_object()) {
                res.malformed_records++;
                continue;
            }

            if (!e_val.has_key("u") || !e_val.has_key("v") || !e_val.has_key("distance_m")) {
                res.malformed_records++;
                res.error_messages.push_back("Edge index " + std::to_string(i) + " missing u/v/distance_m");
                continue;
            }

            NodeId u = e_val["u"].as_int64();
            NodeId v = e_val["v"].as_int64();
            double dist = e_val["distance_m"].as_double();

            if (!std::isfinite(dist) || dist <= 0.0) {
                res.non_positive_weight_edges++;
                res.error_messages.push_back("Edge " + std::to_string(u) + " -> " + std::to_string(v) +
                                            " has non-positive or non-finite weight: " + std::to_string(dist));
                continue;
            }

            if (!has_node(u) || !has_node(v)) {
                res.missing_node_references++;
                res.error_messages.push_back("Edge " + std::to_string(u) + " -> " + std::to_string(v) +
                                            " references uncataloged vertex");
                continue;
            }

            Edge edge;
            edge.u = u;
            edge.v = v;
            edge.weight = dist;
            edge.way_id = e_val["way_id"].as_int64(0);
            edge.name = e_val["name"].as_string();
            edge.highway = e_val["highway"].as_string();
            edge.oneway = e_val["oneway"].as_bool(false);

            if (add_edge(edge)) {
                res.edges_loaded++;
            } else {
                res.malformed_records++;
            }
        }
    } else {
        res.error_messages.push_back("Warning: 'edges' key not found in JSON");
    }

    res.success = (res.nodes_loaded > 0);
    return res;
}

LoadResult RoadGraph::load_from_json_file(const std::string& filepath) {
    LoadResult res;
    std::ifstream fin(filepath, std::ios::binary);
    if (!fin.is_open()) {
        res.success = false;
        res.error_messages.push_back("Cannot open file: " + filepath);
        return res;
    }

    std::ostringstream ss;
    ss << fin.rdbuf();
    return load_from_json_string(ss.str());
}

} // namespace dsa
} // namespace goride
