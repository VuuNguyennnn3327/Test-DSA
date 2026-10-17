#ifndef GORIDE_DB_DATABASE_HPP
#define GORIDE_DB_DATABASE_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "db/sqlite3.h"

namespace goride {
namespace db {

class Database;

/**
 * RAII Prepared Statement wrapper for SQLite3
 */
class Statement {
public:
    Statement(sqlite3* db, const std::string& sql);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    bool is_valid() const { return stmt_ != nullptr; }

    // Parameter binding (1-indexed)
    bool bind(int index, int val);
    bool bind(int index, int64_t val);
    bool bind(int index, double val);
    bool bind(int index, const std::string& val);
    bool bind_null(int index);

    // Step execution
    // Returns true if a row is ready (SQLITE_ROW), false if finished (SQLITE_DONE)
    bool step();

    // Reset statement to be re-executed with new bindings
    void reset();
    void clear_bindings();

    // Column extraction (0-indexed)
    int column_int(int col) const;
    int64_t column_int64(int col) const;
    double column_double(int col) const;
    std::string column_text(int col) const;
    bool is_column_null(int col) const;

    const std::string& sql() const { return sql_; }
    std::string last_error() const;

private:
    sqlite3* db_{nullptr};
    sqlite3_stmt* stmt_{nullptr};
    std::string sql_;
};

/**
 * RAII Transaction manager for atomic multi-statement operations
 */
class Transaction {
public:
    explicit Transaction(Database& db);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    bool commit();
    void rollback();
    bool is_active() const noexcept { return active_; }

private:
    Database& db_;
    bool committed_{false};
    bool active_{false};
};

/**
 * Thread-safe SQLite Database Manager
 */
class Database {
public:
    Database();
    explicit Database(const std::string& db_path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Open connection to SQLite database
    bool open(const std::string& db_path);
    void close();

    bool is_open() const;
    const std::string& path() const { return path_; }
    std::string last_error() const;

    // Execute raw SQL script (e.g. schema creation)
    bool execute(const std::string& sql);

    // Prepare a reusable statement
    std::unique_ptr<Statement> prepare(const std::string& sql);

    // Initialize or migrate database schema
    bool initialize_schema(const std::string& schema_file_path = "database/schema.sql");

    // Helper to resolve database path across different working directories
    static std::string resolve_database_path(const std::string& preferred_path = "database/goride.db");

    // Internal raw handle for statements
    sqlite3* handle() { return db_; }
    std::recursive_mutex& mutex() { return mutex_; }

private:
    std::string path_;
    sqlite3* db_{nullptr};
    mutable std::recursive_mutex mutex_;

    void configure_pragmas();
    static bool ensure_parent_directory(const std::string& file_path);
};

} // namespace db
} // namespace goride

#endif // GORIDE_DB_DATABASE_HPP
