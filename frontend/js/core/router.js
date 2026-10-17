/**
 * GoRide Simple Multi-page Router & Auth Guard
 * Lightweight client-side navigation utility for multi-page application.
 */

const ROUTES = {
    home: {
        name: 'home',
        file: 'index.html',
        title: 'GoRide — Đặt xe dễ dàng, đi mọi hành trình.',
        requiresAuth: false
    },
    login: {
        name: 'login',
        file: 'login.html',
        title: 'Đăng nhập - GoRide',
        requiresAuth: false,
        guestOnly: true
    },
    register: {
        name: 'register',
        file: 'register.html',
        title: 'Đăng ký tài khoản - GoRide',
        requiresAuth: false,
        guestOnly: true
    },
    customer: {
        name: 'customer',
        file: 'customer.html',
        title: 'Đặt xe | GoRide Khách hàng',
        requiresAuth: false
    },
    driver: {
        name: 'driver',
        file: 'driver.html',
        title: 'Cổng thông tin Tài xế | GoRide Driver',
        requiresAuth: true,
        roles: ['driver', 'admin']
    },
    admin: {
        name: 'admin',
        file: 'admin.html',
        title: 'Bảng Quản trị Hệ thống | GoRide Admin',
        requiresAuth: true,
        roles: ['admin']
    },
    'dsa-demo': {
        name: 'dsa-demo',
        file: 'dsa-demo.html',
        title: 'DSA Algorithm Demo Shell | GoRide',
        requiresAuth: false
    }
};

class Router {
    constructor(routes = ROUTES) {
        this.routes = routes;
    }

    /**
     * Check if currently executing inside the /pages/ subdirectory
     * @returns {boolean}
     */
    isInPagesDir() {
        if (typeof window === 'undefined') return false;
        const pathname = window.location.pathname.replace(/\\/g, '/');
        return pathname.includes('/pages/') || pathname.endsWith('/pages');
    }

    /**
     * Resolve correct relative path to target page regardless of current location
     * @param {string} pageKey - e.g. 'home', 'login', 'customer'
     * @returns {string} Relative URL e.g. 'pages/login.html' or 'login.html' or '../index.html'
     */
    resolvePath(pageKey) {
        const inPages = this.isInPagesDir();
        const route = this.routes[pageKey];
        const filename = route ? route.file : `${pageKey}.html`;

        if (pageKey === 'home') {
            return inPages ? '../index.html' : 'index.html';
        }

        return inPages ? filename : `pages/${filename}`;
    }

    /**
     * Determines current page name from window.location.pathname
     * @returns {string} Route key (e.g. 'home', 'login', 'customer', etc.)
     */
    getCurrentPage() {
        return this.getPageFromUrl(window.location.pathname);
    }

    /**
     * Parse route key from any path or URL string
     * @param {string} path 
     * @returns {string} Route name
     */
    getPageFromUrl(path) {
        if (!path) return 'home';

        const filename = path.split('/').pop().split('?')[0].split('#')[0].toLowerCase();

        if (!filename || filename === '' || filename === 'index.html') {
            return 'home';
        }

        for (const [key, route] of Object.entries(this.routes)) {
            if (route.file.toLowerCase() === filename) {
                return key;
            }
        }

        const baseName = filename.replace('.html', '');
        if (this.routes[baseName]) {
            return baseName;
        }

        return 'home';
    }

    /**
     * Get route definition for given page key or filename
     * @param {string} pageKey 
     * @returns {Object|null}
     */
    getRoute(pageKey) {
        return this.routes[pageKey] || null;
    }

    /**
     * Navigate to a designated page with query parameters
     * @param {string} pageKey - Key in ROUTES or direct name
     * @param {Object} [params] - Optional query parameters
     */
    navigate(pageKey, params = {}) {
        const targetPath = this.resolvePath(pageKey);

        const searchParams = new URLSearchParams();
        Object.entries(params).forEach(([key, value]) => {
            if (value !== undefined && value !== null) {
                searchParams.append(key, value);
            }
        });

        const qs = searchParams.toString();
        const fullUrl = qs ? `${targetPath}?${qs}` : targetPath;

        window.location.href = fullUrl;
    }

    /**
     * Get appropriate default dashboard URL for a user role
     * @param {string} role 
     * @returns {string}
     */
    getDefaultDashboardForRole(role) {
        switch (role) {
            case 'driver':
                return this.resolvePath('driver');
            case 'admin':
                return this.resolvePath('admin');
            case 'customer':
            default:
                return this.resolvePath('customer');
        }
    }

    /**
     * Check authentication and authorization guards for current page.
     * Redirects automatically if access is forbidden.
     * @returns {boolean} True if allowed to remain on current page
     */
    checkAuthGuard() {
        if (typeof window === 'undefined') return true;

        const currentPageKey = this.getCurrentPage();
        const currentRoute = this.getRoute(currentPageKey);

        if (!currentRoute) return true;

        // Set document title if defined
        if (currentRoute.title && typeof document !== 'undefined') {
            document.title = currentRoute.title;
        }

        // Session check (client-side demo session)
        const isAuth = typeof sessionManager !== 'undefined' ? sessionManager.isAuthenticated() : false;
        const currentRole = isAuth && typeof sessionManager !== 'undefined' ? sessionManager.getRole() : null;

        // 1. If page requires authentication and user is NOT logged in
        if (currentRoute.requiresAuth && !isAuth) {
            console.warn(`[Router Guard] Access denied to '${currentPageKey}'. Authentication required.`);
            const targetRedirect = currentPageKey;
            this.navigate('login', { redirect: targetRedirect });
            return false;
        }

        // 2. If page is guest only (e.g. login/register) and user is ALREADY logged in
        // Allow staying on login page if explicitly switching account (?switch=true)
        const urlParams = new URLSearchParams(window.location.search);
        const isSwitchingAccount = urlParams.get('switch') === 'true';

        if (currentRoute.guestOnly && isAuth && !isSwitchingAccount) {
            console.info(`[Router Guard] User already authenticated as ${currentRole}. Redirecting to dashboard.`);
            const defaultDash = this.getDefaultDashboardForRole(currentRole);
            window.location.href = defaultDash;
            return false;
        }

        // 3. Role authorization check
        if (currentRoute.requiresAuth && currentRoute.roles && currentRoute.roles.length > 0) {
            if (!currentRoute.roles.includes(currentRole)) {
                console.warn(`[Router Guard] Role '${currentRole}' is not authorized for '${currentPageKey}'`);
                const defaultDash = this.getDefaultDashboardForRole(currentRole);
                window.location.href = defaultDash;
                return false;
            }
        }

        return true;
    }
}

// Singleton instance
const router = new Router(ROUTES);

// Expose to window for global access
if (typeof window !== 'undefined') {
    window.ROUTES = ROUTES;
    window.Router = Router;
    window.router = router;
}

// Module export support
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { ROUTES, Router, router };
}
