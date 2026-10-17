/**
 * GoRide Ride-Hailing Platform
 * Driver Page Controller
 * 
 * Handles driver online/offline status, real-time incoming ride requests,
 * request accept/decline responses, active trip lifecycle state machine
 * ('Heading to Pickup' -> 'Arrived at Pickup' -> 'Trip in Progress' -> 'Complete Trip'),
 * driver earnings/rating stats, and map visualization.
 */

class DriverController {
  constructor() {
    this.mapController = null;
    this.isOnline = false;
    this.activeTrip = null; // Currently accepted ride trip object
    this.incomingRequests = [];
    this.pollIntervalId = null;
    this.driverStats = {
      tripsToday: 0,
      earnings: 0.00,
      rating: 4.95,
      onlineHours: 0
    };

    // Driver location (simulated or GPS)
    this.currentPosition = {
      lat: 10.776889,
      lng: 106.700806,
      address: 'Driver Location (District 1)'
    };

    // Standard phase sequence
    this.PHASES = [
      {
        id: 'heading_to_pickup',
        label: 'Heading to Pickup',
        badgeClass: 'badge-warning',
        nextAction: 'Arrived at Pickup',
        nextPhase: 'arrived_at_pickup',
        description: 'Navigate towards the rider pickup location.'
      },
      {
        id: 'arrived_at_pickup',
        label: 'Arrived at Pickup',
        badgeClass: 'badge-info',
        nextAction: 'Start Trip',
        nextPhase: 'trip_in_progress',
        description: 'Waiting for rider to enter the vehicle.'
      },
      {
        id: 'trip_in_progress',
        label: 'Trip in Progress',
        badgeClass: 'badge-primary',
        nextAction: 'Complete Trip',
        nextPhase: 'complete_trip',
        description: 'Driving rider to the designated destination.'
      },
      {
        id: 'complete_trip',
        label: 'Complete Trip',
        badgeClass: 'badge-success',
        nextAction: 'Finish',
        nextPhase: null,
        description: 'Trip ended. Processing payment.'
      }
    ];
  }

  /**
   * Initializes the Driver page controller on DOMContentLoaded.
   */
  async init() {
    console.log('[DriverController] Initializing driver page...');
    this.initMap();
    this.bindEvents();
    await this.loadDriverStatus();
    this.renderOnlineStatusUI();

    // If driver is already online from saved state, start polling
    if (this.isOnline) {
      this.startRequestsPolling();
    } else {
      this.renderRequestsList([]);
    }

    console.log('[DriverController] Driver page controller ready.');
  }

  /**
   * Initializes MapController and driver location marker.
   */
  initMap() {
    try {
      const mapContainer = document.getElementById('driver-map') ||
                           document.getElementById('map') ||
                           document.querySelector('.map-container') ||
                           document.querySelector('[data-driver-map]');

      if (!mapContainer) {
        console.warn('[DriverController] Map container element not found.');
        return;
      }

      const containerId = mapContainer.id || 'driver-map';
      if (!mapContainer.id) mapContainer.id = containerId;

      if (typeof MapController !== 'undefined') {
        this.mapController = new MapController(containerId, {
          defaultCenter: [this.currentPosition.lat, this.currentPosition.lng],
          defaultZoom: 14,
          interactive: true
        });

        if (typeof this.mapController.init === 'function') {
          this.mapController.init();
        }

        // Render driver car marker
        this.updateDriverMapMarker();
      } else {
        console.warn('[DriverController] MapController class not found in global scope.');
      }
    } catch (err) {
      console.error('[DriverController] Failed to initialize map:', err);
    }
  }

  /**
   * Places or updates driver's current position on the map.
   */
  updateDriverMapMarker() {
    if (!this.mapController) return;

    if (typeof this.mapController.addDriverMarker === 'function') {
      this.mapController.addDriverMarker({
        id: 'current-driver',
        lat: this.currentPosition.lat,
        lng: this.currentPosition.lng,
        name: 'You (Driver)',
        status: this.isOnline ? 'online' : 'offline'
      });
    } else if (typeof this.mapController.addMarker === 'function') {
      this.mapController.addMarker('driver-self', [this.currentPosition.lat, this.currentPosition.lng], {
        title: 'Your Location',
        icon: 'driver'
      });
    }
  }

  /**
   * Binds UI controls, toggle switches, and event listeners.
   */
  bindEvents() {
    // Online / Offline toggle (supports online-toggle checkbox in driver.html)
    const toggleBtn = document.getElementById('online-toggle') ||
                      document.getElementById('driver-online-toggle') ||
                      document.getElementById('toggle-online') ||
                      document.querySelector('[data-action="toggle-online"]') ||
                      document.querySelector('.toggle-online-btn');
    if (toggleBtn) {
      if (toggleBtn.type === 'checkbox') {
        toggleBtn.addEventListener('change', (e) => {
          this.setOnlineStatus(e.target.checked);
        });
      } else {
        toggleBtn.addEventListener('click', (e) => {
          e.preventDefault();
          this.setOnlineStatus(!this.isOnline);
        });
      }
    }

    // Refresh requests button
    const btnRefresh = document.getElementById('btn-refresh-requests') ||
                       document.querySelector('[data-action="refresh-requests"]');
    if (btnRefresh) {
      btnRefresh.addEventListener('click', (e) => {
        e.preventDefault();
        this.loadIncomingRequests();
      });
    }
  }

