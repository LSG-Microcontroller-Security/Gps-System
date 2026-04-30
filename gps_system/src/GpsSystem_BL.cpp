// File: GpsSystem_BL.cpp
#include "GpsSystem_BL.h"
#include <stdio.h>
#include <stdlib.h>
#include <Arduino.h>
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
	if (gps_activity_ == nullptr) {
		return false;
	}
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
bool GpsSystem_BL::call_phone_number() {
    if (sim_repository_ == nullptr) {
        return false;
	}
    char number[sizeof(APP_PHONE_NUMBER)] = {};
    if (!load_phone_number_from_eeprom_with_prefix(number)) {
        return false;
    }
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
    Serial.print(F("calling number: "));
    Serial.println(number);
#endif
	sim_repository_->listen();
    return sim_repository_->call(number);
}
bool GpsSystem_BL::send_sms(const GpsSmsData& gps_sms_data) {
    if (sim_repository_ == nullptr) {
        return false;
    }

    char number[sizeof(APP_PHONE_NUMBER)] = {};
    if (!load_phone_number_from_eeprom_with_prefix(number)) {
        return false;
    }

    if (number[0] == '\0') {
        return false;
    }
    char sms_message[112];
    const int written = snprintf(
        sms_message,
        sizeof(sms_message),
        "https://www.google.com/maps/search/?api=1&query=%ld.%06ld,%ld.%06ld ALT=%ld.%01ldm SPD=%ld.%02ldkn",
        gps_sms_data.latitude_x1000000 / 1000000L,
        labs(gps_sms_data.latitude_x1000000 % 1000000L),
        gps_sms_data.longitude_x1000000 / 1000000L,
        labs(gps_sms_data.longitude_x1000000 % 1000000L),
        gps_sms_data.altitude_m_x10 / 10L,
        labs(gps_sms_data.altitude_m_x10 % 10L),
        gps_sms_data.speed_knots_x100 / 100L,
        labs(gps_sms_data.speed_knots_x100 % 100L));

    if (written <= 0 || written >= static_cast<int>(sizeof(sms_message))) {
        return false;
    }

#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
    Serial.print(F("sms: "));
    Serial.println(sms_message);
#endif
    //NON ATTIVARE MAI
    /*sim_repository_->listen();
    const bool is_sent = sim_repository_->sendSms(number, sms_message);*/
    //return is_sent;
	return false;
}
bool GpsSystem_BL::check_if_sms_arrived() {
	if (sim_repository_ == nullptr) {
			return false;
	}
	this->sim_repository_->listen();
	this->sim_repository_->delay(100); // Attendo un breve periodo per assicurarmi che eventuali SMS in arrivo vengano registrati.
	int sms_count = this->sim_repository_->getSmsCount();
	this->sim_repository_->delay(100); // Attendo un breve periodo per assicurarmi che eventuali SMS in arrivo vengano registrati.
	return sms_count > 0;
}
bool GpsSystem_BL::read_sms(char* out_message, uint8_t out_message_size) {
    if (sim_repository_ == nullptr || out_message == nullptr || out_message_size == 0U) {
        return false;
    }

    out_message[0] = '\0';
    sim_repository_->listen();

    if (!sim_repository_->readSms(1U, out_message)) {
        return false;
    }

    out_message[out_message_size - 1U] = '\0';
    (void)sim_repository_->deleteSmsAt(1U);
    return true;
}
void GpsSystem_BL::analyze_sms_message(const char* message) {
    if (message == nullptr || message[0] == '\0') {
        return;
    }

    switch (message[0]) {
    case '#': {
        const char* phone_number = message + 1;

        if (eeprom_repository_ != nullptr) {
            if (store_phone_local_number_to_eeprom(*eeprom_repository_, phone_number)) {
                (void)call_phone_number();
            }
        }

#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
        Serial.print(F("numero di telefono: "));
        Serial.println(phone_number);
        Serial.print(F("comando ricevuto: "));
        Serial.println(message[0]);
#endif

        break;
    }

    case '*': {
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
        Serial.print(F("comando asterisco: "));
        Serial.println(message + 1);
#endif

        break;
    }

    default: {
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
        Serial.print(F("comando sconosciuto: "));
        Serial.println(message);
#endif

        break;
    }
    }
}
void GpsSystem_BL::hang_up_phone_call() {
	if (sim_repository_ == nullptr) {
		return;
	}
	this->sim_repository_->listen();
	this->sim_repository_->hangUp();
}
void GpsSystem_BL::startSmsReception() {
	if (sim_repository_ == nullptr) {
		return;
	}
	this->sim_repository_->listen();
	this->sim_repository_->initSmsReception();
}
bool GpsSystem_BL::load_phone_number_from_eeprom_with_prefix(char (&out_phone_number)[sizeof(APP_PHONE_NUMBER)]) const {
    if (eeprom_repository_ == nullptr) {
        return false;
    }

    uint8_t out_index = 0U;
    for (uint8_t i = 0U; APP_PHONE_PREFIX[i] != '\0'; ++i) {
        out_phone_number[out_index++] = APP_PHONE_PREFIX[i];
    }

    for (uint8_t i = 0U; i < APP_PHONE_LOCAL_NUMBER_LENGTH; ++i) {
        uint8_t value = 0U;
        if (!eeprom_repository_->read_uint8(i, value)) {
            return false;
        }
        const char c = static_cast<char>(value);
        if (c < '0' || c > '9') {
            return false;
        }
        out_phone_number[out_index++] = c;
    }
    out_phone_number[out_index] = '\0';
    return true;
}
bool GpsSystem_BL::store_phone_local_number_to_eeprom(EepromRepository& eeprom_repository, const char* phone_local_number) {
    if (phone_local_number == nullptr) {
        return false;
    }

    const char* local_number_ptr = phone_local_number;
    uint8_t payload_len = 0U;
    while (phone_local_number[payload_len] != '\0') {
        ++payload_len;
    }

    uint8_t prefix_len = 0U;
    while (APP_PHONE_PREFIX[prefix_len] != '\0') {
        ++prefix_len;
    }

    if (payload_len == static_cast<uint8_t>(prefix_len + APP_PHONE_LOCAL_NUMBER_LENGTH)) {
        for (uint8_t i = 0U; i < prefix_len; ++i) {
            if (phone_local_number[i] != APP_PHONE_PREFIX[i]) {
                return false;
            }
        }
        local_number_ptr = phone_local_number + prefix_len;
    }
    else if (payload_len != APP_PHONE_LOCAL_NUMBER_LENGTH) {
        return false;
    }

    for (uint8_t index = 0U; index < APP_PHONE_LOCAL_NUMBER_LENGTH; ++index) {
        const char c = local_number_ptr[index];
        if (c < '0' || c > '9') {
            return false;
        }
        if (!eeprom_repository.write_uint8(index, static_cast<uint8_t>(c))) {
            return false;
        }
    }

    return eeprom_repository.write_uint8(APP_PHONE_LOCAL_NUMBER_LENGTH, static_cast<uint8_t>('\0'));
}
