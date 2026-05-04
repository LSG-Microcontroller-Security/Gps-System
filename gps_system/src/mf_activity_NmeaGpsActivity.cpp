// mf_activity_NmeaGpsActivity.cpp
#include "mf_activity_NmeaGpsActivity.h"
#ifdef _DEBUG_FOR_SERIAL
#include <Arduino.h>
#endif // _DEBUG_FOR_SERIAL 

namespace {
constexpr uint8_t max_cycles_without_position_before_stale = 15U;
constexpr uint8_t nmea_checksum_chars = 2U;
}

NmeaGpsActivity::NmeaGpsActivity(NmeaGpsRepository& nmea_gps_repository)
    : nmea_gps_repository_(nmea_gps_repository),
      has_last_coordinates_(false) {
    clear_data(last_status_);
    nmea_gps_repository_.begin();
}

NmeaGpsRepository& NmeaGpsActivity::get_repository() {
    return nmea_gps_repository_;
}

const NmeaGpsRepository& NmeaGpsActivity::get_repository() const {
    return nmea_gps_repository_;
}

bool NmeaGpsActivity::try_read_data(GpsConnectionStatus& out_data, uint8_t max_sentences_to_read) {
    clear_data(out_data);
    bool parsed_any_sentence = false;
    char sentence[nmea_sentence_buffer_length];
    for (uint8_t sentence_index = 0; sentence_index < max_sentences_to_read; ++sentence_index) {
        if (!nmea_gps_repository_.read_sentence(sentence, nmea_sentence_buffer_length)) {
            continue;
        }
        if (!is_sentence_checksum_valid(sentence)) {
            continue;
        }
        if (parse_sentence(sentence, out_data)) {
            parsed_any_sentence = true;
        }
    }

    return parsed_any_sentence;
}

const GpsConnectionStatus& NmeaGpsActivity::update_gps_status() {
    GpsConnectionStatus parsed_data = {};
    const bool is_read = try_read_data(parsed_data);

    if (is_read) {
        last_status_.has_fix = parsed_data.has_fix;
        last_status_.has_position = parsed_data.has_position;

        if (parsed_data.has_fix && parsed_data.has_position) {
            has_last_coordinates_ = true;
            last_status_.latitude_x1000000 = parsed_data.latitude_x1000000;
            last_status_.longitude_x1000000 = parsed_data.longitude_x1000000;
            last_status_.altitude_m_x10 = parsed_data.altitude_m_x10;
            last_status_.speed_knots_x100 = parsed_data.speed_knots_x100;
            last_status_.has_altitude = parsed_data.has_altitude;
            last_status_.has_speed = parsed_data.has_speed;
            last_status_.cycles_since_last_position = 0U;
            last_status_.has_recent_coordinates = true;
            return last_status_;
        }
    } else {
        last_status_.has_fix = false;
        last_status_.has_position = false;
    }

    if (!has_last_coordinates_) {
        last_status_.latitude_x1000000 = 0L;
        last_status_.longitude_x1000000 = 0L;
        last_status_.altitude_m_x10 = 0L;
        last_status_.speed_knots_x100 = 0L;
        last_status_.has_altitude = false;
        last_status_.has_speed = false;
        last_status_.cycles_since_last_position = 0U;
        last_status_.has_recent_coordinates = false;
        return last_status_;
    }

    if (last_status_.cycles_since_last_position < UINT8_MAX) {
        ++last_status_.cycles_since_last_position;
    }

    last_status_.has_recent_coordinates =
        last_status_.cycles_since_last_position <= max_cycles_without_position_before_stale;

    return last_status_;
}

bool NmeaGpsActivity::parse_sentence(const char* sentence, GpsConnectionStatus& data) const {
    if (is_sentence_type(sentence, 'G', 'G', 'A')) {
        return parse_gga_sentence(sentence, data);
    }

    if (is_sentence_type(sentence, 'R', 'M', 'C')) {
        return parse_rmc_sentence(sentence, data);
    }

    return false;
}

