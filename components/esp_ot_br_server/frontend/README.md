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

`confirmModeSelection()` resolves the newest RCP firmware URL for the selected mode, marks the modal as flashing (`dataset.flashing = 'true'`) and hides it, then sends:
```
POST /flash/rcp  { "url": "<rcp_firmware.bin>", "type": n }
```

The RCP is flashed **live — no ESP reboot**. Progress and completion are reported via the
SSE `device_state` / `flash_progress` / `flash_complete` events (see [SSE
Events](../../../README.md#sse-events) in the top-level README), which drive the same
`flash_window` modal used for manual re-flashes. `onFlashComplete()` checks the modal's
`flashing` marker and reloads the page ~1.5s after a successful setup flash so the UI
picks up the newly active mode.

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

### Firmware info fetchers

Two functions fetch and parse firmware metadata into a common shape:

```js
// FirmwareEntry
{
  version:    string  // release tag or version string, e.g. "V1.0.0" / "20250403"
  link:       string  // direct .bin download URL
  notes_link: string  // URL of the release page or changelog
}
```

| Function | Source | `notes_link` |
|---|---|---|
| `fetch_github_firmwares(url)` | GitHub Releases API | `html_url` of the release |
| `fetch_manifest_firmwares(url, mode)` | Manifest JSON | first URL extracted from `notes` field |

Both return a jQuery Promise. Usage:

```js
fetch_github_firmwares(ESP_RELEASES_URL)
  .done(function(list) { /* list is FirmwareEntry[] */ })
  .fail(onError);

fetch_manifest_firmwares(ZB_MANIFEST_URL, 'coordinator')
  .done(function(list) { ... });
```

The `notes` field in the manifest can be a bare URL (`https://...`) or a Markdown link (`[text](url)`). `manifest_extract_url()` handles both cases.

### Manifest format

The Zigbee manifest (`ZB_MANIFEST_URL`) has this structure:

```json
{
  "coordinator": {
    "CC2652P7": {
      "filename.bin": { "ver": "20250403", "link": "https://...", "notes": "https://...", "baud": "115200" }
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

Each firmware-list row's button calls either `do_esp_flash_with_url(url)` or
`do_rcp_flash_with_url(url, mode)`, depending on which chip the row is for.

- **RCP** — sends `POST /flash/rcp { "url": "...", "type": mode }` and hides the picker.
  Flashing is live (no ESP reboot); the SSE-driven `flash_window` modal
  (`device_state`/`flash_progress`/`flash_complete`) shows progress and the result, then
  hides itself again — see [First Boot](#first-boot--mode-selection-dialog) above for the
  same mechanism.
- **ESP** — sends `POST /flash/esp { "url": "..." }`. This *does* still reboot the device
  (a real ESP-IDF OTA update), so on a successful response `do_esp_flash_with_url()` calls
  `pollUntilOnline()` to wait for the device to come back, then reloads the page.

---

## Adding New Sections

### 1. Add the HTML section

All sections live in `index.html` as `<section>` elements inside the main content area. The minimum structure is:

```html
<section id="MyFeature"
         class="page-section hidden"
         data-modes="thread"
         data-nav-label="My Feature"
         data-nav-icon="icon-settings">
  <div class="container">
    <div class="section-header default-margin">
      <h2>My <span class="text-primary">Feature</span></h2>
      <p>Short description shown below the heading.</p>
    </div>
    <div class="content-panel">
      <div class="form-card">
        <!-- content here -->
      </div>
    </div>
  </div>
</section>
```

- `class="page-section hidden"` — always include `hidden` for mode-specific sections; omit it for sections that are always visible.
- `data-modes` — controls when the section is shown (see table below).
- `data-nav-label` — the label that appears in the sidebar navigation link.
- `data-nav-icon` — the icon id (from the SVG sprite) for the sidebar link. Omit to show the link without an icon.

### 2. Sidebar navigation

The sidebar is built automatically from the `data-nav-label` and `data-nav-icon` attributes of all visible sections. **No manual HTML changes needed** — `buildSidebar()` in `restful.js` generates the links after the device mode is fetched.

### 3. Visibility — data-modes values

| Value | When visible |
|---|---|
| `all` | Always (Network, Firmware, Debug, Overview) |
| `thread` | Thread OTBR mode only (mode 0) |
| `zigbee` | Any Zigbee mode (modes 1, 2, 3) |
| `coordinator` | Zigbee Coordinator only (modes 1 and 2) |

Multiple values can be combined with a space: `data-modes="thread coordinator"`.

### 4. Adding a new mode

To add a mode not covered above, extend `sectionVisible()` in `restful.js`:

```javascript
function sectionVisible(section, group, isCoordinator) {
  var modes = (section.dataset.modes || 'all').split(' ');
  return modes.some(function(m) {
    if (m === 'all')         return true;
    if (m === 'thread')      return group === 'thread';
    if (m === 'zigbee')      return group === 'zigbee';
    if (m === 'coordinator') return isCoordinator;
    if (m === 'router')      return group === 'zigbee' && !isCoordinator; // example
    return false;
  });
}
```

Then use `data-modes="router"` on the section.

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
| `POST` | `/device/mode` | Zigbee transport switch (coordinator USB &lt;-&gt; Net) |
| `POST` | `/device/loglevel` | Debug page — set ESP32 log level (`{"mode": <int>}`, 1=Error .. 5=Verbose) |
| `GET` | `/network/status` | Polling for internet connectivity (first boot) |
| `GET` | `/network/wifi` | Load WiFi config form |
| `POST` | `/network/wifi` | Save WiFi config |
| `GET` | `/network/ethernet` | Load Ethernet config form |
| `POST` | `/network/ethernet` | Save Ethernet config |
| `POST` | `/flash/rcp` | Flash CC2652 RCP live with a specific firmware URL + mode — no reboot; used for first-boot setup and manual re-flash |
| `POST` | `/flash/esp` | OTA update of the ESP32 — reboots on success |
| `GET` | `/get_properties` | OpenThread network properties (Thread mode) |
| `GET` | `/node` | OpenThread node info |
| `GET` | `/topology` | Thread network topology graph |
