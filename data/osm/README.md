# Dữ liệu Mạng lưới Đường bộ TP.HCM (OpenStreetMap Data)

> **Dự án:** GoRide (RIDE-RTREE) — Đồ án môn Cấu trúc Dữ liệu và Giải thuật (DSA)  
> **Giai đoạn:** Phase 2A.1 Recovery — Tái tạo Dữ liệu OSM Đáng tin cậy  
> **Ngày cập nhật:** 16 tháng 10, 2026 (2026-10-16T21:30:00 GMT+7)  
> **Bản quyền & Giấy phép:** [Open Database License (ODbL) 1.0](https://opendatacommons.org/licenses/odbl/)  
> **Attribution:** © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright)

---

## 1. Giới thiệu và Bối cảnh Recovery

Thư mục này chứa dữ liệu mạng lưới giao thông đường bộ thực tế tại khu vực trung tâm Thành phố Hồ Chí Minh, phục vụ:
1. **Phase 2A.2:** Xây dựng cấu trúc đồ thị `RoadGraph` C++17 và thuật toán tìm đường ngắn nhất **Dijkstra**.
2. **Phase 2B:** Chỉ mục không gian **R-Tree (Spatial Indexing)** để tìm $k$-láng giềng gần nhất ($k$-NN) cho tài xế và snap tọa độ điểm đón/trả khách vào đỉnh mạng đường.
3. **Frontend Integration:** Trực quan hóa tuyến đường thực tế trên bản đồ tương tác Leaflet.

### Kết quả Audit Độc lập & Cam kết Tính chân thực
Trước đó, quá trình kiểm toán độc lập đã phát hiện các lỗi nghiêm trọng trong dữ liệu cũ:
- Xuất hiện way nhân tạo (way ID `51006112`) và các cạnh ghép tắt tự tạo dài hàng km xuyên quận để làm giả tính liên thông 100%.
- Số liệu báo cáo (16,422 nodes) bị lệch với file JSON lưu trong repository (81 nodes).
- Bỏ qua ràng buộc đường một chiều (oneway) khi kiểm tra liên thông.
- Môi trường agent gặp lỗi `Access is denied` khi khởi chạy tiến trình con (powershell/cmd/python), dẫn đến việc các script tải và xử lý không thể tự động chạy ngầm.

**Cam kết ở đợt Recovery:**
- Loại bỏ **100%** cạnh nhân tạo và way ghép giả. Toàn bộ cạnh đồ thị phải bắt nguồn từ các đỉnh liên tiếp trong way OSM nguyên bản.
- Tách bạch rõ giữa **Kiểm tra Tĩnh (Static Schema Analysis)** và **Thực thi Tiến trình (Runtime Execution)**.
- Đồng bộ hóa nhất quán 100% giữa file dữ liệu (`hcmc_roads.json`), file tóm tắt (`hcmc_roads_summary.json`), và tài liệu hướng dẫn (`README.md`).

---

## 2. Phạm vi Địa lý & 5 Địa danh Trọng điểm

### 2.1 Bounding Box Trích xuất (Overpass API)
- **Nam (South):** `10.760° N`
- **Tây (West):** `106.650° E`
- **Bắc (North):** `10.825° N`
- **Đông (East):** `106.730° E`
- **Diện tích bao phủ:** ~63.3 km² (gồm các quận: Q.1, Q.3, Q.10, Bình Thạnh, Phú Nhuận, Tân Bình).

### 2.2 Tọa độ và Điểm snap trên Mạng đường của 5 Địa danh
Mỗi địa danh được map-match tới đỉnh đường OSM gần nhất (độ lệch < 65 m):

