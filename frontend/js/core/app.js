/**
 * RIDE-RTREE Main Application Initializer & Global Utilities
 * Coordinates UI state, auth guards, notifications, modals, and formatters.
 */

/* ==========================================================================
   FORMATTING UTILITIES
   ========================================================================== */

/**
 * Format currency amount into Vietnamese Dong (VND)
 * @param {number} amount - Value in VND
 * @param {string} [currency='VND'] - Currency code
 * @returns {string} Formatted string, e.g. "50.000 ₫"
 */
function formatCurrency(amount, currency = 'VND') {
    if (amount === undefined || amount === null || isNaN(amount)) {
        return '0 ₫';
    }

    try {
        return new Intl.NumberFormat('vi-VN', {
            style: 'currency',
            currency: currency,
            maximumFractionDigits: 0
        }).format(amount);
    } catch {
        return `${Number(amount).toLocaleString('vi-VN')} ₫`;
    }
}

/**
 * Format distance in meters into human readable meter or kilometer string
 * @param {number} meters - Distance in meters
 * @returns {string} e.g. "750 m" or "3.4 km"
 */
function formatDistance(meters) {
    if (meters === undefined || meters === null || isNaN(meters)) {
        return '0 m';
    }

    const dist = Number(meters);
    if (dist < 1000) {
        return `${Math.round(dist)} m`;
    }
    return `${(dist / 1000).toFixed(1)} km`;
}

/**
 * Format duration in seconds into human-readable duration
 * @param {number} seconds - Duration in seconds
 * @returns {string} e.g. "45 giây", "12 phút", "1 giờ 15 phút"
 */
function formatDuration(seconds) {
    if (seconds === undefined || seconds === null || isNaN(seconds)) {
        return '0 phút';
    }

    const s = Math.round(Number(seconds));
    if (s < 60) {
        return `${s} giây`;
    }

    const minutes = Math.round(s / 60);
    if (minutes < 60) {
        return `${minutes} phút`;
    }

    const hours = Math.floor(minutes / 60);
    const remainingMins = minutes % 60;
    if (remainingMins === 0) {
        return `${hours} giờ`;
    }
    return `${hours} giờ ${remainingMins} phút`;
}

/**
 * Format date-time into Vietnamese standard format
 * @param {string|number|Date} dateInput 
 * @returns {string} e.g. "14:30 16/10/2026"
 */
function formatDateTime(dateInput) {
    if (!dateInput) return '';
    const date = new Date(dateInput);
    if (isNaN(date.getTime())) return '';

    return new Intl.DateTimeFormat('vi-VN', {
        hour: '2-digit',
        minute: '2-digit',
        day: '2-digit',
        month: '2-digit',
        year: 'numeric'
    }).format(date);
}

/* ==========================================================================
   TOAST NOTIFICATION SYSTEM
   ========================================================================== */

/**
 * Display a toast alert notification
 * @param {string} message - Text or description to display
 * @param {'info'|'success'|'warning'|'error'} [type='info'] - Alert severity level
 * @param {number} [duration=3500] - Duration in ms before auto-dismiss
 */
function showToast(message, type = 'info', duration = 3500) {
    let container = document.getElementById('toast-container');
    if (!container) {
        container = document.createElement('div');
        container.id = 'toast-container';
        container.className = 'toast-container';
        container.setAttribute('aria-live', 'polite');
        container.setAttribute('role', 'region');
        container.setAttribute('aria-label', 'Thông báo hệ thống');
        document.body.appendChild(container);
    }

    const icons = {
        success: `<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"></path><polyline points="22 4 12 14.01 9 11.01"></polyline></svg>`,
        error: `<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><line x1="15" y1="9" x2="9" y2="15"></line><line x1="9" y1="9" x2="15" y2="15"></line></svg>`,
        warning: `<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m21.73 18-8-14a2 2 0 0 0-3.48 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3Z"></path><line x1="12" y1="9" x2="12" y2="13"></line><line x1="12" y1="17" x2="12.01" y2="17"></line></svg>`,
        info: `<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg>`
    };

    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    toast.setAttribute('role', 'alert');

    toast.innerHTML = `
        <div class="toast-icon">${icons[type] || icons.info}</div>
        <div class="toast-content">${message}</div>
        <button type="button" class="toast-close" aria-label="Đóng">&times;</button>
    `;

    container.appendChild(toast);

    // Fade-in animation trigger
    requestAnimationFrame(() => {
        toast.classList.add('toast-show');
    });

    let dismissTimer = null;

    const dismissToast = () => {
        if (dismissTimer) clearTimeout(dismissTimer);
        toast.classList.remove('toast-show');
        toast.classList.add('toast-hide');
        setTimeout(() => {
            if (toast.parentNode) {
                toast.parentNode.removeChild(toast);
            }
        }, 300);
    };

    const closeBtn = toast.querySelector('.toast-close');
    if (closeBtn) {
        closeBtn.addEventListener('click', dismissToast);
    }

    if (duration > 0) {
        dismissTimer = setTimeout(dismissToast, duration);
    }

    return dismissToast;
}

