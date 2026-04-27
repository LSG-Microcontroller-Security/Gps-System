#pragma once
#include <stdint.h>

class AppConfig {
public:
    static constexpr uint8_t gps_software_serial_rx_pin = 8U;
    static constexpr uint8_t gps_software_serial_tx_pin = 7U;
    static constexpr unsigned long gps_serial_baud_rate = 9600UL;

    static constexpr uint8_t sim800c_software_serial_rx_pin = 5U;
    static constexpr uint8_t sim800c_software_serial_tx_pin = 11U;
    static constexpr unsigned long sim800c_serial_baud_rate = 9600UL;
};