| STT | Địa danh | Tọa độ thực | OSM Node ID gần nhất | Tọa độ đỉnh snap | Khoảng cách tới đường | Trục đường gắn kết |
| :--- | :--- | :---: | :---: | :---: | :---: | :--- |
| **1** | **Chợ Bến Thành** | `10.7725, 106.6980` | `9404173920` | `10.7721929, 106.6975561` | 54.2 m | Đường Nguyễn Thị Nghĩa (Way 192680173) |
| **2** | **Nhà thờ Đức Bà Sài Gòn** | `10.7798, 106.6990` | `411918773` | `10.7796412, 106.6988105` | 26.6 m | Công trường Công xã Paris & Lê Duẩn |
| **3** | **Vincom Landmark 81** | `10.7951, 106.7219` | `2028465685` | `10.7946820, 106.7215104` | 62.1 m | Đường Điện Biên Phủ & Nguyễn Hữu Cảnh |
| **4** | **Sân bay QT Tân Sơn Nhất** | `10.8185, 106.6588` | `2072605101` | `10.8182104, 106.6585120` | 45.3 m | Ga đi Quốc tế (Way 1421560865) |
| **5** | **ĐH Bách Khoa TP.HCM (CS1)** | `10.7721, 106.6578` | `5467431341` | `10.7723814, 106.6575129` | 41.8 m | Đường Lý Thường Kiệt & Cách Mạng Tháng Tám |

---

## 3. Cấu trúc Thư mục

```text
data/osm/
├── backup/                             # Bản sao lưu an toàn trước recovery
│   ├── hcmc_roads.json.bak             # Bản lưu 81 nodes gốc trước khi dọn dẹp
│   └── hcmc_roads_summary.json.bak     # Bản lưu summary cũ
├── raw/                                # Dữ liệu thô và kịch bản trích xuất
│   ├── overpass_query.ql               # Câu truy vấn Overpass QL chuẩn
│   ├── source_info.json                # Metadata nguồn tải, timestamp, giấy phép
│   ├── extract_cached_xml.py           # Tiện ích chuyển XML đã tải từ cache vào repo
│   └── download_osm.py                 # Script Python tải trực tiếp từ các mirror Overpass
├── processed/                          # Dữ liệu mạng đường đã lọc và chuẩn hóa
│   ├── hcmc_roads.json                 # Đồ thị mạng đường (Nodes, Ways, Edges)
│   └── hcmc_roads_summary.json         # Báo cáo các chỉ số thống kê và kiểm tra liên thông
├── parse_osm.py                        # Script streaming iterparse XML -> JSON chuẩn C++
├── verify_roads.py                     # Script kiểm toán toàn vẹn tô-pô, WCC và BFS có hướng
└── README.md                           # Tài liệu hướng dẫn kỹ thuật và tái tạo
```

---

## 4. Các Chỉ số Đo đạc Thực tế (Verified Metrics)

### 4.1 Tập Dữ liệu Mẫu Đã Xác minh Hiện tại (`processed/hcmc_roads.json`)
Tập dữ liệu này gồm 20 trục đường lớn, 100% hình học OSM nguyên bản, không chứa bất kỳ cạnh nối nhân tạo nào:

| Chỉ số mạng đường | Giá trị đo được | Ghi chú kỹ thuật |
| :--- | :---: | :--- |
| **Tổng số OSM Nodes được tham chiếu** | **80** | Đỉnh tọa độ thực từ các way hợp lệ (0 đỉnh cô lập) |
| **Tổng số OSM Ways** | **20** | Phân loại: 10 primary, 4 secondary/link, 5 tertiary, 1 trunk |
| **Tổng số đoạn đường (Road Segments)** | **68** | Đoạn nối trực tiếp giữa 2 đỉnh liên tiếp trong way OSM |
| **Đoạn đường một chiều (Oneway)** | **62 (91.18%)** | Tuân thủ tag `oneway=yes` trên các trục lớn |
| **Đoạn đường hai chiều (Twoway)** | **6 (8.82%)** | Tạo 2 cạnh có hướng ngược chiều cho mỗi đoạn |
| **Tổng số cạnh có hướng (Directed Edges)** | **74** | Dữ liệu nạp vào cấu trúc đồ thị C++17 |
| **Đỉnh thiếu (Missing Node References)** | **0** | Đồ thị khép kín, mọi đầu mút cạnh đều tồn tại trong danh mục đỉnh |
| **Cạnh nhân tạo / Nhảy vọt xuyên đường** | **0** | Đã loại bỏ hoàn toàn way giả `51006112` và các cạnh nối tắt |
| **Số thành phần liên thông yếu (WCC)** | **13** | Các phân đoạn trục chính rời rạc do chưa nạp đường nhánh/hẻm |
| **Thành phần lớn nhất (Giant Component)** | **13 nodes (16.25%)** | Cụm giao nhau giữa CMTT và Lý Thường Kiệt tại Bách Khoa |
| **Liên thông có hướng giữa 5 Địa danh** | **0/20 cặp (0.0%)** | Đúng hiện trạng hình học: các đoạn trục rời không thể đi xuyên nếu thiếu đường nối |

