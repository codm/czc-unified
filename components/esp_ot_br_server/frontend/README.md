# Frontend

Single-page web application served from the SPIFFS partition of the ESP32. All assets are gzip-compressed at build time and served with `Content-Encoding: gzip`.

---

## Technology Stack

| Tool | Purpose |
|---|---|
| Vanilla JavaScript | All logic — no framework, no build step |
| jQuery 3.x | AJAX helpers and DOM queries |
| Bootstrap 3.3.7 | Responsive grid and basic components |
| Mermaid | Diagrams (not used at runtime, only in docs) |

---

## File Structure

```
frontend/
├── index.html              Main SPA — all sections live in one file
├── wifi_configuration.html SoftAP captive-portal page (also embedded as fallback binary)
└── static/
    ├── restful.js          All frontend logic
    └── style.css           All styles (minified single line)
```

---

## App State and Device Modes

`restful.js` maintains global mode state in three arrays at the top of the file:

```javascript
var MODE_NAMES  = ['Thread OTBR', 'Zigbee Coordinator USB', 'Zigbee Coordinator Net', 'Zigbee Router'];
var MODE_ICONS  = ['icon-thread',  'icon-zigbee', 'icon-zigbee', 'icon-zigbee'];
var MODE_GROUPS = ['thread',       'zigbee',       'zigbee',      'zigbee'];
```

Mode indices map to `DeviceMode` enum values on the backend:

| Index | Mode |
|---|---|
| 0 | Thread OTBR |
| 1 | Zigbee Coordinator USB |
| 2 | Zigbee Coordinator Network/TCP |
| 3 | Zigbee Router |

`applyDeviceMode(mode)` is called on every page load and after any mode change. It adds CSS classes to `<body>` which drive all conditional visibility:

| CSS class on `<body>` | When set |
|---|---|
| `mode-thread` | Mode 0 |
| `mode-zigbee` | Modes 1, 2, 3 |
| `mode-coordinator` | Modes 1, 2 |

Sections and sidebar links use matching CSS classes to show or hide:

```css
.thread-section   { display: none; }  body.mode-thread .thread-section   { display: block; }
.zigbee-section   { display: none; }  body.mode-zigbee .zigbee-section   { display: block; }
.coordinator-section { display: none; } body.mode-coordinator .coordinator-section { display: block; }
```

---

## First Boot — Mode Selection Dialog

On every page load, `initFirstBootCheck()` calls `GET /device/mode`. If the response contains `device_setup: false`, the device has never been configured.

```
GET /device/mode
    │
    ├── device_setup: true  → normal startup, call applyDeviceMode()
    │
    └── device_setup: false → show #internet-waiting-overlay
                                  │
                                  └── poll GET /network/status every 2 s
                                            │
                                            └── connected: true
                                                    │
                                                    └── hide overlay
                                                        show #mode-selection-modal
```

The mode selection modal shows four cards (one per mode). Clicking a card calls `selectMode(n)` which highlights it and shows a confirm bar at the bottom.

`confirmModeSelection()` sends:
```
POST /device/mode  { "mode": n }
```

The backend flashes the matching RCP firmware and reboots. The page waits 20 seconds then reloads.

---

## Navigation and Section Visibility

The sidebar links correspond to `<section id="...">` elements. Clicking a link scrolls the page to that section.

Thread-only sections (hidden in Zigbee mode):
- Scan, Form, Settings, Status, Topology

Zigbee-only sections (hidden in Thread mode):
- MQTT, Security

Coordinator-only sections (hidden in Router and Thread mode):
- ZigbeeConnection

Always visible:
- Overview, Network, Firmware, Debug

---

## RCP Flash Dialog

Opened by `frontend_flash_rcp_button()`. The dialog contains three tabs:

| Tab | Firmware source | Mode sent on flash |
|---|---|---|
| Thread | GitHub Releases API (`czc-ot-rcp-fw`) | 0 |
| Zigbee Coordinator | `manifest.json` → `coordinator` key | 1 (USB) or 2 (NET) |
| Zigbee Router | `manifest.json` → `router` key | 3 |

### Manifest format

The Zigbee manifest (`ZB_MANIFEST_URL`) has this structure:

```json
{
  "coordinator": {
    "CC2652P7": {
      "filename.bin": { "ver": "20250403", "link": "https://...", "baud": "115200" }
    }
  },
  "router": { ... },
  "thread": {}
}
```

`parse_zb_manifest(data, type)` iterates `data[type]` → device → entries and builds a flat firmware list.

### Coordinator transport toggle

When the Coordinator tab is active, a sub-row appears with two buttons: **USB / UART** and **Network / TCP**. This sets `g_coordinator_mode` (1 or 2), which determines which mode is sent alongside the flash.

### Flash flow

`do_flash_with_url(url)` performs two requests in order:

1. `POST /device/mode  { "mode": n }` — saves the mode to NVS before reboot
2. `POST /flash/rcp   { "url": "..." }` — downloads and flashes the RCP binary, then reboots

---

## Adding New Sections

### 1. Add the HTML section