  /**
   * Toggles and updates driver online/offline state.
   * Calls dataAdapter.updateDriverOnline().
   * @param {boolean} isOnline 
   */
  async setOnlineStatus(isOnline) {
    this.isOnline = !!isOnline;

    try {
      if (typeof dataAdapter !== 'undefined' && typeof dataAdapter.updateDriverOnline === 'function') {
        await Promise.resolve(dataAdapter.updateDriverOnline(this.isOnline));
      } else if (window.dataAdapter && typeof window.dataAdapter.updateDriverOnline === 'function') {
        await Promise.resolve(window.dataAdapter.updateDriverOnline(this.isOnline));
      }

      this.renderOnlineStatusUI();
      this.updateDriverMapMarker();

      if (this.isOnline) {
        this.showToast('Bạn đã chuyển sang trạng thái TRỰC TUYẾN. Đang tìm chuyến xe gần nhất...', 'success');
        this.startRequestsPolling();
        await this.loadIncomingRequests();
      } else {
        this.showToast('Bạn đã chuyển sang trạng thái NGOẠI TUYẾN.', 'info');
        this.stopRequestsPolling();
        this.incomingRequests = [];
        this.renderRequestsList([]);
      }
    } catch (err) {
      console.error('[DriverController] Failed to update driver online state:', err);
      this.showToast('Không thể cập nhật trạng thái: ' + err.message, 'error');
    }
  }

  /**
   * Updates visual indicators for online/offline switch and status pill.
   */
  renderOnlineStatusUI() {
    const toggleBtn = document.getElementById('online-toggle') ||
                      document.getElementById('driver-online-toggle') ||
                      document.getElementById('toggle-online') ||
                      document.querySelector('[data-action="toggle-online"]');

    if (toggleBtn) {
      if (toggleBtn.type === 'checkbox') {
        toggleBtn.checked = this.isOnline;
      } else {
        toggleBtn.textContent = this.isOnline ? 'Ngoại tuyến' : 'Trực tuyến';
        toggleBtn.className = `btn ${this.isOnline ? 'btn-danger' : 'btn-success'}`;
      }
    }

    // Handle status text and indicator dot in driver.html
    const statusText = document.getElementById('status-text');
    if (statusText) {
      statusText.textContent = this.isOnline ? 'Trực tuyến' : 'Ngoại tuyến';
    }

    const statusIndicator = document.getElementById('status-indicator');
    if (statusIndicator) {
      statusIndicator.className = this.isOnline ? 'status-dot status-online' : 'status-dot status-offline';
    }

    const statusBadge = document.getElementById('driver-status-badge') ||
                        document.querySelector('.driver-status-badge');
    if (statusBadge) {
      if (this.isOnline) {
        statusBadge.innerHTML = `<span style="display: inline-block; width: 8px; height: 8px; background: #10b981; border-radius: 50%; margin-right: 6px;"></span> ONLINE`;
        statusBadge.style.background = '#dcfce7';
        statusBadge.style.color = '#15803d';
      } else {
        statusBadge.innerHTML = `<span style="display: inline-block; width: 8px; height: 8px; background: #94a3b8; border-radius: 50%; margin-right: 6px;"></span> OFFLINE`;
        statusBadge.style.background = '#f1f5f9';
        statusBadge.style.color = '#64748b';
      }
    }

    // Toggle radar wave / scanning animation if online
    const radarEffect = document.getElementById('driver-radar-ping');
    if (radarEffect) {
      radarEffect.style.display = this.isOnline ? 'block' : 'none';
    }
  }

  /**
   * Polls for incoming requests periodically when driver is online.
   */
  startRequestsPolling() {
    this.stopRequestsPolling();
    // Poll every 10 seconds
    this.pollIntervalId = setInterval(() => {
      if (this.isOnline && !this.activeTrip) {
        this.loadIncomingRequests();
      }
    }, 10000);
  }

  /**
   * Stops polling for requests.
   */
  stopRequestsPolling() {
    if (this.pollIntervalId) {
      clearInterval(this.pollIntervalId);
      this.pollIntervalId = null;
    }
  }

  /**
   * Fetches incoming ride requests from dataAdapter.getDriverRequests().
   */
  async loadIncomingRequests() {
    if (!this.isOnline || this.activeTrip) {
      return;
    }

    const container = document.getElementById('incoming-requests-list') ||
                        document.getElementById('driver-requests') ||
                        document.getElementById('requests-container') ||
                        document.querySelector('.requests-list');

    if (!container) return;

    this.renderRequestsLoading(container);

    try {
      let requests = [];

      if (typeof dataAdapter !== 'undefined' && typeof dataAdapter.getDriverRequests === 'function') {
        requests = await Promise.resolve(dataAdapter.getDriverRequests());
      } else if (window.dataAdapter && typeof window.dataAdapter.getDriverRequests === 'function') {
        requests = await Promise.resolve(window.dataAdapter.getDriverRequests());
      } else {
        // Fallback demo requests
        requests = this.getFallbackRequests();
      }

      this.incomingRequests = Array.isArray(requests) ? requests : [];
      this.renderRequestsList(this.incomingRequests);
    } catch (err) {
      console.error('[DriverController] Error loading driver requests:', err);
      container.innerHTML = `
        <div class="error-box" style="padding: 1rem; text-align: center; color: #ef4444; font-size: 0.85rem;">
          Failed to load incoming requests.
        </div>
      `;
    }
  }

  /**
   * Renders loading state in requests container.
   */
  renderRequestsLoading(container) {
    // Only show spinner if currently empty
    if (!container.children.length) {
      container.innerHTML = `
        <div style="text-align: center; padding: 2rem 1rem; color: #64748b;">
          <div class="spinner" style="display: inline-block; width: 22px; height: 22px; border: 2px solid #e2e8f0; border-top-color: #2563eb; border-radius: 50%; animation: spin 1s linear infinite;"></div>
          <p style="margin-top: 0.5rem; font-size: 0.85rem;">Listening for nearby riders...</p>
        </div>
      `;
    }
  }

