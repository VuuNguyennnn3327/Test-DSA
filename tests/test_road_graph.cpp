#include "dsa/dijkstra.hpp"
#include "dsa/road_graph.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace goride::dsa;

namespace {

int total_tests = 0;
int passed_tests = 0;

void record_test(const std::string& name, bool condition, const std::string& details = "") {
    ++total_tests;
    std::cout << "[TEST " << total_tests << "] " << name << ": ";
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

// TEST 1: Small graph with known shortest path
void run_test_known_shortest_path() {
    std::cout << "\n--- TEST SUITE 1: Known Shortest Path on Small Graph ---" << std::endl;
    RoadGraph g;
    // Nodes 1 to 6
    g.add_node(1, 10.70, 106.60);
    g.add_node(2, 10.71, 106.61);
    g.add_node(3, 10.72, 106.62);
    g.add_node(4, 10.73, 106.63);
    g.add_node(5, 10.74, 106.64);
    g.add_node(6, 10.75, 106.65);

    // Edges with known shortest path: 1 -> 3 -> 6 -> 5
    // Weights:
    // 1 -> 2: 7.0
    // 1 -> 3: 9.0
    // 1 -> 6: 14.0
    // 2 -> 3: 10.0
    // 2 -> 4: 15.0
    // 3 -> 4: 11.0
    // 3 -> 6: 2.0
    // 4 -> 5: 6.0
    // 6 -> 5: 9.0
    // Paths 1 to 5:
    // 1->6->5 = 14 + 9 = 23
    // 1->3->6->5 = 9 + 2 + 9 = 20  <-- SHORTEST
    // 1->2->4->5 = 7 + 15 + 6 = 28
    // 1->3->4->5 = 9 + 11 + 6 = 26
    g.add_edge(1, 2, 7.0, 101, "Route 1-2");
    g.add_edge(1, 3, 9.0, 102, "Route 1-3");
    g.add_edge(1, 6, 14.0, 103, "Route 1-6");
    g.add_edge(2, 3, 10.0, 104, "Route 2-3");
    g.add_edge(2, 4, 15.0, 105, "Route 2-4");
    g.add_edge(3, 4, 11.0, 106, "Route 3-4");
    g.add_edge(3, 6, 2.0, 107, "Route 3-6");
    g.add_edge(4, 5, 6.0, 108, "Route 4-5");
    g.add_edge(6, 5, 9.0, 109, "Route 6-5");

    DijkstraRouter router(g);
    auto res = router.find_shortest_path(1, 5);

    record_test("Path 1 -> 5 is found", res.found);
    record_test("Distance 1 -> 5 is exactly 20.0", std::abs(res.total_distance - 20.0) < 1e-6,
                "got " + std::to_string(res.total_distance));
    std::vector<NodeId> expected_nodes = {1, 3, 6, 5};
    record_test("Node sequence is [1, 3, 6, 5]", res.path_nodes == expected_nodes);
    record_test("Edge count traversed is 3", res.path_edges.size() == 3);
}

// TEST 2: One-way enforcement
void run_test_oneway_enforcement() {
    std::cout << "\n--- TEST SUITE 2: One-Way Road Directionality ---" << std::endl;
    RoadGraph g;
    g.add_node(10, 10.77, 106.69);
    g.add_node(20, 10.78, 106.70);

    // Only 10 -> 20 exists, oneway = true
    g.add_edge(10, 20, 150.0, 201, "Nam Ky Khoi Nghia", "primary", true);

    DijkstraRouter router(g);
    auto forward = router.find_shortest_path(10, 20);
    record_test("Forward oneway 10 -> 20 succeeds", forward.found && std::abs(forward.total_distance - 150.0) < 1e-6);

    auto reverse = router.find_shortest_path(20, 10);
    record_test("Reverse oneway 20 -> 10 is blocked", !reverse.found, reverse.error_message);
    record_test("Reverse oneway distance is infinite", std::isinf(reverse.total_distance));
}

// TEST 3: Disconnected graph / Unreachable nodes
void run_test_unreachable_cases() {
    std::cout << "\n--- TEST SUITE 3: Unreachable and Invalid Node Queries ---" << std::endl;
    RoadGraph g;
    g.add_node(100, 10.1, 106.1);
    g.add_node(101, 10.2, 106.2);
    g.add_node(200, 10.8, 106.8);
    g.add_node(201, 10.9, 106.9);

    g.add_edge(100, 101, 50.0, 301, "Island A");
    g.add_edge(200, 201, 60.0, 302, "Island B");

    DijkstraRouter router(g);
    auto disconnected = router.find_shortest_path(100, 201);
    record_test("Disconnected components 100 -> 201 return not found", !disconnected.found);

    auto non_existent_src = router.find_shortest_path(9999, 101);
    record_test("Non-existent source node rejected gracefully", !non_existent_src.found, non_existent_src.error_message);

    auto non_existent_tgt = router.find_shortest_path(100, 8888);
    record_test("Non-existent target node rejected gracefully", !non_existent_tgt.found, non_existent_tgt.error_message);
}

// TEST 4: Source equals Target
void run_test_source_equals_target() {
    std::cout << "\n--- TEST SUITE 4: Source Equals Target (Trivial Path) ---" << std::endl;
    RoadGraph g;
    g.add_node(42, 10.7725, 106.6980);
    g.add_node(43, 10.7750, 106.7000);
    g.add_edge(42, 43, 100.0, 401, "Test Way");

    DijkstraRouter router(g);
    auto res = router.find_shortest_path(42, 42);

    record_test("Source == Target is found", res.found);
    record_test("Distance is 0.0", res.total_distance == 0.0);
    std::vector<NodeId> expected_nodes = {42};
    record_test("Path nodes contains only [42]", res.path_nodes == expected_nodes);
    record_test("Path edges is empty", res.path_edges.empty());
    record_test("Geometry contains exactly 1 coordinate point", res.path_geometry.size() == 1);
    record_test("Visited nodes count is 1", res.nodes_visited == 1);
}

// TEST 5: Weight sum consistency and geometry ordering
void run_test_weight_sum_and_geometry() {
    std::cout << "\n--- TEST SUITE 5: Weight Sum Consistency & Geometry Order ---" << std::endl;
    RoadGraph g;
    g.add_node(1, 10.7710, 106.6910);
    g.add_node(2, 10.7720, 106.6920);
    g.add_node(3, 10.7730, 106.6930);
    g.add_node(4, 10.7740, 106.6940);

    double w1 = 123.45;
    double w2 = 67.89;
    double w3 = 314.16;

    g.add_edge(1, 2, w1, 501, "Seg 1");
    g.add_edge(2, 3, w2, 502, "Seg 2");
    g.add_edge(3, 4, w3, 503, "Seg 3");
    g.add_edge(1, 4, 1000.0, 504, "Expensive direct");

    DijkstraRouter router(g);
    auto res = router.find_shortest_path(1, 4);

    record_test("Path 1 -> 4 found", res.found);

    double manual_sum = 0.0;
    for (const auto& e : res.path_edges) {
        manual_sum += e.weight;
    }

    record_test("Total distance equals sum of traversed edge weights",
                std::abs(res.total_distance - manual_sum) < 1e-9,
                "diff = " + std::to_string(std::abs(res.total_distance - manual_sum)));

    record_test("Total distance matches expected 505.50 m",
                std::abs(res.total_distance - (w1 + w2 + w3)) < 1e-6);

    record_test("Geometry points count equals nodes count (4)", res.path_geometry.size() == 4);

    // Verify geometry strictly orders 1 -> 2 -> 3 -> 4
    bool geo_order = true;
    if (res.path_geometry.size() == 4) {
        if (std::abs(res.path_geometry[0].first - 10.7710) > 1e-4) geo_order = false;
        if (std::abs(res.path_geometry[1].first - 10.7720) > 1e-4) geo_order = false;
        if (std::abs(res.path_geometry[2].first - 10.7730) > 1e-4) geo_order = false;
        if (std::abs(res.path_geometry[3].first - 10.7740) > 1e-4) geo_order = false;
    } else {
        geo_order = false;
    }
    record_test("Geometry points follow exact forward node order", geo_order);
}

// TEST 6: Real OSM Data Ingestion and Routing
void run_test_real_osm_ingestion(const std::string& osm_json_path) {
    std::cout << "\n--- TEST SUITE 6: Ingestion of Real Processed OSM Data ---" << std::endl;
    std::cout << "[INFO] Loading dataset from: " << osm_json_path << std::endl;

    RoadGraph g;
    auto load_res = g.load_from_json_file(osm_json_path);

    record_test("OSM JSON loaded successfully", load_res.success);

    // Invariant: Loaded nodes count matches graph vertex catalog and is non-empty
    bool nodes_valid = (load_res.nodes_loaded == g.node_count()) && (load_res.nodes_loaded >= 80);
    record_test("Loaded nodes count matches graph vertex catalog", nodes_valid,
                "loaded " + std::to_string(load_res.nodes_loaded) + " vertices");

    // Invariant: Loaded edges count matches graph edge inventory and is non-empty
    bool edges_valid = (load_res.edges_loaded == g.edge_count()) && (load_res.edges_loaded >= 74);
    record_test("Loaded directed edges count matches graph edge inventory", edges_valid,
                "loaded " + std::to_string(load_res.edges_loaded) + " edges");

    record_test("Zero malformed records", load_res.malformed_records == 0,
                "malformed = " + std::to_string(load_res.malformed_records));
    record_test("Zero missing node references", load_res.missing_node_references == 0,
                "missing = " + std::to_string(load_res.missing_node_references));
    record_test("Zero non-positive weight edges", load_res.non_positive_weight_edges == 0,
                "non-positive = " + std::to_string(load_res.non_positive_weight_edges));

    // Test real routing along Cách Mạng Tháng Tám (Way 1549466500)
    // From Bach Khoa (5467431341) to To Hien Thanh end (14092962354)
    NodeId start_nid = 5467431341;
    NodeId end_nid = 14092962354;

    DijkstraRouter router(g);
    auto cmtt_forward = router.find_shortest_path(start_nid, end_nid);
    record_test("Dijkstra route on CMTT arterial 5467431341 -> 14092962354 succeeds", cmtt_forward.found);
    record_test("CMTT route traverses 9 edge segments", cmtt_forward.path_edges.size() == 9);
    record_test("CMTT route geometry contains 10 ordered vertices", cmtt_forward.path_geometry.size() == 10);

    // 1. Direct Adjacency Verification:
    // Verify that every segment along the one-way CMTT forward path has NO direct reverse edge in the graph
    bool direct_reverse_forbidden = true;
    for (const auto& fwd_edge : cmtt_forward.path_edges) {
        if (fwd_edge.oneway) {
            for (const auto& rev_cand : g.get_outgoing_edges(fwd_edge.v)) {
                if (rev_cand.v == fwd_edge.u && rev_cand.way_id == fwd_edge.way_id) {
                    direct_reverse_forbidden = false;
                    break;
                }
            }
        }
    }
    record_test("One-way CMTT segments have NO direct reverse edges in adjacency list", direct_reverse_forbidden);

    // 2. Reachability on Isolated CMTT Corridor:
    // Build an isolated corridor graph containing ONLY the vertices and directed edges of the CMTT arterial.
    // In this corridor without city detours, reverse traversal is 100% BLOCKED.
    RoadGraph cmtt_corridor;
    for (NodeId nid : cmtt_forward.path_nodes) {
        const Node* n = g.get_node(nid);
        if (n) cmtt_corridor.add_node(*n);
    }
    for (const auto& e : cmtt_forward.path_edges) {
        cmtt_corridor.add_edge(e);
    }
    DijkstraRouter corridor_router(cmtt_corridor);
    auto corridor_reverse = corridor_router.find_shortest_path(end_nid, start_nid);
    record_test("Reverse traversal on isolated one-way CMTT corridor is BLOCKED", !corridor_reverse.found,
                corridor_reverse.error_message);

    // 3. Full Graph Detour Verification:
    // On the full city network, Dijkstra may find a legal detour route via parallel streets (e.g. To Hien Thanh -> Ly Thuong Kiet).
    // Verify that if a detour route exists, it strictly obeys one-way restrictions and DOES NOT drive backwards on CMTT.
    auto city_reverse = router.find_shortest_path(end_nid, start_nid);
    if (city_reverse.found) {
        bool violates_cmtt_oneway = false;
        for (const auto& rev_e : city_reverse.path_edges) {
            for (const auto& fwd_e : cmtt_forward.path_edges) {
                if (rev_e.u == fwd_e.v && rev_e.v == fwd_e.u && rev_e.way_id == fwd_e.way_id) {
                    violates_cmtt_oneway = true;
                    break;
                }
            }
        }
        record_test("City reverse detour path does NOT violate one-way direction on CMTT", !violates_cmtt_oneway,
                    "detour distance: " + std::to_string(city_reverse.total_distance) + " m");
    }
}

// TEST 7: Malformed Data and Robust Edge Case Rejection
void run_test_malformed_data_rejection() {
    std::cout << "\n--- TEST SUITE 7: Malformed & Negative Weight Edge Rejection ---" << std::endl;
    RoadGraph g;
    // Malformed JSON string with negative distance, missing u, zero distance, and missing node reference
    std::string bad_json = R"({
        "nodes": {
            "1": [10.77, 106.69],
            "2": [10.78, 106.70]
        },
        "edges": [
            {"u": 1, "v": 2, "distance_m": -25.0, "way_id": 1, "name": "Negative edge"},
            {"u": 1, "v": 2, "distance_m": 0.0, "way_id": 2, "name": "Zero edge"},
            {"u": 1, "v": 999999, "distance_m": 50.0, "way_id": 3, "name": "Missing endpoint"},
            {"v": 2, "distance_m": 30.0, "way_id": 4, "name": "Missing u field"},
            {"u": 1, "v": 2, "distance_m": 42.0, "way_id": 5, "name": "Valid edge"}
        ]
    })";

