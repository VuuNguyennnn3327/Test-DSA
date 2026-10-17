#include "db/database.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace goride {
namespace db {

namespace {

const char* DEFAULT_EMBEDDED_SCHEMA = R"(
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS users (
    id              TEXT PRIMARY KEY,
    name            TEXT NOT NULL,
    email           TEXT NOT NULL UNIQUE COLLATE NOCASE,
    phone           TEXT NOT NULL DEFAULT '',
    password_hash   TEXT NOT NULL,
    salt            TEXT NOT NULL,
    role            TEXT NOT NULL CHECK (role IN ('customer', 'driver', 'admin')),
    status          TEXT NOT NULL CHECK (status IN ('active', 'pending', 'suspended')),
    wallet_balance  REAL NOT NULL DEFAULT 0.0,
    rating          REAL NOT NULL DEFAULT 5.0,
    avatar_path     TEXT,
    created_at      INTEGER NOT NULL,
    updated_at      INTEGER NOT NULL,
    created_at_iso  TEXT NOT NULL DEFAULT ''
);

CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
CREATE INDEX IF NOT EXISTS idx_users_role_status ON users(role, status);

CREATE TABLE IF NOT EXISTS drivers (
    user_id         TEXT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    license_plate   TEXT NOT NULL,
    vehicle_type    TEXT NOT NULL CHECK (vehicle_type IN ('motorbike', 'car4', 'car7')),
    vehicle_model   TEXT NOT NULL DEFAULT '',
    experience      INTEGER NOT NULL DEFAULT 0,
    approval_status TEXT NOT NULL CHECK (approval_status IN ('PENDING', 'APPROVED', 'REJECTED')) DEFAULT 'PENDING',
    online_status   TEXT NOT NULL CHECK (online_status IN ('ONLINE', 'OFFLINE', 'BUSY')) DEFAULT 'OFFLINE',
    is_online       INTEGER NOT NULL DEFAULT 0,
    last_lat        REAL,
    last_lon        REAL,
    heading         REAL DEFAULT 0.0,
    location_at     INTEGER
);

CREATE INDEX IF NOT EXISTS idx_drivers_status ON drivers(approval_status, online_status);
CREATE INDEX IF NOT EXISTS idx_drivers_online ON drivers(is_online) WHERE is_online = 1;

CREATE TABLE IF NOT EXISTS sessions (
    token           TEXT PRIMARY KEY,
    token_hash      TEXT,
    user_id         TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at      INTEGER NOT NULL,
    expires_at      INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_sessions_expires ON sessions(expires_at);

CREATE TABLE IF NOT EXISTS trips (
    id                  TEXT PRIMARY KEY,
    customer_id         TEXT NOT NULL REFERENCES users(id),
    driver_id           TEXT REFERENCES users(id),
    vehicle_type        TEXT NOT NULL CHECK (vehicle_type IN ('motorbike', 'car4', 'car4_plus', 'car7')),
    status              TEXT NOT NULL CHECK (status IN (
                            'REQUESTED', 'OFFERED', 'ACCEPTED', 
                            'ARRIVED', 'IN_PROGRESS', 'COMPLETED', 
                            'CANCELLED', 'EXPIRED'
                        )) DEFAULT 'REQUESTED',
    pickup_name         TEXT NOT NULL DEFAULT '',
    pickup_lat          REAL NOT NULL,
    pickup_lon          REAL NOT NULL,
    dropoff_name        TEXT NOT NULL DEFAULT '',
    dropoff_lat         REAL NOT NULL,
    dropoff_lon         REAL NOT NULL,
    distance_meters     INTEGER NOT NULL,
    fare_amount         INTEGER NOT NULL,
    current_offered_to  TEXT REFERENCES users(id),
    offer_expires_at    INTEGER DEFAULT 0,
    payment_method      TEXT NOT NULL DEFAULT 'cash',
    cancel_reason       TEXT,
    cancelled_by        TEXT CHECK (cancelled_by IN ('customer', 'driver', 'admin', 'system')),
    created_at          INTEGER NOT NULL,
    accepted_at         INTEGER,
    arrived_at          INTEGER,
    started_at          INTEGER,
    completed_at        INTEGER,
    cancelled_at        INTEGER
);

CREATE INDEX IF NOT EXISTS idx_trips_customer ON trips(customer_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_trips_driver ON trips(driver_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_trips_status ON trips(status);
CREATE INDEX IF NOT EXISTS idx_trips_dispatch ON trips(status, offer_expires_at);

CREATE TABLE IF NOT EXISTS trip_offers (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    trip_id         TEXT NOT NULL REFERENCES trips(id) ON DELETE CASCADE,
    driver_id       TEXT NOT NULL REFERENCES users(id),
    status          TEXT NOT NULL CHECK (status IN ('PENDING', 'ACCEPTED', 'REJECTED', 'EXPIRED')),
    offered_at      INTEGER NOT NULL,
    expires_at      INTEGER NOT NULL,
    responded_at    INTEGER
);

CREATE INDEX IF NOT EXISTS idx_trip_offers_lookup ON trip_offers(trip_id, driver_id);
CREATE INDEX IF NOT EXISTS idx_trip_offers_driver ON trip_offers(driver_id, status);
CREATE UNIQUE INDEX IF NOT EXISTS idx_trip_offers_single_pending ON trip_offers(trip_id) WHERE status = 'PENDING';

PRAGMA user_version = 1;
)";

} // namespace

// ----------------------------------------------------------------------------
// Statement Implementation
// ----------------------------------------------------------------------------

Statement::Statement(sqlite3* db, const std::string& sql)
    : db_(db), sql_(sql) {
    if (!db_) return;
    int rc = sqlite3_prepare_v2(db_, sql_.c_str(), static_cast<int>(sql_.size()), &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database Statement Error] Failed to prepare: " << sql_
                  << " (" << sqlite3_errmsg(db_) << ")" << std::endl;
        stmt_ = nullptr;
    }
}

Statement::~Statement() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
}

Statement::Statement(Statement&& other) noexcept
    : db_(other.db_), stmt_(other.stmt_), sql_(std::move(other.sql_)) {
    other.db_ = nullptr;
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_) sqlite3_finalize(stmt_);
        db_ = other.db_;
        stmt_ = other.stmt_;
        sql_ = std::move(other.sql_);
        other.db_ = nullptr;
        other.stmt_ = nullptr;
    }
    return *this;
}

