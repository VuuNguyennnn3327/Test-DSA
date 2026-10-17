-- ============================================================================
-- GoRide (RIDE-RTREE) Database Schema
-- Production-grade SQLite schema for Users, Drivers, Trips and Sessions
-- ============================================================================

PRAGMA foreign_keys = ON;

-- 1. Users table (Customer, Driver, Admin)
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

-- 2. Drivers profile table
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

-- 3. Sessions table (Token Authentication)
CREATE TABLE IF NOT EXISTS sessions (
    token           TEXT PRIMARY KEY,
    token_hash      TEXT,
    user_id         TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at      INTEGER NOT NULL,
    expires_at      INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_sessions_expires ON sessions(expires_at);

-- 4. Trips (Rides) table
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

-- 5. Trip Offers table (Audit history & Exclusive Dispatch Lock)
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

-- Database versioning pragma
PRAGMA user_version = 1;

