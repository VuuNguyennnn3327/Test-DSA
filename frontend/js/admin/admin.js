/**
 * GoRide Admin Page Controller
 * Handles dashboard stats, trips management, driver approvals, and DSA demo navigation.
 * All data sourced from DemoDataAdapter.
 */

class AdminController {
    constructor() {
        this.mapController = null;
        this.mapRenderer = null;
        this.currentTab = 'dashboard';
        this.isProcessingApproval = new Set();
    }

    async init() {
        console.log('[Admin] Initializing AdminController');
        // Synchronize auth token with apiClient
        if (typeof sessionManager !== 'undefined' && sessionManager && sessionManager.getToken()) {
            if (typeof apiClient !== 'undefined' && apiClient) {
                apiClient.setAuthToken(sessionManager.getToken());
            }
        }
        this.setupTabNavigation();
        this.setupSidebarToggle();
        this.setupLogout();

        // Initialize map
        try {
            this.mapController = new MapController('admin-map', {
                zoom: 12,
                center: [10.7769, 106.7009]
            });
            this.mapController.init();
            this.mapRenderer = new MapRenderer(this.mapController);
        } catch (err) {
            console.warn('[Admin] Map initialization failed:', err.message);
        }

        // Load initial data
        await this.loadDashboard();

        // Check URL hash for tab
        const hash = window.location.hash.replace('#', '');
        if (hash && ['dashboard', 'trips', 'drivers', 'dsa'].includes(hash)) {
            this.switchTab(hash);
        }
    }

    setupTabNavigation() {
        document.querySelectorAll('[data-tab]').forEach(link => {
            link.addEventListener('click', (e) => {
                e.preventDefault();
                const tab = link.getAttribute('data-tab');
                this.switchTab(tab);
            });
        });
    }

    switchTab(tabName) {
        this.currentTab = tabName;
        window.location.hash = tabName;

        // Update nav active state
        document.querySelectorAll('[data-tab]').forEach(link => {
            link.classList.toggle('active', link.getAttribute('data-tab') === tabName);
            link.classList.toggle('nav-item-active', link.getAttribute('data-tab') === tabName);
        });

        // Show/hide tab content
        const tabs = ['dashboard', 'trips', 'drivers', 'dsa'];
        tabs.forEach(t => {
            const el = document.getElementById('tab-' + t);
            if (el) {
                el.style.display = (t === tabName) ? '' : 'none';
            }
        });

        // Load data for tab if needed
        if (tabName === 'trips') this.loadTrips();
        else if (tabName === 'drivers') this.loadDriverApprovals();

        // Resize map if switching to dashboard
        if (tabName === 'dashboard' && this.mapController) {
            this.mapController.invalidateSize();
            setTimeout(() => this.mapController.invalidateSize(), 150);
            setTimeout(() => this.mapController.invalidateSize(), 350);
        }

        // Close sidebar on mobile
        const sidebar = document.getElementById('sidebar');
        if (sidebar) sidebar.classList.remove('is-open');
    }

    setupSidebarToggle() {
        const toggle = document.getElementById('sidebar-toggle');
        const sidebar = document.getElementById('sidebar');
        if (toggle && sidebar) {
            toggle.addEventListener('click', () => {
                sidebar.classList.toggle('is-open');
            });
        }
    }

    setupLogout() {
        document.querySelectorAll('[data-action="logout"]').forEach(el => {
            el.addEventListener('click', (e) => {
                e.preventDefault();
                if (typeof sessionManager !== 'undefined') {
                    sessionManager.logout();
                }
                window.location.href = 'login.html';
            });
        });
    }

