#pragma once

#include <stdint.h>
#include <mf_commons_commonsLayer.h>
#include "../AppConfig.h"
#include "mf_repository_EepromRepository.h"
#include "mf_repository_SimRepository.h"
#include "mf_activity_NmeaGpsActivity.h"

#if _ON_MOCKING_TESTS && _DEBUG_FOR_SERIAL
#error "_DEBUG_FOR_SERIAL must be 0 when _ON_MOCKING_TESTS is 1"
#endif

class GpsSystem_BL {
public:
    struct GpsSmsData {
        int32_t latitude_x1000000;
        int32_t longitude_x1000000;
        int32_t altitude_m_x10;
        int32_t speed_knots_x100;
    };

    explicit GpsSystem_BL(NmeaGpsActivity* gps_activity);
    GpsSystem_BL(SimRepository* sim_repository, EepromRepository* eeprom_repository);
    ~GpsSystem_BL() = default;
    bool read_and_log_coordinates(GpsConnectionStatus& out_status);
    void call_phone_number();
    void send_sms(const GpsSmsData& gps_sms_data);
    bool check_if_sms_arrived();
    bool read_sms(char* out_message, uint8_t out_message_size);
    bool set_sim_netlight_enabled(bool enabled);
    void delete_all_sms();
    void analyze_sms_message(const char* message, const GpsConnectionStatus& gps_status);
    void hang_up_phone_call();
    void startSmsReception();
    // Provisional tilt-filter parameters: calibrate after hardware measurements.
    // Eight observation slots fit into one byte of motion history.
    static constexpr uint8_t movement_observation_intervals = 8U;
    static constexpr uint16_t movement_observation_interval_ms = 500U;
    static constexpr uint8_t movement_minimum_active_intervals = 3U;
    static constexpr uint8_t movement_minimum_spread_intervals = 4U;
    static bool is_movement_confirmed(uint8_t active_intervals_mask);
    static bool get_if_interrupt_enabled();
    static bool get_if_gps_flow_enabled();
    static bool get_if_sleep_mode_enabled();
    static bool get_if_sim_power_off_enabled();
    static void enable_start_in_sleep_mode();
    static void disable_sleep_mode();
    static void disable_sim_power_off_mode();
    static bool store_phone_local_number_to_eeprom(
        EepromRepository& eeprom_repository,
        const char* phone_local_number);
    bool load_phone_number_from_eeprom_with_prefix(char (&out_phone_number)[sizeof(APP_PHONE_NUMBER)]) const;

private:
    static bool is_interrupt_enabled;
    static bool is_gps_flow_enabled;
    static bool is_sleep_mode_enabled;
    static bool is_sim_power_off_enabled;
    NmeaGpsActivity* gps_activity_;
    SimRepository* sim_repository_;
    EepromRepository* eeprom_repository_;
};
