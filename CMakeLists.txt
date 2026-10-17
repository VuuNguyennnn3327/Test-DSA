cmake_minimum_required(VERSION 3.16)
project(RIDE_RTREE VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(backend/include)

# DSA Routing library (Graph, Dijkstra, SpatialGrid, HttpServer, AuthService, Database)
add_library(dsa_routing STATIC
    backend/src/road_graph.cpp
    backend/src/dijkstra.cpp
    backend/src/spatial_grid.cpp
    backend/src/http_server.cpp
    backend/src/database.cpp
    backend/src/auth_service.cpp
    backend/src/trip_service.cpp
)
target_include_directories(dsa_routing PUBLIC backend/include)

if(WIN32)
    target_link_libraries(dsa_routing PUBLIC ws2_32)
endif()

# SQLite3 database integration
# Configured for official MinGW64 package (mingw-w64-x86_64-sqlite3)
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH "C:/msys64/mingw64")
endif()

find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    message(STATUS "[SQLITE3] Official SQLite3 package found: ${SQLite3_VERSION}")
    message(STATUS "[SQLITE3] Include dirs: ${SQLite3_INCLUDE_DIRS}")
    message(STATUS "[SQLITE3] Libraries:    ${SQLite3_LIBRARIES}")
    target_link_libraries(dsa_routing PUBLIC SQLite3::SQLite3)
else()
    find_path(SQLITE3_INCLUDE_DIR NAMES sqlite3.h PATHS "C:/msys64/mingw64/include" "/usr/include" "/usr/local/include")
    find_library(SQLITE3_LIB NAMES sqlite3 libsqlite3 PATHS "C:/msys64/mingw64/lib" "/usr/local/lib" "/usr/lib")
    if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIB)
        message(STATUS "[SQLITE3] Found MinGW64 SQLite3 at: ${SQLITE3_LIB}")
        target_include_directories(dsa_routing PUBLIC "${SQLITE3_INCLUDE_DIR}")
        target_link_libraries(dsa_routing PUBLIC "${SQLITE3_LIB}")
    elseif(WIN32)
        find_file(WINSQLITE3_DLL NAMES winsqlite3.dll PATHS "$ENV{SystemRoot}/System32" "C:/Windows/System32" NO_DEFAULT_PATH)
        if(WINSQLITE3_DLL)
            message(STATUS "[SQLITE3] Notice: Official MinGW64 SQLite3 not found. Linking native winsqlite3.dll at: ${WINSQLITE3_DLL}")
            target_link_libraries(dsa_routing PUBLIC "${WINSQLITE3_DLL}")
        else()
            message(WARNING "[SQLITE3] winsqlite3.dll not found in System32. Falling back to -lsqlite3")
            target_link_libraries(dsa_routing PUBLIC sqlite3)
        endif()
    else()
        message(STATUS "[SQLITE3] Linking default -lsqlite3")
        target_link_libraries(dsa_routing PUBLIC sqlite3)
    endif()
endif()

# Main server executable
add_executable(ride_rtree_server backend/main.cpp)
target_link_libraries(ride_rtree_server PRIVATE dsa_routing)

# Test executable for Phase 2A.2 (RoadGraph & Dijkstra)
add_executable(test_road_graph tests/test_road_graph.cpp)
target_link_libraries(test_road_graph PRIVATE dsa_routing)

# Test executable for Phase 2A.3 (SpatialGrid, API Handlers & HTTP Server)
add_executable(test_api_routing tests/test_api_routing.cpp)
target_link_libraries(test_api_routing PRIVATE dsa_routing)

# Test executable for Phase 2A.4 (Authentication & Security)
add_executable(test_auth tests/test_auth.cpp)
target_link_libraries(test_auth PRIVATE dsa_routing)

# Test executable for Phase C.2 (Trip Lifecycle & TripService Foundation)
add_executable(test_trip tests/test_trip.cpp)
target_link_libraries(test_trip PRIVATE dsa_routing)

# Test executable for Phase C.3 (Trip Lifecycle REST API Foundation & RBAC)
add_executable(test_trip_api tests/test_trip_api.cpp)
target_link_libraries(test_trip_api PRIVATE dsa_routing)

# Enable testing
enable_testing()
add_test(NAME RoadGraphDijkstraTest COMMAND test_road_graph "${CMAKE_SOURCE_DIR}/data/osm/processed/hcmc_roads.json")
add_test(NAME ApiRoutingTest COMMAND test_api_routing "${CMAKE_SOURCE_DIR}/data/osm/processed/hcmc_roads.json")
add_test(NAME AuthSecurityTest COMMAND test_auth)
add_test(NAME TripServiceTest COMMAND test_trip)
add_test(NAME TripApiTest COMMAND test_trip_api)

message(STATUS "RIDE-RTREE v${PROJECT_VERSION} - Phase C.3 (Trip REST API & RBAC Foundation) Configured")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
