// mf_repository_NmeaGpsRepository.cpp
#include "mf_repository_NmeaGpsRepository.h"
#include <Arduino.h>
namespace {
constexpr unsigned long nmea_gps_default_baud_rate = 9600UL;
constexpr unsigned long nmea_gps_default_timeout_ms = 1000UL;
constexpr uint8_t nmea_min_sentence_length = 7;
}

NmeaGpsRepository::NmeaGpsRepository(ISerial& serial)
    : serial_(serial), timeout_ms_(nmea_gps_default_timeout_ms) {
}

void NmeaGpsRepository::begin() {
    serial_.begin(nmea_gps_default_baud_rate);
    serial_.setTimeout(timeout_ms_);
    clear_receive_buffer();
}

bool NmeaGpsRepository::listen() {
    return serial_.listen();
}

bool NmeaGpsRepository::is_listening() {
    return serial_.isListening();
}

bool NmeaGpsRepository::stop_listening() {
    return serial_.stopListening();
}

void NmeaGpsRepository::set_timeout(unsigned long timeout_ms) {
    timeout_ms_ = timeout_ms;
    serial_.setTimeout(timeout_ms_);
}

bool NmeaGpsRepository::read_sentence(char* out_sentence, uint8_t max_length) {
    if (out_sentence == nullptr || max_length < nmea_min_sentence_length) {
        return false;
    }
    unsigned long start_ms = millis();
    uint8_t index = 0;
    bool sentence_started = false;

    while (static_cast<unsigned long>(millis() - start_ms) < timeout_ms_) {
        if (serial_.available() <= 0) {
            continue;
        }

        int read_value = serial_.read();
        if (read_value < 0) {
            continue;
        }
        char current_char = static_cast<char>(read_value);
        if (current_char == '$') {
            sentence_started = true;
            index = 0;
            out_sentence[index++] = current_char;
            continue;
        }

        if (!sentence_started) {
            continue;
        }
        if (current_char == '\r') {
            continue;
        }

        if (current_char == '\n') {
            out_sentence[index] = '\0';
            return true;
        }

        if (index >= static_cast<uint8_t>(max_length - 1)) {
            out_sentence[0] = '\0';
            return false;
        }

        out_sentence[index++] = current_char;
    }
    out_sentence[0] = '\0';
    return false;
}

void NmeaGpsRepository::clear_receive_buffer() {
    while (serial_.available() > 0) {
        serial_.read();
    }
}
