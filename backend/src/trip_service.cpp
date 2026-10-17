#include "trip/trip_service.hpp"

#include <chrono>
#include <sstream>
#include <iostream>

namespace goride {
namespace trip {

std::string trip_status_to_string(TripStatus status) {
    switch (status) {
        case TripStatus::REQUESTED:   return "REQUESTED";
        case TripStatus::OFFERED:     return "OFFERED";
        case TripStatus::ACCEPTED:    return "ACCEPTED";
        case TripStatus::ARRIVED:     return "ARRIVED";
        case TripStatus::IN_PROGRESS: return "IN_PROGRESS";
        case TripStatus::COMPLETED:   return "COMPLETED";
        case TripStatus::CANCELLED:   return "CANCELLED";
        case TripStatus::EXPIRED:     return "EXPIRED";
    }
    return "REQUESTED";
}

bool trip_status_from_string(const std::string& str, TripStatus& out_status) {
    if (str == "REQUESTED")   { out_status = TripStatus::REQUESTED;   return true; }
    if (str == "OFFERED")     { out_status = TripStatus::OFFERED;     return true; }
    if (str == "ACCEPTED")    { out_status = TripStatus::ACCEPTED;    return true; }
    if (str == "ARRIVED")     { out_status = TripStatus::ARRIVED;     return true; }
    if (str == "IN_PROGRESS") { out_status = TripStatus::IN_PROGRESS; return true; }
    if (str == "COMPLETED")   { out_status = TripStatus::COMPLETED;   return true; }
    if (str == "CANCELLED")   { out_status = TripStatus::CANCELLED;   return true; }
    if (str == "EXPIRED")     { out_status = TripStatus::EXPIRED;     return true; }
    return false;
}

std::string trip_offer_status_to_string(TripOfferStatus status) {
    switch (status) {
        case TripOfferStatus::PENDING:  return "PENDING";
        case TripOfferStatus::ACCEPTED: return "ACCEPTED";
        case TripOfferStatus::REJECTED: return "REJECTED";
        case TripOfferStatus::EXPIRED:  return "EXPIRED";
    }
    return "PENDING";
}

bool trip_offer_status_from_string(const std::string& str, TripOfferStatus& out_status) {
    if (str == "PENDING")  { out_status = TripOfferStatus::PENDING;  return true; }
    if (str == "ACCEPTED") { out_status = TripOfferStatus::ACCEPTED; return true; }
    if (str == "REJECTED") { out_status = TripOfferStatus::REJECTED; return true; }
    if (str == "EXPIRED")  { out_status = TripOfferStatus::EXPIRED;  return true; }
    return false;
}

std::string Trip::to_json() const {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"id\": \"" << id << "\",\n"
       << "  \"customerId\": \"" << customer_id << "\",\n"
       << "  \"driverId\": \"" << driver_id << "\",\n"
       << "  \"vehicleType\": \"" << vehicle_type << "\",\n"
       << "  \"status\": \"" << trip_status_to_string(status) << "\",\n"
       << "  \"pickup\": {\n"
       << "    \"name\": \"" << pickup_name << "\",\n"
       << "    \"lat\": " << pickup_lat << ",\n"
       << "    \"lon\": " << pickup_lon << "\n"
       << "  },\n"
       << "  \"dropoff\": {\n"
       << "    \"name\": \"" << dropoff_name << "\",\n"
       << "    \"lat\": " << dropoff_lat << ",\n"
       << "    \"lon\": " << dropoff_lon << "\n"
       << "  },\n"
       << "  \"distanceMeters\": " << distance_meters << ",\n"
       << "  \"fareAmount\": " << fare_amount << ",\n"
       << "  \"currentOfferedTo\": \"" << current_offered_to << "\",\n"
       << "  \"offerExpiresAt\": " << offer_expires_at << ",\n"
       << "  \"paymentMethod\": \"" << payment_method << "\",\n"
       << "  \"cancelReason\": \"" << cancel_reason << "\",\n"
       << "  \"cancelledBy\": \"" << cancelled_by << "\",\n"
       << "  \"createdAt\": " << created_at << ",\n"
       << "  \"acceptedAt\": " << accepted_at << ",\n"
       << "  \"arrivedAt\": " << arrived_at << ",\n"
       << "  \"startedAt\": " << started_at << ",\n"
       << "  \"completedAt\": " << completed_at << ",\n"
       << "  \"cancelledAt\": " << cancelled_at << "\n"
       << "}";
    return ss.str();
}

std::string TripOffer::to_json() const {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"id\": " << id << ",\n"
       << "  \"tripId\": \"" << trip_id << "\",\n"
       << "  \"driverId\": \"" << driver_id << "\",\n"
       << "  \"status\": \"" << trip_offer_status_to_string(status) << "\",\n"
       << "  \"offeredAt\": " << offered_at << ",\n"
       << "  \"expiresAt\": " << expires_at << ",\n"
       << "  \"respondedAt\": " << responded_at << "\n"
       << "}";
    return ss.str();
}

// ----------------------------------------------------------------------------
// TripService Implementation
// ----------------------------------------------------------------------------

TripService::TripService(db::Database& db)
    : db_(db) {}

int64_t TripService::current_epoch_seconds() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

bool TripService::create_trip(const Trip& trip, std::string& out_error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open()) {
        out_error = "Database is not open or unavailable";
        return false;
    }

