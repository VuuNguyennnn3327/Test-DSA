# GoRide (RIDE-RTREE) 🚗
> **“Đặt xe dễ dàng, đi mọi hành trình.”**

Hệ thống đặt xe trực tuyến mô phỏng - Bài tập lớn môn Cấu trúc dữ liệu và Giải thuật (DSA)

## Tổng quan

**GoRide** là một ứng dụng đặt xe web-based, ứng dụng các cấu trúc dữ liệu và giải thuật không gian như **R-Tree** (lập chỉ mục không gian, tìm k tài xế lân cận) và **Dijkstra** (tìm lộ trình ngắn nhất trên đồ thị mạng đường giao thông).

## Trạng thái hiện tại: Phase 1.2 (Redesign From Project Landing Page To Real Ride-Hailing App)

### ✅ Đã hoàn thành (Phase 0, 1, 1.1 & 1.2)
- **Tái thiết kế Trang chủ thành Ứng dụng Đặt xe Thực thụ (ref-01)**: Thay thế giao diện landing page marketing thành giao diện App Home chuẩn Grab/be; gồm thanh tìm kiếm nhanh *"Bạn muốn đi đâu hôm nay?"*, lưới 6 dịch vụ di chuyển (Xe máy, Ô tô 4 chỗ, GoRide Plus, Xe 7 chỗ, Giao hàng, Đặt trước), thẻ phương thức thanh toán/ví, danh sách địa chỉ đã lưu & gần đây (Nhà, Công ty, Chợ Bến Thành, Sân bay Tân Sơn Nhất, ĐH Bách Khoa).
- **Màn hình Tìm kiếm Điểm đến (ref-02)**: Thiết kế twin-inputs (Điểm đón & Điểm đến), nút đảo chiều lộ trình, nút *"Chọn trên bản đồ"*, danh sách gợi ý địa điểm TP.HCM lọc theo thời gian thực kèm huy hiệu điểm đến phổ biến.
- **Xác nhận Điểm đón trên Bản đồ (ref-03)**: Bản đồ Leaflet toàn màn hình với ghim trung tâm định vị động, nút GPS định vị lại, các pill tùy chọn chuyến đi (*Chuyến đi yên lặng*, *Có hành lý*), nút CTA lớn *"XÁC NHẬN ĐIỂM ĐÓN"*.
- **Chọn Phương tiện & Ước tính Cước (ref-04)**: Thanh tóm tắt lộ trình nổi trên đầu, lộ trình polyline trên bản đồ Leaflet, bottom sheet danh mục xe (GoRide Bike, Car 4 chỗ, GoRide Plus, Car 7 chỗ) với cước phí tự động tính theo cự ly thực tế, thông báo nhu cầu cao, chọn phương thức thanh toán, nút CTA *"Đặt chuyến GoRide"*.
- **Mô phỏng Tìm Tài xế & Theo dõi Chuyến đi Thời gian Thực (ref-05/06)**: Hiệu ứng sóng radar quét tài xế (mô phỏng k-NN R-Tree), chuyển đổi trạng thái *Tài xế đang đến*, xe tài xế di chuyển thực tế trên bản đồ, thẻ thông tin tài xế (avatar, tên, đánh giá 4.88★, biển số 51G-889.24, loại xe), thanh tiến trình chuyến đi, các nút gọi điện, nhắn tin và hủy chuyến kèm modal đánh giá sau chuyến.
- **Đồng bộ thương hiệu GoRide**: Tên thương hiệu **GoRide** cùng slogan *“Đặt xe dễ dàng, đi mọi hành trình.”* trên toàn bộ giao diện và router.
- **Responsive chuẩn mực**: Tối ưu hai chế độ PC (1440/1280) và Mobile (390/360) bằng CSS Grid/Flexbox/Media Queries; App Shell đóng khung thiết bị di động sang trọng trên desktop.
- **Design system nhất quán**: CSS variables cho màu sắc teal/cyan/blue, typography, khoảng cách, buttons, cards, modals, toasts.
- **API Client & Demo Data Adapter**: Tách bạch giao diện khỏi dữ liệu; sẵn sàng thay thế bằng `ApiDataAdapter` khi kết nối C++ backend.
- **CMake skeleton**: Chuẩn C++17 cho backend.

