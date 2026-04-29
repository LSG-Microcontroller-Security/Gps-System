// mf_activity_NmeaGpsActivity.h
#pragma once

#include <stdint.h>
#include "mf_repository_NmeaGpsRepository.h"

struct GpsConnectionStatus {
    int32_t latitude_x1000000;
    int32_t longitude_x1000000;
    int32_t altitude_m_x10;
    int32_t speed_knots_x100;
    uint8_t cycles_since_last_position;
    bool has_fix;
    bool has_position;
    bool has_altitude;
    bool has_speed;
    bool has_recent_coordinates;
};

class NmeaGpsActivity {
public:
    explicit NmeaGpsActivity(NmeaGpsRepository& nmea_gps_repository);
    ~NmeaGpsActivity() = default;

    const GpsConnectionStatus& update_gps_status();
    NmeaGpsRepository& get_repository();
    const NmeaGpsRepository& get_repository() const;

private:
    static const uint8_t nmea_sentence_buffer_length = 83;
    static const uint8_t max_sentences_per_cycle = 6;

    bool try_read_data(GpsConnectionStatus& out_data, uint8_t max_sentences_to_read = max_sentences_per_cycle);
    bool parse_sentence(const char* sentence, GpsConnectionStatus& data) const;
    bool parse_gga_sentence(const char* sentence, GpsConnectionStatus& data) const;
    bool parse_rmc_sentence(const char* sentence, GpsConnectionStatus& data) const;
    bool is_sentence_checksum_valid(const char* sentence) const;
    int8_t hex_to_nibble(char value) const;
    bool is_sentence_type(const char* sentence, char first, char second, char third) const;
    bool get_field(const char* sentence, uint8_t field_index, const char*& field, uint8_t& length) const;
    bool parse_uint_field(const char* field, uint8_t length, uint16_t& out_value) const;
    bool parse_decimal_scaled(const char* field, uint8_t length, uint8_t scale_digits, int32_t& out_value) const;
    bool parse_coordinate_field(const char* field, uint8_t length, char hemisphere, uint8_t degree_digits, int32_t& out_value) const;
    void clear_data(GpsConnectionStatus& data) const;

private:
    NmeaGpsRepository& nmea_gps_repository_;
    bool has_last_coordinates_;
    GpsConnectionStatus last_status_;
};
