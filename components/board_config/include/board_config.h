#ifndef CZC_BOARD_CONFIG_H_
#define CZC_BOARD_CONFIG_H_

#include "driver/gpio.h"
#include "driver/uart.h"

namespace Board {

inline constexpr gpio_num_t  CC_RST_PIN   = GPIO_NUM_16;
inline constexpr gpio_num_t  CC_BSL_PIN   = GPIO_NUM_32;

inline constexpr uart_port_t RCP_UART     = UART_NUM_2;
inline constexpr gpio_num_t  RCP_UART_TX  = GPIO_NUM_4;
inline constexpr gpio_num_t  RCP_UART_RX  = GPIO_NUM_36;

inline constexpr uint32_t    BSL_BAUD     = 115200;
inline constexpr uint32_t    SPINEL_BAUD  = 921600;

} // namespace Board

#endif // CZC_BOARD_CONFIG_H_
