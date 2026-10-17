#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "auth/auth_service.hpp"
#include "dsa/dijkstra.hpp"
#include "dsa/road_graph.hpp"
#include "dsa/spatial_grid.hpp"
#include "server/http_server.hpp"
#include "trip/trip_service.hpp"
#include "utils/json.hpp"

/*
 * GoRide (RIDE-RTREE) Backend Server - Phase 2A.3
 * 
 * Integrated Components:
 * - C++17 RoadGraph with directed adjacency list from OSM
 * - SpatialGrid O(1) GPS coordinate snapping via Haversine
 * - Min-heap Dijkstra shortest path routing
 * - Lightweight HTTP/1.1 REST API server with CORS
 * 
 * Endpoints:
 * - GET /api/health: Check server health and dataset statistics
 * - GET /api/route:  Find shortest path between GPS coordinates
 */

namespace {

// Helper to locate OSM dataset file across common working directories
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

std::atomic<uint64_t> g_trip_counter{1000};
std::string generate_trip_id() {
    auto now = std::chrono::system_clock::now();
    auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    uint64_t cnt = g_trip_counter.fetch_add(1);
    std::ostringstream ss;
    ss << "trp_" << epoch_ms << "_" << (cnt % 10000);
    return ss.str();
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "==========================================================" << std::endl;
    std::cout << "           GoRide (RIDE-RTREE) Routing Server            " << std::endl;
    std::cout << "  Phase 2A.3: C++ RoadGraph + Dijkstra + REST API Server  " << std::endl;
    std::cout << "==========================================================" << std::endl;

    std::string preferred_path = (argc > 1) ? argv[1] : "data/osm/processed/hcmc_roads.json";
    std::string osm_path = find_osm_file(preferred_path);

    int port = 8080;
    if (argc > 2) {
        try {
            port = std::stoi(argv[2]);
        } catch (...) {}
    } else if (const char* env_p = std::getenv("PORT")) {
        try {
            port = std::stoi(env_p);
        } catch (...) {}
    }

    std::string host = (argc > 3) ? argv[3] : "0.0.0.0";

    // 1. Load RoadGraph from OSM dataset
    std::cout << "\n[1/3] Loading OpenStreetMap Road Network..." << std::endl;
    std::cout << "      File path: " << osm_path << std::endl;

    goride::dsa::RoadGraph graph;
    auto start_load = std::chrono::high_resolution_clock::now();
    auto load_res = graph.load_from_json_file(osm_path);
    auto end_load = std::chrono::high_resolution_clock::now();
    double load_time_ms = std::chrono::duration<double, std::milli>(end_load - start_load).count();

    if (load_res.success) {
        std::cout << "      [SUCCESS] RoadGraph loaded in " << std::fixed << std::setprecision(1) << load_time_ms << " ms" << std::endl;
        std::cout << "      - Motorable Nodes: " << load_res.nodes_loaded << std::endl;
        std::cout << "      - Directed Edges: " << load_res.edges_loaded << std::endl;
    } else {
        std::cerr << "      [ERROR] Could not load OSM dataset from: " << osm_path << std::endl;
        if (!load_res.error_messages.empty()) {
            for (const auto& err_msg : load_res.error_messages) {
                std::cerr << "              - " << err_msg << std::endl;
            }
        } else {
            std::cerr << "              - Unknown error occurred while reading OSM JSON dataset." << std::endl;
        }
        std::cerr << "      Server aborted: cannot operate without valid road network data." << std::endl;
        return 1;
    }

    // 2. Build SpatialGrid index
    std::cout << "\n[2/3] Building SpatialGrid Index (cell size: 0.005 deg ~ 550m)..." << std::endl;
    goride::dsa::SpatialGrid grid(0.005);
    if (graph.node_count() > 0) {
        auto start_grid = std::chrono::high_resolution_clock::now();
        grid.build_index(graph);
        auto end_grid = std::chrono::high_resolution_clock::now();
        double grid_time_ms = std::chrono::duration<double, std::milli>(end_grid - start_grid).count();

        std::cout << "      [SUCCESS] Indexed " << grid.indexed_nodes_count()
                  << " nodes in " << std::fixed << std::setprecision(1) << grid_time_ms << " ms" << std::endl;
    } else {
        std::cout << "      [SKIPPED] No nodes to index in SpatialGrid." << std::endl;
    }

    // 3. Configure HTTP REST API Server
    std::cout << "\n[3/3] Setting up HTTP REST API routes..." << std::endl;
    goride::server::HttpServer server;

    // Route: Root /
    auto handle_root = [&](const goride::server::HttpRequest&, goride::server::HttpResponse& res) {
        std::ostringstream ss;
        ss << "{\n"
           << "  \"name\": \"GoRide Routing Engine API\",\n"
           << "  \"version\": \"Phase C.3\",\n"
           << "  \"status\": \"" << (graph.node_count() > 0 ? "operational" : "degraded") << "\",\n"
           << "  \"endpoints\": [\n"
           << "    \"GET /api/health\",\n"
           << "    \"GET /api/route?from_lat=...&from_lon=...&to_lat=...&to_lon=...\",\n"
           << "    \"POST /api/auth/login\",\n"
           << "    \"POST /api/auth/register\",\n"
           << "    \"GET /api/auth/me\",\n"
           << "    \"POST /api/auth/logout\",\n"
           << "    \"POST /api/auth/approve-driver\",\n"
           << "    \"GET /api/admin/drivers/pending\",\n"
           << "    \"POST /api/trips/request\",\n"
           << "    \"GET /api/trips/active\",\n"
           << "    \"GET /api/trips/detail?id=...\",\n"
           << "    \"POST /api/trips/cancel\",\n"
           << "    \"GET /api/driver/offers/pending\",\n"
           << "    \"POST /api/driver/offers/accept\",\n"
           << "    \"POST /api/driver/offers/reject\",\n"
           << "    \"POST /api/driver/trips/status\",\n"
           << "    \"GET /api/admin/trips\",\n"
           << "    \"POST /api/admin/trips/offer\"\n"
           << "  ]\n"
           << "}";
        res.set_json(ss.str(), 200);
    };
    server.get("/", handle_root);

    // Route: Health check
    auto handle_health = [&](const goride::server::HttpRequest&, goride::server::HttpResponse& res) {
        std::ostringstream ss;
        ss << "{\n"
           << "  \"success\": true,\n"
           << "  \"status\": \"" << (graph.node_count() > 0 ? "ready" : "no_data") << "\",\n"
           << "  \"nodes\": " << graph.node_count() << ",\n"
           << "  \"edges\": " << graph.edge_count() << ",\n"
           << "  \"indexed_nodes\": " << grid.indexed_nodes_count() << ",\n"
           << "  \"dataset\": \"" << osm_path << "\",\n"
           << "  \"engine\": \"C++17 RoadGraph + DijkstraRouter\",\n"
           << "  \"version\": \"0.1.0-phase2a.3\"\n"
           << "}";
        res.set_json(ss.str(), 200);
    };
    server.get("/health", handle_health);
    server.get("/api/health", handle_health);
    server.get("/api/v1/health", handle_health);

    // Route: Route calculation via Dijkstra
    auto handle_route = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        if (graph.node_count() == 0) {
            res.set_json("{\"success\":false,\"code\":\"GRAPH_EMPTY\",\"error\":\"OSM road network dataset is not loaded.\"}", 503);
            return;
        }

        // Support standard and alias parameter names
        std::string s_from_lat = req.get_param("from_lat", req.get_param("origin_lat", req.get_param("pickup_lat")));
        std::string s_from_lon = req.get_param("from_lon", req.get_param("from_lng", req.get_param("origin_lon", req.get_param("origin_lng"))));
        std::string s_to_lat = req.get_param("to_lat", req.get_param("dest_lat", req.get_param("destination_lat")));
        std::string s_to_lon = req.get_param("to_lon", req.get_param("to_lng", req.get_param("dest_lon", req.get_param("dest_lng"))));

        if (s_from_lat.empty() || s_from_lon.empty() || s_to_lat.empty() || s_to_lon.empty()) {
            res.set_json("{\"success\":false,\"code\":\"MISSING_PARAMETERS\",\"error\":\"Missing required query parameters: from_lat, from_lon, to_lat, to_lon\"}", 400);
            return;
        }

        double from_lat = 0.0, from_lon = 0.0, to_lat = 0.0, to_lon = 0.0;
        try {
            from_lat = std::stod(s_from_lat);
            from_lon = std::stod(s_from_lon);
            to_lat = std::stod(s_to_lat);
            to_lon = std::stod(s_to_lon);
        } catch (...) {
            res.set_json("{\"success\":false,\"code\":\"INVALID_COORDINATES\",\"error\":\"Coordinates must be valid floating point numbers.\"}", 400);
            return;
        }

        if (from_lat < -90.0 || from_lat > 90.0 || to_lat < -90.0 || to_lat > 90.0 ||
            from_lon < -180.0 || from_lon > 180.0 || to_lon < -180.0 || to_lon > 180.0) {
            res.set_json("{\"success\":false,\"code\":\"OUT_OF_BOUNDS_COORDINATES\",\"error\":\"Coordinates out of WGS84 range: lat [-90, 90], lon [-180, 180].\"}", 400);
            return;
        }

        // Snap threshold in meters (default 1000m)
        double max_snap = 1000.0;
        if (req.has_param("max_snap_m")) {
            try { max_snap = std::stod(req.get_param("max_snap_m")); } catch (...) {}
        }

        // 1. Query nearest road edge candidates (for high-precision centerline projection)
        auto origin_edges = grid.find_k_nearest_edges(from_lat, from_lon, 5, max_snap);
        auto dest_edges = grid.find_k_nearest_edges(to_lat, to_lon, 5, max_snap);

        // Fallback candidate vertices
        auto origin_nodes = grid.find_k_nearest_nodes(from_lat, from_lon, 5, max_snap);
        auto dest_nodes = grid.find_k_nearest_nodes(to_lat, to_lon, 5, max_snap);

        if (origin_edges.empty() && origin_nodes.empty()) {
            std::ostringstream err;
            err << "{\"success\":false,\"code\":\"ORIGIN_SNAP_FAILED\",\"error\":\"Origin location ("
                << from_lat << ", " << from_lon << ") is farther than " << max_snap
                << "m from any motorable OSM road.\"}";
            res.set_json(err.str(), 422);
            return;
        }

        if (dest_edges.empty() && dest_nodes.empty()) {
            std::ostringstream err;
            err << "{\"success\":false,\"code\":\"DESTINATION_SNAP_FAILED\",\"error\":\"Destination location ("
                << to_lat << ", " << to_lon << ") is farther than " << max_snap
                << "m from any motorable OSM road.\"}";
            res.set_json(err.str(), 422);
            return;
        }

        // 2. Shortest path routing with Dijkstra & Edge Snapping
        auto start_route = std::chrono::high_resolution_clock::now();

        bool route_found = false;
        double best_total_cost = std::numeric_limits<double>::infinity();
        double best_distance_m = 0.0;
        size_t best_nodes_visited = 0;
        std::vector<goride::dsa::NodeId> best_path_nodes;
        std::vector<std::pair<double, double>> best_path_geometry; // (lat, lon)

        double snap_orig_lat = 0.0, snap_orig_lon = 0.0, snap_orig_dist = 0.0;
        std::string snap_orig_node_id;
        double snap_dest_lat = 0.0, snap_dest_lon = 0.0, snap_dest_dist = 0.0;
        std::string snap_dest_node_id;

        // Try edge-based candidate pairs
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
                    goride::dsa::DijkstraResult d_res;
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
                        d_res = goride::dsa::DijkstraRouter::find_shortest_path(graph, oe.edge.v, de.edge.u);
                    }

