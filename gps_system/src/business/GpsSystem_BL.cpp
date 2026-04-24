// File: GpsSystem_BL.cpp

#include "GpsSystem_BL.h"

#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
#include <Arduino.h>
#endif

namespace {
constexpr uint16_t max_consecutive_read_failures_before_problem = 3U;
constexpr uint16_t max_cycles_without_position_before_stale = 15U;

#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
void print_state_label(GpsWorkflowState state) {
    switch (state) {
    case GpsWorkflowState::waiting_for_data:
        Serial.print(F("waiting_data"));
        break;
    case GpsWorkflowState::connected_waiting_fix:
        Serial.print(F("waiting_fix"));
        break;
    case GpsWorkflowState::connected_waiting_position:
        Serial.print(F("waiting_position"));
        break;
    case GpsWorkflowState::coordinates_ready:
        Serial.print(F("coordinates_ready"));
        break;
    case GpsWorkflowState::coordinates_stale:
        Serial.print(F("coordinates_stale"));
        break;
    case GpsWorkflowState::connection_problem:
        Serial.print(F("connection_problem"));
        break;
    default:
        Serial.print(F("unknown"));
        break;
    }
}

void print_two_digits(uint8_t value) {
    if (value < 10U) {
        Serial.print('0');
    }

    Serial.print(static_cast<int32_t>(value));
}

void print_unsigned_fixed(uint32_t value, uint8_t fractional_digits) {
    uint32_t divisor = 1UL;
    for (uint8_t digit = 0U; digit < fractional_digits; ++digit) {
        divisor *= 10UL;
    }

    Serial.print(static_cast<uint32_t>(value / divisor));

    if (fractional_digits == 0U) {
        return;
    }

    Serial.print('.');
    uint32_t fractional_part = value % divisor;
    uint32_t place = divisor / 10UL;

    while (place > 0UL) {
        Serial.print(static_cast<char>('0' + (fractional_part / place)));
        fractional_part %= place;
        place /= 10UL;
    }
}

void print_signed_fixed(int32_t value, uint8_t fractional_digits) {
    uint32_t absolute_value = static_cast<uint32_t>(value);

    if (value < 0L) {
        Serial.print('-');
        absolute_value = static_cast<uint32_t>(-(value + 1L)) + 1UL;
    }

    print_unsigned_fixed(absolute_value, fractional_digits);
}

void print_coordinate_x1000000(int32_t coordinate_x1000000) {
    uint32_t absolute_coordinate = static_cast<uint32_t>(coordinate_x1000000);
    if (coordinate_x1000000 < 0L) {
        Serial.print('-');
        absolute_coordinate = static_cast<uint32_t>(-(coordinate_x1000000 + 1L)) + 1UL;
    }

    uint32_t integer_part = absolute_coordinate / 1000000UL;
    uint32_t fractional_part = absolute_coordinate % 1000000UL;
    char fractional_digits[6];

    for (int8_t index = 5; index >= 0; --index) {
        fractional_digits[index] = static_cast<char>('0' + (fractional_part % 10UL));
        fractional_part /= 10UL;
    }

    Serial.print(integer_part);
    Serial.print('.');
    for (uint8_t index = 0U; index < 6U; ++index) {
        Serial.print(fractional_digits[index]);
    }
}
#endif
}

GpsSystem_BL::GpsSystem_BL(mf_activity_NmeaGpsActivity& nmea_gps_activity)
    : nmea_gps_activity(nmea_gps_activity),
      has_last_coordinates(false) {
    this->set_status_defaults(this->last_status);
}

void GpsSystem_BL::set_status_defaults(GpsConnectionStatus& gps_status) const {
    gps_status.state = GpsWorkflowState::waiting_for_data;
    gps_status.is_connected_to_stream = false;
    gps_status.has_fix = false;
    gps_status.has_position = false;
    gps_status.has_altitude = false;
    gps_status.has_recent_coordinates = false;
    gps_status.has_connection_problem = false;
    gps_status.should_wait = true;
    gps_status.consecutive_read_failures = 0U;
    gps_status.consecutive_no_fix_reads = 0U;
    gps_status.satellites = 0U;
    gps_status.fix_quality = 0U;
    gps_status.latitude_x1000000 = 0L;
    gps_status.longitude_x1000000 = 0L;
    gps_status.altitude_meters_x10 = 0L;
    gps_status.cycles_since_last_position = 0U;
}