    // Input Validation
    if (trip.id.empty()) {
        out_error = "Trip ID cannot be empty";
        return false;
    }
    if (trip.customer_id.empty()) {
        out_error = "Customer ID cannot be empty";
        return false;
    }
    if (trip.pickup_lat < -90.0 || trip.pickup_lat > 90.0 ||
        trip.pickup_lon < -180.0 || trip.pickup_lon > 180.0 ||
        trip.dropoff_lat < -90.0 || trip.dropoff_lat > 90.0 ||
        trip.dropoff_lon < -180.0 || trip.dropoff_lon > 180.0) {
        out_error = "Coordinates out of bounds: lat [-90, 90], lon [-180, 180]";
        return false;
    }
    if (trip.distance_meters <= 0) {
        out_error = "Distance in meters must be positive";
        return false;
    }
    if (trip.fare_amount < 0) {
        out_error = "Fare amount cannot be negative";
        return false;
    }

    std::string v_type = trip.vehicle_type;
    if (v_type != "motorbike" && v_type != "car4" && v_type != "car4_plus" && v_type != "car7") {
        out_error = "Invalid vehicle_type: must be motorbike, car4, car4_plus, or car7";
        return false;
    }

    // Atomic Active Trip Check under recursive_mutex & Transaction to prevent race conditions
    db::Transaction txn(db_);

    Trip existing_active;
    if (get_active_trip_for_customer(trip.customer_id, existing_active)) {
        out_error = "Customer already has an active trip";
        return false;
    }

    int64_t now = trip.created_at > 0 ? trip.created_at : current_epoch_seconds();

    auto stmt = db_.prepare(
        "INSERT INTO trips (id, customer_id, vehicle_type, status, pickup_name, pickup_lat, pickup_lon, "
        "                   dropoff_name, dropoff_lat, dropoff_lon, distance_meters, fare_amount, "
        "                   payment_method, created_at) "
        "VALUES (?, ?, ?, 'REQUESTED', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
    );
    if (!stmt) {
        out_error = "Failed to prepare insert statement: " + db_.last_error();
        return false;
    }

    stmt->bind(1, trip.id);
    stmt->bind(2, trip.customer_id);
    stmt->bind(3, v_type);
    stmt->bind(4, trip.pickup_name);
    stmt->bind(5, trip.pickup_lat);
    stmt->bind(6, trip.pickup_lon);
    stmt->bind(7, trip.dropoff_name);
    stmt->bind(8, trip.dropoff_lat);
    stmt->bind(9, trip.dropoff_lon);
    stmt->bind(10, trip.distance_meters);
    stmt->bind(11, trip.fare_amount);
    stmt->bind(12, trip.payment_method.empty() ? "cash" : trip.payment_method);
    stmt->bind(13, now);

