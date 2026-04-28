// mf_repository_SimRepository.h
#pragma once

#include <stdint.h>
#include <mf_repository_ISerial.h>

class SimRepository {
public:
    SimRepository(ISerial& serial, uint8_t power_pin, unsigned long baud_rate);
    virtual ~SimRepository() = default;

    virtual bool call(const char* number);
    virtual void hangUp();
    virtual void setSmsTextMode();
    virtual void setSmsPduMode();
    virtual bool sendSms(const char* number, const char* message);
    virtual void initSmsReception();
    virtual bool readSms(uint8_t index, char* message);
    virtual void deleteAllSms();
    virtual int getSmsCount();
    virtual bool deleteSmsAt(uint8_t index);
    virtual uint8_t findSmsWithContent(const char* pattern);
    virtual bool isRegisteredToNetwork();
    virtual int getSignalStrength();
    virtual bool isGprsAttached();
    // Attiva l'ascolto della seriale quando l'adapter sottostante lo supporta.
    virtual bool listen();
    // Indica se la seriale sottostante e' attualmente in ascolto.
    virtual bool is_listening();
    // Disattiva l'ascolto della seriale quando l'adapter sottostante lo supporta.
    virtual bool stop_listening();
    virtual void delay(unsigned long ms);

private:
    void clear_receive_buffer();
    bool wait_for_pattern(const char* pattern, unsigned long timeout_ms);
    uint8_t read_response(char* buffer, uint8_t buffer_size, unsigned long timeout_ms);

private:
    ISerial& serial_;
    uint8_t power_pin_;
    unsigned long baud_rate_;
    bool is_sms_receive_initialized_;
};
