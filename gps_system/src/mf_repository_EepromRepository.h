// File: mf_repository_EepromRepository.h
#pragma once

#include <stdint.h>

class EepromRepository {
public:
    virtual ~EepromRepository() = default;

    virtual uint16_t get_capacity_bytes() const;
    virtual bool is_valid_address(uint16_t address) const;

    virtual bool read_uint8(uint16_t address, uint8_t& out_value) const;
    virtual bool write_uint8(uint16_t address, uint8_t value);

    virtual bool read_uint16(uint16_t address, uint16_t& out_value) const;
    virtual bool write_uint16(uint16_t address, uint16_t value);

    virtual bool read_uint32(uint16_t address, uint32_t& out_value) const;
    virtual bool write_uint32(uint16_t address, uint32_t value);

    virtual bool read_block(uint16_t start_address, uint8_t* out_buffer, uint16_t length) const;
    virtual bool write_block(uint16_t start_address, const uint8_t* data, uint16_t length);

protected:
    virtual uint8_t raw_read_byte(uint16_t address) const;
    virtual void raw_update_byte(uint16_t address, uint8_t value);

private:
    bool is_valid_range(uint16_t start_address, uint16_t length) const;
};
