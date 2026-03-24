#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define SW0_PIN 9
#define SW1_PIN 8
#define SW2_PIN 7

#define PWM_WRAP 999 //1kHz frequency 1000-1
#define STEP 2 //speed of dimming
#define PWM_DEFAULT_LEVEL 500 //50% brightness
#define PWM_CLKDIV 125 //1MHz frequency divider 125MHz/125 = 1MHz

const int pwm_pins[] = {20, 21, 22};

//states
bool leds_on = false;
uint16_t brightness = PWM_DEFAULT_LEVEL;
bool sw1_last_state = false;

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
    for (int i = 0; i < 3; i++)
    {
        pwm_set_gpio_level(pwm_pins[i], level);
    }
}

int main()
{
    stdio_init_all();

    //initialize pins
    gpio_init(SW0_PIN);
    gpio_set_dir(SW0_PIN, GPIO_IN);
    gpio_pull_up(SW0_PIN);
    gpio_init(SW1_PIN);
    gpio_set_dir(SW1_PIN, GPIO_IN);
    gpio_pull_up(SW1_PIN);
    gpio_init(SW2_PIN);
    gpio_set_dir(SW2_PIN, GPIO_IN);
    gpio_pull_up(SW2_PIN);

    //initialize PWM
    for (int i = 0; i < 3; i++) init_pwm(pwm_pins[i]);

    while (true)
    {
        bool sw0_pressed = !gpio_get(SW0_PIN);
        bool sw1_pressed = !gpio_get(SW1_PIN);
        bool sw2_pressed = !gpio_get(SW2_PIN);

        // sw1 toggles the leds on & off
        if (sw1_pressed && !sw1_last_state)
        {
            if (leds_on && brightness == 0)
            {
                brightness = PWM_DEFAULT_LEVEL; //pressing sw1 when leds are dimmed to 0 sets the leds to 50%
            }
            else
            {
                leds_on = !leds_on;

                if (leds_on && brightness == 0)
                {
                    brightness = PWM_DEFAULT_LEVEL;
                }
            }
            sleep_ms(50); //button debounce
        }
        sw1_last_state = sw1_pressed;

        //pressing sw0 decreases brightness
        if (sw0_pressed && leds_on)
        {
            if (brightness >= STEP)
            {
                brightness -= STEP;
            }
            else
            {
                brightness = 0;
            }
        }

        //pressing sw2 increases brightness
        if (sw2_pressed && leds_on)
        {
            brightness += STEP;
            if (brightness > PWM_WRAP)
            {
                brightness = PWM_WRAP; //max 1kHz
            }
        }

        if (leds_on)
        {
            update_leds(brightness);
        }
        else
        {
            update_leds(0); //leds off
        }
        printf("Brightness: %d /999\n", brightness); //for testing

        sleep_ms(5); //5ms sleep
    }
}