void GpsSystem_BL::apply_last_coordinates(GpsConnectionStatus& gps_status, uint16_t cycles_since_last_position) const {
    if (!this->has_last_coordinates) {
        gps_status.has_recent_coordinates = false;
        gps_status.cycles_since_last_position = 0U;
        return;
    }

    gps_status.latitude_x1000000 = this->last_status.latitude_x1000000;
    gps_status.longitude_x1000000 = this->last_status.longitude_x1000000;
    gps_status.altitude_meters_x10 = this->last_status.altitude_meters_x10;
    gps_status.has_altitude = this->last_status.has_altitude;
    gps_status.cycles_since_last_position = cycles_since_last_position;
    gps_status.has_recent_coordinates = cycles_since_last_position <= max_cycles_without_position_before_stale;
}

bool GpsSystem_BL::try_read_gps_data(NmeaGpsData& gps_data) {
    return this->nmea_gps_activity.try_read_data(gps_data);
}

bool GpsSystem_BL::try_get_coordinates(int32_t& latitude_x1000000, int32_t& longitude_x1000000) {
    GpsConnectionStatus gps_status = this->update_gps_status();
    if (!gps_status.has_recent_coordinates) {
        return false;
    }

    latitude_x1000000 = gps_status.latitude_x1000000;
    longitude_x1000000 = gps_status.longitude_x1000000;
    return true;
}

GpsConnectionStatus GpsSystem_BL::update_gps_status() {
    GpsConnectionStatus gps_status;
    this->set_status_defaults(gps_status);

    uint16_t consecutive_read_failures = this->last_status.consecutive_read_failures;
    uint16_t consecutive_no_fix_reads = this->last_status.consecutive_no_fix_reads;
    uint16_t cycles_since_last_position = this->last_status.cycles_since_last_position;

    this->apply_last_coordinates(gps_status, cycles_since_last_position);
    gps_status.consecutive_read_failures = consecutive_read_failures;
    gps_status.consecutive_no_fix_reads = consecutive_no_fix_reads;

    NmeaGpsData gps_data = {};
    const bool is_read = this->try_read_gps_data(gps_data);

    if (!is_read) {
        if (consecutive_read_failures < UINT16_MAX) {
            ++consecutive_read_failures;
        }

        gps_status.consecutive_read_failures = consecutive_read_failures;

        if (this->has_last_coordinates && cycles_since_last_position < UINT16_MAX) {
            ++cycles_since_last_position;
        }

        this->apply_last_coordinates(gps_status, cycles_since_last_position);

        if (consecutive_read_failures >= max_consecutive_read_failures_before_problem) {
            gps_status.state = GpsWorkflowState::connection_problem;
            gps_status.has_connection_problem = true;
        } else if (this->has_last_coordinates && !gps_status.has_recent_coordinates) {
            gps_status.state = GpsWorkflowState::coordinates_stale;
            gps_status.has_connection_problem = true;
        } else {
            gps_status.state = GpsWorkflowState::waiting_for_data;
        }

        gps_status.should_wait = true;
        this->last_status = gps_status;
        this->log_connection_status(gps_status);
        return gps_status;
    }

    consecutive_read_failures = 0U;
    gps_status.consecutive_read_failures = 0U;
    gps_status.is_connected_to_stream = true;
    gps_status.has_fix = gps_data.has_fix;
    gps_status.has_position = gps_data.has_position;
    gps_status.satellites = gps_data.satellites;
    gps_status.fix_quality = gps_data.fix_quality;

    if (gps_data.has_altitude) {
        gps_status.has_altitude = true;
        gps_status.altitude_meters_x10 = gps_data.altitude_meters_x10;
    }

    if (!gps_data.has_fix) {
        if (consecutive_no_fix_reads < UINT16_MAX) {
            ++consecutive_no_fix_reads;
        }

        gps_status.consecutive_no_fix_reads = consecutive_no_fix_reads;

        if (this->has_last_coordinates && cycles_since_last_position < UINT16_MAX) {
            ++cycles_since_last_position;
        }

        this->apply_last_coordinates(gps_status, cycles_since_last_position);

        if (this->has_last_coordinates && !gps_status.has_recent_coordinates) {
            gps_status.state = GpsWorkflowState::coordinates_stale;
            gps_status.has_connection_problem = true;
        } else {
            gps_status.state = GpsWorkflowState::connected_waiting_fix;
        }

        gps_status.should_wait = true;
        this->last_status = gps_status;
#if _DEBUG_FOR_SERIAL
        this->log_gps_data(gps_data);
#endif
        this->log_connection_status(gps_status);
        return gps_status;
    }

    consecutive_no_fix_reads = 0U;
    gps_status.consecutive_no_fix_reads = 0U;

    if (gps_data.has_position) {
        this->has_last_coordinates = true;
        cycles_since_last_position = 0U;

        gps_status.latitude_x1000000 = gps_data.latitude_x1000000;
        gps_status.longitude_x1000000 = gps_data.longitude_x1000000;
        gps_status.has_recent_coordinates = true;
        gps_status.cycles_since_last_position = 0U;
        gps_status.state = GpsWorkflowState::coordinates_ready;
        gps_status.should_wait = false;
    } else {
        if (this->has_last_coordinates && cycles_since_last_position < UINT16_MAX) {
            ++cycles_since_last_position;
        }

        this->apply_last_coordinates(gps_status, cycles_since_last_position);

        if (this->has_last_coordinates && !gps_status.has_recent_coordinates) {
            gps_status.state = GpsWorkflowState::coordinates_stale;
            gps_status.has_connection_problem = true;
        } else {
            gps_status.state = GpsWorkflowState::connected_waiting_position;
        }

        gps_status.should_wait = true;
    }

    this->last_status = gps_status;
#if _DEBUG_FOR_SERIAL
    this->log_gps_data(gps_data);
#endif
    this->log_connection_status(gps_status);
    return gps_status;
}

