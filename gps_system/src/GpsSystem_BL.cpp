// File: GpsSystem_BL.cpp

#include "GpsSystem_BL.h"

#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
#include <Arduino.h>
#endif

GpsSystem_BL::GpsSystem_BL(IGpsActivity& gps_activity,SimRepository&   sim_repository)
    : gps_activity_(gps_activity),
      sim_repository_(sim_repository) {

}

bool GpsSystem_BL::read_and_log_coordinates() {
	this->gps_activity_.get_repository().listen();
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

bool GpsSystem_BL::call_phone_number(const char* number) {
	sim_repository_.listen();
    return sim_repository_.call(number);
}
void GpsSystem_BL::hang_up_phone_call() {
    sim_repository_.hangUp();
}
