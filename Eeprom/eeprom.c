#include <stdio.h>
#include "eeprom.h"
#include "pico/stdlib.h"

#define EEPROM_ADDRESS 0x50
#define LED_STATE_ADDRESS 0x7FFE

void eeprom_init() {
    i2c_init(i2c0, BAUDRATE);
    gpio_set_function(I2C0_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL_PIN, GPIO_FUNC_I2C);
}

void write_led_state(uint8_t value) {
    ledstate ls;
    ls.state = value; //the actual led state
    ls.inverted_state = ~value; //inverted state

    uint8_t buffer[4];
    buffer[0] = LED_STATE_ADDRESS >> 8; //high byte
    buffer[1] = LED_STATE_ADDRESS & 0xFF; //low byte

    buffer[2] = ls.state;
    buffer[3] = ls.inverted_state;

    i2c_write_blocking(i2c0, EEPROM_ADDRESS, buffer, 4, false);
    sleep_ms(10);
}

//sanity check
bool led_state_is_valid(ledstate *ls) {
    return ls->state == (uint8_t)~ls->inverted_state;
}

//take high& low bytes, send them to eeprom and read led state
void read_led_state(ledstate *ls) {
    uint8_t address_buffer[2] = {LED_STATE_ADDRESS >> 8, LED_STATE_ADDRESS & 0xFF};
    i2c_write_blocking(i2c0, EEPROM_ADDRESS, address_buffer, 2, true);
    i2c_read_blocking(i2c0, EEPROM_ADDRESS, (uint8_t*)ls, 2, false);
}

void print_led_state(uint8_t state) {
    uint64_t now = time_us_64();
    float time_since_power_up = now / 1000000.0;

    //print timestamp
    printf("[%0.2fs] ", time_since_power_up);

    //print hex state
    printf("State: 0x%02X (", state);

    //print led status
    for (int i = 0; i < 3; i++) {
        bool is_on = (state >> i) & 1; //bit shifting to check if bit is 1 or 0
        printf("LED%d: %s", i, is_on ? "ON" : "OFF");
    }
    printf(")\n");
}