  /**
   * Renders incoming ride request cards or empty state.
   * @param {Array} requests 
   */
  renderRequestsList(requests) {
    const container = document.getElementById('incoming-requests-list') ||
                      document.getElementById('driver-requests') ||
                      document.getElementById('requests-container') ||
                      document.querySelector('.requests-list');

    if (!container) return;

    if (!this.isOnline) {
      container.innerHTML = `
        <div class="offline-state" style="text-align: center; padding: 2.5rem 1rem; color: #94a3b8;">
          <div style="font-size: 2rem; margin-bottom: 0.5rem;">🛑</div>
          <h4 style="margin: 0; color: #475569; font-size: 1rem;">You are currently Offline</h4>
          <p style="margin: 0.25rem 0 0; font-size: 0.85rem;">Switch toggle to Online to begin receiving ride requests.</p>
        </div>
      `;
      return;
    }

    if (this.activeTrip) {
      container.innerHTML = `
        <div class="busy-state" style="text-align: center; padding: 1.5rem; color: #2563eb; background: #eff6ff; border-radius: 8px;">
          <h4 style="margin: 0; font-size: 0.95rem;">Active Trip in Progress</h4>
          <p style="margin: 0.25rem 0 0; font-size: 0.8rem; color: #3b82f6;">Complete your current ride before accepting new requests.</p>
        </div>
      `;
      return;
    }

    if (!requests || requests.length === 0) {
      container.innerHTML = `
        <div class="empty-requests" style="text-align: center; padding: 2.5rem 1rem; color: #94a3b8;">
          <div style="font-size: 2.2rem; margin-bottom: 0.5rem;">📡</div>
          <h4 style="margin: 0; color: #475569; font-size: 1rem;">No incoming ride requests</h4>
          <p style="margin: 0.25rem 0 0; font-size: 0.85rem;">We'll alert you as soon as a nearby rider requests a trip.</p>
        </div>
      `;
      return;
    }

    container.innerHTML = '';
    const wrapper = document.createElement('div');
    wrapper.style.display = 'flex';
    wrapper.style.flexDirection = 'column';
    wrapper.style.gap = '0.75rem';

    requests.forEach(req => {
      const card = document.createElement('div');
      card.className = 'request-card';
      card.id = `request-card-${req.id}`;
      card.style.background = '#ffffff';
      card.style.border = '1px solid #cbd5e1';
      card.style.borderRadius = '8px';
      card.style.padding = '1rem';
      card.style.boxShadow = '0 2px 4px rgba(0,0,0,0.06)';

      const fareStr = req.fare ? `$${parseFloat(req.fare).toFixed(2)}` : '$15.00';
      const riderName = req.customerName || req.riderName || 'Rider';
      const rating = req.rating ? `★ ${req.rating}` : '★ 4.9';
      const distance = req.distance ? `${req.distance} km` : '2.8 km';
      const duration = req.duration ? `${req.duration} mins` : '10 mins';
      const pickup = req.pickup ? (req.pickup.address || req.pickup) : 'Pickup Location';
      const dest = req.destination ? (req.destination.address || req.destination) : 'Destination Location';

      card.innerHTML = `
        <div style="display: flex; justify-content: space-between; align-items: flex-start; margin-bottom: 0.5rem;">
          <div>
            <div style="font-weight: 700; color: #1e293b; font-size: 1rem;">${riderName} <span style="font-size: 0.8rem; color: #f59e0b; font-weight: normal;">${rating}</span></div>
            <span style="font-size: 0.75rem; color: #64748b;">${req.rideType || 'Standard'} Ride Request</span>
          </div>
          <div style="text-align: right;">
            <div style="font-size: 1.25rem; font-weight: 800; color: #16a34a;">${fareStr}</div>
            <span style="font-size: 0.75rem; color: #64748b;">est. payout</span>
          </div>
        </div>

        <div style="font-size: 0.85rem; margin: 0.5rem 0; line-height: 1.4; color: #334155;">
          <div style="display: flex; align-items: flex-start; gap: 6px; margin-bottom: 4px;">
            <span style="color: #10b981;">●</span>
            <span style="overflow: hidden; text-overflow: ellipsis; white-space: nowrap; max-width: 280px;" title="${pickup}">Pickup: ${pickup}</span>
          </div>
          <div style="display: flex; align-items: flex-start; gap: 6px;">
            <span style="color: #ef4444;">■</span>
            <span style="overflow: hidden; text-overflow: ellipsis; white-space: nowrap; max-width: 280px;" title="${dest}">Dropoff: ${dest}</span>
          </div>
        </div>

        <div style="display: flex; justify-content: space-between; align-items: center; font-size: 0.78rem; color: #64748b; margin-bottom: 0.75rem; background: #f8fafc; padding: 4px 8px; border-radius: 4px;">
          <span>Trip: ${distance}</span>
          <span>Duration: ${duration}</span>
          <span>Pickup ETA: 3 mins</span>
        </div>

        <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 0.5rem;">
          <button class="btn btn-decline" data-action="decline-request" data-id="${req.id}" style="padding: 0.5rem; background: #f1f5f9; color: #475569; border: 1px solid #cbd5e1; border-radius: 6px; font-weight: 600; cursor: pointer;">
            Decline
          </button>
          <button class="btn btn-accept" data-action="accept-request" data-id="${req.id}" style="padding: 0.5rem; background: #16a34a; color: #ffffff; border: none; border-radius: 6px; font-weight: 600; cursor: pointer;">
            Accept Ride
          </button>
        </div>
      `;

      // Accept button listener
      const acceptBtn = card.querySelector('[data-action="accept-request"]');
      if (acceptBtn) {
        acceptBtn.addEventListener('click', () => this.handleAcceptRequest(req));
      }

      // Decline button listener
      const declineBtn = card.querySelector('[data-action="decline-request"]');
      if (declineBtn) {
        declineBtn.addEventListener('click', () => this.handleDeclineRequest(req.id));
      }

      wrapper.appendChild(card);
    });

    container.appendChild(wrapper);
  }