    if (!stmt->step()) {
        out_error = "Failed to insert trip: " + db_.last_error();
        return false;
    }

    if (!txn.commit()) {
        out_error = "Failed to commit transaction: " + db_.last_error();
        return false;
    }

    return true;
}

bool TripService::get_trip(const std::string& trip_id, Trip& out_trip) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open() || trip_id.empty()) return false;

    auto stmt = db_.prepare(
        "SELECT id, customer_id, driver_id, vehicle_type, status, pickup_name, pickup_lat, pickup_lon, "
        "       dropoff_name, dropoff_lat, dropoff_lon, distance_meters, fare_amount, current_offered_to, "
        "       offer_expires_at, payment_method, cancel_reason, cancelled_by, created_at, accepted_at, "
        "       arrived_at, started_at, completed_at, cancelled_at "
        "FROM trips WHERE id = ? LIMIT 1;"
    );
    if (!stmt) return false;

    stmt->bind(1, trip_id);
    if (!stmt->step()) return false;

    out_trip.id = stmt->column_text(0);
    out_trip.customer_id = stmt->column_text(1);
    out_trip.driver_id = stmt->column_text(2);
    out_trip.vehicle_type = stmt->column_text(3);
    trip_status_from_string(stmt->column_text(4), out_trip.status);
    out_trip.pickup_name = stmt->column_text(5);
    out_trip.pickup_lat = stmt->column_double(6);
    out_trip.pickup_lon = stmt->column_double(7);
    out_trip.dropoff_name = stmt->column_text(8);
    out_trip.dropoff_lat = stmt->column_double(9);
    out_trip.dropoff_lon = stmt->column_double(10);
    out_trip.distance_meters = stmt->column_int(11);
    out_trip.fare_amount = stmt->column_int(12);
    out_trip.current_offered_to = stmt->column_text(13);
    out_trip.offer_expires_at = stmt->column_int64(14);
    out_trip.payment_method = stmt->column_text(15);
    out_trip.cancel_reason = stmt->column_text(16);
    out_trip.cancelled_by = stmt->column_text(17);
    out_trip.created_at = stmt->column_int64(18);
    out_trip.accepted_at = stmt->column_int64(19);
    out_trip.arrived_at = stmt->column_int64(20);
    out_trip.started_at = stmt->column_int64(21);
    out_trip.completed_at = stmt->column_int64(22);
    out_trip.cancelled_at = stmt->column_int64(23);

    return true;
}

bool TripService::get_active_trip_for_customer(const std::string& customer_id, Trip& out_trip) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open() || customer_id.empty()) return false;

    auto stmt = db_.prepare(
        "SELECT id FROM trips "
        "WHERE customer_id = ? AND status NOT IN ('COMPLETED', 'CANCELLED', 'EXPIRED') "
        "ORDER BY created_at DESC LIMIT 1;"
    );
    if (!stmt) return false;

    stmt->bind(1, customer_id);
    if (!stmt->step()) return false;

    std::string tid = stmt->column_text(0);
    return get_trip(tid, out_trip);
}

bool TripService::get_active_trip_for_driver(const std::string& driver_id, Trip& out_trip) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open() || driver_id.empty()) return false;

    int64_t now = current_epoch_seconds();
    auto stmt = db_.prepare(
        "SELECT id FROM trips "
        "WHERE (driver_id = ? AND status IN ('ACCEPTED', 'ARRIVED', 'IN_PROGRESS')) "
        "   OR (current_offered_to = ? AND status = 'OFFERED' AND offer_expires_at >= ?) "
        "ORDER BY created_at DESC LIMIT 1;"
    );
    if (!stmt) return false;

    stmt->bind(1, driver_id);
    stmt->bind(2, driver_id);
    stmt->bind(3, now);
    if (!stmt->step()) return false;

    std::string tid = stmt->column_text(0);
    return get_trip(tid, out_trip);
}

