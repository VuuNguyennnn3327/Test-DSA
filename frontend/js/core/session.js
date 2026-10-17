/**
 * RIDE-RTREE Session Manager
 * 
 * NOTE: This is a client-side UI session manager using sessionStorage.
 * Real JWT/OAuth authentication with backend signature verification and
 * session cookie validation will be integrated in a future phase.
 */

const SESSION_STORAGE_KEYS = {
    USER: 'ride_rtree_user',
    TOKEN: 'ride_rtree_token'
};

class SessionManager {
    constructor() {
        this.currentUser = null;
        this.token = null;
        this._initFromStorage();
    }

    /**
     * Initialize user session from sessionStorage
     * @private
     */
    _initFromStorage() {
        try {
            const rawUser = sessionStorage.getItem(SESSION_STORAGE_KEYS.USER);
            const token = sessionStorage.getItem(SESSION_STORAGE_KEYS.TOKEN);

            if (rawUser) {
                this.currentUser = JSON.parse(rawUser);
                this.token = token || null;

                // Sync token with API client if available
                if (typeof apiClient !== 'undefined' && apiClient && this.token) {
                    apiClient.setAuthToken(this.token);
                }
            } else {
                this.currentUser = null;
                this.token = null;
            }
        } catch (error) {
            console.error('[SessionManager] Error parsing session storage:', error);
            this.currentUser = null;
            this.token = null;
        }
    }

    /**
     * Get the currently logged in user profile
     * @returns {Object|null} User data object or null if not authenticated
     */
    getCurrentUser() {
        if (!this.currentUser) {
            this._initFromStorage();
        }
        return this.currentUser;
    }

    /**
     * Check if a user is currently logged in
     * @returns {boolean}
     */
    isAuthenticated() {
        return !!this.getCurrentUser();
    }

    /**
     * Get the role of the logged in user
     * @returns {'customer'|'driver'|'admin'|null}
     */
    getRole() {
        const user = this.getCurrentUser();
        return user ? (user.role || 'customer') : null;
    }

    /**
     * Get the active session token
     * @returns {string|null}
     */
    getToken() {
        return this.token || sessionStorage.getItem(SESSION_STORAGE_KEYS.TOKEN) || null;
    }

    /**
     * Log in a user and store session in sessionStorage
     * @param {Object} userData - User profile object
     * @param {string} [token] - Optional mock/real auth token
     * @returns {Object} Stored user data
     */
    login(userData, token = 'demo-jwt-token-' + Date.now()) {
        if (!userData || typeof userData !== 'object') {
            throw new Error('Invalid user data provided to login()');
        }

        this.currentUser = { ...userData };
        this.token = token;

        try {
            sessionStorage.setItem(SESSION_STORAGE_KEYS.USER, JSON.stringify(this.currentUser));
            sessionStorage.setItem(SESSION_STORAGE_KEYS.TOKEN, this.token);
        } catch (error) {
            console.error('[SessionManager] Error saving to sessionStorage:', error);
        }

        // Sync with ApiClient
        if (typeof apiClient !== 'undefined' && apiClient) {
            apiClient.setAuthToken(this.token);
        }

        // Dispatch events for reactive UI updates
        this._dispatchAuthEvents('login', this.currentUser);

        return this.currentUser;
    }

    /**
     * Update current user profile fields and persist to storage
     * @param {Object} partialData - Key-values to update
     * @returns {Object|null}
     */
    updateUser(partialData) {
        if (!this.currentUser) return null;

        this.currentUser = {
            ...this.currentUser,
            ...partialData
        };

        try {
            sessionStorage.setItem(SESSION_STORAGE_KEYS.USER, JSON.stringify(this.currentUser));
        } catch (error) {
            console.error('[SessionManager] Error updating user in sessionStorage:', error);
        }

        this._dispatchAuthEvents('update', this.currentUser);
        return this.currentUser;
    }

    /**
     * Log out current user and clear stored data
     */
    logout() {
        const previousUser = this.currentUser;
        this.currentUser = null;
        this.token = null;

        try {
            sessionStorage.removeItem(SESSION_STORAGE_KEYS.USER);
            sessionStorage.removeItem(SESSION_STORAGE_KEYS.TOKEN);
        } catch (error) {
            console.error('[SessionManager] Error clearing sessionStorage:', error);
        }

        // Clear API client token
        if (typeof apiClient !== 'undefined' && apiClient) {
            apiClient.clearAuthToken();
        }

        // Dispatch events for reactive UI updates
        this._dispatchAuthEvents('logout', null, previousUser);
    }

    /**
     * Dispatches custom events on window
     * @private
     */
    _dispatchAuthEvents(action, user, previousUser = null) {
        if (typeof window === 'undefined') return;

        const detail = {
            action,
            user,
            previousUser,
            isAuthenticated: !!user,
            role: user ? user.role : null,
            timestamp: Date.now()
        };

        window.dispatchEvent(new CustomEvent(`session:${action}`, { detail }));
        window.dispatchEvent(new CustomEvent('session:change', { detail }));
    }
}

// Singleton instance
const sessionManager = new SessionManager();

// Expose to window for multi-page scripts
if (typeof window !== 'undefined') {
    window.SESSION_STORAGE_KEYS = SESSION_STORAGE_KEYS;
    window.SessionManager = SessionManager;
    window.sessionManager = sessionManager;
}

// Module export support
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { SESSION_STORAGE_KEYS, SessionManager, sessionManager };
}
