// gps_system.ino
/*
 Name:      gps_system.ino
 Created:   4/24/2026 12:45:48 PM By Luigi
 Author:    luigi
*/
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "AppConfig.h"
#include "GitVersion.h"
#include <mf_adapter_SoftwareSerialAdapter.h>
#include <mf_repository_EepromRepository.h>
#include <mf_repository_NmeaGpsRepository.h>
#include <mf_repository_SimRepository.h>
#include <mf_activity_NmeaGpsActivity.h>
#include <mf_commons_commonsLayer.h>
#include "src/GpsSystem_BL.h"
extern unsigned int __heap_start;
extern void* __brkval;
GpsConnectionStatus gps_connection_status = {};
bool is_first_sim_flow_run = true;
// Watchdog timing
constexpr uint8_t watchdog_ticks_intervall = 8U; // 8 * 8s = circa 64s
volatile bool is_interrupt_triggered = false;
volatile bool is_watchdog_triggered = false;
volatile uint8_t wake_interrupt_edges = 0U;
bool is_required_sleep_mode = true;
bool is_sim800c_in_sleep_mode = false;
bool is_interrupt_management_enabled = true;
bool is_wake_interrupt_attached = false;
bool is_sim_netlight_disable_pending_after_power_on = true;
constexpr uint8_t external_device_pins_portb_mask = _BV(PB0) | _BV(PB1) | _BV(PB2); // D8, D9, D10
constexpr uint8_t external_device_pins_portd_mask = _BV(PD4) | _BV(PD6) | _BV(PD7); // D4, D6, D7
uint8_t external_device_pins_ddrb_state = 0U;
uint8_t external_device_pins_portb_state = 0U;
uint8_t external_device_pins_ddrd_state = 0U;
uint8_t external_device_pins_portd_state = 0U;
// Watchdog counters
uint8_t watchdog_ticks_accumulator = 0U;
bool sim_800c_turn_on_module();
void attach_wake_interrupt_if_enabled();
void detach_wake_interrupt_if_attached();
void clear_wake_interrupt_state();
bool is_wake_movement_confirmed();
void enter_deep_sleep(bool is_watchdog_enabled);
void disable_external_device_pins_for_sleep();
void restore_external_device_pins_after_sleep();
void apply_post_sms_runtime_state();

ISR(WDT_vect) {
	is_watchdog_triggered = true;
}
#if !_ON_MOCKING_TESTS && _DEBUG_FREE_MEMORY
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

