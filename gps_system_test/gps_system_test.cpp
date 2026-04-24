#include "pch.h"
#include "src/extend.h"
#include "CppUnitTest.h"
#include "../gps_system/src/business/GpsSystem_BL.h"
#include <mf_commons_commonsLayer.h>
#include <mf_activity_NmeaGpsActivity.h>
#include <mf_repository_NmeaGpsRepository.h>
#include <initializer_list>
#include <memory>
#include <vector>
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace {
constexpr uint8_t read_attempts_per_update = 6U;
constexpr const char* gga_with_fix_and_position = "$GNGGA,141823.000,4530.63660,N,00914.61928,E,1,08,1.3,189.7,M,0.0,M,,*7D";
constexpr const char* rmc_with_fix_and_position = "$GNRMC,141823.000,A,4530.63660,N,00914.61928,E,0.00,0.00,230426,,,A*73";
constexpr const char* vtg_with_speed = "$GNVTG,0.00,T,,M,0.00,N,0.00,K,A*23";

bool copy_sentence_to_buffer(const char* sentence, char* out_sentence, uint8_t max_length) {
    if (sentence == nullptr || out_sentence == nullptr || max_length < 2U) {
        return false;
    }

    uint8_t write_index = 0U;
    while (sentence[write_index] != '\0' && write_index < static_cast<uint8_t>(max_length - 1U)) {
        out_sentence[write_index] = sentence[write_index];
        ++write_index;
    }

    out_sentence[write_index] = '\0';
    return true;
}
void append_cycle(std::vector<const char*>& read_script, std::initializer_list<const char*> sentences_for_cycle) {
    uint8_t slots_filled = 0U;

    for (const char* sentence : sentences_for_cycle) {
        if (slots_filled >= read_attempts_per_update) {
            break;
        }

        read_script.push_back(sentence);
        ++slots_filled;
    }

    while (slots_filled < read_attempts_per_update) {
        read_script.push_back(nullptr);
        ++slots_filled;
    }
}
void setup_repository_read_script(Mock<mf_repository_NmeaGpsRepository>& mocked_repository, const std::vector<const char*>& read_script) {
    std::shared_ptr<size_t> read_index = std::make_shared<size_t>(0U);

    When(Method(mocked_repository, read_sentence)).AlwaysDo([read_script, read_index](char* out_sentence, uint8_t max_length) {
        if (*read_index >= read_script.size()) {
            return false;
        }

        const char* sentence = read_script[*read_index];
        ++(*read_index);
        return copy_sentence_to_buffer(sentence, out_sentence, max_length);
    });
}
}
namespace GpsSystemTest {
    TEST_CLASS(GpsSystemTest) {
    public:
        TEST_METHOD(try_read_gps_data_loads_trace_into_gps_data_object) {
            Mock<mf_repository_NmeaGpsRepository> mocked_nmea_gps_repository;
            std::vector<const char*> read_script;
            append_cycle(read_script, { gga_with_fix_and_position, rmc_with_fix_and_position, vtg_with_speed });
            setup_repository_read_script(mocked_nmea_gps_repository, read_script);
            mf_repository_NmeaGpsRepository& nmea_gps_repository = mocked_nmea_gps_repository.get();
            mf_activity_NmeaGpsActivity nmea_gps_activity(nmea_gps_repository);
            GpsSystem_BL gps_system_bl(nmea_gps_activity);
            NmeaGpsData gps_data = {};
            const bool read_ok = gps_system_bl.try_read_gps_data(gps_data);
            Assert::IsTrue(read_ok);
            Assert::IsTrue(gps_data.has_fix);
            Assert::IsTrue(gps_data.has_position);
            Assert::IsTrue(gps_data.has_altitude);
            Assert::IsTrue(gps_data.has_speed);
            Assert::IsTrue(gps_data.has_date);
            Assert::IsTrue(gps_data.has_time);
            Assert::AreEqual((uint8_t)8U, gps_data.satellites);
            Assert::AreEqual((uint8_t)1U, gps_data.fix_quality);
            Assert::AreEqual((int32_t)45510610L, gps_data.latitude_x1000000);
            Assert::AreEqual((int32_t)9243655L, gps_data.longitude_x1000000);
            Assert::AreEqual((int32_t)1897L, gps_data.altitude_meters_x10);
            Assert::AreEqual((uint16_t)0U, gps_data.speed_kmh_x100);
        }
    };
}