                    if (d_res.found) {
                        double drive_dist = oe.dist_to_v_m + d_res.total_distance + de.dist_from_u_m;
                        double total_cost = drive_dist + oe.distance_m + de.distance_m;
                        if (total_cost < best_total_cost) {
                            best_total_cost = total_cost;
                            best_distance_m = drive_dist;
                            route_found = true;
                            best_nodes_visited = d_res.nodes_visited + 2;

                            // Assemble path nodes
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

                            // Assemble continuous centerline geometry: [oe.snap] -> [intermediate nodes] -> [de.snap]
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

        // Fallback: Node-based candidate pairs if edge routing didn't yield a path
        if (!route_found && !origin_nodes.empty() && !dest_nodes.empty()) {
            size_t max_o = std::min<size_t>(3, origin_nodes.size());
            size_t max_d = std::min<size_t>(3, dest_nodes.size());
            for (size_t oi = 0; oi < max_o; ++oi) {
                for (size_t di = 0; di < max_d; ++di) {
                    auto cand_res = goride::dsa::DijkstraRouter::find_shortest_path(graph, origin_nodes[oi].node_id, dest_nodes[di].node_id);
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
            std::ostringstream err;
            err << "{\"success\":false,\"code\":\"ROUTE_NOT_FOUND\",\"error\":\"No valid motorable route found between coordinates.\",\"nodes_visited\":" << best_nodes_visited << "}";
            res.set_json(err.str(), 404);
            return;
        }

        // 3. Format JSON response (GeoJSON coordinates: [longitude, latitude])
        std::ostringstream oss;
        oss.precision(7);
        oss << std::fixed;

        oss << "{\n";
        oss << "  \"success\": true,\n";
        oss << "  \"distance_m\": " << best_distance_m << ",\n";
        oss << "  \"distance_km\": " << (best_distance_m / 1000.0) << ",\n";
        oss << "  \"estimated_duration_s\": " << static_cast<int>(std::round(best_distance_m / 6.944)) << ",\n";
        oss << "  \"computation_time_ms\": " << route_time_ms << ",\n";
        oss << "  \"nodes_visited\": " << best_nodes_visited << ",\n";

        // Snapping details
        oss << "  \"origin\": {\n";
        oss << "    \"query\": {\"lat\": " << from_lat << ", \"lon\": " << from_lon << "},\n";
        oss << "    \"snapped\": {\"node_id\": \"" << snap_orig_node_id << "\", \"lat\": " << snap_orig_lat
            << ", \"lon\": " << snap_orig_lon << ", \"snap_distance_m\": " << snap_orig_dist << "}\n";
        oss << "  },\n";

        oss << "  \"destination\": {\n";
        oss << "    \"query\": {\"lat\": " << to_lat << ", \"lon\": " << to_lon << "},\n";
        oss << "    \"snapped\": {\"node_id\": \"" << snap_dest_node_id << "\", \"lat\": " << snap_dest_lat
            << ", \"lon\": " << snap_dest_lon << ", \"snap_distance_m\": " << snap_dest_dist << "}\n";
        oss << "  },\n";

        // Sequence of node IDs as strings (preserving 64-bit precision in JS)
        oss << "  \"path_nodes\": [";
        for (size_t i = 0; i < best_path_nodes.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << best_path_nodes[i] << "\"";
        }
        oss << "],\n";

        // GeoJSON geometry: array of [lon, lat] pairs along real road network graph
        oss << "  \"geometry\": [\n";
        for (size_t i = 0; i < best_path_geometry.size(); ++i) {
            double p_lat = best_path_geometry[i].first;
            double p_lon = best_path_geometry[i].second;
            oss << "    [" << p_lon << ", " << p_lat << "]";
            if (i + 1 < best_path_geometry.size()) {
                oss << ",\n";
            }
        }
        oss << "\n  ]\n";
        oss << "}";

        res.set_json(oss.str(), 200);
    };

    server.get("/route", handle_route);
    server.get("/api/route", handle_route);
    server.get("/api/v1/route", handle_route);

    // 3.5. Authentication & User Management Service (SQLite Database)
    std::cout << "\n[3.5] Initializing Authentication & User Management Service (SQLite Database)..." << std::endl;
    goride::auth::AuthService auth_service("database/goride.db", "data/users.json");
    std::cout << "      [SUCCESS] Loaded " << auth_service.user_count() << " user accounts from SQLite database (" << auth_service.database().path() << ")." << std::endl;

    // Route: Auth Login (POST /api/auth/login)
    auto handle_login = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        std::string err;
        auto root = goride::utils::JsonValue::parse(req.body, err);
        std::string email;
        std::string password;

        if (err.empty() && root.is_object()) {
            email = root["email"].as_string();
            password = root["password"].as_string();
        } else {
            email = req.get_param("email");
            password = req.get_param("password");
        }

        auto auth_res = auth_service.login(email, password);
        res.set_json(auth_res.to_json(), auth_res.status_code);
    };

    server.post("/login", handle_login);
    server.post("/auth/login", handle_login);
    server.post("/api/auth/login", handle_login);
    server.post("/api/v1/auth/login", handle_login);

    // Route: Auth Register (POST /api/auth/register)
    auto handle_register = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        std::string err;
        auto root = goride::utils::JsonValue::parse(req.body, err);
        if (!err.empty() || !root.is_object()) {
            res.set_json("{\"success\":false,\"code\":\"INVALID_JSON\",\"error\":\"Dữ liệu gửi lên không đúng định dạng JSON.\"}", 400);
            return;
        }

        std::string name = root["name"].as_string();
        std::string email = root["email"].as_string();
        std::string phone = root["phone"].as_string();
        std::string password = root["password"].as_string();
        std::string role = root["role"].as_string("customer");
        std::string license_plate = root.has_key("licensePlate") ? root["licensePlate"].as_string() : root["license_plate"].as_string();
        std::string vehicle_type = root.has_key("vehicleType") ? root["vehicleType"].as_string() : root["vehicle_type"].as_string();
        int experience = static_cast<int>(root["experience"].as_int64(0));

        auto reg_res = auth_service.register_user(name, email, phone, password, role, license_plate, vehicle_type, experience);
        res.set_json(reg_res.to_json(), reg_res.status_code);
    };

    server.post("/register", handle_register);
    server.post("/auth/register", handle_register);
    server.post("/api/auth/register", handle_register);
    server.post("/api/v1/auth/register", handle_register);

    // Route: Auth Current User Profile (GET /api/auth/me)
    auto handle_me = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        std::string token = req.get_bearer_token();
        if (token.empty()) {
            token = req.get_param("token");
        }
        auto val_res = auth_service.validate_token(token);
        res.set_json(val_res.to_json(), val_res.status_code);
    };

