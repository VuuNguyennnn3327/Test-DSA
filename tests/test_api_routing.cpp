#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "dsa/dijkstra.hpp"
#include "dsa/road_graph.hpp"
#include "dsa/spatial_grid.hpp"
#include "server/http_server.hpp"

using namespace goride::dsa;
using namespace goride::server;

namespace {

int total_tests = 0;
int passed_tests = 0;

void record_test(const std::string& name, bool condition, const std::string& details = "") {
    ++total_tests;
    std::cout << "[TEST " << std::setw(2) << total_tests << "] " << name << ": ";
    if (condition) {
        ++passed_tests;
        std::cout << "PASS";
    } else {
        std::cout << "FAIL";
    }
    if (!details.empty()) {
        std::cout << " (" << details << ")";
    }
    std::cout << std::endl;
}

std::string find_osm_file(const std::string& preferred_path) {
    std::vector<std::string> candidates = {
        preferred_path,
        "data/osm/processed/hcmc_roads.json",
        "../data/osm/processed/hcmc_roads.json",
        "../../data/osm/processed/hcmc_roads.json",
        "../../../data/osm/processed/hcmc_roads.json"
    };

    for (const auto& path : candidates) {
        if (path.empty()) continue;
        std::ifstream file(path);
        if (file.good()) {
            return path;
        }
    }
    return preferred_path;
}

// -------------------------------------------------------------
// SUITE 1: SpatialGrid & Haversine Unit Tests
// -------------------------------------------------------------
void run_suite_spatial_grid() {
    std::cout << "\n--- SUITE 1: SpatialGrid & Haversine Unit Tests ---" << std::endl;

    // TEST 1: Haversine distance identical coordinates returns 0.0
    double dist_zero = SpatialGrid::haversine_distance_m(10.7725, 106.6980, 10.7725, 106.6980);
    record_test("Haversine distance between identical coordinates is 0.0", std::abs(dist_zero) < 1e-6);

    // TEST 2: Haversine distance Ben Thanh to Notre Dame is ~818m (+-20m)
    double dist_bt_nd = SpatialGrid::haversine_distance_m(10.7725, 106.6980, 10.7798, 106.6990);
    record_test("Haversine distance Ben Thanh to Notre Dame is ~818m",
                dist_bt_nd >= 800.0 && dist_bt_nd <= 850.0,
                "calculated: " + std::to_string(dist_bt_nd) + "m");

    // TEST 3: Empty SpatialGrid returns success = false
    SpatialGrid empty_grid;
    auto empty_snap = empty_grid.find_nearest_node(10.7725, 106.6980, 1000.0);
    record_test("Empty SpatialGrid query returns success = false", !empty_snap.success);

    // TEST 4: Build SpatialGrid on small synthetic graph
    RoadGraph synthetic_graph;
    synthetic_graph.add_node(101, 10.7700, 106.6900);
    synthetic_graph.add_node(102, 10.7750, 106.6950);
    synthetic_graph.add_node(103, 10.7800, 106.7000);

    SpatialGrid small_grid(0.005);
    small_grid.build_index(synthetic_graph);
    record_test("SpatialGrid indexes synthetic nodes correctly", small_grid.indexed_nodes_count() == 3);

    // TEST 5: Query exact node coordinate finds exact node with dist ~ 0m
    auto exact_snap = small_grid.find_nearest_node(10.7750, 106.6950, 500.0);
    record_test("SpatialGrid snaps to exact node with 0m distance",
                exact_snap.success && exact_snap.node_id == 102 && exact_snap.distance_m < 1.0,
                "node_id: " + std::to_string(exact_snap.node_id));

    // TEST 6: Query coordinate outside max_snap threshold returns success = false
    auto far_snap = small_grid.find_nearest_node(21.0285, 105.8542, 1000.0); // Hanoi coordinates
    record_test("SpatialGrid query outside max_snap threshold returns false",
                !far_snap.success,
                far_snap.error_message);

    // TEST 7: find_k_nearest_nodes returns candidates ordered by distance
    auto k_snaps = small_grid.find_k_nearest_nodes(10.7725, 106.6925, 3, 2000.0);
    bool k_ordered = (k_snaps.size() >= 2) && (k_snaps[0].distance_m <= k_snaps[1].distance_m);
    record_test("SpatialGrid find_k_nearest_nodes returns candidates sorted by distance",
                k_ordered,
                "candidates: " + std::to_string(k_snaps.size()));

    // TEST 8: find_k_nearest_nodes with k = 0 returns empty vector
    auto k_zero = small_grid.find_k_nearest_nodes(10.7725, 106.6925, 0, 2000.0);
    record_test("SpatialGrid find_k_nearest_nodes with k = 0 returns empty vector", k_zero.empty());

    // TEST 9: find_k_nearest_nodes outside snap radius returns empty vector
    auto k_far = small_grid.find_k_nearest_nodes(21.0285, 105.8542, 5, 1000.0);
    record_test("SpatialGrid find_k_nearest_nodes outside snap radius returns empty", k_far.empty());

    // TEST 10: find_k_nearest_nodes with invalid coordinates returns empty vector
    auto k_invalid = small_grid.find_k_nearest_nodes(999.0, 106.6925, 5, 1000.0);
    record_test("SpatialGrid find_k_nearest_nodes with invalid coordinates returns empty", k_invalid.empty());

    // TEST 11: project_point_on_segment projects point perpendicular to segment midpoint
    double t_mid = 0.0;
    auto proj_mid = SpatialGrid::project_point_on_segment(
        10.7710, 106.6950, // Query point directly north of segment midpoint
        10.7700, 106.6900, // Segment Start A
        10.7700, 106.7000, // Segment End B
        t_mid
    );
    record_test("project_point_on_segment projects perpendicular to segment midpoint (t ~ 0.5)",
                std::abs(t_mid - 0.5) < 0.05 && std::abs(proj_mid.first - 10.7700) < 1e-4 && std::abs(proj_mid.second - 106.6950) < 1e-4,
                "t: " + std::to_string(t_mid));

    // TEST 12: project_point_on_segment clamps t to [0, 1] for points beyond endpoints
    double t_clamp_low = 0.0, t_clamp_high = 0.0;
    auto proj_low = SpatialGrid::project_point_on_segment(10.7700, 106.6800, 10.7700, 106.6900, 10.7700, 106.7000, t_clamp_low);
    auto proj_high = SpatialGrid::project_point_on_segment(10.7700, 106.7100, 10.7700, 106.6900, 10.7700, 106.7000, t_clamp_high);
    record_test("project_point_on_segment clamps t to 0.0 and 1.0 outside segment boundaries",
                std::abs(t_clamp_low - 0.0) < 1e-6 && std::abs(t_clamp_high - 1.0) < 1e-6);

    // TEST 13: find_k_nearest_edges on graph with edges returns valid projection
    synthetic_graph.add_edge(101, 102, 550.0);
    synthetic_graph.add_edge(102, 103, 550.0);
    small_grid.build_index(synthetic_graph);
    auto edge_cands = small_grid.find_k_nearest_edges(10.7725, 106.6925, 3, 500.0);
    record_test("find_k_nearest_edges snaps query point to nearest road edge with valid t",
                !edge_cands.empty() && edge_cands[0].success && edge_cands[0].t >= 0.0 && edge_cands[0].t <= 1.0,
                "edges: " + std::to_string(edge_cands.size()) + ", t: " + (edge_cands.empty() ? "" : std::to_string(edge_cands[0].t)));
}

// -------------------------------------------------------------
// SUITE 2: HttpServer & Request Parsing Unit Tests
// -------------------------------------------------------------
void run_suite_http_parsing() {
    std::cout << "\n--- SUITE 2: HttpServer & Request Parsing Unit Tests ---" << std::endl;

    // TEST 7: URL decode plain string
    std::string decoded_plain = HttpServer::url_decode("GoRide_Route_Test");
    record_test("URL decode standard string unchanged", decoded_plain == "GoRide_Route_Test");

    // TEST 8: URL decode percent-encoded chars and plus
    std::string decoded_special = HttpServer::url_decode("Cho+Ben+Thanh%2C%20Q.1");
    record_test("URL decode handles percent-encoding and plus", decoded_special == "Cho Ben Thanh, Q.1");

    // TEST 9: Parse query parameters
    std::string qs = "from_lat=10.7725&from_lon=106.6980&to_lat=10.7798&to_lon=106.6990";
    auto params = HttpServer::parse_query_params(qs);
    record_test("Parse query parameters key-value map",
                params["from_lat"] == "10.7725" &&
                params["from_lon"] == "106.6980" &&
                params["to_lat"] == "10.7798" &&
                params["to_lon"] == "106.6990");

    // TEST 10: Parse query string with empty value or multiple amps
    auto params_edge = HttpServer::parse_query_params("flag&key=val&&empty=");
    record_test("Parse query params with empty keys/flags",
                params_edge.find("flag") != params_edge.end() && params_edge["key"] == "val");

    // TEST 11: Parse raw HTTP request
    std::string raw_http =
        "GET /api/route?from_lat=10.77&to_lat=10.78 HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: GoRideTest\r\n"
        "\r\n";
    HttpRequest req = HttpServer::parse_request(raw_http);
    record_test("Parse HttpRequest method, path, and headers",
                req.method == "GET" &&
                req.path == "/api/route" &&
                req.params["from_lat"] == "10.77" &&
                req.headers["Host"] == "localhost:8080");

    // TEST 12: Format HttpResponse with CORS headers
    HttpResponse res;
    res.set_json("{\"test\":true}", 200);
    std::string formatted = HttpServer::format_response(res);
    record_test("Format HttpResponse contains HTTP/1.1 200, Content-Type, and CORS",
                formatted.find("HTTP/1.1 200 OK") != std::string::npos &&
                formatted.find("Content-Type: application/json") != std::string::npos &&
                formatted.find("Access-Control-Allow-Origin: *") != std::string::npos);
}

// -------------------------------------------------------------
// SUITE 3: Real OSM Dataset Snapping & Dijkstra Integration Tests
// -------------------------------------------------------------
void run_suite_osm_integration(const std::string& osm_path) {
    std::cout << "\n--- SUITE 3: Real OSM Dataset Snapping & Dijkstra Tests ---" << std::endl;
    std::cout << "Loading dataset: " << osm_path << std::endl;

    RoadGraph graph;
    auto load_res = graph.load_from_json_file(osm_path);

    // TEST 13: Graph load assertion
    record_test("Load real OSM dataset (>10000 nodes, >15000 edges)",
                load_res.success && graph.node_count() >= 10000 && graph.edge_count() >= 15000,
                "Nodes: " + std::to_string(graph.node_count()) + ", Edges: " + std::to_string(graph.edge_count()));

    if (!load_res.success) {
        std::cerr << "Cannot continue Suite 3 & 4 tests without OSM dataset." << std::endl;
        return;
    }

    // TEST 14: Build SpatialGrid on full graph
    SpatialGrid grid(0.005);
    grid.build_index(graph);
    record_test("Build SpatialGrid index on full OSM road graph",
                grid.indexed_nodes_count() == graph.node_count(),
                "Indexed: " + std::to_string(grid.indexed_nodes_count()));

    // TEST 15: Snap Ben Thanh coordinates (10.7725, 106.6980)
    auto snap_bt = grid.find_nearest_node(10.7725, 106.6980, 200.0);
    record_test("Snap Ben Thanh coordinates within 200m",
                snap_bt.success && snap_bt.distance_m < 200.0,
                "Snap distance: " + std::to_string(snap_bt.distance_m) + "m, node: " + std::to_string(snap_bt.node_id));

    // TEST 16: Snap Notre Dame Cathedral (10.7798, 106.6990)
    auto snap_nd = grid.find_nearest_node(10.7798, 106.6990, 200.0);
    record_test("Snap Notre Dame Cathedral coordinates within 200m",
                snap_nd.success && snap_nd.distance_m < 200.0,
                "Snap distance: " + std::to_string(snap_nd.distance_m) + "m, node: " + std::to_string(snap_nd.node_id));

    // TEST 17: Snap Bach Khoa University (10.7721, 106.6578)
    auto snap_bk = grid.find_nearest_node(10.7721, 106.6578, 200.0);
    record_test("Snap Bach Khoa University coordinates within 200m",
                snap_bk.success && snap_bk.distance_m < 200.0,
                "Snap distance: " + std::to_string(snap_bk.distance_m) + "m, node: " + std::to_string(snap_bk.node_id));

    // TEST 18: Snap Landmark 81 (10.7951, 106.7219)
    auto snap_lm = grid.find_nearest_node(10.7951, 106.7219, 300.0);
    record_test("Snap Landmark 81 coordinates within 300m",
                snap_lm.success && snap_lm.distance_m < 300.0,
                "Snap distance: " + std::to_string(snap_lm.distance_m) + "m, node: " + std::to_string(snap_lm.node_id));

    // TEST 19: Snap distant point (Hanoi) fails with max_snap=1000m
    auto snap_hanoi = grid.find_nearest_node(21.0285, 105.8542, 1000.0);
    record_test("Snap point outside HCMC (Hanoi) fails with 1000m threshold", !snap_hanoi.success);

    // TEST 20: Route Ben Thanh -> Notre Dame via Dijkstra
    auto route_bt_nd = DijkstraRouter::find_shortest_path(graph, snap_bt.node_id, snap_nd.node_id);
    record_test("Shortest path from Ben Thanh to Notre Dame found",
                route_bt_nd.found && route_bt_nd.total_distance > 0.0,
                "Distance: " + std::to_string(route_bt_nd.total_distance) + "m, Visited: " + std::to_string(route_bt_nd.nodes_visited));

    // TEST 21: Route Ben Thanh -> Notre Dame distance is reasonable (800m - 2500m)
    record_test("Route Ben Thanh -> Notre Dame distance is realistic for city driving",
                route_bt_nd.total_distance >= 800.0 && route_bt_nd.total_distance <= 2500.0,
                "Distance: " + std::to_string(route_bt_nd.total_distance) + "m");

    // TEST 22: Route Notre Dame -> Ben Thanh via Dijkstra
    auto route_nd_bt = DijkstraRouter::find_shortest_path(graph, snap_nd.node_id, snap_bt.node_id);
    record_test("Shortest path from Notre Dame to Ben Thanh found",
                route_nd_bt.found && route_nd_bt.total_distance > 0.0,
                "Distance: " + std::to_string(route_nd_bt.total_distance) + "m");

    // TEST 23: Route Ben Thanh -> Bach Khoa University
    auto route_bt_bk = DijkstraRouter::find_shortest_path(graph, snap_bt.node_id, snap_bk.node_id);
    record_test("Shortest path from Ben Thanh to Bach Khoa University found",
                route_bt_bk.found && route_bt_bk.total_distance >= 3500.0 && route_bt_bk.total_distance <= 7500.0,
                "Distance: " + std::to_string(route_bt_bk.total_distance) + "m");

    // TEST 24: Geometry points validity
    bool geometry_valid = !route_bt_nd.path_geometry.empty();
    for (const auto& pt : route_bt_nd.path_geometry) {
        if (pt.first < 10.6 || pt.first > 10.9 || pt.second < 106.5 || pt.second > 106.8) {
            geometry_valid = false;
            break;
        }
    }
    record_test("All path geometry coordinates fall within valid HCMC bounding box",
                geometry_valid,
                "Points count: " + std::to_string(route_bt_nd.path_geometry.size()));

    // TEST 25: Contiguity of traversed edges
    bool edges_contiguous = true;
    for (size_t i = 1; i < route_bt_nd.path_edges.size(); ++i) {
        if (route_bt_nd.path_edges[i-1].v != route_bt_nd.path_edges[i].u) {
            edges_contiguous = false;
            break;
        }
    }
    record_test("Traversed edges form a strictly contiguous directed path",
                edges_contiguous && !route_bt_nd.path_edges.empty(),
                "Edges count: " + std::to_string(route_bt_nd.path_edges.size()));

    // TEST 26: Path total distance strictly matches sum of individual edge weights
    double edge_sum = 0.0;
    for (const auto& e : route_bt_nd.path_edges) {
        edge_sum += e.weight;
    }
    record_test("Total distance strictly equals sum of edge weights (diff < 1e-4)",
                std::abs(route_bt_nd.total_distance - edge_sum) < 1e-4,
                "diff: " + std::to_string(std::abs(route_bt_nd.total_distance - edge_sum)));

    // Helper to check edge presence in graph
    auto has_edge = [&](NodeId u, NodeId v) -> bool {
        const auto& outgoing = graph.get_outgoing_edges(u);
        for (const auto& e : outgoing) {
            if (e.v == v) return true;
        }
        return false;
    };

    // TEST 27: Multi-turn route Ben Thanh -> Vincom Center Đồng Khởi
    auto snap_vc = grid.find_nearest_node(10.7781, 106.7023, 300.0);
    auto route_bt_vc = DijkstraRouter::find_shortest_path(graph, snap_bt.node_id, snap_vc.node_id);
    record_test("Multi-turn route from Ben Thanh to Vincom Center found",
                route_bt_vc.found && route_bt_vc.total_distance >= 700.0 && route_bt_vc.total_distance <= 2500.0,
                "Distance: " + std::to_string(route_bt_vc.total_distance) + "m");

    // TEST 28: Multi-turn route Ben Thanh -> Landmark 81
    auto route_bt_lm = DijkstraRouter::find_shortest_path(graph, snap_bt.node_id, snap_lm.node_id);
    record_test("Multi-turn route from Ben Thanh to Landmark 81 found",
                route_bt_lm.found && route_bt_lm.total_distance >= 4000.0 && route_bt_lm.total_distance <= 9000.0,
                "Distance: " + std::to_string(route_bt_lm.total_distance) + "m");

    // TEST 29: Driver approach route: Pasteur -> Ben Thanh (navigates one-way streets via candidates)
    DijkstraResult route_pas_bt;
    auto pas_cands = grid.find_k_nearest_nodes(10.7753, 106.7002, 3, 300.0);
    auto bt_cands = grid.find_k_nearest_nodes(10.7725, 106.6980, 3, 300.0);
    for (const auto& pc : pas_cands) {
        for (const auto& bc : bt_cands) {
            auto r = DijkstraRouter::find_shortest_path(graph, pc.node_id, bc.node_id);
            if (r.found) {
                route_pas_bt = r;
                break;
            }
        }
        if (route_pas_bt.found) break;
    }
    record_test("Driver approach multi-turn route from Pasteur to Ben Thanh found",
                route_pas_bt.found && route_pas_bt.total_distance > 0.0,
                "Distance: " + std::to_string(route_pas_bt.total_distance) + "m");

    // TEST 30: Verification that every consecutive node pair in path_nodes is a valid graph edge
    bool all_edges_exist = true;
    for (size_t i = 1; i < route_bt_vc.path_nodes.size(); ++i) {
        if (!has_edge(route_bt_vc.path_nodes[i - 1], route_bt_vc.path_nodes[i])) {
            all_edges_exist = false;
            break;
        }
    }
    record_test("Every consecutive node pair in multi-turn route is a valid directed edge in RoadGraph",
                all_edges_exist && route_bt_vc.path_nodes.size() > 1,
                "Nodes count: " + std::to_string(route_bt_vc.path_nodes.size()));

    // TEST 31: Road-strict geometry: every geometry coordinate matches its node in RoadGraph
    bool all_geom_match_nodes = (route_bt_vc.path_geometry.size() == route_bt_vc.path_nodes.size());
    for (size_t i = 0; i < route_bt_vc.path_nodes.size() && all_geom_match_nodes; ++i) {
        const Node* n = graph.get_node(route_bt_vc.path_nodes[i]);
        if (!n || std::abs(n->lat - route_bt_vc.path_geometry[i].first) > 1e-6 ||
            std::abs(n->lon - route_bt_vc.path_geometry[i].second) > 1e-6) {
            all_geom_match_nodes = false;
        }
    }
    record_test("Path geometry coordinates strictly match graph vertex coordinates",
                all_geom_match_nodes,
                "Vertices verified: " + std::to_string(route_bt_vc.path_geometry.size()));

    // TEST 32: Multi-turn route has multiple intermediate vertices (> 5 vertices along street corridors)
    record_test("Multi-turn route traverses at least 5 intermediate road nodes",
                route_bt_vc.path_nodes.size() >= 5,
                "Actual node count: " + std::to_string(route_bt_vc.path_nodes.size()));

    // TEST 33: Directed one-way enforcement (reverse route is distinct from forward route)
    auto route_vc_bt = DijkstraRouter::find_shortest_path(graph, snap_vc.node_id, snap_bt.node_id);
    record_test("Reverse route Vincom to Ben Thanh follows distinct one-way corridor",
                route_vc_bt.found && route_vc_bt.total_distance > 0.0 && route_vc_bt.path_nodes != route_bt_vc.path_nodes,
                "Forward: " + std::to_string(route_bt_vc.total_distance) + "m, Reverse: " + std::to_string(route_vc_bt.total_distance) + "m");

    // -------------------------------------------------------------
    // SUITE 4: End-to-End API Handler Logic Simulation Tests
    // -------------------------------------------------------------
    std::cout << "\n--- SUITE 4: End-to-End API Handler Simulation Tests ---" << std::endl;

    // We simulate the route handler directly matching backend/main.cpp logic
    auto simulate_route_call = [&](const std::string& query) -> HttpResponse {
        HttpRequest req;
        req.method = "GET";
        req.path = "/api/route";
        req.params = HttpServer::parse_query_params(query);
        HttpResponse res;

        // Handler logic:
        std::string s_from_lat = req.get_param("from_lat");
        std::string s_from_lon = req.get_param("from_lon");
        std::string s_to_lat = req.get_param("to_lat");
        std::string s_to_lon = req.get_param("to_lon");

        if (s_from_lat.empty() || s_from_lon.empty() || s_to_lat.empty() || s_to_lon.empty()) {
            res.set_json(R"({"success":false,"code":"MISSING_PARAMETERS"})", 400);
            return res;
        }

        double f_lat, f_lon, t_lat, t_lon;
        try {
            f_lat = std::stod(s_from_lat);
            f_lon = std::stod(s_from_lon);
            t_lat = std::stod(s_to_lat);
            t_lon = std::stod(s_to_lon);
        } catch (...) {
            res.set_json(R"({"success":false,"code":"INVALID_COORDINATES"})", 400);
            return res;
        }

        if (f_lat < -90.0 || f_lat > 90.0 || t_lat < -90.0 || t_lat > 90.0 ||
            f_lon < -180.0 || f_lon > 180.0 || t_lon < -180.0 || t_lon > 180.0) {
            res.set_json(R"({"success":false,"code":"OUT_OF_BOUNDS_COORDINATES"})", 400);
            return res;
        }

        auto origin_edges = grid.find_k_nearest_edges(f_lat, f_lon, 5, 1000.0);
        auto dest_edges = grid.find_k_nearest_edges(t_lat, t_lon, 5, 1000.0);
        auto origin_nodes = grid.find_k_nearest_nodes(f_lat, f_lon, 5, 1000.0);
        auto dest_nodes = grid.find_k_nearest_nodes(t_lat, t_lon, 5, 1000.0);

        if (origin_edges.empty() && origin_nodes.empty()) {
            res.set_json(R"({"success":false,"code":"ORIGIN_SNAP_FAILED"})", 422);
            return res;
        }

        if (dest_edges.empty() && dest_nodes.empty()) {
            res.set_json(R"({"success":false,"code":"DESTINATION_SNAP_FAILED"})", 422);
            return res;
        }

        bool route_found = false;
        double best_total_cost = std::numeric_limits<double>::infinity();
        double best_distance_m = 0.0;
        std::vector<goride::dsa::NodeId> best_path_nodes;
        std::vector<std::pair<double, double>> best_path_geometry;

        if (!origin_edges.empty() && !dest_edges.empty()) {
            size_t max_oe = std::min<size_t>(3, origin_edges.size());
            size_t max_de = std::min<size_t>(3, dest_edges.size());

            for (size_t oi = 0; oi < max_oe; ++oi) {
                const auto& oe = origin_edges[oi];
                for (size_t di = 0; di < max_de; ++di) {
                    const auto& de = dest_edges[di];

                    if (oe.edge.u == de.edge.u && oe.edge.v == de.edge.v && oe.t <= de.t) {
                        double drive_dist = (de.t - oe.t) * oe.edge.weight;
                        double total_cost = drive_dist + oe.distance_m + de.distance_m;
                        if (total_cost < best_total_cost) {
                            best_total_cost = total_cost;
                            best_distance_m = drive_dist;
                            route_found = true;
                            best_path_nodes = {oe.edge.u, oe.edge.v};
                            best_path_geometry = {{oe.snap_lat, oe.snap_lon}, {de.snap_lat, de.snap_lon}};
                        }
                        continue;
                    }

                    goride::dsa::DijkstraResult d_res;
                    if (oe.edge.v == de.edge.u) {
                        d_res.found = true;
                        d_res.total_distance = 0.0;
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
                        }
                    }
                }
            }
        }

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
                            best_path_nodes = cand_res.path_nodes;
                            best_path_geometry = cand_res.path_geometry;
                        }
                    }
                }
            }
        }

        if (!route_found) {
            res.set_json(R"({"success":false,"code":"ROUTE_NOT_FOUND"})", 404);
            return res;
        }

        // Build GeoJSON output along real road network graph
        std::ostringstream oss;
        oss.precision(7);
        oss << std::fixed;
        oss << "{\"success\":true,\"distance_m\":" << best_distance_m << ",\"geometry\":[";
        for (size_t i = 0; i < best_path_geometry.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "[" << best_path_geometry[i].second << "," << best_path_geometry[i].first << "]"; // [lon, lat]
        }
        oss << "],\"path_nodes\":[";
        for (size_t i = 0; i < best_path_nodes.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << best_path_nodes[i] << "\"";
        }
        oss << "]}";
        res.set_json(oss.str(), 200);
        return res;
    };

    // TEST 32: Missing parameters -> HTTP 400
    auto res_missing = simulate_route_call("from_lat=10.7725&from_lon=106.6980");
    record_test("API Handler: Missing query parameters returns HTTP 400",
                res_missing.status_code == 400 && res_missing.body.find("MISSING_PARAMETERS") != std::string::npos);

    // TEST 33: Non-numeric coordinate -> HTTP 400
    auto res_invalid = simulate_route_call("from_lat=invalid&from_lon=106.69&to_lat=10.78&to_lon=106.70");
    record_test("API Handler: Non-numeric coordinates return HTTP 400",
                res_invalid.status_code == 400 && res_invalid.body.find("INVALID_COORDINATES") != std::string::npos);

    // TEST 34: Out of bounds coordinate -> HTTP 400
    auto res_oob = simulate_route_call("from_lat=999.0&from_lon=106.69&to_lat=10.78&to_lon=106.70");
    record_test("API Handler: Out-of-bounds coordinates return HTTP 400",
                res_oob.status_code == 400 && res_oob.body.find("OUT_OF_BOUNDS_COORDINATES") != std::string::npos);

    // TEST 35: Unsnappable origin (far out) -> HTTP 422
    auto res_far = simulate_route_call("from_lat=0.0&from_lon=0.0&to_lat=10.7798&to_lon=106.6990");
    record_test("API Handler: Distant unsnappable origin returns HTTP 422 ORIGIN_SNAP_FAILED",
                res_far.status_code == 422 && res_far.body.find("ORIGIN_SNAP_FAILED") != std::string::npos);

    // TEST 36: Valid route request Ben Thanh -> Notre Dame -> HTTP 200
    auto res_valid = simulate_route_call("from_lat=10.7725&from_lon=106.6980&to_lat=10.7798&to_lon=106.6990");
    record_test("API Handler: Valid route coordinates return HTTP 200 OK",
                res_valid.status_code == 200 && res_valid.body.find("\"success\":true") != std::string::npos);

    // TEST 37: GeoJSON geometry format verification
    record_test("API Handler: Response geometry contains [longitude, latitude] ordering",
                res_valid.body.find("[106.698") != std::string::npos,
                "GeoJSON order verified");

    // TEST 38: 64-bit node IDs as strings
    record_test("API Handler: Path node IDs are serialized as JSON string literals",
                res_valid.body.find("\"path_nodes\":[\"") != std::string::npos,
                "Node IDs as strings verified");

    // TEST 39: Driver approach route API call: Pasteur -> Ben Thanh
    auto res_driver = simulate_route_call("from_lat=10.7753&from_lon=106.7002&to_lat=10.7725&to_lon=106.6980");
    record_test("API Handler: Driver approach route returns HTTP 200 OK on real OSM road network",
                res_driver.status_code == 200 && res_driver.body.find("\"success\":true") != std::string::npos,
                "Status: " + std::to_string(res_driver.status_code));

    // TEST 40: API response for multi-turn route contains multiple intermediate nodes
    size_t comma_count = 0;
    size_t pn_pos = res_driver.body.find("\"path_nodes\":[");
    if (pn_pos != std::string::npos) {
        size_t end_pos = res_driver.body.find("]", pn_pos);
        for (size_t i = pn_pos; i < end_pos; ++i) {
            if (res_driver.body[i] == ',') ++comma_count;
        }
    }
    record_test("API Handler: Multi-turn route contains >= 5 intermediate vertices",
                comma_count >= 4,
                "Nodes count: " + std::to_string(comma_count + 1));

    // TEST 41: Unreachable destination returns HTTP 404 ROUTE_NOT_FOUND
    graph.add_node(9999999999ULL, 10.70001, 106.70001); // Isolated node with 0 edges
    grid.build_index(graph);
    auto res_unreachable = simulate_route_call("from_lat=10.7725&from_lon=106.6980&to_lat=10.70001&to_lon=106.70001");
    record_test("API Handler: Unreachable destination returns HTTP 404 ROUTE_NOT_FOUND",
                res_unreachable.status_code == 404 && res_unreachable.body.find("ROUTE_NOT_FOUND") != std::string::npos,
                "Status: " + std::to_string(res_unreachable.status_code));
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "==========================================================" << std::endl;
    std::cout << "    GoRide (RIDE-RTREE) Phase 2A.3 API & Routing Tests    " << std::endl;
    std::cout << "==========================================================" << std::endl;

    std::string preferred_path = (argc > 1) ? argv[1] : "data/osm/processed/hcmc_roads.json";
    std::string osm_path = find_osm_file(preferred_path);

    run_suite_spatial_grid();
    run_suite_http_parsing();
    run_suite_osm_integration(osm_path);

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "TEST RESULTS SUMMARY: " << passed_tests << " / " << total_tests << " PASSED" << std::endl;
    if (passed_tests == total_tests) {
        std::cout << "OVERALL STATUS: ALL TESTS PASSED (100%)" << std::endl;
    } else {
        std::cout << "OVERALL STATUS: " << (total_tests - passed_tests) << " TESTS FAILED" << std::endl;
    }
    std::cout << "==========================================================" << std::endl;

    return (passed_tests == total_tests) ? 0 : 1;
}