/* ==========================================================================
   MODAL HELPERS
   ========================================================================== */

/**
 * Open a modal dialogue by ID or Element
 * @param {string|HTMLElement} modalTarget 
 */
function openModal(modalTarget) {
    const modal = typeof modalTarget === 'string' ? document.getElementById(modalTarget) : modalTarget;
    if (!modal) {
        console.warn(`[App] Modal not found:`, modalTarget);
        return;
    }

    modal.classList.add('modal-active');
    modal.setAttribute('aria-hidden', 'false');
    document.body.classList.add('modal-open');

    // Accessibility focus on first interactive element or modal itself
    const focusable = modal.querySelector('button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])');
    if (focusable) {
        focusable.focus();
    }
}

/**
 * Close a modal dialogue
 * @param {string|HTMLElement} modalTarget 
 */
function closeModal(modalTarget) {
    const modal = typeof modalTarget === 'string' ? document.getElementById(modalTarget) : modalTarget;
    if (!modal) return;

    modal.classList.remove('modal-active');
    modal.setAttribute('aria-hidden', 'true');

    // Check if any other modal is still active
    if (!document.querySelector('.modal-active')) {
        document.body.classList.remove('modal-open');
    }
}

/* ==========================================================================
   LOADING OVERLAY HELPER
   ========================================================================== */

/**
 * Display full-page or scoped loading overlay
 * @param {string} [message='Đang tải...'] 
 */
function showLoading(message = 'Đang tải...') {
    let overlay = document.getElementById('app-loading-overlay');
    if (!overlay) {
        overlay = document.createElement('div');
        overlay.id = 'app-loading-overlay';
        overlay.className = 'loading-overlay';
        overlay.innerHTML = `
            <div class="loading-spinner-wrapper">
                <div class="loading-spinner"></div>
                <div class="loading-text" id="loading-overlay-text"></div>
            </div>
        `;
        document.body.appendChild(overlay);
    }

    const textEl = document.getElementById('loading-overlay-text');
    if (textEl) textEl.textContent = message;

    overlay.classList.add('loading-active');
}

/**
 * Hide loading overlay
 */
function hideLoading() {
    const overlay = document.getElementById('app-loading-overlay');
    if (overlay) {
        overlay.classList.remove('loading-active');
    }
}

/* ==========================================================================
   APPLICATION COORDINATOR CLASS
   ========================================================================== */

class App {
    constructor() {
        this.initialized = false;
    }

    /**
     * Initialize application state, check router auth guards,
     * setup global listeners and sync navigation UI with session state.
     */
    init() {
        if (this.initialized) return;
        this.initialized = true;

        console.log('[App] Initializing GoRide Core Application...');

        // 1. Run navigation & role guards
        if (typeof router !== 'undefined' && router.checkAuthGuard) {
            const isAllowed = router.checkAuthGuard();
            if (!isAllowed) {
                // If redirected, stop further setup on this unauthorized page view
                return;
            }
        }

        // 2. Setup global UI event listeners (modals, auth actions)
        this._setupEventListeners();

        // 3. Update DOM UI matching session state
        this.updateAuthUI();

        // 4. Setup global uncaught error listeners
        this._setupErrorHandlers();

        console.log('[App] GoRide Ready.');
    }

