/*
 Name:      gps_system.ino
 Created:   4/24/2026 12:45:48 PM
 Author:    luigi
*/
#include <Arduino.h>
#include <SoftwareSerial.h>
#include "AppConfig.h"
#include <mf_adapter_SoftwareSerialAdapter.h>
#include "src/mf_repository_EepromRepository.h"
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
GpsConnectionStatus gps_connection_status = {};
bool is_first_sim_flow_run = true;

namespace {
}

void run_gps_flow() {
    SoftwareSerial gps_software_serial(APP_GPS_SW_SERIAL_RX_PIN, APP_GPS_SW_SERIAL_TX_PIN);
    SoftwareSerialAdapter gps_software_serial_adapter(gps_software_serial);
    NmeaGpsRepository nmea_gps_repository(gps_software_serial_adapter);
    NmeaGpsActivity nmea_gps_activity(nmea_gps_repository);
    GpsSystem_BL gps_bl(&nmea_gps_activity);
    gps_bl.read_and_log_coordinates(gps_connection_status);
}
void run_sim_flow() {
    SoftwareSerial sim800c_software_serial(APP_SIM_SW_SERIAL_RX_PIN, APP_SIM_SW_SERIAL_TX_PIN);
    SoftwareSerialAdapter sim800c_software_serial_adapter(sim800c_software_serial);
    SimRepository sim800c_repository(sim800c_software_serial_adapter, 0, APP_SIM_SERIAL_BAUD_RATE);
    if (is_first_sim_flow_run) {
        sim800c_repository.listen();
        sim800c_repository.deleteAllSms();
        is_first_sim_flow_run = false;
    }
    EepromRepository eeprom_repository;
    GpsSystem_BL sim_bl(&sim800c_repository, &eeprom_repository);
    sim_bl.startSmsReception();
    char message_buffer[16];
    if (sim_bl.read_sms(message_buffer, sizeof(message_buffer))) {
        Serial.println(message_buffer);
        // TODO: passare message_buffer alla futura funzione di analisi azioni.
        GpsSystem_BL::GpsSmsData gps_sms_data = {
            gps_connection_status.latitude_x1000000,
            gps_connection_status.longitude_x1000000,
            gps_connection_status.altitude_m_x10,
            gps_connection_status.speed_knots_x100
        };
        sim_bl.send_sms(gps_sms_data);
    }
    //sim_bl.call_phone_number();
}
void setup() {
    Serial.begin(115200);
    //USE ONLY WITH EEPROM CLEAR
    // EepromRepository eeprom_repository;
    // //(void)GpsSystem_BL::initialize_phone_local_number(eeprom_repository, APP_PHONE_LOCAL_NUMBER); // bootstrap one-shot
}
void loop() {
    run_gps_flow();
    run_sim_flow();
    Serial.print(F("Free RAM after SIM: "));
    Serial.println(freeMemory());
	delay(1000);
}