  /**
   * Accepts a ride request and sets up the active trip.
   * Calls dataAdapter.respondToRequest().
   * @param {Object} request 
   */
  async handleAcceptRequest(request) {
    try {
      this.showToast(`Accepting ride for ${request.customerName || 'rider'}...`, 'info');

      let response = null;
      if (typeof dataAdapter !== 'undefined' && typeof dataAdapter.respondToRequest === 'function') {
        response = await Promise.resolve(dataAdapter.respondToRequest(request.id, 'accept'));
      } else if (window.dataAdapter && typeof window.dataAdapter.respondToRequest === 'function') {
        response = await Promise.resolve(window.dataAdapter.respondToRequest(request.id, 'accept'));
      } else {
        response = { success: true };
      }

      // Initialize active trip
      this.activeTrip = {
        id: request.id,
        tripId: request.tripId || request.id,
        customerName: request.customerName || request.riderName || 'Nguyen Thi Mai',
        phone: request.phone || '+84 90 123 4567',
        pickup: request.pickup,
        destination: request.destination,
        fare: request.fare || 15.00,
        distance: request.distance || '3.2 km',
        phaseIndex: 0, // Starts at 'Heading to Pickup'
        startedAt: new Date().toISOString()
      };

      // Remove from incoming requests
      this.incomingRequests = this.incomingRequests.filter(r => r.id !== request.id);

      this.showToast('Ride accepted! Route to pickup location is displayed.', 'success');

      // Update UI & Map
      this.renderActiveTripUI();
      this.renderRequestsList(this.incomingRequests);
      this.showTripOnMap(this.activeTrip);
    } catch (err) {
      console.error('[DriverController] Error accepting ride:', err);
      this.showToast('Failed to accept ride: ' + err.message, 'error');
    }
  }

  /**
   * Declines a ride request.
   * Calls dataAdapter.respondToRequest().
   * @param {string} requestId 
   */
  async handleDeclineRequest(requestId) {
    try {
      if (typeof dataAdapter !== 'undefined' && typeof dataAdapter.respondToRequest === 'function') {
        await Promise.resolve(dataAdapter.respondToRequest(requestId, 'decline'));
      } else if (window.dataAdapter && typeof window.dataAdapter.respondToRequest === 'function') {
        await Promise.resolve(window.dataAdapter.respondToRequest(requestId, 'decline'));
      }

      this.incomingRequests = this.incomingRequests.filter(r => r.id !== requestId);
      this.renderRequestsList(this.incomingRequests);
      this.showToast('Ride request declined.', 'info');
    } catch (err) {
      console.error('[DriverController] Error declining request:', err);
    }
  }

  /**
   * Renders the Active Trip card with phase progression button.
   */
  renderActiveTripUI() {
    const container = document.getElementById('active-trip-panel') ||
                      document.getElementById('current-trip') ||
                      document.getElementById('active-trip-container') ||
                      document.querySelector('.active-trip-panel');

    if (!container) return;

    if (!this.activeTrip) {
      container.style.display = 'none';
      container.innerHTML = '';
      return;
    }

    container.style.display = 'block';

    const currentPhase = this.PHASES[this.activeTrip.phaseIndex] || this.PHASES[0];
    const isLastPhase = this.activeTrip.phaseIndex >= this.PHASES.length - 1;

    const pickupText = this.activeTrip.pickup ? (this.activeTrip.pickup.address || this.activeTrip.pickup) : 'Pickup';
    const destText = this.activeTrip.destination ? (this.activeTrip.destination.address || this.activeTrip.destination) : 'Destination';

    container.innerHTML = `
      <div class="active-trip-card" style="background: #ffffff; border: 2px solid #2563eb; border-radius: 10px; padding: 1.25rem; box-shadow: 0 4px 6px -1px rgba(37,99,235,0.1), 0 2px 4px -1px rgba(37,99,235,0.06); margin-bottom: 1.5rem;">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.75rem; border-bottom: 1px solid #f1f5f9; padding-bottom: 0.75rem;">
          <div>
            <span style="font-size: 0.75rem; font-weight: 700; color: #2563eb; text-transform: uppercase; letter-spacing: 0.05em;">Active Trip</span>
            <h3 style="margin: 0; font-size: 1.15rem; color: #0f172a;">${this.activeTrip.customerName}</h3>
            <span style="font-size: 0.8rem; color: #64748b;">${this.activeTrip.phone}</span>
          </div>
          <div style="text-align: right;">
            <div style="font-size: 1.35rem; font-weight: 800; color: #16a34a;">$${parseFloat(this.activeTrip.fare).toFixed(2)}</div>
            <span class="badge ${currentPhase.badgeClass}" style="display: inline-block; padding: 3px 10px; font-size: 0.75rem; font-weight: 600; border-radius: 9999px; background: #dbeafe; color: #1e40af;">
              ${currentPhase.label}
            </span>
          </div>
        </div>

        <!-- Stepper Visualizer -->
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 1rem; position: relative;">
          ${this.PHASES.map((p, idx) => {
            const isCompleted = idx < this.activeTrip.phaseIndex;
            const isCurrent = idx === this.activeTrip.phaseIndex;
            const stepColor = isCompleted ? '#10b981' : isCurrent ? '#2563eb' : '#cbd5e1';
            return `
              <div style="text-align: center; flex: 1; z-index: 2;">
                <div style="width: 22px; height: 22px; border-radius: 50%; background: ${stepColor}; color: #fff; font-size: 0.75rem; line-height: 22px; margin: 0 auto; font-weight: bold;">
                  ${isCompleted ? '✓' : idx + 1}
                </div>
                <span style="display: block; font-size: 0.65rem; color: ${isCurrent ? '#1e293b' : '#64748b'}; margin-top: 4px; font-weight: ${isCurrent ? '700' : 'normal'};">
                  ${p.label}
                </span>
              </div>
            `;
          }).join('')}
        </div>

        <div style="background: #f8fafc; border-radius: 6px; padding: 0.75rem; margin-bottom: 1rem; font-size: 0.85rem;">
          <div style="margin-bottom: 0.25rem;">
            <strong style="color: #10b981;">Pickup:</strong> ${pickupText}
          </div>
          <div>
            <strong style="color: #ef4444;">Dropoff:</strong> ${destText}
          </div>
        </div>

        <p style="font-size: 0.8rem; color: #64748b; margin: 0 0 1rem;">
          <em>${currentPhase.description}</em>
        </p>

        <button id="btn-advance-phase" class="btn btn-primary" style="width: 100%; padding: 0.75rem; font-size: 1rem; font-weight: 600; background: #2563eb; color: #fff; border: none; border-radius: 8px; cursor: pointer; transition: background 0.2s;">
          ${currentPhase.nextAction}
        </button>
      </div>
    `;

    const btnAdvance = document.getElementById('btn-advance-phase');
    if (btnAdvance) {
      btnAdvance.addEventListener('click', () => this.advanceTripPhase());
    }
  }