void print_free_memory() {
	Serial.print(F("Free RAM: "));
	Serial.println(freeMemory());
}
#endif
void setup() {
	Serial.begin(9600);
	// GS-7: versions are generated at prebuild time and read directly from flash.
	Serial.print(F("Firmware Git: "));
	Serial.println(F(GIT_VERSION));
	Serial.print(F("Libraries Git: "));
	Serial.println(F(GIT_LIBRARIES_VERSION));
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
	Serial.println(F("start"));
#endif
	//pinMode(APP_INTERRUPT_PIN, INPUT_PULLUP);
	pinMode(APP_GPS_POWER_PIN, OUTPUT);
	digitalWrite(APP_GPS_POWER_PIN, LOW);
	pinMode(APP_SIM_SLEEP_PIN, OUTPUT);
	digitalWrite(APP_SIM_SLEEP_PIN, LOW);
	// Rilascia il PWRKEY: evita una pressione lunga involontaria durante reset/deploy.
	pinMode(APP_SIM_POWER_PIN, INPUT);
	digitalWrite(APP_SIM_POWER_PIN, LOW);
#if APP_START_IN_SLEEP_MODE_ENABLED
	GpsSystem_BL::enable_start_in_sleep_mode();
	is_required_sleep_mode = true;
	is_sim800c_in_sleep_mode = false;
	watchdog_ticks_accumulator = 0U;
#else
	delay(5000);
	(void)sim_800c_turn_on_module();
	run_sim_flow();
	apply_post_sms_runtime_state();
#endif
	// USE ONLY WITH EEPROM CLEAR
	// EepromRepository eeprom_repository;
	// (void)GpsSystem_BL::store_phone_local_number_to_eeprom(eeprom_repository, XXXXXXXXXXXXXXXX); // bootstrap one-shot
}
void loop() {
	if (is_required_sleep_mode) {
		is_watchdog_triggered = false;
		digitalWrite(APP_GPS_POWER_PIN, LOW);
		if (GpsSystem_BL::get_if_sim_power_off_enabled()) {
			is_sim800c_in_sleep_mode = false;
		}
		else if (!is_sim800c_in_sleep_mode) {
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
			Serial.println(F("sim sleep enter"));
#endif
			sim_800c_enter_sleep_mode();
			is_sim800c_in_sleep_mode = true;
		}
		attach_wake_interrupt_if_enabled();
		disable_external_device_pins_for_sleep();
		enter_deep_sleep(!GpsSystem_BL::get_if_sim_power_off_enabled());
		restore_external_device_pins_after_sleep();
        // INT0 stays attached briefly after a motion wake to observe later edges.
        if (is_interrupt_triggered) {
            const bool is_movement_confirmed = is_wake_movement_confirmed();
            detach_wake_interrupt_if_attached();
            if (!is_movement_confirmed) {
                clear_wake_interrupt_state();
                // Preserve the watchdog path if both wake sources fired.
                if (!is_watchdog_triggered) {
                    return;
                }
            }
        }
        else {
            detach_wake_interrupt_if_attached();
        }
		if (is_interrupt_triggered) {
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
			Serial.println(F("mcu sleep exit interrupt"));
#endif
			if (GpsSystem_BL::get_if_sim_power_off_enabled()) {
				if (sim_800c_turn_on_module()) {
					GpsSystem_BL::disable_sim_power_off_mode();
					is_first_sim_flow_run = true;
					is_sim800c_in_sleep_mode = false;
					is_sim_netlight_disable_pending_after_power_on = true;
				}
				else {
#if !_ON_MOCKING_TESTS && _DEBUG_FREE_MEMORY
					print_free_memory();
#endif
					return;
				}
			}
			else if (is_sim800c_in_sleep_mode) {
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
				Serial.println(F("sim sleep exit"));
#endif
				sim_800c_exit_sleep_mode();
				is_sim800c_in_sleep_mode = false;
			}
#if !_ON_MOCKING_TESTS && _DEBUG_FREE_MEMORY
			print_free_memory();
#endif
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
#if !_ON_MOCKING_TESTS && _DEBUG_FREE_MEMORY
			print_free_memory();
#endif
			if (GpsSystem_BL::get_if_sim_power_off_enabled()) {
				return;
			}
			if (is_sim800c_in_sleep_mode) {
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
				Serial.println(F("sim sleep exit"));
#endif
				sim_800c_exit_sleep_mode();
				is_sim800c_in_sleep_mode = false;
			}
			run_sim_flow();
			apply_post_sms_runtime_state();
			return;
		}
	}
	run_gps_flow();
	run_sim_flow();
	apply_post_sms_runtime_state();
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
	delay(1000);
}
void on_int0_wake() {
    is_interrupt_triggered = true;
    if (wake_interrupt_edges < 255U) {
        ++wake_interrupt_edges;
    }
}
bool is_wake_movement_confirmed() {
    // Ignore the wake edge itself. Count only later edges, across time slots.
    noInterrupts();
    wake_interrupt_edges = 0U;
    interrupts();

    uint8_t active_intervals_mask = 0U;
    for (uint8_t interval = 0U; interval < GpsSystem_BL::movement_observation_intervals; ++interval) {
        delay(GpsSystem_BL::movement_observation_interval_ms);
        noInterrupts();
        const uint8_t edges_in_interval = wake_interrupt_edges;
        wake_interrupt_edges = 0U;
        interrupts();

        if (edges_in_interval != 0U) {
            active_intervals_mask |= static_cast<uint8_t>(1U << interval);
        }
    }
    return GpsSystem_BL::is_movement_confirmed(active_intervals_mask);
}
void clear_wake_interrupt_state() {
    is_interrupt_triggered = false;
    wake_interrupt_edges = 0U;
    EIFR = static_cast<uint8_t>(_BV(INTF0));
}
void attach_wake_interrupt_if_enabled() {
	if (!is_interrupt_management_enabled || is_wake_interrupt_attached) {
		return;
	}
	clear_wake_interrupt_state();
	attachInterrupt(digitalPinToInterrupt(APP_INTERRUPT_PIN), on_int0_wake, FALLING);
	is_wake_interrupt_attached = true;
}
void detach_wake_interrupt_if_attached() {
	if (!is_wake_interrupt_attached) {
		return;
	}
	detachInterrupt(digitalPinToInterrupt(APP_INTERRUPT_PIN));
	is_wake_interrupt_attached = false;
}
void configure_watchdog_8s_interrupt() {
	MCUSR &= static_cast<uint8_t>(~_BV(WDRF));
	WDTCSR = static_cast<uint8_t>(_BV(WDCE) | _BV(WDE));
	WDTCSR = static_cast<uint8_t>(_BV(WDIE) | _BV(WDP3) | _BV(WDP0)); // ~8s interrupt only
}
void disable_watchdog() {
	wdt_disable();
}
void enter_deep_sleep(bool is_watchdog_enabled) {
	ADCSRA &= static_cast<uint8_t>(~_BV(ADEN));
	power_adc_disable();
	power_spi_disable();
	power_twi_disable();
	power_timer0_disable();
	power_timer1_disable();
	power_timer2_disable();
	ACSR |= _BV(ACD);

	if (is_watchdog_enabled) {
		configure_watchdog_8s_interrupt();
	}
	else {
		disable_watchdog();
	}
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_enable();
	sleep_cpu();
	sleep_disable();
	if (is_watchdog_enabled) {
		disable_watchdog();
	}
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
bool sim_800c_turn_on_module() {
	SoftwareSerial sim800c_software_serial(APP_SIM_SW_SERIAL_RX_PIN, APP_SIM_SW_SERIAL_TX_PIN);
	SoftwareSerialAdapter sim800c_software_serial_adapter(sim800c_software_serial);
	SimRepository sim800c_repository(sim800c_software_serial_adapter, APP_SIM_SLEEP_PIN, APP_SIM_SERIAL_BAUD_RATE, APP_SIM_POWER_PIN);
	for (uint8_t probe_attempt = 0U; probe_attempt < 3U; ++probe_attempt) {
		if (sim800c_repository.is_module_on()) {
			return true;
		}
		sim800c_repository.delay(500UL);
	}
	for (uint8_t power_on_attempt = 0U; power_on_attempt < 2U; ++power_on_attempt) {
		(void)sim800c_repository.turn_on_module();
		for (uint8_t probe_attempt = 0U; probe_attempt < 4U; ++probe_attempt) {
			if (sim800c_repository.is_module_on()) {
				return true;
			}
			sim800c_repository.delay(500UL);
		}
	}
	return false;
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
	bool is_network_registered = false;
	while (!is_network_registered) {
		sim800c_repository.listen();
		is_network_registered = sim800c_repository.isRegisteredToNetwork();
		if (!is_network_registered) {
			sim800c_repository.delay(1000UL);
		}
	}
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
	Serial.println(F("sim network registered"));
#endif
#if !APP_SIM_ENABLE_NETLIGHT_ON_SETUP
	if (is_sim_netlight_disable_pending_after_power_on) {
		turnOffSimLed();
		is_sim_netlight_disable_pending_after_power_on = false;
	}
#endif

	if (is_first_sim_flow_run) {
		sim_bl.delete_all_sms();
		sim_bl.startSmsReception();
		// if (GpsSystem_BL::get_if_interrupt_enabled()) {
		sim_bl.call_phone_number();
		// }
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
}
void apply_post_sms_runtime_state() {
	if (!GpsSystem_BL::get_if_interrupt_enabled()) {
		if (is_interrupt_management_enabled) {
			is_interrupt_management_enabled = false;
			is_interrupt_triggered = false;
			detach_wake_interrupt_if_attached();
		}
	}
	else if (!is_interrupt_management_enabled) {
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
void disable_external_device_pins_for_sleep() {
	external_device_pins_ddrb_state = DDRB & external_device_pins_portb_mask;
	external_device_pins_portb_state = PORTB & external_device_pins_portb_mask;
	external_device_pins_ddrd_state = DDRD & external_device_pins_portd_mask;
	external_device_pins_portd_state = PORTD & external_device_pins_portd_mask;

	DDRB &= static_cast<uint8_t>(~external_device_pins_portb_mask);
	PORTB &= static_cast<uint8_t>(~external_device_pins_portb_mask);
	DDRD &= static_cast<uint8_t>(~external_device_pins_portd_mask);
	PORTD &= static_cast<uint8_t>(~external_device_pins_portd_mask);
}
void restore_external_device_pins_after_sleep() {
	PORTB = static_cast<uint8_t>((PORTB & static_cast<uint8_t>(~external_device_pins_portb_mask)) | external_device_pins_portb_state);
	DDRB = static_cast<uint8_t>((DDRB & static_cast<uint8_t>(~external_device_pins_portb_mask)) | external_device_pins_ddrb_state);
	PORTD = static_cast<uint8_t>((PORTD & static_cast<uint8_t>(~external_device_pins_portd_mask)) | external_device_pins_portd_state);
	DDRD = static_cast<uint8_t>((DDRD & static_cast<uint8_t>(~external_device_pins_portd_mask)) | external_device_pins_ddrd_state);
}

