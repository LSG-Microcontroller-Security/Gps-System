// gps_system.ino
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
#include "mf_adapter_SoftwareSerialAdapter.h"
#include "mf_repository_EepromRepository.h"
#include "mf_repository_NmeaGpsRepository.h"
#include "mf_repository_SimRepository.h"
#include "mf_activity_NmeaGpsActivity.h"
#include "mf_commons_commonsLayer.h"
#include "src/GpsSystem_BL.h"
extern unsigned int __heap_start;
extern void* __brkval;
GpsConnectionStatus gps_connection_status = {};
bool is_first_sim_flow_run = true;
// Watchdog timing
constexpr uint8_t watchdog_ticks_intervall = 8U; // 8 * 8s = circa 64s
volatile bool is_interrupt_triggered = false;
volatile bool is_watchdog_triggered = false;

bool is_required_sleep_mode = true;
bool is_sim800c_in_sleep_mode = false;
bool is_interrupt_management_enabled = true;

// Watchdog counters
uint8_t watchdog_ticks_accumulator = 0U;

ISR(WDT_vect) {
	is_watchdog_triggered = true;
}

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
void setup() {
	Serial.begin(9600);
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
	Serial.println(F("start"));
#endif
	pinMode(APP_INTERRUPT_PIN, INPUT_PULLUP);
	pinMode(APP_GPS_POWER_PIN, OUTPUT);
	digitalWrite(APP_GPS_POWER_PIN, LOW);
	pinMode(APP_SIM_SLEEP_PIN, OUTPUT);
	digitalWrite(APP_SIM_SLEEP_PIN, LOW);
	pinMode(APP_SIM_POWER_PIN, OUTPUT);
	digitalWrite(APP_SIM_POWER_PIN, LOW);
	delay(5000);
	attachInterrupt(digitalPinToInterrupt(APP_INTERRUPT_PIN), on_int0_wake, FALLING);
#if APP_SIM_DISABLE_NETLIGHT_ON_SETUP
	turnOffSimLed();
#endif
	// USE ONLY WITH EEPROM CLEAR
	// EepromRepository eeprom_repository;
	// (void)GpsSystem_BL::store_phone_local_number_to_eeprom(eeprom_repository, XXXXXXXXXXXXXXXX); // bootstrap one-shot
}
void loop() {
	if (is_required_sleep_mode) {
		is_watchdog_triggered = false;
		digitalWrite(APP_GPS_POWER_PIN, LOW);
		if (!is_sim800c_in_sleep_mode) {
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
			Serial.println(F("sim sleep enter"));
#endif
			sim_800c_enter_sleep_mode();
			is_sim800c_in_sleep_mode = true;
		}
		enter_deep_sleep();
		if (is_interrupt_triggered) {
			if (is_sim800c_in_sleep_mode) {
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
				Serial.println(F("sim sleep exit"));
#endif
				sim_800c_exit_sleep_mode();
				is_sim800c_in_sleep_mode = false;
			}
			watchdog_ticks_accumulator = 0U;
			is_required_sleep_mode = false;
		}
		else if (is_watchdog_triggered) {
			if (watchdog_ticks_accumulator < watchdog_ticks_intervall) {
				++watchdog_ticks_accumulator;
			}

			if (watchdog_ticks_accumulator < watchdog_ticks_intervall) {
				return;
			}
			watchdog_ticks_accumulator = 0U;
			if (is_sim800c_in_sleep_mode) {
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
				Serial.println(F("sim sleep exit"));
#endif
				sim_800c_exit_sleep_mode();
				is_sim800c_in_sleep_mode = false;
			}
			run_sim_flow();
			return;
		}
	}

	run_gps_flow();
	run_sim_flow();

	// Prima leggiamo e applichiamo le direttive SMS, poi facciamo l'eventuale chiamata da wake interrupt.
	if (is_interrupt_triggered) {
		if (!is_required_sleep_mode) {
			make_alarm_call();
		}
		is_interrupt_triggered = false;

		// Dopo la chiamata torniamo in sleep se il GPS flow non e' attivo.
		if (!is_required_sleep_mode && !GpsSystem_BL::get_if_gps_flow_enabled()) {
			is_required_sleep_mode = true;
			watchdog_ticks_accumulator = 0U;
		}
	}
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
	Serial.print(F("Free RAM after SIM: "));
	Serial.println(freeMemory());
#endif

	delay(1000);
}
void on_int0_wake() {
	is_interrupt_triggered = true;
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
void sim_800c_enter_sleep_mode() {
	SoftwareSerial sim800c_software_serial(APP_SIM_SW_SERIAL_RX_PIN, APP_SIM_SW_SERIAL_TX_PIN);
	SoftwareSerialAdapter sim800c_software_serial_adapter(sim800c_software_serial);
	SimRepository sim800c_repository(sim800c_software_serial_adapter, APP_SIM_SLEEP_PIN, APP_SIM_SERIAL_BAUD_RATE);
	(void)sim800c_repository.enterSleepMode();
}
void sim_800c_exit_sleep_mode() {
	SoftwareSerial sim800c_software_serial(APP_SIM_SW_SERIAL_RX_PIN, APP_SIM_SW_SERIAL_TX_PIN);
	SoftwareSerialAdapter sim800c_software_serial_adapter(sim800c_software_serial);
	SimRepository sim800c_repository(sim800c_software_serial_adapter, APP_SIM_SLEEP_PIN, APP_SIM_SERIAL_BAUD_RATE);
	sim800c_repository.exitSleepMode();
}
void turnOffSimLed() {
	digitalWrite(APP_SIM_SLEEP_PIN, LOW);
	SoftwareSerial sim800c_software_serial(APP_SIM_SW_SERIAL_RX_PIN, APP_SIM_SW_SERIAL_TX_PIN);
	SoftwareSerialAdapter sim800c_software_serial_adapter(sim800c_software_serial);
	SimRepository sim800c_repository(sim800c_software_serial_adapter, APP_SIM_SLEEP_PIN, APP_SIM_SERIAL_BAUD_RATE);
	for (uint8_t attempt = 0U; attempt < 3U; ++attempt) {
		if (sim800c_repository.setNetlightEnabled(false)) {
			break;
		}
		delay(400);
	}
}
void make_alarm_call() {
	if (!GpsSystem_BL::get_if_interrupt_enabled()) return;
#if !APP_SIM_ENABLE_PHONE_CALLS
	return;
#endif

	SoftwareSerial sim800c_software_serial(APP_SIM_SW_SERIAL_RX_PIN, APP_SIM_SW_SERIAL_TX_PIN);
	SoftwareSerialAdapter sim800c_software_serial_adapter(sim800c_software_serial);
	SimRepository sim800c_repository(sim800c_software_serial_adapter, APP_SIM_SLEEP_PIN, APP_SIM_SERIAL_BAUD_RATE);
	EepromRepository eeprom_repository;
	GpsSystem_BL sim_bl(&sim800c_repository, &eeprom_repository);
	char number[sizeof(APP_PHONE_NUMBER)] = {};
	sim_bl.load_phone_number_from_eeprom_with_prefix(number);
	sim800c_repository.call(number);
}
void run_gps_flow() {
	if (!GpsSystem_BL::get_if_gps_flow_enabled()) {
		digitalWrite(APP_GPS_POWER_PIN, LOW);
		return;
	}
	digitalWrite(APP_GPS_POWER_PIN, HIGH);
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
	SimRepository sim800c_repository(sim800c_software_serial_adapter, APP_SIM_SLEEP_PIN, APP_SIM_SERIAL_BAUD_RATE, APP_SIM_POWER_PIN);
	EepromRepository eeprom_repository;
	GpsSystem_BL sim_bl(&sim800c_repository, &eeprom_repository);

	if (is_first_sim_flow_run) {
		sim_bl.delete_all_sms();
		sim_bl.startSmsReception();
		if (GpsSystem_BL::get_if_interrupt_enabled()) {
			sim_bl.call_phone_number();
		}
		is_first_sim_flow_run = false;
	}

	char message_buffer[APP_PHONE_LOCAL_NUMBER_LENGTH + 2U];

	if (sim_bl.check_if_sms_arrived()) {
		if (sim_bl.read_sms(message_buffer, sizeof(message_buffer))) {
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
			Serial.print(F("sms ricevuto: "));
			Serial.println(message_buffer);
#endif
			sim_bl.analyze_sms_message(message_buffer, gps_connection_status);
		}
	}

	// L'interrupt INT0 resta attivo solo quando le chiamate da interrupt sono abilitate.
	if (!GpsSystem_BL::get_if_interrupt_enabled()) {
		if (is_interrupt_management_enabled) {
			detachInterrupt(digitalPinToInterrupt(APP_INTERRUPT_PIN));
			is_interrupt_management_enabled = false;
			is_interrupt_triggered = false;
		}
	}
	else if (!is_interrupt_management_enabled) {
		attachInterrupt(digitalPinToInterrupt(APP_INTERRUPT_PIN), on_int0_wake, FALLING);
		is_interrupt_management_enabled = true;
	}

	if (GpsSystem_BL::get_if_sleep_mode_enabled()) {
		is_required_sleep_mode = true;
		watchdog_ticks_accumulator = 0U;
		digitalWrite(APP_GPS_POWER_PIN, LOW);
		GpsSystem_BL::disable_sleep_mode();
	}

	if (GpsSystem_BL::get_if_gps_flow_enabled()) {
		is_required_sleep_mode = false;
		watchdog_ticks_accumulator = 0U;
	}
}