std::vector<Trip> TripService::list_active_trips() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Trip> trips;
    if (!db_.is_open()) return trips;

    auto stmt = db_.prepare(
        "SELECT id FROM trips "
        "WHERE status NOT IN ('COMPLETED', 'CANCELLED', 'EXPIRED') "
        "ORDER BY created_at ASC;"
    );
    if (!stmt) return trips;

    std::vector<std::string> ids;
    while (stmt->step()) {
        ids.push_back(stmt->column_text(0));
    }

    for (const auto& id : ids) {
        Trip t;
        if (get_trip(id, t)) {
            trips.push_back(t);
        }
    }
    return trips;
}

bool TripService::has_driver_been_offered(const std::string& trip_id, const std::string& driver_id) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open()) return false;

    auto stmt = db_.prepare(
        "SELECT 1 FROM trip_offers WHERE trip_id = ? AND driver_id = ? LIMIT 1;"
    );
    if (!stmt) return false;

    stmt->bind(1, trip_id);
    stmt->bind(2, driver_id);
    return stmt->step();
}

std::vector<TripOffer> TripService::get_offers_for_trip(const std::string& trip_id) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<TripOffer> offers;
    if (!db_.is_open() || trip_id.empty()) return offers;

    auto stmt = db_.prepare(
        "SELECT id, trip_id, driver_id, status, offered_at, expires_at, responded_at "
        "FROM trip_offers WHERE trip_id = ? ORDER BY offered_at ASC;"
    );
    if (!stmt) return offers;

    stmt->bind(1, trip_id);
    while (stmt->step()) {
        TripOffer o;
        o.id = stmt->column_int64(0);
        o.trip_id = stmt->column_text(1);
        o.driver_id = stmt->column_text(2);
        trip_offer_status_from_string(stmt->column_text(3), o.status);
        o.offered_at = stmt->column_int64(4);
        o.expires_at = stmt->column_int64(5);
        o.responded_at = stmt->column_int64(6);
        offers.push_back(o);
    }
    return offers;
}

bool TripService::offer_trip(const std::string& trip_id, const std::string& driver_id,
                             int offer_duration_seconds, std::string& out_error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open()) {
        out_error = "Database is not open";
        return false;
    }
    if (offer_duration_seconds <= 0) offer_duration_seconds = 15;

    // Check if driver has already been offered this trip
    if (has_driver_been_offered(trip_id, driver_id)) {
        out_error = "Driver has already received an offer for this trip";
        return false;
    }

    int64_t now = current_epoch_seconds();
    int64_t expires = now + offer_duration_seconds;

    // Atomic transaction for exclusive offer lock
    db::Transaction txn(db_);

    // 1. Expire any old pending offer for this trip if exists
    auto exp_stmt = db_.prepare(
        "UPDATE trip_offers SET status = 'EXPIRED', responded_at = ? "
        "WHERE trip_id = ? AND status = 'PENDING';"
    );
    if (exp_stmt) {
        exp_stmt->bind(1, now);
        exp_stmt->bind(2, trip_id);
        exp_stmt->step();
    }

    // 2. Conditional update on trips table:
    // Only lock if status is REQUESTED, OR status is OFFERED but previous offer already expired
    auto upd_stmt = db_.prepare(
        "UPDATE trips "
        "SET status = 'OFFERED', current_offered_to = ?, offer_expires_at = ? "
        "WHERE id = ? AND (status = 'REQUESTED' OR (status = 'OFFERED' AND offer_expires_at < ?));"
    );
    if (!upd_stmt) {
        out_error = "Failed to prepare trip offer update: " + db_.last_error();
        return false;
    }

    upd_stmt->bind(1, driver_id);
    upd_stmt->bind(2, expires);
    upd_stmt->bind(3, trip_id);
    upd_stmt->bind(4, now);

    if (!upd_stmt->step() || sqlite3_changes(db_.handle()) == 0) {
        out_error = "Trip is not eligible for new offer (either accepted, cancelled or actively locked to another driver)";
        return false;
    }

    // 3. Record new offer in trip_offers history
    auto ins_stmt = db_.prepare(
        "INSERT INTO trip_offers (trip_id, driver_id, status, offered_at, expires_at) "
        "VALUES (?, ?, 'PENDING', ?, ?);"
    );
    if (!ins_stmt) {
        out_error = "Failed to record trip offer history: " + db_.last_error();
        return false;
    }

    ins_stmt->bind(1, trip_id);
    ins_stmt->bind(2, driver_id);
    ins_stmt->bind(3, now);
    ins_stmt->bind(4, expires);

    if (!ins_stmt->step()) {
        out_error = "Failed to insert trip offer record: " + db_.last_error();
        return false;
    }

    if (!txn.commit()) {
        out_error = "Transaction commit failed while offering trip";
        return false;
    }

    return true;
}

