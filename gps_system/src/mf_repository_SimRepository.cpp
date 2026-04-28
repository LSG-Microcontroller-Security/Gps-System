// mf_repository_SimRepository.cpp
#include "mf_repository_SimRepository.h"
#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

#define AT_SMS_HEADER                "+CMGR:"
#define AT_SET_SMS_TEXT_MODE         "AT+CMGF=1"
#define AT_SET_SMS_PDU_MODE          "AT+CMGF=0"
#define AT_SET_SMS_CHARSET           "AT+CSCS=\"GSM\""
#define ATD_COMMAND                  "ATD+"
#define HANGUP_COMMAND               "ATH"
#define AT_SEND_SMS_PREFIX           "AT+CMGS=\""
#define LITERAL_OK                   "OK"
#define LITERAL_CTRL_Z               ((char)26)
#define LITERAL_RETURN               "\r\n"
#define AT_READ_SMS_PREFIX           "AT+CMGR="
#define AT_DELETE_ALL_SMS            "AT+CMGD=1,4"
#define AT_SET_SMS_MEMORY            "AT+CPMS=\"SM\",\"SM\",\"SM\""
#define AT_GET_SMS_STORAGE_STATUS    "AT+CPMS?"
#define AT_SMS_STORAGE_HEADER        "+CPMS:"
#define AT_DELETE_SMS_PREFIX         "AT+CMGD="
#define AT_NETWORK_REGISTRATION      "AT+CREG?"
#define AT_SIGNAL_STRENGTH           "AT+CSQ"
#define AT_GPRS_ATTACHED             "AT+CGATT?"
#define AT_SET_SMS_RECEIVE_IND       "AT+CNMI=1,1,0,0,0"

namespace {
constexpr uint8_t sim_response_buffer_size = 20;
constexpr uint8_t sim_small_response_buffer_size = 20;
constexpr uint8_t sim_sms_message_buffer_size = 16;

inline void send_at_cmd(ISerial& serial, const char* cmd) {
    serial.print(cmd);
    serial.print(LITERAL_RETURN);
}
}

SimRepository::SimRepository(ISerial& serial, uint8_t power_pin, unsigned long baud_rate)
    : serial_(serial), power_pin_(power_pin), baud_rate_(baud_rate), is_sms_receive_initialized_(false) {
    serial_.begin(baud_rate_);
}

bool SimRepository::call(const char* number) {
    clear_receive_buffer();
    serial_.print(ATD_COMMAND);
    serial_.print(number);
    serial_.print(";");
    serial_.print("\r");

    unsigned long start = millis();
    while (static_cast<unsigned long>(millis() - start) < 5000UL) {
        if (serial_.available() > 0) {
            int read_value = serial_.read();
            if (read_value >= 0) {
                return true;
            }
        }
    }
    return false;
}

void SimRepository::hangUp() {
    clear_receive_buffer();
    send_at_cmd(serial_, HANGUP_COMMAND);
    wait_for_pattern(LITERAL_OK, 2000UL);
}

void SimRepository::setSmsTextMode() {
    clear_receive_buffer();
    send_at_cmd(serial_, AT_SET_SMS_TEXT_MODE);
    wait_for_pattern(LITERAL_OK, 2000UL);
}

void SimRepository::setSmsPduMode() {
    clear_receive_buffer();
    send_at_cmd(serial_, AT_SET_SMS_PDU_MODE);
    wait_for_pattern(LITERAL_OK, 2000UL);
}

bool SimRepository::sendSms(const char* number, const char* message) {
    clear_receive_buffer();
    serial_.print(AT_SEND_SMS_PREFIX);
    serial_.print(number);
    serial_.print("\"");
    serial_.print(LITERAL_RETURN);

    unsigned long start = millis();
    bool got_prompt = false;
    while (static_cast<unsigned long>(millis() - start) < 3000UL) {
        if (serial_.available() > 0) {
            int read_value = serial_.read();
            if (read_value == '>') {
                got_prompt = true;
                break;
            }
        }
    }

    if (!got_prompt) {
        return false;
    }

    serial_.print(message);
    serial_.print(LITERAL_CTRL_Z);
    delay(1000UL);
    return true;
}

