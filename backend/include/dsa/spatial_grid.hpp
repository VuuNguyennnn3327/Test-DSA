#ifndef GORIDE_DSA_SPATIAL_GRID_HPP
#define GORIDE_DSA_SPATIAL_GRID_HPP

#include "road_graph.hpp"
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace goride {
namespace dsa {

struct SnapResult {
    bool success{false};
    NodeId node_id{0};
    double lat{0.0};
    double lon{0.0};
    double distance_m{0.0}; // Distance from query coordinates to snapped node in meters
    std::string error_message;
};

struct SnapEdgeResult {
    bool success{false};
    Edge edge;                   // The road edge (u -> v)
    double snap_lat{0.0};        // Projected point on the road segment
    double snap_lon{0.0};        // Projected point on the road segment
    double distance_m{0.0};      // Orthogonal distance from query point to edge
    double t{0.0};               // Parameter [0, 1] along segment (0 = u, 1 = v)
    double dist_from_u_m{0.0};   // Distance along edge from u to projection point
    double dist_to_v_m{0.0};     // Distance along edge from projection point to v
    std::string error_message;
};

/**
 * Fast O(1) Spatial Hash Grid for snapping GPS coordinates
 * to the nearest motorable road network vertices and edges using Haversine distance.
 */
class SpatialGrid {
public:
    explicit SpatialGrid(double cell_size_degrees = 0.005); // ~550m grid cells

    // Build spatial hash index from a RoadGraph's node and edge catalog
    void build_index(const RoadGraph& graph);

    // Snap an input WGS84 coordinate to the closest graph node
    SnapResult find_nearest_node(double query_lat, double query_lon,
                                 double max_snap_distance_m = 1000.0) const;

    // Snap an input WGS84 coordinate to up to k closest graph nodes sorted by distance
    std::vector<SnapResult> find_k_nearest_nodes(double query_lat, double query_lon,
                                                 size_t k = 5,
                                                 double max_snap_distance_m = 1000.0) const;

    // Project a query point onto segment A -> B, returning projected (lat, lon) and parameter t in [0, 1]
    static std::pair<double, double> project_point_on_segment(
        double q_lat, double q_lon,
        double a_lat, double a_lon,
        double b_lat, double b_lon,
        double& out_t
    );

    // Snap an input WGS84 coordinate to up to k closest road edges within max_snap_distance_m
    std::vector<SnapEdgeResult> find_k_nearest_edges(
        double query_lat, double query_lon,
        size_t k = 5,
        double max_snap_distance_m = 1000.0
    ) const;

    // Calculate great-circle Haversine distance in meters
    static double haversine_distance_m(double lat1, double lon1, double lat2, double lon2);

    size_t indexed_nodes_count() const { return total_nodes_; }
    size_t indexed_edges_count() const { return edge_catalog_.size(); }
    bool is_empty() const { return total_nodes_ == 0; }

private:
    struct IndexedEdge {
        Edge edge;
        double lat_u{0.0};
        double lon_u{0.0};
        double lat_v{0.0};
        double lon_v{0.0};
    };

    double cell_size_;
    size_t total_nodes_{0};
    const RoadGraph* graph_{nullptr};

    // Maps 64-bit combined grid key (cell_x, cell_y) to list of NodeIds
    std::unordered_map<int64_t, std::vector<NodeId>> grid_;

    // Edge catalog and edge spatial index
    std::vector<IndexedEdge> edge_catalog_;
    std::unordered_map<int64_t, std::vector<size_t>> edge_grid_;

    int64_t make_cell_key(int32_t cell_x, int32_t cell_y) const {
        return (static_cast<int64_t>(cell_x) << 32) ^ (static_cast<int64_t>(cell_y) & 0xFFFFFFFFLL);
    }

    std::pair<int32_t, int32_t> get_cell_coords(double lat, double lon) const {
        int32_t cell_x = static_cast<int32_t>(std::floor(lon / cell_size_));
        int32_t cell_y = static_cast<int32_t>(std::floor(lat / cell_size_));
        return {cell_x, cell_y};
    }
};

} // namespace dsa
} // namespace goride

#endif // GORIDE_DSA_SPATIAL_GRID_HPP
