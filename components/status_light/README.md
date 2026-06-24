# status_light

Event-driven status LED manager for the CZC firmware.

Controls two GPIO LEDs:
| LED | Colour | GPIO |
|-----|--------|------|
| Power | Green | `Board::LED_PWR_PIN` (GPIO 14) |
| Mode | Red | `Board::LED_MODE_PIN` (GPIO 12) |

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
```

### Internal structure

| File | Role |
|------|------|
| `include/status_light_event.h` | Public — `LedState` enum + `STATUS_LED_EVENT` base. Include this to post events. |
| `include/status_light_manager.h` | Public — `StatusLightManager` class. Include this in `main` to call `init()`. |
| `private_include/status_light_hal.h` | Private — raw GPIO control, not accessible outside the component. |
| `src/status_light_hal.cpp` | GPIO init, `setPwr()`, `setMode()`. |
| `src/status_light_manager.cpp` | Event handler, 100 ms timer, blink logic. |

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

| State | Priority | Power LED (green) | Mode LED (red) | When to post |
|-------|----------|-------------------|----------------|--------------|
| `ERROR` | 9 (highest) | blink 3 Hz | blink 3 Hz | Unrecoverable fault |
| `FLASHING` | 8 | blink 3 Hz | blink 3 Hz | RCP or ESP firmware update in progress |
| `ZIGBEE_ERROR` | 7 | on | blink 3 Hz | Communication failure with ZigBee chip |
| `ZIGBEE_CONNECTING` | 6 | on | blink 1 Hz | ZigBee chip connection check at startup |
| `BOOTING` | 5 | blink 1 Hz | off | System starting up (default initial state) |
| `NETWORK_DOWN` | 4 | blink 1 Hz | off | No network connection available |
| `THREAD_ACTIVE` | 3 | on | off | Thread/OTBR stack running |
| `ZIGBEE_USB` | 2 | on | on | Zigbee mode, USB host connected |
| `ZIGBEE_HOST_WAIT` | 1 | blink 1 Hz | off | Zigbee mode, waiting for host application |
| `ZIGBEE_NET` | 0 (lowest) | on | off | Zigbee mode, network host connected |

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

## ZigBee LED (yellow)

The yellow ZigBee LED is physically connected to the CC2652P7 and is **not** controlled
by this component. It is driven by the ZigBee firmware itself and can be toggled via
ZStack MT commands from `ZigbeeProxyController`.
