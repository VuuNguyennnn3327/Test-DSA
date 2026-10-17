# CMake generated Testfile for 
# Source directory: D:/Nam2_ky1/newDSA/RIDE-RTREE
# Build directory: D:/Nam2_ky1/newDSA/RIDE-RTREE/build-mingw
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(RoadGraphDijkstraTest "D:/Nam2_ky1/newDSA/RIDE-RTREE/build-mingw/bin/test_road_graph.exe" "D:/Nam2_ky1/newDSA/RIDE-RTREE/data/osm/processed/hcmc_roads.json")
set_tests_properties(RoadGraphDijkstraTest PROPERTIES  _BACKTRACE_TRIPLES "D:/Nam2_ky1/newDSA/RIDE-RTREE/CMakeLists.txt;61;add_test;D:/Nam2_ky1/newDSA/RIDE-RTREE/CMakeLists.txt;0;")
add_test(ApiRoutingTest "D:/Nam2_ky1/newDSA/RIDE-RTREE/build-mingw/bin/test_api_routing.exe" "D:/Nam2_ky1/newDSA/RIDE-RTREE/data/osm/processed/hcmc_roads.json")
set_tests_properties(ApiRoutingTest PROPERTIES  _BACKTRACE_TRIPLES "D:/Nam2_ky1/newDSA/RIDE-RTREE/CMakeLists.txt;62;add_test;D:/Nam2_ky1/newDSA/RIDE-RTREE/CMakeLists.txt;0;")
add_test(AuthSecurityTest "D:/Nam2_ky1/newDSA/RIDE-RTREE/build-mingw/bin/test_auth.exe")
set_tests_properties(AuthSecurityTest PROPERTIES  _BACKTRACE_TRIPLES "D:/Nam2_ky1/newDSA/RIDE-RTREE/CMakeLists.txt;63;add_test;D:/Nam2_ky1/newDSA/RIDE-RTREE/CMakeLists.txt;0;")
