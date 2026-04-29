#pragma once

#include <stdint.h>
#include <mf_commons_commonsLayer.h>
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
    explicit GpsSystem_BL(SimRepository* sim_repository);
    ~GpsSystem_BL() = default;
    bool read_and_log_coordinates(GpsConnectionStatus& out_status);
    bool call_phone_number(const char* number);
    bool send_sms(const char* number, const GpsSmsData& gps_sms_data);
    bool check_if_sms_arrived();
    void hang_up_phone_call();
    void startSmsReception();

private:
    NmeaGpsActivity* gps_activity_;
    SimRepository* sim_repository_;
};
