/*
 Name:      gps_system.ino
 Created:   4/24/2026 12:45:48 PM
 Author:    luigi
*/

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "AppConfig.h"
#include <mf_adapter_SoftwareSerialAdapter.h>
#include <mf_repository_NmeaGpsRepository.h>
#include <mf_activity_NmeaGpsActivity.h>
#include "src/business/GpsSystem_BL.h"

SoftwareSerial gps_serial(AppConfig::software_serial_rx_pin, AppConfig::software_serial_tx_pin);
SoftwareSerialAdapter gps_serial_adapter(gps_serial);
mf_repository_NmeaGpsRepository nmea_gps_repository(gps_serial_adapter);
mf_activity_NmeaGpsActivity nmea_gps_activity(nmea_gps_repository);
GpsSystem_BL gps_system_bl(nmea_gps_activity);
NmeaGpsData gps_data_for_external_flow = {};
bool has_gps_data_for_external_flow = false;

void setup() {
#if _DEBUG_FOR_SERIAL
    Serial.begin(115200);
#endif
    nmea_gps_repository.begin();
}

void loop() {
    // Questo oggetto contiene tutte le informazioni utili:
    // latitudine, longitudine, quota, velocita', data/ora, fix, satelliti, ecc.
    has_gps_data_for_external_flow = gps_system_bl.try_read_gps_data(gps_data_for_external_flow);
    delay(1000);
}
