#pragma once

#include <stdint.h>
#include <mf_commons_commonsLayer.h>
#include "mf_activity_NmeaGpsActivity.h"

#if _ON_MOCKING_TESTS && _DEBUG_FOR_SERIAL
#error "_DEBUG_FOR_SERIAL must be 0 when _ON_MOCKING_TESTS is 1"
#endif

class GpsSystem_BL {
public:
    // Inizializza la business logic GPS e azzera lo stato interno.
    explicit GpsSystem_BL(IGpsActivity& gps_activity);
    ~GpsSystem_BL() = default;

    // Chiede all'activity una nuova lettura grezza/parziale dal flusso NMEA.
    bool try_read_gps_data(NmeaGpsData& gps_data);
    // Restituisce le coordinate recenti se disponibili, altrimenti false.
    bool try_get_coordinates(int32_t& latitude_x1000000, int32_t& longitude_x1000000);
    // Aggiorna lo stato GPS completo leggendo lo stato dall'activity.
    const GpsConnectionStatus& update_gps_status();
    // Fornisce l'ultimo stato calcolato senza ricalcolarlo.
    const GpsConnectionStatus& get_last_status() const;
    // Stampa su seriale i dati GPS letti, solo quando il debug e abilitato.
    // Esegue update stato e ritorna se ci sono coordinate recenti da usare.
    bool read_and_log_coordinates();

private:
    IGpsActivity& gps_activity_;
};
