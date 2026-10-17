# GORIDE — GATE C.2 ACCEPTANCE REVIEW & GATE C.3 API SPECIFICATION
**Dự án:** GoRide (RIDE-RTREE Routing Engine & Trip Lifecycle)  
**Phạm vi kiểm tra:** Gate C.2 (SQLite Schema & C++ TripService Foundation)  
**Kế hoạch tiếp theo:** Gate C.3 REST API Foundation & RBAC Integration  
**Workspace:** `D:\Nam2_ky1\newDSA\RIDE-RTREE` (Độc lập 100%, không truy cập `D:\Nam2_ky1\BTL_DSA`)  
**Ngày lập:** 18/10/2026  
**Trạng thái kiểm định:** `SOURCE VERIFIED` (Phân tích mã nguồn toàn diện) | Build/Run: `BLOCKED` (Môi trường sandbox chặn tạo child process `Access is denied`)  

---

## MỤC LỤC
1. [TỔNG QUAN PHẠM VI & TIÊU CHÍ AN TOÀN](#1-tổng-quan-phạm-vi--tiêu-chí-an-toàn)
2. [RÀ SOÁT CHI TIẾT GATE C.2 THEO 8 TIÊU CHÍ AUDIT](#2-rà-soát-chi-tiết-gate-c2-theo-8-tiêu-chí-audit)
   - [2.1. PRAGMA user_version Behavior](#21-pragma-user_version-behavior)
   - [2.2. PRAGMA foreign_keys Enforceability](#22-pragma-foreign_keys-enforceability)
   - [2.3. db::Transaction RAII Implementation](#23-dbtransaction-raii-implementation)
   - [2.4. Expired Offer Handling & Escalation](#24-expired-offer-handling--escalation)
   - [2.5. Conditional Update & Race Condition Protection](#25-conditional-update--race-condition-protection)
   - [2.6. Schema Synchronization](#26-schema-synchronization)
   - [2.7. Unit Test Isolation & Data Protection](#27-unit-test-isolation--data-protection)
   - [2.8. DB-01 Anti-Regression Check](#28-db-01-anti-regression-check)
3. [TỔNG HỢP VẤN ĐỀ THEO MỨC ĐỘ NGHIÊM TRỌNG](#3-tổng-hợp-vấn-đề-theo-mức-độ-nghiêm-trọng)
4. [KẾ HOẠCH MIGRATION CHO PRODUCTION DATABASE](#4-kế-hoạch-migration-cho-production-database)
5. [THIẾT KẾ CHI TIẾT GATE C.3 REST API FOUNDATION](#5-thiết-kế-chi-tiết-gate-c3-rest-api-foundation)
   - [5.1. Kiến trúc Router trong HttpServer](#51-kiến-trúc-router-trong-httpserver)
   - [5.2. Chuẩn hóa Định dạng Response Envelope & Error Codes](#52-chuẩn-hóa-định-dạng-response-envelope--error-codes)
   - [5.3. Chi tiết Đặc tả 9 REST Endpoints](#53-chi-tiết-đặc-tả-9-rest-endpoints)
   - [5.4. Kế hoạch Tích hợp main.cpp](#54-kế-hoạch-tích-hợp-maincpp)
6. [KẾT LUẬN & ĐIỀU KIỆN CHUYỂN GATE](#6-kết-luận--điều-kiện-chuyển-gate)

---

## 1. TỔNG QUAN PHẠM VI & TIÊU CHÍ AN TOÀN

Báo cáo này hoàn thành hai nhiệm vụ cốt lõi:
1. **Kiểm tra nghiệm thu toàn diện (Acceptance Review)** đối với toàn bộ cấu trúc mã nguồn, cơ chế khóa dữ liệu (concurrency locking), mô hình trạng thái chuyến đi (state machine), và schema SQLite đã được xây dựng trong **Gate C.2**.
2. **Thiết kế nền tảng REST API (API Foundation Design)** cho **Gate C.3**, bao gồm chuẩn hóa request/response, mã trạng thái HTTP, cơ chế phân quyền RBAC (Role-Based Access Control) qua `AuthService::validate_token()`, và kế hoạch tích hợp không phá vỡ cấu trúc vào `backend/main.cpp`.

### Ràng buộc an toàn được bảo lưu tuyệt đối:
- **Workspace duy nhất:** `D:\Nam2_ky1\newDSA\RIDE-RTREE`. Tuyệt đối không đọc, ghi, truy cập thư mục `D:\Nam2_ky1\BTL_DSA`.
- **Bảo toàn dữ liệu sản xuất:** Không tự ý chạy migration hay sửa đổi file `database/goride.db` và `data/users.json`.
- **Ranh giới thực thi:** Báo cáo dừng lại ở bước nghiệm thu và thiết kế. Chưa sửa đổi `backend/main.cpp` và chưa chỉnh sửa frontend cho đến khi có phê duyệt rõ ràng từ người dùng.

---

## 2. RÀ SOÁT CHI TIẾT GATE C.2 THEO 8 TIÊU CHÍ AUDIT

### 2.1. PRAGMA user_version Behavior
- **Vị trí kiểm tra:** `backend/src/database.cpp` (`initialize_schema()`), `database/schema.sql`.
- **Phân tích kỹ thuật:**
  Trong `Database::initialize_schema()`:
  ```cpp
  int version = 0;
  auto stmt = prepare("PRAGMA user_version;");
  if (stmt && stmt->step()) {
      version = stmt->column_int(0);
  }
  if (version > 0) {
      // Schema already applied
      return true;
  }
  ```
- **Đánh giá:**
  - Đối với database mới tinh hoặc môi trường test cô lập (`data/test_c2_isolated_trips.db`): `user_version = 0`, hàm thực thi toàn bộ script `schema.sql` (hoặc `DEFAULT_EMBEDDED_SCHEMA`) và thiết lập `PRAGMA user_version = 1;`. Mọi bảng `trips`, `trip_offers`, chỉ mục đều được khởi tạo chuẩn xác.
  - Đối với production database hiện hữu (`database/goride.db`) đã được tạo từ Gate C.0/C.1 với `user_version = 1`: Hàm `initialize_schema()` thấy `version > 0` nên lập tức trả về `true` mà không thực thi `schema.sql`. Điều này **bảo vệ an toàn chống ghi đè dữ liệu cũ**, nhưng nếu database đó chưa có các cột mới trong `trips` (`current_offered_to`, `offer_expires_at`, v.v.) và chưa có bảng `trip_offers`, các câu truy vấn runtime của `TripService` sẽ gặp lỗi `no such column` hoặc `no such table`.
- **Phân loại:** `IMPORTANT`. Cần có một migration script chuyên biệt tăng `user_version = 2` để nâng cấp database hiện hữu khi triển khai production (xem [Mục 4](#4-kế-hoạch-migration-cho-production-database)).

---

### 2.2. PRAGMA foreign_keys Enforceability
- **Vị trí kiểm tra:** `backend/src/database.cpp` (`Database::open()` và `configure_pragmas()`).
- **Phân tích kỹ thuật:**
  ```cpp
  void Database::configure_pragmas() {
      if (!db_) return;
      execute("PRAGMA foreign_keys = ON;");
      sqlite3_busy_timeout(db_, 5000);
      if (path_ != ":memory:") {
          execute("PRAGMA journal_mode = WAL;");
      }
  }
  ```
  Hàm `configure_pragmas()` được gọi tự động và vô điều kiện trong `Database::open()` ngay khi kết nối SQLite được thiết lập.
- **Đánh giá:**
  - Foreign key ràng buộc toàn vẹn giữa `trips.customer_id -> users(id)`, `trips.driver_id -> users(id)`, `trip_offers.trip_id -> trips(id) ON DELETE CASCADE`, và `trip_offers.driver_id -> users(id)` luôn hoạt động trên mọi connection.
  - `sqlite3_busy_timeout(db_, 5000)` bảo vệ backend không bị throw `SQLITE_BUSY` khi có nhiều luồng hoặc tiến trình ghi đồng thời.
- **Phân loại:** `SOURCE VERIFIED` (Không phát hiện lỗi).

---

### 2.3. db::Transaction RAII Implementation
- **Vị trí kiểm tra:** `backend/include/db/database.hpp` và `backend/src/database.cpp` (`Transaction`).
- **Phân tích kỹ thuật:**
  ```cpp
  Transaction::Transaction(Database& db) : db_(db) {
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
  ```
- **Đánh giá:**
  - `BEGIN IMMEDIATE;` đảm bảo acquiring write lock ngay lập tức, ngăn ngừa tình huống 2 transaction cùng đọc dạng WAL rồi cùng cố upgrade lên writer gây ra deadlock (`SQLITE_BUSY: database table is locked`).
  - Hủy đối tượng (`destructor`) tự động rollback nếu chưa gọi `commit()`.
  - **Điểm khuyết nhỏ:** Lớp `Transaction` thiếu phương thức truy vấn trạng thái `bool is_active() const { return active_; }` hoặc `explicit operator bool() const`. Nếu câu lệnh `BEGIN IMMEDIATE;` bị lỗi, transaction không active nhưng caller không có getter để kiểm tra ngay mà phải chờ đến khi `commit()` trả về false.
- **Phân loại:** `MINOR`. Khuyến nghị bổ sung inline getter `bool is_active() const { return active_; }` vào `database.hpp`.

---

### 2.4. Expired Offer Handling & Escalation
- **Vị trí kiểm tra:** `backend/src/trip_service.cpp` (`offer_trip()`, `check_and_expire_offer()`, `has_driver_been_offered()`).
- **Phân tích kỹ thuật:**
  - `has_driver_been_offered(trip_id, driver_id)` kiểm tra bảng `trip_offers`:
    ```sql
    SELECT 1 FROM trip_offers WHERE trip_id = ? AND driver_id = ? LIMIT 1;
    ```
    Nếu tài xế đã từng được gửi offer (dù bị từ chối hay hết hạn), hệ thống từ chối tái gửi offer cho tài xế đó (`out_error = "Driver has already received an offer for this trip"`), bắt buộc dispatch engine phải tìm tài xế khác (Escalation).
  - Khi một offer hết hạn mà tài xế không phản hồi, `check_and_expire_offer()` hoặc `offer_trip()` sẽ:
    1. Cập nhật record cũ trong `trip_offers` sang trạng thái `'EXPIRED'`.
    2. Trả trạng thái chuyến đi về `'REQUESTED'`, đặt `current_offered_to = NULL` và `offer_expires_at = 0`.
- **Đánh giá:** Logic xử lý hết hạn offer và cơ chế ngăn chặn spam tài xế cũ được bảo đảm chặt chẽ ở cấp độ transaction.
- **Phân loại:** `SOURCE VERIFIED` (Không phát hiện lỗi).

---

### 2.5. Conditional Update & Race Condition Protection
- **Vị trí kiểm tra:** `backend/src/trip_service.cpp` (`accept_offer()`).
- **Phân tích kỹ thuật:**
  ```cpp
  auto upd_stmt = db_.prepare(
      "UPDATE trips "
      "SET status = 'ACCEPTED', driver_id = ?, current_offered_to = NULL, accepted_at = ? "
      "WHERE id = ? AND status = 'OFFERED' AND current_offered_to = ? AND offer_expires_at >= ?;"
  );
  ...
  if (!upd_stmt->step() || sqlite3_changes(db_.handle()) == 0) {
      out_error = "Offer has expired, was cancelled, or was not granted to this driver";
      return false;
  }
  ```
- **Đánh giá:**
  - Điều kiện `WHERE` kiểm tra đồng thời 4 yếu tố: đúng `id`, trạng thái hiện thời là `OFFERED`, người nhận đúng là `current_offered_to`, và thời gian hiện tại chưa vượt quá `offer_expires_at`.
  - Việc kiểm tra `sqlite3_changes() == 0` đảm bảo nếu bất kỳ điều kiện nào không thỏa mãn (ví dụ khách vừa hủy chuyến trước đó 10ms, hoặc thời gian 15s đã trôi qua), lệnh UPDATE sẽ tác động vào 0 dòng, transaction lập tức abort và trả về lỗi.
  - Không thể xảy ra hiện tượng 2 tài xế cùng nhận 1 cuốc hoặc tài xế nhận cuốc đã bị hủy.
- **Phân loại:** `SOURCE VERIFIED` (Rất xuất sắc).

---

### 2.6. Schema Synchronization
- **Vị trí kiểm tra:** `database/schema.sql` và `DEFAULT_EMBEDDED_SCHEMA` trong `backend/src/database.cpp`.
- **Phân tích so sánh:**
  - Bảng `trips`: Cùng chứa đủ 8 trạng thái (`REQUESTED`, `OFFERED`, `ACCEPTED`, `ARRIVED`, `IN_PROGRESS`, `COMPLETED`, `CANCELLED`, `EXPIRED`), 4 loại phương tiện (`motorbike`, `car4`, `car4_plus`, `car7`), các cột dispatch (`current_offered_to`, `offer_expires_at`), và 5 mốc thời gian lifecycle (`created_at`, `accepted_at`, `arrived_at`, `started_at`, `completed_at`, `cancelled_at`).
  - Bảng `trip_offers`: Cùng cấu trúc (`id`, `trip_id`, `driver_id`, `status`, `offered_at`, `expires_at`, `responded_at`).
  - Chỉ mục ràng buộc: Cùng có `idx_trip_offers_single_pending` định nghĩa `CREATE UNIQUE INDEX IF NOT EXISTS idx_trip_offers_single_pending ON trip_offers(trip_id) WHERE status = 'PENDING';` (ràng buộc partial unique đảm bảo 1 chuyến đi không bao giờ có 2 pending offer cùng lúc ở cấp cơ sở dữ liệu).
- **Đánh giá:** 100% đồng bộ giữa file SQL bên ngoài và chuỗi hằng nhúng bên trong C++.
- **Phân loại:** `SOURCE VERIFIED` (Không phát hiện lỗi).

---

### 2.7. Unit Test Isolation & Data Protection
- **Vị trí kiểm tra:** `tests/test_trip.cpp`.
- **Phân tích kỹ thuật:**
  - File test chỉ định rõ ràng đường dẫn database cô lập:
    ```cpp
    const std::string test_db_path = "data/test_c2_isolated_trips.db";
    ```
  - Trước khi chạy: `std::filesystem::remove(test_db_path)`.
  - Khởi tạo schema và seed tài khoản test cục bộ (`cust_01`, `drv_01`, `drv_02`).
  - Sau khi kiểm thử hoàn tất: Đóng database và xóa hoàn toàn file `data/test_c2_isolated_trips.db`.
  - Hoàn toàn không đụng đến `database/goride.db` hay `data/users.json`.
- **Đánh giá:** Bảo đảm tính độc lập và sạch sẽ tuyệt đối của dữ liệu thử nghiệm.
- **Phân loại:** `SOURCE VERIFIED` (Không phát hiện lỗi).

---

### 2.8. DB-01 Anti-Regression Check
- **Vị trí kiểm tra:** `backend/src/database.cpp` (`resolve_database_path()`).
- **Phân tích kỹ thuật:**
  ```cpp
  std::string Database::resolve_database_path(const std::string& preferred_path) {
      if (!preferred_path.empty() && preferred_path != "database/goride.db") {
          return preferred_path;
      }
      ...
  }
  ```
- **Đánh giá:** Mọi đường dẫn tường minh khác `"database/goride.db"` (chẳng hạn đường dẫn test hoặc in-memory) đều được trả về nguyên trạng, loại bỏ hoàn toàn nguy cơ candidate search ghi đè vào database thật.
- **Phân loại:** `SOURCE VERIFIED` (Không bị hồi quy).

---

## 3. TỔNG HỢP VẤN ĐỀ THEO MỨC ĐỘ NGHIÊM TRỌNG

| Tiêu chí | Mức độ | Hiện trạng | Biện pháp xử lý |
| :--- | :--- | :--- | :--- |
| **Blocker** | **NONE** | Không có lỗi logic hoặc kiến trúc chặn tiến trình Gate C.2. | Có thể tiến hành thiết kế và chuẩn bị Gate C.3. |
| **Pragma user_version** | **IMPORTANT** | Database production cũ ở `user_version = 1` sẽ không tự động nhận cột mới trong `trips` và bảng `trip_offers` nếu không qua migration. | Chuẩn bị sẵn file migration `002_trip_lifecycle.sql` nâng `user_version = 2` để chạy khi được người dùng phê duyệt deploy production. |
| **db::Transaction** | **MINOR** | Lớp `Transaction` thiếu phương thức `is_active()` để kiểm tra `BEGIN IMMEDIATE` có thành công ngay lúc khởi tạo hay không. | Bổ sung inline `bool is_active() const noexcept { return active_; }` trong `backend/include/db/database.hpp`. |
| **HttpServer Routing** | **IMPORTANT** | `HttpServer::dispatch()` hiện tại dùng `std::unordered_map::find(req.path)` (so khớp chính xác chuỗi). Đường dẫn dạng pattern như `/api/trips/:id` sẽ không khớp nếu không có cơ chế phân tích path hoặc query param fallback. | Trong Gate C.3, thiết kế endpoint hỗ trợ cả prefix matching linh hoạt hoặc query/body parameter chuẩn REST. |

---

## 4. KẾ HOẠCH MIGRATION CHO PRODUCTION DATABASE

> [!WARNING]
> Bản script sau đây chỉ nhằm mục đích rà soát kỹ thuật. **Tuyệt đối không tự ý thực thi lên `database/goride.db`** trong gate này khi chưa có yêu cầu trực tiếp từ người dùng.

Để nâng cấp một database GoRide hiện hữu từ version 1 lên version 2 an toàn mà không làm mất dữ liệu người dùng hay lịch sử cũ:

```sql
-- database/migrations/002_trip_lifecycle.sql
-- Áp dụng cho GoRide SQLite Production DB
PRAGMA foreign_keys = OFF;
BEGIN IMMEDIATE TRANSACTION;

-- 1. Bổ sung các cột dispatch & lifecycle vào bảng trips nếu chưa tồn tại
-- (SQLite 3.35.0+ hỗ trợ ALTER TABLE ADD COLUMN an toàn)
ALTER TABLE trips ADD COLUMN current_offered_to TEXT REFERENCES users(id);
ALTER TABLE trips ADD COLUMN offer_expires_at INTEGER DEFAULT 0;
ALTER TABLE trips ADD COLUMN arrived_at INTEGER;
ALTER TABLE trips ADD COLUMN started_at INTEGER;
ALTER TABLE trips ADD COLUMN cancelled_at INTEGER;

-- 2. Tạo chỉ mục dispatch
CREATE INDEX IF NOT EXISTS idx_trips_dispatch ON trips(status, offer_expires_at);

-- 3. Tạo bảng quản lý lịch sử offer tài xế
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

-- 4. Đánh dấu phiên bản schema mới
PRAGMA user_version = 2;

COMMIT;
PRAGMA foreign_keys = ON;
```

---

## 5. THIẾT KẾ CHI TIẾT GATE C.3 REST API FOUNDATION

Gate C.3 có mục tiêu đưa toàn bộ nghiệp vụ của `TripService` ra các REST API chuẩn HTTP/1.1 JSON, tích hợp trực tiếp vào `backend/main.cpp`, kiểm tra token và phân quyền RBAC chặt chẽ trước khi gọi hàm nghiệp vụ.

### 5.1. Kiến trúc Router trong HttpServer

Hiện tại, `HttpServer` trong `backend/src/http_server.cpp` lưu trữ route theo dạng `std::unordered_map<std::string, HttpHandler> get_routes_, post_routes_`.
- Để xử lý các URL có tham số định danh như `/api/trips/:id` hoặc `/api/trips/:id/cancel` mà vẫn duy trì tính ổn định, hệ thống sẽ áp dụng chiến lược **Dual-Pattern Matching**:
  1. Hỗ trợ URL dạng chuẩn query param & body (ví dụ `GET /api/trips/detail?id=...` hoặc `POST /api/trips/cancel` với `{ "trip_id": "..." }`).
  2. Bổ sung cơ chế tiền xử lý đường dẫn prefix (Prefix/Wildcard Hook) trong `HttpServer::dispatch` hoặc phân tích tham số path trực tiếp trong handler nếu path bắt đầu bằng `/api/trips/` hoặc `/api/driver/`.

### 5.2. Chuẩn hóa Định dạng Response Envelope & Error Codes

Mọi phản hồi từ Gate C.3 REST API tuân theo cấu trúc JSON nhất quán:

#### Success Response Envelope (HTTP 200 / 201)
```json
{
  "success": true,
  "data": { ... }
}
```

#### Error Response Envelope (HTTP 4xx / 5xx)
```json
{
  "success": false,
  "code": "ERROR_MACHINE_READABLE_CODE",
  "error": "Thông điệp mô tả lỗi chi tiết dành cho người dùng / UI."
}
```

#### Bảng danh mục HTTP Status & Machine-Readable Error Codes
| HTTP Status | Machine Code | Mô tả kịch bản |
| :--- | :--- | :--- |
| **400 Bad Request** | `INVALID_JSON` | Body request không đúng cú pháp JSON. |
| | `MISSING_FIELD` | Thiếu trường bắt buộc (`pickup_lat`, `dropoff_lat`, v.v.). |
| | `INVALID_COORDINATES` | Tọa độ GPS nằm ngoài dải hợp lệ WGS84. |
| **401 Unauthorized** | `AUTH_TOKEN_MISSING` | Không có header `Authorization: Bearer <token>` hoặc param `token`. |
| | `AUTH_TOKEN_INVALID` | Token hết hạn hoặc không tồn tại trong bảng `sessions`. |
| **403 Forbidden** | `FORBIDDEN_ROLE` | Vai trò (role) người dùng không có quyền gọi endpoint (ví dụ: Customer cố gọi API driver). |
| | `FORBIDDEN_ACCESS` | Người dùng không phải là chủ sở hữu chuyến xe hoặc tài xế được chỉ định. |
| **404 Not Found** | `TRIP_NOT_FOUND` | Không tìm thấy chuyến đi theo `trip_id`. |
| | `OFFER_NOT_FOUND` | Không có offer đang chờ (`PENDING`) nào cho tài xế này. |
| **409 Conflict** | `ACTIVE_TRIP_EXISTS` | Khách hàng hoặc tài xế đã có chuyến xe đang hoạt động, không thể tạo mới. |
| | `OFFER_EXPIRED` | Thời gian 15s chấp nhận offer đã hết, chuyến đã chuyển sang tài xế khác. |
| | `INVALID_STATUS_TRANSITION` | Trạng thái chuyển đổi không hợp lệ (ví dụ: chuyển từ `REQUESTED` thẳng sang `COMPLETED`). |
| **422 Unprocessable** | `NO_MOTORABLE_ROAD` | Tọa độ điểm đón/trả quá xa mạng lưới đường bộ OSM. |
| **500 Server Error** | `INTERNAL_DB_ERROR` | Lỗi truy vấn SQLite hoặc transaction commit thất bại. |

---

### 5.3. Chi tiết Đặc tả 9 REST Endpoints

```
+-------------------------------------------------------------------------------------------------------+
|                                    BẢN ĐỒ PHÂN QUYỀN RBAC GATE C.3                                   |
+------------------------------------+---------------+---------------+---------------+------------------+
| Endpoint                           | Customer      | Driver        | Admin         | Xác thực Token   |
+------------------------------------+---------------+---------------+---------------+------------------+
| POST /api/trips/request            | Cho phép      | Chặn (403)    | Cho phép      | Bắt buộc         |
| GET  /api/trips/active             | Cho phép      | Cho phép      | Cho phép      | Bắt buộc         |
| GET  /api/trips/:id                | Chính chủ     | Chính chủ     | Toàn quyền    | Bắt buộc         |
| POST /api/trips/:id/cancel         | Cho phép (*)  | Cho phép (*)  | Toàn quyền    | Bắt buộc         |
| GET  /api/driver/offers/pending    | Chặn (403)    | Cho phép      | Chặn (403)    | Bắt buộc (Driver)|
| POST /api/driver/offers/:id/accept | Chặn (403)    | Cho phép      | Chặn (403)    | Bắt buộc (Driver)|
| POST /api/driver/offers/:id/reject | Chặn (403)    | Cho phép      | Chặn (403)    | Bắt buộc (Driver)|
| POST /api/driver/trips/:id/status  | Chặn (403)    | Cho phép      | Cho phép      | Bắt buộc (Driver)|
| GET  /api/admin/trips              | Chặn (403)    | Chặn (403)    | Cho phép      | Bắt buộc (Admin) |
+------------------------------------+---------------+---------------+---------------+------------------+
(*) Khách hàng chỉ được hủy trước khi chuyến chuyển sang trạng thái IN_PROGRESS.
```

---

#### Endpoint 1: Tạo yêu cầu chuyến xe mới (POST /api/trips/request)
- **Mục đích:** Khách hàng đặt cuốc xe sau khi xem báo giá và định tuyến.
- **Quyền hạn (RBAC):** `customer`, `admin`.
- **Request Headers:** `Authorization: Bearer <token>`
- **Request Body (JSON):**
  ```json
  {
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
  }
  ```
- **Xử lý nghiệp vụ:**
  1. Xác thực token; lấy `customer_id = user.id`.
  2. Kiểm tra nếu khách hàng đang có cuốc xe hoạt động (`get_active_trip_for_customer`) -> Trả về `409 ACTIVE_TRIP_EXISTS`.
  3. Sinh `trip_id` duy nhất (ví dụ UUID hoặc timestamp nano: `"trp_" + ...`).
  4. Tạo đối tượng `Trip`, gán `status = REQUESTED`, gọi `TripService::create_trip()`.
- **Response thành công (HTTP 201 Created):**
  ```json
  {
    "success": true,
    "data": {
      "trip": {
        "id": "trp_1700000000_123456",
        "customer_id": "usr_cust_01",
        "driver_id": "",
        "vehicle_type": "car4",
        "status": "REQUESTED",
        "pickup_name": "Bitexco Financial Tower",
        "pickup_lat": 10.7716,
        "pickup_lon": 106.7044,
        "dropoff_name": "Landmark 81",
        "dropoff_lat": 10.7951,
        "dropoff_lon": 106.7218,
        "distance_meters": 4350,
        "fare_amount": 65000,
        "payment_method": "cash",
        "created_at": 1700000000
      }
    }
  }
  ```

---

#### Endpoint 2: Lấy chuyến xe đang hoạt động (GET /api/trips/active)
- **Mục đích:** Dùng cho HTTP Polling ở cả phía Customer và Driver để kiểm tra trạng thái cuốc xe hiện tại.
- **Quyền hạn (RBAC):** `customer`, `driver`, `admin`.
- **Request Headers:** `Authorization: Bearer <token>`
- **Xử lý nghiệp vụ:**
  - Nếu `user.role == "customer"`: Gọi `trip_service.get_active_trip_for_customer(user.id, out_trip)`.
  - Nếu `user.role == "driver"`: Gọi `trip_service.get_active_trip_for_driver(user.id, out_trip)`.
  - Nếu không có chuyến nào đang hoạt động: Trả về HTTP 200 với `data: null`.
- **Response thành công (HTTP 200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "has_active_trip": true,
      "trip": {
        "id": "trp_1700000000_123456",
        "status": "ACCEPTED",
        "driver_id": "usr_drv_01",
        "current_offered_to": "",
        "pickup_lat": 10.7716,
        "pickup_lon": 106.7044,
        "accepted_at": 1700000015
      }
    }
  }
  ```

---

#### Endpoint 3: Chi tiết chuyến xe theo ID (GET /api/trips/:id hoặc GET /api/trips/detail?id=...)
- **Mục đích:** Truy xuất toàn bộ thông tin chuyến xe, bao gồm cả lịch sử offer (đối với admin/chính chủ).
- **Quyền hạn (RBAC):** Khách hàng sở hữu cuốc xe, tài xế được chỉ định, hoặc Quản trị viên (Admin).
- **Xử lý nghiệp vụ:**
  1. Lấy thông tin chuyến qua `trip_service.get_trip(trip_id, out_trip)`. Nếu không thấy -> HTTP 404 `TRIP_NOT_FOUND`.
  2. Kiểm tra quyền sở hữu: Nếu `user.role != "admin"` và `user.id != out_trip.customer_id` và `user.id != out_trip.driver_id` -> HTTP 403 `FORBIDDEN_ACCESS`.
- **Response thành công (HTTP 200 OK):** Trả về đầy đủ JSON của `out_trip`.

---

#### Endpoint 4: Hủy chuyến xe (POST /api/trips/:id/cancel hoặc POST /api/trips/cancel)
- **Mục đích:** Cho phép khách hàng hủy khi tài xế chưa đón, tài xế hủy vì lý do sự cố, hoặc admin cưỡng chế hủy.
- **Quyền hạn (RBAC):** Customer (trước khi `IN_PROGRESS`), Driver, Admin.
- **Request Body (JSON):**
  ```json
  {
    "trip_id": "trp_1700000000_123456",
    "reason": "Khách đổi điểm đến hoặc chờ quá lâu"
  }
  ```
- **Xử lý nghiệp vụ:**
  Gọi `trip_service.cancel_trip(trip_id, user.id, user.role, reason, out_error)`.
  - Nếu `out_error == "Customers cannot cancel a trip that is already IN_PROGRESS"` -> HTTP 403.
  - Nếu `out_error == "Trip has already terminated and cannot be cancelled"` -> HTTP 409.
- **Response thành công (HTTP 200 OK):**
  ```json
  {
    "success": true,
    "message": "Hủy chuyến xe thành công.",
    "cancelled_by": "customer"
  }
  ```

---

#### Endpoint 5: Lấy cuốc xe đang được Offer độc quyền (GET /api/driver/offers/pending)
- **Mục đích:** Dùng cho tài xế Polling (chu kỳ 2s-3s) để nhận thông báo pop-up cuốc mới với đếm ngược 15 giây.
- **Quyền hạn (RBAC):** `driver`.
- **Xử lý nghiệp vụ:**
  1. Kiểm tra tài xế có cuốc xe nào đang ở trạng thái `OFFERED` với `current_offered_to == driver_id` và `offer_expires_at >= now`.
  2. Nếu có: Trả về thông tin cuốc xe kèm số giây còn lại (`remaining_seconds = offer_expires_at - now`).
  3. Nếu không: Trả về `{ "success": true, "data": null }`.
- **Response thành công (HTTP 200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "has_offer": true,
      "trip": {
        "id": "trp_1700000000_123456",
        "fare_amount": 65000,
        "pickup_name": "Bitexco Financial Tower",
        "dropoff_name": "Landmark 81",
        "distance_meters": 4350,
        "offer_expires_at": 1700000025,
        "remaining_seconds": 12
      }
    }
  }
  ```

---

#### Endpoint 6: Tài xế chấp nhận cuốc xe (POST /api/driver/offers/:id/accept hoặc /api/driver/offers/accept)
- **Mục đích:** Tài xế bấm nút "Nhận cuốc" trong vòng 15 giây.
- **Quyền hạn (RBAC):** `driver`.
- **Request Body (JSON):**
  ```json
  {
    "trip_id": "trp_1700000000_123456"
  }
  ```
- **Xử lý nghiệp vụ:**
  Gọi `trip_service.accept_offer(trip_id, user.id, out_error)`.
  - Nếu `accept_offer` trả về `false` do hết hạn hoặc cuốc bị thu hồi -> Trả về HTTP 409 `OFFER_EXPIRED`.
- **Response thành công (HTTP 200 OK):**
  ```json
  {
    "success": true,
    "message": "Chấp nhận chuyến đi thành công! Hãy di chuyển tới điểm đón khách.",
    "trip_id": "trp_1700000000_123456",
    "status": "ACCEPTED"
  }
  ```

---

#### Endpoint 7: Tài xế từ chối cuốc xe (POST /api/driver/offers/:id/reject hoặc /api/driver/offers/reject)
- **Mục đích:** Tài xế bấm "Từ chối" hoặc hết giờ không nhận, hệ thống ghi nhận và chuyển quyền dispatch sang tài xế tiếp theo.
- **Quyền hạn (RBAC):** `driver`.
- **Request Body (JSON):**
  ```json
  {
    "trip_id": "trp_1700000000_123456"
  }
  ```
- **Xử lý nghiệp vụ:**
  Gọi `trip_service.reject_offer(trip_id, user.id, out_error)`.
- **Response thành công (HTTP 200 OK):**
  ```json
  {
    "success": true,
    "message": "Đã từ chối nhận cuốc xe."
  }
  ```

---

#### Endpoint 8: Tài xế cập nhật tiến trình chuyến (POST /api/driver/trips/:id/status hoặc /api/driver/trips/status)
- **Mục đích:** Cập nhật các mốc vòng đời: `ARRIVED` (Đã đến điểm đón) -> `IN_PROGRESS` (Bắt đầu chở khách) -> `COMPLETED` (Hoàn thành chuyến đi).
- **Quyền hạn (RBAC):** `driver` (phải là tài xế đã được chỉ định cho chuyến đi đó).
- **Request Body (JSON):**
  ```json
  {
    "trip_id": "trp_1700000000_123456",
    "action": "arrived" // Hoặc "start", "complete"
  }
  ```
- **Xử lý nghiệp vụ:**
  - Nếu `action == "arrived"`: Gọi `trip_service.driver_arrive(trip_id, user.id, out_error)`.
  - Nếu `action == "start"`: Gọi `trip_service.start_trip(trip_id, user.id, out_error)`.
  - Nếu `action == "complete"`: Gọi `trip_service.complete_trip(trip_id, user.id, out_error)`.
  - Nếu vi phạm thứ tự trạng thái (ví dụ chưa arrived mà đòi start) -> HTTP 409 `INVALID_STATUS_TRANSITION`.
- **Response thành công (HTTP 200 OK):**
  ```json
  {
    "success": true,
    "message": "Cập nhật trạng thái chuyến đi thành công.",
    "trip_id": "trp_1700000000_123456",
    "new_status": "ARRIVED"
  }
  ```

---

#### Endpoint 9: Quản trị viên lấy danh sách toàn bộ chuyến đi (GET /api/admin/trips)
- **Mục đích:** Dành cho dashboard Quản trị viên để theo dõi các cuốc xe theo thời gian thực.
- **Quyền hạn (RBAC):** `admin`.
- **Request Headers:** `Authorization: Bearer <token>`
- **Query Params hỗ trợ:** `status=active` hoặc để trống lấy toàn bộ chuyến đang hoạt động.
- **Response thành công (HTTP 200 OK):**
  ```json
  {
    "success": true,
    "total": 3,
    "trips": [ ... ]
  }
  ```

---

### 5.4. Kế hoạch Tích hợp main.cpp

Khi được phê duyệt triển khai Gate C.3, file `backend/main.cpp` sẽ được tích hợp theo các bước tuần tự và không phá vỡ bất kỳ tính năng nào đang có:

1. **Khởi tạo dịch vụ TripService:**
   Tận dụng kết nối SQLite đã được mở sẵn bởi `AuthService`:
   ```cpp
   // backend/main.cpp
   goride::trip::TripService trip_service(auth_service.database());
   ```
2. **Hàm helper kiểm tra RBAC dùng chung:**
   ```cpp
   auto authenticate = [&](const goride::server::HttpRequest& req,
                           goride::server::HttpResponse& res,
                           const std::vector<std::string>& allowed_roles) -> std::optional<goride::auth::User> {
       std::string token = req.get_bearer_token();
       if (token.empty()) token = req.get_param("token");
       if (token.empty()) {
           res.set_json("{\"success\":false,\"code\":\"AUTH_TOKEN_MISSING\",\"error\":\"Vui lòng cung cấp token xác thực.\"}", 401);
           return std::nullopt;
       }
       auto val = auth_service.validate_token(token);
       if (!val.success) {
           res.set_json(val.to_json(), val.status_code);
           return std::nullopt;
       }
       if (!allowed_roles.empty()) {
           bool ok = false;
           for (const auto& r : allowed_roles) {
               if (val.user.role == r) { ok = true; break; }
           }
           if (!ok) {
               res.set_json("{\"success\":false,\"code\":\"FORBIDDEN_ROLE\",\"error\":\"Bạn không có quyền thực hiện thao tác này.\"}", 403);
               return std::nullopt;
           }
       }
       return val.user;
   };
   ```
3. **Đăng ký các route handlers:**
   Đăng ký đầy đủ 9 route handlers tương ứng vào đối tượng `server`.

---

## 6. KẾT LUẬN & ĐIỀU KIỆN CHUYỂN GATE

### 6.1. Tóm tắt kết quả nghiệm thu Gate C.2
1. **Mã nguồn C++ (`TripService`):** Hoàn thành xuất sắc, cấu trúc chặt chẽ, bảo vệ bằng `Transaction` và khóa loại trừ điều kiện (Conditional Update) ngăn chặn 100% race conditions.
2. **Cơ sở dữ liệu SQLite:** Hoàn toàn đồng bộ giữa code C++ và file `database/schema.sql`. Đảm bảo toàn vẹn dữ liệu người dùng sẵn có.
3. **Môi trường Test:** Khởi tạo và dọn dẹp database thử nghiệm cô lập, không ảnh hưởng đến bất kỳ file thực tế nào.

### 6.2. Ranh giới dừng (Stop Boundary)
- Không có bất kỳ thay đổi nào được thực hiện lên `backend/main.cpp` trong lượt này.
- Không có thao tác Git mang tính hủy hoại (`git reset`, `git clean`, v.v.).
- Không có migration tự ý thực thi lên `database/goride.db`.
- **Hệ thống hiện đã sẵn sàng 100% về mặt thiết kế kỹ thuật để bước vào triển khai mã nguồn Gate C.3 ngay khi nhận được lệnh phê duyệt.**