bool NmeaGpsActivity::parse_gga_sentence(const char* sentence, GpsConnectionStatus& data) const {
    const char* field = nullptr;
    uint8_t length = 0;

    uint16_t fix_quality = 0;
    if (!get_field(sentence, 6, field, length) || !parse_uint_field(field, length, fix_quality)) {
        return false;
    }

    const bool has_fix_in_sentence = fix_quality > 0U;
    data.has_fix = data.has_fix || has_fix_in_sentence;
    if (!has_fix_in_sentence) {
        return true;
    }

    const char* latitude_field = nullptr;
    const char* longitude_field = nullptr;
    const char* ns_field = nullptr;
    const char* ew_field = nullptr;
    uint8_t latitude_length = 0;
    uint8_t longitude_length = 0;
    uint8_t ns_length = 0;
    uint8_t ew_length = 0;

    if (!get_field(sentence, 2, latitude_field, latitude_length) ||
        !get_field(sentence, 3, ns_field, ns_length) ||
        !get_field(sentence, 4, longitude_field, longitude_length) ||
        !get_field(sentence, 5, ew_field, ew_length)) {
        return true;
    }

    int32_t latitude_x1000000 = 0L;
    int32_t longitude_x1000000 = 0L;
    if (ns_length == 1U &&
        ew_length == 1U &&
        parse_coordinate_field(latitude_field, latitude_length, ns_field[0], 2, latitude_x1000000) &&
        parse_coordinate_field(longitude_field, longitude_length, ew_field[0], 3, longitude_x1000000)) {
        data.latitude_x1000000 = latitude_x1000000;
        data.longitude_x1000000 = longitude_x1000000;
        data.has_position = true;
    }

    const char* altitude_field = nullptr;
    uint8_t altitude_length = 0;
    int32_t altitude_m_x10 = 0L;
    if (get_field(sentence, 9, altitude_field, altitude_length) &&
        parse_decimal_scaled(altitude_field, altitude_length, 1U, altitude_m_x10)) {
        data.altitude_m_x10 = altitude_m_x10;
        data.has_altitude = true;
    }

    return true;
}

bool NmeaGpsActivity::parse_rmc_sentence(const char* sentence, GpsConnectionStatus& data) const {
    const char* field = nullptr;
    uint8_t length = 0;

    if (!get_field(sentence, 2, field, length) || length != 1U) {
        return false;
    }

    const bool has_fix_in_sentence = field[0] == 'A';
    data.has_fix = data.has_fix || has_fix_in_sentence;
    if (!has_fix_in_sentence) {
        return true;
    }

    const char* latitude_field = nullptr;
    const char* longitude_field = nullptr;
    const char* ns_field = nullptr;
    const char* ew_field = nullptr;
    uint8_t latitude_length = 0;
    uint8_t longitude_length = 0;
    uint8_t ns_length = 0;
    uint8_t ew_length = 0;

    if (!get_field(sentence, 3, latitude_field, latitude_length) ||
        !get_field(sentence, 4, ns_field, ns_length) ||
        !get_field(sentence, 5, longitude_field, longitude_length) ||
        !get_field(sentence, 6, ew_field, ew_length)) {
        return true;
    }

    int32_t latitude_x1000000 = 0L;
    int32_t longitude_x1000000 = 0L;
    if (ns_length == 1U &&
        ew_length == 1U &&
        parse_coordinate_field(latitude_field, latitude_length, ns_field[0], 2, latitude_x1000000) &&
        parse_coordinate_field(longitude_field, longitude_length, ew_field[0], 3, longitude_x1000000)) {
        data.latitude_x1000000 = latitude_x1000000;
        data.longitude_x1000000 = longitude_x1000000;
        data.has_position = true;
    }

    const char* speed_field = nullptr;
    uint8_t speed_length = 0;
    int32_t speed_knots_x100 = 0L;
    if (get_field(sentence, 7, speed_field, speed_length) &&
        parse_decimal_scaled(speed_field, speed_length, 2U, speed_knots_x100) &&
        speed_knots_x100 >= 0L) {
        data.speed_knots_x100 = speed_knots_x100;
        data.has_speed = true;
    }

    return true;
}

bool NmeaGpsActivity::is_sentence_checksum_valid(const char* sentence) const {
    if (sentence == nullptr || sentence[0] != '$') {
        return false;
    }

    uint8_t checksum = 0U;
    uint8_t index = 1U;

    while (sentence[index] != '\0' && sentence[index] != '*') {
        checksum ^= static_cast<uint8_t>(sentence[index]);
        ++index;
    }

    if (sentence[index] != '*') {
        return false;
    }

    const int8_t high_nibble = hex_to_nibble(sentence[index + 1U]);
    const int8_t low_nibble = hex_to_nibble(sentence[index + 2U]);
    if (high_nibble < 0 || low_nibble < 0) {
        return false;
    }

    if (sentence[index + 1U + nmea_checksum_chars] != '\0') {
        return false;
    }

    const uint8_t received_checksum = static_cast<uint8_t>(
        (static_cast<uint8_t>(high_nibble) << 4U) | static_cast<uint8_t>(low_nibble));
    return checksum == received_checksum;
}

int8_t NmeaGpsActivity::hex_to_nibble(char value) const {
    if (value >= '0' && value <= '9') {
        return static_cast<int8_t>(value - '0');
    }

    if (value >= 'A' && value <= 'F') {
        return static_cast<int8_t>(value - 'A' + 10);
    }

    if (value >= 'a' && value <= 'f') {
        return static_cast<int8_t>(value - 'a' + 10);
    }

    return -1;
}