    async loadDashboard() {
        try {
            const stats = await dataAdapter.getAdminStats();
            
            // Update stat cards
            const setStatEl = (id, value) => {
                const el = document.getElementById(id);
                if (el) el.textContent = value;
            };

            setStatEl('stat-total-rides', stats.totalRides?.toLocaleString() || '0');
            setStatEl('stat-active-drivers', stats.activeDrivers?.toString() || '0');
            setStatEl('stat-revenue', formatCurrency(stats.revenue));
            setStatEl('stat-customers', stats.newCustomers?.toString() || '0');

            // Update trends
            const setTrend = (id, trend) => {
                const el = document.getElementById(id);
                if (el && trend) {
                    const isUp = trend > 0;
                    el.className = 'stat-card-trend ' + (isUp ? 'trend-up' : 'trend-down');
                    el.textContent = (isUp ? '↑' : '↓') + ' ' + Math.abs(trend) + '%';
                }
            };

            setTrend('stat-rides-trend', stats.ridesTrend);
            setTrend('stat-drivers-trend', stats.driversTrend);
            setTrend('stat-revenue-trend', stats.revenueTrend);
            setTrend('stat-customers-trend', stats.customersTrend);

            // Load recent trips for dashboard
            const trips = await dataAdapter.getAdminTrips();
            this.renderRecentTrips(trips.slice(0, 5));

            // Show demo drivers on map
            if (this.mapRenderer) {
                this.mapRenderer.renderDriversOnMap([
                    { id: 'd1', lat: 10.7769, lng: 106.7009, heading: 45 },
                    { id: 'd2', lat: 10.7820, lng: 106.6950, heading: 120 },
                    { id: 'd3', lat: 10.7700, lng: 106.7100, heading: 270 },
                    { id: 'd4', lat: 10.7850, lng: 106.7050, heading: 0 },
                    { id: 'd5', lat: 10.7730, lng: 106.6900, heading: 180 }
                ]);
            }
        } catch (err) {
            console.error('[Admin] Failed to load dashboard:', err);
            if (typeof App !== 'undefined' && App.showToast) {
                App.showToast('Không thể tải dữ liệu tổng quan', 'error');
            }
        }
    }

    renderRecentTrips(trips) {
        const tbody = document.getElementById('recent-trips-body');
        if (!tbody) return;

        if (!trips || trips.length === 0) {
            tbody.innerHTML = '<tr><td colspan="5" class="text-center">Không có dữ liệu</td></tr>';
            return;
        }

        tbody.innerHTML = trips.map(trip => {
            const statusClass = {
                'completed': 'badge-success',
                'in_progress': 'badge-primary',
                'cancelled': 'badge-danger',
                'searching': 'badge-warning'
            }[trip.status] || 'badge-info';

            const statusText = {
                'completed': 'Hoàn thành',
                'in_progress': 'Đang thực hiện',
                'cancelled': 'Đã hủy',
                'searching': 'Đang tìm'
            }[trip.status] || trip.status;

            return `<tr>
                <td>#${trip.id}</td>
                <td>${trip.customerName || '--'}</td>
                <td>${trip.driverName || '--'}</td>
                <td><span class="badge ${statusClass}">${statusText}</span></td>
                <td>${formatCurrency(trip.fare)}</td>
            </tr>`;
        }).join('');
    }

    async loadTrips() {
        const tbody = document.getElementById('all-trips-body');
        if (!tbody) return;

        tbody.innerHTML = '<tr><td colspan="7" class="text-center"><div class="spinner"></div></td></tr>';

        try {
            const trips = await dataAdapter.getAdminTrips();
            if (!trips || trips.length === 0) {
                tbody.innerHTML = '<tr><td colspan="7" class="text-center">Không có dữ liệu</td></tr>';
                return;
            }

            tbody.innerHTML = trips.map(trip => {
                const statusClass = {
                    'completed': 'badge-success',
                    'in_progress': 'badge-primary',
                    'cancelled': 'badge-danger',
                    'searching': 'badge-warning'
                }[trip.status] || 'badge-info';

                const statusText = {
                    'completed': 'Hoàn thành',
                    'in_progress': 'Đang thực hiện',
                    'cancelled': 'Đã hủy',
                    'searching': 'Đang tìm'
                }[trip.status] || trip.status;

                return `<tr>
                    <td>#${trip.id}</td>
                    <td>${trip.customerName || '--'}</td>
                    <td>${trip.driverName || '--'}</td>
                    <td>${trip.pickupName || '--'}</td>
                    <td>${trip.destinationName || '--'}</td>
                    <td><span class="badge ${statusClass}">${statusText}</span></td>
                    <td>${formatCurrency(trip.fare)}</td>
                </tr>`;
            }).join('');
        } catch (err) {
            console.error('[Admin] Failed to load trips:', err);
            tbody.innerHTML = '<tr><td colspan="7" class="text-center" style="color:var(--color-error);">Lỗi tải dữ liệu</td></tr>';
        }
    }