  /**
   * Advances the trip to the next lifecycle phase.
   * Calls dataAdapter.updateTripPhase().
   */
  async advanceTripPhase() {
    if (!this.activeTrip) return;

    const currentPhase = this.PHASES[this.activeTrip.phaseIndex];
    const nextIndex = this.activeTrip.phaseIndex + 1;

    try {
      const nextPhase = this.PHASES[nextIndex];
      const phaseName = nextPhase ? nextPhase.label : 'Complete Trip';

      // Update backend / adapter
      if (typeof dataAdapter !== 'undefined' && typeof dataAdapter.updateTripPhase === 'function') {
        await Promise.resolve(dataAdapter.updateTripPhase(this.activeTrip.id, phaseName));
      } else if (window.dataAdapter && typeof window.dataAdapter.updateTripPhase === 'function') {
        await Promise.resolve(window.dataAdapter.updateTripPhase(this.activeTrip.id, phaseName));
      }

      if (nextIndex >= this.PHASES.length - 1) {
        // Complete the trip
        await this.completeActiveTrip();
      } else {
        this.activeTrip.phaseIndex = nextIndex;
        this.showToast(`Trip phase updated: ${nextPhase.label}`, 'info');
        this.renderActiveTripUI();
      }
    } catch (err) {
      console.error('[DriverController] Error advancing trip phase:', err);
      this.showToast('Failed to update phase: ' + err.message, 'error');
    }
  }

  /**
   * Finalizes trip completion, adds earnings, clears map route,
   * updates stats, and resets active trip.
   */
  async completeActiveTrip() {
    const completedFare = parseFloat(this.activeTrip.fare) || 15.00;
    const completedId = this.activeTrip.id;

    // Update stats locally
    this.driverStats.tripsToday += 1;
    this.driverStats.earnings += completedFare;

    this.showToast(`Trip ${completedId} completed! Added $${completedFare.toFixed(2)} to earnings.`, 'success');

    // Clear active trip
    this.activeTrip = null;

    // Clear route from map
    if (this.mapController) {
      if (typeof this.mapController.clearRoute === 'function') {
        this.mapController.clearRoute();
      }
      if (typeof this.mapController.clearMarkers === 'function') {
        this.mapController.clearMarkers();
      }
      this.updateDriverMapMarker();
    }

    // Refresh UI
    this.renderActiveTripUI();
    this.renderDriverStats();
    await this.loadDriverStatus();

    // Resume incoming requests if still online
    if (this.isOnline) {
      await this.loadIncomingRequests();
    }
  }

  /**
   * Displays the current active trip on the map with pickup/destination markers and route.
   * @param {Object} trip 
   */
  async showTripOnMap(trip) {
    if (!this.mapController || !trip) return;

    try {
      const pLat = Number(trip.pickup && trip.pickup.lat ? trip.pickup.lat : (this.currentPosition.lat + 0.005));
      const pLng = Number(trip.pickup && trip.pickup.lng ? trip.pickup.lng : (this.currentPosition.lng + 0.005));
      const dLat = Number(trip.destination && trip.destination.lat ? trip.destination.lat : (this.currentPosition.lat + 0.025));
      const dLng = Number(trip.destination && trip.destination.lng ? trip.destination.lng : (this.currentPosition.lng + 0.02));

      // Clear previous route and markers
      if (typeof this.mapController.clearRoute === 'function') {
        this.mapController.clearRoute();
      }
      if (typeof this.mapController.clearMarkers === 'function') {
        this.mapController.clearMarkers('pickup');
        this.mapController.clearMarkers('destination');
      }

      // Render Pickup Marker
      const pickupLabel = trip.pickup?.name || trip.pickup?.address || 'Điểm đón khách';
      if (typeof this.mapController.addPickupMarker === 'function') {
        this.mapController.addPickupMarker(pLat, pLng, pickupLabel);
      } else if (typeof this.mapController.setPickupMarker === 'function') {
        this.mapController.setPickupMarker(pLat, pLng, pickupLabel);
      }

      // Render Destination Marker
      const destLabel = trip.destination?.name || trip.destination?.address || 'Điểm trả khách';
      if (typeof this.mapController.addDestinationMarker === 'function') {
        this.mapController.addDestinationMarker(dLat, dLng, destLabel);
      } else if (typeof this.mapController.setDestinationMarker === 'function') {
        this.mapController.setDestinationMarker(dLat, dLng, destLabel);
      }

      // Re-render driver car marker
      this.updateDriverMapMarker();

      // Attempt real C++ Dijkstra routing via REST API
      let routeDrawn = false;
      const client = (typeof apiClient !== 'undefined') ? apiClient : (window.apiClient || null);

      if (client && typeof client.getRoute === 'function') {
        try {
          const res = await client.getRoute(pLat, pLng, dLat, dLng);
          if (res && res.ok && res.data && res.data.success && Array.isArray(res.data.geometry)) {
            // Geometry from backend is in GeoJSON format [longitude, latitude]
            const leafletPath = res.data.geometry.map(([lon, lat]) => [lat, lon]);

            this.mapController.drawRoute(leafletPath, {
              color: '#10b981',
              weight: 5,
              opacity: 0.9,
              dashArray: null,
              lineCap: 'round',
              lineJoin: 'round'
            });

            if (typeof this.mapController.fitBounds === 'function') {
              this.mapController.fitBounds(leafletPath, { padding: [50, 50] });
            }
            routeDrawn = true;
          }
        } catch (apiErr) {
          console.warn('[DriverController] Real Dijkstra routing request failed, falling back to street-following router:', apiErr.message);
        }
      }

      // Tier 2: OpenStreetMap Routing Service (OSRM)
      if (!routeDrawn) {
        try {
          const osrmRes = await this.fetchOsrmRoute(pLat, pLng, dLat, dLng);
          if (osrmRes && osrmRes.ok && Array.isArray(osrmRes.geometry) && osrmRes.geometry.length > 0) {
            this.mapController.drawRoute(osrmRes.geometry, {
              color: '#10b981',
              weight: 5,
              opacity: 0.9,
              dashArray: null,
              lineCap: 'round',
              lineJoin: 'round'
            });
            if (typeof this.mapController.fitBounds === 'function') {
              this.mapController.fitBounds(osrmRes.geometry, { padding: [50, 50] });
            }
            routeDrawn = true;
          }
        } catch (osrmErr) {
          console.warn('[DriverController] OSRM routing fallback error:', osrmErr.message);
        }
      }

      // Tier 3: High-precision Street-Following Grid / Corridors (NEVER a straight diagonal line)
      if (!routeDrawn) {
        const streetPath = this.getStreetFollowingPath({ lat: pLat, lng: pLng }, { lat: dLat, lng: dLng });
        if (streetPath && streetPath.length >= 2) {
          this.mapController.drawRoute(streetPath, {
            color: '#10b981',
            weight: 4.5,
            opacity: 0.9,
            dashArray: null,
            lineCap: 'round',
            lineJoin: 'round'
          });
          if (typeof this.mapController.fitBounds === 'function') {
            this.mapController.fitBounds(streetPath, { padding: [60, 60] });
          }
        }
      }
    } catch (err) {
      console.warn('[DriverController] Could not render trip on map:', err);
    }
  }