bool NmeaGpsActivity::is_sentence_type(const char* sentence, char first, char second, char third) const {
    return sentence != nullptr &&
        sentence[0] == '$' &&
        sentence[3] == first &&
        sentence[4] == second &&
        sentence[5] == third;
}

bool NmeaGpsActivity::get_field(const char* sentence, uint8_t field_index, const char*& field, uint8_t& length) const {
   /* if (sentence == nullptr) {
        return false;
    }*/

    const char* current = sentence;
    if (*current == '$') {
        ++current;
    }

    const char* field_start = current;
    uint8_t current_field_index = 0U;
    while (*current != '\0') {
        if (*current == ',' || *current == '*') {
            if (current_field_index == field_index) {
                field = field_start;
                length = static_cast<uint8_t>(current - field_start);
                return true;
            }

            if (*current == '*') {
                return false;
            }

            ++current_field_index;
            field_start = current + 1;
        }

        ++current;
    }

    return false;
}

bool NmeaGpsActivity::parse_uint_field(const char* field, uint8_t length, uint16_t& out_value) const {
    if (field == nullptr || length == 0U) {
        return false;
    }

    uint16_t value = 0U;
    for (uint8_t index = 0U; index < length; ++index) {
        if (field[index] < '0' || field[index] > '9') {
            return false;
        }

        value = static_cast<uint16_t>((value * 10U) + static_cast<uint16_t>(field[index] - '0'));
    }

    out_value = value;
    return true;
}

bool NmeaGpsActivity::parse_decimal_scaled(const char* field, uint8_t length, uint8_t scale_digits, int32_t& out_value) const {
    if (field == nullptr || length == 0U) {
        return false;
    }

    bool is_negative = false;
    bool has_digit = false;
    bool has_decimal_separator = false;
    uint8_t fraction_digits = 0U;
    int32_t value = 0L;
    uint8_t index = 0U;

    if (field[0] == '-') {
        is_negative = true;
        index = 1U;
    }

    for (; index < length; ++index) {
        const char current_char = field[index];

        if (current_char == '.') {
            if (has_decimal_separator) {
                return false;
            }
            has_decimal_separator = true;
            continue;
        }

        if (current_char < '0' || current_char > '9') {
            return false;
        }

        has_digit = true;
        if (!has_decimal_separator || fraction_digits < scale_digits) {
            value = (value * 10L) + static_cast<int32_t>(current_char - '0');
        }

        if (has_decimal_separator && fraction_digits < scale_digits) {
            ++fraction_digits;
        }
    }

    if (!has_digit) {
        return false;
    }

    while (fraction_digits < scale_digits) {
        value *= 10L;
        ++fraction_digits;
    }

    out_value = is_negative ? -value : value;
    return true;
}

bool NmeaGpsActivity::parse_coordinate_field(
    const char* field,
    uint8_t length,
    char hemisphere,
    uint8_t degree_digits,
    int32_t& out_value) const {
    if (field == nullptr || length <= degree_digits) {
        return false;
    }

    uint16_t degrees = 0U;
    for (uint8_t index = 0U; index < degree_digits; ++index) {
        if (field[index] < '0' || field[index] > '9') {
            return false;
        }
        degrees = static_cast<uint16_t>((degrees * 10U) + static_cast<uint16_t>(field[index] - '0'));
    }

    int32_t minutes_x100000 = 0L;
    if (!parse_decimal_scaled(
            field + degree_digits,
            static_cast<uint8_t>(length - degree_digits),
            5U,
            minutes_x100000) ||
        minutes_x100000 < 0L) {
        return false;
    }

    int32_t coordinate_x1000000 = static_cast<int32_t>(degrees) * 1000000L;
    coordinate_x1000000 += static_cast<int32_t>((minutes_x100000 + 3L) / 6L);

    if (hemisphere == 'S' || hemisphere == 'W') {
        coordinate_x1000000 = -coordinate_x1000000;
    } else if (hemisphere != 'N' && hemisphere != 'E') {
        return false;
    }

    out_value = coordinate_x1000000;
    return true;
}

void NmeaGpsActivity::clear_data(GpsConnectionStatus& data) const {
    data.latitude_x1000000 = 0L;
    data.longitude_x1000000 = 0L;
    data.altitude_m_x10 = 0L;
    data.speed_knots_x100 = 0L;
    data.cycles_since_last_position = 0U;
    data.has_fix = false;
    data.has_position = false;
    data.has_altitude = false;
    data.has_speed = false;
    data.has_recent_coordinates = false;
}