All sections live in `index.html` as `<section>` elements inside the main content area. The minimum structure is:

```html
<section id="MyFeature" class="sub-section VISIBILITY-CLASS">
  <div class="container">
    <div class="section-header default-margin">
      <h2>My <span style="color: var(--color-primary)">Feature</span></h2>
      <p>Short description shown below the heading.</p>
    </div>
    <div class="part">
      <div class="submit-form">
        <!-- content here -->
      </div>
    </div>
  </div>
</section>
```

Replace `VISIBILITY-CLASS` with one of the classes from the table below — or omit it entirely if the section should always be visible.

### 2. Add a sidebar link (optional)

Sidebar links live in the `<ul class="sidebar-nav">` in `index.html`. Add an `<li>` with the same visibility class:

```html
<li class="VISIBILITY-CLASS">
  <a href="#MyFeature">
    <svg class="icon-stroke"><use href="#icon-settings"/></svg>
    My Feature
  </a>
</li>
```

### 3. Visibility classes

| Class | When visible | Use for |
|---|---|---|
| *(none)* | Always | Network, Firmware, Debug, Overview |
| `thread-section` | Thread mode only (mode 0) | Scan, Form, OpenThread settings, Topology |
| `zigbee-section` | Any Zigbee mode (modes 1, 2, 3) | MQTT, Security, anything Zigbee-specific |
| `coordinator-section` | Coordinator only (modes 1 and 2) | Connection type toggle, host config |

These classes work through CSS rules in `style.css`:

```css
.thread-section      { display: none; }   body.mode-thread      .thread-section      { display: block; }
.zigbee-section      { display: none; }   body.mode-zigbee      .zigbee-section      { display: block; }
.coordinator-section { display: none; }   body.mode-coordinator .coordinator-section { display: block; }
```

The body class is set by `applyDeviceMode(mode)` in `restful.js` on every page load.

### 4. Adding a new mode-specific body class

If you need a visibility class that isn't covered above (e.g., router-only), add two things:

**`style.css`** — append:
```css
.router-section{display:none}body.mode-router .router-section{display:block}
```

**`restful.js`** — in `applyDeviceMode()`, add the class when appropriate:
```javascript
function applyDeviceMode(mode) {
  document.body.classList.remove('mode-thread', 'mode-zigbee', 'mode-coordinator', 'mode-router');
  // ... existing logic ...
  if (mode === 3) document.body.classList.add('mode-router');
}
```

Then use `class="sub-section router-section"` on the section.

### 5. Add JavaScript logic

Write your functions in `restful.js`. For sections that load data from the backend, use the same lazy-load pattern as the Network section if the data is only needed when the user scrolls there:

```javascript
// In the $(document).ready block at the bottom of restful.js:
var mySection = document.getElementById('MyFeature');
if (mySection) {
  var loaded = false;
  new IntersectionObserver(function(entries) {
    if (entries[0].isIntersecting && !loaded) {
      loaded = true;
      loadMyFeatureData();
    }
  }).observe(mySection);
}
```

---

## Network Configuration

The Network section loads lazily on first scroll into view (IntersectionObserver). It calls `loadNetworkConfig('wifi')` and `loadNetworkConfig('ethernet')`, which each call the respective `GET /network/wifi` or `GET /network/ethernet` endpoint and populate the form.

DHCP/static toggle is handled by `toggleStaticIpFields(prefix)`, which enables or disables the static IP input fields.

Save buttons call `saveNetworkConfig(type)`, which sends:
```
POST /network/wifi      { ssid, password, dhcp, ip, gateway, dns }
POST /network/ethernet  { dhcp, ip, gateway, dns }
```

---

## Zigbee Connection Section

Visible only in coordinator mode (modes 1 and 2). Shows two buttons: **USB / UART** and **Network / TCP**.

`applyDeviceMode()` sets the active button based on the current mode. Clicking a button calls `setZigbeeTransport(mode)`:

1. Updates button active states
2. Shows status text "Switching — device will reboot..."
3. Sends `POST /device/mode { "mode": 1 or 2 }`
4. On response: shows "Rebooting..." and reloads after 15 seconds

---

## API Endpoints Used by the Frontend

| Method | Path | Used for |
|---|---|---|
| `GET` | `/device/mode` | First boot check, current mode display |
| `POST` | `/device/mode` | First boot setup, Zigbee transport switch, pre-flash mode save |
| `GET` | `/network/status` | Polling for internet connectivity (first boot) |
| `GET` | `/network/wifi` | Load WiFi config form |
| `POST` | `/network/wifi` | Save WiFi config |
| `GET` | `/network/ethernet` | Load Ethernet config form |
| `POST` | `/network/ethernet` | Save Ethernet config |
| `POST` | `/flash/rcp` | Flash CC2652 RCP with a specific firmware URL |
| `POST` | `/flash/esp` | OTA update of the ESP32 |
| `GET` | `/get_properties` | OpenThread network properties (Thread mode) |
| `GET` | `/node` | OpenThread node info |
| `GET` | `/topology` | Thread network topology graph |
