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
#include "src/mf_activity_NmeaGpsActivity.h"
#include "src/GpsSystem_BL.h"

SoftwareSerial gps_serial(AppConfig::software_serial_rx_pin, AppConfig::software_serial_tx_pin);
SoftwareSerialAdapter gps_serial_adapter(gps_serial);
mf_repository_NmeaGpsRepository nmea_gps_repository(gps_serial_adapter);
mf_activity_NmeaGpsActivity nmea_gps_activity(nmea_gps_repository);
GpsSystem_BL gps_system_bl(nmea_gps_activity);
//SoftwareSerial gps_serial(8, 9); // RX, TX - GPS TX deve andare su Arduino D8
void setup() {
    //Serial.begin(115200);     // Console PC
    //gps_serial.begin(9600);   // GPS NEO-6M tipicamente a 9600
    //gps_serial.listen();
    Serial.begin(115200);
    Serial.println("GPS RAW TEST START");
}

void loop() {
    //while (gps_serial.available() > 0) {
    //    int value = gps_serial.read();
    //    if (value >= 0) {
    //        Serial.write((char)value);
    //    }
    //}
    Serial.println(gps_system_bl.read_and_log_coordinates());
    delay(1000);
}
