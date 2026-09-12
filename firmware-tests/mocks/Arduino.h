#pragma once

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Arduino mock types and constants
#define HIGH 0x1
#define LOW  0x0

#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2

#define LED_BUILTIN 25

// Mock Serial class
class MockSerial {
public:
    void begin(unsigned long baud);
    void end();
    int available();
    int read();
    size_t write(uint8_t);
    size_t write(const uint8_t *buffer, size_t size);
    size_t print(const char *);
    size_t print(int, int base = 10);
    size_t print(unsigned int, int base = 10);
    size_t print(long, int base = 10);
    size_t print(unsigned long, int base = 10);
    size_t print(double, int digits = 2);
    size_t println(const char *);
    size_t println(int, int base = 10);
    size_t println(unsigned int, int base = 10);
    size_t println(long, int base = 10);
    size_t println(unsigned long, int base = 10);
    size_t println(double, int digits = 2);
    size_t println();
    void flush();
    
    // Test helpers
    const char* getLastOutput() const;
    void clearOutput();
    
private:
    char outputBuffer[4096];
    size_t outputLen = 0;
};

extern MockSerial Serial;

// Mock functions
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int val);
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);

// Test control functions
namespace ArduinoMock {
    void reset();
    void setMillis(unsigned long ms);
    void setMicros(unsigned long us);
    void setAnalogRead(uint8_t pin, int value);
    void setDigitalRead(uint8_t pin, int value);
    int getDigitalWrite(uint8_t pin);
    int getAnalogWrite(uint8_t pin);
    int getPinMode(uint8_t pin);
}
