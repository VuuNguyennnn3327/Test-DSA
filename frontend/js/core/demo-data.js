/**
 * GoRide Demo Data Adapter
 * 
 * [DEMO DATA] All data in this module is static demonstration data.
 * It will be replaced by ApiDataAdapter connected to the C++ backend
 * in a future phase.
 * 
 * This adapter mirrors the interface that ApiDataAdapter will implement,
 * making the swap transparent to UI code.
 */

class DemoDataAdapter {
    constructor() {
        this._initDemoState();
    }

    /**
     * Simulate network delay
     * @private
     */
    async _delay(min = 200, max = 500) {
        const ms = Math.floor(Math.random() * (max - min + 1)) + min;
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    /**
     * Initialize mutable in-memory mock datasets
     * @private
     */
    _initDemoState() {
        this.driverOnlineState = true;

        this.mockUsers = {
            customer: {
                id: 'usr_c101',
                name: 'Nguyễn Văn An',
                email: 'customer@demo.com',
                phone: '0901234567',
                role: 'customer',
                avatar: 'https://images.unsplash.com/photo-1535713875002-d1d0cf377fde?auto=format&fit=crop&w=120&q=80',
                rating: 4.9,
                walletBalance: 350000,
                createdAt: '2026-01-15T08:30:00Z'
            },
            driver: {
                id: 'drv_d202',
                name: 'Trần Văn Bình',
                email: 'driver@demo.com',
                phone: '0918765432',
                role: 'driver',
                avatar: 'https://images.unsplash.com/photo-1570295999919-56ceb5ecca61?auto=format&fit=crop&w=120&q=80',
                vehicle: {
                    type: 'car4',
                    typeName: 'Ô tô 4 chỗ',
                    plate: '51G-889.24',
                    model: 'Toyota Vios 2023',
                    color: 'Bạc Kim Loại'
                },
                rating: 4.88,
                totalTrips: 428,
                status: 'online',
                walletBalance: 1450000,
                createdAt: '2025-11-20T10:15:00Z'
            },
            admin: {
                id: 'adm_admin',
                name: 'admin',
                email: 'admin',
                phone: '0988888888',
                role: 'admin',
                avatar: 'https://images.unsplash.com/photo-1472099645785-5658abf4ff4e?auto=format&fit=crop&w=120&q=80',
                walletBalance: 0,
                createdAt: '2025-01-01T00:00:00Z'
            }
        };

        this.customerTrips = [
            {
                id: 'trip_vn_101',
                status: 'completed',
                pickup: {
                    name: 'Chợ Bến Thành',
                    address: 'Đường Lê Lợi, Phường Bến Thành, Quận 1, TP.HCM',
                    lat: 10.7725,
                    lng: 106.6980
                },
                destination: {
                    name: 'Landmark 81, Vinhomes Central Park',
                    address: '720A Điện Biên Phủ, Phường 22, Bình Thạnh, TP.HCM',
                    lat: 10.7951,
                    lng: 106.7219
                },
                distance: 5800,
                duration: 1140,
                fare: 78000,
                vehicleType: 'car4',
                vehicleName: 'Ô tô 4 chỗ',
                driver: {
                    name: 'Trần Văn Bình',
                    phone: '0918765432',
                    plate: '51G-889.24',
                    model: 'Toyota Vios 2023',
                    rating: 4.9
                },
                createdAt: '2026-10-15T14:20:00Z',
                completedAt: '2026-10-15T14:42:00Z',
                ratingGiven: 5
            },
            {
                id: 'trip_vn_102',
                status: 'completed',
                pickup: {
                    name: 'Đại học Bách Khoa TP.HCM',
                    address: '268 Lý Thường Kiệt, Phường 14, Quận 10, TP.HCM',
                    lat: 10.7721,
                    lng: 106.6578
                },
                destination: {
                    name: 'Sân bay Quốc tế Tân Sơn Nhất',
                    address: 'Đường Trường Sơn, Phường 2, Quận Tân Bình, TP.HCM',
                    lat: 10.8185,
                    lng: 106.6588
                },
                distance: 6200,
                duration: 1320,
                fare: 85000,
                vehicleType: 'car4',
                vehicleName: 'Ô tô 4 chỗ',
                driver: {
                    name: 'Nguyễn Văn Minh',
                    phone: '0903112233',
                    plate: '51H-234.56',
                    model: 'Hyundai Accent 2022',
                    rating: 4.8
                },
                createdAt: '2026-10-14T09:10:00Z',
                completedAt: '2026-10-14T09:35:00Z',
                ratingGiven: 5
            },
            {
                id: 'trip_vn_103',
                status: 'completed',
                pickup: {
                    name: 'Nhà Thờ Đức Bà Sài Gòn',
                    address: '01 Công xã Paris, Phường Bến Nghé, Quận 1, TP.HCM',
                    lat: 10.7798,
                    lng: 106.6990
                },
                destination: {
                    name: 'Thảo Cầm Viên Sài Gòn',
                    address: '02 Nguyễn Bỉnh Khiêm, Phường Bến Nghé, Quận 1, TP.HCM',
                    lat: 10.7876,
                    lng: 106.7051
                },
                distance: 1800,
                duration: 420,
                fare: 25000,
                vehicleType: 'bike',
                vehicleName: 'Xe máy',
                driver: {
                    name: 'Phạm Hùng Cường',
                    phone: '0977665544',
                    plate: '59P1-987.65',
                    model: 'Honda Air Blade',
                    rating: 5.0
                },
                createdAt: '2026-10-13T18:05:00Z',
                completedAt: '2026-10-13T18:14:00Z',
                ratingGiven: 5
            },
            {
                id: 'trip_vn_104',
                status: 'cancelled',
                pickup: {
                    name: 'Phố đi bộ Nguyễn Huệ',
                    address: 'Đường Nguyễn Huệ, Phường Bến Nghé, Quận 1, TP.HCM',
                    lat: 10.7743,
                    lng: 106.7036
                },
                destination: {
                    name: 'Hồ Con Rùa',
                    address: 'Công trường Quốc tế, Phường 6, Quận 3, TP.HCM',
                    lat: 10.7826,
                    lng: 106.6961
                },
                distance: 2100,
                duration: 540,
                fare: 32000,
                vehicleType: 'bike',
                vehicleName: 'Xe máy',
                driver: null,
                cancelReason: 'Khách hàng thay đổi lịch trình cá nhân',
                createdAt: '2026-10-12T20:15:00Z',
                completedAt: null,
                ratingGiven: null
            }
        ];

        this.driverActiveTrip = {
            id: 'trip_act_901',
            status: 'in_progress',
            phase: 'heading_to_pickup', // 'heading_to_pickup' | 'arrived_at_pickup' | 'on_trip' | 'completed'
            passenger: {
                name: 'Hoàng Minh Anh',
                phone: '0943998877',
                rating: 4.95,
                avatar: 'https://images.unsplash.com/photo-1544005313-94ddf0286df2?auto=format&fit=crop&w=120&q=80'
            },
            pickup: {
                name: 'Khách sạn Caravelle Sài Gòn',
                address: '19-23 Công Trường Lam Sơn, Bến Nghé, Quận 1',
                lat: 10.7765,
                lng: 106.7031
            },
            destination: {
                name: 'Bitexco Financial Tower',
                address: '02 Hải Triều, Bến Nghé, Quận 1',
                lat: 10.7716,
                lng: 106.7044
            },
            distance: 1400,
            duration: 360,
            fare: 45000,
            driverEarnings: 36000,
            vehicleType: 'car4',
            notes: 'Đứng chờ ở sảnh chính khách sạn Caravelle',
            createdAt: '2026-10-16T11:00:00Z'
        };

        this.pendingRequests = [
            {
                requestId: 'req_live_01',
                passenger: {
                    name: 'Lê Thị Thu',
                    phone: '0934567890',
                    rating: 4.95
                },
                pickup: {
                    name: 'Chợ Bến Thành',
                    address: 'Cửa Nam, Đường Lê Lợi, Bến Thành, Quận 1',
                    lat: 10.7725,
                    lng: 106.6980
                },
                destination: {
                    name: 'Sân bay Quốc tế Tân Sơn Nhất - Ga Quốc Nội',
                    address: 'Đường Trường Sơn, Phường 2, Tân Bình',
                    lat: 10.8185,
                    lng: 106.6588
                },
                distanceToPickup: 450,
                tripDistance: 6800,
                estimatedFare: 92000,
                driverEarnings: 73600,
                vehicleType: 'car4',
                expiresInSeconds: 25,
                rtreeRank: 'Hạng 1 (Gần nhất theo chỉ mục R-Tree)',
                createdAt: new Date().toISOString()
            },
            {
                requestId: 'req_live_02',
                passenger: {
                    name: 'Vũ Đức Nam',
                    phone: '0912345678',
                    rating: 4.8
                },
                pickup: {
                    name: 'Nhà Thờ Đức Bà Sài Gòn',
                    address: '01 Công xã Paris, Bến Nghé, Quận 1',
                    lat: 10.7798,
                    lng: 106.6990
                },
                destination: {
                    name: 'Landmark 81, Vinhomes Central Park',
                    address: '720A Điện Biên Phủ, Phường 22, Bình Thạnh',
                    lat: 10.7951,
                    lng: 106.7219
                },
                distanceToPickup: 850,
                tripDistance: 4900,
                estimatedFare: 72000,
                driverEarnings: 57600,
                vehicleType: 'car4',
                expiresInSeconds: 30,
                rtreeRank: 'Hạng 2 (Bán kính MBR mở rộng)',
                createdAt: new Date().toISOString()
            }
        ];

        this.driverApprovals = [
            {
                driverId: 'drv_pend_01',
                name: 'Đặng Quốc Huy',
                phone: '0945112233',
                email: 'quochuy.dang@gmail.com',
                vehicle: {
                    type: 'car4',
                    typeName: 'Ô tô 4 chỗ',
                    model: 'Kia Morning 2022',
                    plate: '51K-556.78',
                    color: 'Đỏ Ruby'
                },
                licenseNumber: 'B2-7901824792',
                registeredAt: '2026-10-16T08:15:00Z',
                status: 'pending',
                documents: ['cccd_mat_truoc.jpg', 'cccd_mat_sau.jpg', 'bang_lai_b2.jpg', 'dang_kiem_xe.jpg']
            },
            {
                driverId: 'drv_pend_02',
                name: 'Võ Minh Tâm',
                phone: '0978990011',
                email: 'minhtam.vo@gmail.com',
                vehicle: {
                    type: 'bike',
                    typeName: 'Xe máy',
                    model: 'Honda Wave Alpha 110',
                    plate: '59T2-112.33',
                    color: 'Xanh Đen'
                },
                licenseNumber: 'A1-5201948271',
                registeredAt: '2026-10-16T10:40:00Z',
                status: 'pending',
                documents: ['cccd_mat_truoc.jpg', 'cccd_mat_sau.jpg', 'bang_lai_a1.jpg', 'ca_vet_xe.jpg']
            }
        ];
    }

    /**
     * Authenticate user with email and password
     */
    async login(email, password) {
        console.log('[Adapter] login requested', { email });
        await this._delay(250, 450);

        if (!email || !password) {
            throw new Error('Vui lòng nhập đầy đủ email và mật khẩu.');
        }

        if (password.length < 6) {
            throw new Error('Mật khẩu phải có tối thiểu 6 ký tự.');
        }

        const emailClean = email.toLowerCase().trim();

        // Exact match verification against registered mock users with required password
        const accounts = [
            { email: 'customer@demo.com', password: 'password123', user: this.mockUsers.customer },
            { email: 'driver@demo.com', password: 'password123', user: this.mockUsers.driver },
            { email: 'admin@demo.com', password: 'password123', user: this.mockUsers.admin }
        ];

        const match = accounts.find(a => a.email === emailClean);
        if (!match) {
            throw new Error('Tài khoản không tồn tại trên hệ thống. Vui lòng kết nối máy chủ C++ để truy vấn cơ sở dữ liệu SQLite.');
        }

        if (match.password !== password) {
            throw new Error('Mật khẩu không chính xác.');
        }

        const token = `jwt-${match.user.role}-${Date.now()}`;
        return {
            success: true,
            user: { ...match.user },
            token
        };
    }

    /**
     * Register a new user
     */
    async register(userData) {
        console.log('[DEMO] register called', userData);
        await this._delay(300, 500);

        const role = userData.role || 'customer';
        const newUser = {
            id: `usr_${Date.now()}`,
            name: userData.name || 'Người dùng mới',
            email: userData.email,
            phone: userData.phone || '0900000000',
            role: role,
            avatar: 'https://images.unsplash.com/photo-1535713875002-d1d0cf377fde?auto=format&fit=crop&w=120&q=80',
            rating: 5.0,
            walletBalance: 100000,
            vehicle: role === 'driver' ? (userData.vehicle || {
                type: 'car4',
                typeName: 'Ô tô 4 chỗ',
                plate: userData.plate || '51H-999.99',
                model: userData.model || 'Hyundai Grand i10',
                color: 'Trắng'
            }) : null,
            createdAt: new Date().toISOString()
        };

        const token = `demo-jwt-${role}-${Date.now()}`;
        return {
            success: true,
            message: 'Đăng ký tài khoản thành công',
            user: newUser,
            token
        };
    }

    /**
     * Get trip history for customer
     */
    async getCustomerTrips(customerId) {
        console.log('[DEMO] getCustomerTrips called', { customerId });
        await this._delay(200, 400);
        return [...this.customerTrips];
    }

    /**
     * Calculate route estimates and pricing tiers
     */
    async getRideEstimate(pickup, destination) {
        console.log('[DEMO] getRideEstimate called', { pickup, destination });
        await this._delay(250, 450);

        // Approximate distance calculation using HCMC coords (or fallback to ~5.4 km)
        let distanceMeters = 5400;
        if (pickup?.lat && pickup?.lng && destination?.lat && destination?.lng) {
            const dLat = (destination.lat - pickup.lat) * 111000;
            const dLng = (destination.lng - pickup.lng) * 111000 * Math.cos(pickup.lat * Math.PI / 180);
            const euclideanDist = Math.sqrt(dLat * dLat + dLng * dLng);
            distanceMeters = Math.max(800, Math.round(euclideanDist * 1.35)); // 1.35 road factor
        }

        const durationSeconds = Math.round((distanceMeters / 1000) * 190); // ~19 km/h urban speed
        const km = distanceMeters / 1000;

        const tiers = [
            {
                id: 'bike',
                name: 'R-Bike',
                displayName: 'Xe máy',
                capacity: '1 khách',
                etaMinutes: 2,
                fare: Math.round((14000 + km * 5500) / 1000) * 1000,
                description: 'Di chuyển nhanh, luồn lách qua ngõ ngách TP.HCM',
                icon: 'bike'
            },
            {
                id: 'car4',
                name: 'R-Car 4 Chỗ',
                displayName: 'Ô tô 4 chỗ',
                capacity: '4 khách',
                etaMinutes: 4,
                fare: Math.round((26000 + km * 11500) / 1000) * 1000,
                description: 'Tiện lợi, êm ái, máy lạnh mát mẻ',
                icon: 'car'
            },
            {
                id: 'car7',
                name: 'R-Car 7 Chỗ',
                displayName: 'Ô tô 7 chỗ',
                capacity: '7 khách',
                etaMinutes: 6,
                fare: Math.round((33000 + km * 14500) / 1000) * 1000,
                description: 'Rộng rãi cho gia đình và nhóm bạn',
                icon: 'truck'
            },
            {
                id: 'premium',
                name: 'R-Luxury',
                displayName: 'Xe sang Cao cấp',
                capacity: '4 khách',
                etaMinutes: 7,
                fare: Math.round((48000 + km * 21000) / 1000) * 1000,
                description: 'Dòng xe cao cấp Mercedes/Camry, tài xế vest lịch thiệp',
                icon: 'award'
            }
        ];

        return {
            distanceMeters,
            durationSeconds,
            distanceText: `${(distanceMeters / 1000).toFixed(1)} km`,
            durationText: `${Math.round(durationSeconds / 60)} phút`,
            pickup: pickup || { name: 'Chợ Bến Thành, Quận 1', lat: 10.7725, lng: 106.6980 },
            destination: destination || { name: 'Landmark 81, Bình Thạnh', lat: 10.7951, lng: 106.7219 },
            spatialSearchStats: {
                algorithm: 'R-Tree k-NN Spatial Query',
                activeDriversEvaluated: 48,
                nearestCandidateDistanceMeters: 380,
                queryExecutionTimeMs: 1.2
            },
            options: tiers
        };
    }

    /**
     * Request a ride
     */
    async requestRide(rideData) {
        console.log('[DEMO] requestRide called', rideData);
        await this._delay(400, 600);

        const tripId = `trip_${Date.now()}`;
        const newTrip = {
            id: tripId,
            status: 'driver_assigned',
            vehicleType: rideData.vehicleType || 'car4',
            pickup: rideData.pickup || {
                name: 'Chợ Bến Thành, Lê Lợi, Quận 1',
                lat: 10.7725,
                lng: 106.6980
            },
            destination: rideData.destination || {
                name: 'Landmark 81, Bình Thạnh',
                lat: 10.7951,
                lng: 106.7219
            },
            fare: rideData.fare || 78000,
            distance: rideData.distance || 5800,
            duration: rideData.duration || 1140,
            driver: {
                id: 'drv_d202',
                name: 'Trần Văn Bình',
                phone: '0918765432',
                rating: 4.88,
                plate: '51G-889.24',
                model: 'Toyota Vios 2023',
                avatar: 'https://images.unsplash.com/photo-1570295999919-56ceb5ecca61?auto=format&fit=crop&w=120&q=80',
                currentLocation: {
                    lat: 10.7745,
                    lng: 106.6995
                },
                etaMinutes: 3
            },
            createdAt: new Date().toISOString()
        };

        this.customerTrips.unshift(newTrip);

        return {
            success: true,
            message: 'Đã tìm thấy tài xế thông qua thuật toán R-Tree k-NN',
            trip: newTrip
        };
    }

    /**
     * Get driver real-time status & statistics
     */
    async getDriverStatus(driverId) {
        console.log('[DEMO] getDriverStatus called', { driverId });
        await this._delay(200, 400);

        return {
            driverId: driverId || 'drv_d202',
            isOnline: this.driverOnlineState,
            currentLocation: {
                lat: 10.7769,
                lng: 106.7009,
                area: 'Quận 1, TP. Hồ Chí Minh'
            },
            todayEarnings: 745000,
            completedTripsToday: 10,
            onlineHoursToday: 6.8,
            rating: 4.88,
            acceptanceRate: 97,
            completionRate: 99,
            currentTripId: this.driverActiveTrip ? this.driverActiveTrip.id : null,
            rtreeStatus: {
                indexed: this.driverOnlineState,
                mbrCell: 'HCM_D1_CELL_B2',
                lastIndexedAt: new Date().toISOString()
            }
        };
    }

    /**
     * Get currently active trip for driver
     */
    async getDriverCurrentTrip(driverId) {
        console.log('[DEMO] getDriverCurrentTrip called', { driverId });
        await this._delay(200, 350);
        return this.driverActiveTrip;
    }

    /**
     * Get incoming ride dispatch requests for driver
     */
    async getDriverRequests(driverId) {
        console.log('[DEMO] getDriverRequests called', { driverId });
        await this._delay(200, 350);

        if (!this.driverOnlineState) {
            return [];
        }

        return [...this.pendingRequests];
    }

    /**
     * Toggle driver online/offline availability
     */
    async updateDriverOnline(isOnline) {
        console.log('[DEMO] updateDriverOnline called', { isOnline });
        await this._delay(200, 400);
        this.driverOnlineState = !!isOnline;

        return {
            success: true,
            isOnline: this.driverOnlineState,
            message: this.driverOnlineState
                ? 'Bạn đã bật trạng thái trực tuyến. Hệ thống R-Tree đã cập nhật tọa độ đón khách.'
                : 'Bạn đã tắt trạng thái trực tuyến. Tạm dừng nhận cuốc xe mới.'
        };
    }

    /**
     * Respond to an incoming ride dispatch request
     */
    async respondToRequest(requestId, accept) {
        console.log('[DEMO] respondToRequest called', { requestId, accept });
        await this._delay(250, 450);

        this.pendingRequests = this.pendingRequests.filter(r => r.requestId !== requestId);

        if (accept) {
            this.driverActiveTrip = {
                id: `trip_${Date.now()}`,
                status: 'in_progress',
                phase: 'heading_to_pickup',
                passenger: {
                    name: 'Lê Thị Thu',
                    phone: '0934567890',
                    rating: 4.95
                },
                pickup: {
                    name: 'Chợ Bến Thành, Quận 1',
                    address: 'Cửa Nam, Đường Lê Lợi, Bến Thành, Quận 1',
                    lat: 10.7725,
                    lng: 106.6980
                },
                destination: {
                    name: 'Sân bay Quốc tế Tân Sơn Nhất',
                    address: 'Đường Trường Sơn, Phường 2, Tân Bình',
                    lat: 10.8185,
                    lng: 106.6588
                },
                distance: 6800,
                duration: 1380,
                fare: 92000,
                driverEarnings: 73600,
                vehicleType: 'car4',
                createdAt: new Date().toISOString()
            };

            return {
                success: true,
                status: 'accepted',
                message: 'Đã nhận cuốc xe thành công! Vui lòng di chuyển đến điểm đón.',
                trip: this.driverActiveTrip
            };
        }

        return {
            success: true,
            status: 'rejected',
            message: 'Đã từ chối cuốc xe. Hệ thống R-Tree sẽ tự động điều phối tài xế kế tiếp.',
            tripId: null
        };
    }

    /**
     * Update trip phase as driver moves through journey
     * @param {string} tripId
     * @param {'heading_to_pickup'|'arrived_at_pickup'|'on_trip'|'completed'} phase
     */
    async updateTripPhase(tripId, phase) {
        console.log('[DEMO] updateTripPhase called', { tripId, phase });
        await this._delay(200, 400);

        if (this.driverActiveTrip) {
            this.driverActiveTrip.phase = phase;

            if (phase === 'completed') {
                this.driverActiveTrip.status = 'completed';
                this.driverActiveTrip.completedAt = new Date().toISOString();
                const finishedTrip = { ...this.driverActiveTrip };
                this.customerTrips.unshift(finishedTrip);
                this.driverActiveTrip = null;
                return finishedTrip;
            }

            return { ...this.driverActiveTrip };
        }

        return {
            id: tripId,
            phase,
            status: phase === 'completed' ? 'completed' : 'in_progress',
            updatedAt: new Date().toISOString()
        };
    }

    /**
     * Get system-wide platform & R-Tree metrics for admin dashboard
     */
    async getAdminStats() {
        console.log('[DEMO] getAdminStats called');
        await this._delay(250, 450);

        return {
            totalRidesToday: 1428,
            activeDriversOnline: 215,
            totalRevenueToday: 104680000, // VND
            systemCommissionToday: 20936000, // 20%
            averageWaitTimeMinutes: 3.4,
            completionRatePercent: 97.8,
            spatialIndex: {
                algorithm: 'R*-Tree Multi-Dimensional Index',
                treeDepth: 4,
                totalLeafNodes: 284,
                totalIndexedDrivers: 215,
                maxMbrCapacity: 16,
                minMbrCapacity: 8,
                averageKnnLatencyMs: 1.38,
                reinsertionRatio: '30%',
                boundingBoxHCMC: {
                    minLat: 10.6500,
                    maxLat: 10.9000,
                    minLng: 106.5500,
                    maxLng: 106.8500
                }
            },
            trafficStatus: 'Ổn định - Giờ cao điểm chiều tối',
            serverStatus: 'C++ Spatial Service Online (0.8% CPU, 48MB RAM)'
        };
    }

    /**
     * Get list of recent trips for admin inspection
     */
    async getAdminTrips(filters = {}) {
        console.log('[DEMO] getAdminTrips called', filters);
        await this._delay(250, 450);

        const trips = [
            {
                id: 'TRP-88219',
                customerName: 'Nguyễn Văn An',
                customerPhone: '0901234567',
                driverName: 'Trần Văn Bình',
                driverPlate: '51G-889.24',
                pickup: 'Chợ Bến Thành, Quận 1',
                destination: 'Landmark 81, Bình Thạnh',
                distance: 5800,
                fare: 78000,
                status: 'completed',
                spatialLookupMs: 1.15,
                createdAt: '2026-10-16T10:30:00Z'
            },
            {
                id: 'TRP-88220',
                customerName: 'Lê Thị Thu',
                customerPhone: '0934567890',
                driverName: 'Nguyễn Văn Minh',
                driverPlate: '51H-234.56',
                pickup: 'Đại học Bách Khoa TP.HCM, Quận 10',
                destination: 'Sân bay Tân Sơn Nhất, Tân Bình',
                distance: 6200,
                fare: 85000,
                status: 'completed',
                spatialLookupMs: 1.42,
                createdAt: '2026-10-16T10:45:00Z'
            },
            {
                id: 'TRP-88221',
                customerName: 'Hoàng Minh Anh',
                customerPhone: '0943998877',
                driverName: 'Trần Văn Bình',
                driverPlate: '51G-889.24',
                pickup: 'Caravelle Sài Gòn, Quận 1',
                destination: 'Bitexco Financial Tower, Quận 1',
                distance: 1400,
                fare: 45000,
                status: 'in_progress',
                spatialLookupMs: 0.98,
                createdAt: '2026-10-16T11:00:00Z'
            },
            {
                id: 'TRP-88222',
                customerName: 'Trần Hải Đăng',
                customerPhone: '0919223344',
                driverName: 'Phạm Hùng Cường',
                driverPlate: '59P1-987.65',
                pickup: 'Nhà Thờ Đức Bà Sài Gòn, Quận 1',
                destination: 'Thảo Cầm Viên, Quận 1',
                distance: 1800,
                fare: 25000,
                status: 'completed',
                spatialLookupMs: 1.05,
                createdAt: '2026-10-16T11:15:00Z'
            },
            {
                id: 'TRP-88223',
                customerName: 'Đỗ Mỹ Linh',
                customerPhone: '0981122334',
                driverName: 'Đang tìm kiếm...',
                driverPlate: '---',
                pickup: 'Crescent Mall, Quận 7',
                destination: 'Phố đi bộ Nguyễn Huệ, Quận 1',
                distance: 7200,
                fare: 98000,
                status: 'matching',
                spatialLookupMs: 1.55,
                createdAt: '2026-10-16T11:22:00Z'
            },
            {
                id: 'TRP-88224',
                customerName: 'Bùi Gia Huy',
                customerPhone: '0966778899',
                driverName: null,
                driverPlate: null,
                pickup: 'Aeon Mall Bình Tân',
                destination: 'Bến xe Miền Tây, Bình Tân',
                distance: 2600,
                fare: 35000,
                status: 'cancelled',
                spatialLookupMs: 1.12,
                createdAt: '2026-10-16T11:25:00Z'
            }
        ];

        if (filters.status && filters.status !== 'all') {
            return trips.filter(t => t.status === filters.status);
        }

        if (filters.search) {
            const query = filters.search.toLowerCase();
            return trips.filter(t => 
                t.customerName.toLowerCase().includes(query) ||
                (t.driverName && t.driverName.toLowerCase().includes(query)) ||
                t.id.toLowerCase().includes(query) ||
                t.pickup.toLowerCase().includes(query) ||
                t.destination.toLowerCase().includes(query)
            );
        }

        return trips;
    }

    /**
     * Get pending driver registrations awaiting administrator approval
     */
    async getAdminDriverApprovals() {
        console.log('[DEMO] getAdminDriverApprovals called');
        await this._delay(200, 400);
        return [...this.driverApprovals];
    }

    /**
     * Approve or reject a driver registration
     */
    async approveDriver(driverId, approved) {
        console.log('[DEMO] approveDriver called', { driverId, approved });
        await this._delay(250, 450);

        const targetDriver = this.driverApprovals.find(d => d.driverId === driverId);
        if (targetDriver) {
            targetDriver.status = approved ? 'approved' : 'rejected';
        }

        return {
            success: true,
            driverId,
            status: approved ? 'approved' : 'rejected',
            message: approved 
                ? 'Hồ sơ tài xế đã được phê duyệt thành công.' 
                : 'Đã từ chối hồ sơ đăng ký của tài xế.'
        };
    }
}

// Singleton instance
const dataAdapter = new DemoDataAdapter();

// Expose to window for global access
if (typeof window !== 'undefined') {
    window.DemoDataAdapter = DemoDataAdapter;
    window.dataAdapter = dataAdapter;
}

// Module export support
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { DemoDataAdapter, dataAdapter };
}
