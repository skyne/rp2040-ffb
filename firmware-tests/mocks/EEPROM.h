#pragma once

#include <stdint.h>
#include <string.h>

class MockEEPROM {
public:
    MockEEPROM() {
        memset(data, 0xFF, sizeof(data));
    }
    
    void begin(size_t size) {
        if (size > sizeof(data)) {
            size = sizeof(data);
        }
        this->size = size;
    }
    
    uint8_t read(int address) {
        if (address >= 0 && (size_t)address < size) {
            return data[address];
        }
        return 0xFF;
    }
    
    void write(int address, uint8_t value) {
        if (address >= 0 && (size_t)address < size) {
            data[address] = value;
        }
    }
    
    template<typename T>
    T &get(int address, T &t) {
        if (address >= 0 && (size_t)(address + sizeof(T)) <= size) {
            memcpy(&t, &data[address], sizeof(T));
        }
        return t;
    }
    
    template<typename T>
    const T &put(int address, const T &t) {
        if (address >= 0 && (size_t)(address + sizeof(T)) <= size) {
            memcpy(&data[address], &t, sizeof(T));
        }
        return t;
    }
    
    bool commit() {
        return true;
    }
    
    void end() {}
    
    // Test helpers
    void clear() {
        memset(data, 0xFF, sizeof(data));
    }
    
    const uint8_t* getData() const {
        return data;
    }
    
    void setData(const uint8_t* newData, size_t len) {
        if (len > sizeof(data)) {
            len = sizeof(data);
        }
        memcpy(data, newData, len);
    }
    
private:
    uint8_t data[4096];
    size_t size = 0;
};

extern MockEEPROM EEPROM;