    server.get("/me", handle_me);
    server.get("/auth/me", handle_me);
    server.get("/api/auth/me", handle_me);
    server.get("/api/v1/auth/me", handle_me);

    // Route: Auth Logout (POST /api/auth/logout)
    auto handle_logout = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        std::string token = req.get_bearer_token();
        if (token.empty()) {
            std::string err;
            auto root = goride::utils::JsonValue::parse(req.body, err);
            if (err.empty() && root.is_object()) {
                token = root["token"].as_string();
            }
        }
        auth_service.logout(token);
        res.set_json("{\"success\":true,\"message\":\"Đăng xuất thành công.\"}", 200);
    };

    server.post("/logout", handle_logout);
    server.post("/auth/logout", handle_logout);
    server.post("/api/auth/logout", handle_logout);
    server.post("/api/v1/auth/logout", handle_logout);

    // Route: Admin Approve/Reject Driver (POST /api/auth/approve-driver or /api/admin/drivers/approve)
    auto handle_approve_driver = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        std::string admin_token = req.get_bearer_token();
        if (admin_token.empty()) {
            admin_token = req.get_param("token");
        }
        auto admin_check = auth_service.validate_token(admin_token);
        if (!admin_check.success) {
            res.set_json(admin_check.to_json(), admin_check.status_code);
            return;
        }
        if (admin_check.user.role != "admin") {
            res.set_json("{\"success\":false,\"code\":\"FORBIDDEN\",\"error\":\"Chỉ Quản trị viên (Admin) mới có quyền xét duyệt tài xế.\"}", 403);
            return;
        }

        std::string target_id;
        bool is_approve = true;
        if (req.path.find("/reject") != std::string::npos) {
            is_approve = false;
        }

        std::string err;
        auto root = goride::utils::JsonValue::parse(req.body, err);
        if (err.empty() && root.is_object()) {
            if (root.has_key("driverId")) target_id = root["driverId"].as_string();
            else if (root.has_key("driver_id")) target_id = root["driver_id"].as_string();
            else if (root.has_key("email")) target_id = root["email"].as_string();

            bool has_action = root.has_key("action");
            bool has_approved = root.has_key("approved");

            if (has_action) {
                std::string act = root["action"].as_string();
                if (act != "approve" && act != "reject") {
                    res.set_json("{\"success\":false,\"code\":\"INVALID_ACTION\",\"error\":\"Giá trị trường 'action' không hợp lệ (chỉ chấp nhận 'approve' hoặc 'reject').\"}", 400);
                    return;
                }
            }

            if (has_action && has_approved) {
                std::string act = root["action"].as_string();
                bool app = root["approved"].as_bool(true);
                if ((act == "approve" && !app) || (act == "reject" && app)) {
                    res.set_json("{\"success\":false,\"code\":\"CONFLICTING_ACTION_APPROVED\",\"error\":\"Dữ liệu không hợp lệ: 'action' và 'approved' mâu thuẫn nhau.\"}", 400);
                    return;
                }
                is_approve = (act == "approve");
            } else if (has_action) {
                is_approve = (root["action"].as_string() == "approve");
            } else if (has_approved) {
                is_approve = root["approved"].as_bool(true);
            }
        }
        if (target_id.empty()) {
            target_id = req.get_param("driver_id", req.get_param("driverId", req.get_param("email")));
        }

        if (target_id.empty()) {
            res.set_json("{\"success\":false,\"code\":\"MISSING_DRIVER_ID\",\"error\":\"Vui lòng cung cấp driverId hoặc email cần xét duyệt.\"}", 400);
            return;
        }

        auto result = auth_service.process_driver_approval(target_id, is_approve);
        switch (result) {
            case goride::auth::DriverApprovalResult::SUCCESS:
                if (is_approve) {
                    res.set_json("{\"success\":true,\"message\":\"Phê duyệt tài khoản tài xế thành công! Tài xế hiện đã có thể đăng nhập nhận chuyến.\"}", 200);
                } else {
                    res.set_json("{\"success\":true,\"message\":\"Đã từ chối hồ sơ đăng ký của tài xế thành công.\"}", 200);
                }
                break;
            case goride::auth::DriverApprovalResult::NOT_FOUND:
            case goride::auth::DriverApprovalResult::NOT_A_DRIVER:
                res.set_json("{\"success\":false,\"code\":\"DRIVER_NOT_FOUND\",\"error\":\"Không tìm thấy tài xế tương ứng hoặc tài khoản không phải vai trò driver.\"}", 404);
                break;
            case goride::auth::DriverApprovalResult::ALREADY_APPROVED:
                res.set_json("{\"success\":false,\"code\":\"ALREADY_APPROVED\",\"error\":\"Tài khoản tài xế này đã được phê duyệt trước đó và đang hoạt động.\"}", 409);
                break;
            case goride::auth::DriverApprovalResult::ALREADY_REJECTED:
                res.set_json("{\"success\":false,\"code\":\"ALREADY_REJECTED\",\"error\":\"Tài khoản tài xế này đã bị từ chối hoặc đang bị khóa.\"}", 409);
                break;
        }
    };

    server.post("/api/auth/approve-driver", handle_approve_driver);
    server.post("/api/v1/auth/approve-driver", handle_approve_driver);
    server.post("/api/admin/drivers/approve", handle_approve_driver);
    server.post("/api/admin/drivers/reject", handle_approve_driver);

    // Route: Admin Get Pending Drivers (GET /api/admin/drivers/pending)
    auto handle_pending_drivers = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        std::string admin_token = req.get_bearer_token();
        if (admin_token.empty()) {
            admin_token = req.get_param("token");
        }
        auto admin_check = auth_service.validate_token(admin_token);
        if (!admin_check.success) {
            res.set_json(admin_check.to_json(), admin_check.status_code);
            return;
        }
        if (admin_check.user.role != "admin") {
            res.set_json("{\"success\":false,\"code\":\"FORBIDDEN\",\"error\":\"Chỉ Quản trị viên (Admin) mới có quyền truy cập danh sách chờ duyệt.\"}", 403);
            return;
        }

        auto pending = auth_service.get_pending_drivers();
        std::ostringstream ss;
        ss << "{\n  \"success\": true,\n  \"total\": " << pending.size() << ",\n  \"drivers\": [\n";
        for (size_t i = 0; i < pending.size(); ++i) {
            ss << "    " << pending[i].to_json();
            if (i + 1 < pending.size()) ss << ",";
            ss << "\n";
        }
        ss << "  ]\n}";
        res.set_json(ss.str(), 200);
    };

    server.get("/api/admin/drivers/pending", handle_pending_drivers);
    server.get("/api/v1/admin/drivers/pending", handle_pending_drivers);

    // 3.6. Trip Lifecycle & Dispatch REST API (Gate C.3)
    std::cout << "\n[3.6] Initializing Trip Lifecycle & Dispatch Service (SQLite Database)..." << std::endl;
    goride::trip::TripService trip_service(auth_service.database());
    std::cout << "      [SUCCESS] TripService initialized on database (" << trip_service.database().path() << ")." << std::endl;

    // RBAC Authenticate Helper Lambda
    auto authenticate_user = [&](const goride::server::HttpRequest& req,
                                 goride::server::HttpResponse& res,
                                 const std::vector<std::string>& allowed_roles = {}) -> std::optional<goride::auth::User> {
        std::string token = req.get_bearer_token();
        if (token.empty()) {
            token = req.get_param("token");
        }
        if (token.empty()) {
            res.set_json("{\"success\":false,\"code\":\"AUTH_TOKEN_MISSING\",\"error\":\"Vui lòng cung cấp token xác thực qua Authorization: Bearer <token>.\"}", 401);
            return std::nullopt;
        }
        auto val = auth_service.validate_token(token);
        if (!val.success) {
            res.set_json(val.to_json(), val.status_code);
            return std::nullopt;
        }
        if (!allowed_roles.empty()) {
            bool role_ok = false;
            for (const auto& r : allowed_roles) {
                if (val.user.role == r) {
                    role_ok = true;
                    break;
                }
            }
            if (!role_ok) {
                res.set_json("{\"success\":false,\"code\":\"FORBIDDEN_ROLE\",\"error\":\"Bạn không có quyền thực hiện thao tác này (yêu cầu vai trò phù hợp).\"}", 403);
                return std::nullopt;
            }
        }
        return val.user;
    };

    // Route: Create Trip Request (POST /api/trips/request)
    auto handle_trip_request = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        auto user = authenticate_user(req, res, {"customer", "admin"});
        if (!user) return;

        // Check if customer already has an active trip
        goride::trip::Trip existing_trip;
        if (trip_service.get_active_trip_for_customer(user->id, existing_trip)) {
            std::ostringstream ss;
            ss << "{\n"
               << "  \"success\": false,\n"
               << "  \"code\": \"ACTIVE_TRIP_EXISTS\",\n"
               << "  \"error\": \"Bạn đang có một chuyến xe đang hoạt động, không thể tạo thêm chuyến mới.\",\n"
               << "  \"active_trip_id\": \"" << existing_trip.id << "\"\n"
               << "}";
            res.set_json(ss.str(), 409);
            return;
        }

        std::string err;
        auto root = goride::utils::JsonValue::parse(req.body, err);
        if (!err.empty() || !root.is_object()) {
            res.set_json("{\"success\":false,\"code\":\"INVALID_JSON\",\"error\":\"Dữ liệu gửi lên không đúng định dạng JSON.\"}", 400);
            return;
        }

        std::string vehicle_type = root.has_key("vehicleType") ? root["vehicleType"].as_string() : root["vehicle_type"].as_string();
        if (vehicle_type != "motorbike" && vehicle_type != "car4" && vehicle_type != "car4_plus" && vehicle_type != "car7") {
            res.set_json("{\"success\":false,\"code\":\"INVALID_VEHICLE_TYPE\",\"error\":\"Loại phương tiện không hợp lệ (hỗ trợ: motorbike, car4, car4_plus, car7).\"}", 400);
            return;
        }

        double p_lat = root.has_key("pickupLat") ? root["pickupLat"].as_double() : root["pickup_lat"].as_double();
        double p_lon = root.has_key("pickupLon") ? root["pickupLon"].as_double() : root["pickup_lon"].as_double();
        double d_lat = root.has_key("dropoffLat") ? root["dropoffLat"].as_double() : root["dropoff_lat"].as_double();
        double d_lon = root.has_key("dropoffLon") ? root["dropoffLon"].as_double() : root["dropoff_lon"].as_double();

        if (std::isnan(p_lat) || std::isnan(p_lon) || std::isnan(d_lat) || std::isnan(d_lon) ||
            std::isinf(p_lat) || std::isinf(p_lon) || std::isinf(d_lat) || std::isinf(d_lon) ||
            p_lat < -90.0 || p_lat > 90.0 || d_lat < -90.0 || d_lat > 90.0 ||
            p_lon < -180.0 || p_lon > 180.0 || d_lon < -180.0 || d_lon > 180.0 ||
            (p_lat == 0.0 && p_lon == 0.0) || (d_lat == 0.0 && d_lon == 0.0)) {
            res.set_json("{\"success\":false,\"code\":\"INVALID_COORDINATES\",\"error\":\"Tọa độ GPS điểm đón hoặc điểm trả không hợp lệ (WGS84).\"}", 400);
            return;
        }

        int distance_m = static_cast<int>(root.has_key("distanceMeters") ? root["distanceMeters"].as_int64(0) : root["distance_meters"].as_int64(0));
        int fare = static_cast<int>(root.has_key("fareAmount") ? root["fareAmount"].as_int64(0) : root["fare_amount"].as_int64(0));

        if (distance_m <= 0 || fare <= 0) {
            res.set_json("{\"success\":false,\"code\":\"INVALID_DISTANCE_OR_FARE\",\"error\":\"Khoảng cách và giá cước chuyến đi phải lớn hơn 0.\"}", 400);
            return;
        }

        // Distance physical feasibility validation (lower bound against Haversine great-circle distance)
        double haversine_m = goride::dsa::SpatialGrid::haversine_distance_m(p_lat, p_lon, d_lat, d_lon);
        if (haversine_m > 50.0 && distance_m < static_cast<int>(haversine_m * 0.7)) {
            res.set_json("{\"success\":false,\"code\":\"INVALID_DISTANCE\",\"error\":\"Khoảng cách lộ trình không thể nhỏ hơn khoảng cách đường chim bay thực tế.\"}", 400);
            return;
        }

        // Server-authoritative fare calculation and validation
        double dist_km = distance_m / 1000.0;
        int expected_fare = 0;
        if (vehicle_type == "motorbike") {
            expected_fare = static_cast<int>(std::round(14000.0 + dist_km * 5500.0));
        } else if (vehicle_type == "car4") {
            expected_fare = static_cast<int>(std::round(26000.0 + dist_km * 11500.0));
        } else if (vehicle_type == "car4_plus") {
            expected_fare = static_cast<int>(std::round(45000.0 + dist_km * 18500.0));
        } else if (vehicle_type == "car7") {
            expected_fare = static_cast<int>(std::round(35000.0 + dist_km * 14500.0));
        }
        // Round to nearest 1,000 VND
        expected_fare = ((expected_fare + 500) / 1000) * 1000;

        // Check for fare tampering (allow max 35% discount or 50% surge from catalog expected fare)
        double min_allowed = expected_fare * 0.65;
        double max_allowed = expected_fare * 1.50;
        if (fare < min_allowed || fare > max_allowed) {
            res.set_json("{\"success\":false,\"code\":\"INVALID_FARE\",\"error\":\"Giá cước không hợp lệ hoặc có dấu hiệu can thiệp giá.\"}", 400);
            return;
        }

        std::string p_name = root.has_key("pickupName") ? root["pickupName"].as_string() : root["pickup_name"].as_string();
        std::string d_name = root.has_key("dropoffName") ? root["dropoffName"].as_string() : root["dropoff_name"].as_string();
        std::string pay_method = root.has_key("paymentMethod") ? root["paymentMethod"].as_string() : root["payment_method"].as_string("cash");

        goride::trip::Trip trip;
        trip.id = generate_trip_id();
        trip.customer_id = user->id;
        trip.vehicle_type = vehicle_type;
        trip.pickup_name = p_name.empty() ? "Điểm đón" : p_name;
        trip.pickup_lat = p_lat;
        trip.pickup_lon = p_lon;
        trip.dropoff_name = d_name.empty() ? "Điểm đến" : d_name;
        trip.dropoff_lat = d_lat;
        trip.dropoff_lon = d_lon;
        trip.distance_meters = distance_m;
        trip.fare_amount = fare;
        trip.payment_method = pay_method.empty() ? "cash" : pay_method;

        std::string trip_err;
        if (!trip_service.create_trip(trip, trip_err)) {
            if (trip_err == "Customer already has an active trip") {
                res.set_json("{\"success\":false,\"code\":\"ACTIVE_TRIP_EXISTS\",\"error\":\"Bạn đang có một chuyến xe đang hoạt động, không thể tạo thêm chuyến mới.\"}", 409);
                return;
            }
            res.set_json(std::string("{\"success\":false,\"code\":\"CREATE_TRIP_FAILED\",\"error\":\"") + trip_err + "\"}", 500);
            return;
        }

        std::ostringstream ss;
        ss << "{\n"
           << "  \"success\": true,\n"
           << "  \"message\": \"Tạo yêu cầu chuyến xe thành công.\",\n"
           << "  \"trip\": " << trip.to_json() << "\n"
           << "}";
        res.set_json(ss.str(), 201);
    };

    server.post("/api/trips/request", handle_trip_request);
    server.post("/api/v1/trips/request", handle_trip_request);
    server.post("/trips/request", handle_trip_request);

    // Route: Get Active Trip (GET /api/trips/active)
    auto handle_active_trip = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        auto user = authenticate_user(req, res);
        if (!user) return;

        goride::trip::Trip active_trip;
        bool has_active = false;
        if (user->role == "customer") {
            has_active = trip_service.get_active_trip_for_customer(user->id, active_trip);
        } else if (user->role == "driver") {
            has_active = trip_service.get_active_trip_for_driver(user->id, active_trip);
        } else {
            std::string target_uid = req.get_param("user_id", req.get_param("customer_id", req.get_param("driver_id")));
            if (!target_uid.empty()) {
                has_active = trip_service.get_active_trip_for_customer(target_uid, active_trip) ||
                             trip_service.get_active_trip_for_driver(target_uid, active_trip);
            }
        }

        std::ostringstream ss;
        if (has_active) {
            ss << "{\n"
               << "  \"success\": true,\n"
               << "  \"has_active_trip\": true,\n"
               << "  \"trip\": " << active_trip.to_json() << "\n"
               << "}";
        } else {
            ss << "{\n"
               << "  \"success\": true,\n"
               << "  \"has_active_trip\": false,\n"
               << "  \"trip\": null\n"
               << "}";
        }
        res.set_json(ss.str(), 200);
    };

    server.get("/api/trips/active", handle_active_trip);
    server.get("/api/v1/trips/active", handle_active_trip);
    server.get("/trips/active", handle_active_trip);

    // Route: Get Trip Detail (GET /api/trips/detail)
    auto handle_trip_detail = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        auto user = authenticate_user(req, res);
        if (!user) return;

        std::string trip_id = req.get_param("id", req.get_param("trip_id"));
        if (trip_id.empty()) {
            res.set_json("{\"success\":false,\"code\":\"MISSING_TRIP_ID\",\"error\":\"Vui lòng cung cấp mã chuyến đi (id hoặc trip_id).\"}", 400);
            return;
        }

        goride::trip::Trip trip;
        if (!trip_service.get_trip(trip_id, trip)) {
            res.set_json("{\"success\":false,\"code\":\"TRIP_NOT_FOUND\",\"error\":\"Không tìm thấy chuyến xe có mã tương ứng.\"}", 404);
            return;
        }

        if (user->role != "admin" && user->id != trip.customer_id && user->id != trip.driver_id) {
            res.set_json("{\"success\":false,\"code\":\"FORBIDDEN_ACCESS\",\"error\":\"Bạn không có quyền xem thông tin chuyến xe này.\"}", 403);
            return;
        }

        std::ostringstream ss;
        ss << "{\n"
           << "  \"success\": true,\n"
           << "  \"trip\": " << trip.to_json() << "\n"
           << "}";
        res.set_json(ss.str(), 200);
    };

    server.get("/api/trips/detail", handle_trip_detail);
    server.get("/api/v1/trips/detail", handle_trip_detail);
    server.get("/trips/detail", handle_trip_detail);

    // Route: Cancel Trip (POST /api/trips/cancel)
    auto handle_trip_cancel = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        auto user = authenticate_user(req, res);
        if (!user) return;

        std::string trip_id, reason;
        std::string err;
        auto root = goride::utils::JsonValue::parse(req.body, err);
        if (err.empty() && root.is_object()) {
            if (root.has_key("trip_id")) trip_id = root["trip_id"].as_string();
            else if (root.has_key("tripId")) trip_id = root["tripId"].as_string();
            else if (root.has_key("id")) trip_id = root["id"].as_string();
            if (root.has_key("reason")) reason = root["reason"].as_string();
        }
        if (trip_id.empty()) {
            trip_id = req.get_param("trip_id", req.get_param("id"));
            if (reason.empty()) reason = req.get_param("reason");
        }

        if (trip_id.empty()) {
            res.set_json("{\"success\":false,\"code\":\"MISSING_TRIP_ID\",\"error\":\"Vui lòng cung cấp mã chuyến đi cần hủy (trip_id).\"}", 400);
            return;
        }

        goride::trip::Trip trip;
        if (!trip_service.get_trip(trip_id, trip)) {
            res.set_json("{\"success\":false,\"code\":\"TRIP_NOT_FOUND\",\"error\":\"Không tìm thấy chuyến xe có mã tương ứng.\"}", 404);
            return;
        }

        if (user->role != "admin" && user->id != trip.customer_id && user->id != trip.driver_id) {
            res.set_json("{\"success\":false,\"code\":\"FORBIDDEN_ACCESS\",\"error\":\"Bạn không có quyền hủy chuyến xe của người khác.\"}", 403);
            return;
        }

        std::string cancel_err;
        if (!trip_service.cancel_trip(trip_id, user->id, user->role, reason, cancel_err)) {
            int status_code = 400;
            std::string code = "CANCEL_FAILED";
            if (cancel_err.find("already terminated") != std::string::npos) {
                status_code = 409;
                code = "TRIP_ALREADY_TERMINATED";
            } else if (cancel_err.find("already IN_PROGRESS") != std::string::npos) {
                status_code = 403;
                code = "CANNOT_CANCEL_IN_PROGRESS";
            }
            res.set_json(std::string("{\"success\":false,\"code\":\"") + code + "\",\"error\":\"" + cancel_err + "\"}", status_code);
            return;
        }

        std::ostringstream ss;
        ss << "{\n"
           << "  \"success\": true,\n"
           << "  \"message\": \"Hủy chuyến xe thành công.\",\n"
           << "  \"trip_id\": \"" << trip_id << "\",\n"
           << "  \"cancelled_by\": \"" << user->role << "\"\n"
           << "}";
        res.set_json(ss.str(), 200);
    };

    server.post("/api/trips/cancel", handle_trip_cancel);
    server.post("/api/v1/trips/cancel", handle_trip_cancel);
    server.post("/trips/cancel", handle_trip_cancel);

    // Route: Get Pending Driver Offer (GET /api/driver/offers/pending)
    auto handle_driver_pending_offer = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        auto user = authenticate_user(req, res, {"driver"});
        if (!user) return;

        goride::trip::Trip offered_trip;
        bool has_offer = false;
        int64_t remaining_s = 0;
        if (trip_service.get_active_trip_for_driver(user->id, offered_trip)) {
            if (offered_trip.status == goride::trip::TripStatus::OFFERED &&
                offered_trip.current_offered_to == user->id) {
                auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                if (offered_trip.offer_expires_at >= now) {
                    has_offer = true;
                    remaining_s = offered_trip.offer_expires_at - now;
                }
            }
        }

        std::ostringstream ss;
        if (has_offer) {
            ss << "{\n"
               << "  \"success\": true,\n"
               << "  \"has_offer\": true,\n"
               << "  \"remaining_seconds\": " << remaining_s << ",\n"
               << "  \"trip\": " << offered_trip.to_json() << "\n"
               << "}";
        } else {
            ss << "{\n"
               << "  \"success\": true,\n"
               << "  \"has_offer\": false,\n"
               << "  \"trip\": null\n"
               << "}";
        }
        res.set_json(ss.str(), 200);
    };

    server.get("/api/driver/offers/pending", handle_driver_pending_offer);
    server.get("/api/v1/driver/offers/pending", handle_driver_pending_offer);
    server.get("/driver/offers/pending", handle_driver_pending_offer);

    // Route: Driver Accept Offer (POST /api/driver/offers/accept)
    auto handle_driver_accept_offer = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        auto user = authenticate_user(req, res, {"driver"});
        if (!user) return;

        std::string trip_id;
        std::string err;
        auto root = goride::utils::JsonValue::parse(req.body, err);
        if (err.empty() && root.is_object()) {
            if (root.has_key("trip_id")) trip_id = root["trip_id"].as_string();
            else if (root.has_key("tripId")) trip_id = root["tripId"].as_string();
            else if (root.has_key("id")) trip_id = root["id"].as_string();
        }
        if (trip_id.empty()) {
            trip_id = req.get_param("trip_id", req.get_param("id"));
        }

        if (trip_id.empty()) {
            res.set_json("{\"success\":false,\"code\":\"MISSING_TRIP_ID\",\"error\":\"Vui lòng cung cấp mã chuyến đi cần nhận (trip_id).\"}", 400);
            return;
        }

        std::string accept_err;
        if (!trip_service.accept_offer(trip_id, user->id, accept_err)) {
            res.set_json(std::string("{\"success\":false,\"code\":\"OFFER_EXPIRED\",\"error\":\"") + accept_err + "\"}", 409);
            return;
        }

        std::ostringstream ss;
        ss << "{\n"
           << "  \"success\": true,\n"
           << "  \"message\": \"Chấp nhận chuyến đi thành công! Hãy di chuyển tới điểm đón khách.\",\n"
           << "  \"trip_id\": \"" << trip_id << "\",\n"
           << "  \"status\": \"ACCEPTED\"\n"
           << "}";
        res.set_json(ss.str(), 200);
    };

    server.post("/api/driver/offers/accept", handle_driver_accept_offer);
    server.post("/api/v1/driver/offers/accept", handle_driver_accept_offer);
    server.post("/driver/offers/accept", handle_driver_accept_offer);

    // Route: Driver Reject Offer (POST /api/driver/offers/reject)
    auto handle_driver_reject_offer = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        auto user = authenticate_user(req, res, {"driver"});
        if (!user) return;

        std::string trip_id;
        std::string err;
        auto root = goride::utils::JsonValue::parse(req.body, err);
        if (err.empty() && root.is_object()) {
            if (root.has_key("trip_id")) trip_id = root["trip_id"].as_string();
            else if (root.has_key("tripId")) trip_id = root["tripId"].as_string();
            else if (root.has_key("id")) trip_id = root["id"].as_string();
        }
        if (trip_id.empty()) {
            trip_id = req.get_param("trip_id", req.get_param("id"));
        }

        if (trip_id.empty()) {
            res.set_json("{\"success\":false,\"code\":\"MISSING_TRIP_ID\",\"error\":\"Vui lòng cung cấp mã chuyến đi cần từ chối (trip_id).\"}", 400);
            return;
        }

        std::string reject_err;
        if (!trip_service.reject_offer(trip_id, user->id, reject_err)) {
            res.set_json(std::string("{\"success\":false,\"code\":\"REJECT_FAILED\",\"error\":\"") + reject_err + "\"}", 409);
            return;
        }

        std::ostringstream ss;
        ss << "{\n"
           << "  \"success\": true,\n"
           << "  \"message\": \"Đã từ chối nhận cuốc xe.\",\n"
           << "  \"trip_id\": \"" << trip_id << "\"\n"
           << "}";
        res.set_json(ss.str(), 200);
    };

    server.post("/api/driver/offers/reject", handle_driver_reject_offer);
    server.post("/api/v1/driver/offers/reject", handle_driver_reject_offer);
    server.post("/driver/offers/reject", handle_driver_reject_offer);

    // Route: Driver Update Trip Status (POST /api/driver/trips/status)
    auto handle_driver_trip_status = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        auto user = authenticate_user(req, res, {"driver"});
        if (!user) return;

        std::string trip_id, action, status_val;
        std::string err;
        auto root = goride::utils::JsonValue::parse(req.body, err);
        if (err.empty() && root.is_object()) {
            if (root.has_key("trip_id")) trip_id = root["trip_id"].as_string();
            else if (root.has_key("tripId")) trip_id = root["tripId"].as_string();
            else if (root.has_key("id")) trip_id = root["id"].as_string();
            if (root.has_key("status")) status_val = root["status"].as_string();
            if (root.has_key("action")) action = root["action"].as_string();
        }
        if (trip_id.empty()) trip_id = req.get_param("trip_id", req.get_param("id"));
        if (status_val.empty()) status_val = req.get_param("status");
        if (action.empty()) action = req.get_param("action");

        if (trip_id.empty()) {
            res.set_json("{\"success\":false,\"code\":\"MISSING_TRIP_ID\",\"error\":\"Vui lòng cung cấp mã chuyến đi (trip_id).\"}", 400);
            return;
        }

        std::string target_status;
        if (action == "arrived" || status_val == "ARRIVED") {
            target_status = "ARRIVED";
        } else if (action == "start" || action == "start_trip" || status_val == "IN_PROGRESS") {
            target_status = "IN_PROGRESS";
        } else if (action == "complete" || action == "complete_trip" || status_val == "COMPLETED") {
            target_status = "COMPLETED";
        } else {
            res.set_json("{\"success\":false,\"code\":\"INVALID_STATUS_ACTION\",\"error\":\"Hành động cập nhật trạng thái không hợp lệ (chỉ chấp nhận arrived, start, complete).\"}", 400);
            return;
        }

        std::string status_err;
        bool ok = false;
        if (target_status == "ARRIVED") {
            ok = trip_service.driver_arrive(trip_id, user->id, status_err);
        } else if (target_status == "IN_PROGRESS") {
            ok = trip_service.start_trip(trip_id, user->id, status_err);
        } else if (target_status == "COMPLETED") {
            ok = trip_service.complete_trip(trip_id, user->id, status_err);
        }

        if (!ok) {
            res.set_json(std::string("{\"success\":false,\"code\":\"INVALID_STATUS_TRANSITION\",\"error\":\"") + status_err + "\"}", 409);
            return;
        }

        std::ostringstream ss;
        ss << "{\n"
           << "  \"success\": true,\n"
           << "  \"message\": \"Cập nhật trạng thái chuyến xe thành công.\",\n"
           << "  \"trip_id\": \"" << trip_id << "\",\n"
           << "  \"new_status\": \"" << target_status << "\"\n"
           << "}";
        res.set_json(ss.str(), 200);
    };

    server.post("/api/driver/trips/status", handle_driver_trip_status);
    server.post("/api/v1/driver/trips/status", handle_driver_trip_status);
    server.post("/driver/trips/status", handle_driver_trip_status);

    // Route: Admin List All Active Trips (GET /api/admin/trips)
    auto handle_admin_trips = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        auto user = authenticate_user(req, res, {"admin"});
        if (!user) return;

        auto trips = trip_service.list_active_trips();
        std::ostringstream ss;
        ss << "{\n"
           << "  \"success\": true,\n"
           << "  \"total\": " << trips.size() << ",\n"
           << "  \"trips\": [\n";
        for (size_t i = 0; i < trips.size(); ++i) {
            ss << "    " << trips[i].to_json();
            if (i + 1 < trips.size()) ss << ",";
            ss << "\n";
        }
        ss << "  ]\n}";
        res.set_json(ss.str(), 200);
    };

    server.get("/api/admin/trips", handle_admin_trips);
    server.get("/api/v1/admin/trips", handle_admin_trips);
    server.get("/admin/trips", handle_admin_trips);

    // Route: Admin Offer Dispatch (POST /api/admin/trips/offer)
    auto handle_admin_offer = [&](const goride::server::HttpRequest& req, goride::server::HttpResponse& res) {
        auto user = authenticate_user(req, res, {"admin"});
        if (!user) return;

        std::string trip_id, driver_id;
        int duration = 15;
        std::string err;
        auto root = goride::utils::JsonValue::parse(req.body, err);
        if (err.empty() && root.is_object()) {
            if (root.has_key("trip_id")) trip_id = root["trip_id"].as_string();
            else if (root.has_key("tripId")) trip_id = root["tripId"].as_string();
            if (root.has_key("driver_id")) driver_id = root["driver_id"].as_string();
            else if (root.has_key("driverId")) driver_id = root["driverId"].as_string();
            if (root.has_key("duration")) duration = static_cast<int>(root["duration"].as_int64(15));
            else if (root.has_key("duration_seconds")) duration = static_cast<int>(root["duration_seconds"].as_int64(15));
        }
        if (trip_id.empty()) trip_id = req.get_param("trip_id");
        if (driver_id.empty()) driver_id = req.get_param("driver_id");

        if (trip_id.empty() || driver_id.empty()) {
            res.set_json("{\"success\":false,\"code\":\"MISSING_PARAMETERS\",\"error\":\"Vui lòng cung cấp đầy đủ trip_id và driver_id.\"}", 400);
            return;
        }

        std::string offer_err;
        if (!trip_service.offer_trip(trip_id, driver_id, duration, offer_err)) {
            res.set_json(std::string("{\"success\":false,\"code\":\"OFFER_DISPATCH_FAILED\",\"error\":\"") + offer_err + "\"}", 409);
            return;
        }

        std::ostringstream ss;
        ss << "{\n"
           << "  \"success\": true,\n"
           << "  \"message\": \"Gửi offer cho tài xế thành công.\",\n"
           << "  \"trip_id\": \"" << trip_id << "\",\n"
           << "  \"driver_id\": \"" << driver_id << "\",\n"
           << "  \"duration_seconds\": " << duration << "\n"
           << "}";
        res.set_json(ss.str(), 200);
    };

    server.post("/api/admin/trips/offer", handle_admin_offer);
    server.post("/api/v1/admin/trips/offer", handle_admin_offer);

    // 4. Start HTTP Server
    std::cout << "\n==========================================================" << std::endl;
    std::cout << "  Server starting on http://localhost:" << port << " /" << std::endl;
    std::cout << "  Health Check: http://localhost:" << port << "/api/health" << std::endl;
    std::cout << "  Route API:    http://localhost:" << port << "/api/route" << std::endl;
    std::cout << "  Auth API:     http://localhost:" << port << "/api/auth/login" << std::endl;
    std::cout << "==========================================================" << std::endl;

    if (!server.start(port, host)) {
        std::cerr << "[FATAL] Failed to start HTTP server on " << host << ":" << port << std::endl;
        return 1;
    }

    return 0;
}
