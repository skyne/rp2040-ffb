#include "Arduino.h"
#include <string.h>

MockSerial Serial;

namespace {
    unsigned long g_millis = 0;
    unsigned long g_micros = 0;
    int g_pinModes[64] = {0};
    int g_digitalWrites[64] = {0};
    int g_analogWrites[64] = {0};
    int g_analogReads[64] = {0};
    int g_digitalReads[64] = {0};
}

// MockSerial implementation
void MockSerial::begin(unsigned long baud) {
    (void)baud;
    outputLen = 0;
    memset(outputBuffer, 0, sizeof(outputBuffer));
}

void MockSerial::end() {}

int MockSerial::available() {
    return 0;
}

int MockSerial::read() {
    return -1;
}

size_t MockSerial::write(uint8_t c) {
    if (outputLen < sizeof(outputBuffer) - 1) {
        outputBuffer[outputLen++] = (char)c;
        outputBuffer[outputLen] = '\0';
    }
    return 1;
}

size_t MockSerial::write(const uint8_t *buffer, size_t size) {
    size_t written = 0;
    for (size_t i = 0; i < size && outputLen < sizeof(outputBuffer) - 1; i++) {
        outputBuffer[outputLen++] = (char)buffer[i];
        written++;
    }
    outputBuffer[outputLen] = '\0';
    return written;
}

size_t MockSerial::print(const char *str) {
    size_t len = strlen(str);
    return write((const uint8_t*)str, len);
}

size_t MockSerial::print(int val, int base) {
    char buf[32];
    if (base == 10) {
        snprintf(buf, sizeof(buf), "%d", val);
    } else if (base == 16) {
        snprintf(buf, sizeof(buf), "%x", val);
    } else {
        snprintf(buf, sizeof(buf), "%d", val);
    }
    return print(buf);
}

size_t MockSerial::print(unsigned int val, int base) {
    char buf[32];
    if (base == 10) {
        snprintf(buf, sizeof(buf), "%u", val);
    } else if (base == 16) {
        snprintf(buf, sizeof(buf), "%x", val);
    } else {
        snprintf(buf, sizeof(buf), "%u", val);
    }
    return print(buf);
}

size_t MockSerial::print(long val, int base) {
    char buf[32];
    if (base == 10) {
        snprintf(buf, sizeof(buf), "%ld", val);
    } else if (base == 16) {
        snprintf(buf, sizeof(buf), "%lx", val);
    } else {
        snprintf(buf, sizeof(buf), "%ld", val);
    }
    return print(buf);
}

size_t MockSerial::print(unsigned long val, int base) {
    char buf[32];
    if (base == 10) {
        snprintf(buf, sizeof(buf), "%lu", val);
    } else if (base == 16) {
        snprintf(buf, sizeof(buf), "%lx", val);
    } else {
        snprintf(buf, sizeof(buf), "%lu", val);
    }
    return print(buf);
}

size_t MockSerial::print(double val, int digits) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", digits, val);
    return print(buf);
}

size_t MockSerial::println(const char *str) {
    size_t n = print(str);
    return n + print("\r\n");
}

size_t MockSerial::println(int val, int base) {
    size_t n = print(val, base);
    return n + print("\r\n");
}

size_t MockSerial::println(unsigned int val, int base) {
    size_t n = print(val, base);
    return n + print("\r\n");
}

size_t MockSerial::println(long val, int base) {
    size_t n = print(val, base);
    return n + print("\r\n");
}

size_t MockSerial::println(unsigned long val, int base) {
    size_t n = print(val, base);
    return n + print("\r\n");
}

size_t MockSerial::println(double val, int digits) {
    size_t n = print(val, digits);
    return n + print("\r\n");
}

size_t MockSerial::println() {
    return print("\r\n");
}

void MockSerial::flush() {}

const char* MockSerial::getLastOutput() const {
    return outputBuffer;
}

void MockSerial::clearOutput() {
    outputLen = 0;
    memset(outputBuffer, 0, sizeof(outputBuffer));
}

// Arduino functions
void pinMode(uint8_t pin, uint8_t mode) {
    if (pin < 64) {
        g_pinModes[pin] = mode;
    }
}

void digitalWrite(uint8_t pin, uint8_t val) {
    if (pin < 64) {
        g_digitalWrites[pin] = val;
    }
}

int digitalRead(uint8_t pin) {
    if (pin < 64) {
        return g_digitalReads[pin];
    }
    return LOW;
}

int analogRead(uint8_t pin) {
    if (pin < 64) {
        return g_analogReads[pin];
    }
    return 0;
}

void analogWrite(uint8_t pin, int val) {
    if (pin < 64) {
        g_analogWrites[pin] = val;
    }
}

unsigned long millis() {
    return g_millis;
}

unsigned long micros() {
    return g_micros;
}

void delay(unsigned long ms) {
    g_millis += ms;
    g_micros += ms * 1000;
}

void delayMicroseconds(unsigned int us) {
    g_micros += us;
    g_millis += us / 1000;
}

// Test control
namespace ArduinoMock {
    void reset() {
        g_millis = 0;
        g_micros = 0;
        memset(g_pinModes, 0, sizeof(g_pinModes));
        memset(g_digitalWrites, 0, sizeof(g_digitalWrites));
        memset(g_analogWrites, 0, sizeof(g_analogWrites));
        memset(g_analogReads, 0, sizeof(g_analogReads));
        memset(g_digitalReads, 0, sizeof(g_digitalReads));
        Serial.clearOutput();
    }
    
    void setMillis(unsigned long ms) {
        g_millis = ms;
    }
    
    void setMicros(unsigned long us) {
        g_micros = us;
    }
    
    void setAnalogRead(uint8_t pin, int value) {
        if (pin < 64) {
            g_analogReads[pin] = value;
        }
    }
    
    void setDigitalRead(uint8_t pin, int value) {
        if (pin < 64) {
            g_digitalReads[pin] = value;
        }
    }
    
    int getDigitalWrite(uint8_t pin) {
        if (pin < 64) {
            return g_digitalWrites[pin];
        }
        return -1;
    }
    
    int getAnalogWrite(uint8_t pin) {
        if (pin < 64) {
            return g_analogWrites[pin];
        }
        return -1;
    }
    
    int getPinMode(uint8_t pin) {
        if (pin < 64) {
            return g_pinModes[pin];
        }
        return -1;
    }
}