void SimRepository::initSmsReception() {
    bool ok = true;

    clear_receive_buffer();
    send_at_cmd(serial_, AT_SET_SMS_MEMORY);
    ok = ok && wait_for_pattern(LITERAL_OK, 5000UL);

    clear_receive_buffer();
    send_at_cmd(serial_, AT_SET_SMS_TEXT_MODE);
    ok = ok && wait_for_pattern(LITERAL_OK, 5000UL);

    clear_receive_buffer();
    send_at_cmd(serial_, AT_SET_SMS_CHARSET);
    ok = ok && wait_for_pattern(LITERAL_OK, 5000UL);

    // CNMI coerente con memorizzazione SMS in storage (SM) per lettura via CPMS/CMGR.
    clear_receive_buffer();
    send_at_cmd(serial_, "AT+CNMI=2,1,0,0,0");
    ok = ok && wait_for_pattern(LITERAL_OK, 5000UL);

    is_sms_receive_initialized_ = ok;
}


bool SimRepository::readSms(uint8_t index, char* message) {
    if (message == nullptr) {
        return false;
    }

    if (!is_sms_receive_initialized_) {
        initSmsReception();
    }

    clear_receive_buffer();
    serial_.print(AT_READ_SMS_PREFIX);
    serial_.print(index);
    serial_.print(LITERAL_RETURN);

    bool found_header = false;
    bool found_message = false;
    uint8_t message_pos = 0;
    uint8_t crlf_count = 0;
    unsigned long start = millis();
    char last_chars[7] = { 0 };

    while (static_cast<unsigned long>(millis() - start) < 2500UL && message_pos < static_cast<uint8_t>(sim_sms_message_buffer_size - 1U)) {
        if (serial_.available() <= 0) {
            continue;
        }

        int read_value = serial_.read();
        if (read_value < 0) {
            continue;
        }

        char c = static_cast<char>(read_value);
        if (!found_header) {
            memmove(last_chars, last_chars + 1, 6);
            last_chars[6] = c;
            if (strncmp(last_chars, AT_SMS_HEADER, 6) == 0) {
                found_header = true;
                crlf_count = 0;
            }
        }
        else if (!found_message) {
            if (c == '\n') {
                crlf_count++;
            }
            if (crlf_count == 1 && c != '\r' && c != '\n') {
                message[message_pos++] = c;
                found_message = true;
            }
        }
        else {
            if (c == '\r' || c == '\n') {
                break;
            }
            message[message_pos++] = c;
        }
    }

    message[message_pos] = '\0';
    return found_message && message_pos > 0;
}

void SimRepository::deleteAllSms() {
    clear_receive_buffer();
    send_at_cmd(serial_, AT_DELETE_ALL_SMS);
    wait_for_pattern(LITERAL_OK, 3000UL);
}

int SimRepository::getSmsCount() {
    if (!is_sms_receive_initialized_) {
        initSmsReception();
        if (!is_sms_receive_initialized_) {
            return -1;
        }
    }

    clear_receive_buffer();
    send_at_cmd(serial_, AT_GET_SMS_STORAGE_STATUS);

    char buffer[32];
    read_response(buffer, static_cast<uint8_t>(sizeof(buffer)), 3000UL);

    char* header = strstr(buffer, AT_SMS_STORAGE_HEADER);
    if (header == nullptr) {
        return -1;
    }

    // Formato tipico: +CPMS: "SM",<used>,<total>[,...]
    char* first_comma = strchr(header, ',');
    if (first_comma == nullptr) {
        return -1;
    }

    int used = atoi(first_comma + 1);
    if (used < 0) {
        return -1;
    }

    return used;
}


bool SimRepository::deleteSmsAt(uint8_t index) {
    clear_receive_buffer();
    serial_.print(AT_DELETE_SMS_PREFIX);
    serial_.print(index);
    serial_.print(LITERAL_RETURN);
    return wait_for_pattern(LITERAL_OK, 2000UL);
}

uint8_t SimRepository::findSmsWithContent(const char* pattern) {
    if (pattern == nullptr || pattern[0] == '\0') {
        return 0;
    }

    if (!is_sms_receive_initialized_) {
        initSmsReception();
        if (!is_sms_receive_initialized_) {
            return 0;
        }
    }

    int sms_count = getSmsCount();
    if (sms_count <= 0) {
        return 0;
    }

    char buffer[sim_sms_message_buffer_size];

    // Gli indici SMS reali possono avere buchi: cerco su una finestra piu' ampia.
    uint8_t max_index_to_scan = static_cast<uint8_t>(sms_count + 10);
    if (max_index_to_scan < static_cast<uint8_t>(sms_count)) {
        max_index_to_scan = 255; // protezione overflow
    }

    for (uint8_t i = 1; i <= max_index_to_scan; ++i) {
        if (!readSms(i, buffer)) {
            continue;
        }

        if (strcmp(buffer, pattern) == 0) {
            return i;
        }
    }

    return 0;
}


