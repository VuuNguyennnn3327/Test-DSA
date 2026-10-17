#ifndef GORIDE_DSA_ROUTE_SERVICE_HPP
#define GORIDE_DSA_ROUTE_SERVICE_HPP

#include "dsa/dijkstra.hpp"
#include "dsa/road_graph.hpp"
#include "dsa/spatial_grid.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace goride {
namespace dsa {

enum class RouteComputeStatus {
    SUCCESS,
    GRAPH_EMPTY,
    INVALID_COORDINATES,
    OUT_OF_BOUNDS_COORDINATES,
    ORIGIN_SNAP_FAILED,
    DESTINATION_SNAP_FAILED,
    ROUTE_NOT_FOUND
};

struct RouteComputeResult {
    bool success{false};
    RouteComputeStatus status{RouteComputeStatus::ROUTE_NOT_FOUND};
    std::string code;
    std::string error;
    double distance_m{0.0};
    double duration_s{0.0};
    double computation_time_ms{0.0};
    size_t nodes_visited{0};
    std::vector<NodeId> path_nodes;
    std::vector<std::pair<double, double>> path_geometry; // (lat, lon)

    // Snapping metadata
    double snap_orig_lat{0.0};
    double snap_orig_lon{0.0};
    double snap_orig_dist_m{0.0};
    std::string snap_orig_node_id;