    async loadDriverApprovals() {
        const container = document.getElementById('driver-approvals');
        if (!container) return;

        // Loading state
        container.innerHTML = `
            <div class="text-center" style="padding:var(--space-6);">
                <div class="spinner"></div>
                <p style="margin-top:var(--space-2); color:var(--color-gray-600); font-size:0.875rem;">
                    Đang tải danh sách chờ duyệt từ máy chủ C++ (GET /api/admin/drivers/pending)...
                </p>
            </div>`;

        try {
            // Ensure token is synced
            if (typeof sessionManager !== 'undefined' && sessionManager && sessionManager.getToken()) {
                if (typeof apiClient !== 'undefined' && apiClient) {
                    apiClient.setAuthToken(sessionManager.getToken());
                }
            }

            // Real backend C++ API call
            const response = await apiClient.getPendingDrivers();
            const drivers = response?.data?.drivers || [];

            // Update badge in Drivers Tab if element exists
            const badgeEl = document.getElementById('drivers-badge-status');
            if (badgeEl) {
                badgeEl.textContent = `C++ REST API (${drivers.length} chờ duyệt)`;
                badgeEl.className = 'badge badge-success';
            }

            // Empty state
            if (!drivers || drivers.length === 0) {
                container.innerHTML = `
                    <div class="empty-state" style="padding:var(--space-6); text-align:center;">
                        <div class="empty-state-icon" style="font-size:2.5rem; margin-bottom:var(--space-2);">✅</div>
                        <p class="empty-state-title" style="font-weight:600; font-size:1.1rem;">Không có yêu cầu chờ duyệt</p>
                        <p class="empty-state-description" style="color:var(--color-gray-500); font-size:0.875rem;">
                            Toàn bộ hồ sơ tài xế đối tác đã được xét duyệt hoàn tất trên máy chủ.
                        </p>
                    </div>`;
                return;
            }

            // Vehicle label dictionary
            const vehicleLabels = {
                motorbike: '🛵 Xe máy (GoRide Bike)',
                car4: '🚗 Ô tô 4 chỗ (GoRide Car)',
                car7: '🚐 Ô tô 7 chỗ (GoRide XL)'
            };

            // Render pending driver cards
            container.innerHTML = drivers.map(driver => {
                const vType = driver.vehicle?.type || driver.vehicleType || 'car4';
                const vLabel = vehicleLabels[vType] || vType;
                const plate = driver.vehicle?.plate || driver.licensePlate || 'Chưa có biển số';
                const exp = driver.vehicle?.experience ?? driver.experience ?? 0;
                const created = driver.createdAt ? (typeof formatDateTime === 'function' ? formatDateTime(driver.createdAt) : driver.createdAt) : '';

                return `
                    <div class="card" style="margin-bottom:var(--space-3); padding:var(--space-4);" id="approval-${driver.id}">
                        <div style="display:flex; justify-content:space-between; align-items:start; flex-wrap:wrap; gap:var(--space-3);">
                            <div>
                                <div style="display:flex; align-items:center; gap:var(--space-2); margin-bottom:var(--space-1);">
                                    <h4 style="margin:0; font-size:1.05rem;">${driver.name}</h4>
                                    <span class="badge badge-warning" style="font-size:0.75rem;">Chờ duyệt</span>
                                    <span style="font-size:0.75rem; color:var(--color-gray-500); font-family:monospace;">ID: ${driver.id}</span>
                                </div>
                                <p style="color:var(--color-gray-600); font-size:0.875rem; margin:var(--space-1) 0;">
                                    ✉️ ${driver.email} &bull; 📞 ${driver.phone}
                                </p>
                                <p style="color:var(--color-gray-700); font-size:0.875rem; margin:var(--space-1) 0; font-weight:500;">
                                    ${vLabel} &bull; Biển số: <strong style="color:var(--color-primary);">${plate}</strong>
                                </p>
                                <p style="color:var(--color-gray-500); font-size:0.8rem; margin:var(--space-1) 0;">
                                    Kinh nghiệm: <strong>${exp} năm</strong> ${created ? `&bull; Đăng ký lúc: ${created}` : ''}
                                </p>
                            </div>
                            <div class="approval-actions" style="display:flex; gap:var(--space-2); align-self:center;">
                                <button class="btn btn-success btn-sm btn-approve" onclick="adminController.approveDriver('${driver.id}', true)">
                                    ✓ Duyệt hồ sơ
                                </button>
                                <button class="btn btn-danger btn-sm btn-reject" onclick="adminController.approveDriver('${driver.id}', false)">
                                    ✕ Từ chối
                                </button>
                            </div>
                        </div>
                    </div>
                `;
            }).join('');
        } catch (err) {
            console.error('[Admin] Lỗi kết nối API danh sách tài xế chờ duyệt:', err);
            container.innerHTML = `
                <div class="error-state" style="padding:var(--space-4); text-align:center; background:#fef2f2; border:1px solid #fecaca; border-radius:var(--radius-md);">
                    <p style="color:var(--color-error); font-weight:600; margin-bottom:var(--space-2);">
                        ⚠️ Không thể tải danh sách tài xế từ máy chủ C++
                    </p>
                    <p style="font-size:0.85rem; color:var(--color-gray-700); margin-bottom:var(--space-3);">
                        Chi tiết: ${err.message || 'Lỗi kết nối máy chủ'}. Hãy khởi động máy chủ backend C++ (cổng 8080) và đảm bảo đăng nhập với tài khoản Admin.
                    </p>
                    <button class="btn btn-primary btn-sm" onclick="adminController.loadDriverApprovals()">Thử lại</button>
                </div>`;
            if (typeof App !== 'undefined' && App.showToast) {
                App.showToast('Lỗi tải danh sách tài xế từ backend: ' + err.message, 'error');
            }
        }
    }

