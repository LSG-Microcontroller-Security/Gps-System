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
    bool call_phone_number();
    bool send_sms(const GpsSmsData& gps_sms_data);
    bool check_if_sms_arrived();
    bool read_sms(char* out_message, uint8_t out_message_size);
    void hang_up_phone_call();
    void startSmsReception();
    static bool initialize_phone_local_number(
        EepromRepository& eeprom_repository,
        const char (&phone_local_number)[APP_PHONE_LOCAL_NUMBER_LENGTH + 1U]);

private:
    bool load_phone_number_with_prefix(char (&out_phone_number)[sizeof(APP_PHONE_NUMBER)]) const;

    NmeaGpsActivity* gps_activity_;
    SimRepository* sim_repository_;
    EepromRepository* eeprom_repository_;
};
