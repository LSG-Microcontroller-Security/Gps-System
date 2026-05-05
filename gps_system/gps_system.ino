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

constexpr uint8_t interrupt_pin = 2U; // INT0 on ATmega328P (Nano D2)
constexpr uint8_t gps_power_pin = 12U;

// Watchdog timing
constexpr uint8_t watchdog_poll_interval_ticks = 8U; // 8 * 8s = circa 64s

volatile bool interrupt_0_wake_state = false;
volatile bool watchdog_wake_state = false;

bool system_awake_latch_state = false;
bool tilt_interrupt_lock_state = false;
bool sim800c_sleep_state = false;

// Watchdog counters
uint8_t watchdog_poll_elapsed_ticks = 0U;
uint16_t watchdog_tilt_lock_remaining_ticks = 0U;

ISR(WDT_vect) {
	watchdog_wake_state = true;
}

void setup() {
	Serial.begin(115200);
	delay(100);

#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
	Serial.println(F("start"));
#endif

	//delay(10000);

	pinMode(interrupt_pin, INPUT_PULLUP);
	pinMode(gps_power_pin, OUTPUT);

	digitalWrite(gps_power_pin, LOW);
	delay(100);
	attachInterrupt(digitalPinToInterrupt(interrupt_pin), on_int0_wake, FALLING);
	turnOffSimLed();
	// USE ONLY WITH EEPROM CLEAR
	// EepromRepository eeprom_repository;
	// (void)GpsSystem_BL::store_phone_local_number_to_eeprom(eeprom_repository, XXXXXXXXXXXXXXXX); // bootstrap one-shot
}

void loop() {
	if (!system_awake_latch_state) {
		interrupt_0_wake_state = false;
		watchdog_wake_state = false;
		digitalWrite(gps_power_pin, LOW);
		if (!sim800c_sleep_state) {
			sim_800c_enter_sleep_mode();
			sim800c_sleep_state = true;
		}
		enter_deep_sleep();
		// Questo blocco viene valutato a ogni wake da watchdog, solo quando il lock interrupt e' attivo.
		if (tilt_interrupt_lock_state && watchdog_wake_state) {
			if (watchdog_tilt_lock_remaining_ticks > 0U) {
				--watchdog_tilt_lock_remaining_ticks;
			}
			if (watchdog_tilt_lock_remaining_ticks == 0U) {
				tilt_interrupt_lock_state = false;
				attachInterrupt(digitalPinToInterrupt(interrupt_pin), on_int0_wake, FALLING);
			}
		}
		if (interrupt_0_wake_state) {
			if (sim800c_sleep_state) {
				sim_800c_exit_sleep_mode();
				sim800c_sleep_state = false;
			}

			watchdog_poll_elapsed_ticks = 0U;
			system_awake_latch_state = true;
		}
		else if (watchdog_wake_state) {
			if (watchdog_poll_elapsed_ticks < watchdog_poll_interval_ticks) {
				++watchdog_poll_elapsed_ticks;
			}

			if (watchdog_poll_elapsed_ticks < watchdog_poll_interval_ticks) {
				return;
			}
			watchdog_poll_elapsed_ticks = 0U;
			if (sim800c_sleep_state) {
				sim_800c_exit_sleep_mode();
				sim800c_sleep_state = false;
			}
			run_sim_flow();
			return;
		}
	}

	if (interrupt_0_wake_state) {
		make_alarm_call();
		interrupt_0_wake_state = false;
	}

	run_gps_flow();
	run_sim_flow();

	Serial.print(F("Free RAM after SIM: "));
	Serial.println(freeMemory());

	delay(1000);
}

void on_int0_wake() {
	interrupt_0_wake_state = true;
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
	SimRepository sim800c_repository(sim800c_software_serial_adapter, 10, APP_SIM_SERIAL_BAUD_RATE);
	sim800c_repository.enterSleepMode();
}

