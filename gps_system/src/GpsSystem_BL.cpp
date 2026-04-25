// File: GpsSystem_BL.cpp

#include "GpsSystem_BL.h"

#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
#include <Arduino.h>
#endif

GpsSystem_BL::GpsSystem_BL(IGpsActivity& gps_activity)
    : gps_activity_(gps_activity) {
}

bool GpsSystem_BL::read_and_log_coordinates() {
    const auto gps_status = gps_activity_.update_gps_status();
    if (!gps_status.has_recent_coordinates) {
        return false;
    }
    float latitude = static_cast<float>(gps_status.latitude_x1000000) / 1000000.0f;
    float longitude = static_cast<float>(gps_status.longitude_x1000000) / 1000000.0f;
    Serial.print("https://www.google.com/maps/search/?api=1&query=");
    Serial.print(latitude, 6);
    Serial.print(",");
    Serial.println(longitude, 6);

    return true;
}