bool SimRepository::isRegisteredToNetwork() {
    clear_receive_buffer();
    send_at_cmd(serial_, AT_NETWORK_REGISTRATION);

    char buffer[sim_small_response_buffer_size];
    read_response(buffer, sim_small_response_buffer_size, 2000UL);

    char* creg = strstr(buffer, "+CREG:");
    if (creg == nullptr) {
        return false;
    }

    char* comma = strchr(creg, ',');
    if (comma == nullptr) {
        return false;
    }

    int stat = atoi(comma + 1);
    return stat == 1 || stat == 5;
}

int SimRepository::getSignalStrength() {
    clear_receive_buffer();
    send_at_cmd(serial_, AT_SIGNAL_STRENGTH);

    char buffer[sim_small_response_buffer_size];
    read_response(buffer, sim_small_response_buffer_size, 2000UL);

    char* csq = strstr(buffer, "+CSQ:");
    if (csq == nullptr) {
        return -1;
    }

    int rssi = atoi(csq + 5);
    if (rssi >= 0 && rssi <= 31) {
        return rssi;
    }

    return -1;
}

bool SimRepository::isGprsAttached() {
    clear_receive_buffer();
    send_at_cmd(serial_, AT_GPRS_ATTACHED);

    char buffer[sim_small_response_buffer_size];
    read_response(buffer, sim_small_response_buffer_size, 2000UL);

    char* ptr = strstr(buffer, "+CGATT:");
    if (ptr == nullptr) {
        return false;
    }

    int status = atoi(ptr + 7);
    return status == 1;
}

void SimRepository::clear_receive_buffer() {
    while (serial_.available() > 0) {
        serial_.read();
    }
}

bool SimRepository::wait_for_pattern(const char* pattern, unsigned long timeout_ms) {
    if (pattern == nullptr || pattern[0] == '\0') {
        return false;
    }

    const char* match = pattern;
    unsigned long start = millis();
    while (static_cast<unsigned long>(millis() - start) < timeout_ms) {
        if (serial_.available() <= 0) {
            continue;
        }

        int read_value = serial_.read();
        if (read_value < 0) {
            continue;
        }

        char c = static_cast<char>(read_value);
        if (c == *match) {
            match++;
            if (*match == '\0') {
                return true;
            }
        }
        else {
            match = (c == pattern[0]) ? pattern + 1 : pattern;
            if (*match == '\0') {
                return true;
            }
        }
    }

    return false;
}

uint8_t SimRepository::read_response(char* buffer, uint8_t buffer_size, unsigned long timeout_ms) {
    if (buffer == nullptr || buffer_size == 0) {
        return 0;
    }

    uint8_t pos = 0;
    unsigned long start = millis();

    while (static_cast<unsigned long>(millis() - start) < timeout_ms &&
        pos < static_cast<uint8_t>(buffer_size - 1U)) {
        if (serial_.available() <= 0) {
            continue;
        }

        int read_value = serial_.read();
        if (read_value < 0) {
            continue;
        }

        buffer[pos++] = static_cast<char>(read_value);
        buffer[pos] = '\0';

        // Stop anticipato appena arriva terminatore modem.
        if (strstr(buffer, "\r\nOK\r\n") != nullptr ||
            strstr(buffer, "\r\nERROR\r\n") != nullptr) {
            break;
        }

        // In alcuni modem il terminatore puo' arrivare come "OK\r\n".
        if (strstr(buffer, "OK\r\n") != nullptr ||
            strstr(buffer, "ERROR\r\n") != nullptr) {
            break;
        }
    }

    buffer[pos] = '\0';
    return pos;
}


bool SimRepository::listen() {
    return serial_.listen();
}

bool SimRepository::is_listening() {
    return serial_.isListening();
}

bool SimRepository::stop_listening() {
    return serial_.stopListening();
}

void SimRepository::delay(unsigned long ms) {
    ::delay(ms);
}
