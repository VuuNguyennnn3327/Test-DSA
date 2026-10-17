/**
 * RIDE-RTREE - MapRenderer
 * High-level rendering utilities for spatial visualization, ride routes,
 * R-Tree bounding boxes (MBRs), KNN search radiuses, and state overlays.
 */

(function (window) {
  'use strict';

  const RENDERER_STYLE_ID = 'rtree-renderer-styles';

  function injectRendererStyles() {
    if (document.getElementById(RENDERER_STYLE_ID)) return;

    const style = document.createElement('style');
    style.id = RENDERER_STYLE_ID;
    style.textContent = `
      /* Map container must have relative positioning for overlays */
      .rtree-map-container-relative {
        position: relative !important;
      }

      /* Generic full-area overlay */
      .rtree-map-overlay {
        position: absolute;
        top: 0;
        left: 0;
        right: 0;
        bottom: 0;
        background: rgba(15, 23, 42, 0.72);
        backdrop-filter: blur(4px);
        -webkit-backdrop-filter: blur(4px);
        z-index: 1100;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        padding: 20px;
        text-align: center;
        transition: opacity 0.25s ease, visibility 0.25s ease;
        pointer-events: auto;
      }

      /* Loading Spinner */
      .rtree-loading-spinner {
        width: 48px;
        height: 48px;
        border: 4px solid rgba(56, 189, 248, 0.2);
        border-top-color: #38bdf8;
        border-radius: 50%;
        animation: rtree-spin 0.9s linear infinite;
        margin-bottom: 14px;
      }

      .rtree-loading-text {
        color: #f1f5f9;
        font-size: 14px;
        font-weight: 600;
        letter-spacing: 0.3px;
        max-width: 80%;
      }

      @keyframes rtree-spin {
        to {
          transform: rotate(360deg);
        }
      }

      /* Overlay Card Base */
      .rtree-overlay-card {
        background: #1e293b;
        border-radius: 12px;
        padding: 20px 24px;
        max-width: 380px;
        width: 90%;
        box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.5), 0 8px 10px -6px rgba(0, 0, 0, 0.5);
        display: flex;
        flex-direction: column;
        align-items: center;
        text-align: center;
        animation: rtree-pop-in 0.25s cubic-bezier(0.16, 1, 0.3, 1);
      }

      @keyframes rtree-pop-in {
        from {
          transform: scale(0.92);
          opacity: 0;
        }
        to {
          transform: scale(1);
          opacity: 1;
        }
      }

      /* Error card */
      .rtree-overlay-card.rtree-error-card {
        border: 1px solid rgba(239, 68, 68, 0.35);
      }
      .rtree-overlay-icon-error {
        width: 44px;
        height: 44px;
        border-radius: 50%;
        background: rgba(239, 68, 68, 0.15);
        color: #ef4444;
        display: flex;
        align-items: center;
        justify-content: center;
        margin-bottom: 12px;
      }

      /* Empty state card */
      .rtree-overlay-card.rtree-empty-card {
        border: 1px solid rgba(148, 163, 184, 0.2);
      }
      .rtree-overlay-icon-empty {
        width: 44px;
        height: 44px;
        border-radius: 50%;
        background: rgba(148, 163, 184, 0.12);
        color: #94a3b8;
        display: flex;
        align-items: center;
        justify-content: center;
        margin-bottom: 12px;
      }

      .rtree-overlay-title {
        color: #f8fafc;
        font-size: 15px;
        font-weight: 700;
        margin-bottom: 6px;
      }

      .rtree-overlay-desc {
        color: #94a3b8;
        font-size: 13px;
        line-height: 1.5;
        margin-bottom: 16px;
      }

      .rtree-overlay-btn {
        background: #334155;
        color: #f8fafc;
        border: 1px solid #475569;
        border-radius: 8px;
        padding: 6px 16px;
        font-size: 13px;
        font-weight: 600;
        cursor: pointer;
        transition: all 0.2s;
      }
      .rtree-overlay-btn:hover {
        background: #475569;
        color: #ffffff;
      }

      /* Highlight marker pulse */
      .rtree-highlight-node {
        position: relative;
        width: 28px;
        height: 28px;
        display: flex;
        align-items: center;
        justify-content: center;
      }
      .rtree-highlight-dot {
        width: 14px;
        height: 14px;
        border-radius: 50%;
        border: 2px solid #ffffff;
        box-shadow: 0 0 10px currentColor;
        z-index: 2;
      }
      .rtree-highlight-ring {
        position: absolute;
        width: 28px;
        height: 28px;
        border-radius: 50%;
        opacity: 0.6;
        animation: rtree-highlight-ping 1.6s cubic-bezier(0, 0, 0.2, 1) infinite;
        z-index: 1;
      }

      @keyframes rtree-highlight-ping {
        75%, 100% {
          transform: scale(2.2);
          opacity: 0;
        }
      }

      /* R-Tree MBR rectangle styling and tooltip */
      .rtree-mbr-tooltip {
        background: rgba(15, 23, 42, 0.95) !important;
        color: #f8fafc !important;
        border: 1px solid rgba(255, 255, 255, 0.15) !important;
        border-radius: 8px !important;
        padding: 6px 10px !important;
        font-size: 12px !important;
        font-family: inherit !important;
        box-shadow: 0 8px 16px rgba(0, 0, 0, 0.4) !important;
      }
      .rtree-mbr-tooltip::before {
        border-top-color: rgba(15, 23, 42, 0.95) !important;
      }
    `;
    document.head.appendChild(style);
  }

  // Predefined hierarchy colors for R-Tree bounding boxes
  const RTREE_LEVEL_COLORS = [
    '#10b981', // Level 0 (Leaf node): Emerald green
    '#3b82f6', // Level 1: Royal blue
    '#8b5cf6', // Level 2: Purple
    '#f59e0b', // Level 3: Amber
    '#ec4899', // Level 4: Pink
    '#06b6d4'  // Level 5+: Cyan
  ];

  class MapRenderer {
    /**
     * @param {MapController} mapController - An instance of MapController
     */
    constructor(mapController) {
      if (!mapController) {
        throw new Error('[MapRenderer] MapController instance is required.');
      }

      this.controller = mapController;

      // Ensure container has relative positioning for UI overlays
      if (this.controller.container) {
        this.controller.container.classList.add('rtree-map-container-relative');
      }

      // Layer groups for clean management
      this.straightLineLayer = null;
      this.tripRouteLayer = null;
      this.searchLayers = [];
      this.rtreeLayers = [];
      this.highlightLayers = [];

      // DOM Overlay elements
      this._loadingOverlayEl = null;
      this._errorOverlayEl = null;
      this._emptyStateOverlayEl = null;

      injectRendererStyles();
    }

    /**
     * Safely gets the underlying Leaflet map instance
     * @returns {L.Map}
     */
    get map() {
      return this.controller ? this.controller.map : null;
    }

    /**
     * Renders both pickup and destination markers and connects them with a straight dashed line.
     * Fits map bounds to show both locations clearly.
     * @param {{lat: number, lng: number, label?: string}} pickup
     * @param {{lat: number, lng: number, label?: string}} destination
     * @param {Object} [lineOptions]
     */
    renderPickupAndDestination(pickup, destination, lineOptions = {}) {
      if (!this.map || !pickup || !destination) return;

      const pLat = Number(pickup.lat || pickup.latitude);
      const pLng = Number(pickup.lng || pickup.lon || pickup.longitude);
      const dLat = Number(destination.lat || destination.latitude);
      const dLng = Number(destination.lng || destination.lon || destination.longitude);

      if (isNaN(pLat) || isNaN(pLng) || isNaN(dLat) || isNaN(dLng)) {
        console.warn('[MapRenderer] Invalid coordinates provided to renderPickupAndDestination');
        return;
      }

      // Add pickup marker
      this.controller.addPickupMarker(pLat, pLng, pickup.label || 'Điểm đón');

      // Add destination marker
      this.controller.addDestinationMarker(dLat, dLng, destination.label || 'Điểm đến');

      // Clear previous line connection if exists
      if (this.straightLineLayer && this.map.hasLayer(this.straightLineLayer)) {
        this.map.removeLayer(this.straightLineLayer);
        this.straightLineLayer = null;
      }

      // Fit map view to include both points with padding without drawing artificial diagonal lines across buildings
      this.controller.fitBounds([[pLat, pLng], [dLat, dLng]], {
        padding: [60, 60]
      });
    }

    /**
     * Renders multiple drivers on the map.
     * Clears previous driver markers and plots the provided list.
     * @param {Array<Object>} drivers - Array of driver objects [{id, lat, lng, heading, ...}]
     */
    renderDriversOnMap(drivers = []) {
      if (!this.map) return;

      // Clear existing driver markers
      this.controller.clearMarkers('drivers');

      if (!Array.isArray(drivers) || drivers.length === 0) {
        return;
      }

      drivers.forEach((driver) => {
        if (!driver) return;

        const id = driver.id || driver.driverId || driver._id;
        const lat = Number(driver.lat || driver.latitude);
        const lng = Number(driver.lng || driver.lon || driver.longitude);
        const heading = Number(driver.heading || driver.direction || 0);

        if (id !== undefined && !isNaN(lat) && !isNaN(lng)) {
          this.controller.addDriverMarker(id, lat, lng, heading, driver);
        }
      });
    }

    /**
     * Renders a trip route polyline on the map and fits view bounds.
     * @param {Array<Array<number>|Object>} routeCoordinates
     * @param {Object} [options]
     * @returns {L.Polyline}
     */
    renderTripRoute(routeCoordinates, options = {}) {
      if (!this.map || !Array.isArray(routeCoordinates) || routeCoordinates.length === 0) {
        return null;
      }

      const polyline = this.controller.drawRoute(routeCoordinates, options);

      if (polyline) {
        this.controller.fitBounds(routeCoordinates, {
          padding: [50, 50]
        });
      }

      return polyline;
    }

    /**
     * Draws a search radius circle for DSA spatial search demo (e.g. KNN or Range Search).
     * @param {Array<number>|Object} center - [lat, lng] or {lat, lng}
     * @param {number} radius - Search radius in meters
     * @param {Object} [options] - Leaflet circle styling options
     * @returns {L.Circle}
     */
    renderSearchArea(center, radius, options = {}) {
      if (!this.map || !center || typeof radius !== 'number') return null;

      const lat = Array.isArray(center) ? center[0] : (center.lat || center.latitude);
      const lng = Array.isArray(center) ? center[1] : (center.lng || center.lon || center.longitude);

      if (isNaN(lat) || isNaN(lng)) return null;

      // Clear existing search layers
      this.clearSearchArea();

      const circleOpts = Object.assign(
        {
          color: '#3b82f6',
          fillColor: '#60a5fa',
          fillOpacity: 0.16,
          weight: 2,
          dashArray: '5, 5'
        },
        options
      );

      const circle = L.circle([lat, lng], {
        radius,
        ...circleOpts
      }).addTo(this.map);

      const radiusKm = (radius / 1000).toFixed(2);
      circle.bindTooltip(
        `<b>Vùng tìm kiếm R-Tree</b><br>Bán kính: ${radius >= 1000 ? `${radiusKm} km` : `${radius} m`}`,
        {
          className: 'rtree-mbr-tooltip',
          direction: 'top'
        }
      );

      // Add a subtle center marker pulse
      const centerPulseIcon = L.divIcon({
        className: 'rtree-custom-icon',
        iconSize: [16, 16],
        iconAnchor: [8, 8],
        html: `
          <div style="position:relative;width:16px;height:16px;display:flex;align-items:center;justify-content:center;">
            <div style="width:8px;height:8px;border-radius:50%;background:#3b82f6;border:2px solid #ffffff;z-index:2;"></div>
            <div style="position:absolute;width:16px;height:16px;border-radius:50%;background:rgba(59,130,246,0.5);animation:rtree-highlight-ping 1.5s infinite;z-index:1;"></div>
          </div>
        `
      });

      const centerMarker = L.marker([lat, lng], {
        icon: centerPulseIcon,
        interactive: false
      }).addTo(this.map);

      this.searchLayers.push(circle, centerMarker);
      return circle;
    }

    /**
     * Clears all search area circles and markers
     */
    clearSearchArea() {
      if (!this.map) return;
      this.searchLayers.forEach((layer) => {
        if (this.map.hasLayer(layer)) {
          this.map.removeLayer(layer);
        }
      });
      this.searchLayers = [];
    }

    /**
     * Draws rectangles representing R-Tree Minimum Bounding Boxes (MBRs) for DSA demo.
     * Each bound can be:
     * - { minLat, minLng, maxLat, maxLng, level, id, count }
     * - [minLat, minLng, maxLat, maxLng]
     * - [[minLat, minLng], [maxLat, maxLng]]
     * @param {Array<Object|Array>} bounds
     * @param {Object} [options]
     */
    renderRTreeBounds(bounds = [], options = {}) {
      if (!this.map || !Array.isArray(bounds) || bounds.length === 0) return;

      this.clearRTreeBounds();

      const allRectBounds = [];

      bounds.forEach((b, index) => {
        let minLat, minLng, maxLat, maxLng;
        let level = 0;
        let label = `MBR Node #${index + 1}`;
        let count = null;

        if (Array.isArray(b)) {
          if (Array.isArray(b[0])) {
            minLat = Number(b[0][0]);
            minLng = Number(b[0][1]);
            maxLat = Number(b[1][0]);
            maxLng = Number(b[1][1]);
          } else {
            minLat = Number(b[0]);
            minLng = Number(b[1]);
            maxLat = Number(b[2]);
            maxLng = Number(b[3]);
          }
        } else if (typeof b === 'object' && b !== null) {
          minLat = Number(b.minLat !== undefined ? b.minLat : b.minY !== undefined ? b.minY : b.south);
          minLng = Number(b.minLng !== undefined ? b.minLng : b.minX !== undefined ? b.minX : b.west);
          maxLat = Number(b.maxLat !== undefined ? b.maxLat : b.maxY !== undefined ? b.maxY : b.north);
          maxLng = Number(b.maxLng !== undefined ? b.maxLng : b.maxX !== undefined ? b.maxX : b.east);

          if (typeof b.level === 'number') level = b.level;
          if (b.id) label = `Node ${b.id}`;
          if (b.count !== undefined) count = b.count;
          if (b.label) label = b.label;
        }

        if (isNaN(minLat) || isNaN(minLng) || isNaN(maxLat) || isNaN(maxLng)) {
          return;
        }

        const rectBounds = [[minLat, minLng], [maxLat, maxLng]];
        allRectBounds.push(rectBounds);

        // Determine level color
        const colorIndex = Math.min(level, RTREE_LEVEL_COLORS.length - 1);
        const color = options.color || RTREE_LEVEL_COLORS[colorIndex];

        const rect = L.rectangle(rectBounds, {
          color: color,
          weight: options.weight || (level === 0 ? 2 : 1.5),
          fillColor: color,
          fillOpacity: options.fillOpacity !== undefined ? options.fillOpacity : (level === 0 ? 0.08 : 0.03),
          dashArray: level === 0 ? null : '4, 4',
          interactive: true
        });

        // Detailed tooltip on hover
        const tooltipText = `
          <div style="font-weight:700;color:${color};margin-bottom:2px;">${label} (Level ${level})</div>
          <div>SW: ${minLat.toFixed(4)}, ${minLng.toFixed(4)}</div>
          <div>NE: ${maxLat.toFixed(4)}, ${maxLng.toFixed(4)}</div>
          ${count !== null ? `<div>Số lượng phần tử: <b>${count}</b></div>` : ''}
        `;

        rect.bindTooltip(tooltipText, {
          className: 'rtree-mbr-tooltip',
          sticky: true
        });

        rect.addTo(this.map);
        this.rtreeLayers.push(rect);
      });

      // Fit bounds if requested
      if (options.fitBounds && allRectBounds.length > 0) {
        const boundsGroup = L.latLngBounds(allRectBounds.flat());
        this.controller.fitBounds(boundsGroup, { padding: [40, 40] });
      }
    }

    /**
     * Clears all rendered R-Tree bounding rectangles
     */
    clearRTreeBounds() {
      if (!this.map) return;
      this.rtreeLayers.forEach((rect) => {
        if (this.map.hasLayer(rect)) {
          this.map.removeLayer(rect);
        }
      });
      this.rtreeLayers = [];
    }

    /**
     * Highlights specific spatial points on the map (e.g. KNN query results or matched drivers)
     * @param {Array<Object|Array>} points - [{lat, lng, label, distance, rank}]
     * @param {string} [color='#f59e0b'] - Highlight theme color (HEX / RGB)
     * @param {Object} [options]
     */
    highlightPoints(points = [], color = '#f59e0b', options = {}) {
      if (!this.map || !Array.isArray(points) || points.length === 0) return;

      this.clearHighlights();

      points.forEach((point, idx) => {
        let lat, lng, label, distance, rank;

        if (Array.isArray(point)) {
          lat = Number(point[0]);
          lng = Number(point[1]);
          label = `Điểm #${idx + 1}`;
        } else if (typeof point === 'object' && point !== null) {
          lat = Number(point.lat || point.latitude);
          lng = Number(point.lng || point.lon || point.longitude);
          label = point.label || point.name || `Ứng viên #${idx + 1}`;
          distance = point.distance !== undefined ? point.distance : null;
          rank = point.rank !== undefined ? point.rank : idx + 1;
        }

        if (isNaN(lat) || isNaN(lng)) return;

        const pulseIcon = L.divIcon({
          className: 'rtree-custom-icon',
          iconSize: [32, 32],
          iconAnchor: [16, 16],
          popupAnchor: [0, -16],
          html: `
            <div class="rtree-highlight-node">
              <div class="rtree-highlight-ring" style="background:${color};"></div>
              <div class="rtree-highlight-dot" style="background:${color};"></div>
            </div>
          `
        });

        const marker = L.marker([lat, lng], {
          icon: pulseIcon,
          riseOnHover: true
        });

        let popupContent = `
          <div class="rtree-popup-title" style="color: ${color};">
            <span style="display:inline-block;width:8px;height:8px;border-radius:50%;background:${color};"></span>
            ${rank ? `#${rank} - ` : ''}${label}
          </div>
          <div class="rtree-popup-sub">Vị trí: ${lat.toFixed(5)}, ${lng.toFixed(5)}</div>
        `;

        if (distance !== null && distance !== undefined) {
          const distKm = distance > 1000 ? `${(distance / 1000).toFixed(2)} km` : `${Math.round(distance)} m`;
          popupContent += `<div class="rtree-popup-sub" style="color:#f8fafc;margin-top:2px;">Khoảng cách: <b>${distKm}</b></div>`;
        }

        marker.bindPopup(popupContent, { className: 'rtree-popup' });
        marker.addTo(this.map);
        this.highlightLayers.push(marker);
      });
    }

    /**
     * Clears all highlighted points
     */
    clearHighlights() {
      if (!this.map) return;
      this.highlightLayers.forEach((marker) => {
        if (this.map.hasLayer(marker)) {
          this.map.removeLayer(marker);
        }
      });
      this.highlightLayers = [];
    }

    /**
     * Clears all visualizations, markers, routes, searches, bounding boxes and highlights
     */
    clearAll() {
      // Clear markers from controller
      this.controller.clearMarkers('all');

      // Clear route from controller
      this.controller.clearRoute();

      // Clear straight line connecting pickup & destination
      if (this.straightLineLayer && this.map && this.map.hasLayer(this.straightLineLayer)) {
        this.map.removeLayer(this.straightLineLayer);
      }
      this.straightLineLayer = null;

      // Clear spatial layers
      this.clearSearchArea();
      this.clearRTreeBounds();
      this.clearHighlights();

      // Clear UI overlays
      this.hideLoadingOverlay();
      this.hideErrorOverlay();
      this.hideEmptyState();
    }

    /**
     * Shows a loading overlay on top of the map container
     * @param {string} [message='Đang tải dữ liệu bản đồ...']
     */
    showLoadingOverlay(message = 'Đang tải dữ liệu bản đồ...') {
      this.hideLoadingOverlay();

      const container = this.controller.container;
      if (!container) return;

      const overlay = document.createElement('div');
      overlay.className = 'rtree-map-overlay';
      overlay.innerHTML = `
        <div class="rtree-loading-spinner"></div>
        <div class="rtree-loading-text">${message}</div>
      `;

      container.appendChild(overlay);
      this._loadingOverlayEl = overlay;
    }

    /**
     * Hides the loading overlay
     */
    hideLoadingOverlay() {
      if (this._loadingOverlayEl && this._loadingOverlayEl.parentNode) {
        this._loadingOverlayEl.parentNode.removeChild(this._loadingOverlayEl);
      }
      this._loadingOverlayEl = null;
    }

    /**
     * Shows an error overlay inside the map container
     * @param {string} [message='Đã xảy ra lỗi khi tải dữ liệu']
     * @param {Function} [onRetry]
     */
    showErrorOverlay(message = 'Đã xảy ra lỗi khi tải dữ liệu', onRetry = null) {
      this.hideErrorOverlay();

      const container = this.controller.container;
      if (!container) return;

      const overlay = document.createElement('div');
      overlay.className = 'rtree-map-overlay';

      overlay.innerHTML = `
        <div class="rtree-overlay-card rtree-error-card">
          <div class="rtree-overlay-icon-error">
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
              <circle cx="12" cy="12" r="10" />
              <line x1="12" y1="8" x2="12" y2="12" />
              <line x1="12" y1="16" x2="12.01" y2="16" />
            </svg>
          </div>
          <div class="rtree-overlay-title">Đã xảy ra sự cố</div>
          <div class="rtree-overlay-desc">${message}</div>
          <div style="display:flex;gap:8px;">
            ${onRetry ? '<button type="button" class="rtree-overlay-btn rtree-btn-retry" style="background:#2563eb;border-color:#3b82f6;">Thử lại</button>' : ''}
            <button type="button" class="rtree-overlay-btn rtree-btn-dismiss">Đóng</button>
          </div>
        </div>
      `;

      const dismissBtn = overlay.querySelector('.rtree-btn-dismiss');
      if (dismissBtn) {
        dismissBtn.addEventListener('click', () => this.hideErrorOverlay());
      }

      if (onRetry) {
        const retryBtn = overlay.querySelector('.rtree-btn-retry');
        if (retryBtn) {
          retryBtn.addEventListener('click', () => {
            this.hideErrorOverlay();
            onRetry();
          });
        }
      }

      container.appendChild(overlay);
      this._errorOverlayEl = overlay;
    }

    /**
     * Hides the error overlay
     */
    hideErrorOverlay() {
      if (this._errorOverlayEl && this._errorOverlayEl.parentNode) {
        this._errorOverlayEl.parentNode.removeChild(this._errorOverlayEl);
      }
      this._errorOverlayEl = null;
    }

    /**
     * Shows an empty state overlay when there is no data to display
     * @param {string} [message='Không có dữ liệu hiển thị trên bản đồ']
     */
    showEmptyState(message = 'Không có dữ liệu hiển thị trên bản đồ') {
      this.hideEmptyState();

      const container = this.controller.container;
      if (!container) return;

      const overlay = document.createElement('div');
      overlay.className = 'rtree-map-overlay';

      overlay.innerHTML = `
        <div class="rtree-overlay-card rtree-empty-card">
          <div class="rtree-overlay-icon-empty">
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <polygon points="1 6 1 22 8 18 16 22 23 18 23 2 16 6 8 2 1 6" />
              <line x1="8" y1="2" x2="8" y2="18" />
              <line x1="16" y1="6" x2="16" y2="22" />
            </svg>
          </div>
          <div class="rtree-overlay-title">Không tìm thấy dữ liệu</div>
          <div class="rtree-overlay-desc">${message}</div>
          <button type="button" class="rtree-overlay-btn rtree-btn-dismiss">Đóng</button>
        </div>
      `;

      const dismissBtn = overlay.querySelector('.rtree-btn-dismiss');
      if (dismissBtn) {
        dismissBtn.addEventListener('click', () => this.hideEmptyState());
      }

      container.appendChild(overlay);
      this._emptyStateOverlayEl = overlay;
    }

    /**
     * Hides the empty state overlay
     */
    hideEmptyState() {
      if (this._emptyStateOverlayEl && this._emptyStateOverlayEl.parentNode) {
        this._emptyStateOverlayEl.parentNode.removeChild(this._emptyStateOverlayEl);
      }
      this._emptyStateOverlayEl = null;
    }
  }

  // Export to window/global scope
  window.MapRenderer = MapRenderer;
})(typeof window !== 'undefined' ? window : this);