bool Statement::bind(int index, int val) {
    if (!stmt_) return false;
    return sqlite3_bind_int(stmt_, index, val) == SQLITE_OK;
}

bool Statement::bind(int index, int64_t val) {
    if (!stmt_) return false;
    return sqlite3_bind_int64(stmt_, index, val) == SQLITE_OK;
}

bool Statement::bind(int index, double val) {
    if (!stmt_) return false;
    return sqlite3_bind_double(stmt_, index, val) == SQLITE_OK;
}

bool Statement::bind(int index, const std::string& val) {
    if (!stmt_) return false;
    return sqlite3_bind_text(stmt_, index, val.c_str(), static_cast<int>(val.size()), SQLITE_TRANSIENT) == SQLITE_OK;
}

bool Statement::bind_null(int index) {
    if (!stmt_) return false;
    return sqlite3_bind_null(stmt_, index) == SQLITE_OK;
}

bool Statement::step() {
    if (!stmt_) return false;
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    std::cerr << "[Database Step Error] " << sqlite3_errmsg(db_) << " for: " << sql_ << std::endl;
    return false;
}

void Statement::reset() {
    if (stmt_) sqlite3_reset(stmt_);
}

void Statement::clear_bindings() {
    if (stmt_) sqlite3_clear_bindings(stmt_);
}

int Statement::column_int(int col) const {
    return stmt_ ? sqlite3_column_int(stmt_, col) : 0;
}

int64_t Statement::column_int64(int col) const {
    return stmt_ ? sqlite3_column_int64(stmt_, col) : 0;
}

double Statement::column_double(int col) const {
    return stmt_ ? sqlite3_column_double(stmt_, col) : 0.0;
}