void sim_800c_exit_sleep_mode() {
	SoftwareSerial sim800c_software_serial(APP_SIM_SW_SERIAL_RX_PIN, APP_SIM_SW_SERIAL_TX_PIN);
	SoftwareSerialAdapter sim800c_software_serial_adapter(sim800c_software_serial);
	SimRepository sim800c_repository(sim800c_software_serial_adapter, 10, APP_SIM_SERIAL_BAUD_RATE);
	sim800c_repository.exitSleepMode();
}

void turnOffSimLed() {
	SoftwareSerial sim800c_software_serial(APP_SIM_SW_SERIAL_RX_PIN, APP_SIM_SW_SERIAL_TX_PIN);
	SoftwareSerialAdapter sim800c_software_serial_adapter(sim800c_software_serial);
	SimRepository sim800c_repository(sim800c_software_serial_adapter, 10, APP_SIM_SERIAL_BAUD_RATE);
	EepromRepository eeprom_repository;
	GpsSystem_BL sim_bl(&sim800c_repository, &eeprom_repository);
	sim_bl.set_sim_netlight_enabled(false);
}

void make_alarm_call() {
	if (GpsSystem_BL::get_if_call_disabled()) return;

	SoftwareSerial sim800c_software_serial(APP_SIM_SW_SERIAL_RX_PIN, APP_SIM_SW_SERIAL_TX_PIN);
	SoftwareSerialAdapter sim800c_software_serial_adapter(sim800c_software_serial);
	SimRepository sim800c_repository(sim800c_software_serial_adapter, 10, APP_SIM_SERIAL_BAUD_RATE);
	EepromRepository eeprom_repository;
	GpsSystem_BL sim_bl(&sim800c_repository, &eeprom_repository);

	char number[sizeof(APP_PHONE_NUMBER)] = {};
	sim_bl.load_phone_number_from_eeprom_with_prefix(number);
	sim800c_repository.call(number);
}

void run_gps_flow() {
	if (!GpsSystem_BL::get_if_gps_flow_enabled()) {
		digitalWrite(gps_power_pin, LOW);
		return;
	}

	digitalWrite(gps_power_pin, HIGH);

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
	SimRepository sim800c_repository(sim800c_software_serial_adapter, 10, APP_SIM_SERIAL_BAUD_RATE);
	EepromRepository eeprom_repository;
	GpsSystem_BL sim_bl(&sim800c_repository, &eeprom_repository);

	if (is_first_sim_flow_run) {
		sim_bl.delete_all_sms();
		sim_bl.startSmsReception();
		is_first_sim_flow_run = false;
	}

	char message_buffer[16];

	if (sim_bl.check_if_sms_arrived()) {
		if (sim_bl.read_sms(message_buffer, sizeof(message_buffer))) {
			sim_bl.analyze_sms_message(message_buffer, gps_connection_status);
		}
	}

	if (GpsSystem_BL::get_if_wake_mode_requested()) {
		system_awake_latch_state = true;
		watchdog_poll_elapsed_ticks = 0U;
		/*digitalWrite(kGpsPowerPin, LOW);*/
		GpsSystem_BL::clear_wake_mode_requested();
	}

	if (GpsSystem_BL::get_if_sleep_mode_requested()) {
		system_awake_latch_state = false;
		watchdog_poll_elapsed_ticks = 0U;
		digitalWrite(gps_power_pin, LOW);
		GpsSystem_BL::clear_sleep_mode_requested();
	}
	
	if (GpsSystem_BL::get_if_tilt_lock_requested()) {
		const uint16_t minutes = GpsSystem_BL::get_tilt_lock_minutes();
		system_awake_latch_state = false;
		watchdog_poll_elapsed_ticks = 0U;
		digitalWrite(gps_power_pin, LOW);
		tilt_interrupt_lock_state = true;
		const unsigned long total_seconds = static_cast<unsigned long>(minutes) * 60UL;
		const unsigned long ticks = (total_seconds + 7UL) / 8UL;
		watchdog_tilt_lock_remaining_ticks = (ticks > 65535UL) ? 65535U : static_cast<uint16_t>(ticks);
		detachInterrupt(digitalPinToInterrupt(interrupt_pin));
		GpsSystem_BL::clear_tilt_lock_requested();
	}
}

