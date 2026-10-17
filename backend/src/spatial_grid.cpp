#include "dsa/spatial_grid.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace goride {
namespace dsa {

namespace {
constexpr double EARTH_RADIUS_M = 6371000.0;
}

SpatialGrid::SpatialGrid(double cell_size_degrees)
    : cell_size_(cell_size_degrees > 0.0 ? cell_size_degrees : 0.005) {}

double SpatialGrid::haversine_distance_m(double lat1, double lon1, double lat2, double lon2) {
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double dphi = (lat2 - lat1) * M_PI / 180.0;
    double dlambda = (lon2 - lon1) * M_PI / 180.0;

    double a = std::sin(dphi / 2.0) * std::sin(dphi / 2.0) +
               std::cos(phi1) * std::cos(phi2) * std::sin(dlambda / 2.0) * std::sin(dlambda / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return EARTH_RADIUS_M * c;
}

void SpatialGrid::build_index(const RoadGraph& graph) {
    grid_.clear();
    edge_catalog_.clear();
    edge_grid_.clear();
    total_nodes_ = 0;
    graph_ = &graph;

    for (const auto& [nid, node] : graph.nodes()) {
        if (!node.is_valid()) continue;
        auto [cx, cy] = get_cell_coords(node.lat, node.lon);
        int64_t key = make_cell_key(cx, cy);
        grid_[key].push_back(nid);
        ++total_nodes_;
    }

    for (const auto& [u_id, edge_list] : graph.adjacency_list()) {
        const Node* u_node = graph.get_node(u_id);
        if (!u_node || !u_node->is_valid()) continue;

        for (const auto& edge : edge_list) {
            const Node* v_node = graph.get_node(edge.v);
            if (!v_node || !v_node->is_valid()) continue;

            IndexedEdge ie;
            ie.edge = edge;
            ie.lat_u = u_node->lat;
            ie.lon_u = u_node->lon;
            ie.lat_v = v_node->lat;
            ie.lon_v = v_node->lon;

            size_t edge_idx = edge_catalog_.size();
            edge_catalog_.push_back(ie);

            auto [cx_u, cy_u] = get_cell_coords(ie.lat_u, ie.lon_u);
            auto [cx_v, cy_v] = get_cell_coords(ie.lat_v, ie.lon_v);

            int32_t min_cx = std::min(cx_u, cx_v);
            int32_t max_cx = std::max(cx_u, cx_v);
            int32_t min_cy = std::min(cy_u, cy_v);
            int32_t max_cy = std::max(cy_u, cy_v);

            for (int32_t cy = min_cy; cy <= max_cy; ++cy) {
                for (int32_t cx = min_cx; cx <= max_cx; ++cx) {
                    int64_t key = make_cell_key(cx, cy);
                    edge_grid_[key].push_back(edge_idx);
                }
            }
        }
    }
}

std::pair<double, double> SpatialGrid::project_point_on_segment(
    double q_lat, double q_lon,
    double a_lat, double a_lon,
    double b_lat, double b_lon,
    double& out_t
) {
    double mid_lat_rad = 0.5 * (a_lat + b_lat) * M_PI / 180.0;
    double cos_lat = std::cos(mid_lat_rad);

    double meters_per_deg = 111195.0;
    double dx_ab = (b_lon - a_lon) * meters_per_deg * cos_lat;
    double dy_ab = (b_lat - a_lat) * meters_per_deg;

    double dx_aq = (q_lon - a_lon) * meters_per_deg * cos_lat;
    double dy_aq = (q_lat - a_lat) * meters_per_deg;

    double seg_len_sq = dx_ab * dx_ab + dy_ab * dy_ab;
    if (seg_len_sq < 1e-8) {
        out_t = 0.0;
        return {a_lat, a_lon};
    }

    double dot = dx_aq * dx_ab + dy_aq * dy_ab;
    double t = dot / seg_len_sq;
    t = std::max(0.0, std::min(1.0, t));
    out_t = t;

    double proj_lat = a_lat + t * (b_lat - a_lat);
    double proj_lon = a_lon + t * (b_lon - a_lon);
    return {proj_lat, proj_lon};
}

std::vector<SnapEdgeResult> SpatialGrid::find_k_nearest_edges(
    double query_lat, double query_lon,
    size_t k,
    double max_snap_distance_m
) const {
    std::vector<SnapEdgeResult> results;
    if (k == 0) return results;

    if (!std::isfinite(query_lat) || !std::isfinite(query_lon) ||
        query_lat < -90.0 || query_lat > 90.0 ||
        query_lon < -180.0 || query_lon > 180.0) {
        return results;
    }

    if (!graph_ || edge_catalog_.empty()) {
        return results;
    }

    auto [center_x, center_y] = get_cell_coords(query_lat, query_lon);

    double meters_per_deg_lat = 111195.0;
    double meters_per_deg_lon = 111195.0 * std::max(0.1, std::cos(query_lat * M_PI / 180.0));

    int32_t radius_y = static_cast<int32_t>(std::ceil(max_snap_distance_m / (cell_size_ * meters_per_deg_lat))) + 1;
    int32_t radius_x = static_cast<int32_t>(std::ceil(max_snap_distance_m / (cell_size_ * meters_per_deg_lon))) + 1;

    std::vector<size_t> candidate_indices;
    std::unordered_set<size_t> seen_edges;

    for (int32_t cy = center_y - radius_y; cy <= center_y + radius_y; ++cy) {
        for (int32_t cx = center_x - radius_x; cx <= center_x + radius_x; ++cx) {
            int64_t key = make_cell_key(cx, cy);
            auto it = edge_grid_.find(key);
            if (it == edge_grid_.end()) continue;

            for (size_t edge_idx : it->second) {
                if (seen_edges.insert(edge_idx).second) {
                    candidate_indices.push_back(edge_idx);
                }
            }
        }
    }

    std::vector<SnapEdgeResult> evaluated;
    for (size_t idx : candidate_indices) {
        const auto& ie = edge_catalog_[idx];
        double t = 0.0;
        auto [snap_lat, snap_lon] = project_point_on_segment(
            query_lat, query_lon,
            ie.lat_u, ie.lon_u,
            ie.lat_v, ie.lon_v,
            t
        );

        double dist = haversine_distance_m(query_lat, query_lon, snap_lat, snap_lon);
        if (dist <= max_snap_distance_m) {
            SnapEdgeResult ser;
            ser.success = true;
            ser.edge = ie.edge;
            ser.snap_lat = snap_lat;
            ser.snap_lon = snap_lon;
            ser.distance_m = dist;
            ser.t = t;
            ser.dist_from_u_m = t * ie.edge.weight;
            ser.dist_to_v_m = (1.0 - t) * ie.edge.weight;
            evaluated.push_back(std::move(ser));
        }
    }

    std::sort(evaluated.begin(), evaluated.end(), [](const SnapEdgeResult& a, const SnapEdgeResult& b) {
        return a.distance_m < b.distance_m;
    });

    size_t count = std::min(k, evaluated.size());
    results.assign(evaluated.begin(), evaluated.begin() + count);
    return results;
}

std::vector<SnapResult> SpatialGrid::find_k_nearest_nodes(double query_lat, double query_lon,
                                                          size_t k,
                                                          double max_snap_distance_m) const {
    std::vector<SnapResult> results;
    if (k == 0) return results;

    // Validate coordinate range
    if (!std::isfinite(query_lat) || !std::isfinite(query_lon) ||
        query_lat < -90.0 || query_lat > 90.0 ||
        query_lon < -180.0 || query_lon > 180.0) {
        return results;
    }

    if (!graph_ || total_nodes_ == 0) {
        return results;
    }

    auto [center_x, center_y] = get_cell_coords(query_lat, query_lon);

    // Approximate meters per degree of latitude (~111,000m)
    // and longitude (~111,000 * cos(lat))
    double meters_per_deg_lat = 111000.0;
    double meters_per_deg_lon = 111000.0 * std::max(0.1, std::cos(query_lat * M_PI / 180.0));

    int32_t radius_y = static_cast<int32_t>(std::ceil(max_snap_distance_m / (cell_size_ * meters_per_deg_lat))) + 1;
    int32_t radius_x = static_cast<int32_t>(std::ceil(max_snap_distance_m / (cell_size_ * meters_per_deg_lon))) + 1;

    std::vector<std::pair<double, NodeId>> candidates;

    // Search cells in bounding window
    for (int32_t cy = center_y - radius_y; cy <= center_y + radius_y; ++cy) {
        for (int32_t cx = center_x - radius_x; cx <= center_x + radius_x; ++cx) {
            int64_t key = make_cell_key(cx, cy);
            auto it = grid_.find(key);
            if (it == grid_.end()) continue;

            for (NodeId nid : it->second) {
                const Node* node = graph_->get_node(nid);
                if (!node) continue;

                double d = haversine_distance_m(query_lat, query_lon, node->lat, node->lon);
                if (d <= max_snap_distance_m) {
                    candidates.emplace_back(d, nid);
                }
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    size_t count = std::min(k, candidates.size());
    results.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const Node* n = graph_->get_node(candidates[i].second);
        if (!n) continue;

        SnapResult sr;
        sr.success = true;
        sr.node_id = candidates[i].second;
        sr.lat = n->lat;
        sr.lon = n->lon;
        sr.distance_m = candidates[i].first;
        results.push_back(sr);
    }

    return results;
}

SnapResult SpatialGrid::find_nearest_node(double query_lat, double query_lon,
                                         double max_snap_distance_m) const {
    SnapResult result;

    // Validate coordinate range
    if (!std::isfinite(query_lat) || !std::isfinite(query_lon) ||
        query_lat < -90.0 || query_lat > 90.0 ||
        query_lon < -180.0 || query_lon > 180.0) {
        result.success = false;
        result.error_message = "Invalid GPS coordinates: latitude or longitude out of bounds.";
        return result;
    }

    if (!graph_ || total_nodes_ == 0) {
        result.success = false;
        result.error_message = "Spatial grid index is empty or road graph is not initialized.";
        return result;
    }

    auto candidates = find_k_nearest_nodes(query_lat, query_lon, 1, max_snap_distance_m);
    if (!candidates.empty()) {
        return candidates[0];
    }

    result.success = false;
    result.distance_m = std::numeric_limits<double>::infinity();
    result.error_message = "Coordinates (" + std::to_string(query_lat) + ", " +
                           std::to_string(query_lon) + ") are too far from the road network (threshold is " +
                           std::to_string(std::round(max_snap_distance_m)) + "m).";
    return result;
}

} // namespace dsa
} // namespace goride
