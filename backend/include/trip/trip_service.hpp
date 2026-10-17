#ifndef GORIDE_TRIP_TRIP_SERVICE_HPP
#define GORIDE_TRIP_TRIP_SERVICE_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "db/database.hpp"

namespace goride {
namespace trip {

enum class TripStatus {
    REQUESTED,
    OFFERED,
    ACCEPTED,
    ARRIVED,
    IN_PROGRESS,
    COMPLETED,
    CANCELLED,
    EXPIRED
};

enum class TripOfferStatus {
    PENDING,
    ACCEPTED,
    REJECTED,
    EXPIRED
};

std::string trip_status_to_string(TripStatus status);
bool trip_status_from_string(const std::string& str, TripStatus& out_status);

std::string trip_offer_status_to_string(TripOfferStatus status);
bool trip_offer_status_from_string(const std::string& str, TripOfferStatus& out_status);

struct Trip {
    std::string id;
    std::string customer_id;
    std::string driver_id;             // Empty until ACCEPTED
    std::string vehicle_type;          // 'motorbike', 'car4', 'car4_plus', 'car7'
    TripStatus status{TripStatus::REQUESTED};
    std::string pickup_name;
    double pickup_lat{0.0};
    double pickup_lon{0.0};
    std::string dropoff_name;
    double dropoff_lat{0.0};
    double dropoff_lon{0.0};
    int distance_meters{0};
    int fare_amount{0};
    std::string current_offered_to;    // Driver currently offered the trip (in OFFERED state)
    int64_t offer_expires_at{0};       // Expiration timestamp of the current offer
    std::string payment_method{"cash"};
    std::string cancel_reason;
    std::string cancelled_by;          // 'customer', 'driver', 'admin', 'system'
    int64_t created_at{0};
    int64_t accepted_at{0};
    int64_t arrived_at{0};
    int64_t started_at{0};
    int64_t completed_at{0};
    int64_t cancelled_at{0};

    std::string to_json() const;
};

struct TripOffer {
    int64_t id{0};
    std::string trip_id;
    std::string driver_id;
    TripOfferStatus status{TripOfferStatus::PENDING};
    int64_t offered_at{0};
    int64_t expires_at{0};
    int64_t responded_at{0};

    std::string to_json() const;
};

/**
 * TripService: Core Trip Lifecycle & State Machine Management
 * 
 * Thread-safe business logic for ride request creation, exclusive single-driver 15s offer dispatch,
 * atomic offer acceptance/rejection (race-condition free), lifecycle progression,
 * offer audit history and cancellation handling.
 */
class TripService {
public:
    explicit TripService(db::Database& db);

    // Create a new ride request from customer
    bool create_trip(const Trip& trip, std::string& out_error);

    // Retrieve trip by unique ID
    bool get_trip(const std::string& trip_id, Trip& out_trip) const;

    // Retrieve active trip for customer or driver (not completed, cancelled or expired)
    bool get_active_trip_for_customer(const std::string& customer_id, Trip& out_trip) const;
    bool get_active_trip_for_driver(const std::string& driver_id, Trip& out_trip) const;

    // List all active trips
    std::vector<Trip> list_active_trips() const;

    // Exclusive dispatch offer: lock trip exclusively to 1 driver for offer_duration_seconds (default 15s)
    bool offer_trip(const std::string& trip_id, const std::string& driver_id, int offer_duration_seconds, std::string& out_error);

    // Driver offer response actions
    bool accept_offer(const std::string& trip_id, const std::string& driver_id, std::string& out_error);
    bool reject_offer(const std::string& trip_id, const std::string& driver_id, std::string& out_error);

    // Trip lifecycle progression (driver initiated)
    bool driver_arrive(const std::string& trip_id, const std::string& driver_id, std::string& out_error);
    bool start_trip(const std::string& trip_id, const std::string& driver_id, std::string& out_error);
    bool complete_trip(const std::string& trip_id, const std::string& driver_id, std::string& out_error);

    // Cancellation (by customer, driver, or admin)
    bool cancel_trip(const std::string& trip_id, const std::string& user_id, const std::string& role,
                     const std::string& reason, std::string& out_error);

    // Offer expiration check (e.g. called on polling or background sweep)
    bool check_and_expire_offer(const std::string& trip_id, std::string& out_error);

    // Check if driver has already been offered this trip
    bool has_driver_been_offered(const std::string& trip_id, const std::string& driver_id) const;

    // Retrieve all offers history for a trip
    std::vector<TripOffer> get_offers_for_trip(const std::string& trip_id) const;

    // Database reference
    db::Database& database() { return db_; }
    const db::Database& database() const { return db_; }

private:
    db::Database& db_;
    mutable std::recursive_mutex mutex_;

    static int64_t current_epoch_seconds();
};

} // namespace trip
} // namespace goride

#endif // GORIDE_TRIP_TRIP_SERVICE_HPP
