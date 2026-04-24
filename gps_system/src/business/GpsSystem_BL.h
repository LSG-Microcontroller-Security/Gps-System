#pragma once

#include <stdint.h>
#include <mf_commons_commonsLayer.h>
#include <mf_activity_NmeaGpsActivity.h>

#if _ON_MOCKING_TESTS && _DEBUG_FOR_SERIAL
#error "_DEBUG_FOR_SERIAL must be 0 when _ON_MOCKING_TESTS is 1"
#endif

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

class GpsSystem_BL {
public:
    // Inizializza la business logic GPS e azzera lo stato interno.
    explicit GpsSystem_BL(mf_activity_NmeaGpsActivity& nmea_gps_activity);
    ~GpsSystem_BL() = default;

    // Chiede all'activity una nuova lettura grezza/parziale dal flusso NMEA.
    bool try_read_gps_data(NmeaGpsData& gps_data);
    // Restituisce le coordinate recenti se disponibili, altrimenti false.
    bool try_get_coordinates(int32_t& latitude_x1000000, int32_t& longitude_x1000000);
    // Aggiorna lo stato GPS completo (fix, posizione, errori, stale, attesa).
    GpsConnectionStatus update_gps_status();
    // Fornisce l'ultimo stato calcolato senza ricalcolarlo.
    const GpsConnectionStatus& get_last_status() const;
    // Stampa su seriale i dati GPS letti, solo quando il debug e abilitato.
    void log_gps_data(const NmeaGpsData& gps_data);
    // Esegue update stato e ritorna se ci sono coordinate recenti da usare.
    bool read_and_log_coordinates();

private:
    // Applica le ultime coordinate valide note e gestisce il flag "recenti/stale".
    void apply_last_coordinates(GpsConnectionStatus& gps_status, uint16_t cycles_since_last_position) const;
    // Imposta valori di default coerenti per uno stato GPS appena creato.
    void set_status_defaults(GpsConnectionStatus& gps_status) const;
    // Stampa il riepilogo dello stato di connessione su seriale in debug.
    void log_connection_status(const GpsConnectionStatus& gps_status);

    mf_activity_NmeaGpsActivity& nmea_gps_activity;
    bool has_last_coordinates;
    GpsConnectionStatus last_status;
};