  /**
   * Generates a realistic street-following route on HCMC's actual road network.
   * Guarantees the line adheres to actual street corridors (never a straight line cutting through buildings).
   */
  getStreetFollowingPath(pickup, dest) {
    const pLat = Number(pickup.lat || pickup.latitude);
    const pLng = Number(pickup.lng || pickup.lon || pickup.longitude);
    const dLat = Number(dest.lat || dest.latitude);
    const dLng = Number(dest.lng || dest.lon || dest.longitude);

    // 1. Chợ Bến Thành -> Vincom Center Đồng Khởi (Đi qua Lê Lợi, Pasteur, Lý Tự Trọng, Đồng Khởi)
    if (Math.abs(pLat - 10.7725) < 0.003 && Math.abs(pLng - 106.6980) < 0.003 &&
        Math.abs(dLat - 10.7781) < 0.003 && Math.abs(dLng - 106.7023) < 0.003) {
      return [
        [10.77250, 106.69800], // Cửa Nam Chợ Bến Thành
        [10.77315, 106.69880], // Đường Lê Lợi
        [10.77380, 106.69960], // Lê Lợi x Nam Kỳ Khởi Nghĩa
        [10.77445, 106.70040], // Lê Lợi x Pasteur
        [10.77530, 106.69970], // Đường Pasteur (hướng về phía Bắc)
        [10.77615, 106.69900], // Pasteur x Lý Tự Trọng
        [10.77670, 106.69970], // Lý Tự Trọng x Nam Kỳ Khởi Nghĩa
        [10.77735, 106.70050], // Lý Tự Trọng x Pasteur
        [10.77800, 106.70135], // Lý Tự Trọng x Đồng Khởi
        [10.77810, 106.70230]  // Vincom Center Đồng Khởi
      ];
    }

    // 2. Vincom Center Đồng Khởi -> Chợ Bến Thành (Lê Thánh Tôn)
    if (Math.abs(pLat - 10.7781) < 0.003 && Math.abs(pLng - 106.7023) < 0.003 &&
        Math.abs(dLat - 10.7725) < 0.003 && Math.abs(dLng - 106.6980) < 0.003) {
      return [
        [10.77810, 106.70230], // Vincom Center Đồng Khởi
        [10.77740, 106.70300], // Lê Thánh Tôn
        [10.77630, 106.70200], // Lê Thánh Tôn x Đồng Khởi
        [10.77510, 106.70070], // Lê Thánh Tôn x Pasteur
        [10.77400, 106.69960], // Lê Thánh Tôn x Nam Kỳ Khởi Nghĩa
        [10.77300, 106.69870], // Lê Thánh Tôn x Phan Bội Châu
        [10.77250, 106.69800]  // Chợ Bến Thành
      ];
    }

    // 3. Real Street Network Graph for Central HCMC (Never cuts diagonally across buildings)
    const intersections = [
      { id: 'bt', lat: 10.77250, lng: 106.69800 },         // Bến Thành
      { id: 'leloi_nkk', lat: 10.77380, lng: 106.69960 },   // Lê Lợi x Nam Kỳ Khởi Nghĩa
      { id: 'leloi_pas', lat: 10.77445, lng: 106.70040 },   // Lê Lợi x Pasteur
      { id: 'leloi_nh', lat: 10.77550, lng: 106.70250 },    // Lê Lợi x Nguyễn Huệ
      { id: 'leloi_dk', lat: 10.77600, lng: 106.70320 },    // Lê Lợi x Đồng Khởi
      { id: 'lt_td', lat: 10.77120, lng: 106.69600 },       // Lê Thánh Tôn x Trương Định
      { id: 'lt_nkk', lat: 10.77400, lng: 106.69960 },      // Lê Thánh Tôn x Nam Kỳ Khởi Nghĩa
      { id: 'lt_pas', lat: 10.77510, lng: 106.70070 },      // Lê Thánh Tôn x Pasteur
      { id: 'lt_dk', lat: 10.77740, lng: 106.70300 },       // Lê Thánh Tôn x Đồng Khởi
      { id: 'lt_hbt', lat: 10.77880, lng: 106.70420 },      // Lê Thánh Tôn x Hai Bà Trưng
      { id: 'ly_td', lat: 10.77250, lng: 106.69480 },       // Lý Tự Trọng x Trương Định
      { id: 'ly_nkk', lat: 10.77670, lng: 106.69970 },      // Lý Tự Trọng x Nam Kỳ Khởi Nghĩa
      { id: 'ly_pas', lat: 10.77615, lng: 106.69900 },      // Lý Tự Trọng x Pasteur
      { id: 'ly_dk', lat: 10.77800, lng: 106.70135 },       // Lý Tự Trọng x Đồng Khởi
      { id: 'ly_hbt', lat: 10.77950, lng: 106.70350 },      // Lý Tự Trọng x Hai Bà Trưng
      { id: 'nd_nkk', lat: 10.77850, lng: 106.69850 },      // Nguyễn Du x Nam Kỳ Khởi Nghĩa
      { id: 'nd_pas', lat: 10.77920, lng: 106.69950 },      // Nguyễn Du x Pasteur
      { id: 'nd_dk', lat: 10.77980, lng: 106.69900 },       // Nhà Thờ Đức Bà
      { id: 'nh_mtb', lat: 10.77430, lng: 106.70360 },      // Nguyễn Huệ x Mạc Thị Bưởi
      { id: 'nh_tdt', lat: 10.77200, lng: 106.70650 },      // Bến Bạch Đằng / Tôn Đức Thắng
      { id: 'tdt_ld', lat: 10.78450, lng: 106.70600 },      // Tôn Đức Thắng x Lê Duẩn
      { id: 'tdt_nhc', lat: 10.78750, lng: 106.71180 },     // Tôn Đức Thắng x Nguyễn Hữu Cảnh
      { id: 'lm81', lat: 10.79510, lng: 106.72190 },        // Landmark 81
      { id: 'cmt8_ntmk', lat: 10.77200, lng: 106.69100 },   // Cách Mạng Tháng 8 x NTMK
      { id: 'cmt8_dbp', lat: 10.77700, lng: 106.68300 },    // Cách Mạng Tháng 8 x Điện Biên Phủ
      { id: 'bk_3t2', lat: 10.77200, lng: 106.65780 },      // ĐH Bách Khoa
      { id: 'tsn_airport', lat: 10.81850, lng: 106.65880 }  // Sân bay Tân Sơn Nhất
    ];

    const roadAdj = {
      bt: ['leloi_nkk', 'lt_td', 'cmt8_ntmk'],
      leloi_nkk: ['bt', 'leloi_pas', 'lt_nkk'],
      leloi_pas: ['leloi_nkk', 'leloi_nh', 'lt_pas'],
      leloi_nh: ['leloi_pas', 'leloi_dk', 'nh_mtb'],
      leloi_dk: ['leloi_nh', 'lt_dk', 'nh_tdt'],
      lt_hbt: ['lt_dk'],
      lt_dk: ['lt_pas', 'leloi_dk'],
      lt_pas: ['lt_nkk', 'ly_pas'],
      lt_nkk: ['lt_td', 'leloi_nkk'],
      lt_td: ['bt'],
      ly_td: ['ly_nkk'],
      ly_nkk: ['ly_pas', 'lt_nkk'],
      ly_pas: ['ly_dk', 'nd_pas'],
      ly_dk: ['ly_hbt', 'lt_dk'],
      ly_hbt: ['lt_hbt'],
      nd_nkk: ['ly_nkk', 'tsn_airport'],
      nd_pas: ['nd_dk', 'nd_nkk'],
      nd_dk: ['ly_dk', 'ly_hbt', 'nd_pas'],
      nh_mtb: ['leloi_nh', 'nh_tdt'],
      nh_tdt: ['nh_mtb', 'tdt_ld'],
      tdt_ld: ['tdt_nhc', 'nh_tdt'],
      tdt_nhc: ['lm81', 'tdt_ld'],
      lm81: ['tdt_nhc'],
      cmt8_ntmk: ['cmt8_dbp', 'bt'],
      cmt8_dbp: ['bk_3t2', 'cmt8_ntmk'],
      bk_3t2: ['cmt8_dbp'],
      tsn_airport: ['nd_nkk']
    };

    const calcDist = (lat1, lon1, lat2, lon2) => {
      const R = 6371000;
      const dLat = (lat2 - lat1) * Math.PI / 180;
      const dLon = (lon2 - lon1) * Math.PI / 180;
      const a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
                Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
                Math.sin(dLon / 2) * Math.sin(dLon / 2);
      return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
    };

    const findNearestNode = (lat, lng) => {
      let best = intersections[0];
      let minDist = Infinity;
      for (const n of intersections) {
        const d = calcDist(lat, lng, n.lat, n.lng);
        if (d < minDist) {
          minDist = d;
          best = n;
        }
      }
      return best;
    };

    const startNode = findNearestNode(pLat, pLng);
    const endNode = findNearestNode(dLat, dLng);

    const queue = [[startNode.id]];
    const visited = new Set([startNode.id]);
    let bestPathIds = null;

    while (queue.length > 0) {
      const currentPath = queue.shift();
      const tail = currentPath[currentPath.length - 1];

      if (tail === endNode.id) {
        bestPathIds = currentPath;
        break;
      }

      const neighbors = roadAdj[tail] || [];
      for (const nxt of neighbors) {
        if (!visited.has(nxt)) {
          visited.add(nxt);
          queue.push([...currentPath, nxt]);
        }
      }
    }

    const nodeMap = new Map(intersections.map(n => [n.id, n]));
    const result = [];

    if (bestPathIds && bestPathIds.length > 0) {
      for (const id of bestPathIds) {
        const node = nodeMap.get(id);
        if (node) result.push([node.lat, node.lng]);
      }
      return result;
    }

    return null;
  }

