#include <cassert>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "auth/auth_service.hpp"
#include "utils/sha256.hpp"

using namespace goride::auth;
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

} // namespace

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "   GoRide Phase 2A.4: Authentication & Security Unit Tests" << std::endl;
    std::cout << "==========================================================" << std::endl;

    // ---------------------------------------------------------------------
    // SUITE 1: Cryptographic Primitives & Password Hashing
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 1: Cryptographic Primitives & Password Hashing ---" << std::endl;
    {
        // 1. Standard SHA-256 NIST test vector (empty string)
        std::string empty_hash = sha256("");
        record_test("SHA-256 empty string matches standard",
                    empty_hash == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                    "hash: " + empty_hash.substr(0, 16) + "...");

        // 2. Standard SHA-256 NIST test vector ("abc")
        std::string abc_hash = sha256("abc");
        record_test("SHA-256 'abc' matches NIST standard",
                    abc_hash == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                    "hash: " + abc_hash.substr(0, 16) + "...");

        // 3. Salted password hashing generates 64-char hex
        std::string salt = "test_salt_123456";
        std::string h1 = hash_password("secret123", salt);
        record_test("Salted password hash length is 64 hex chars",
                    h1.size() == 64 && h1 != "secret123",
                    "hash: " + h1.substr(0, 16) + "...");

        // 4. Verification with correct password returns true
        record_test("verify_password succeeds on correct password",
                    verify_password("secret123", salt, h1));

        // 5. Verification with incorrect password returns false
        record_test("verify_password fails on incorrect password",
                    !verify_password("wrong_secret", salt, h1));

        // 6. Distinct salts yield distinct hashes for identical passwords
        std::string h2 = hash_password("secret123", "different_salt_99");
        record_test("Different salts produce completely different hashes",
                    h1 != h2,
                    "h1 != h2");
    }

    // ---------------------------------------------------------------------
    // SUITE 2: AuthService Seed Accounts & Login Authentication
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 2: Seed Accounts & Login Authentication ---" << std::endl;
    // Use in-memory / temporary path to not overwrite production json during tests
    AuthService auth("");
    {
        // 7. Initial seed accounts are pre-populated
        record_test("Seed accounts loaded (count >= 4)",
                    auth.user_count() >= 4,
                    "count: " + std::to_string(auth.user_count()));

        // 8. Successful login for customer
        auto res_cust = auth.login("customer@demo.com", "password123");
        record_test("Customer login succeeds with correct credentials",
                    res_cust.success && res_cust.status_code == 200 &&
                    res_cust.user.role == "customer" && !res_cust.token.empty(),
                    "user: " + res_cust.user.name + ", role: " + res_cust.user.role);

        // 9. Login is case-insensitive for email
        auto res_case = auth.login("  CUSTOMER@DEMO.COM  ", "password123");
        record_test("Email normalization handles case and whitespace",
                    res_case.success && res_case.user.email == "customer@demo.com");

        // 10. Login with invalid password returns 401
        auto res_wrong_pwd = auth.login("customer@demo.com", "incorrect_pwd");
        record_test("Invalid password returns 401 Unauthorized",
                    !res_wrong_pwd.success && res_wrong_pwd.status_code == 401 &&
                    res_wrong_pwd.code == "INVALID_PASSWORD",
                    "code: " + res_wrong_pwd.code);

        // 11. Login with non-existent email returns 404
        auto res_no_user = auth.login("nobody_exists@unknown.com", "password123");
        record_test("Non-existent user returns 404 User Not Found",
                    !res_no_user.success && res_no_user.status_code == 404 &&
                    res_no_user.code == "USER_NOT_FOUND",
                    "code: " + res_no_user.code);

        // 12. Login with empty credentials returns 400
        auto res_empty = auth.login("", "");
        record_test("Empty credentials return 400 Bad Request",
                    !res_empty.success && res_empty.status_code == 400);

        // 13. Admin login succeeds with admin role (including fixed admin / admin@123)
        auto res_admin = auth.login("admin@demo.com", "password123");
        auto res_fixed_admin = auth.login("admin", "admin@123");
        record_test("Admin login succeeds and has 'admin' role",
                    res_admin.success && res_admin.user.role == "admin" &&
                    res_fixed_admin.success && res_fixed_admin.user.role == "admin" &&
                    res_fixed_admin.user.name == "admin",
                    "user: " + res_fixed_admin.user.name);

        // 14. Approved driver login succeeds
        auto res_driver = auth.login("driver@demo.com", "password123");
        record_test("Approved driver login succeeds (status == 'active')",
                    res_driver.success && res_driver.user.role == "driver" &&
                    res_driver.user.status == "active",
                    "plate: " + res_driver.user.license_plate);
    }

    // ---------------------------------------------------------------------
    // SUITE 3: Driver Approval & Pending Status Enforcement
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 3: Driver Status Enforcement & Admin Approval ---" << std::endl;
    {
        // 15. Pending driver login is blocked with 403 Forbidden
        auto res_pend = auth.login("pending.driver@demo.com", "password123");
        record_test("Pending driver login blocked with 403 ACCOUNT_PENDING",
                    !res_pend.success && res_pend.status_code == 403 &&
                    res_pend.code == "ACCOUNT_PENDING",
                    "code: " + res_pend.code);

        // 16. Pending drivers query returns pending driver
        auto pending_list = auth.get_pending_drivers();
        bool found_pending = false;
        for (const auto& u : pending_list) {
            if (u.email == "pending.driver@demo.com") found_pending = true;
        }
        record_test("get_pending_drivers includes pending account",
                    found_pending,
                    "pending count: " + std::to_string(pending_list.size()));

        // 17. Admin approves driver and duplicate approval returns ALREADY_APPROVED
        auto app_res = auth.process_driver_approval("pending.driver@demo.com", true);
        auto dup_app_res = auth.process_driver_approval("pending.driver@demo.com", true);
        record_test("Admin approves driver (and duplicate approval returns ALREADY_APPROVED)",
                    app_res == DriverApprovalResult::SUCCESS &&
                    dup_app_res == DriverApprovalResult::ALREADY_APPROVED);

        // 18. Newly approved driver can now log in successfully
        auto res_now_active = auth.login("pending.driver@demo.com", "password123");
        record_test("Approved driver can now log in successfully (status: 'active')",
                    res_now_active.success && res_now_active.status_code == 200 &&
                    res_now_active.user.status == "active");

        // 19. find_user_by_id retrieves correct user record & customer approval returns NOT_A_DRIVER
        User u_by_id;
        bool found_id = auth.find_user_by_id("drv_d202", u_by_id);
        auto non_drv_res = auth.process_driver_approval("customer@demo.com", true);
        record_test("find_user_by_id retrieves driver by ID & customer approval rejected",
                    found_id && u_by_id.email == "driver@demo.com" && u_by_id.role == "driver" &&
                    non_drv_res == DriverApprovalResult::NOT_A_DRIVER);

        // 20. Approving non-existent driver returns NOT_FOUND
        auto bogus_res = auth.process_driver_approval("non_existent_drv_9999", true);
        record_test("Approving non-existent driver returns NOT_FOUND",
                    bogus_res == DriverApprovalResult::NOT_FOUND);

        // 21. Admin rejects pending driver and duplicate rejection returns ALREADY_REJECTED
        auto reg_rej = auth.register_user("Tài Xế Bị Từ Chối", "reject.driver@demo.com", "0933445566", "password123", "driver", "59B1-111.22", "motorbike", 1);
        assert(reg_rej.success);
        auto rej_res = auth.process_driver_approval("reject.driver@demo.com", false);
        auto dup_rej_res = auth.process_driver_approval("reject.driver@demo.com", false);
        record_test("Admin rejects driver (and duplicate rejection returns ALREADY_REJECTED)",
                    rej_res == DriverApprovalResult::SUCCESS &&
                    dup_rej_res == DriverApprovalResult::ALREADY_REJECTED);

        // 22. Rejected driver is blocked from login with 403 ACCOUNT_SUSPENDED
        auto res_rej_login = auth.login("reject.driver@demo.com", "password123");
        record_test("Rejected driver cannot login (403 ACCOUNT_SUSPENDED)",
                    !res_rej_login.success && res_rej_login.status_code == 403 &&
                    res_rej_login.code == "ACCOUNT_SUSPENDED");

        // 23. Rejected driver excluded from pending list & persistence verified
        auto pending_after_rej = auth.get_pending_drivers();
        bool found_in_pending = false;
        for (const auto& u : pending_after_rej) {
            if (u.email == "reject.driver@demo.com") found_in_pending = true;
        }

        // Isolated persistence check (using dedicated SQLite test database)
        bool persist_ok = false;
        const std::string temp_db = "data/test_isolated_users_temp.db";
        try { std::filesystem::remove(temp_db); } catch (...) {}
        {
            AuthService auth_p(temp_db, "");
            auto reg_p = auth_p.register_user("Driver P", "persist.driver@demo.com", "0911223344", "password123", "driver", "59A-12345", "car4", 3);
            if (reg_p.success) {
                auth_p.process_driver_approval(reg_p.user.id, true);
            }
        }
        {
            AuthService auth_reloaded(temp_db, "");
            User reloaded_u;
            if (auth_reloaded.find_user_by_email("persist.driver@demo.com", reloaded_u)) {
                persist_ok = (reloaded_u.status == "active");
            }
        }
        try { std::filesystem::remove(temp_db); } catch (...) {}

        record_test("Rejected driver excluded from pending list & approval persistence verified",
                    !found_in_pending && persist_ok);
    }

    // ---------------------------------------------------------------------
    // SUITE 4: Registration Workflow & Role Authorization Security
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 4: Registration Workflow & Role Security ---" << std::endl;
    {
        // 24. Public registration with 'admin' role is rejected (403 Forbidden)
        auto res_reg_admin = auth.register_user("Hacker Admin", "hacker@evil.com", "0999999999", "password123", "admin");
        record_test("Registration with role 'admin' is rejected (403 Forbidden)",
                    !res_reg_admin.success && res_reg_admin.status_code == 403 &&
                    res_reg_admin.code == "ADMIN_REGISTRATION_FORBIDDEN",
                    "code: " + res_reg_admin.code);

        // 25. Short password (< 6 chars) is rejected (400 Bad Request)
        auto res_short_pwd = auth.register_user("New User", "user@demo.com", "0900000001", "12345", "customer");
        record_test("Short password (< 6 chars) is rejected with 400",
                    !res_short_pwd.success && res_short_pwd.status_code == 400 &&
                    res_short_pwd.code == "PASSWORD_TOO_SHORT");

        // 26. Duplicate email registration is rejected (409 Conflict)
        auto res_dup = auth.register_user("Duplicate User", "customer@demo.com", "0900000002", "password123", "customer");
        record_test("Duplicate email is rejected with 409 Conflict",
                    !res_dup.success && res_dup.status_code == 409 &&
                    res_dup.code == "EMAIL_ALREADY_EXISTS");

        // 27. Register new customer succeeds (201 Created)
        auto res_new_cust = auth.register_user("Trương Văn Mới", "truong.moi@goride.vn", "0912345678", "securePass2026", "customer");
        record_test("Customer registration succeeds with 201 Created",
                    res_new_cust.success && res_new_cust.status_code == 201 &&
                    res_new_cust.user.role == "customer" && res_new_cust.user.status == "active" &&
                    !res_new_cust.token.empty(),
                    "id: " + res_new_cust.user.id);

        // 28. Log in with newly registered customer credentials
        auto res_login_new = auth.login("truong.moi@goride.vn", "securePass2026");
        record_test("Newly registered customer can log in immediately",
                    res_login_new.success && res_login_new.status_code == 200 &&
                    res_login_new.user.name == "Trương Văn Mới");

        // 29. Register new driver defaults to status 'pending'
        auto res_new_drv = auth.register_user("Ngô Tài Xế Mới", "ngo.driver@goride.vn", "0987654321", "driverPass2026", "driver", "59G2-999.88", "car7", 4);
        record_test("Driver registration creates account with status 'pending'",
                    res_new_drv.success && res_new_drv.status_code == 201 &&
                    res_new_drv.user.role == "driver" && res_new_drv.user.status == "pending",
                    "status: " + res_new_drv.user.status);

        // 30. New driver cannot log in until approved
        auto res_login_new_drv = auth.login("ngo.driver@goride.vn", "driverPass2026");
        record_test("New driver cannot log in before admin approval (403)",
                    !res_login_new_drv.success && res_login_new_drv.status_code == 403);
    }

    // ---------------------------------------------------------------------
    // SUITE 5: Session Token Management & Protected Endpoints
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 5: Session Tokens & Protected Endpoints ---" << std::endl;
    {
        // 31. Validate empty token returns 401 Unauthorized
        auto res_no_tok = auth.validate_token("");
        record_test("Empty token access returns 401 Unauthorized",
                    !res_no_tok.success && res_no_tok.status_code == 401);

        // 32. Validate bogus token returns 401 Unauthorized
        auto res_fake_tok = auth.validate_token("bogus-forged-token-xyz");
        record_test("Bogus/forged token returns 401 Unauthorized",
                    !res_fake_tok.success && res_fake_tok.status_code == 401);

        // 33. Valid session token returns user profile
        auto login_cust = auth.login("customer@demo.com", "password123");
        assert(login_cust.success);
        auto res_valid_tok = auth.validate_token(login_cust.token);
        record_test("Valid session token retrieves user profile successfully",
                    res_valid_tok.success && res_valid_tok.status_code == 200 &&
                    res_valid_tok.user.email == "customer@demo.com",
                    "name: " + res_valid_tok.user.name);

        // 34. Logout invalidates token
        bool logged_out = auth.logout(login_cust.token);
        record_test("Logout invalidates active token",
                    logged_out);

        // 35. Using invalidated token after logout returns 401
        auto res_after_logout = auth.validate_token(login_cust.token);
        record_test("Token cannot be reused after logout (401 Unauthorized)",
                    !res_after_logout.success && res_after_logout.status_code == 401);

        // 36. Serialized JSON user profile omits password hash and salt
        std::string user_json = res_valid_tok.user.to_json();
        record_test("Serialized profile never leaks password_hash or salt",
                    user_json.find("password_hash") == std::string::npos &&
                    user_json.find("salt") == std::string::npos &&
                    user_json.find("password123") == std::string::npos,
                    "sanitized JSON checked");
    }

    // ---------------------------------------------------------------------
    // SUITE 6: SQLite Database Persistence, Constraints & Restart Verification
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 6: SQLite Database Persistence & Constraints ---" << std::endl;
    {
        std::string test_db_path = "data/test_temp_persistence.db";
        // Remove old test db file if exists
        try { std::filesystem::remove(test_db_path); } catch (...) {}

        // 37. Database & Schema initialization in SQLite file
        {
            AuthService db_auth(test_db_path, "");
            record_test("SQLite file database initializes and opens successfully",
                        db_auth.database().is_open() && db_auth.user_count() >= 4,
                        "initial user count: " + std::to_string(db_auth.user_count()));

            // 38. Register customer in SQLite
            auto reg_cust = db_auth.register_user("Lê Minh Khách", "le.khach@goride.vn", "0911223344", "matkhau123", "customer");
            record_test("Customer registers into SQLite table 'users'",
                        reg_cust.success && reg_cust.status_code == 201 && reg_cust.user.role == "customer");

            // 39. Email uniqueness constraint enforced by SQLite
            auto reg_dup = db_auth.register_user("Lê Minh Trùng", "le.khach@goride.vn", "0999888777", "matkhau123", "customer");
            record_test("Duplicate email rejected by SQLite constraint (409)",
                        !reg_dup.success && reg_dup.status_code == 409 && reg_dup.code == "EMAIL_ALREADY_EXISTS");

            // 40. Register driver in SQLite
            auto reg_drv = db_auth.register_user("Nguyễn Văn Tài Xế", "nguyen.driver@goride.vn", "0933445566", "matkhau123", "driver", "51K-123.45", "car4", 3);
            record_test("Driver registers into SQLite 'users' & 'drivers' with status 'pending'",
                        reg_drv.success && reg_drv.status_code == 201 && reg_drv.user.status == "pending");

            // 41. Driver with status 'pending' is blocked from logging in
            auto login_pend = db_auth.login("nguyen.driver@goride.vn", "matkhau123");
            record_test("Pending driver blocked from logging in (403 ACCOUNT_PENDING)",
                        !login_pend.success && login_pend.status_code == 403 && login_pend.code == "ACCOUNT_PENDING");

            // 42. Admin approves driver in SQLite
            bool approved = db_auth.approve_driver("nguyen.driver@goride.vn");
            record_test("Admin approves driver updating status to 'active'",
                        approved);

            // 43. Approved driver can now log in
            auto login_appr = db_auth.login("nguyen.driver@goride.vn", "matkhau123");
            record_test("Approved driver logs in successfully with status 200",
                        login_appr.success && login_appr.status_code == 200 && !login_appr.token.empty());

            // 44. Foreign key constraint prevents orphan driver insert
            auto bad_fk_stmt = db_auth.database().prepare(
                "INSERT INTO drivers (user_id, license_plate, vehicle_type, vehicle_model, experience, approval_status, online_status, is_online) "
                "VALUES ('non_existent_user_id_9999', '59X1-00000', 'motorbike', '', 1, 'PENDING', 'OFFLINE', 0);"
            );
            bool fk_blocked = (bad_fk_stmt != nullptr) ? (!bad_fk_stmt->step()) : true;
            record_test("Foreign key constraint prevents orphan driver records",
                        fk_blocked);
        }

        // 45. RESTART PERSISTENCE: Open completely new AuthService instance on the same SQLite file
        {
            AuthService restarted_auth(test_db_path, "");
            record_test("Restart persistence: New instance opens existing SQLite database",
                        restarted_auth.database().is_open() && restarted_auth.user_count() >= 6,
                        "persisted user count: " + std::to_string(restarted_auth.user_count()));

            // 46. Persisted customer account can still log in
            auto login_restarted = restarted_auth.login("le.khach@goride.vn", "matkhau123");
            record_test("Restart persistence: Customer account logs in with persisted credentials",
                        login_restarted.success && login_restarted.status_code == 200 &&
                        login_restarted.user.name == "Lê Minh Khách");

            // 47. Persisted approved driver status retained after restart
            auto login_drv_restarted = restarted_auth.login("nguyen.driver@goride.vn", "matkhau123");
            record_test("Restart persistence: Driver approved status retained across restarts",
                        login_drv_restarted.success && login_drv_restarted.status_code == 200 &&
                        login_drv_restarted.user.status == "active");

            // 48. Fixed Admin account 'admin' / 'admin@123' retained across restarts
            auto login_admin_restarted = restarted_auth.login("admin", "admin@123");
            record_test("Restart persistence: Default Admin account 'admin' remains active",
                        login_admin_restarted.success && login_admin_restarted.user.role == "admin");
        }

        // Clean up temp test database file
        try { std::filesystem::remove(test_db_path); } catch (...) {}
    }

    // ---------------------------------------------------------------------
    // SUITE 7: Database Path Safety & DB-01 Isolation Verification (Gate C.1)
    // ---------------------------------------------------------------------
    std::cout << "\n--- SUITE 7: Database Path Safety & DB-01 Isolation Verification (Gate C.1) ---" << std::endl;
    {
        // 49. Preferred path does not exist yet: resolve_database_path must respect it and open creates it
        std::string p1 = "data/test_c1_non_existent.db";
        try { std::filesystem::remove(p1); } catch (...) {}
        std::string res1 = goride::db::Database::resolve_database_path(p1);
        goride::db::Database db1;
        bool open1 = db1.open(p1);
        bool file_created = std::filesystem::exists(p1);
        db1.close();
        try { std::filesystem::remove(p1); } catch (...) {}
        record_test("Non-existent preferred path is respected and successfully created (Fixes DB-01)",
                    res1 == p1 && open1 && file_created,
                    "resolved: " + res1);

        // 50. Explicit test path is never hijacked by default database or CWD candidates
        std::string p2 = "data/test_c1_isolated_custom.db";
        try { std::filesystem::remove(p2); } catch (...) {}
        goride::db::Database db2;
        db2.open(p2);
        std::string opened_path2 = db2.path();
        db2.close();
        try { std::filesystem::remove(p2); } catch (...) {}
        record_test("Explicit test database path is never redirected to database/goride.db",
                    opened_path2 == p2,
                    "opened path: " + opened_path2);

        // 51. Explicit custom path resolution is independent of CWD candidates
        std::string p3 = "data/test_c1_cwd_independent.db";
        std::string res3 = goride::db::Database::resolve_database_path(p3);
        record_test("Explicit custom path resolution is completely independent of CWD candidates",
                    res3 == p3);

        // 52. Parent directory creation: ensure_parent_directory creates non-existent parent directory
        std::string nested_dir = "data/test_c1_nested_dir";
        std::string p4 = nested_dir + "/nested_test.db";
        try { std::filesystem::remove_all(nested_dir); } catch (...) {}
        goride::db::Database db4;
        bool open4 = db4.open(p4);
        bool dir_created = std::filesystem::exists(nested_dir);
        db4.close();
        try { std::filesystem::remove_all(nested_dir); } catch (...) {}
        record_test("ensure_parent_directory creates non-existent parent directory automatically",
                    open4 && dir_created);

        // 53. Passing .json path to AuthService rejects using it as SQLite DB and leaves is_valid() == false
        std::string p5 = "data/test_invalid_legacy.json";
        AuthService auth_json(p5, "");
        record_test("Passing .json path to AuthService does NOT open database/goride.db",
                    !auth_json.is_valid() && auth_json.database().path() != "database/goride.db",
                    "is_valid: " + std::string(auth_json.is_valid() ? "true" : "false"));

        // 54. ':memory:' path resolution works and opens in-memory database
        std::string res6 = goride::db::Database::resolve_database_path(":memory:");
        goride::db::Database db6;
        bool open6 = db6.open(":memory:");
        record_test("':memory:' database path resolution is fully supported and operational",
                    res6 == ":memory:" && open6 && db6.is_open());
        db6.close();

        // 55. Failed database open reports error and AuthService returns 503 DATABASE_UNAVAILABLE
        goride::db::Database db7;
        bool open7 = db7.open("invalid_dir<>:\"|?*/test.db");
        AuthService auth_inv("invalid_dir<>:\"|?*/test.db", "");
        auto login_inv = auth_inv.login("any@email.com", "any_pwd");
        record_test("Failed database open reports error and AuthService returns 503 DATABASE_UNAVAILABLE",
                    !open7 && !db7.is_open() && !auth_inv.is_valid() &&
                    !login_inv.success && login_inv.status_code == 503 &&
                    login_inv.code == "DATABASE_UNAVAILABLE");

        // 56. AuthService test database isolation: data in DB A does not leak to DB B
        std::string p8_a = "data/test_c1_iso_a.db";
        std::string p8_b = "data/test_c1_iso_b.db";
        try { std::filesystem::remove(p8_a); } catch (...) {}
        try { std::filesystem::remove(p8_b); } catch (...) {}
        {
            AuthService auth_a(p8_a, "");
            auth_a.register_user("User Only In A", "only_a@goride.vn", "0911111111", "pass123456", "customer");
        }
        {
            AuthService auth_b(p8_b, "");
            User check_u;
            bool found_in_b = auth_b.find_user_by_email("only_a@goride.vn", check_u);
            record_test("AuthService database isolation: user created in DB A does not leak into DB B",
                        !found_in_b);
        }
        try { std::filesystem::remove(p8_a); } catch (...) {}
        try { std::filesystem::remove(p8_b); } catch (...) {}

        // 57. In-memory AuthService never creates or overwrites legacy json backup
        std::string fake_json = "data/test_should_not_exist.json";
        try { std::filesystem::remove(fake_json); } catch (...) {}
        {
            AuthService auth_mem("", fake_json);
            auth_mem.register_user("Mem User", "mem@goride.vn", "0922334455", "memPass123", "customer");
        }
        bool json_leak = std::filesystem::exists(fake_json);
        try { std::filesystem::remove(fake_json); } catch (...) {}
        record_test("In-memory AuthService never writes to legacy json backup",
                    !json_leak);
    }

    // Summary
    std::cout << "\n==========================================================" << std::endl;
    std::cout << "TEST SUMMARY: " << passed_tests << "/" << total_tests << " Passed ("
              << (passed_tests == total_tests ? "100% SUCCESS" : "FAILURES DETECTED") << ")" << std::endl;
    std::cout << "==========================================================" << std::endl;

    return (passed_tests == total_tests) ? 0 : 1;
}