bool TripService::accept_offer(const std::string& trip_id, const std::string& driver_id, std::string& out_error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open()) {
        out_error = "Database is not open";
        return false;
    }

    int64_t now = current_epoch_seconds();
    db::Transaction txn(db_);

    // 1. Atomic conditional update on trips:
    // MUST match trip_id, status == 'OFFERED', current_offered_to == driver_id, AND offer_expires_at >= now
    auto upd_stmt = db_.prepare(
        "UPDATE trips "
        "SET status = 'ACCEPTED', driver_id = ?, current_offered_to = NULL, accepted_at = ? "
        "WHERE id = ? AND status = 'OFFERED' AND current_offered_to = ? AND offer_expires_at >= ?;"
    );
    if (!upd_stmt) {
        out_error = "Failed to prepare accept statement: " + db_.last_error();
        return false;
    }

    upd_stmt->bind(1, driver_id);
    upd_stmt->bind(2, now);
    upd_stmt->bind(3, trip_id);
    upd_stmt->bind(4, driver_id);
    upd_stmt->bind(5, now);

    if (!upd_stmt->step() || sqlite3_changes(db_.handle()) == 0) {
        out_error = "Offer has expired, was cancelled, or was not granted to this driver";
        return false;
    }

    // 2. Update trip_offers status to ACCEPTED
    auto off_stmt = db_.prepare(
        "UPDATE trip_offers "
        "SET status = 'ACCEPTED', responded_at = ? "
        "WHERE trip_id = ? AND driver_id = ? AND status = 'PENDING';"
    );
    if (off_stmt) {
        off_stmt->bind(1, now);
        off_stmt->bind(2, trip_id);
        off_stmt->bind(3, driver_id);
        off_stmt->step();
    }

    // 3. Update driver online status to BUSY
    auto drv_stmt = db_.prepare(
        "UPDATE drivers SET online_status = 'BUSY' WHERE user_id = ?;"
    );
    if (drv_stmt) {
        drv_stmt->bind(1, driver_id);
        drv_stmt->step();
    }

    if (!txn.commit()) {
        out_error = "Transaction commit failed while accepting offer";
        return false;
    }

    return true;
}

bool TripService::reject_offer(const std::string& trip_id, const std::string& driver_id, std::string& out_error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open()) {
        out_error = "Database is not open";
        return false;
    }

    int64_t now = current_epoch_seconds();
    db::Transaction txn(db_);

    // 1. Revert trip status back to REQUESTED and clear current_offered_to
    auto upd_stmt = db_.prepare(
        "UPDATE trips "
        "SET status = 'REQUESTED', current_offered_to = NULL, offer_expires_at = 0 "
        "WHERE id = ? AND status = 'OFFERED' AND current_offered_to = ?;"
    );
    if (!upd_stmt) {
        out_error = "Failed to prepare reject statement: " + db_.last_error();
        return false;
    }

    upd_stmt->bind(1, trip_id);
    upd_stmt->bind(2, driver_id);

    if (!upd_stmt->step() || sqlite3_changes(db_.handle()) == 0) {
        out_error = "No active offer found for this driver on this trip";
        return false;
    }

    // 2. Mark offer as REJECTED in trip_offers
    auto off_stmt = db_.prepare(
        "UPDATE trip_offers "
        "SET status = 'REJECTED', responded_at = ? "
        "WHERE trip_id = ? AND driver_id = ? AND status = 'PENDING';"
    );
    if (off_stmt) {
        off_stmt->bind(1, now);
        off_stmt->bind(2, trip_id);
        off_stmt->bind(3, driver_id);
        off_stmt->step();
    }

    if (!txn.commit()) {
        out_error = "Transaction commit failed while rejecting offer";
        return false;
    }

    return true;
}