    async approveDriver(driverId, approved) {
        // Anti-duplicate click guard while request is pending
        if (this.isProcessingApproval.has(driverId)) {
            return;
        }
        this.isProcessingApproval.add(driverId);

        const card = document.getElementById('approval-' + driverId);
        const btnApprove = card?.querySelector('.btn-approve');
        const btnReject = card?.querySelector('.btn-reject');
        if (btnApprove) {
            btnApprove.disabled = true;
            if (approved) btnApprove.textContent = 'Đang duyệt...';
        }
        if (btnReject) {
            btnReject.disabled = true;
            if (!approved) btnReject.textContent = 'Đang từ chối...';
        }

        try {
            // Ensure token is synced
            if (typeof sessionManager !== 'undefined' && sessionManager && sessionManager.getToken()) {
                if (typeof apiClient !== 'undefined' && apiClient) {
                    apiClient.setAuthToken(sessionManager.getToken());
                }
            }

            let res;
            if (approved) {
                res = await apiClient.approveDriver(driverId);
            } else {
                res = await apiClient.rejectDriver(driverId);
            }

            const successMsg = res?.data?.message || (approved ? 'Phê duyệt tài khoản tài xế thành công!' : 'Đã từ chối hồ sơ tài xế.');
            if (typeof App !== 'undefined' && App.showToast) {
                App.showToast(successMsg, approved ? 'success' : 'info');
            }

            // Animate card removal
            if (card) {
                card.style.opacity = '0.4';
                card.style.transition = 'all 0.3s ease';
            }

            // Refresh from real backend to reflect updated state
            await this.loadDriverApprovals();
        } catch (err) {
            console.error('[Admin] Lỗi xét duyệt tài xế qua C++ backend:', err);
            const errMsg = err?.data?.error || err.message || 'Lỗi xử lý yêu cầu';
            if (typeof App !== 'undefined' && App.showToast) {
                App.showToast('Thao tác thất bại: ' + errMsg, 'error');
            }

            // Restore buttons on error
            if (btnApprove) {
                btnApprove.disabled = false;
                btnApprove.textContent = '✓ Duyệt hồ sơ';
            }
            if (btnReject) {
                btnReject.disabled = false;
                btnReject.textContent = '✕ Từ chối';
            }
        } finally {
            this.isProcessingApproval.delete(driverId);
        }
    }
}

// Initialize on page load
let adminController;
document.addEventListener('DOMContentLoaded', () => {
    adminController = new AdminController();
    adminController.init();
});
