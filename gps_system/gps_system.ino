/*
 Name:      gps_system.ino
 Created:   4/24/2026 12:45:48 PM
 Author:    luigi
*/
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "AppConfig.h"
#include <mf_adapter_SoftwareSerialAdapter.h>
#include "src/mf_repository_EepromRepository.h"
#include "src/mf_repository_NmeaGpsRepository.h"
#include "src/mf_repository_SimRepository.h"
#include "src/mf_activity_NmeaGpsActivity.h"
//#include <mf_repository_AvrMicroRepository.h>
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
constexpr uint8_t kWakeInt0Pin = 2U; // INT0 on ATmega328P (Nano D2)
volatile bool g_woke_from_int0 = false;
volatile bool g_woke_from_wdt = false;
bool g_stay_awake_latched = false;

ISR(WDT_vect) {
    g_woke_from_wdt = true;
}
void on_int0_wake() {
    g_woke_from_int0 = true;
}
void configure_watchdog_8s_interrupt() {
    MCUSR &= static_cast<uint8_t>(~_BV(WDRF));
    WDTCSR = static_cast<uint8_t>(_BV(WDCE) | _BV(WDE));
    WDTCSR = static_cast<uint8_t>(_BV(WDIE) | _BV(WDP3) | _BV(WDP0)); // ~8s interrupt only
}
void disable_watchdog() {
    wdt_disable();
}
void enter_deep_sleep() {
    ADCSRA &= static_cast<uint8_t>(~_BV(ADEN));
    power_adc_disable();
    power_spi_disable();
    power_twi_disable();
    power_timer0_disable();
    power_timer1_disable();
    power_timer2_disable();
    ACSR |= _BV(ACD);

    configure_watchdog_8s_interrupt();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_cpu();
    sleep_disable();
    disable_watchdog();

    power_timer0_enable();
    power_timer1_enable();
    power_timer2_enable();
    power_spi_enable();
    power_twi_enable();
    power_adc_enable();
    ADCSRA |= _BV(ADEN);
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
    EepromRepository eeprom_repository;
    GpsSystem_BL sim_bl(&sim800c_repository, &eeprom_repository);
    if (is_first_sim_flow_run) {
        sim800c_repository.listen();
        sim800c_repository.deleteAllSms();
        is_first_sim_flow_run = false;
    }
    sim_bl.startSmsReception();
    char message_buffer[16];
    if (sim_bl.check_if_sms_arrived()) {
        if (sim_bl.read_sms(message_buffer, sizeof(message_buffer))) {
            sim_bl.analyze_sms_message(message_buffer, gps_connection_status);
        }
    }
}
void setup() {
    Serial.begin(115200);
    delay(100);
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
    Serial.println(F("start"));
#endif
    pinMode(kWakeInt0Pin, INPUT_PULLUP);
    delay(100);
    attachInterrupt(digitalPinToInterrupt(kWakeInt0Pin), on_int0_wake, FALLING);

    //USE ONLY WITH EEPROM CLEAR
    //EepromRepository eeprom_repository;
    //(void)GpsSystem_BL::store_phone_local_number_to_eeprom(eeprom_repository, XXXXXXXXXXXXXXXX  ); // bootstrap one-shot
}
void loop() {
    if (!g_stay_awake_latched) {
        g_woke_from_int0 = false;
        g_woke_from_wdt = false;
        enter_deep_sleep();
        if (g_woke_from_int0) {
            g_stay_awake_latched = true;
        }
        if (g_woke_from_wdt && !g_woke_from_int0) {
            return;
        }
    }
    run_gps_flow();
    run_sim_flow();
    Serial.print(F("Free RAM after SIM: "));
    Serial.println(freeMemory());
	delay(1000);
}
