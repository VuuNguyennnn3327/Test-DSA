/**
 * RIDE-RTREE - MapController
 * Core map controller wrapping Leaflet for ride-hailing interactions.
 * Reusable across Customer, Driver, and Admin portals.
 */

(function (window) {
  'use strict';

  // Inject CSS styles for custom markers and animations once
  const STYLE_ID = 'rtree-map-styles';
  function injectMapStyles() {
    if (document.getElementById(STYLE_ID)) return;

    const style = document.createElement('style');
    style.id = STYLE_ID;
    style.textContent = `
      /* Common marker base */
      .rtree-marker-container {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        pointer-events: auto;
      }

      /* Pin icon styling */
      .rtree-pin {
        position: relative;
        width: 34px;
        height: 42px;
        filter: drop-shadow(0 4px 6px rgba(0, 0, 0, 0.35));
        transition: transform 0.2s ease;
      }
      .rtree-pin:hover {
        transform: scale(1.1) translateY(-2px);
      }

      .rtree-pin-body {
        position: absolute;
        top: 0;
        left: 0;
        width: 34px;
        height: 34px;
        border-radius: 50% 50% 50% 0;
        transform: rotate(-45deg);
        display: flex;
        align-items: center;
        justify-content: center;
        box-shadow: inset 0 2px 4px rgba(255, 255, 255, 0.3);
      }

      .rtree-pin-body svg {
        transform: rotate(45deg);
      }

      /* Green Pickup Pin */
      .rtree-pin-pickup .rtree-pin-body {
        background: linear-gradient(135deg, #10b981 0%, #059669 100%);
        border: 2px solid #ffffff;
      }

      /* Red Destination Pin */
      .rtree-pin-destination .rtree-pin-body {
        background: linear-gradient(135deg, #ef4444 0%, #b91c1c 100%);
        border: 2px solid #ffffff;
      }

      /* Blue Temporary Selection Pin */
      .rtree-pin-temp .rtree-pin-body {
        background: linear-gradient(135deg, #3b82f6 0%, #1d4ed8 100%);
        border: 2px solid #ffffff;
      }

      /* Pulsing ring for pins */
      .rtree-pulse {
        position: absolute;
        bottom: 2px;
        left: 50%;
        transform: translateX(-50%);
        width: 14px;
        height: 6px;
        background: rgba(0, 0, 0, 0.3);
        border-radius: 50%;
        animation: rtree-pulse-anim 1.8s infinite;
      }

      @keyframes rtree-pulse-anim {
        0% {
          transform: translateX(-50%) scale(0.8);
          opacity: 0.8;
        }
        70% {
          transform: translateX(-50%) scale(2.2);
          opacity: 0;
        }
        100% {
          transform: translateX(-50%) scale(0.8);
          opacity: 0;
        }
      }

      /* Modern Top-Down Vehicle Marker */
      .rtree-driver-marker {
        position: relative;
        width: 30px;
        height: 50px;
        display: flex;
        align-items: center;
        justify-content: center;
        transition: transform 0.28s cubic-bezier(0.2, 0.8, 0.4, 1);
        pointer-events: auto;
        cursor: pointer;
      }

      .rtree-car-body {
        transition: transform 0.2s ease, filter 0.2s ease;
        filter: drop-shadow(0 3px 6px rgba(0, 0, 0, 0.35));
        display: flex;
        align-items: center;
        justify-content: center;
      }

      .rtree-driver-marker:hover .rtree-car-body {
        transform: scale(1.15);
        filter: drop-shadow(0 6px 12px rgba(0, 0, 0, 0.5));
      }

      /* Active trip driver glowing radar halo */
      .rtree-active-driver-halo {
        position: absolute;
        width: 48px;
        height: 48px;
        border-radius: 50%;
        background: rgba(16, 185, 129, 0.2);
        border: 2px solid rgba(16, 185, 129, 0.65);
        animation: rtree-active-driver-pulse 1.8s cubic-bezier(0, 0, 0.2, 1) infinite;
        pointer-events: none;
        z-index: -1;
      }

      @keyframes rtree-active-driver-pulse {
        0% {
          transform: scale(0.6);
          opacity: 0.95;
        }
        70%, 100% {
          transform: scale(1.75);
          opacity: 0;
        }
      }

      /* Popup custom styling */
      .rtree-popup .leaflet-popup-content-wrapper {
        background: #0f172a;
        color: #f8fafc;
        border-radius: 10px;
        padding: 4px;
        box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.5);
        border: 1px solid rgba(255, 255, 255, 0.1);
      }
      .rtree-popup .leaflet-popup-tip {
        background: #0f172a;
      }
      .rtree-popup-title {
        font-weight: 700;
        font-size: 13px;
        margin-bottom: 2px;
        display: flex;
        align-items: center;
        gap: 6px;
      }
      .rtree-popup-sub {
        font-size: 11px;
        color: #94a3b8;
      }

      /* Map Tile Error Fallback Banner */
      .rtree-tile-error-banner {
        position: absolute;
        top: 12px;
        left: 50%;
        transform: translateX(-50%);
        background: #fee2e2;
        border: 1px solid #f87171;
        color: #991b1b;
        padding: 8px 16px;
        border-radius: 8px;
        font-size: 12px;
        font-weight: 600;
        box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
        z-index: 1000;
        display: flex;
        align-items: center;
        gap: 8px;
        max-width: 90%;
        pointer-events: auto;
      }
      .rtree-tile-error-banner button {
        background: none;
        border: none;
        cursor: pointer;
        color: #991b1b;
        font-size: 14px;
        font-weight: bold;
        padding: 0 4px;
      }

      /* Basemap Provider Switcher Pill (Top-Right) */
      .rtree-basemap-control {
        background: rgba(15, 23, 42, 0.88);
        backdrop-filter: blur(8px);
        -webkit-backdrop-filter: blur(8px);
        border: 1px solid rgba(255, 255, 255, 0.15);
        border-radius: 20px;
        padding: 3px;
        display: flex;
        gap: 3px;
        box-shadow: 0 4px 14px rgba(0, 0, 0, 0.3);
        z-index: 1000;
        margin-top: 10px !important;
        margin-right: 10px !important;
      }
      .rtree-basemap-btn {
        background: transparent;
        color: #94a3b8;
        border: none;
        border-radius: 14px;
        padding: 4px 10px;
        font-size: 11px;
        font-weight: 600;
        cursor: pointer;
        transition: all 0.2s ease;
        white-space: nowrap;
      }
      .rtree-basemap-btn:hover {
        color: #ffffff;
        background: rgba(255, 255, 255, 0.1);
      }
      .rtree-basemap-btn.active {
        background: #10b981;
        color: #ffffff;
        box-shadow: 0 2px 6px rgba(16, 185, 129, 0.4);
      }
    `;
    document.head.appendChild(style);
  }

  // Default coordinate: Ho Chi Minh City Center
  const DEFAULT_CENTER = [10.7769, 106.7009];
  const DEFAULT_ZOOM = 13;

  // Preset tile providers catalog for Vietnam and international basemaps
  const TILE_PROVIDERS = {
    // 1. Google Maps Vietnam: Official Google Maps tile server with Vietnam localization (hl=vi&gl=VN)
    // Ensures full compliance with Vietnamese sovereignty and geographical representation.
    google: {
      name: 'Bản đồ Google Việt Nam',
      url: 'https://{s}.google.com/vt/lyrs=m&hl=vi&gl=VN&x={x}&y={y}&z={z}',
      attribution: '&copy; Bản đồ Google Việt Nam',
      maxZoom: 20,
      subdomains: ['mt0', 'mt1', 'mt2', 'mt3']
    },
    // Aliasing 'osm' to Google Maps Vietnam to prevent accidental disputed boundary tiles
    osm: {
      name: 'Bản đồ Google Việt Nam',
      url: 'https://{s}.google.com/vt/lyrs=m&hl=vi&gl=VN&x={x}&y={y}&z={z}',
      attribution: '&copy; Bản đồ Google Việt Nam',
      maxZoom: 20,
      subdomains: ['mt0', 'mt1', 'mt2', 'mt3']
    },
    // 2. VietMap API: Vietnamese national provider (requires ?apikey=...)
    vietmap: {
      name: 'VietMap Maps API',
      url: 'https://maps.vietmap.vn/api/maps/raster/{z}/{x}/{y}.png?apikey={key}',
      attribution: '&copy; <a href="https://vietmap.vn" target="_blank" rel="noopener">VietMap</a>',
      maxZoom: 19,
      subdomains: []
    },
    // 3. Map4D Platform: Vietnamese 2D/3D map engine (requires ?key=...)
    map4d: {
      name: 'Map4D Platform',
      url: 'https://api.map4d.vn/sdk/tile/{z}/{x}/{y}?key={key}',
      attribution: '&copy; <a href="https://map4d.vn" target="_blank" rel="noopener">Map4D</a>',
      maxZoom: 20,
      subdomains: []
    },
    // 4. Esri World Street Map: Global GIS provider
    esri: {
      name: 'Esri World Street Map',
      url: 'https://server.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer/tile/{z}/{y}/{x}',
      attribution: 'Tiles &copy; <a href="https://www.esri.com" target="_blank" rel="noopener">Esri</a> &mdash; Sources: Esri, DeLorme, NAVTEQ, TomTom, USGS',
      maxZoom: 19,
      subdomains: []
    }
  };

  class MapController {
    /**
     * @param {string|HTMLElement} container - DOM element ID or HTMLElement
     * @param {Object} [options] - Map initialization options
     */
    constructor(container, options = {}) {
      if (!container) {
        throw new Error('[MapController] Container element or ID is required.');
      }

      this.containerId = typeof container === 'string' ? container : container.id;
      this.container = typeof container === 'string' ? document.getElementById(container) : container;

      if (!this.container) {
        throw new Error(`[MapController] Container element "#${this.containerId}" not found in DOM.`);
      }

      // Resolve tile provider configuration (Default: 'google' for compliant Vietnam basemap)
      let storedProvider = typeof localStorage !== 'undefined' ? localStorage.getItem('goride_map_provider') : null;
      if (storedProvider === 'osm') {
        storedProvider = 'google';
        if (typeof localStorage !== 'undefined') {
          localStorage.setItem('goride_map_provider', 'google');
        }
      }
      const activeProviderKey = options.provider || storedProvider || (typeof window !== 'undefined' && window.GORIDE_MAP_PROVIDER) || 'google';
      const providerConfig = TILE_PROVIDERS[activeProviderKey] || TILE_PROVIDERS.google;

      let resolvedTileUrl = options.tileUrl || providerConfig.url;
      const apiKey = options.apiKey || (typeof localStorage !== 'undefined' && localStorage.getItem('goride_map_apikey')) || (typeof window !== 'undefined' && window.GORIDE_MAP_APIKEY);
      if (apiKey) {
        resolvedTileUrl = resolvedTileUrl.replace('{key}', apiKey).replace('{apikey}', apiKey);
      }

      this.options = Object.assign(
        {
          center: DEFAULT_CENTER,
          zoom: DEFAULT_ZOOM,
          minZoom: 5,
          maxZoom: providerConfig.maxZoom || 20,
          coordinatesElementId: null,
          zoomControl: true,
          attributionControl: true,
          provider: activeProviderKey,

          tileUrl: resolvedTileUrl,
          tileAttribution: options.tileAttribution || providerConfig.attribution,
          fallbackTileUrl: 'https://{s}.google.com/vt/lyrs=m&hl=vi&gl=VN&x={x}&y={y}&z={z}',
          
          subdomains: options.subdomains || providerConfig.subdomains || ['mt0', 'mt1', 'mt2', 'mt3'],
          keepBuffer: 3,
          updateWhenIdle: true,
          updateWhenZooming: false,
          crossOrigin: false
        },
        options
      );

      // Core leaflet instances
      this.map = null;
      this.tileLayer = null;

      // Categorized marker collections
      this.markers = {
        pickup: null,
        destination: null,
        temp: null,
        drivers: new Map() // Map<driverId, L.Marker>
      };

      // Polyline routes
      this.routePolyline = null;

      // State flags
      this.isClickToSelectActive = false;
      this._clickToSelectHandler = null;
      this._userSelectCallback = null;

      // Custom event listeners map
      this._eventListeners = new Map();

      // Observers & handlers for cleanup
      this._resizeObserver = null;
      this._windowResizeHandler = null;
      this._tileErrorBanner = null;
      this._tileErrorCount = 0;
      this._tileLoadCount = 0;
      this._providerControlElement = null;

      // Ensure CSS rules are available
      injectMapStyles();
    }

    /**
     * Initializes the Leaflet map, adds base OSM tiles, sets up error handlers and observers.
     * @returns {MapController}
     */
    init() {
      if (typeof window.L === 'undefined') {
        throw new Error('[MapController] Leaflet library (L) is not loaded. Please include leaflet.js CDN.');
      }

      if (this.map) {
        console.warn('[MapController] Map is already initialized.');
        return this;
      }

      // Initialize Leaflet map
      this.map = L.map(this.container, {
        center: this.options.center,
        zoom: this.options.zoom,
        minZoom: this.options.minZoom,
        maxZoom: this.options.maxZoom,
        zoomControl: this.options.zoomControl,
        attributionControl: this.options.attributionControl
      });

      // Add OpenStreetMap base tile layer with subdomains and buffer optimization
      this.tileLayer = L.tileLayer(this.options.tileUrl, {
        maxZoom: this.options.maxZoom,
        attribution: this.options.tileAttribution,
        subdomains: this.options.subdomains,
        keepBuffer: this.options.keepBuffer,
        updateWhenIdle: this.options.updateWhenIdle,
        updateWhenZooming: this.options.updateWhenZooming,
        crossOrigin: this.options.crossOrigin
      });

      // Handle tile load failures and retries gracefully
      this._setupTileErrorHandling();

      this.tileLayer.addTo(this.map);

      // Add dynamic basemap provider switcher control in top-right
      this._createProviderControl();

      // Listen to map moves to update coordinate display if configured
      this.map.on('mousemove', (e) => {
        this.showCoordinates(e.latlng.lat, e.latlng.lng);
      });

      // Handle map resize when container dimensions change
      this._setupResizeHandling();

      // Ensure correct initial viewport sizing across layout settling phases
      const resizeTimes = [50, 200, 500];
      resizeTimes.forEach((delay) => {
        setTimeout(() => {
          if (this.map) {
            this.map.invalidateSize({ animate: false });
          }
        }, delay);
      });

      return this;
    }

    /**
     * Sets tile error and retry listeners. Never locks permanent error banner on single transient tile glitch.
     * Automatically hides warning banner once tiles finish loading.
     * @private
     */
    _setupTileErrorHandling() {
      if (!this.tileLayer) return;

      this._tileErrorCount = 0;
      this._tileLoadCount = 0;

      this.tileLayer.on('tileerror', (error) => {
        const tile = error.tile;
        if (tile && !tile._retryCount) {
          tile._retryCount = 1;
          setTimeout(() => {
            if (tile && tile.src) {
              const currentSrc = tile.src;
              const sep = currentSrc.indexOf('?') === -1 ? '?' : '&';
              tile.src = currentSrc + sep + '_r=' + Date.now();
            }
          }, 1000);
          return;
        }

        this._tileErrorCount++;

        // Only failover if primary tile server is truly unreachable (>= 12 failed tiles AND fewer than 3 tiles loaded)
        // Never trigger destructive setUrl() on transient panning hiccups
        if (this._tileErrorCount >= 12 && this._tileLoadCount < 3 && !this._hasFailedOver) {
          this._hasFailedOver = true;
          const fallbackKey = (this.options.provider === 'google') ? 'esri' : 'google';
          console.warn(`[MapController] Primary tile server (${this.options.provider}) unreachable. Failing over to ${fallbackKey}.`);
          this.setProvider(fallbackKey);
          return;
        }

        // Only display warning if widespread failure occurs (>= 8 tiles failed and error exceeds 50% of loaded)
        if (this._tileErrorCount >= 8 && this._tileErrorCount > this._tileLoadCount * 0.5) {
          console.warn('[MapController] Transient tile loading delay from map tile server.');
          this._showTileErrorBanner('Dữ liệu bản đồ tải chậm từ máy chủ. Đang tự động kết nối lại...');
        }
      });

      this.tileLayer.on('tileload', () => {
        this._tileLoadCount++;
      });

      this.tileLayer.on('load', () => {
        // Visible tiles loaded; if banner was shown due to transient delays, clear it
        if (this._tileErrorCount < 8 || this._tileLoadCount > 4) {
          this._hideTileErrorBanner();
        }
      });
    }

    /**
     * Shows error banner inside map container if tiles fail
     * @private
     */
    _showTileErrorBanner(message) {
      if (this._tileErrorBanner) return;

      const banner = document.createElement('div');
      banner.className = 'rtree-tile-error-banner';
      banner.innerHTML = `
        <span>⚠️ ${message}</span>
        <button type="button" title="Đóng">&times;</button>
      `;

      banner.querySelector('button').addEventListener('click', () => {
        this._hideTileErrorBanner();
      });

      this.container.appendChild(banner);
      this._tileErrorBanner = banner;

      // Auto dismiss after 8 seconds so user is never permanently blocked
      setTimeout(() => {
        this._hideTileErrorBanner();
      }, 8000);
    }

    /**
     * Hides tile error banner
     * @private
     */
    _hideTileErrorBanner() {
      if (this._tileErrorBanner && this._tileErrorBanner.parentNode) {
        this._tileErrorBanner.parentNode.removeChild(this._tileErrorBanner);
      }
      this._tileErrorBanner = null;
    }

    /**
     * Setup resize observer and window resize listener
     * @private
     */
    _setupResizeHandling() {
      this._windowResizeHandler = () => {
        if (this.map) {
          this.map.invalidateSize({ animate: false });
        }
      };
      window.addEventListener('resize', this._windowResizeHandler);

      if (typeof window.ResizeObserver !== 'undefined' && this.container) {
        this._resizeObserver = new ResizeObserver(() => {
          if (this.map) {
            this.map.invalidateSize({ animate: false });
          }
        });
        this._resizeObserver.observe(this.container);
        if (this.container.parentElement) {
          this._resizeObserver.observe(this.container.parentElement);
        }
      }
    }

    /**
     * Dynamically switches the active map tile provider.
     * @param {'google'|'vietmap'|'map4d'|'esri'|'osm'} providerKey
     * @param {string} [apiKey]
     */
    setProvider(providerKey, apiKey = null) {
      if (providerKey === 'osm') providerKey = 'google';
      const config = TILE_PROVIDERS[providerKey] || TILE_PROVIDERS.google;
      let url = config.url;
      const resolvedKey = apiKey || (typeof localStorage !== 'undefined' && localStorage.getItem('goride_map_apikey')) || (typeof window !== 'undefined' && window.GORIDE_MAP_APIKEY);
      if (resolvedKey) {
        url = url.replace('{key}', resolvedKey).replace('{apikey}', resolvedKey);
      }

      this.options.provider = providerKey;
      this.options.tileUrl = url;
      this.options.tileAttribution = config.attribution;
      this.options.subdomains = config.subdomains || ['mt0', 'mt1', 'mt2', 'mt3'];
      this.options.maxZoom = config.maxZoom || 20;

      if (this.tileLayer && this.map) {
        this.map.removeLayer(this.tileLayer);
        this.tileLayer = L.tileLayer(url, {
          maxZoom: this.options.maxZoom,
          attribution: this.options.tileAttribution,
          subdomains: this.options.subdomains,
          keepBuffer: this.options.keepBuffer,
          updateWhenIdle: this.options.updateWhenIdle,
          updateWhenZooming: this.options.updateWhenZooming,
          crossOrigin: this.options.crossOrigin
        });
        this._setupTileErrorHandling();
        this.tileLayer.addTo(this.map);
      }

      if (typeof localStorage !== 'undefined') {
        localStorage.setItem('goride_map_provider', providerKey);
        if (apiKey) localStorage.setItem('goride_map_apikey', apiKey);
      }

      this._updateProviderControlActiveState(providerKey);
      console.log(`[MapController] Switched basemap provider to "${config.name}".`);
      return this;
    }

    /**
     * Builds top-right basemap switcher control.
     * @private
     */
    _createProviderControl() {
      if (!this.map) return;
      const self = this;
      const Control = L.Control.extend({
        options: { position: 'topright' },
        onAdd: function () {
          const div = L.DomUtil.create('div', 'rtree-basemap-control');
          div.setAttribute('title', 'Chọn nhà cung cấp bản đồ');

          const providers = [
            { key: 'google', label: '🇻🇳 Google Maps VN' },
            { key: 'vietmap', label: '🇻🇳 VietMap' },
            { key: 'map4d', label: '🇻🇳 Map4D' },
            { key: 'esri', label: '🌍 Esri' }
          ];

          providers.forEach((p) => {
            const btn = document.createElement('button');
            btn.type = 'button';
            btn.className = 'rtree-basemap-btn' + (self.options.provider === p.key ? ' active' : '');
            btn.dataset.provider = p.key;
            btn.textContent = p.label;

            btn.addEventListener('click', (e) => {
              e.preventDefault();
              e.stopPropagation();
              self._handleProviderSwitch(p.key);
            });
            div.appendChild(btn);
          });

          L.DomEvent.disableClickPropagation(div);
          L.DomEvent.disableScrollPropagation(div);
          self._providerControlElement = div;
          return div;
        }
      });
      new Control().addTo(this.map);
    }

    /**
     * Handles provider selection and manages API key input if needed.
     * @private
     */
    _handleProviderSwitch(providerKey) {
      if (providerKey === 'vietmap' || providerKey === 'map4d') {
        let key = (typeof localStorage !== 'undefined' && localStorage.getItem('goride_map_apikey')) || (typeof window !== 'undefined' && window.GORIDE_MAP_APIKEY);
        if (!key) {
          const provName = providerKey === 'vietmap' ? 'VietMap' : 'Map4D';
          const inputKey = window.prompt(`Để kích hoạt bản đồ ${provName}, vui lòng nhập API Key của bạn:\n(Hoặc nhấn Hủy để quay lại Google Maps Việt Nam không cần key)`);
          if (inputKey && inputKey.trim()) {
            key = inputKey.trim();
            if (typeof localStorage !== 'undefined') {
              localStorage.setItem('goride_map_apikey', key);
            }
          } else {
            console.log(`[MapController] No API key entered for ${provName}. Remaining on ${this.options.provider}.`);
            return;
          }
        }
        this.setProvider(providerKey, key);
      } else {
        this.setProvider(providerKey);
      }
    }

    /**
     * Updates active class on basemap control buttons.
     * @private
     */
    _updateProviderControlActiveState(activeKey) {
      if (!this._providerControlElement) return;
      const buttons = this._providerControlElement.querySelectorAll('.rtree-basemap-btn');
      buttons.forEach((btn) => {
        if (btn.dataset.provider === activeKey) {
          btn.classList.add('active');
        } else {
          btn.classList.remove('active');
        }
      });
    }

    /**
     * Set map view to given coordinates and zoom level
     * @param {number} lat
     * @param {number} lng
     * @param {number} [zoom]
     */
    setView(lat, lng, zoom) {
      if (!this.map) return;
      const targetZoom = typeof zoom === 'number' ? zoom : this.map.getZoom();
      this.map.setView([lat, lng], targetZoom);
      this.showCoordinates(lat, lng);
    }

    /**
     * Creates a custom HTML DivIcon for pickup marker
     * @private
     */
    _createPickupIcon() {
      return L.divIcon({
        className: 'rtree-custom-icon',
        iconSize: [34, 42],
        iconAnchor: [17, 40],
        popupAnchor: [0, -38],
        html: `
          <div class="rtree-marker-container rtree-pin-pickup">
            <div class="rtree-pin">
              <div class="rtree-pin-body">
                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                  <circle cx="12" cy="12" r="4" fill="#ffffff" />
                </svg>
              </div>
              <div class="rtree-pulse"></div>
            </div>
          </div>
        `
      });
    }

    /**
     * Creates a custom HTML DivIcon for destination marker
     * @private
     */
    _createDestinationIcon() {
      return L.divIcon({
        className: 'rtree-custom-icon',
        iconSize: [34, 42],
        iconAnchor: [17, 40],
        popupAnchor: [0, -38],
        html: `
          <div class="rtree-marker-container rtree-pin-destination">
            <div class="rtree-pin">
              <div class="rtree-pin-body">
                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                  <path d="M4 15s1-1 4-1 5 2 8 2 4-1 4-1V3s-1 1-4 1-5-2-8-2-4 1-4 1z" fill="#ffffff"></path>
                  <line x1="4" y1="22" x2="4" y2="15"></line>
                </svg>
              </div>
              <div class="rtree-pulse"></div>
            </div>
          </div>
        `
      });
    }

    /**
     * Creates a custom HTML DivIcon for driver marker with modern top-down vector vehicle
     * @private
     * @param {number} heading - Degrees (0 - 360)
     * @param {Object} [metadata={}]
     */
    _createDriverIcon(heading = 0, metadata = {}) {
      const safeHeading = ((heading || 0) % 360 + 360) % 360;
      const isActive = metadata && (metadata.isActive || metadata.id === 'active_driver');

      // Modern Ride-Hailing Palette:
      // Active trip car: GoRide Emerald Green (#10b981) with subtle pulsing halo
      // Idle surrounding cars: Crisp Pearl White (#ffffff) with silver accents and dark windshield
      const bodyColor = isActive ? '#10b981' : (metadata.color || '#ffffff');
      const strokeColor = isActive ? '#059669' : (metadata.strokeColor || '#94a3b8');
      const roofColor = isActive ? '#047857' : '#e2e8f0';
      const mirrorColor = isActive ? '#10b981' : '#64748b';

      return L.divIcon({
        className: 'rtree-custom-icon',
        iconSize: [30, 50],
        iconAnchor: [15, 25],
        popupAnchor: [0, -25],
        html: `
          <div class="rtree-driver-marker" style="transform: rotate(${safeHeading}deg);">
            ${isActive ? '<div class="rtree-active-driver-halo"></div>' : ''}
            <div class="rtree-car-body">
              <svg width="26" height="46" viewBox="0 0 26 46" fill="none" xmlns="http://www.w3.org/2000/svg">
                <!-- Tires -->
                <rect x="0.5" y="8" width="2.5" height="7" rx="1.2" fill="#1e293b" />
                <rect x="23" y="8" width="2.5" height="7" rx="1.2" fill="#1e293b" />
                <rect x="0.5" y="31" width="2.5" height="7" rx="1.2" fill="#1e293b" />
                <rect x="23" y="31" width="2.5" height="7" rx="1.2" fill="#1e293b" />

                <!-- Car Chassis Outer Body -->
                <path d="M6 4 C10 2.5, 16 2.5, 20 4 C23 6, 24 11, 24 19 C24 31, 23 41, 21 44 C19 45.5, 7 45.5, 5 44 C3 41, 2 31, 2 19 C2 11, 3 6, 6 4 Z" 
                      fill="${bodyColor}" stroke="${strokeColor}" stroke-width="1.2" />

                <!-- Front Windshield -->
                <path d="M5.5 14 C9 12.5, 17 12.5, 20.5 14 C19.8 19, 18.5 20, 13 20 C7.5 20, 6.2 19, 5.5 14 Z" 
                      fill="#1e293b" />

                <!-- Roof Panel -->
                <rect x="6.5" y="21.5" width="13" height="10" rx="1.5" fill="${roofColor}" />

                <!-- Rear Windshield -->
                <path d="M7 33.5 C10 33, 16 33, 19 33.5 C19.5 36.5, 19 37.5, 17.5 38.5 C15 39, 11 39, 8.5 38.5 C7 37.5, 6.5 36.5, 7 33.5 Z" 
                      fill="#1e293b" />

                <!-- Side Mirrors -->
                <path d="M1.5 15.5 C0.5 15.5, 0 16.5, 0 17.5 C0 18.5, 1 19, 2 18.5 Z" fill="${mirrorColor}" />
                <path d="M24.5 15.5 C25.5 15.5, 26 16.5, 26 17.5 C26 18.5, 25 19, 24 18.5 Z" fill="${mirrorColor}" />

                <!-- Headlights (Warm LED) -->
                <ellipse cx="5" cy="5" rx="1.8" ry="1.2" fill="#fef08a" />
                <ellipse cx="21" cy="5" rx="1.8" ry="1.2" fill="#fef08a" />

                <!-- Taillights (Red LED) -->
                <ellipse cx="4.5" cy="43.5" rx="1.5" ry="1" fill="#ef4444" />
                <ellipse cx="21.5" cy="43.5" rx="1.5" ry="1" fill="#ef4444" />
              </svg>
            </div>
          </div>
        `
      });
    }

    /**
     * Creates a custom HTML DivIcon for temporary click selection
     * @private
     */
    _createTempIcon() {
      return L.divIcon({
        className: 'rtree-custom-icon',
        iconSize: [34, 42],
        iconAnchor: [17, 40],
        popupAnchor: [0, -38],
        html: `
          <div class="rtree-marker-container rtree-pin-temp">
            <div class="rtree-pin">
              <div class="rtree-pin-body">
                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                  <path d="M12 2v20M2 12h20" />
                </svg>
              </div>
              <div class="rtree-pulse"></div>
            </div>
          </div>
        `
      });
    }

    /**
     * Adds green pickup marker with popup 'Điểm đón'
     * @param {number} lat
     * @param {number} lng
     * @param {string} [popupContent='Điểm đón']
     * @returns {L.Marker}
     */
    addPickupMarker(lat, lng, popupContent = 'Điểm đón') {
      if (!this.map) return null;

      if (this.markers.pickup) {
        this.map.removeLayer(this.markers.pickup);
        this.markers.pickup = null;
      }

      const icon = this._createPickupIcon();
      const marker = L.marker([lat, lng], {
        icon,
        title: 'Điểm đón',
        riseOnHover: true
      });

      const popupHtml = `
        <div class="rtree-popup-title" style="color: #10b981;">
          <span style="display:inline-block;width:8px;height:8px;border-radius:50%;background:#10b981;"></span>
          ${popupContent}
        </div>
        <div class="rtree-popup-sub">Vĩ độ: ${lat.toFixed(5)}, Kinh độ: ${lng.toFixed(5)}</div>
      `;

      marker.bindPopup(popupHtml, { className: 'rtree-popup', closeButton: true });
      marker.addTo(this.map);

      this.markers.pickup = marker;
      return marker;
    }

    /**
     * Adds red destination marker with popup 'Điểm đến'
     * @param {number} lat
     * @param {number} lng
     * @param {string} [popupContent='Điểm đến']
     * @returns {L.Marker}
     */
    addDestinationMarker(lat, lng, popupContent = 'Điểm đến') {
      if (!this.map) return null;

      if (this.markers.destination) {
        this.map.removeLayer(this.markers.destination);
        this.markers.destination = null;
      }

      const icon = this._createDestinationIcon();
      const marker = L.marker([lat, lng], {
        icon,
        title: 'Điểm đến',
        riseOnHover: true
      });

      const popupHtml = `
        <div class="rtree-popup-title" style="color: #ef4444;">
          <span style="display:inline-block;width:8px;height:8px;border-radius:50%;background:#ef4444;"></span>
          ${popupContent}
        </div>
        <div class="rtree-popup-sub">Vĩ độ: ${lat.toFixed(5)}, Kinh độ: ${lng.toFixed(5)}</div>
      `;

      marker.bindPopup(popupHtml, { className: 'rtree-popup', closeButton: true });
      marker.addTo(this.map);

      this.markers.destination = marker;
      return marker;
    }

    /**
     * Adds driver marker with car icon
     * @param {string|number} id - Driver unique ID
     * @param {number} lat
     * @param {number} lng
     * @param {number} [heading=0] - Direction heading in degrees
     * @param {Object} [metadata={}] - Optional driver metadata for popup
     * @returns {L.Marker}
     */
    addDriverMarker(id, lat, lng, heading = 0, metadata = {}) {
      if (!this.map) return null;

      // Handle polymorphic invocation: addDriverMarker({ id, lat, lng, heading, ... })
      if (id && typeof id === 'object') {
        const dObj = id;
        return this.addDriverMarker(
          dObj.id || dObj.driverId || 'driver',
          dObj.lat || dObj.latitude,
          dObj.lng || dObj.lon || dObj.longitude,
          dObj.heading || 0,
          dObj
        );
      }

      const driverKey = String(id);

      if (this.markers.drivers.has(driverKey)) {
        return this.updateDriverMarker(driverKey, lat, lng, heading, metadata);
      }

      const icon = this._createDriverIcon(heading, metadata);
      const marker = L.marker([lat, lng], {
        icon,
        title: `Tài xế #${driverKey}`,
        riseOnHover: true
      });

      const driverName = metadata.name || `Tài xế #${driverKey}`;
      const driverVehicle = metadata.vehicle || 'Xe máy / Ô tô';
      const statusText = metadata.status ? `<div style="font-size:11px;color:#38bdf8;margin-top:2px;">Trạng thái: ${metadata.status}</div>` : '';

      const popupHtml = `
        <div class="rtree-popup-title" style="color: #10b981;">
          <span style="display:inline-block;width:8px;height:8px;border-radius:50%;background:#10b981;"></span>
          ${driverName}
        </div>
        <div class="rtree-popup-sub">${driverVehicle}</div>
        <div class="rtree-popup-sub">Tọa độ: ${lat.toFixed(5)}, ${lng.toFixed(5)}</div>
        <div class="rtree-popup-sub">Góc hướng: ${Math.round(heading || 0)}°</div>
        ${statusText}
      `;

      marker.bindPopup(popupHtml, { className: 'rtree-popup' });
      marker.addTo(this.map);

      // Store heading on marker instance for animation reference
      marker._currentHeading = heading;
      marker._continuousHeading = heading;
      marker._driverData = metadata;

      this.markers.drivers.set(driverKey, marker);
      return marker;
    }

    /**
     * Updates an existing driver marker position and heading
     * @param {string|number} id
     * @param {number} lat
     * @param {number} lng
     * @param {number} [heading=0]
     * @param {Object} [metadata]
     * @returns {L.Marker}
     */
    updateDriverMarker(id, lat, lng, heading = 0, metadata = null) {
      if (!this.map) return null;

      const driverKey = String(id);
      const marker = this.markers.drivers.get(driverKey);

      if (!marker) {
        return this.addDriverMarker(driverKey, lat, lng, heading, metadata || {});
      }

      // Update position
      marker.setLatLng([lat, lng]);

      // Update metadata
      if (metadata) {
        const hadActive = marker._driverData && marker._driverData.isActive;
        marker._driverData = Object.assign(marker._driverData || {}, metadata);
        // If active state changed, re-render icon
        if (!hadActive && marker._driverData.isActive) {
          marker.setIcon(this._createDriverIcon(heading, marker._driverData));
        }
      }

      // Continuous shortest-angle heading rotation
      const safeHeading = ((heading || 0) % 360 + 360) % 360;
      marker._currentHeading = safeHeading;

      if (typeof marker._continuousHeading === 'undefined') {
        marker._continuousHeading = safeHeading;
      } else {
        let diff = safeHeading - (marker._continuousHeading % 360);
        if (diff > 180) diff -= 360;
        if (diff < -180) diff += 360;
        marker._continuousHeading += diff;
      }

      // Re-create icon or rotate element
      const iconElement = marker.getElement();
      if (iconElement) {
        const carMarker = iconElement.querySelector('.rtree-driver-marker');
        if (carMarker) {
          carMarker.style.transform = `rotate(${marker._continuousHeading}deg)`;
        } else {
          marker.setIcon(this._createDriverIcon(marker._continuousHeading, marker._driverData));
        }
      } else {
        marker.setIcon(this._createDriverIcon(marker._continuousHeading, marker._driverData));
      }

      return marker;
    }

    /**
     * Removes driver marker by ID
     * @param {string|number} id
     * @returns {boolean}
     */
    removeDriverMarker(id) {
      if (!this.map) return false;

      const driverKey = String(id);
      const marker = this.markers.drivers.get(driverKey);

      if (marker) {
        this.map.removeLayer(marker);
        this.markers.drivers.delete(driverKey);
        return true;
      }
      return false;
    }

    /**
     * Clears markers by type ('pickup', 'destination', 'drivers', 'temp', or 'all')
     * @param {string} [type='all']
     */
    clearMarkers(type = 'all') {
      if (!this.map) return;

      const targetType = (type || 'all').toLowerCase();

      if (targetType === 'pickup' || targetType === 'all') {
        if (this.markers.pickup) {
          this.map.removeLayer(this.markers.pickup);
          this.markers.pickup = null;
        }
      }

      if (targetType === 'destination' || targetType === 'all') {
        if (this.markers.destination) {
          this.map.removeLayer(this.markers.destination);
          this.markers.destination = null;
        }
      }

      if (targetType === 'temp' || targetType === 'all') {
        if (this.markers.temp) {
          this.map.removeLayer(this.markers.temp);
          this.markers.temp = null;
        }
      }

      if (targetType === 'drivers' || targetType === 'all') {
        this.markers.drivers.forEach((marker) => {
          this.map.removeLayer(marker);
        });
        this.markers.drivers.clear();
      }
    }

    /**
     * Draws route polyline on map (blue dashed line by default)
     * @param {Array<Array<number>|Object>} coordinates - Array of [lat, lng] or {lat, lng}
     * @param {Object} [options] - Leaflet polyline options
     * @returns {L.Polyline}
     */
    drawRoute(coordinates, options = {}) {
      if (!this.map || !coordinates) {
        return null;
      }

      // Handle signature overload: drawRoute([pLat, pLng], [dLat, dLng], opts)
      let targetCoords = coordinates;
      let targetOpts = options;
      if (
        Array.isArray(coordinates) &&
        coordinates.length === 2 &&
        typeof coordinates[0] === 'number' &&
        typeof coordinates[1] === 'number' &&
        Array.isArray(options) &&
        options.length === 2 &&
        typeof options[0] === 'number' &&
        typeof options[1] === 'number'
      ) {
        targetCoords = [coordinates, options];
        targetOpts = arguments[2] || {};
      }

      if (!Array.isArray(targetCoords) || targetCoords.length === 0) {
        return null;
      }

      this.clearRoute();

      // Normalize coordinates to [lat, lng]
      const latLngs = targetCoords.map((coord) => {
        if (Array.isArray(coord)) {
          return [Number(coord[0]), Number(coord[1])];
        }
        if (coord && typeof coord === 'object') {
          return [Number(coord.lat || coord.latitude), Number(coord.lng || coord.lon || coord.longitude)];
        }
        return coord;
      });

      const polylineOptions = Object.assign(
        {
          color: '#2563eb',
          weight: 5,
          opacity: 0.85,
          dashArray: null,
          lineCap: 'round',
          lineJoin: 'round'
        },
        targetOpts
      );

      this.routePolyline = L.polyline(latLngs, polylineOptions).addTo(this.map);
      return this.routePolyline;
    }

    /**
     * Compatibility alias for showing route between two objects {lat, lng} or [lat, lng]
     */
    showRoute(origin, destination, options = {}) {
      if (!origin || !destination) return null;
      const p1 = Array.isArray(origin) ? origin : [origin.lat || origin.latitude, origin.lng || origin.lon || origin.longitude];
      const p2 = Array.isArray(destination) ? destination : [destination.lat || destination.latitude, destination.lng || destination.lon || destination.longitude];
      return this.drawRoute([p1, p2], options);
    }

    /**
     * Compatibility alias: setPickupMarker
     */
    setPickupMarker(lat, lng, popupContent) {
      return this.addPickupMarker(lat, lng, popupContent);
    }

    /**
     * Compatibility alias: setDestinationMarker
     */
    setDestinationMarker(lat, lng, popupContent) {
      return this.addDestinationMarker(lat, lng, popupContent);
    }

    /**
     * Clears current route polyline from map
     */
    clearRoute() {
      if (this.routePolyline && this.map) {
        this.map.removeLayer(this.routePolyline);
        this.routePolyline = null;
      }
    }

    /**
     * Enables click-to-select mode.
     * When user clicks on map, callback({lat, lng}) is invoked, a temporary marker is placed,
     * and a 'locationSelected' event is dispatched.
     * @param {Function} callback
     */
    enableClickToSelect(callback) {
      if (!this.map) return;

      this.disableClickToSelect();
      this.isClickToSelectActive = true;
      this._userSelectCallback = callback;

      // Change cursor to crosshair
      this.container.style.cursor = 'crosshair';

      this._clickToSelectHandler = (e) => {
        const { lat, lng } = e.latlng;

        // Place or move temporary selection marker
        if (this.markers.temp) {
          this.markers.temp.setLatLng([lat, lng]);
        } else {
          this.markers.temp = L.marker([lat, lng], {
            icon: this._createTempIcon(),
            title: 'Vị trí đã chọn',
            riseOnHover: true
          }).addTo(this.map);
        }

        // Update coordinate display
        this.showCoordinates(lat, lng);

        const eventData = { lat, lng };

        // Invoke callback
        if (typeof this._userSelectCallback === 'function') {
          this._userSelectCallback(eventData);
        }

        // Trigger custom event
        this.trigger('locationSelected', eventData);

        // Also dispatch standard DOM CustomEvent on container
        const customEvt = new CustomEvent('locationSelected', {
          bubbles: true,
          detail: eventData
        });
        this.container.dispatchEvent(customEvt);
      };

      this.map.on('click', this._clickToSelectHandler);
    }

    /**
     * Disables click-to-select mode and removes temporary marker
     */
    disableClickToSelect() {
      if (!this.map) return;

      if (this._clickToSelectHandler) {
        this.map.off('click', this._clickToSelectHandler);
        this._clickToSelectHandler = null;
      }

      this.isClickToSelectActive = false;
      this._userSelectCallback = null;

      if (this.container) {
        this.container.style.cursor = '';
      }

      if (this.markers.temp) {
        this.map.removeLayer(this.markers.temp);
        this.markers.temp = null;
      }
    }

    /**
     * Updates coordinates display in UI element
     * @param {number} lat
     * @param {number} lng
     */
    showCoordinates(lat, lng) {
      if (typeof lat !== 'number' || typeof lng !== 'number') return;

      const elementId = this.options.coordinatesElementId;
      if (elementId) {
        const el = document.getElementById(elementId);
        if (el) {
          el.textContent = `${lat.toFixed(5)}, ${lng.toFixed(5)}`;
        }
      }
    }

    /**
     * Fits map view to contain given points
     * @param {Array<Array<number>|Object>|L.LatLngBounds} points
     * @param {Object} [options]
     */
    fitBounds(points, options = {}) {
      if (!this.map) return;

      if (!points) return;

      // Invalidate size first to ensure bounds calculation reflects actual container dimensions
      this.map.invalidateSize({ animate: false });

      const fitOptions = Object.assign(
        {
          padding: [40, 40],
          maxZoom: 16
        },
        options
      );

      // If already a LatLngBounds
      if (points instanceof L.LatLngBounds) {
        this.map.fitBounds(points, fitOptions);
        return;
      }

      if (Array.isArray(points) && points.length > 0) {
        const validCoords = points
          .map((p) => {
            if (Array.isArray(p)) return [p[0], p[1]];
            if (p && typeof p === 'object') return [p.lat || p.latitude, p.lng || p.lon || p.longitude];
            return null;
          })
          .filter((p) => p && typeof p[0] === 'number' && typeof p[1] === 'number');

        if (validCoords.length > 0) {
          const bounds = L.latLngBounds(validCoords);
          this.map.fitBounds(bounds, fitOptions);
        }
      }
    }

    /**
     * Returns current center of the map
     * @returns {{lat: number, lng: number}|null}
     */
    getCenter() {
      if (!this.map) return null;
      const center = this.map.getCenter();
      return { lat: center.lat, lng: center.lng };
    }

    /**
     * Returns current zoom level
     * @returns {number|null}
     */
    getZoom() {
      if (!this.map) return null;
      return this.map.getZoom();
    }

    /**
     * Register event listener
     * @param {string} event
     * @param {Function} handler
     */
    on(event, handler) {
      if (!this._eventListeners.has(event)) {
        this._eventListeners.set(event, []);
      }
      this._eventListeners.get(event).push(handler);
      return this;
    }

    /**
     * Unregister event listener
     * @param {string} event
     * @param {Function} handler
     */
    off(event, handler) {
      if (!this._eventListeners.has(event)) return this;
      const handlers = this._eventListeners.get(event).filter((h) => h !== handler);
      this._eventListeners.set(event, handlers);
      return this;
    }

    /**
     * Trigger event listener
     * @param {string} event
     * @param {*} data
     */
    trigger(event, data) {
      if (!this._eventListeners.has(event)) return;
      this._eventListeners.get(event).forEach((handler) => {
        try {
          handler(data);
        } catch (err) {
          console.error(`[MapController] Error in event listener for "${event}":`, err);
        }
      });
    }

    /**
     * Forces recalculation of map container size
     */
    invalidateSize() {
      if (this.map) {
        this.map.invalidateSize();
      }
    }

    /**
     * Cleans up map, markers, listeners, and observers
     */
    destroy() {
      this.disableClickToSelect();
      this.clearMarkers('all');
      this.clearRoute();

      if (this._resizeObserver) {
        this._resizeObserver.disconnect();
        this._resizeObserver = null;
      }

      if (this._windowResizeHandler) {
        window.removeEventListener('resize', this._windowResizeHandler);
        this._windowResizeHandler = null;
      }

      this._hideTileErrorBanner();

      if (this.map) {
        this.map.remove();
        this.map = null;
      }

      this._eventListeners.clear();
    }
  }

  // Export to window/global scope
  window.TILE_PROVIDERS = TILE_PROVIDERS;
  window.MapController = MapController;
})(typeof window !== 'undefined' ? window : this);
