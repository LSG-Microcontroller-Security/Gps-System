// File: GpsSystem_BL.cpp
#include "GpsSystem_BL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Arduino.h>

bool GpsSystem_BL::is_interrupt_enabled = true;
bool GpsSystem_BL::is_gps_flow_enabled = false;
bool GpsSystem_BL::is_sleep_mode_enabled = false;
bool GpsSystem_BL::is_sim_power_off_enabled = false;

GpsSystem_BL::GpsSystem_BL(NmeaGpsActivity* gps_activity)
	: gps_activity_(gps_activity),
	sim_repository_(nullptr),
	eeprom_repository_(nullptr) {
}
GpsSystem_BL::GpsSystem_BL(SimRepository* sim_repository, EepromRepository* eeprom_repository)
	: gps_activity_(nullptr),
	sim_repository_(sim_repository),
	eeprom_repository_(eeprom_repository) {
}
bool GpsSystem_BL::read_and_log_coordinates(GpsConnectionStatus& out_status) {
	this->gps_activity_->get_repository().listen();
	const auto gps_status = gps_activity_->update_gps_status();
	out_status = gps_status;
	if (!gps_status.has_recent_coordinates) {
		return false;
	}
	/*   float latitude = static_cast<float>(gps_status.latitude_x1000000) / 1000000.0f;
	   float longitude = static_cast<float>(gps_status.longitude_x1000000) / 1000000.0f;
	   Serial.print("https://www.google.com/maps/search/?api=1&query=");
	   Serial.print(latitude, 6);
	   Serial.print(",");
	   Serial.println(longitude, 6);*/
	return true;
}
void GpsSystem_BL::call_phone_number() {
	char number[sizeof(APP_PHONE_NUMBER)] = {};
	load_phone_number_from_eeprom_with_prefix(number);

#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
	Serial.print(F("calling number: "));
	Serial.println(number);
#endif
#if APP_SIM_ENABLE_PHONE_CALLS
	sim_repository_->listen();
	sim_repository_->call(number);
#endif
}
void GpsSystem_BL::send_sms(const GpsSmsData& gps_sms_data) {
	char number[sizeof(APP_PHONE_NUMBER)] = {};
	load_phone_number_from_eeprom_with_prefix(number);
	char sms_message[56];
	const int written = snprintf(
		sms_message,
		sizeof(sms_message),
		"https://maps.google.com/?q=%ld.%06ld,%ld.%06ld",
		gps_sms_data.latitude_x1000000 / 1000000L,
		labs(gps_sms_data.latitude_x1000000 % 1000000L),
		gps_sms_data.longitude_x1000000 / 1000000L,
		labs(gps_sms_data.longitude_x1000000 % 1000000L));

#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
	Serial.print(F("sms: "));
	Serial.println(sms_message);
#endif
	//NON ATTIVARE MAI
	sim_repository_->listen();
	sim_repository_->sendSms(number, sms_message);
}
bool GpsSystem_BL::check_if_sms_arrived() {
	this->sim_repository_->listen();
	this->sim_repository_->delay(100); // Attendo un breve periodo per assicurarmi che eventuali SMS in arrivo vengano registrati.
	int sms_count = this->sim_repository_->getSmsCount();
	this->sim_repository_->delay(100); // Attendo un breve periodo per assicurarmi che eventuali SMS in arrivo vengano registrati.
	return sms_count > 0;
}
bool GpsSystem_BL::read_sms(char* out_message, uint8_t out_message_size) {
	out_message[0] = '\0';
	sim_repository_->listen();

	if (!sim_repository_->readSms(1U, out_message)) {
		return false;
	}

	out_message[out_message_size - 1U] = '\0';
	sim_repository_->deleteAllSms();
	return true;
}
void GpsSystem_BL::analyze_sms_message(const char* message, const GpsConnectionStatus& gps_status) {
	if (message == nullptr || message[0] == '\0') {
		return;
	}
	switch (message[0]) {
	case '#': {
		const char* phone_number = message + 1;
		if (store_phone_local_number_to_eeprom(*eeprom_repository_, phone_number)) {
			(void)call_phone_number();
		}

#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
		Serial.print(F("numero di telefono: "));
		Serial.println(phone_number);
		Serial.print(F("comando ricevuto: "));
		Serial.println(message[0]);
#endif

		break;
	}
	case 'D':{
		is_sleep_mode_enabled = true;
		is_interrupt_enabled = false;
		is_gps_flow_enabled = false;
		is_sim_power_off_enabled = false;
		this->call_phone_number();
		break;
	}
	case 'A':{
		is_interrupt_enabled = true;
		is_sleep_mode_enabled = true;
		is_gps_flow_enabled = false;
		is_sim_power_off_enabled = false;
		this->call_phone_number();
		break;
	}
	case 'S':{
		is_interrupt_enabled = true;
		is_sleep_mode_enabled = true;
		is_gps_flow_enabled = false;
		is_sim_power_off_enabled = true;
		this->call_phone_number();
		this->sim_repository_->delay(20000UL);
		(void)this->sim_repository_->turn_off_module();
		break;
	}
	case 'Q': {
		if (!is_gps_flow_enabled) {
			is_interrupt_enabled = false;
			is_gps_flow_enabled = true;
			is_sleep_mode_enabled = false;
			is_sim_power_off_enabled = false;
			this->call_phone_number();
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
			Serial.println(F("alarm disabled(phone call)+ gps flow enabled"));
#endif
			break;
		}
		if (!gps_status.has_position) {
			break;
		}
		const GpsSmsData gps_sms_data = {
			gps_status.latitude_x1000000,
			gps_status.longitude_x1000000,
			gps_status.altitude_m_x10,
			gps_status.speed_knots_x100
		};
		send_sms(gps_sms_data);
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
		Serial.print(F("comando sconosciuto: "));
		Serial.println(message);
#endif

		break;
	}
	}
}
void GpsSystem_BL::hang_up_phone_call() {
	this->sim_repository_->listen();
	this->sim_repository_->hangUp();
}
bool GpsSystem_BL::set_sim_netlight_enabled(bool enabled) {
	this->sim_repository_->listen();
	delay(5000);
	return this->sim_repository_->setNetlightEnabled(enabled);
}
void GpsSystem_BL::delete_all_sms() {
	this->sim_repository_->listen();
	this->sim_repository_->deleteAllSms();
}
void GpsSystem_BL::startSmsReception() {
	this->sim_repository_->listen();
	this->sim_repository_->initSmsReception();
}
bool GpsSystem_BL::load_phone_number_from_eeprom_with_prefix(char (&out_phone_number)[sizeof(APP_PHONE_NUMBER)]) const {
	uint8_t out_index = 0U;
	for (uint8_t i = 0U; APP_PHONE_PREFIX[i] != '\0'; ++i) {
		out_phone_number[out_index++] = APP_PHONE_PREFIX[i];
	}

	for (uint8_t i = 0U; i < APP_PHONE_LOCAL_NUMBER_LENGTH; ++i) {
		uint8_t value = 0U;
		if (!eeprom_repository_->read_uint8(i, value)) {
			return false;
		}
		out_phone_number[out_index++] = static_cast<char>(value);
	}
	out_phone_number[out_index] = '\0';
	return true;
}
bool GpsSystem_BL::store_phone_local_number_to_eeprom(EepromRepository& eeprom_repository, const char* phone_local_number) {
	if (phone_local_number == nullptr) {
		return false;
	}

	if (phone_local_number[APP_PHONE_LOCAL_NUMBER_LENGTH] != '\0') {
		return false;
	}

	for (uint8_t index = 0U; index < APP_PHONE_LOCAL_NUMBER_LENGTH; ++index) {
		const char c = phone_local_number[index];
		if (c < '0' || c > '9') {
			return false;
		}
		if (!eeprom_repository.write_uint8(index, static_cast<uint8_t>(c))) {
			return false;
		}
	}

	return eeprom_repository.write_uint8(APP_PHONE_LOCAL_NUMBER_LENGTH, static_cast<uint8_t>('\0'));
}
bool GpsSystem_BL::get_if_interrupt_enabled() {
	return is_interrupt_enabled;
}
bool GpsSystem_BL::get_if_gps_flow_enabled() {
	return is_gps_flow_enabled;
}
bool GpsSystem_BL::get_if_sleep_mode_enabled() {
	return is_sleep_mode_enabled;
}
bool GpsSystem_BL::get_if_sim_power_off_enabled() {
	return is_sim_power_off_enabled;
}
void GpsSystem_BL::disable_sleep_mode() {
	is_sleep_mode_enabled = false;
}
void GpsSystem_BL::disable_sim_power_off_mode() {
	is_sim_power_off_enabled = false;
}
