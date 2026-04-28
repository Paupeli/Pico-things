#ifndef EEPROM_EEPROM_H
#define EEPROM_EEPROM_H

#include "hardware/i2c.h"
#include <stdint.h>

#define I2C0_SDA_PIN 16
#define I2C0_SCL_PIN 17
#define BAUDRATE 100000
#define EEPROM_ADDRESS 0x50
#define LED_STATE_ADDRESS 0x7FFE

typedef struct ledstate {
    uint8_t state;
    uint8_t inverted_state;
} ledstate;

void eeprom_init();
void write_led_state(uint8_t value);
void read_led_state(ledstate *ls);
void print_led_state(uint8_t state);
bool led_state_is_valid(ledstate *ls);
void eeprom_write_bytes(uint16_t addr, const uint8_t *data, size_t len);
void eeprom_read_bytes(uint16_t addr, uint8_t *data, size_t len);
void eeprom_write_page(uint16_t addr, const uint8_t *data, size_t len);

#endif //EEPROM_EEPROM_H