    double snap_dest_lat{0.0};
    double snap_dest_lon{0.0};
    double snap_dest_dist_m{0.0};
    std::string snap_dest_node_id;
};

/**
 * High-performance, reentrant, thread-safe route calculation service.
 * Snaps origin and destination WGS84 coordinates to motorable road edges
 * with orthogonal centerline projection (fallback to nearest vertex).
 * Computes shortest path distance via Dijkstra router.
 *
 * graph and grid must remain immutable/const during computation.
 */
inline RouteComputeResult compute_shortest_route(
    const RoadGraph& graph,
    const SpatialGrid& grid,
    double from_lat, double from_lon,
    double to_lat, double to_lon,
    double max_snap = 1000.0
) {
    RouteComputeResult result;

    // 1. Validate coordinate validity
    if (std::isnan(from_lat) || std::isnan(from_lon) || std::isnan(to_lat) || std::isnan(to_lon) ||
        std::isinf(from_lat) || std::isinf(from_lon) || std::isinf(to_lat) || std::isinf(to_lon) ||
        (from_lat == 0.0 && from_lon == 0.0) || (to_lat == 0.0 && to_lon == 0.0)) {
        result.success = false;
        result.status = RouteComputeStatus::INVALID_COORDINATES;
        result.code = "INVALID_COORDINATES";
        result.error = "Coordinates must be valid non-zero floating point numbers.";
        return result;
    }

    if (from_lat < -90.0 || from_lat > 90.0 || to_lat < -90.0 || to_lat > 90.0 ||
        from_lon < -180.0 || from_lon > 180.0 || to_lon < -180.0 || to_lon > 180.0) {
        result.success = false;
        result.status = RouteComputeStatus::OUT_OF_BOUNDS_COORDINATES;
        result.code = "OUT_OF_BOUNDS_COORDINATES";
        result.error = "Coordinates out of WGS84 range: lat [-90, 90], lon [-180, 180].";
        return result;
    }

    if (graph.node_count() == 0) {
        result.success = false;
        result.status = RouteComputeStatus::GRAPH_EMPTY;
        result.code = "GRAPH_EMPTY";
        result.error = "OSM road network dataset is not loaded.";
        return result;
    }

    if (max_snap <= 0.0) {
        max_snap = 1000.0;
    }

    // 2. Query nearest road edge candidates (centerline projection) and fallback vertices
    auto origin_edges = grid.find_k_nearest_edges(from_lat, from_lon, 5, max_snap);
    auto dest_edges = grid.find_k_nearest_edges(to_lat, to_lon, 5, max_snap);

    auto origin_nodes = grid.find_k_nearest_nodes(from_lat, from_lon, 5, max_snap);
    auto dest_nodes = grid.find_k_nearest_nodes(to_lat, to_lon, 5, max_snap);

    if (origin_edges.empty() && origin_nodes.empty()) {
        result.success = false;
        result.status = RouteComputeStatus::ORIGIN_SNAP_FAILED;
        result.code = "ORIGIN_SNAP_FAILED";
        result.error = "Origin location (" + std::to_string(from_lat) + ", " + std::to_string(from_lon) +
                       ") is farther than " + std::to_string(static_cast<int>(max_snap)) + "m from any motorable OSM road.";
        return result;
    }

    if (dest_edges.empty() && dest_nodes.empty()) {
        result.success = false;
        result.status = RouteComputeStatus::DESTINATION_SNAP_FAILED;
        result.code = "DESTINATION_SNAP_FAILED";
        result.error = "Destination location (" + std::to_string(to_lat) + ", " + std::to_string(to_lon) +
                       ") is farther than " + std::to_string(static_cast<int>(max_snap)) + "m from any motorable OSM road.";
        return result;
    }

    // 3. Shortest path routing with Dijkstra & Edge Snapping
    auto start_route = std::chrono::high_resolution_clock::now();

    bool route_found = false;
    double best_total_cost = std::numeric_limits<double>::infinity();
    double best_distance_m = 0.0;
    size_t best_nodes_visited = 0;
    std::vector<NodeId> best_path_nodes;
    std::vector<std::pair<double, double>> best_path_geometry;

    double snap_orig_lat = 0.0, snap_orig_lon = 0.0, snap_orig_dist = 0.0;
    std::string snap_orig_node_id;
    double snap_dest_lat = 0.0, snap_dest_lon = 0.0, snap_dest_dist = 0.0;
    std::string snap_dest_node_id;

    // Evaluate edge-based candidate pairs
    if (!origin_edges.empty() && !dest_edges.empty()) {
        size_t max_oe = std::min<size_t>(3, origin_edges.size());
        size_t max_de = std::min<size_t>(3, dest_edges.size());

        for (size_t oi = 0; oi < max_oe; ++oi) {
            const auto& oe = origin_edges[oi];
            for (size_t di = 0; di < max_de; ++di) {
                const auto& de = dest_edges[di];

                // Case A: Same directed edge, origin precedes destination
                if (oe.edge.u == de.edge.u && oe.edge.v == de.edge.v && oe.t <= de.t) {
                    double drive_dist = (de.t - oe.t) * oe.edge.weight;
                    double total_cost = drive_dist + oe.distance_m + de.distance_m;
                    if (total_cost < best_total_cost) {
                        best_total_cost = total_cost;
                        best_distance_m = drive_dist;
                        route_found = true;
                        best_nodes_visited = 2;
                        best_path_nodes = {oe.edge.u, oe.edge.v};
                        best_path_geometry = {{oe.snap_lat, oe.snap_lon}, {de.snap_lat, de.snap_lon}};
                        snap_orig_lat = oe.snap_lat; snap_orig_lon = oe.snap_lon; snap_orig_dist = oe.distance_m;
                        snap_orig_node_id = std::to_string(oe.edge.u) + "_" + std::to_string(oe.edge.v);
                        snap_dest_lat = de.snap_lat; snap_dest_lon = de.snap_lon; snap_dest_dist = de.distance_m;
                        snap_dest_node_id = std::to_string(de.edge.u) + "_" + std::to_string(de.edge.v);
                    }
                    continue;
                }

                // Case B: General routing between edge endpoints
                DijkstraResult d_res;
                if (oe.edge.v == de.edge.u) {
                    d_res.found = true;
                    d_res.total_distance = 0.0;
                    d_res.nodes_visited = 1;
                    const auto* mid_node = graph.get_node(oe.edge.v);
                    if (mid_node) {
                        d_res.path_geometry = {{mid_node->lat, mid_node->lon}};
                        d_res.path_nodes = {oe.edge.v};
                    }
                } else {
                    d_res = DijkstraRouter::find_shortest_path(graph, oe.edge.v, de.edge.u);
                }

                if (d_res.found) {
                    double drive_dist = oe.dist_to_v_m + d_res.total_distance + de.dist_from_u_m;
                    double total_cost = drive_dist + oe.distance_m + de.distance_m;
                    if (total_cost < best_total_cost) {
                        best_total_cost = total_cost;
                        best_distance_m = drive_dist;
                        route_found = true;
                        best_nodes_visited = d_res.nodes_visited + 2;

                        best_path_nodes.clear();
                        best_path_nodes.push_back(oe.edge.u);
                        for (auto nid : d_res.path_nodes) {
                            if (best_path_nodes.empty() || best_path_nodes.back() != nid) {
                                best_path_nodes.push_back(nid);
                            }
                        }
                        if (best_path_nodes.empty() || best_path_nodes.back() != de.edge.v) {
                            best_path_nodes.push_back(de.edge.v);
                        }

                        best_path_geometry.clear();
                        best_path_geometry.push_back({oe.snap_lat, oe.snap_lon});
                        for (const auto& pt : d_res.path_geometry) {
                            if (best_path_geometry.empty() ||
                                std::abs(best_path_geometry.back().first - pt.first) > 1e-6 ||
                                std::abs(best_path_geometry.back().second - pt.second) > 1e-6) {
                                best_path_geometry.push_back(pt);
                            }
                        }
                        if (best_path_geometry.empty() ||
                            std::abs(best_path_geometry.back().first - de.snap_lat) > 1e-6 ||
                            std::abs(best_path_geometry.back().second - de.snap_lon) > 1e-6) {
                            best_path_geometry.push_back({de.snap_lat, de.snap_lon});
                        }

                        snap_orig_lat = oe.snap_lat; snap_orig_lon = oe.snap_lon; snap_orig_dist = oe.distance_m;
                        snap_orig_node_id = std::to_string(oe.edge.u) + "_" + std::to_string(oe.edge.v);
                        snap_dest_lat = de.snap_lat; snap_dest_lon = de.snap_lon; snap_dest_dist = de.distance_m;
                        snap_dest_node_id = std::to_string(de.edge.u) + "_" + std::to_string(de.edge.v);
                    }
                }
            }
        }
    }

    // Fallback: Node-based candidate pairs
    if (!route_found && !origin_nodes.empty() && !dest_nodes.empty()) {
        size_t max_o = std::min<size_t>(3, origin_nodes.size());
        size_t max_d = std::min<size_t>(3, dest_nodes.size());
        for (size_t oi = 0; oi < max_o; ++oi) {
            for (size_t di = 0; di < max_d; ++di) {
                auto cand_res = DijkstraRouter::find_shortest_path(graph, origin_nodes[oi].node_id, dest_nodes[di].node_id);
                if (cand_res.found) {
                    double total_cost = origin_nodes[oi].distance_m + cand_res.total_distance + dest_nodes[di].distance_m;
                    if (total_cost < best_total_cost) {
                        best_total_cost = total_cost;
                        best_distance_m = cand_res.total_distance;
                        route_found = true;
                        best_nodes_visited = cand_res.nodes_visited;
                        best_path_nodes = cand_res.path_nodes;
                        best_path_geometry = cand_res.path_geometry;
                        snap_orig_lat = origin_nodes[oi].lat; snap_orig_lon = origin_nodes[oi].lon; snap_orig_dist = origin_nodes[oi].distance_m;
                        snap_orig_node_id = std::to_string(origin_nodes[oi].node_id);
                        snap_dest_lat = dest_nodes[di].lat; snap_dest_lon = dest_nodes[di].lon; snap_dest_dist = dest_nodes[di].distance_m;
                        snap_dest_node_id = std::to_string(dest_nodes[di].node_id);
                    }
                }
            }
        }
    }

    auto end_route = std::chrono::high_resolution_clock::now();
    double route_time_ms = std::chrono::duration<double, std::milli>(end_route - start_route).count();

    if (!route_found) {
        result.success = false;
        result.status = RouteComputeStatus::ROUTE_NOT_FOUND;
        result.code = "ROUTE_NOT_FOUND";
        result.error = "No valid motorable route found between coordinates.";
        result.nodes_visited = best_nodes_visited;
        result.computation_time_ms = route_time_ms;
        return result;
    }

    result.success = true;
    result.status = RouteComputeStatus::SUCCESS;
    result.distance_m = best_distance_m;
    result.duration_s = best_distance_m / 6.944; // ~25 km/h urban average speed
    result.computation_time_ms = route_time_ms;
    result.nodes_visited = best_nodes_visited;
    result.path_nodes = std::move(best_path_nodes);
    result.path_geometry = std::move(best_path_geometry);

    result.snap_orig_lat = snap_orig_lat;
    result.snap_orig_lon = snap_orig_lon;
    result.snap_orig_dist_m = snap_orig_dist;
    result.snap_orig_node_id = snap_orig_node_id;

    result.snap_dest_lat = snap_dest_lat;
    result.snap_dest_lon = snap_dest_lon;
    result.snap_dest_dist_m = snap_dest_dist;
    result.snap_dest_node_id = snap_dest_node_id;

    return result;
}

} // namespace dsa
} // namespace goride

#endif // GORIDE_DSA_ROUTE_SERVICE_HPP