bool TripService::check_and_expire_offer(const std::string& trip_id, std::string& out_error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open()) return false;

    int64_t now = current_epoch_seconds();
    db::Transaction txn(db_);

    auto upd_stmt = db_.prepare(
        "UPDATE trips "
        "SET status = 'REQUESTED', current_offered_to = NULL, offer_expires_at = 0 "
        "WHERE id = ? AND status = 'OFFERED' AND offer_expires_at < ?;"
    );
    if (!upd_stmt) return false;

    upd_stmt->bind(1, trip_id);
    upd_stmt->bind(2, now);

    if (!upd_stmt->step() || sqlite3_changes(db_.handle()) == 0) {
        return false; // Not expired or not in offered state
    }

    auto off_stmt = db_.prepare(
        "UPDATE trip_offers "
        "SET status = 'EXPIRED', responded_at = ? "
        "WHERE trip_id = ? AND status = 'PENDING';"
    );
    if (off_stmt) {
        off_stmt->bind(1, now);
        off_stmt->bind(2, trip_id);
        off_stmt->step();
    }

    return txn.commit();
}

bool TripService::driver_arrive(const std::string& trip_id, const std::string& driver_id, std::string& out_error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open()) {
        out_error = "Database is not open";
        return false;
    }

    int64_t now = current_epoch_seconds();
    auto stmt = db_.prepare(
        "UPDATE trips "
        "SET status = 'ARRIVED', arrived_at = ? "
        "WHERE id = ? AND driver_id = ? AND status = 'ACCEPTED';"
    );
    if (!stmt) {
        out_error = "Failed to prepare arrive update: " + db_.last_error();
        return false;
    }

    stmt->bind(1, now);
    stmt->bind(2, trip_id);
    stmt->bind(3, driver_id);

    if (!stmt->step() || sqlite3_changes(db_.handle()) == 0) {
        out_error = "Cannot transition to ARRIVED: trip is not in ACCEPTED state or assigned to another driver";
        return false;
    }

    return true;
}

bool TripService::start_trip(const std::string& trip_id, const std::string& driver_id, std::string& out_error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open()) {
        out_error = "Database is not open";
        return false;
    }

    int64_t now = current_epoch_seconds();
    auto stmt = db_.prepare(
        "UPDATE trips "
        "SET status = 'IN_PROGRESS', started_at = ? "
        "WHERE id = ? AND driver_id = ? AND status = 'ARRIVED';"
    );
    if (!stmt) {
        out_error = "Failed to prepare start trip statement: " + db_.last_error();
        return false;
    }

    stmt->bind(1, now);
    stmt->bind(2, trip_id);
    stmt->bind(3, driver_id);

    if (!stmt->step() || sqlite3_changes(db_.handle()) == 0) {
        out_error = "Cannot transition to IN_PROGRESS: driver has not arrived yet or trip is assigned to someone else";
        return false;
    }

    return true;
}

