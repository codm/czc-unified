# esp_ot_br_server

Embedded HTTP server providing the OpenThread Border Router REST API and the Web GUI.
Based on the [Espressif OTBR example](https://github.com/espressif/esp-idf/blob/master/examples/openthread/ot_br/README.md),
extended with custom firmware-update endpoints and a Wi-Fi configuration portal.

---

## File Structure

```
esp_ot_br_server/
├── src/
│   ├── esp_br_web.c            HTTP server setup, route table and all request handlers
│   ├── esp_br_web_api.c        OpenThread REST resource implementations
│   ├── esp_br_web_base.c       JSON ↔ OpenThread struct (de)serialization helpers
│   ├── esp_br_wifi_config.c    Wi-Fi configuration web server (captive-portal submit flow)
│   └── openapi.yaml            OpenAPI spec for the inherited REST endpoints
│
├── include/
│   ├── esp_br_web.h            Public API: esp_br_web_start(), system_flash_callbacks_t
│   └── esp_br_wifi_config.h    Public API: wifi config start/stop/get
│
├── private_include/
│   ├── esp_br_web_api.h        Internal: REST handler declarations
│   └── esp_br_web_base.h       Internal: JSON/struct helper declarations
│
├── frontend/
│   ├── index.html              Main Web GUI
│   ├── wifi_configuration.html Wi-Fi setup page (also embedded into the binary as fallback)
│   └── static/
│       ├── restful.js          Frontend JS for the Web GUI
│       └── style.css           Stylesheet
│
├── favicon.ico                 Favicon — embedded directly into the binary via EMBED_FILES
├── gzip_frontend.py            Build helper: compresses all frontend assets with gzip level 9
└── CMakeLists.txt              Component build definition
```

---

## Source Files

### `src/esp_br_web.c`

Core of the component. Responsibilities:

- Starts and stops the `esp_http_server` instance.
- Registers all URI handlers (REST API + Web GUI + static file fallback).
- Serves static files (HTML, CSS, JS) from SPIFFS with `Content-Encoding: gzip`.
- Serves the favicon from the embedded binary symbol.
- Holds `s_flash_cbs` — the callback struct that bridges the C webserver to the C++ `AppController`.
- Listens on `IP_EVENT_STA_GOT_IP` and `IP_EVENT_ETH_GOT_IP` to auto-start the server once the device has an IP address.

### `src/esp_br_web_api.c`

Implements each OpenThread REST handler function declared in `esp_br_web.h`. Every handler acquires the OpenThread lock, queries the OpenThread instance, and serialises the result to JSON. Covers: node info, RLOC, state, ext address, network name, leader data, router count, ext PAN ID, border agent ID, active/pending dataset, diagnostics, topology, commissioning.

Also implements the two custom firmware-update handlers (`/flash/esp`, `/flash/rcp`) by invoking the appropriate callback from `s_flash_cbs`.

### `src/esp_br_web_base.c`

Stateless serialization/deserialization helpers shared by `esp_br_web_api.c`:

- `hex_to_string` / `string_to_hex` — byte array ↔ hex string conversion.
- `otbr_properties_struct_convert2_json` — OpenThread properties → JSON.
- `avaiable_network_struct_convert2_json` — scanned Thread network → JSON.
- `network_formation_param_json_convert2_struct` / `network_join_param_json_convert2_struct` — form/join request body → struct.
- `dailnosticTlv_set_convert2_json` — diagnostic TLV set → JSON topology array.
- `ActiveDataset2Json` / `Json2ActiveDataset` and pending dataset equivalents.
- `ot_br_web_response_code_get` / `convert_ot_err_to_response_code` — OT error → HTTP status string.

### `src/esp_br_wifi_config.c`

A minimal captive-portal web server used during the initial Wi-Fi setup flow. Runs independently of the main Thread BR server.

Provides:
- `GET /` — serves `wifi_configuration.html` (from SPIFFS or embedded fallback).
- `POST /submit` — receives `{ "ssid": "...", "password": "..." }`, stores credentials in memory, and signals an `EventGroup` bit so the caller (`NetworkStateMachine`) can retrieve them via `esp_br_wifi_config_get_configured_wifi()`.
- Captive-portal redirect handlers for iOS/Android/Windows connectivity checks.
- Icon handlers (204 No Content) to suppress 404 warnings.

SoftAP setup, DNS server, and Wi-Fi scanning have been removed — those responsibilities belong to the `network` component's state machine.

---

## Build Process

### Gzip Compression

All frontend assets are compressed with **gzip level 9** at build time to reduce SPIFFS footprint and browser transfer size. Typical results:

| File | Original | Compressed | Ratio |
|---|---|---|---|
| `wifi_configuration.html` | 10.3 KB | 2.4 KB | 23% |
| `index.html` | 36.6 KB | 8.5 KB | 23% |
| `static/restful.js` | 36.9 KB | 7.1 KB | 19% |
| `static/style.css` | 25.6 KB | 5.6 KB | 22% |

The compression is driven by [`gzip_frontend.py`](gzip_frontend.py) and is invoked from `CMakeLists.txt` in two ways:

1. **Configure time** (`execute_process`) — runs immediately when CMake processes the component, so the compressed files exist before `EMBED_FILES` tries to embed `wifi_configuration.html`.
2. **Build time** (`add_custom_command`) — re-runs automatically whenever a source file under `frontend/` changes, with the stamp file `build/frontend_gz/.stamp` tracking freshness.

### Where the files end up

| File(s) | Mechanism | Location in flash |
|---|---|---|
| `favicon.ico` | `EMBED_FILES` → binary symbol `_binary_favicon_ico_*` | App binary (`.rodata`) |
| `wifi_configuration.html` (gzipped) | `EMBED_FILES` → binary symbol `_binary_wifi_configuration_html_*` | App binary (`.rodata`) |
| `index.html`, `restful.js`, `style.css` (gzipped) | `spiffs_create_partition_image` | SPIFFS partition (`0x3F2000`, 56 KB) |

### Serving gzip files

All file handlers in `esp_br_web.c` and `wifi_config_index_handler` set the `Content-Encoding: gzip` response header before sending. Modern browsers decompress transparently.

The file reading uses `fopen("rb")` and `httpd_resp_send_chunk(req, buf, len)` (binary-safe) instead of the string-based `httpd_resp_sendstr_chunk` — this is required because gzip data contains null bytes that would otherwise truncate the response.

---

## API Endpoints

### Inherited REST API

The full set of OpenThread resource endpoints is documented in [`src/openapi.yaml`](src/openapi.yaml).

### Custom Endpoints

This project adds two endpoints for remote firmware updates:

| Endpoint | Method | Purpose | Request body | Response |
|---|---|---|---|---|
| `/flash/esp` | `POST` | Starts an OTA update of the ESP32 itself | `{ "url": "<firmware.ota.bin>" }` | `{ "status": "flashing", "message": "..." }` |
| `/flash/rcp` | `POST` | Schedules a firmware update of the CC2652 RCP; target mode written to NVS and applied after the flash completes on next boot | `{ "url": "<rcp_firmware.bin>", "type": <DeviceMode int> }` | `{ "status": "scheduled", "reboot": true }` |

> **Testing:** A browser address bar cannot send POST requests. Use e.g. `curl`:
> ```bash
> curl -X POST http://<device-ip>/flash/rcp \
>      -H "Content-Type: application/json" \
>      -d '{"url": "https://example.com/rcp_firmware.bin"}'
> ```

### Bridging the C Webserver and the C++ Application

The webserver (`esp_br_web.c`) is plain C, and all its handlers share the fixed signature
`esp_err_t handler(httpd_req_t *req)` — there is no room for extra parameters. The
`System_manager`, which actually performs the flashing, lives in C++ and is invisible to
the webserver. The two are connected through a small callback struct defined in
[`include/esp_br_web.h`](include/esp_br_web.h):

```c
typedef struct {
    esp_err_t (*flash_esp)(void *ctx, const char *url);
    esp_err_t (*flash_rcp)(void *ctx, const char *url, int mode);
    void *ctx;   // opaque pointer, cast back to AppController* in the adapters
} web_firmware_callbacks_t;
```

**How the struct travels through the program:**

1. `AppController::fillFirmwareCallbacks()` builds a `web_firmware_callbacks_t`, pointing
   the function pointers at two `static` adapter functions and setting `ctx = this`.
2. `esp_br_web_start(base_path, flash_cbs, net_cbs)` copies the struct **by value** into a
   static, file-scope variable `s_fw_cbs` — a copy is required because the original struct
   lives on the caller's stack and would be invalid once that function returns.
3. The HTTP handlers `esp_otbr_flash_esp_post_handler` / `esp_otbr_flash_rcp_post_handler`
   parse `{"url": ..., "type": ...}` from the request body and call
   `s_fw_cbs.flash_esp(s_fw_cbs.ctx, url)` / `s_fw_cbs.flash_rcp(s_fw_cbs.ctx, url, mode)`.
4. The static adapter functions cast `ctx` back to `AppController*` and call
   `requestEspFlash(url)` / `requestRcpFlash(url, mode)`.

**Why `flash_rcp` carries the mode:** The target `DeviceMode` is written atomically to NVS
alongside the URL and pending flag inside `requestRcpFlash`. This avoids calling `set_mode`
before the flash, which would attempt a live firmware-manager restart with the wrong RCP
firmware and could crash the HTTP server task before the response is sent.

This keeps the webserver completely free of `System_manager`/C++ knowledge while still
letting it trigger application-level actions. The same pattern can be reused for future
endpoints by extending `system_flash_callbacks_t`.
