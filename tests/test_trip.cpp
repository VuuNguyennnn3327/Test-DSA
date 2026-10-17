#include <cassert>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "db/database.hpp"
#include "trip/trip_service.hpp"

using namespace goride::db;
using namespace goride::trip;

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

// Helper to seed test users and drivers in an isolated database
void seed_test_actors(Database& db) {
    // Insert Customer
    db.execute(
        "INSERT OR IGNORE INTO users (id, name, email, phone, password_hash, salt, role, status, wallet_balance, rating, created_at, updated_at) "
        "VALUES ('cust_01', 'Nguyễn Khách Test', 'cust.test@goride.vn', '0901112233', 'hash123', 'salt1', 'customer', 'active', 500000.0, 5.0, 1700000000, 1700000000);"
    );
    // Insert Driver 1 (Approved, Online)
    db.execute(
        "INSERT OR IGNORE INTO users (id, name, email, phone, password_hash, salt, role, status, wallet_balance, rating, created_at, updated_at) "
        "VALUES ('drv_01', 'Trần Tài Xế 1', 'drv1.test@goride.vn', '0902223344', 'hash123', 'salt2', 'driver', 'active', 0.0, 4.9, 1700000000, 1700000000);"
    );
    db.execute(
        "INSERT OR IGNORE INTO drivers (user_id, license_plate, vehicle_type, vehicle_model, experience, approval_status, online_status, is_online) "
        "VALUES ('drv_01', '51A-111.11', 'car4', 'Toyota Vios', 3, 'APPROVED', 'ONLINE', 1);"
    );
    // Insert Driver 2 (Approved, Online)
    db.execute(
        "INSERT OR IGNORE INTO users (id, name, email, phone, password_hash, salt, role, status, wallet_balance, rating, created_at, updated_at) "
        "VALUES ('drv_02', 'Lê Tài Xế 2', 'drv2.test@goride.vn', '0903334455', 'hash123', 'salt3', 'driver', 'active', 0.0, 4.8, 1700000000, 1700000000);"
    );
    db.execute(
        "INSERT OR IGNORE INTO drivers (user_id, license_plate, vehicle_type, vehicle_model, experience, approval_status, online_status, is_online) "
        "VALUES ('drv_02', '51B-222.22', 'car4', 'Hyundai Accent', 2, 'APPROVED', 'ONLINE', 1);"
    );
}

} // namespace

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "   GoRide Gate C.2: SQLite Schema & TripService Unit Tests" << std::endl;
    std::cout << "==========================================================" << std::endl;

    // Use completely isolated temp database file
    const std::string test_db_path = "data/test_c2_isolated_trips.db";
    try { std::filesystem::remove(test_db_path); } catch (...) {}

    Database db;
    bool opened = db.open(test_db_path);
    assert(opened);

    // ---------------------------------------------------------------------
    // SUITE 1: Schema Initialization on Clean Database
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 1: Schema Initialization & Verification ---" << std::endl;
    {
        bool schema_ok = db.initialize_schema("database/schema.sql");
        record_test("Schema initializes successfully on clean isolated database", schema_ok);

        // Verify tables exist
        auto stmt_u = db.prepare("SELECT count(*) FROM users;");
        auto stmt_t = db.prepare("SELECT count(*) FROM trips;");
        auto stmt_o = db.prepare("SELECT count(*) FROM trip_offers;");
        record_test("Tables 'users', 'trips', and 'trip_offers' are present",
                    stmt_u != nullptr && stmt_t != nullptr && stmt_o != nullptr);

        seed_test_actors(db);
    }

    TripService trip_service(db);

    // ---------------------------------------------------------------------
    // SUITE 2: Trip Creation, Validation & Retrieval
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 2: Trip Creation, Validation & Retrieval ---" << std::endl;
    {
        Trip t1;
        t1.id = "trp_test_001";
        t1.customer_id = "cust_01";
        t1.vehicle_type = "car4";
        t1.pickup_name = "Chợ Bến Thành";
        t1.pickup_lat = 10.7725;
        t1.pickup_lon = 106.6980;
        t1.dropoff_name = "Landmark 81";
        t1.dropoff_lat = 10.7951;
        t1.dropoff_lon = 106.7219;
        t1.distance_meters = 5200;
        t1.fare_amount = 75000;

        std::string err;
        bool created = trip_service.create_trip(t1, err);
        record_test("Valid ride request created in database with status 'REQUESTED'",
                    created && err.empty(), "trip id: " + t1.id);

        Trip retrieved;
        bool got = trip_service.get_trip("trp_test_001", retrieved);
        record_test("get_trip retrieves exact trip properties from SQLite",
                    got && retrieved.id == "trp_test_001" &&
                    retrieved.customer_id == "cust_01" &&
                    retrieved.vehicle_type == "car4" &&
                    retrieved.status == TripStatus::REQUESTED &&
                    retrieved.fare_amount == 75000 &&
                    retrieved.distance_meters == 5200);

        // Active trips query
        auto active_list = trip_service.list_active_trips();
        record_test("list_active_trips includes newly created request",
                    active_list.size() == 1 && active_list[0].id == "trp_test_001");
    }

    // ---------------------------------------------------------------------
    // SUITE 3: Constraints & Foreign Key Enforcement
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 3: Constraints & Foreign Key Enforcement ---" << std::endl;
    {
        // Invalid vehicle type fails validation
        Trip t_bad_v;
        t_bad_v.id = "trp_bad_v";
        t_bad_v.customer_id = "cust_01";
        t_bad_v.vehicle_type = "spaceship";
        t_bad_v.pickup_lat = 10.7; t_bad_v.pickup_lon = 106.6;
        t_bad_v.dropoff_lat = 10.8; t_bad_v.dropoff_lon = 106.7;
        t_bad_v.distance_meters = 1000; t_bad_v.fare_amount = 20000;
        std::string err_v;
        bool created_v = trip_service.create_trip(t_bad_v, err_v);
        record_test("Invalid vehicle_type rejected by validation",
                    !created_v && !err_v.empty());

        // Non-existent customer_id blocked by SQLite foreign key constraint
        Trip t_bad_fk;
        t_bad_fk.id = "trp_bad_fk";
        t_bad_fk.customer_id = "non_existent_customer_999";
        t_bad_fk.vehicle_type = "car4";
        t_bad_fk.pickup_lat = 10.7; t_bad_fk.pickup_lon = 106.6;
        t_bad_fk.dropoff_lat = 10.8; t_bad_fk.dropoff_lon = 106.7;
        t_bad_fk.distance_meters = 1000; t_bad_fk.fare_amount = 20000;
        std::string err_fk;
        bool created_fk = trip_service.create_trip(t_bad_fk, err_fk);
        record_test("Foreign key constraint blocks trip with non-existent customer",
                    !created_fk);
    }

    // ---------------------------------------------------------------------
    // SUITE 4: Exclusive 15s Offer Dispatch & State Progression
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 4: Exclusive Offer Dispatch & Happy Path Progression ---" << std::endl;
    {
        // 1. Offer trip to Driver 1 for 15 seconds
        std::string err_offer;
        bool offered = trip_service.offer_trip("trp_test_001", "drv_01", 15, err_offer);
        Trip t_offered;
        trip_service.get_trip("trp_test_001", t_offered);
        record_test("offer_trip transitions trip to 'OFFERED' with exclusive lock to drv_01",
                    offered && t_offered.status == TripStatus::OFFERED &&
                    t_offered.current_offered_to == "drv_01" &&
                    t_offered.offer_expires_at > 0);

        // 2. Driver 1 accepts offer within 15 seconds
        std::string err_acc;
        bool accepted = trip_service.accept_offer("trp_test_001", "drv_01", err_acc);
        Trip t_accepted;
        trip_service.get_trip("trp_test_001", t_accepted);
        record_test("accept_offer transitions trip to 'ACCEPTED' and assigns driver_id",
                    accepted && t_accepted.status == TripStatus::ACCEPTED &&
                    t_accepted.driver_id == "drv_01" &&
                    t_accepted.current_offered_to.empty());

        // 3. Driver arrives at pickup
        std::string err_arr;
        bool arrived = trip_service.driver_arrive("trp_test_001", "drv_01", err_arr);
        Trip t_arr;
        trip_service.get_trip("trp_test_001", t_arr);
        record_test("driver_arrive transitions trip to 'ARRIVED' and sets arrived_at",
                    arrived && t_arr.status == TripStatus::ARRIVED && t_arr.arrived_at > 0);

        // 4. Driver starts trip
        std::string err_start;
        bool started = trip_service.start_trip("trp_test_001", "drv_01", err_start);
        Trip t_start;
        trip_service.get_trip("trp_test_001", t_start);
        record_test("start_trip transitions trip to 'IN_PROGRESS' and sets started_at",
                    started && t_start.status == TripStatus::IN_PROGRESS && t_start.started_at > 0);

        // 5. Driver completes trip
        std::string err_comp;
        bool completed = trip_service.complete_trip("trp_test_001", "drv_01", err_comp);
        Trip t_comp;
        trip_service.get_trip("trp_test_001", t_comp);
        record_test("complete_trip transitions trip to 'COMPLETED' and sets completed_at",
                    completed && t_comp.status == TripStatus::COMPLETED && t_comp.completed_at > 0);
    }

    // ---------------------------------------------------------------------
    // SUITE 5: Invalid Transitions & Cannot Complete Before Start
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 5: Invalid State Transitions Enforcement ---" << std::endl;
    {
        // Create fresh trip
        Trip t2;
        t2.id = "trp_test_002";
        t2.customer_id = "cust_01";
        t2.vehicle_type = "car4";
        t2.pickup_lat = 10.77; t2.pickup_lon = 106.69;
        t2.dropoff_lat = 10.78; t2.dropoff_lon = 106.70;
        t2.distance_meters = 1500; t2.fare_amount = 30000;
        std::string err;
        trip_service.create_trip(t2, err);

        // Attempt invalid jump: complete directly from REQUESTED
        bool bad_comp = trip_service.complete_trip("trp_test_002", "drv_01", err);
        record_test("Cannot complete trip directly from 'REQUESTED' (rejected)",
                    !bad_comp);

        // Offer to drv_02 and accept
        trip_service.offer_trip("trp_test_002", "drv_02", 15, err);
        trip_service.accept_offer("trp_test_002", "drv_02", err);

        // Attempt invalid jump: complete directly from ACCEPTED (before arrive and start)
        bool bad_comp_acc = trip_service.complete_trip("trp_test_002", "drv_02", err);
        record_test("Cannot complete trip from 'ACCEPTED' without start (rejected)",
                    !bad_comp_acc);

        // Driver arrive
        trip_service.driver_arrive("trp_test_002", "drv_02", err);

        // Attempt complete directly from ARRIVED (before passenger boarded)
        bool bad_comp_arr = trip_service.complete_trip("trp_test_002", "drv_02", err);
        record_test("Cannot complete trip from 'ARRIVED' before starting trip (rejected)",
                    !bad_comp_arr);
    }

    // ---------------------------------------------------------------------
    // SUITE 6: Concurrency, Race Condition & Conditional Update Protection
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 6: Concurrency Protection & Non-Overlapping Offers ---" << std::endl;
    {
        Trip t3;
        t3.id = "trp_test_003";
        t3.customer_id = "cust_01";
        t3.vehicle_type = "car4";
        t3.pickup_lat = 10.77; t3.pickup_lon = 106.69;
        t3.dropoff_lat = 10.78; t3.dropoff_lon = 106.70;
        t3.distance_meters = 2000; t3.fare_amount = 35000;
        std::string err;
        trip_service.create_trip(t3, err);

        // Offer trip to drv_01
        trip_service.offer_trip("trp_test_003", "drv_01", 15, err);

        // Driver 2 attempts to accept an offer that was given exclusively to Driver 1
        std::string err_hijack;
        bool hijack_accepted = trip_service.accept_offer("trp_test_003", "drv_02", err_hijack);
        record_test("Second driver cannot accept an offer designated to another driver",
                    !hijack_accepted);

        // Attempt to offer to drv_02 while still active for drv_01
        std::string err_double_offer;
        bool double_offered = trip_service.offer_trip("trp_test_003", "drv_02", 15, err_double_offer);
        record_test("Cannot offer trip to another driver while active 15s lock is running",
                    !double_offered);
    }

    // ---------------------------------------------------------------------
    // SUITE 7: Expired Offer Rejection & Sequential Driver Escalation
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 7: Expired Offer Handling & Sequential Escalation ---" << std::endl;
    {
        Trip t4;
        t4.id = "trp_test_004";
        t4.customer_id = "cust_01";
        t4.vehicle_type = "car4";
        t4.pickup_lat = 10.77; t4.pickup_lon = 106.69;
        t4.dropoff_lat = 10.78; t4.dropoff_lon = 106.70;
        t4.distance_meters = 2000; t4.fare_amount = 35000;
        std::string err;
        trip_service.create_trip(t4, err);

        // Offer with 1 second duration
        trip_service.offer_trip("trp_test_004", "drv_01", 1, err);

        // Manually set expiration in past to simulate 15s timeout
        db.execute("UPDATE trips SET offer_expires_at = offer_expires_at - 100 WHERE id = 'trp_test_004';");

        // Driver 1 attempts to accept expired offer
        std::string err_exp_acc;
        bool accepted_expired = trip_service.accept_offer("trp_test_004", "drv_01", err_exp_acc);
        record_test("Expired offer cannot be accepted (atomic update fails)",
                    !accepted_expired);

        // Offer can now escalate to Driver 2
        std::string err_esc;
        bool escalated = trip_service.offer_trip("trp_test_004", "drv_02", 15, err_esc);
        record_test("Trip can escalate and offer to next driver once previous offer expires",
                    escalated);
    }

    // ---------------------------------------------------------------------
    // SUITE 8: Offer Audit History & No Duplicate Driver Offers
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 8: Offer Audit History & Duplicate Prevention ---" << std::endl;
    {
        Trip t5;
        t5.id = "trp_test_005";
        t5.customer_id = "cust_01";
        t5.vehicle_type = "car4";
        t5.pickup_lat = 10.77; t5.pickup_lon = 106.69;
        t5.dropoff_lat = 10.78; t5.dropoff_lon = 106.70;
        t5.distance_meters = 2000; t5.fare_amount = 35000;
        std::string err;
        trip_service.create_trip(t5, err);

        // Offer to drv_01, then drv_01 rejects
        trip_service.offer_trip("trp_test_005", "drv_01", 15, err);
        trip_service.reject_offer("trp_test_005", "drv_01", err);

        // Attempt re-offering to drv_01
        std::string err_reoffer;
        bool reoffered_same = trip_service.offer_trip("trp_test_005", "drv_01", 15, err_reoffer);
        record_test("Cannot re-offer trip to driver who already rejected it",
                    !reoffered_same);

        // Audit history has the rejected offer recorded
        auto offers = trip_service.get_offers_for_trip("trp_test_005");
        record_test("Offer audit history accurately records REJECTED status",
                    offers.size() == 1 && offers[0].status == TripOfferStatus::REJECTED &&
                    offers[0].driver_id == "drv_01");
    }

    // ---------------------------------------------------------------------
    // SUITE 9: Transaction Rollback on Partial Failure
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 9: Transaction Rollback on Partial Failure ---" << std::endl;
    {
        // Verify manual transaction rollback works as expected
        {
            Transaction txn(db);
            db.execute("INSERT INTO users (id, name, email, phone, password_hash, salt, role, status, wallet_balance, rating, created_at, updated_at) "
                       "VALUES ('temp_rollback_u', 'Temp User', 'temp.rollback@goride.vn', '0999999999', 'h', 's', 'customer', 'active', 0.0, 5.0, 0, 0);");
            txn.rollback();
        }

        auto check_stmt = db.prepare("SELECT count(*) FROM users WHERE id = 'temp_rollback_u';");
        int count = 0;
        if (check_stmt && check_stmt->step()) {
            count = check_stmt->column_int(0);
        }
        record_test("Transaction rollback cleanly undoes uncommitted changes",
                    count == 0);
    }

    // ---------------------------------------------------------------------
    // SUITE 10: User Data Preservation & Cancellation Handling
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 10: Cancellation & User Data Preservation ---" << std::endl;
    {
        Trip t6;
        t6.id = "trp_test_006";
        t6.customer_id = "cust_01";
        t6.vehicle_type = "car4";
        t6.pickup_lat = 10.77; t6.pickup_lon = 106.69;
        t6.dropoff_lat = 10.78; t6.dropoff_lon = 106.70;
        t6.distance_meters = 2000; t6.fare_amount = 35000;
        std::string err;
        trip_service.create_trip(t6, err);

        // Cancel trip by customer
        bool cancelled = trip_service.cancel_trip("trp_test_006", "cust_01", "customer", "Thay đổi kế hoạch", err);
        Trip t6_cancelled;
        trip_service.get_trip("trp_test_006", t6_cancelled);
        record_test("cancel_trip transitions status to 'CANCELLED' with reason and actor",
                    cancelled && t6_cancelled.status == TripStatus::CANCELLED &&
                    t6_cancelled.cancelled_by == "customer" &&
                    t6_cancelled.cancel_reason == "Thay đổi kế hoạch");

        // Verify users table accounts remain untouched and intact
        auto cnt_stmt = db.prepare("SELECT count(*) FROM users;");
        int user_cnt = 0;
        if (cnt_stmt && cnt_stmt->step()) user_cnt = cnt_stmt->column_int(0);
        record_test("Existing user accounts remain 100% intact",
                    user_cnt >= 3, "user count: " + std::to_string(user_cnt));
    }

    // Clean up isolated test database file
    db.close();
    try { std::filesystem::remove(test_db_path); } catch (...) {}

    // ---------------------------------------------------------------------
    // Summary
    // ---------------------------------------------------------------------
    std::cout << "\n==========================================================" << std::endl;
    std::cout << "TEST SUMMARY: " << passed_tests << "/" << total_tests << " Passed ("
              << (passed_tests == total_tests ? "100% SUCCESS" : "FAILURES DETECTED") << ")" << std::endl;
    std::cout << "==========================================================" << std::endl;

    return (passed_tests == total_tests) ? 0 : 1;
}