    auto res = g.load_from_json_string(bad_json);
    record_test("Graph loaded nodes despite bad edges", res.success && res.nodes_loaded == 2);
    record_test("Only 1 valid edge was loaded", res.edges_loaded == 1);
    record_test("Non-positive weights detected (2)", res.non_positive_weight_edges == 2);
    record_test("Missing node reference detected (1)", res.missing_node_references == 1);
    record_test("Malformed record detected (1)", res.malformed_records == 1);
}

} // namespace

int main(int argc, char** argv) {
    std::cout << "=================================================================" << std::endl;
    std::cout << "  GoRide (RIDE-RTREE) - C++17 RoadGraph & Dijkstra Test Suite   " << std::endl;
    std::cout << "=================================================================" << std::endl;

    std::string osm_path = "data/osm/processed/hcmc_roads.json";
    if (argc > 1) {
        osm_path = argv[1];
    }

    run_test_known_shortest_path();
    run_test_oneway_enforcement();
    run_test_unreachable_cases();
    run_test_source_equals_target();
    run_test_weight_sum_and_geometry();
    run_test_real_osm_ingestion(osm_path);
    run_test_malformed_data_rejection();

    std::cout << "\n=================================================================" << std::endl;
    std::cout << "  FINAL TEST SUMMARY: " << passed_tests << " / " << total_tests << " PASSED" << std::endl;
    std::cout << "=================================================================" << std::endl;

    return (passed_tests == total_tests) ? 0 : 1;
}
