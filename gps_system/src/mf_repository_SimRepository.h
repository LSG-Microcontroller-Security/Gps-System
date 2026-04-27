// Sim800Repository.h
#pragma once
#include <mf_repository_AvrMicroRepository.h>
#include <mf_utility_SerialUtils.h>
#include <stdint.h>
class SimRepository {
public:
    // Costruttore: injection del repository AVR, pin accensione modulo, baudrate seriale
    SimRepository(AvrMicroRepository& avrMicroRepository, uint8_t powerPin, unsigned long baudRate);

    // Effettua una chiamata al numero GSM (es: "+391234567890")
    bool call(const char* number);
    // (Opzionale) termina la chiamata attiva
    void hangUp();
    void setSmsTextMode();
    void setSmsPduMode();
    bool sendSms(const char* number, const char* message);
    void initSmsReception();
	bool readSms(uint8_t index, char* message);
    void deleteAllSms();
    /*bool readFirstSms(char* message);*/
    int getSmsCount();
    bool deleteSmsAt(uint8_t index);
    uint8_t findSmsWithContent(const char* pattern);
    bool isRegisteredToNetwork();
    int  getSignalStrength();
    bool isGprsAttached();
private:
    AvrMicroRepository* avrMicroRepository;
    uint8_t powerPin;
    unsigned long baudRate;
	bool isSmsReceiveInitialized = false;
};