    /**
     * Synchronize DOM elements with current session state
     */
    updateAuthUI() {
        if (typeof sessionManager === 'undefined') return;

        const isAuth = sessionManager.isAuthenticated();
        const user = sessionManager.getCurrentUser();
        const role = sessionManager.getRole();

        // Guest only elements (e.g. login/register buttons in header)
        document.querySelectorAll('[data-auth="guest"]').forEach(el => {
            el.style.display = isAuth ? 'none' : '';
        });

        // Authenticated user only elements (e.g. user menu, dashboard links)
        document.querySelectorAll('[data-auth="user"]').forEach(el => {
            el.style.display = isAuth ? '' : 'none';
        });

        // Role-filtered elements (e.g. data-auth-role="admin")
        document.querySelectorAll('[data-auth-role]').forEach(el => {
            const allowedRoles = el.getAttribute('data-auth-role').split(',').map(r => r.trim());
            el.style.display = (isAuth && allowedRoles.includes(role)) ? '' : 'none';
        });

        // Fill dynamic user profile placeholders
        if (user) {
            document.querySelectorAll('[data-user-name]').forEach(el => {
                el.textContent = user.name || 'Người dùng';
            });
            document.querySelectorAll('[data-user-email]').forEach(el => {
                el.textContent = user.email || '';
            });
            document.querySelectorAll('[data-user-role]').forEach(el => {
                const roleLabels = { customer: 'Khách hàng', driver: 'Tài xế', admin: 'Quản trị viên' };
                el.textContent = roleLabels[user.role] || user.role;
            });
            document.querySelectorAll('[data-user-avatar]').forEach(el => {
                if (user.avatar) el.setAttribute('src', user.avatar);
            });
            document.querySelectorAll('[data-user-balance]').forEach(el => {
                el.textContent = formatCurrency(user.walletBalance || 0);
            });
        }
    }

    /**
     * Bind universal event handlers across pages
     * @private
     */
    _setupEventListeners() {
        // Global click handler for data-actions
        document.addEventListener('click', (event) => {
            // Logout action buttons
            const logoutTrigger = event.target.closest('[data-action="logout"]');
            if (logoutTrigger) {
                event.preventDefault();
                this.handleLogout();
                return;
            }

            // Modal open triggers: data-modal-open="my-modal-id"
            const modalOpenTrigger = event.target.closest('[data-modal-open]');
            if (modalOpenTrigger) {
                event.preventDefault();
                const targetModalId = modalOpenTrigger.getAttribute('data-modal-open');
                openModal(targetModalId);
                return;
            }

            // Modal close triggers: data-modal-close or [data-dismiss="modal"]
            const modalCloseTrigger = event.target.closest('[data-modal-close], [data-dismiss="modal"]');
            if (modalCloseTrigger) {
                event.preventDefault();
                const modal = modalCloseTrigger.closest('.modal, .modal-backdrop');
                if (modal) {
                    closeModal(modal);
                }
                return;
            }

            // Backdrop click to close active modal
            if (event.target.classList && event.target.classList.contains('modal-backdrop')) {
                closeModal(event.target);
            }
        });

        // ESC key listener to close active modal
        document.addEventListener('keydown', (event) => {
            if (event.key === 'Escape' || event.key === 'Esc') {
                const activeModal = document.querySelector('.modal-active');
                if (activeModal) {
                    closeModal(activeModal);
                }
            }
        });

        // Reactive session change events
        window.addEventListener('session:login', () => {
            this.updateAuthUI();
        });

        window.addEventListener('session:logout', () => {
            this.updateAuthUI();
        });

        window.addEventListener('session:change', () => {
            this.updateAuthUI();
        });
    }

    /**
     * Centralized logout flow
     */
    handleLogout() {
        if (typeof apiClient !== 'undefined') {
            apiClient.logout().catch(() => {});
        }
        if (typeof sessionManager !== 'undefined') {
            sessionManager.logout();
        }
        showToast('Đã đăng xuất thành công', 'info', 2000);
        setTimeout(() => {
            if (typeof router !== 'undefined') {
                router.navigate('login');
            } else {
                window.location.href = 'login.html';
            }
        }, 300);
    }

    /**
     * Global uncaught error handler
     * @private
     */
    _setupErrorHandlers() {
        window.addEventListener('error', (event) => {
            console.error('[App Uncaught Error]:', event.error || event.message);
        });

        window.addEventListener('unhandledrejection', (event) => {
            console.error('[App Unhandled Promise Rejection]:', event.reason);
        });
    }
}

// Singleton app instance
const app = new App();

// Auto initialize on DOM ready
if (typeof document !== 'undefined') {
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', () => app.init());
    } else {
        // DOM already loaded
        app.init();
    }
}

// Expose utilities & app globally for plain script tags
if (typeof window !== 'undefined') {
    window.App = App;
    window.app = app;
    window.formatCurrency = formatCurrency;
    window.formatDistance = formatDistance;
    window.formatDuration = formatDuration;
    window.formatDateTime = formatDateTime;
    window.showToast = showToast;
    window.openModal = openModal;
    window.closeModal = closeModal;
    window.showLoading = showLoading;
    window.hideLoading = hideLoading;
}

// Module export support
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        App,
        app,
        formatCurrency,
        formatDistance,
        formatDuration,
        formatDateTime,
        showToast,
        openModal,
        closeModal,
        showLoading,
        hideLoading
    };
}
