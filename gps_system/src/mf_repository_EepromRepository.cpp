// File: mf_repository_EepromRepository.cpp

#include "mf_repository_EepromRepository.h"
#include <avr/eeprom.h>

uint16_t EepromRepository::get_capacity_bytes() const {
    return (uint16_t)E2END + 1U;
}

bool EepromRepository::is_valid_address(uint16_t address) const {
    return this->is_valid_range(address, 1U);
}

bool EepromRepository::read_uint8(uint16_t address, uint8_t& out_value) const {
    if (!this->is_valid_range(address, 1U)) {
        return false;
    }

    out_value = this->raw_read_byte(address);
    return true;
}

bool EepromRepository::write_uint8(uint16_t address, uint8_t value) {
    if (!this->is_valid_range(address, 1U)) {
        return false;
    }

    this->raw_update_byte(address, value);
    return true;
}

bool EepromRepository::read_uint16(uint16_t address, uint16_t& out_value) const {
    if (!this->is_valid_range(address, 2U)) {
        return false;
    }

    uint8_t low_byte = this->raw_read_byte(address);
    uint8_t high_byte = this->raw_read_byte((uint16_t)(address + 1U));
    out_value = (uint16_t)low_byte | ((uint16_t)high_byte << 8U);
    return true;
}

bool EepromRepository::write_uint16(uint16_t address, uint16_t value) {
    if (!this->is_valid_range(address, 2U)) {
        return false;
    }

    this->raw_update_byte(address, (uint8_t)(value & 0xFFU));
    this->raw_update_byte((uint16_t)(address + 1U), (uint8_t)((value >> 8U) & 0xFFU));
    return true;
}

bool EepromRepository::read_uint32(uint16_t address, uint32_t& out_value) const {
    if (!this->is_valid_range(address, 4U)) {
        return false;
    }

    uint32_t byte0 = this->raw_read_byte(address);
    uint32_t byte1 = this->raw_read_byte((uint16_t)(address + 1U));
    uint32_t byte2 = this->raw_read_byte((uint16_t)(address + 2U));
    uint32_t byte3 = this->raw_read_byte((uint16_t)(address + 3U));

    out_value = byte0 | (byte1 << 8U) | (byte2 << 16U) | (byte3 << 24U);
    return true;
}

bool EepromRepository::write_uint32(uint16_t address, uint32_t value) {
    if (!this->is_valid_range(address, 4U)) {
        return false;
    }

    this->raw_update_byte(address, (uint8_t)(value & 0xFFUL));
    this->raw_update_byte((uint16_t)(address + 1U), (uint8_t)((value >> 8U) & 0xFFUL));
    this->raw_update_byte((uint16_t)(address + 2U), (uint8_t)((value >> 16U) & 0xFFUL));
    this->raw_update_byte((uint16_t)(address + 3U), (uint8_t)((value >> 24U) & 0xFFUL));
    return true;
}

bool EepromRepository::read_block(uint16_t start_address, uint8_t* out_buffer, uint16_t length) const {
    if (out_buffer == 0 || !this->is_valid_range(start_address, length)) {
        return false;
    }

    for (uint16_t i = 0U; i < length; i++) {
        out_buffer[i] = this->raw_read_byte((uint16_t)(start_address + i));
    }

    return true;
}

bool EepromRepository::write_block(uint16_t start_address, const uint8_t* data, uint16_t length) {
    if (data == 0 || !this->is_valid_range(start_address, length)) {
        return false;
    }

    for (uint16_t i = 0U; i < length; i++) {
        this->raw_update_byte((uint16_t)(start_address + i), data[i]);
    }

    return true;
}

uint8_t EepromRepository::raw_read_byte(uint16_t address) const {
    const uint8_t* eeprom_address = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(address));
    return eeprom_read_byte(eeprom_address);
}

void EepromRepository::raw_update_byte(uint16_t address, uint8_t value) {
    uint8_t* eeprom_address = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(address));
    eeprom_update_byte(eeprom_address, value);
}

bool EepromRepository::is_valid_range(uint16_t start_address, uint16_t length) const {
    if (length == 0U) {
        return true;
    }

    uint32_t max_capacity = (uint32_t)this->get_capacity_bytes();
    uint32_t end_address_exclusive = (uint32_t)start_address + (uint32_t)length;
    return end_address_exclusive <= max_capacity;
}