const GpsConnectionStatus& GpsSystem_BL::get_last_status() const {
    return this->last_status;
}

void GpsSystem_BL::log_connection_status(const GpsConnectionStatus& gps_status) {
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
    Serial.print(F("gps state="));
    print_state_label(gps_status.state);
    Serial.print(F(" connected="));
    Serial.print(static_cast<int32_t>(gps_status.is_connected_to_stream ? 1 : 0));
    Serial.print(F(" fix="));
    Serial.print(static_cast<int32_t>(gps_status.has_fix ? 1 : 0));
    Serial.print(F(" pos="));
    Serial.print(static_cast<int32_t>(gps_status.has_position ? 1 : 0));
    Serial.print(F(" stale_cycles="));
    Serial.print(static_cast<int32_t>(gps_status.cycles_since_last_position));
    Serial.print(F(" fail="));
    Serial.print(static_cast<int32_t>(gps_status.consecutive_read_failures));
    Serial.print(F(" nofix="));
    Serial.println(static_cast<int32_t>(gps_status.consecutive_no_fix_reads));
#else
    (void)gps_status;
#endif
}

void GpsSystem_BL::log_gps_data(const NmeaGpsData& gps_data) {
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
    Serial.print(F("gps fix="));
    Serial.print(static_cast<int32_t>(gps_data.has_fix ? 1 : 0));
    Serial.print(F(" sat="));
    Serial.print(static_cast<int32_t>(gps_data.satellites));
    Serial.print(F(" q="));
    Serial.print(static_cast<int32_t>(gps_data.fix_quality));

    if (gps_data.has_position) {
        Serial.print(F(" lat="));
        print_coordinate_x1000000(gps_data.latitude_x1000000);
        Serial.print(F(" lon="));
        print_coordinate_x1000000(gps_data.longitude_x1000000);
    } else {
        Serial.print(F(" lat=na lon=na"));
    }

    if (gps_data.has_speed) {
        Serial.print(F(" kmh="));
        print_unsigned_fixed(static_cast<uint32_t>(gps_data.speed_kmh_x100), 2U);
    }

    if (gps_data.has_altitude) {
        Serial.print(F(" alt_m="));
        print_signed_fixed(gps_data.altitude_meters_x10, 1U);
    }

    if (gps_data.has_date && gps_data.has_time) {
        Serial.print(F(" utc="));
        Serial.print(static_cast<int32_t>(gps_data.year));
        Serial.print('-');
        print_two_digits(gps_data.month);
        Serial.print('-');
        print_two_digits(gps_data.day);
        Serial.print(' ');
        print_two_digits(gps_data.hour);
        Serial.print(':');
        print_two_digits(gps_data.minute);
        Serial.print(':');
        print_two_digits(gps_data.second);
    }

    Serial.println();
#else
    (void)gps_data;
#endif
}

bool GpsSystem_BL::read_and_log_coordinates() {
    return this->update_gps_status().has_recent_coordinates;
}
