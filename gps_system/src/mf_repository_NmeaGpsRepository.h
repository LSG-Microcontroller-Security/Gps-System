// mf_repository_NmeaGpsRepository.h
#pragma once

#include <stdint.h>
#include <mf_repository_ISerial.h>
#include <mf_commons_commonsLayer.h>

class NmeaGpsRepository {
public:
    // Inizializza il repository con l'interfaccia seriale concreta.
    explicit NmeaGpsRepository(ISerial& serial);

#if _ON_MOCKING_TESTS
    virtual ~NmeaGpsRepository() = default;

    // Avvia la seriale GPS con baud rate di default e pulisce il buffer.
    virtual void begin();

    // Attiva l'ascolto della seriale quando l'adapter sottostante lo supporta.
    virtual bool listen();

    // Indica se la seriale sottostante e' attualmente in ascolto.
    virtual bool is_listening();

    // Disattiva l'ascolto della seriale quando l'adapter sottostante lo supporta.
    virtual bool stop_listening();

    // Aggiorna il timeout di lettura usato dalla seriale.
    virtual void set_timeout(unsigned long timeout_ms);

    // Legge una frase NMEA completa grezza dal canale seriale.
    virtual bool read_sentence(char* out_sentence, uint8_t max_length);
#else
    ~NmeaGpsRepository() = default;

    // Avvia la seriale GPS con baud rate di default e pulisce il buffer.
    void begin();

    // Attiva l'ascolto della seriale quando l'adapter sottostante lo supporta.
    bool listen();

    // Indica se la seriale sottostante e' attualmente in ascolto.
    bool is_listening();

    // Disattiva l'ascolto della seriale quando l'adapter sottostante lo supporta.
    bool stop_listening();

    // Aggiorna il timeout di lettura usato dalla seriale.
    void set_timeout(unsigned long timeout_ms);

    // Legge una frase NMEA completa grezza dal canale seriale.
    bool read_sentence(char* out_sentence, uint8_t max_length);
#endif

private:
    // Svuota eventuali byte residui nel buffer seriale.
    void clear_receive_buffer();

private:
    ISerial& serial_;
    unsigned long timeout_ms_;
};