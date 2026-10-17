#include <atomic>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "auth/auth_service.hpp"
#include "db/database.hpp"
#include "dsa/dijkstra.hpp"
#include "dsa/road_graph.hpp"
#include "dsa/route_service.hpp"
#include "dsa/spatial_grid.hpp"
#include "server/http_server.hpp"
#include "trip/trip_service.hpp"
#include "utils/json.hpp"

using namespace goride::auth;
using namespace goride::db;
using namespace goride::dsa;
using namespace goride::server;
using namespace goride::trip;
using namespace goride::utils;

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

// Helper to configure all Gate C.3 REST API routes on an in-memory test HttpServer instance
void register_c3_api_routes(HttpServer& server, AuthService& auth_service, TripService& trip_service,
                            const RoadGraph* graph = nullptr, const SpatialGrid* grid = nullptr) {
    // RBAC Authenticate Helper Lambda
    auto authenticate_user = [&](const HttpRequest& req, HttpResponse& res,
                                 const std::vector<std::string>& allowed_roles = {}) -> std::optional<User> {
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

    // 1. Root /
    server.get("/", [&](const HttpRequest&, HttpResponse& res) {
        res.set_json("{\"name\":\"GoRide Routing Engine API\",\"version\":\"Phase C.3\",\"status\":\"operational\"}", 200);
    });

    // 2. Health check
    server.get("/api/health", [&](const HttpRequest&, HttpResponse& res) {
        res.set_json("{\"success\":true,\"status\":\"ready\"}", 200);
    });

    // 3. POST /api/trips/request
    auto handle_trip_request = [&](const HttpRequest& req, HttpResponse& res) {
        auto user = authenticate_user(req, res, {"customer", "admin"});
        if (!user) return;

        Trip existing_trip;
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
        auto root = JsonValue::parse(req.body, err);
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

        double haversine_m = goride::dsa::SpatialGrid::haversine_distance_m(p_lat, p_lon, d_lat, d_lon);
        int client_distance_m = static_cast<int>(root.has_key("distanceMeters") ? root["distanceMeters"].as_int64(0) : root["distance_meters"].as_int64(0));
        if (haversine_m > 50.0 && client_distance_m > 0 && client_distance_m < static_cast<int>(haversine_m * 0.7)) {
            res.set_json("{\"success\":false,\"code\":\"INVALID_DISTANCE\",\"error\":\"Khoảng cách lộ trình không thể nhỏ hơn khoảng cách đường chim bay thực tế.\"}", 400);
            return;
        }

        // 1. Server-authoritative route distance calculation via RoadGraph & Dijkstra
        int distance_m = 0;
        if (graph && grid && graph->node_count() > 0) {
            auto route_res = goride::dsa::compute_shortest_route(*graph, *grid, p_lat, p_lon, d_lat, d_lon, 1000.0);
            if (!route_res.success) {
                int status_code = 400;
                if (route_res.status == goride::dsa::RouteComputeStatus::ORIGIN_SNAP_FAILED ||
                    route_res.status == goride::dsa::RouteComputeStatus::DESTINATION_SNAP_FAILED) {
                    status_code = 422;
                } else if (route_res.status == goride::dsa::RouteComputeStatus::ROUTE_NOT_FOUND) {
                    status_code = 404;
                }
                std::ostringstream err_resp;
                err_resp << "{\"success\":false,\"code\":\"" << route_res.code
                         << "\",\"error\":\"" << route_res.error << "\"}";
                res.set_json(err_resp.str(), status_code);
                return;
            }
            distance_m = static_cast<int>(std::round(route_res.distance_m));
            if (distance_m <= 0) {
                distance_m = 10;
            }
        } else {
            // Fallback if OSM road graph dataset is not loaded in current environment
            distance_m = client_distance_m;
            if (distance_m <= 0) {
                distance_m = static_cast<int>(std::round(haversine_m * 1.25));
            }
        }

        int fare = static_cast<int>(root.has_key("fareAmount") ? root["fareAmount"].as_int64(0) : root["fare_amount"].as_int64(0));

        // Note: Demo fare estimation formula. Official pricing policy is currently BLOCKED pending product specification.
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

        if (fare <= 0) {
            fare = expected_fare;
        } else {
            // Guard against extreme client tampering (>50% discount or >100% surge from catalog expected fare)
            double min_allowed = expected_fare * 0.50;
            double max_allowed = expected_fare * 2.00;
            if (fare < min_allowed || fare > max_allowed) {
                res.set_json("{\"success\":false,\"code\":\"INVALID_FARE\",\"error\":\"Giá cước không hợp lệ hoặc có dấu hiệu can thiệp giá.\"}", 400);
                return;
            }
        }

        std::string p_name = root.has_key("pickupName") ? root["pickupName"].as_string() : root["pickup_name"].as_string();
        std::string d_name = root.has_key("dropoffName") ? root["dropoffName"].as_string() : root["dropoff_name"].as_string();
        std::string pay_method = root.has_key("paymentMethod") ? root["paymentMethod"].as_string() : root["payment_method"].as_string("cash");

        static std::atomic<uint64_t> test_cnt{5000};
        Trip trip;
        trip.id = "trp_test_" + std::to_string(test_cnt.fetch_add(1));
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

    // 4. GET /api/trips/active
    auto handle_active_trip = [&](const HttpRequest& req, HttpResponse& res) {
        auto user = authenticate_user(req, res);
        if (!user) return;

        Trip active_trip;
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

    // 5. GET /api/trips/detail
    server.get("/api/trips/detail", [&](const HttpRequest& req, HttpResponse& res) {
        auto user = authenticate_user(req, res);
        if (!user) return;

        std::string trip_id = req.get_param("id", req.get_param("trip_id"));
        if (trip_id.empty()) {
            res.set_json("{\"success\":false,\"code\":\"MISSING_TRIP_ID\",\"error\":\"Vui lòng cung cấp mã chuyến đi (id hoặc trip_id).\"}", 400);
            return;
        }

        Trip trip;
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
    });

    // 6. POST /api/trips/cancel
    server.post("/api/trips/cancel", [&](const HttpRequest& req, HttpResponse& res) {
        auto user = authenticate_user(req, res);
        if (!user) return;

        std::string trip_id, reason;
        std::string err;
        auto root = JsonValue::parse(req.body, err);
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

        Trip trip;
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
    });

    // 7. GET /api/driver/offers/pending
    server.get("/api/driver/offers/pending", [&](const HttpRequest& req, HttpResponse& res) {
        auto user = authenticate_user(req, res, {"driver"});
        if (!user) return;

        Trip offered_trip;
        bool has_offer = false;
        int64_t remaining_s = 0;
        if (trip_service.get_active_trip_for_driver(user->id, offered_trip)) {
            if (offered_trip.status == TripStatus::OFFERED && offered_trip.current_offered_to == user->id) {
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
    });

    // 8. POST /api/driver/offers/accept
    server.post("/api/driver/offers/accept", [&](const HttpRequest& req, HttpResponse& res) {
        auto user = authenticate_user(req, res, {"driver"});
        if (!user) return;

        std::string trip_id;
        std::string err;
        auto root = JsonValue::parse(req.body, err);
        if (err.empty() && root.is_object()) {
            if (root.has_key("trip_id")) trip_id = root["trip_id"].as_string();
            else if (root.has_key("tripId")) trip_id = root["tripId"].as_string();
            else if (root.has_key("id")) trip_id = root["id"].as_string();
        }
        if (trip_id.empty()) trip_id = req.get_param("trip_id", req.get_param("id"));

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
    });

    // 9. POST /api/driver/offers/reject
    server.post("/api/driver/offers/reject", [&](const HttpRequest& req, HttpResponse& res) {
        auto user = authenticate_user(req, res, {"driver"});
        if (!user) return;

        std::string trip_id;
        std::string err;
        auto root = JsonValue::parse(req.body, err);
        if (err.empty() && root.is_object()) {
            if (root.has_key("trip_id")) trip_id = root["trip_id"].as_string();
            else if (root.has_key("tripId")) trip_id = root["tripId"].as_string();
            else if (root.has_key("id")) trip_id = root["id"].as_string();
        }
        if (trip_id.empty()) trip_id = req.get_param("trip_id", req.get_param("id"));

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
    });

    // 10. POST /api/driver/trips/status
    server.post("/api/driver/trips/status", [&](const HttpRequest& req, HttpResponse& res) {
        auto user = authenticate_user(req, res, {"driver"});
        if (!user) return;

        std::string trip_id, action, status_val;
        std::string err;
        auto root = JsonValue::parse(req.body, err);
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
    });

    // 11. GET /api/admin/trips
    auto handle_admin_trips = [&](const HttpRequest& req, HttpResponse& res) {
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

    // 12. POST /api/admin/trips/offer
    server.post("/api/admin/trips/offer", [&](const HttpRequest& req, HttpResponse& res) {
        auto user = authenticate_user(req, res, {"admin"});
        if (!user) return;

        std::string trip_id, driver_id;
        int duration = 15;
        std::string err;
        auto root = JsonValue::parse(req.body, err);
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
    });
}

} // namespace

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "   GoRide Gate C.3: Trip REST API & RBAC Test Suite       " << std::endl;
    std::cout << "==========================================================" << std::endl;

    const std::string test_db = "data/test_c3_isolated_trips.db";
    const std::string test_json = "data/test_c3_isolated_users.json";
    try { std::filesystem::remove(test_db); } catch (...) {}
    try { std::filesystem::remove(test_json); } catch (...) {}

    // Initialize isolated AuthService and database
    AuthService auth(test_db, test_json);
    assert(auth.is_valid());
    auth.database().initialize_schema("database/schema.sql");

    TripService trip_service(auth.database());

    // Build synthetic road graph & spatial grid for routing and distance verification
    RoadGraph synthetic_graph;
    synthetic_graph.add_node(1, 10.7716, 106.7044); // Bitexco
    synthetic_graph.add_node(2, 10.7800, 106.7100); // Intermediate node
    synthetic_graph.add_node(3, 10.7951, 106.7218); // Landmark 81
    synthetic_graph.add_node(4, 10.7700, 106.6900); // Concurrency pickup
    synthetic_graph.add_node(5, 10.7750, 106.6950); // Concurrency dropoff
    synthetic_graph.add_node(6, 10.7700, 106.6900); // General pickup
    synthetic_graph.add_node(7, 10.7800, 106.7000); // General dropoff
    synthetic_graph.add_node(999, 10.8500, 106.8000); // Isolated island node (no edges)

    synthetic_graph.add_edge(1, 2, 2000.0, 1001, "Ton Duc Thang");
    synthetic_graph.add_edge(2, 3, 2350.0, 1002, "Nguyen Huu Canh");
    synthetic_graph.add_edge(1, 3, 4350.0, 1003, "Direct Arterial");
    synthetic_graph.add_edge(4, 5, 800.0, 1004, "Le Loi");
    synthetic_graph.add_edge(6, 7, 2000.0, 1005, "District 1 Link");

    SpatialGrid test_grid(0.005);
    test_grid.build_index(synthetic_graph);

    HttpServer server;
    register_c3_api_routes(server, auth, trip_service, &synthetic_graph, &test_grid);

    // Register test accounts and obtain valid tokens
    auto cust_reg = auth.register_user("Nguyễn Khách C3", "cust.c3@goride.vn", "0901234567", "pass123456", "customer");
    assert(cust_reg.success);
    auto cust_login = auth.login("cust.c3@goride.vn", "pass123456");
    assert(cust_login.success);
    std::string cust_token = cust_login.token;
    std::string cust_id = cust_login.user.id;

    auto drv1_reg = auth.register_user("Trần Tài Xế 1", "drv1.c3@goride.vn", "0902345678", "pass123456", "driver", "59A-111.11", "car4", 3);
    assert(drv1_reg.success);
    auth.approve_driver(drv1_reg.user.id);
    auto drv1_login = auth.login("drv1.c3@goride.vn", "pass123456");
    assert(drv1_login.success);
    std::string drv1_token = drv1_login.token;
    std::string drv1_id = drv1_login.user.id;

    auto drv2_reg = auth.register_user("Lê Tài Xế 2", "drv2.c3@goride.vn", "0903456789", "pass123456", "driver", "59B-222.22", "car4", 2);
    assert(drv2_reg.success);
    auth.approve_driver(drv2_reg.user.id);
    auto drv2_login = auth.login("drv2.c3@goride.vn", "pass123456");
    assert(drv2_login.success);
    std::string drv2_token = drv2_login.token;
    std::string drv2_id = drv2_login.user.id;

    auto admin_login = auth.login("admin@goride.vn", "Admin@123456");
    assert(admin_login.success);
    std::string admin_token = admin_login.token;

    // Helper to execute mock HTTP request into server
    auto call_api = [&](const std::string& method, const std::string& path,
                        const std::string& token = "", const std::string& body = "",
                        const std::unordered_map<std::string, std::string>& params = {}) -> HttpResponse {
        HttpRequest req;
        req.method = method;
        req.path = path;
        req.body = body;
        req.params = params;
        if (!token.empty()) {
            req.headers["Authorization"] = "Bearer " + token;
        }
        HttpResponse res;
        server.dispatch(req, res);
        return res;
    };

    // -------------------------------------------------------------
    // SUITE 1: Authentication & Authorization (401 / 403)
    // -------------------------------------------------------------
    std::cout << "\n--- SUITE 1: Authentication & Authorization (401 / 403) ---" << std::endl;
    {
        // 1. Missing token
        auto r1 = call_api("POST", "/api/trips/request", "", "{}");
        record_test("Missing token returns HTTP 401 AUTH_TOKEN_MISSING",
                    r1.status_code == 401 && r1.body.find("AUTH_TOKEN_MISSING") != std::string::npos);

        // 2. Invalid token
        auto r2 = call_api("POST", "/api/trips/request", "invalid_token_xyz", "{}");
        record_test("Invalid token returns HTTP 401 TOKEN_INVALID",
                    r2.status_code == 401 && r2.body.find("TOKEN_INVALID") != std::string::npos);

        // 3. Driver role trying to create trip (customer only)
        auto r3 = call_api("POST", "/api/trips/request", drv1_token, "{}");
        record_test("Driver role forbidden from creating customer trip (HTTP 403 FORBIDDEN_ROLE)",
                    r3.status_code == 403 && r3.body.find("FORBIDDEN_ROLE") != std::string::npos);

        // 4. Customer role trying to check driver offers
        auto r4 = call_api("GET", "/api/driver/offers/pending", cust_token);
        record_test("Customer role forbidden from driver offer endpoints (HTTP 403 FORBIDDEN_ROLE)",
                    r4.status_code == 403 && r4.body.find("FORBIDDEN_ROLE") != std::string::npos);

        // 5. Customer role trying to access admin trips list
        auto r5 = call_api("GET", "/api/admin/trips", cust_token);
        record_test("Customer role forbidden from admin trips endpoint (HTTP 403 FORBIDDEN_ROLE)",
                    r5.status_code == 403 && r5.body.find("FORBIDDEN_ROLE") != std::string::npos);
    }

    // -------------------------------------------------------------
    // SUITE 2: Input Validation (400 Bad Request)
    // -------------------------------------------------------------
    std::cout << "\n--- SUITE 2: Input Validation (400 Bad Request) ---" << std::endl;
    {
        // 1. Malformed JSON
        auto r1 = call_api("POST", "/api/trips/request", cust_token, "{ bad_json ");
        record_test("Malformed JSON payload returns HTTP 400 INVALID_JSON",
                    r1.status_code == 400 && r1.body.find("INVALID_JSON") != std::string::npos);

        // 2. Invalid vehicle type
        auto r2 = call_api("POST", "/api/trips/request", cust_token,
            R"({"vehicle_type":"airplane","pickup_lat":10.77,"pickup_lon":106.69,"dropoff_lat":10.78,"dropoff_lon":106.70,"distance_meters":2000,"fare_amount":30000})");
        record_test("Invalid vehicle type returns HTTP 400 INVALID_VEHICLE_TYPE",
                    r2.status_code == 400 && r2.body.find("INVALID_VEHICLE_TYPE") != std::string::npos);

        // 3. Out of bounds coordinates
        auto r3 = call_api("POST", "/api/trips/request", cust_token,
            R"({"vehicle_type":"car4","pickup_lat":95.0,"pickup_lon":106.69,"dropoff_lat":10.78,"dropoff_lon":106.70,"distance_meters":2000,"fare_amount":30000})");
        record_test("Out of bounds coordinates return HTTP 400 INVALID_COORDINATES",
                    r3.status_code == 400 && r3.body.find("INVALID_COORDINATES") != std::string::npos);

        // 4. Invalid distance or fare
        auto r4 = call_api("POST", "/api/trips/request", cust_token,
            R"({"vehicle_type":"car4","pickup_lat":10.77,"pickup_lon":106.69,"dropoff_lat":10.78,"dropoff_lon":106.70,"distance_meters":0,"fare_amount":0})");
        record_test("Non-positive distance/fare returns HTTP 400 INVALID_DISTANCE_OR_FARE",
                    r4.status_code == 400 && r4.body.find("INVALID_DISTANCE_OR_FARE") != std::string::npos);

        // 5. Tampered fare (e.g. 4350m car4 with fare_amount: 1000 VND)
        auto r5 = call_api("POST", "/api/trips/request", cust_token,
            R"({"vehicle_type":"car4","pickup_lat":10.7716,"pickup_lon":106.7044,"dropoff_lat":10.7951,"dropoff_lon":106.7218,"distance_meters":4350,"fare_amount":1000})");
        record_test("Tampered fare returns HTTP 400 INVALID_FARE",
                    r5.status_code == 400 && r5.body.find("INVALID_FARE") != std::string::npos);

        // 6. Physically impossible distance (< 70% of Haversine great-circle distance)
        auto r6 = call_api("POST", "/api/trips/request", cust_token,
            R"({"vehicle_type":"car4","pickup_lat":10.7716,"pickup_lon":106.7044,"dropoff_lat":10.7951,"dropoff_lon":106.7218,"distance_meters":500,"fare_amount":70000})");
        record_test("Physically impossible distance returns HTTP 400 INVALID_DISTANCE",
                    r6.status_code == 400 && r6.body.find("INVALID_DISTANCE") != std::string::npos);
    }

    // -------------------------------------------------------------
    // SUITE 3: Trip Creation & Active Trip Duplicate Prevention (201 / 409)
    // -------------------------------------------------------------
    std::cout << "\n--- SUITE 3: Trip Creation & Active Trip Duplicate Prevention ---" << std::endl;
    std::string created_trip_id;
    {
        // 1. Initial active check when none exists
        auto r1 = call_api("GET", "/api/trips/active", cust_token);
        record_test("GET /api/trips/active returns has_active_trip: false when clean",
                    r1.status_code == 200 && r1.body.find("\"has_active_trip\": false") != std::string::npos);

        // 2. Create valid trip
        std::string req_body = R"({
            "vehicle_type": "car4",
            "pickup_name": "Bitexco Financial Tower",
            "pickup_lat": 10.7716,
            "pickup_lon": 106.7044,
            "dropoff_name": "Landmark 81",
            "dropoff_lat": 10.7951,
            "dropoff_lon": 106.7218,
            "distance_meters": 4350,
            "fare_amount": 65000,
            "payment_method": "cash"
        })";
        auto r2 = call_api("POST", "/api/trips/request", cust_token, req_body);
        record_test("POST /api/trips/request returns HTTP 201 Created",
                    r2.status_code == 201 && r2.body.find("\"success\": true") != std::string::npos);

        // Extract trip id from response
        std::string err;
        auto root = JsonValue::parse(r2.body, err);
        if (root.is_object() && root.has_key("trip") && root["trip"].has_key("id")) {
            created_trip_id = root["trip"]["id"].as_string();
        }
        record_test("Created trip has valid non-empty trip_id", !created_trip_id.empty(), created_trip_id);

        // 3. Duplicate active trip rejection (HTTP 409)
        auto r3 = call_api("POST", "/api/trips/request", cust_token, req_body);
        record_test("Creating second trip while active returns HTTP 409 ACTIVE_TRIP_EXISTS",
                    r3.status_code == 409 && r3.body.find("ACTIVE_TRIP_EXISTS") != std::string::npos);

        // 4. Verify GET /api/trips/active now returns this trip
        auto r4 = call_api("GET", "/api/trips/active", cust_token);
        record_test("GET /api/trips/active returns created trip with status REQUESTED",
                    r4.status_code == 200 && r4.body.find("\"has_active_trip\": true") != std::string::npos &&
                    r4.body.find(created_trip_id) != std::string::npos);

        // 5. Direct TripService atomic check under recursive_mutex & Transaction
        Trip direct_trip;
        direct_trip.id = "trp_direct_atomic_race_test";
        direct_trip.customer_id = cust_id;
        direct_trip.vehicle_type = "car4";
        direct_trip.pickup_name = "A"; direct_trip.pickup_lat = 10.7716; direct_trip.pickup_lon = 106.7044;
        direct_trip.dropoff_name = "B"; direct_trip.dropoff_lat = 10.7951; direct_trip.dropoff_lon = 106.7218;
        direct_trip.distance_meters = 4350; direct_trip.fare_amount = 70000;
        std::string direct_err;
        bool direct_created = trip_service.create_trip(direct_trip, direct_err);
        record_test("Atomic create_trip prevents duplicate active trip in service layer",
                    !direct_created && direct_err == "Customer already has an active trip");
    }

    // -------------------------------------------------------------
    // SUITE 4: Trip Details & Privacy Access Control (404 / 403 / 200)
    // -------------------------------------------------------------
    std::cout << "\n--- SUITE 4: Trip Details & Privacy Access Control ---" << std::endl;
    {
        // 1. Non-existent trip
        auto r1 = call_api("GET", "/api/trips/detail", cust_token, "", {{"id", "trp_non_existent_123"}});
        record_test("Non-existent trip returns HTTP 404 TRIP_NOT_FOUND",
                    r1.status_code == 404 && r1.body.find("TRIP_NOT_FOUND") != std::string::npos);

        // 2. Unassigned driver tries to view details
        auto r2 = call_api("GET", "/api/trips/detail", drv1_token, "", {{"id", created_trip_id}});
        record_test("Unassigned driver cannot view customer trip details (HTTP 403 FORBIDDEN_ACCESS)",
                    r2.status_code == 403 && r2.body.find("FORBIDDEN_ACCESS") != std::string::npos);

        // 3. Customer owner views details
        auto r3 = call_api("GET", "/api/trips/detail", cust_token, "", {{"id", created_trip_id}});
        record_test("Customer owner successfully views trip details (HTTP 200 OK)",
                    r3.status_code == 200 && r3.body.find(created_trip_id) != std::string::npos);

        // 4. Admin views details
        auto r4 = call_api("GET", "/api/trips/detail", admin_token, "", {{"id", created_trip_id}});
        record_test("Admin successfully views trip details (HTTP 200 OK)",
                    r4.status_code == 200 && r4.body.find(created_trip_id) != std::string::npos);
    }

    // -------------------------------------------------------------
    // SUITE 5: Exclusive Driver Offer, Reject & Escalation
    // -------------------------------------------------------------
    std::cout << "\n--- SUITE 5: Exclusive Driver Offer, Reject & Escalation ---" << std::endl;
    {
        // 1. Admin dispatches offer to Driver 1
        std::string offer_body1 = R"({"trip_id":")" + created_trip_id + R"(","driver_id":")" + drv1_id + R"(","duration":15})";
        auto r1 = call_api("POST", "/api/admin/trips/offer", admin_token, offer_body1);
        record_test("Admin dispatches exclusive offer to Driver 1 (HTTP 200 OK)",
                    r1.status_code == 200 && r1.body.find("\"success\": true") != std::string::npos);

        // 2. Driver 1 polls pending offer
        auto r2 = call_api("GET", "/api/driver/offers/pending", drv1_token);
        record_test("Driver 1 GET /api/driver/offers/pending returns has_offer: true",
                    r2.status_code == 200 && r2.body.find("\"has_offer\": true") != std::string::npos &&
                    r2.body.find(created_trip_id) != std::string::npos);

        // 3. Driver 2 polls pending offer (must be false because offer is exclusive to Driver 1)
        auto r3 = call_api("GET", "/api/driver/offers/pending", drv2_token);
        record_test("Driver 2 has_offer: false while trip is locked exclusively to Driver 1",
                    r3.status_code == 200 && r3.body.find("\"has_offer\": false") != std::string::npos);

        // 4. Driver 2 attempts to accept Driver 1's offer (must fail with 409)
        std::string accept_body = R"({"trip_id":")" + created_trip_id + R"("})";
        auto r4 = call_api("POST", "/api/driver/offers/accept", drv2_token, accept_body);
        record_test("Unassigned driver cannot accept another driver's offer (HTTP 409 OFFER_EXPIRED)",
                    r4.status_code == 409 && r4.body.find("OFFER_EXPIRED") != std::string::npos);

        // 5. Driver 1 rejects the offer
        auto r5 = call_api("POST", "/api/driver/offers/reject", drv1_token, accept_body);
        record_test("Driver 1 successfully rejects the offer (HTTP 200 OK)",
                    r5.status_code == 200 && r5.body.find("\"success\": true") != std::string::npos);

        // 6. Anti-spam escalation check: re-offering to Driver 1 must be rejected
        auto r6 = call_api("POST", "/api/admin/trips/offer", admin_token, offer_body1);
        record_test("Escalation rule: Cannot re-offer same trip to rejected Driver 1 (HTTP 409)",
                    r6.status_code == 409 && r6.body.find("OFFER_DISPATCH_FAILED") != std::string::npos);

        // 7. Re-dispatch offer to Driver 2
        std::string offer_body2 = R"({"trip_id":")" + created_trip_id + R"(","driver_id":")" + drv2_id + R"(","duration":15})";
        auto r7 = call_api("POST", "/api/admin/trips/offer", admin_token, offer_body2);
        record_test("Admin dispatches offer to Driver 2 (HTTP 200 OK)",
                    r7.status_code == 200 && r7.body.find("\"success\": true") != std::string::npos);

        // 8. Driver 2 accepts offer
        auto r8 = call_api("POST", "/api/driver/offers/accept", drv2_token, accept_body);
        record_test("Driver 2 successfully accepts offer (HTTP 200 OK, ACCEPTED)",
                    r8.status_code == 200 && r8.body.find("ACCEPTED") != std::string::npos);

        // 9. Repeated accept attempt is rejected (HTTP 409)
        auto r9 = call_api("POST", "/api/driver/offers/accept", drv2_token, accept_body);
        record_test("Repeated offer accept attempt rejected (HTTP 409 OFFER_EXPIRED)",
                    r9.status_code == 409 && r9.body.find("OFFER_EXPIRED") != std::string::npos);
    }

    // -------------------------------------------------------------
    // SUITE 6: Trip Lifecycle State Progression & Transition Guard
    // -------------------------------------------------------------
    std::cout << "\n--- SUITE 6: Trip Lifecycle State Progression & Transition Guard ---" << std::endl;
    {
        std::string status_url = "/api/driver/trips/status";

        // 1. Driver 2 tries to jump straight from ACCEPTED -> IN_PROGRESS (Must fail: hasn't arrived)
        auto r1 = call_api("POST", status_url, drv2_token, R"({"trip_id":")" + created_trip_id + R"(","action":"start"})");
        record_test("Jumping directly to IN_PROGRESS without ARRIVED is blocked (HTTP 409)",
                    r1.status_code == 409 && r1.body.find("INVALID_STATUS_TRANSITION") != std::string::npos);

        // 2. Driver 2 transitions to ARRIVED
        auto r2 = call_api("POST", status_url, drv2_token, R"({"trip_id":")" + created_trip_id + R"(","action":"arrived"})");
        record_test("Transition to ARRIVED succeeds (HTTP 200 OK)",
                    r2.status_code == 200 && r2.body.find("ARRIVED") != std::string::npos);

        // 3. Customer tries to cancel while driver has arrived (allowed before IN_PROGRESS)
        // Wait: let's test transition to IN_PROGRESS first
        auto r3 = call_api("POST", status_url, drv2_token, R"({"trip_id":")" + created_trip_id + R"(","action":"start"})");
        record_test("Transition to IN_PROGRESS succeeds (HTTP 200 OK)",
                    r3.status_code == 200 && r3.body.find("IN_PROGRESS") != std::string::npos);

        // 4. Customer tries to cancel trip that is now IN_PROGRESS (Must be rejected with 403)
        auto r4 = call_api("POST", "/api/trips/cancel", cust_token, R"({"trip_id":")" + created_trip_id + R"(","reason":"Đổi ý"})");
        record_test("Customer cancellation blocked when trip is IN_PROGRESS (HTTP 403 CANNOT_CANCEL_IN_PROGRESS)",
                    r4.status_code == 403 && r4.body.find("CANNOT_CANCEL_IN_PROGRESS") != std::string::npos);

        // 5. Driver 2 transitions to COMPLETED
        auto r5 = call_api("POST", status_url, drv2_token, R"({"trip_id":")" + created_trip_id + R"(","action":"complete"})");
        record_test("Transition to COMPLETED succeeds (HTTP 200 OK)",
                    r5.status_code == 200 && r5.body.find("COMPLETED") != std::string::npos);

        // 6. Attempting to complete already completed trip
        auto r6 = call_api("POST", status_url, drv2_token, R"({"trip_id":")" + created_trip_id + R"(","action":"complete"})");
        record_test("Operating on COMPLETED trip is blocked (HTTP 409)",
                    r6.status_code == 409 && r6.body.find("INVALID_STATUS_TRANSITION") != std::string::npos);

        // 7. Customer active trip is now clean (can create new trip)
        auto r7 = call_api("GET", "/api/trips/active", cust_token);
        record_test("Customer active trip cleared after completion (has_active_trip: false)",
                    r7.status_code == 200 && r7.body.find("\"has_active_trip\": false") != std::string::npos);
    }

    // -------------------------------------------------------------
    // SUITE 7: Admin Trips List & Existing Routes Anti-Regression
    // -------------------------------------------------------------
    std::cout << "\n--- SUITE 7: Admin Trips List & Existing Routes Anti-Regression ---" << std::endl;
    {
        // 1. Admin trips endpoint
        auto r1 = call_api("GET", "/api/admin/trips", admin_token);
        record_test("Admin GET /api/admin/trips returns total count and list (HTTP 200 OK)",
                    r1.status_code == 200 && r1.body.find("\"success\": true") != std::string::npos &&
                    r1.body.find("\"trips\":") != std::string::npos);

        // 2. Root route anti-regression
        auto r2 = call_api("GET", "/");
        record_test("GET / returns GoRide API information and Phase C.3 status (HTTP 200 OK)",
                    r2.status_code == 200 && r2.body.find("Phase C.3") != std::string::npos);

        // 3. Health route anti-regression
        auto r3 = call_api("GET", "/api/health");
        record_test("GET /api/health returns ready (HTTP 200 OK)",
                    r3.status_code == 200 && r3.body.find("\"status\": \"ready\"") != std::string::npos);

        // 4. CORS preflight OPTIONS anti-regression
        auto r4 = call_api("OPTIONS", "/api/trips/request");
        record_test("OPTIONS CORS preflight returns HTTP 204 No Content",
                    r4.status_code == 204);

        // 5. Unknown endpoint returns HTTP 404
        auto r5 = call_api("GET", "/api/unknown/random/endpoint");
        record_test("Unknown route returns HTTP 404",
                    r5.status_code == 404 && r5.body.find("Endpoint not found") != std::string::npos);

        // 6. API v1 alias: /api/v1/trips/active
        auto r6 = call_api("GET", "/api/v1/trips/active", cust_token);
        record_test("Route alias /api/v1/trips/active functions identically (HTTP 200 OK)",
                    r6.status_code == 200 && r6.body.find("\"has_active_trip\"") != std::string::npos);

        // 7. API v1 alias: /api/v1/admin/trips
        auto r7 = call_api("GET", "/api/v1/admin/trips", admin_token);
        record_test("Route alias /api/v1/admin/trips functions identically (HTTP 200 OK)",
                    r7.status_code == 200 && r7.body.find("\"trips\":") != std::string::npos);
    }

    // -------------------------------------------------------------
    // SUITE 8: Multi-Threaded Concurrency & Database Invariant Verification
    // -------------------------------------------------------------
    std::cout << "\n--- SUITE 8: Multi-Threaded Concurrency & Invariant Verification ---" << std::endl;
    {
        // 1 & 2. Concurrent Trip Creation Race from Same Customer
        auto conc_user_reg = auth.register_user("Nguyễn Concurrency C3", "conc.cust@goride.vn", "0909999888", "pass123456", "customer");
        assert(conc_user_reg.success);
        auto conc_login = auth.login("conc.cust@goride.vn", "pass123456");
        assert(conc_login.success);
        std::string conc_token = conc_login.token;
        std::string conc_id = conc_login.user.id;

        std::string conc_req_body = R"({
            "vehicle_type": "motorbike",
            "pickup_name": "Nhà thờ Đức Bà",
            "pickup_lat": 10.7797,
            "pickup_lon": 106.6990,
            "dropoff_name": "Hồ Con Rùa",
            "dropoff_lat": 10.7828,
            "dropoff_lon": 106.6958,
            "distance_meters": 800,
            "fare_amount": 18000,
            "payment_method": "cash"
        })";

        HttpResponse res_thread1, res_thread2;
        std::atomic<bool> start_race{false};

        std::thread t1([&]() {
            while (!start_race.load()) { std::this_thread::yield(); }
            res_thread1 = call_api("POST", "/api/trips/request", conc_token, conc_req_body);
        });

        std::thread t2([&]() {
            while (!start_race.load()) { std::this_thread::yield(); }
            res_thread2 = call_api("POST", "/api/trips/request", conc_token, conc_req_body);
        });

        start_race.store(true);
        t1.join();
        t2.join();

        // Exactly one request MUST succeed with 201 Created; the other MUST fail with 409 Conflict
        bool race_handled = (res_thread1.status_code == 201 && res_thread2.status_code == 409) ||
                            (res_thread1.status_code == 409 && res_thread2.status_code == 201);
        record_test("Concurrent trip creation: exactly one HTTP 201 and one HTTP 409", race_handled);

        // Database Invariant Verification: customer has strictly 1 active trip in SQLite
        auto stmt_count = auth.database().prepare(
            "SELECT COUNT(*) FROM trips WHERE customer_id = ? AND status NOT IN ('COMPLETED', 'CANCELLED', 'EXPIRED');"
        );
        int active_trips_in_db = 0;
        if (stmt_count) {
            stmt_count->bind(1, conc_id);
            if (stmt_count->step()) {
                active_trips_in_db = stmt_count->column_int(0);
            }
        }
        record_test("Database Invariant: Customer has strictly exactly 1 active trip in SQLite",
                    active_trips_in_db == 1);

        // Extract trip ID of created trip for subsequent concurrency tests
        std::string conc_trip_id;
        std::string err_parse;
        auto root1 = JsonValue::parse(res_thread1.body, err_parse);
        if (root1.is_object() && root1.has_key("trip") && root1["trip"].has_key("id")) {
            conc_trip_id = root1["trip"]["id"].as_string();
        } else {
            auto root2 = JsonValue::parse(res_thread2.body, err_parse);
            if (root2.is_object() && root2.has_key("trip") && root2["trip"].has_key("id")) {
                conc_trip_id = root2["trip"]["id"].as_string();
            }
        }
        assert(!conc_trip_id.empty());

        // 3 & 4. Concurrent Offer Acceptance Race between Two Drivers
        std::string offer_body = R"({"trip_id":")" + conc_trip_id + R"(","driver_id":")" + drv1_id + R"(","duration":30})";
        auto r_off = call_api("POST", "/api/admin/trips/offer", admin_token, offer_body);
        assert(r_off.status_code == 200);

        HttpResponse res_acc1, res_acc2;
        std::atomic<bool> start_accept_race{false};
        std::string accept_payload = R"({"trip_id":")" + conc_trip_id + R"("})";

        std::thread t_acc1([&]() {
            while (!start_accept_race.load()) { std::this_thread::yield(); }
            res_acc1 = call_api("POST", "/api/driver/offers/accept", drv1_token, accept_payload);
        });

        std::thread t_acc2([&]() {
            while (!start_accept_race.load()) { std::this_thread::yield(); }
            res_acc2 = call_api("POST", "/api/driver/offers/accept", drv2_token, accept_payload);
        });

        start_accept_race.store(true);
        t_acc1.join();
        t_acc2.join();

        // Only assigned Driver 1 can accept; Driver 2 must receive 409
        bool accept_handled = (res_acc1.status_code == 200 && res_acc2.status_code == 409);
        record_test("Concurrent offer accept: only assigned driver succeeds (HTTP 200), other rejected (HTTP 409)",
                    accept_handled);

        // Database Invariant Verification: driver_id is Driver 1, status is ACCEPTED, exactly 1 ACCEPTED offer
        auto stmt_trip = auth.database().prepare("SELECT driver_id, status FROM trips WHERE id = ?;");
        std::string db_driver_id, db_status;
        if (stmt_trip) {
            stmt_trip->bind(1, conc_trip_id);
            if (stmt_trip->step()) {
                db_driver_id = stmt_trip->column_text(0);
                db_status = stmt_trip->column_text(1);
            }
        }
        auto stmt_offers = auth.database().prepare("SELECT COUNT(*) FROM trip_offers WHERE trip_id = ? AND status = 'ACCEPTED';");
        int accepted_offers_count = 0;
        if (stmt_offers) {
            stmt_offers->bind(1, conc_trip_id);
            if (stmt_offers->step()) {
                accepted_offers_count = stmt_offers->column_int(0);
            }
        }
        record_test("Database Invariant: Trip assigned exclusively to Driver 1 and exactly 1 ACCEPTED offer record",
                    db_driver_id == drv1_id && db_status == "ACCEPTED" && accepted_offers_count == 1);

        // 5. Concurrent Conflicting Status Transitions (e.g. ARRIVED vs complete)
        HttpResponse res_trans1, res_trans2;
        std::atomic<bool> start_trans_race{false};

        std::thread t_trans1([&]() {
            while (!start_trans_race.load()) { std::this_thread::yield(); }
            res_trans1 = call_api("POST", "/api/driver/trips/status", drv1_token,
                                  R"({"trip_id":")" + conc_trip_id + R"(","action":"arrived"})");
        });

        std::thread t_trans2([&]() {
            while (!start_trans_race.load()) { std::this_thread::yield(); }
            res_trans2 = call_api("POST", "/api/driver/trips/status", drv1_token,
                                  R"({"trip_id":")" + conc_trip_id + R"(","action":"complete"})");
        });

        start_trans_race.store(true);
        t_trans1.join();
        t_trans2.join();

        bool transition_valid = (res_trans1.status_code == 200 && res_trans2.status_code == 409);
        record_test("Concurrent conflicting transitions: valid progression succeeds, illegal jump blocked",
                    transition_valid);

        // 6. Database Invariant: Status transitioned to ARRIVED and no deadlock/corrupt state
        Trip trip_check;
        bool get_ok = trip_service.get_trip(conc_trip_id, trip_check);
        record_test("Database Invariant: Final trip status is strictly ARRIVED after conflicting transitions",
                    get_ok && trip_check.status == TripStatus::ARRIVED && trip_check.driver_id == drv1_id);
    }

    // -------------------------------------------------------------
    // SUITE 9: Server-Authoritative Route Distance & Graph Snapping
    // -------------------------------------------------------------
    std::cout << "\n--- SUITE 9: Server-Authoritative Route Distance & Graph Snapping ---" << std::endl;
    {
        // 1. Register a dedicated customer for Suite 9
        auto cust9_reg = auth.register_user("Lê Khách C3 Route", "cust9.c3@goride.vn", "0908888888", "pass123456", "customer");
        assert(cust9_reg.success);
        auto cust9_login = auth.login("cust9.c3@goride.vn", "pass123456");
        assert(cust9_login.success);
        std::string cust9_token = cust9_login.token;

        // 2. Server calculates exact distance (4350m) and overrides spoofed distance (client sent 99999m)
        std::string spoofed_body = R"({
            "vehicle_type": "car4",
            "pickup_name": "Bitexco Financial Tower",
            "pickup_lat": 10.7716,
            "pickup_lon": 106.7044,
            "dropoff_name": "Landmark 81",
            "dropoff_lat": 10.7951,
            "dropoff_lon": 106.7218,
            "distance_meters": 99999,
            "fare_amount": 76000,
            "payment_method": "cash"
        })";
        auto r_dist = call_api("POST", "/api/trips/request", cust9_token, spoofed_body);
        bool created_ok = (r_dist.status_code == 201);

        std::string route_trip_id;
        std::string err_parse_s9;
        auto root_r = JsonValue::parse(r_dist.body, err_parse_s9);
        if (root_r.is_object() && root_r.has_key("trip") && root_r["trip"].has_key("id")) {
            route_trip_id = root_r["trip"]["id"].as_string();
        }

        Trip saved_trip;
        bool fetch_ok = trip_service.get_trip(route_trip_id, saved_trip);
        record_test("Server-authoritative distance overrides client-spoofed distance (DB distance is 4350m)",
                    created_ok && fetch_ok && saved_trip.distance_meters == 4350,
                    "Expected: 4350m, Actual in DB: " + std::to_string(saved_trip.distance_meters) + "m");

        // Cancel trip to free customer for subsequent tests
        call_api("POST", "/api/trips/cancel", cust9_token, R"({"trip_id":")" + route_trip_id + R"("})");

        // 3. Origin snap failed (> 1000m from any road edge, e.g. out at sea / 10.0, 105.0)
        std::string out_body = R"({
            "vehicle_type": "car4",
            "pickup_lat": 10.0000,
            "pickup_lon": 105.0000,
            "dropoff_lat": 10.7951,
            "dropoff_lon": 106.7218,
            "distance_meters": 5000,
            "fare_amount": 76000
        })";
        auto r_snap_orig = call_api("POST", "/api/trips/request", cust9_token, out_body);
        record_test("Origin coordinate > 1000m from road network returns HTTP 422 ORIGIN_SNAP_FAILED",
                    r_snap_orig.status_code == 422 && r_snap_orig.body.find("ORIGIN_SNAP_FAILED") != std::string::npos);

        // 4. Destination snap failed (> 1000m from road)
        std::string dest_out_body = R"({
            "vehicle_type": "car4",
            "pickup_lat": 10.7716,
            "pickup_lon": 106.7044,
            "dropoff_lat": 10.0000,
            "dropoff_lon": 105.0000,
            "distance_meters": 5000,
            "fare_amount": 76000
        })";
        auto r_snap_dest = call_api("POST", "/api/trips/request", cust9_token, dest_out_body);
        record_test("Destination coordinate > 1000m from road network returns HTTP 422 DESTINATION_SNAP_FAILED",
                    r_snap_dest.status_code == 422 && r_snap_dest.body.find("DESTINATION_SNAP_FAILED") != std::string::npos);

        // 5. Disconnected road network component (Node 999 has no edges) returns HTTP 404 ROUTE_NOT_FOUND
        std::string disconn_body = R"({
            "vehicle_type": "car4",
            "pickup_lat": 10.7716,
            "pickup_lon": 106.7044,
            "dropoff_lat": 10.8500,
            "dropoff_lon": 106.8000,
            "distance_meters": 5000,
            "fare_amount": 76000
        })";
        auto r_disconn = call_api("POST", "/api/trips/request", cust9_token, disconn_body);
        record_test("Unroutable disconnected destinations return HTTP 404 ROUTE_NOT_FOUND",
                    r_disconn.status_code == 404 && r_disconn.body.find("ROUTE_NOT_FOUND") != std::string::npos);
    }

    // Clean up isolated test database files
    auth.database().close();
    try { std::filesystem::remove(test_db); } catch (...) {}
    try { std::filesystem::remove(test_json); } catch (...) {}

    // -------------------------------------------------------------
    // Test Summary
    // -------------------------------------------------------------
    std::cout << "\n==========================================================" << std::endl;
    std::cout << "TEST SUMMARY: " << passed_tests << "/" << total_tests << " Passed ("
              << (passed_tests == total_tests ? "100% SUCCESS" : "FAILURES DETECTED") << ")" << std::endl;
    std::cout << "==========================================================" << std::endl;

    return (passed_tests == total_tests) ? 0 : 1;
}
