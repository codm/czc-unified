#ifndef ZSTACK_NVRAM_ADDR_H_
#define ZSTACK_NVRAM_ADDR_H_

#include <cstdint>
#include <array> 

namespace OsalNv
{
    inline constexpr uint16_t BruteForceMax = 0x03FF;

    inline constexpr uint16_t HAS_CONFIGURED_ZSTACK1 = 0x0F00; 
    inline constexpr uint16_t APP_ITEM_1 = 0x0F01;            
    inline constexpr uint16_t APP_ITEM_2 = 0x0F02;
    inline constexpr uint16_t APP_ITEM_3 = 0x0F03;
    inline constexpr uint16_t APP_ITEM_4 = 0x0F04;
    inline constexpr uint16_t APP_ITEM_5 = 0x0F05;
    inline constexpr uint16_t APP_ITEM_6 = 0x0F06;
    inline constexpr uint16_t RF_TEST_PARMS = 0x0F07;

    inline constexpr std::array<uint16_t, 8> ExtraIds = {
        HAS_CONFIGURED_ZSTACK1,
        APP_ITEM_1, APP_ITEM_2, APP_ITEM_3, APP_ITEM_4, APP_ITEM_5, APP_ITEM_6,
        RF_TEST_PARMS
    };
}

#endif // ZSTACK_NVRAM_ADDR_H_