std::string Statement::column_text(int col) const {
    if (!stmt_) return "";
    const unsigned char* text = sqlite3_column_text(stmt_, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

bool Statement::is_column_null(int col) const {
    return stmt_ ? (sqlite3_column_type(stmt_, col) == SQLITE_NULL) : true;
}

std::string Statement::last_error() const {
    return db_ ? sqlite3_errmsg(db_) : "Database handle is null";
}

// ----------------------------------------------------------------------------
// Transaction Implementation
// ----------------------------------------------------------------------------

Transaction::Transaction(Database& db)
    : db_(db) {
    if (db_.execute("BEGIN IMMEDIATE;")) {
        active_ = true;
    }
}

Transaction::~Transaction() {
    if (active_ && !committed_) {
        rollback();
    }
}

bool Transaction::commit() {
    if (!active_ || committed_) return false;
    if (db_.execute("COMMIT;")) {
        committed_ = true;
        active_ = false;
        return true;
    }
    return false;
}

void Transaction::rollback() {
    if (!active_ || committed_) return;
    db_.execute("ROLLBACK;");
    active_ = false;
}

// ----------------------------------------------------------------------------
// Database Implementation
// ----------------------------------------------------------------------------

Database::Database() = default;

Database::Database(const std::string& db_path) {
    open(db_path);
}

Database::~Database() {
    close();
}

bool Database::ensure_parent_directory(const std::string& file_path) {
    if (file_path == ":memory:" || file_path.empty()) return true;
    try {
        std::filesystem::path p(file_path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::string Database::resolve_database_path(const std::string& preferred_path) {
    // 1. In-memory SQLite database
    if (preferred_path == ":memory:") {
        return ":memory:";
    }

    // 2. Explicit non-default path:
    // If caller specifies an explicit path that is NOT the default "database/goride.db"
    // and NOT empty, ALWAYS respect and return it directly.
    // Never fall back to another existing database file when an explicit path was requested,
    // regardless of whether the requested file exists yet on disk or not (Fixes DB-01).
    if (!preferred_path.empty() && preferred_path != "database/goride.db") {
        return preferred_path;
    }

    // 3. Default resolution mode (caller passed "" or "database/goride.db"):
    // First: Check if environment variable GORIDE_DB is configured
    if (const char* env_p = std::getenv("GORIDE_DB")) {
        std::string env_str(env_p);
        if (!env_str.empty()) {
            return env_str;
        }
    }

    // Second: Search default candidates relative to CWD to locate existing workspace database
    const std::vector<std::string> default_candidates = {
        "database/goride.db",
        "../database/goride.db",
        "../../database/goride.db",
        "data/goride.db"
    };

    for (const auto& cand : default_candidates) {
        if (!cand.empty() && std::filesystem::exists(cand)) {
            return cand;
        }
    }

    // Third: Default fallback when no existing database is found
    return "database/goride.db";
}

bool Database::open(const std::string& db_path) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    close();

    path_ = resolve_database_path(db_path);
    if (!ensure_parent_directory(path_)) {
        std::cerr << "[Database Error] Failed to create parent directory for: " << path_ << std::endl;
        return false;
    }

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    int rc = sqlite3_open_v2(path_.c_str(), &db_, flags, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database Error] Failed to open SQLite database: " << path_
                  << " (" << (db_ ? sqlite3_errmsg(db_) : "cannot allocate memory") << ")" << std::endl;
        if (db_) {
            sqlite3_close_v2(db_);
            db_ = nullptr;
        }
        return false;
    }

    configure_pragmas();
    return true;
}

void Database::close() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

bool Database::is_open() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return db_ != nullptr;
}

std::string Database::last_error() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return db_ ? sqlite3_errmsg(db_) : "Database is not open";
}

void Database::configure_pragmas() {
    if (!db_) return;

    // Enable foreign keys
    execute("PRAGMA foreign_keys = ON;");

    // Set busy timeout to 5000ms to gracefully wait on locked DB
    sqlite3_busy_timeout(db_, 5000);

    // Use WAL (Write-Ahead Logging) for non-memory databases
    if (path_ != ":memory:") {
        execute("PRAGMA journal_mode = WAL;");
    }
}

bool Database::execute(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_) return false;

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database Exec Error] " << (err_msg ? err_msg : "Unknown error")
                  << " executing: " << sql.substr(0, 80) << std::endl;
        return false;
    }
    return true;
}

std::unique_ptr<Statement> Database::prepare(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_) return nullptr;
    auto stmt = std::make_unique<Statement>(db_, sql);
    if (!stmt->is_valid()) return nullptr;
    return stmt;
}

bool Database::initialize_schema(const std::string& schema_file_path) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_) return false;

    // Check current user_version
    int version = 0;
    auto stmt = prepare("PRAGMA user_version;");
    if (stmt && stmt->step()) {
        version = stmt->column_int(0);
    }

    if (version > 0) {
        // Schema already applied
        return true;
    }

    // Try reading schema from file
    std::string schema_sql;
    std::vector<std::string> candidates = {
        schema_file_path,
        "database/schema.sql",
        "../database/schema.sql",
        "../../database/schema.sql"
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            std::ifstream fin(path);
            if (fin.is_open()) {
                std::stringstream buffer;
                buffer << fin.rdbuf();
                schema_sql = buffer.str();
                break;
            }
        }
    }

    if (schema_sql.empty()) {
        schema_sql = DEFAULT_EMBEDDED_SCHEMA;
    }

    return execute(schema_sql);
}

} // namespace db
} // namespace goride
