#ifndef GORIDE_AUTH_AUTH_SERVICE_HPP
#define GORIDE_AUTH_AUTH_SERVICE_HPP

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "db/database.hpp"
#include "utils/sha256.hpp"

namespace goride {
namespace auth {

struct User {
    std::string id;
    std::string name;
    std::string email;
    std::string phone;
    std::string role;           // "customer", "driver", "admin"
    std::string status;         // "active", "pending", "suspended"
    std::string password_hash;  // SHA-256(salt + ":" + password)
    std::string salt;
    std::string license_plate;  // For drivers
    std::string vehicle_type;   // For drivers: "motorbike", "car4", "car7"
    int experience{0};          // For drivers
    double wallet_balance{0.0};
    double rating{5.0};
    std::string created_at;

    // Serialize sanitized public user profile to JSON (no password_hash, no salt)
    std::string to_json() const;
};

struct AuthResult {
    bool success{false};
    int status_code{200};
    std::string code;
    std::string error;
    std::string message;
    std::string token;
    User user;

    std::string to_json() const;
};

enum class DriverApprovalResult {
    SUCCESS,
    NOT_FOUND,
    NOT_A_DRIVER,
    ALREADY_APPROVED,
    ALREADY_REJECTED
};

class AuthService {
public:
    explicit AuthService(const std::string& db_or_storage_path = "database/goride.db",
                         const std::string& legacy_json_path = "data/users.json");

    // Authenticate user with email and password
    AuthResult login(const std::string& email, const std::string& password);

    // Register user: driver accounts default to "pending", public admin registration is forbidden
    AuthResult register_user(const std::string& name,
                             const std::string& email,
                             const std::string& phone,
                             const std::string& password,
                             const std::string& role,
                             const std::string& license_plate = "",
                             const std::string& vehicle_type = "",
                             int experience = 0);

    // Validate active session token (e.g. from Authorization: Bearer <token>)
    AuthResult validate_token(const std::string& token) const;

    // Revoke session token
    bool logout(const std::string& token);

    // Admin action: atomic check and update driver approval status (race-condition free)
    DriverApprovalResult process_driver_approval(const std::string& driver_id_or_email, bool approve);

    // Admin action: approve a pending driver (wrapper around process_driver_approval)
    bool approve_driver(const std::string& driver_id_or_email);

    // Admin action: reject/suspend a driver (wrapper around process_driver_approval)
    bool reject_driver(const std::string& driver_id_or_email);

    // List drivers waiting for admin approval
    std::vector<User> get_pending_drivers() const;

    // Helper: count registered users
    size_t user_count() const;

    // Helper: find user by email (internal use)
    bool find_user_by_email(const std::string& email, User& out_user) const;

    // Helper: find user by ID (internal use)
    bool find_user_by_id(const std::string& id, User& out_user) const;

    // Direct access to SQLite database
    db::Database& database() { return db_; }
    const db::Database& database() const { return db_; }

    // Check if underlying database is open and operational
    bool is_valid() const { return db_.is_open(); }

private:
    std::string db_path_;
    std::string legacy_json_path_;
    mutable std::mutex mutex_;
    mutable db::Database db_;

    // Fast memory token cache: token -> user_id
    mutable std::unordered_map<std::string, std::string> active_tokens_;

    void seed_default_accounts();
    void import_legacy_json_if_needed();
    void backup_legacy_json();
    void sync_backup_json();

    static std::string normalize_email(const std::string& email);
    static std::string current_iso_time();
    static int64_t current_epoch_seconds();
};

} // namespace auth
} // namespace goride

#endif // GORIDE_AUTH_AUTH_SERVICE_HPP
