#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pico/util/queue.h"

#define ROT_A_PIN 10
#define ROT_B_PIN 11
#define ROT_SW_PIN 12

#define LEDS (sizeof(pwm_pins) / sizeof(pwm_pins[0])) //leds
#define PWM_WRAP 999 //1kHz frequency 1000-1
#define PWM_DEFAULT_LEVEL 500 //50% brightness
#define PWM_CLKDIV 125 //1MHz frequency divider 125MHz/125 = 1MHz
#define BRIGHTNESS_STEP 20 //the change of brightness

const int pwm_pins[] = {20, 21, 22};

static queue_t encoder_queue; //queue to pass steps from the interrupt handler

void encoder_callbck(uint gpio, uint32_t events) //ISR for the rotary encoder
{
    int val = 0;
    val = gpio_get(ROT_B_PIN) ? -1 : 1; //clockwise 1, counter-clockwise -1
    queue_try_add((&encoder_queue), &val);
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

    queue_init(&encoder_queue, sizeof(int), 32);

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
    for (int i = 0; i < LEDS; i++)
    {
        gpio_set_function(pwm_pins[i], GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(pwm_pins[i]);
        pwm_set_wrap(slice, PWM_WRAP);
        pwm_set_enabled(slice, true);
    }

    bool leds_enabled = false;
    uint16_t current_brightness = PWM_DEFAULT_LEVEL;
    bool last_sw_state = true;

    while (true)
    {
        bool current_sw_state = gpio_get(ROT_SW_PIN);

        if (last_sw_state == true && current_sw_state == false) //button toggle (falling edge)
        {
            if (leds_enabled && current_brightness == 0)
            {
                current_brightness = PWM_DEFAULT_LEVEL;
            }
            else
            {
                leds_enabled = !leds_enabled;
            }

            if (leds_enabled) //update leds
            {
                update_leds(current_brightness);
            }
            else
            {
                update_leds(0);
            }

            while (!gpio_get(ROT_SW_PIN)) sleep_ms(10); //debounce
        }
        last_sw_state = current_sw_state;

        //process encoder movement & calculate brightness
        int step_value;
        while (queue_try_remove(&encoder_queue, &step_value))
        {
            int new_val = (int)current_brightness + (step_value * BRIGHTNESS_STEP);
            if (new_val > PWM_WRAP) new_val = PWM_WRAP;
            if (new_val < 0) new_val = 0;
            current_brightness = (uint16_t)new_val;
            if (leds_enabled)
            {
                update_leds(current_brightness);
            }
            else
            {
                update_leds(0);
            }
        }

        printf("Brightness: %d /999\n", current_brightness); // for testing
        sleep_ms(5);
    }
}
