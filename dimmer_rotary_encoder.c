#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"

#define ROT_A_PIN 10
#define ROT_B_PIN 11
#define ROT_SW_PIN 12

#define LEDS (sizeof(pwm_pins) / sizeof(pwm_pins[0])) //leds
#define PWM_WRAP 999 //1kHz frequency 1000-1
#define PWM_DEFAULT_LEVEL 500 //50% brightness
#define PWM_CLKDIV 125 //1MHz frequency divider 125MHz/125 = 1MHz
#define BRIGHTNESS_STEP 20 //the change of brightness

const int pwm_pins[] = {20, 21, 22};

//states
volatile int encoder_delta = 0;
bool leds_enabled = false;
uint16_t current_brightness = PWM_DEFAULT_LEVEL;

void encoder_callbck(uint gpio, uint32_t events) //ISR for the rotary encoder
{
    if (gpio == ROT_A_PIN)
    {
        if (gpio_get(ROT_B_PIN))
        {
            encoder_delta--; //rotate counter-clockwise
        }
        else
        {
            encoder_delta++; //rotate clockwise
        }
    }
}

void init_pwm(uint pin) //pwm setup
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_set_clkdiv(slice_num, PWM_CLKDIV);
    pwm_set_wrap(slice_num, PWM_WRAP);
    pwm_set_enabled(slice_num, true);
}

void update_leds(uint16_t level) //updates brightness of the leds
{
    for (int i = 0; i < LEDS; i++)
    {
        pwm_set_gpio_level(pwm_pins[i], level);
    }
}

int main()
{
    stdio_init_all();

    //initialize pins
    gpio_init(ROT_A_PIN);
    gpio_set_dir(ROT_A_PIN, GPIO_IN);
    gpio_pull_up(ROT_A_PIN);
    gpio_init(ROT_B_PIN);
    gpio_set_dir(ROT_B_PIN, GPIO_IN);
    gpio_pull_up(ROT_B_PIN);
    gpio_init(ROT_SW_PIN);
    gpio_set_dir(ROT_SW_PIN, GPIO_IN);
    gpio_pull_up(ROT_SW_PIN);

    //interrupt for pin A on rising edge
    gpio_set_irq_enabled_with_callback(ROT_A_PIN, GPIO_IRQ_EDGE_RISE, true, &encoder_callbck);

    //initialize PWM
    for (int i = 0; i < LEDS; i++) init_pwm(pwm_pins[i]);

    bool last_sw_state = true;

    while (true)
    {
        bool current_sw_state = gpio_get(ROT_SW_PIN);

        if (last_sw_state == true && current_sw_state == false) //button toggle (falling edge detection)
        {
            if (leds_enabled && current_brightness == 0)
            {
                current_brightness = PWM_DEFAULT_LEVEL;
            }
            else
            {
                leds_enabled = !leds_enabled;
            }
            while (!gpio_get(ROT_SW_PIN)) //blocking wait for release to prevent multiple toggles
            {
                sleep_ms(10);
            }
        }
        last_sw_state = current_sw_state;

        int32_t moved = encoder_delta; //encoder movement handling
        encoder_delta = 0;

        if (leds_enabled && moved != 0) //change brightness if leds are on
        {
            int new_val = (int)current_brightness + (moved * BRIGHTNESS_STEP);
            if (new_val > PWM_WRAP) new_val = PWM_WRAP;
            if (new_val < 0) new_val = 0;

            current_brightness = (uint16_t)new_val;
        }

        if (leds_enabled)
        {
            update_leds(current_brightness);
        }
        else
        {
            update_leds(0); //leds off
        }
        printf("Brightness: %d /999\n", current_brightness); //for testing

        sleep_ms(5); //5ms sleep
    }
}
