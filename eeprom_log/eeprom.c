#include <stdio.h>
#include <string.h>
#include "eeprom.h"
#include "log.h"
#include "pico/stdlib.h"

void eeprom_init()
{
    i2c_init(i2c0, BAUDRATE);
    gpio_set_function(I2C0_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL_PIN, GPIO_FUNC_I2C);
}

void eeprom_write_bytes(uint16_t addr, const uint8_t *data, size_t len)
{
    uint8_t buffer[len + 2]; //buffer for address and data
    buffer[0] = addr >> 8; //high byte
    buffer[1] = addr & 0xFF; //low byte

    for (size_t i = 0; i < len; i++) { //copy data into the buffer
        buffer[i + 2] = data[i];
    }

    i2c_write_blocking(i2c0, EEPROM_ADDRESS, buffer, len + 2, false);

    sleep_ms(20);
}

void eeprom_read_bytes(uint16_t addr, uint8_t *data, size_t len)
{
    uint8_t addr_buffer[2] = {addr >> 8, addr & 0xFF};
    i2c_write_blocking(i2c0, EEPROM_ADDRESS, addr_buffer, 2, true);
    i2c_read_blocking(i2c0, EEPROM_ADDRESS, data, len, false);
}

void eeprom_write_page(uint16_t addr, const uint8_t *data, size_t len)
{
    uint8_t buffer[len + 2]; //address and data length

    buffer[0] = (uint8_t)(addr >> 8);   //msb
    buffer[1] = (uint8_t)(addr & 0xFF); //lsb

    memcpy(&buffer[2], data, len);

    i2c_write_blocking(i2c0, EEPROM_ADDRESS, buffer, len + 2, false);

    sleep_ms(20);
}

void write_led_state(uint8_t value)
{
    uint8_t data[2];
    data[0] = value;
    data[1] = ~value;
    eeprom_write_bytes(LED_STATE_ADDRESS, data, 2);
}

void read_led_state(ledstate *ls)
{
    eeprom_read_bytes(LED_STATE_ADDRESS, (uint8_t*)ls, 2);
}

bool led_state_is_valid(ledstate *ls)
{
    return ls->state == (uint8_t)~ls->inverted_state;
}

void print_led_state(uint8_t state)
{
    uint64_t now = time_us_64();
    float time_since_power_up = now / 1000000.0;

    //print timestamp
    printf("%0.2fs ", time_since_power_up);

    //print hex state
    printf("State: 0x%02X ", state);

    //print led status
    for (int i = 0; i < 3; i++) {
        bool is_on = (state >> i) & 1; //bit shifting to check if bit is 1 or 0
        printf("\nLED %d: %s", i, is_on ? "ON" : "OFF");
    }
    printf("\n");
}
