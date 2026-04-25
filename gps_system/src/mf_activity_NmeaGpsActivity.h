// mf_activity_NmeaGpsActivity.h
#pragma once

#include <stdint.h>
#include "mf_repository_NmeaGpsRepository.h"
struct NmeaGpsData {
    bool has_fix;
    bool has_position;
    bool has_time;
    bool has_date;
    bool has_speed;
    bool has_altitude;
    int32_t latitude_x1000000;
    int32_t longitude_x1000000;
    int32_t altitude_meters_x10;
    uint16_t speed_kmh_x100;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t satellites;
    uint8_t fix_quality;
};
enum class GpsWorkflowState : uint8_t {
    waiting_for_data = 0U,
    connected_waiting_fix = 1U,
    connected_waiting_position = 2U,
    coordinates_ready = 3U,
    coordinates_stale = 4U,
    connection_problem = 5U
};
struct GpsConnectionStatus {
    int32_t latitude_x1000000;
    int32_t longitude_x1000000;
    int32_t altitude_meters_x10;
    uint16_t consecutive_read_failures;
    uint16_t consecutive_no_fix_reads;
    uint16_t cycles_since_last_position;
    GpsWorkflowState state;
    uint8_t satellites;
    uint8_t fix_quality;
    bool is_connected_to_stream;
    bool has_fix;
    bool has_position;
    bool has_altitude;
    bool has_recent_coordinates;
    bool has_connection_problem;
    bool should_wait;
};

class IGpsActivity {
public:
    virtual ~IGpsActivity() = default;
    virtual bool try_read_data(NmeaGpsData& out_data, uint8_t max_sentences_to_read = 6) = 0;
    virtual const GpsConnectionStatus& update_gps_status() = 0;
    virtual const GpsConnectionStatus& get_last_status() const = 0;
};

class mf_activity_NmeaGpsActivity : public IGpsActivity {
public:
    // Collega l'activity al repository che legge le frasi NMEA.
    explicit mf_activity_NmeaGpsActivity(mf_repository_NmeaGpsRepository& nmea_gps_repository);
    virtual ~mf_activity_NmeaGpsActivity() = default;

    // Legge piu frasi NMEA, le interpreta e costruisce un NmeaGpsData coerente.
    bool try_read_data(NmeaGpsData& out_data, uint8_t max_sentences_to_read = 6) override;
    // Calcola e aggiorna lo stato complessivo del workflow GPS.
    const GpsConnectionStatus& update_gps_status() override;
    // Espone l'ultimo stato GPS senza ricalcolarlo.
    const GpsConnectionStatus& get_last_status() const override;

private:
    static const uint8_t nmea_sentence_buffer_length = 83;

    // Smista la frase NMEA verso il parser specifico in base al tipo (GGA/RMC/VTG).
    bool parse_sentence(const char* sentence, NmeaGpsData& data) const;
    // Estrae fix, satelliti, altitudine e coordinate da una frase GGA.
    bool parse_gga_sentence(const char* sentence, NmeaGpsData& data) const;
    // Estrae fix, coordinate, velocita in nodi e data/ora da una frase RMC.
    bool parse_rmc_sentence(const char* sentence, NmeaGpsData& data) const;
    // Estrae la velocita in km/h da una frase VTG.
    bool parse_vtg_sentence(const char* sentence, NmeaGpsData& data) const;
    // Verifica checksum e formato base della frase NMEA ricevuta.
    bool is_sentence_checksum_valid(const char* sentence) const;
    // Converte un carattere esadecimale nel relativo nibble.
    int8_t hex_to_nibble(char value) const;
    // Verifica che la frase inizi con il tipo atteso.
    bool is_sentence_type(const char* sentence, char first, char second, char third) const;
    // Recupera un campo NMEA per indice e ne restituisce puntatore/lunghezza.
    bool get_field(const char* sentence, uint8_t field_index, const char*& field, uint8_t& length) const;
    // Converte un campo numerico intero in uint16_t.
    bool parse_uint_field(const char* field, uint8_t length, uint16_t& out_value) const;
    // Converte un campo decimale in intero scalato con il numero di cifre richiesto.
    bool parse_decimal_scaled(const char* field, uint8_t length, uint8_t scale_digits, int32_t& out_value) const;
    // Converte il campo ora hhmmss nei campi ora/minuto/secondo della struct.
    bool parse_time_field(const char* field, uint8_t length, NmeaGpsData& data) const;
    // Converte il campo data ddmmyy in giorno/mese/anno.
    bool parse_date_field(const char* field, uint8_t length, NmeaGpsData& data) const;
    // Converte coordinate NMEA (gradi+minuti) nel formato gradi x 1.000.000.
    bool parse_coordinate_field(const char* field, uint8_t length, char hemisphere, uint8_t degree_digits, int32_t& out_value) const;
    // Legge due cifre in posizione nota e le converte in un byte.
    bool parse_two_digits(const char* field, uint8_t length, uint8_t index, uint8_t& out_value) const;
    // Azzera tutti i campi di output prima di iniziare un nuovo parsing.
    void clear_data(NmeaGpsData& data) const;
    // Applica ultime coordinate valide note e gestisce coordinate recenti/stale.
    void apply_last_coordinates(GpsConnectionStatus& gps_status, uint16_t cycles_since_last_position) const;
    // Inizializza uno stato GPS con valori di default coerenti.
    void set_status_defaults(GpsConnectionStatus& gps_status) const;

private:
    mf_repository_NmeaGpsRepository& nmea_gps_repository_;
    bool has_last_coordinates_;
    GpsConnectionStatus last_status_;
};
