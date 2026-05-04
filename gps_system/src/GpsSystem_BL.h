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
    void set_sim_netlight_enabled(bool enabled);
    void delete_all_sms();
    void analyze_sms_message(const char* message, const GpsConnectionStatus& gps_status);
    void hang_up_phone_call();
    void startSmsReception();
	static bool get_if_call_disabled();
    static bool get_if_gps_flow_enabled();
    static bool get_if_sleep_mode_requested();
    static void clear_sleep_mode_requested();
    static bool get_if_tilt_lock_requested();
    static uint16_t get_tilt_lock_minutes();
    static void clear_tilt_lock_requested();
    static bool store_phone_local_number_to_eeprom(
        EepromRepository& eeprom_repository,
        const char* phone_local_number);
    bool load_phone_number_from_eeprom_with_prefix(char (&out_phone_number)[sizeof(APP_PHONE_NUMBER)]) const;

private:
    static bool is_call_disabled;
    static bool is_gps_flow_enabled;
    static bool is_sleep_mode_requested;
    static bool is_tilt_lock_requested;
    static uint16_t tilt_lock_minutes;
    NmeaGpsActivity* gps_activity_;
    SimRepository* sim_repository_;
    EepromRepository* eeprom_repository_;
};
