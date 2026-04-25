#pragma once

#include <stdint.h>
#include <mf_commons_commonsLayer.h>
#include "mf_activity_NmeaGpsActivity.h"

#if _ON_MOCKING_TESTS && _DEBUG_FOR_SERIAL
#error "_DEBUG_FOR_SERIAL must be 0 when _ON_MOCKING_TESTS is 1"
#endif

class GpsSystem_BL {
public:
    explicit GpsSystem_BL(IGpsActivity& gps_activity);
    ~GpsSystem_BL() = default;

    bool read_and_log_coordinates();

private:
    IGpsActivity& gps_activity_;
};
