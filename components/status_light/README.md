# status_light

Event-driven status LED manager for the CZC firmware.

Controls three LEDs:

| LED | Colour | Driven by |
|-----|--------|-----------|
| Power | Green | ESP32 GPIO — `Board::LED_PWR_PIN` (GPIO 14) |
| Mode | Red | ESP32 GPIO — `Board::LED_MODE_PIN` (GPIO 12) |
| Zigbee | Yellow | CC2652P7 (RCP) — not an ESP32 GPIO; set via a vendor Z-Stack MT command over the RCP UART, see [Zigbee LED](#zigbee-led-cc2652p7-yellow) |

The two GPIO LEDs are driven directly by this component. The Zigbee LED is only
*arbitrated* here — the actual MT command is sent by `firmware_manager` / `zstack_mt`.

---

## Architecture

The component is a pure **event subscriber** — it has no public API beyond `init()`. All other components communicate with it by posting events into the default ESP event loop. The manager never needs to be passed around or referenced again after initialisation.

```
any component                 default event loop         StatusLightManager
     │                               │                          │
     │── esp_event_post(FLASHING) ──►│                          │
     │                               │── eventHandler() ───────►│
     │                               │                          │── currentState_ = FLASHING
     │                               │                          │
     │                               │   (every 100 ms)         │
     │                               │── timerCallback() ───────►│
     │                               │                          │── mapLedsToState()
     │                               │                          │── led_hal.setPwr/setMode
     │                               │◄── esp_event_post(RCP_LED_EVENT) ── (Zigbee LED, only on change)
     │                               │
     │                               │── ledEventHandler() ──► FirmwareManager → Z-Stack MT SET_LED
```

The Zigbee LED path is described in detail in [Zigbee LED](#zigbee-led-cc2652p7-yellow).

### Internal structure

| File | Role |
|------|------|
| `include/status_light_event.h` | Public — `LedState` enum + `STATUS_LED_EVENT` base (post into it) and `RCP_LED_EVENT` base (subscribe to it to drive the Zigbee LED). |
| `include/status_light_manager.h` | Public — `StatusLightManager` class. Include this in `main` to call `init()`. |
| `private_include/status_light_hal.h` | Private — raw GPIO control, not accessible outside the component. |
| `src/status_light_hal.cpp` | GPIO init, `setPwr()`, `setMode()`. |
| `src/status_light_manager.cpp` | Event handler, 100 ms timer, blink logic, `RCP_LED_EVENT` posting. |

---

## Initialisation

Call `init()` once in `main` after `esp_event_loop_create_default()`:

```cpp
#include "status_light_manager.h"

StatusLightManager statusLight;
ESP_ERROR_CHECK(statusLight.init());
```

That's all. The manager registers its own event handler and starts its internal timer.

---

## Posting state changes

Include `status_light_event.h` in any component that needs to report a state. No other header is needed.

```cpp
#include "status_light_event.h"

// Signal that flashing has started
esp_event_post(STATUS_LED_EVENT,
               static_cast<int32_t>(LedState::FLASHING),
               nullptr, 0, 0);

// Signal that the Thread stack is up
esp_event_post(STATUS_LED_EVENT,
               static_cast<int32_t>(LedState::THREAD_ACTIVE),
               nullptr, 0, 0);
```

> **Note:** `esp_event_post` is fire-and-forget and safe to call from any task or ISR (use `esp_event_isr_post` from an ISR context).

---

## LED states

States are prioritised: a higher-priority state blocks lower-priority events until the
higher-priority state is explicitly cleared by posting the appropriate resolved state.

| State | Priority | Power LED (green) | Mode LED (red) | Zigbee LED (yellow, CC2652) | When to post |
|-------|----------|-------------------|----------------|-----------------------------|--------------|
| `ERROR` | 9 (highest) | blink 3 Hz | blink 3 Hz | off | Unrecoverable fault |
| `FLASHING` | 8 | blink 3 Hz | blink 3 Hz | off | RCP or ESP firmware update in progress |
| `ZIGBEE_ERROR` | 7 | on | blink 3 Hz | off | Communication failure with ZigBee chip |
| `ZIGBEE_CONNECTING` | 6 | on | blink 1 Hz | off | ZigBee chip connection check at startup |
| `BOOTING` | 5 | blink 1 Hz | off | off | System starting up (default initial state) |
| `NETWORK_DOWN` | 4 | blink 1 Hz | off | off | No network connection available |
| `THREAD_ACTIVE` | 3 | on | off | off | Thread/OTBR stack running |
| `ZIGBEE_USB` | 2 | on | on | on | Zigbee mode, USB host connected |
| `ZIGBEE_HOST_WAIT` | 1 | blink 1 Hz | off | off | Zigbee mode, waiting for host application |
| `ZIGBEE_NET` | 0 (lowest) | on | off | on | Zigbee mode, network host connected |

The Zigbee LED is never blinked — it is only switched on/off, because every change costs a
blocking MT round-trip on the RCP UART (see below).

### Blink timing

The internal timer fires every **100 ms**. Blink patterns:
- **1 Hz** — 500 ms on / 500 ms off
- **~3 Hz** — 100 ms on / 200 ms off (≈ 3.3 Hz)

### Clearing a state

There is no explicit "clear" call. When a condition resolves, post the state that reflects the new situation:

```cpp
// Flash finished → post whatever the normal operating state is now
esp_event_post(STATUS_LED_EVENT,
               static_cast<int32_t>(LedState::THREAD_ACTIVE),
               nullptr, 0, 0);
```

Because the resolved state has a lower priority number, the manager will only accept it
once the higher-priority state is no longer being posted. The caller is responsible for
posting the correct follow-up state.

---

## Adding a new LED state

1. Add a value to the `LedState` enum in `status_light_event.h`. Choose a numeric value
   that reflects the desired priority relative to existing states.
2. Add a `case` branch in `StatusLightManager::mapLedsToState()` in
   `status_light_manager.cpp` defining the LED pattern for the new state.
3. Update the table in this README.

---

## Zigbee LED (CC2652P7, yellow)

The yellow Zigbee LED is physically wired to the CC2652P7 RCP, not to an ESP32 GPIO — it can
only be toggled by sending the RCP a vendor Z-Stack MT command over the RCP's UART
(`Board::RCP_UART`), the same link `ZigbeeProxyController` uses to relay traffic between the
RCP and the host. Because that UART is a single, exclusively-owned resource, this component
never talks to it directly.

Instead, `mapLedsToState()` derives the desired on/off value in the same `switch` that
drives the two GPIO LEDs, and posts it as a separate event, `RCP_LED_EVENT`, only when the
value actually changes:

```
StatusLightManager::mapLedsToState()        FirmwareManager
        │                                          │
        │── rcpLed = f(currentState) ──┐           │
        │   (posted only on change)    │           │
        │── esp_event_post(RCP_LED_EVENT) ────────►│
        │                                          │── ledEventHandler() [non-blocking]
        │                                          │   xQueueOverwrite(rcpLedQueue, …)
        │                                          │
        │                                          │── rcpLedTask (dedicated task)
        │                                          │   xQueueReceive(rcpLedQueue, …)
        │                                          │   protocol->setRcpLed(ledState)
```

`RCP_LED_EVENT`'s `event_id` carries only the already-arbitrated on/off value (0 or 1) —
subscribers don't need to know about `LedState` priority at all.

**Why a queue + dedicated task:** `esp_event`'s default loop runs every registered handler
for every event base on one shared task. `IProtocolController::setRcpLed()` blocks (up to
500 ms waiting for the RCP mutex, plus up to 1 s — `PACKET_WAIT_TIME_MS` in `zstack_mt` —
waiting for the MT response) — calling it directly from the event handler would stall every
other handler in the system, including `StatusLightManager`'s own handler for the two GPIO
LEDs. `FirmwareManager` keeps `ledEventHandler()` non-blocking (just an `xQueueOverwrite`
into a 1-slot mailbox queue) and does the actual blocking call from `rcpLedTaskFunc()`, a
small dedicated task.

**Which states light it** (see the `rcpLed` assignment in each `case` of
`mapLedsToState()` to change this):

| `currentState` | Zigbee LED |
|---|---|
| `ZIGBEE_NET` | on |
| `ZIGBEE_USB` | on |
| all others | off |

### The Z-Stack MT command

`ZstackMt::setLed()` (component `zstack_mt`) sends a standard MT frame on the RCP UART:

```
SOF   LEN   CMD0  CMD1  DATA[0]     DATA[1]      FCS
0xFE  0x02  0x27  0x0A  0x01        0x01 / 0x00  XOR(LEN..DATA)
                  ^     LED index   on / off
                  vendor SET_LED (SREQ, subsystem UTIL)
```

- `CMD0 = 0x27` is the SREQ type (`0x20`) ORed with the UTIL subsystem (`0x07`);
  `CMD1 = 0x0A` is the vendor `SET_LED` command ID.
- The FCS is the XOR of every byte after `SOF` (`ZstackMt::calcChecksum()`).
- `sendCmdAndWaitForResponse()` then waits up to `PACKET_WAIT_TIME_MS` (1 s) for the
  matching SRSP — same `CMD1`, `CMD0 + 0x40` (`0x67`) — and checks its single status byte;
  a non-zero status is logged as `SET_LED returned failure status` and returned as
  `ESP_ERR_INVALID_RESPONSE`.

`ZstackMt::setLed()` only sends the frame; it does **not** reboot the RCP, and it expects
the UART driver to already be installed by whoever owns the port (`ZigbeeProxyController`).

### Sending the command from the proxy

**Sending the actual command** happens in `firmware_manager`, via
`IProtocolController::setRcpLed(bool ledState)`:

| Controller | Behaviour |
|---|---|
| `ZigbeeProxyController` | Sends the vendor Z-Stack MT `SET_LED` command (`cmd0=0x27`, `cmd1=0x0A`, LED index 1) via `ZstackMt::setLed()`. Pauses `rcpToHostTask`/`hostToRcpTask` for the duration so the command/response frame isn't leaked into the host-bound proxy stream — does **not** stop the whole proxy or reboot the RCP. Guarded by a mutex shared with `resetRcp()`/`factoryReset()` so at most one RCP operation runs at a time; best-effort — returns `ESP_ERR_TIMEOUT` if the RCP is busy with one of those and gives up after 500 ms, or `ESP_ERR_INVALID_STATE` if the proxy isn't running. |
| `ThreadController` | `ESP_ERR_NOT_SUPPORTED` — the CC2652 has no RCP-hosted LED in Thread/OpenThread mode. |

Ported from the legacy Arduino firmware's `CCTools::ledToggle()` / `CommandInterface::_ledToggle()`
(command bytes `zigLed1On`/`zigLed1Off`). The legacy version relied on Arduino's
single-threaded `loop()` to implicitly serialise the LED command against the proxy relay —
here, with real preemptive FreeRTOS tasks, that serialisation is done explicitly via the
mutex and the relay-task pause described above.