bool TripService::complete_trip(const std::string& trip_id, const std::string& driver_id, std::string& out_error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open()) {
        out_error = "Database is not open";
        return false;
    }

    int64_t now = current_epoch_seconds();
    db::Transaction txn(db_);

    // ONLY transition from IN_PROGRESS -> COMPLETED! (Never before start)
    auto stmt = db_.prepare(
        "UPDATE trips "
        "SET status = 'COMPLETED', completed_at = ? "
        "WHERE id = ? AND driver_id = ? AND status = 'IN_PROGRESS';"
    );
    if (!stmt) {
        out_error = "Failed to prepare complete trip statement: " + db_.last_error();
        return false;
    }

    stmt->bind(1, now);
    stmt->bind(2, trip_id);
    stmt->bind(3, driver_id);

    if (!stmt->step() || sqlite3_changes(db_.handle()) == 0) {
        out_error = "Cannot complete trip: trip is not currently IN_PROGRESS or driver does not match";
        return false;
    }

    // Set driver status back to ONLINE
    auto drv_stmt = db_.prepare(
        "UPDATE drivers SET online_status = 'ONLINE' WHERE user_id = ?;"
    );
    if (drv_stmt) {
        drv_stmt->bind(1, driver_id);
        drv_stmt->step();
    }

    if (!txn.commit()) {
        out_error = "Transaction commit failed while completing trip";
        return false;
    }

    return true;
}

bool TripService::cancel_trip(const std::string& trip_id, const std::string& user_id,
                              const std::string& role, const std::string& reason, std::string& out_error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_.is_open()) {
        out_error = "Database is not open";
        return false;
    }

    Trip trip;
    if (!get_trip(trip_id, trip)) {
        out_error = "Trip not found";
        return false;
    }

    if (trip.status == TripStatus::COMPLETED || trip.status == TripStatus::CANCELLED || trip.status == TripStatus::EXPIRED) {
        out_error = "Trip has already terminated and cannot be cancelled";
        return false;
    }

    // Role-based cancellation permissions
    if (trip.status == TripStatus::IN_PROGRESS && role != "admin" && role != "driver") {
        out_error = "Customers cannot cancel a trip that is already IN_PROGRESS";
        return false;
    }

    int64_t now = current_epoch_seconds();
    db::Transaction txn(db_);

    // 1. Expire any active offer
    auto off_stmt = db_.prepare(
        "UPDATE trip_offers SET status = 'EXPIRED', responded_at = ? "
        "WHERE trip_id = ? AND status = 'PENDING';"
    );
    if (off_stmt) {
        off_stmt->bind(1, now);
        off_stmt->bind(2, trip_id);
        off_stmt->step();
    }

    // 2. Free assigned driver if any
    if (!trip.driver_id.empty()) {
        auto drv_stmt = db_.prepare(
            "UPDATE drivers SET online_status = 'ONLINE' WHERE user_id = ?;"
        );
        if (drv_stmt) {
            drv_stmt->bind(1, trip.driver_id);
            drv_stmt->step();
        }
    }

    // 3. Mark trip CANCELLED
    auto upd_stmt = db_.prepare(
        "UPDATE trips "
        "SET status = 'CANCELLED', cancelled_by = ?, cancel_reason = ?, cancelled_at = ?, current_offered_to = NULL "
        "WHERE id = ? AND status NOT IN ('COMPLETED', 'CANCELLED', 'EXPIRED');"
    );
    if (!upd_stmt) {
        out_error = "Failed to prepare cancel statement: " + db_.last_error();
        return false;
    }

    std::string cancel_actor = role;
    if (cancel_actor != "customer" && cancel_actor != "driver" && cancel_actor != "admin") {
        cancel_actor = "system";
    }

    upd_stmt->bind(1, cancel_actor);
    upd_stmt->bind(2, reason.empty() ? "Cancelled by user" : reason);
    upd_stmt->bind(3, now);
    upd_stmt->bind(4, trip_id);

    if (!upd_stmt->step() || sqlite3_changes(db_.handle()) == 0) {
        out_error = "Failed to update trip cancellation status";
        return false;
    }

    if (!txn.commit()) {
        out_error = "Transaction commit failed while cancelling trip";
        return false;
    }

    return true;
}

} // namespace trip
} // namespace goride
