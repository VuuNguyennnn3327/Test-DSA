/**
 * RIDE-RTREE API Client - Phase 2A.3
 * Integrated with C++17 RoadGraph & Dijkstra REST API Server
 */

const getDefaultBaseUrl = () => {
    if (typeof window !== 'undefined' && window.GORIDE_API_URL) {
        return window.GORIDE_API_URL;
    }
    // If running from file:// or standard local static server (5500, 8000, 3000, etc.),
    // target the C++ backend on port 8080
    if (typeof window !== 'undefined' && window.location) {
        if (window.location.protocol === 'file:' || (window.location.port && window.location.port !== '8080')) {
            return 'http://localhost:8080/api';
        }
    }
    return '/api';
};

const API_CONFIG = {
    baseUrl: getDefaultBaseUrl(),
    timeout: 15000,
    headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json'
    }
};

class ApiError extends Error {
    constructor(message, status, data) {
        super(message);
        this.name = 'ApiError';
        this.status = status;
        this.data = data;
    }
}

class NetworkError extends Error {
    constructor(message) {
        super(message);
        this.name = 'NetworkError';
    }
}

class ApiClient {
    constructor(config = {}) {
        this.baseUrl = config.baseUrl || API_CONFIG.baseUrl;
        this.timeout = config.timeout || API_CONFIG.timeout;
        this.defaultHeaders = { ...API_CONFIG.headers, ...config.headers };
        this.authToken = null;
        this.onAuthError = null; // callback when 401 received
    }

    setAuthToken(token) {
        this.authToken = token;
    }

    clearAuthToken() {
        this.authToken = null;
    }

    setOnAuthError(callback) {
        this.onAuthError = callback;
    }

    _buildHeaders() {
        const headers = { ...this.defaultHeaders };
        if (this.authToken) {
            headers['Authorization'] = `Bearer ${this.authToken}`;
        }
        return headers;
    }

    async _request(method, endpoint, { body, params, headers, timeout } = {}) {
        let url = `${this.baseUrl}${endpoint}`;
        
        if (params) {
            const searchParams = new URLSearchParams();
            Object.entries(params).forEach(([key, value]) => {
                if (value !== undefined && value !== null) {
                    searchParams.append(key, value);
                }
            });
            const qs = searchParams.toString();
            if (qs) url += `?${qs}`;
        }

        const options = {
            method,
            headers: { ...this._buildHeaders(), ...headers },
        };

        if (body && method !== 'GET') {
            options.body = JSON.stringify(body);
        }

        const reqTimeout = timeout || this.timeout;
        const controller = new AbortController();
        options.signal = controller.signal;
        const timeoutId = setTimeout(() => controller.abort(), reqTimeout);

        try {
            const response = await fetch(url, options);
            clearTimeout(timeoutId);

            if (response.status === 401 && this.onAuthError) {
                this.onAuthError();
            }

            const contentType = response.headers.get('content-type');
            let data = null;
            if (contentType && contentType.includes('application/json')) {
                data = await response.json();
            } else {
                data = await response.text();
            }

            if (!response.ok) {
                throw new ApiError(
                    data?.error || data?.message || `HTTP ${response.status}`,
                    response.status,
                    data
                );
            }

            return {
                ok: true,
                status: response.status,
                data: data
            };
        } catch (error) {
            clearTimeout(timeoutId);
            if (error instanceof ApiError) throw error;
            if (error.name === 'AbortError') {
                throw new NetworkError('Request timed out');
            }
            throw new NetworkError(error.message || 'Network error');
        }
    }

    get(endpoint, options) { return this._request('GET', endpoint, options); }
    post(endpoint, body, options) { return this._request('POST', endpoint, { ...options, body }); }
    put(endpoint, body, options) { return this._request('PUT', endpoint, { ...options, body }); }
    patch(endpoint, body, options) { return this._request('PATCH', endpoint, { ...options, body }); }
    delete(endpoint, options) { return this._request('DELETE', endpoint, options); }

    /**
     * Compute shortest path between GPS coordinates via C++ Dijkstra backend
     */
    async getRoute(fromLat, fromLon, toLat, toLon) {
        return this.get('/route', {
            params: {
                from_lat: fromLat,
                from_lon: fromLon,
                to_lat: toLat,
                to_lon: toLon
            }
        });
    }

    /**
     * Check backend health and status
     */
    async checkHealth() {
        return this.get('/health', { timeout: 3000 });
    }

    /**
     * Authenticate with backend
     */
    async login(email, password) {
        return this.post('/auth/login', { email, password });
    }

    /**
     * Register new user with backend
     */
    async register(userData) {
        return this.post('/auth/register', userData);
    }

    /**
     * Get current user profile using active token
     */
    async getProfile() {
        return this.get('/auth/me');
    }

    /**
     * Revoke active token on backend
     */
    async logout() {
        return this.post('/auth/logout', {});
    }

    /**
     * Get list of pending driver registrations awaiting approval (Admin only)
     */
    async getPendingDrivers() {
        return this.get('/admin/drivers/pending');
    }

    /**
     * Approve driver (Admin only)
     */
    async approveDriver(driverId) {
        return this.post('/auth/approve-driver', { driverId, action: 'approve', approved: true });
    }

    /**
     * Reject driver registration (Admin only)
     */
    async rejectDriver(driverId) {
        return this.post('/auth/approve-driver', { driverId, action: 'reject', approved: false });
    }
}

// Singleton instance
const apiClient = new ApiClient();

// Expose to window for vanilla multi-page scripts
if (typeof window !== 'undefined') {
    window.API_CONFIG = API_CONFIG;
    window.ApiError = ApiError;
    window.NetworkError = NetworkError;
    window.ApiClient = ApiClient;
    window.apiClient = apiClient;
}

// Module export support if loaded in modular context
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { API_CONFIG, ApiError, NetworkError, ApiClient, apiClient };
}
