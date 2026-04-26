#include <stdio.h>
#include "eeprom.h"
#include "pico/stdlib.h"

const uint BUTTON_PINS[] = {9, 8, 7};
const uint LED_PINS[] = {20, 21, 22};

uint64_t start_time_us;

void print_led_state(uint8_t state);

int main() {
    stdio_init_all();
    start_time_us = time_us_64();
    eeprom_init();

    //button and led init
    for(int i=0; i<3; i++) {
        gpio_init(LED_PINS[i]);
        gpio_set_dir(LED_PINS[i], GPIO_OUT);

        gpio_init(BUTTON_PINS[i]);
        gpio_set_dir(BUTTON_PINS[i], GPIO_IN);
        gpio_pull_up(BUTTON_PINS[i]);
    }

    ledstate current_ls;
    read_led_state(&current_ls);

    uint8_t led_mask;
    if (led_state_is_valid(&current_ls)) { //check if state and inverted state match
        led_mask = current_ls.state;
    } else {
        led_mask = 0b010; //middle led on by default
    }

    for(int i=0; i<3; i++) {
        gpio_put(LED_PINS[i], (led_mask >> i) & 1); //turn leds on and off
    }
    print_led_state(led_mask);

    bool prev_button_state[3]; //check the state of leds
    for(int i=0; i<3; i++) {
        prev_button_state[i] = gpio_get(BUTTON_PINS[i]);
    }

    //works with this as well
    //bool prev_button_state[3] = {true, true, true};

    while (true) {
        bool state_changed = false;

        for (int i = 0; i < 3; i++) {
            bool current_button = gpio_get(BUTTON_PINS[i]);

            //falling edge detection
            if (current_button == false && prev_button_state[i] == true) {
                led_mask ^= (1 << i); //XOR toggle bit
                gpio_put(LED_PINS[i], (led_mask >> i) & 1); //update the specific led
                state_changed = true;
            }
            prev_button_state[i] = current_button; //memory update
        }

        if (state_changed) {
            write_led_state(led_mask); //save the state
            print_led_state(led_mask);
        }

        sleep_ms(20);
    }
}
