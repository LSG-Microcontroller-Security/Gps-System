/*
 Name:      gps_system.ino
 Created:   4/24/2026 12:45:48 PM
 Author:    luigi
*/
#include <Arduino.h>
#include <SoftwareSerial.h>
#include "AppConfig.h"
#include <mf_adapter_SoftwareSerialAdapter.h>
#include "src/mf_repository_NmeaGpsRepository.h"
#include "src/mf_repository_SimRepository.h"
#include "src/mf_activity_NmeaGpsActivity.h"
#include "src/GpsSystem_BL.h"

extern unsigned int __heap_start;
extern void* __brkval;

int freeMemory() {
    int free_memory;
    if (__brkval == 0) {
        free_memory = reinterpret_cast<int>(&free_memory) - reinterpret_cast<int>(&__heap_start);
    }
    else {
        free_memory = reinterpret_cast<int>(&free_memory) - reinterpret_cast<int>(__brkval);
    }
    return free_memory;
}

SoftwareSerial gps_software_serial(AppConfig::gps_software_serial_rx_pin,AppConfig::gps_software_serial_tx_pin);
SoftwareSerialAdapter gps_software_serial_adapter(gps_software_serial);
SoftwareSerial sim800c_software_serial(AppConfig::sim800c_software_serial_rx_pin, AppConfig::sim800c_software_serial_tx_pin);
SoftwareSerialAdapter sim800c_software_serial_adapter(sim800c_software_serial);
NmeaGpsRepository nmea_gps_repository(gps_software_serial_adapter);
NmeaGpsActivity nmea_gps_activity(nmea_gps_repository);
SimRepository sim800c_repository(sim800c_software_serial_adapter,0, AppConfig::sim800c_serial_baud_rate);
GpsSystem_BL gps_system_bl(nmea_gps_activity,sim800c_repository);
GpsConnectionStatus gps_connection_status = {};
void setup() {
    Serial.begin(115200);
	gps_system_bl.startSmsReception();
}
void loop() {
    gps_system_bl.read_and_log_coordinates(gps_connection_status);
	Serial.println(gps_connection_status.latitude_x1000000);
	//gps_system_bl.call_phone_number("393202445649");
	Serial.println(gps_system_bl.check_if_sms_arrived());
    Serial.print(F("Free RAM: "));
    Serial.println(freeMemory());
	delay(1000);
}