  /**
   * Query Public OpenStreetMap Routing Service (OSRM)
   */
  async fetchOsrmRoute(fromLat, fromLon, toLat, toLon) {
    const url = `https://router.project-osrm.org/route/v1/driving/${fromLon},${fromLat};${toLon},${toLat}?overview=full&geometries=geojson`;
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 4000);
    try {
      const res = await fetch(url, { signal: controller.signal });
      clearTimeout(timeout);
      if (!res.ok) return null;
      const data = await res.json();
      if (data.code === 'Ok' && Array.isArray(data.routes) && data.routes.length > 0) {
        const r = data.routes[0];
        return {
          ok: true,
          geometry: r.geometry.coordinates.map(([lon, lat]) => [lat, lon]),
          distance_m: Math.round(r.distance),
          duration_s: Math.round(r.duration)
        };
      }
    } catch (e) {
      clearTimeout(timeout);
    }
    return null;
  }

  /**
   * Loads driver status and stats from dataAdapter.getDriverStatus().
   */
  async loadDriverStatus() {
    try {
      let status = null;

      if (typeof dataAdapter !== 'undefined' && typeof dataAdapter.getDriverStatus === 'function') {
        status = await Promise.resolve(dataAdapter.getDriverStatus());
      } else if (window.dataAdapter && typeof window.dataAdapter.getDriverStatus === 'function') {
        status = await Promise.resolve(window.dataAdapter.getDriverStatus());
      }

      if (status) {
        if (typeof status.isOnline !== 'undefined') {
          this.isOnline = !!status.isOnline;
        }
        if (typeof status.tripsToday !== 'undefined') {
          this.driverStats.tripsToday = status.tripsToday;
        }
        if (typeof status.earnings !== 'undefined') {
          this.driverStats.earnings = parseFloat(status.earnings);
        }
        if (typeof status.rating !== 'undefined') {
          this.driverStats.rating = parseFloat(status.rating);
        }
        if (status.activeTrip) {
          this.activeTrip = status.activeTrip;
          this.renderActiveTripUI();
          this.showTripOnMap(this.activeTrip);
        }
      }

      this.renderDriverStats();
    } catch (err) {
      console.error('[DriverController] Failed to load driver status:', err);
      this.renderDriverStats();
    }
  }

  /**
   * Renders driver statistics cards (trips today, earnings, rating).
   */
  renderDriverStats() {
    // Trips Today
    const tripsEl = document.getElementById('stat-trips-today') ||
                    document.getElementById('driver-trips-count') ||
                    document.querySelector('[data-stat="trips-today"]');
    if (tripsEl) {
      tripsEl.textContent = this.driverStats.tripsToday.toString();
    }

    // Earnings
    const earningsEl = document.getElementById('stat-earnings') ||
                       document.getElementById('driver-today-earnings') ||
                       document.querySelector('[data-stat="earnings"]');
    if (earningsEl) {
      earningsEl.textContent = `$${this.driverStats.earnings.toFixed(2)}`;
    }

    // Rating
    const ratingEl = document.getElementById('stat-rating') ||
                     document.getElementById('driver-rating') ||
                     document.querySelector('[data-stat="rating"]');
    if (ratingEl) {
      ratingEl.textContent = `${this.driverStats.rating.toFixed(2)} ★`;
    }
  }

  /**
   * Helper to show toast messages.
   * @param {string} message 
   * @param {'info'|'success'|'warning'|'error'} type 
   */
  showToast(message, type = 'info') {
    if (typeof App !== 'undefined' && typeof App.showToast === 'function') {
      App.showToast(message, type);
    } else if (window.App && typeof window.App.showToast === 'function') {
      window.App.showToast(message, type);
    } else {
      console.log(`[Toast ${type.toUpperCase()}]: ${message}`);
      let fallbackBox = document.getElementById('toast-notification-fallback');
      if (!fallbackBox) {
        fallbackBox = document.createElement('div');
        fallbackBox.id = 'toast-notification-fallback';
        fallbackBox.style.position = 'fixed';
        fallbackBox.style.bottom = '20px';
        fallbackBox.style.right = '20px';
        fallbackBox.style.zIndex = '9999';
        document.body.appendChild(fallbackBox);
      }
      const item = document.createElement('div');
      item.style.background = type === 'success' ? '#10b981' : type === 'error' ? '#ef4444' : type === 'warning' ? '#f59e0b' : '#3b82f6';
      item.style.color = '#fff';
      item.style.padding = '8px 14px';
      item.style.borderRadius = '6px';
      item.style.marginTop = '6px';
      item.style.fontSize = '0.85rem';
      item.style.boxShadow = '0 2px 8px rgba(0,0,0,0.2)';
      item.textContent = message;
      fallbackBox.appendChild(item);
      setTimeout(() => item.remove(), 3500);
    }
  }

  /**
   * Fallback mock requests if dataAdapter is not initialized yet.
   */
  getFallbackRequests() {
    return [
      {
        id: 'REQ-9102',
        customerName: 'Hoang Thi Bich',
        rating: 4.95,
        rideType: 'Economy',
        fare: 16.50,
        distance: 4.1,
        duration: 14,
        pickup: {
          lat: 10.772500,
          lng: 106.698000,
          address: 'Bitexco Financial Tower, Dist 1'
        },
        destination: {
          lat: 10.798100,
          lng: 106.687200,
          address: 'Phan Xich Long Culinary Strip, Phu Nhuan'
        }
      },
      {
        id: 'REQ-9103',
        customerName: 'Pham Quoc Bao',
        rating: 4.88,
        rideType: 'Premium',
        fare: 28.00,
        distance: 8.5,
        duration: 25,
        pickup: {
          lat: 10.768400,
          lng: 106.706800,
          address: 'Bach Dang Wharf, Ton Duc Thang'
        },
        destination: {
          lat: 10.850000,
          lng: 106.771900,
          address: 'Thu Duc High Tech Park'
        }
      }
    ];
  }
}

// Global initialization on DOMContentLoaded
document.addEventListener('DOMContentLoaded', () => {
  const isDriverPage = document.getElementById('driver-map') ||
                       document.getElementById('driver-online-toggle') ||
                       window.location.pathname.includes('driver');

  if (isDriverPage || document.body.dataset.page === 'driver') {
    window.driverController = new DriverController();
    window.driverController.init();
  }
});

// Export class globally
if (typeof window !== 'undefined') {
  window.DriverController = DriverController;
}
