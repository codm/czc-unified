#ifndef CZC_BOARD_CONFIG_H_
#define CZC_BOARD_CONFIG_H_

#include "driver/gpio.h"
#include "driver/uart.h"

namespace Board {

// --- RCP (CC2652P7) ---
inline constexpr gpio_num_t  CC_RST_PIN   = GPIO_NUM_16;
inline constexpr gpio_num_t  CC_BSL_PIN   = GPIO_NUM_32;

inline constexpr uart_port_t RCP_UART     = UART_NUM_2;
inline constexpr gpio_num_t  RCP_UART_TX  = GPIO_NUM_4;
inline constexpr gpio_num_t  RCP_UART_RX  = GPIO_NUM_36;

inline constexpr uint32_t    BSL_BAUD     = 115200;
inline constexpr uint32_t    COORD_BAUD   = 115200;
inline constexpr uint32_t    SPINEL_BAUD  = 921600;

// --- Status LEDs (active HIGH) ---
inline constexpr gpio_num_t  LED_MODE_PIN = GPIO_NUM_12;  // red
inline constexpr gpio_num_t  LED_PWR_PIN  = GPIO_NUM_14;  // green

// --- UART host-side mux (active HIGH → USB, LOW → network) ---
inline constexpr gpio_num_t  UART_SEL_PIN = GPIO_NUM_33;

// --- User button (active LOW) ---
inline constexpr gpio_num_t  BTN_PIN      = GPIO_NUM_35;

// --- Outside USB-C Uart (UART0, connected to host via USB-to-serial) ---
inline constexpr uart_port_t HOST_UART    = UART_NUM_0;
inline constexpr gpio_num_t  HOST_UART_TX = GPIO_NUM_1;
inline constexpr gpio_num_t  HOST_UART_RX = GPIO_NUM_3;

// --- TCP proxy ---
inline constexpr uint16_t    PROXY_TCP_PORT = 6638;

} // namespace Board

#endif // CZC_BOARD_CONFIG_H_