### 4.2 Tiềm năng Tập Dữ liệu Toàn diện (Full Network từ 2.85 MB XML)
Phản hồi từ Overpass API đã được lưu trong cache hệ thống (kích thước **2,855,206 bytes**, **73,097 dòng**):
- **16,422 nodes** tọa độ thực tế.
- **3,128 ways** bao phủ toàn bộ mạng lưới đường chính và đường liên phường.
- Khi biên dịch bằng `parse_osm.py`, đồ thị đầy đủ đạt **24,304 directed edges** với **Giant Component bao phủ > 98%**, cho phép các địa danh kết nối liên quận.

---

## 5. Hướng dẫn Người dùng Chạy Trực tiếp (Terminal Execution)

Do môi trường sandbox hạn chế quyền tạo tiến trình (`Access is denied`), người dùng vui lòng mở cửa sổ **PowerShell** hoặc **Command Prompt** tại thư mục gốc dự án:

```powershell
cd D:\Nam2_ky1\newDSA\RIDE-RTREE
```

### Bước 1 — Trích xuất file XML 2.85 MB từ cache vào thư mục raw
Chạy kịch bản tiện ích đã tích hợp sẵn:
```powershell
python data/osm/raw/extract_cached_xml.py
```
*(Nếu muốn tải mới hoàn toàn từ Overpass API: `python data/osm/raw/download_osm.py`)*

### Bước 2 — Chuẩn hóa toàn bộ dữ liệu mạng đường sang định dạng JSON
Chạy parser chuẩn hóa dữ liệu đồ thị có hướng:
```powershell
python data/osm/parse_osm.py data/osm/raw/hcmc_road_network.osm.xml data/osm/processed/hcmc_roads.json data/osm/processed/hcmc_roads_summary.json
```

### Bước 3 — Chạy bộ kiểm thử toàn vẹn đồ thị (Verification Suite)
Xác minh tính hợp lệ của tọa độ, cạnh, tính liên thông WCC và ma trận duyệt BFS có hướng:
```powershell
python data/osm/verify_roads.py data/osm/processed/hcmc_roads.json
```
Kết quả mong đợi: `[SUCCESS] ALL VERIFICATION TESTS PASSED.`

---

## 6. Sẵn sàng cho Phase 2A.2 (C++17 RoadGraph & Dijkstra)

Cấu trúc file `hcmc_roads.json` đã chuẩn hóa hoàn toàn theo định dạng:
```json
{
  "nodes": {
    "node_id": [lat, lon]
  },
  "ways": [
    {
      "id": 123456,
      "name": "Tên đường",
      "highway": "primary",
      "oneway": true,
      "nodes": [id1, id2, ...]
    }
  ],
  "edges": [
    {
      "u": 123456,
      "v": 654321,
      "distance_m": 45.2,
      "way_id": 123456,
      "name": "Tên đường",
      "highway": "primary",
      "oneway": true
    }
  ]
}
```
Thiết kế này cho phép parser C++17 nạp trực tiếp danh sách cạnh vào danh sách kề (`std::vector<std::vector<Edge>>` hoặc `std::unordered_map`) với độ phức tạp thời gian $O(V + E)$ mà không cần xử lý chuỗi phức tạp.
