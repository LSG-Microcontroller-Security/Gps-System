// mf_repository_NmeaGpsRepository.h
#pragma once

#include <stdint.h>
#include <mf_repository_ISerial.h>

class mf_repository_NmeaGpsRepository {
public:
    // Inizializza il repository con l'interfaccia seriale concreta.
    explicit mf_repository_NmeaGpsRepository(ISerial& serial);
    virtual ~mf_repository_NmeaGpsRepository() = default;
    // Avvia la seriale GPS con baud rate di default e pulisce il buffer.
    virtual void begin();
    // Aggiorna il timeout di lettura usato dalla seriale.
    virtual void set_timeout(unsigned long timeout_ms);
    // Legge una frase NMEA completa grezza dal canale seriale.
    virtual bool read_sentence(char* out_sentence, uint8_t max_length);
private:
    // Svuota eventuali byte residui nel buffer seriale.
    void clear_receive_buffer();
private:
    ISerial& serial_;
    unsigned long timeout_ms_;
};
