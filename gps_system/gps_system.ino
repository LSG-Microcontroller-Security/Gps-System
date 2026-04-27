/*
 Name:      gps_system.ino
 Created:   4/24/2026 12:45:48 PM
 Author:    luigi
*/
#include <Arduino.h>
#include <SoftwareSerial.h>
#include "AppConfig.h"
#include <mf_adapter_SoftwareSerialAdapter.h>
#include <mf_repository_AvrMicroRepository.h>
#include "src/mf_repository_NmeaGpsRepository.h"
#include "src/mf_repository_SimRepository.h"
#include "src/mf_activity_NmeaGpsActivity.h"
#include "src/GpsSystem_BL.h"

SoftwareSerial gps_software_serial(AppConfig::gps_software_serial_rx_pin,AppConfig::gps_software_serial_tx_pin);
SoftwareSerialAdapter gps_software_serial_adapter(gps_software_serial);
mf_repository_NmeaGpsRepository nmea_gps_repository(gps_software_serial_adapter);
mf_activity_NmeaGpsActivity nmea_gps_activity(nmea_gps_repository);
SoftwareSerial sim800c_software_serial(AppConfig::sim800c_software_serial_rx_pin, AppConfig::sim800c_software_serial_tx_pin);
SoftwareSerialAdapter sim800c_software_serial_adapter(sim800c_software_serial);
SimRepository sim800c_repository(sim800c_software_serial_adapter,0, AppConfig::sim800c_serial_baud_rate);
GpsSystem_BL gps_system_bl(nmea_gps_activity,sim800c_repository);
void setup() {
    Serial.begin(115200);
    Serial.println("GPS RAW TEST START");
}
void loop() {
    Serial.println(gps_system_bl.read_and_log_coordinates());
	//gps_system_bl.call_phone_number("393202445649");
	delay(10000);
}