### ❌ Chưa triển khai
- Backend C++ (HTTP server, REST endpoints)
- SQLite database
- Authentication thực sự
- R-Tree spatial indexing
- Dijkstra routing
- Driver simulator
- Real-time updates
- GPS integration
- Thanh toán

## Cấu trúc thư mục

```
RIDE-RTREE/
├── CMakeLists.txt              # Cấu hình CMake build cho C++17
├── README.md                   # Tài liệu hướng dẫn dự án
├── .gitignore                  # Git ignore rules
├── backend/
│   └── main.cpp                # Điểm khởi chạy C++17 backend skeleton
├── database/
│   └── schema.sql              # Database schema placeholder cho SQLite
└── frontend/                   # Frontend web shell
    ├── index.html              # Cổng điều hướng trung tâm & splash portal
    ├── pages/
    │   ├── login.html          # Trang đăng nhập với gợi ý tài khoản demo
    │   ├── register.html       # Đăng ký tài khoản (Khách hàng / Tài xế)
    │   ├── customer.html       # Giao diện đặt xe khách hàng + bản đồ Leaflet
    │   ├── driver.html         # Giao diện tài xế (nhận chuyến, trạng thái online)
    │   ├── admin.html          # Bảng điều khiển quản trị (thống kê, duyệt tài xế)
    │   └── dsa-demo.html       # Giao diện shell minh họa thuật toán R-Tree & Dijkstra
    ├── css/
    │   ├── design-system.css   # CSS variables, typography, buttons, inputs, modals
    │   ├── layout.css          # App shell, header, sidebar, map layout, panels
    │   ├── components.css      # Booking panel, trip cards, stat cards, request cards
    │   └── responsive.css      # Media queries cho laptop (1440/1280) và mobile (390)
    └── js/
        ├── core/
        │   ├── api.js          # REST API Client (ApiClient, ApiError, auth header)
        │   ├── session.js      # Quản lý phiên UI (SessionManager)
        │   ├── router.js       # Điều hướng & bảo vệ tuyến đường (Router)
        │   ├── demo-data.js    # DemoDataAdapter (tách biệt mock data)
        │   └── app.js          # App khởi tạo, Toast alert, Modal, định dạng tiền tệ
        ├── map/
        │   ├── map-controller.js # Wrapper Leaflet (zoom, pan, marker, click chọn tọa độ)
        │   └── map-renderer.js   # Vẽ lộ trình, khoanh vùng R-Tree MBR, k-NN
        ├── customer/
        │   └── customer.js     # Controller trang khách hàng
        ├── driver/
        │   └── driver.js       # Controller trang tài xế
        └── admin/
            └── admin.js        # Controller trang quản trị
```

## Công nghệ sử dụng

| Component | Technology |
|-----------|------------|
| Frontend | HTML, CSS, JavaScript (vanilla) |
| Bản đồ | Leaflet 1.9.4 (OpenStreetMap tiles) |
| Backend | C++17 (planned) |
| Build | CMake 3.16+ |
| Database | SQLite (planned) |
| API | REST + JSON (planned) |

## Cách chạy

### Frontend
Mở `frontend/index.html` trong trình duyệt, hoặc sử dụng HTTP server:

```bash
# Python
cd frontend && python -m http.server 8080

# Node.js
npx serve frontend

# VS Code: Live Server extension
```

Sau đó truy cập: http://localhost:8080

Tài khoản demo:
- customer@demo.com → Giao diện khách hàng
- driver@demo.com → Giao diện tài xế
- admin@demo.com → Giao diện quản trị

(Mật khẩu bất kỳ - chưa có xác thực thực sự)

### Backend (Skeleton)
```bash
mkdir build && cd build
cmake ..
cmake --build .
./bin/ride_rtree_server
```

## Giai đoạn tiếp theo

- Phase 2: SQLite database + schema
- Phase 3: C++ HTTP server + REST API
- Phase 4: R-Tree implementation
- Phase 5: Dijkstra routing
- Phase 6: Driver simulator
- Phase 7: Full integration

## Lưu ý quan trọng

> Dữ liệu hiển thị trên giao diện là **dữ liệu mẫu (demo data)**, không phải dữ liệu thực.
> Backend chưa được triển khai. Các thao tác trên giao diện chỉ là mô phỏng.

## Giấy phép

DSA Course Project - For Educational Purposes
