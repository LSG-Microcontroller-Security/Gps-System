// mf_repository_SimRepository.h
#pragma once

#include <stdint.h>
#include <mf_repository_ISerial.h>
#include <mf_commons_commonsLayer.h>

class SimRepository {
public:
    SimRepository(ISerial& serial, uint8_t power_pin, unsigned long baud_rate);

#if _ON_MOCKING_TESTS
    virtual ~SimRepository() = default;
    virtual bool call(const char* number);
    virtual void hangUp();
    virtual void setSmsTextMode();
    virtual void setSmsPduMode();
    virtual bool sendSms(const char* number, const char* message);
    virtual void initSmsReception();
    virtual bool setNetlightEnabled(bool enabled);
    virtual bool readSms(uint8_t index, char* message);
    virtual void deleteAllSms();
    virtual int getSmsCount();
    virtual bool deleteSmsAt(uint8_t index);
    virtual uint8_t findSmsWithContent(const char* pattern);
    virtual bool isRegisteredToNetwork();
    virtual int getSignalStrength();
    virtual bool isGprsAttached();
    virtual bool enterSleepMode();
    virtual bool exitSleepMode();
    virtual bool listen();
    virtual bool is_listening();
    virtual bool stop_listening();
    virtual void delay(unsigned long ms);
#else
    ~SimRepository() = default;
    void call(const char* number);
    void hangUp();
    void setSmsTextMode();
    void setSmsPduMode();
    void sendSms(const char* number, const char* message);
    void initSmsReception();
    bool setNetlightEnabled(bool enabled);
    bool readSms(uint8_t index, char* message);
    void deleteAllSms();
    int getSmsCount();
    bool deleteSmsAt(uint8_t index);
    uint8_t findSmsWithContent(const char* pattern);
    bool isRegisteredToNetwork();
    int getSignalStrength();
    bool isGprsAttached();
    bool enterSleepMode();
    bool exitSleepMode();
    bool listen();
    bool is_listening();
    bool stop_listening();
    void delay(unsigned long ms);
#endif

private:
    void clear_receive_buffer();
    bool wait_for_pattern(const char* pattern, unsigned long timeout_ms);
    uint8_t read_response(char* buffer, uint8_t buffer_size, unsigned long timeout_ms);

private:
    ISerial& serial_;
    uint8_t dtr_pin_;
    unsigned long baud_rate_;
    bool is_sms_receive_initialized_;
};
