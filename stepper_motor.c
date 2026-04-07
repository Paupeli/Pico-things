#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

#define OPTO_PIN 28
#define IN1 2
#define IN2 3
#define IN3 6
#define IN4 13
#define LED 21

#define NONE   0
#define STATUS 1
#define CALIB  2
#define RUN    3


//global states
const int stepper_pins[] = {IN1, IN2, IN3, IN4};
bool is_calibrated = false;
int steps_per_rev = 0;
volatile bool edge_detected = false;
static int global_step_index = 0;

//half stepping sequence

static const uint turn_seq[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
};

//helpers

void opto_callback(uint gpio, uint32_t event_mask)
{
    if (event_mask & GPIO_IRQ_EDGE_FALL)
        edge_detected = true;
}

void blink_led(int ms)
{
    gpio_put(LED, 1);
    sleep_ms(ms);
    gpio_put(LED, 0);
}


//moves the motor
void stepper_move(int steps)
{
    int direction = (steps > 0) ? 1 : -1;
    int abs_steps = abs(steps);

    for (int i = 0; i < abs_steps; i++)
    {
        global_step_index += direction;
        int row = global_step_index & 7;

        for (int j = 0; j < 4; j++)
        {
            gpio_put(stepper_pins[j], turn_seq[row][j]);
        }

        sleep_ms(4);
    }
}

//moves until falling edge is detected
int stepper_find_edge()
{
    int count = 0;
    edge_detected = false;
    stepper_move(200);

    while (!edge_detected)
    {
        stepper_move(1);
        count++;

        if (count > 15000) return -1; //timeout
    }
    return count + 200; //for better accuracy
}

void calibration()
{
    is_calibrated = false;


    if (stepper_find_edge() == -1)
    {
        printf("Calibration error.\n");
        return;
    }

    //calibrate 3 rounds
    int total = 0;
    for (int i = 0; i < 3; i++)
    {
        int r = stepper_find_edge();
        if (r == -1) return;
        total += r;
        printf("Rotation %d: %d steps\n", i + 1, r);
    }

    steps_per_rev = total / 3;
    is_calibrated = true;
    printf("Calibration finished. Steps: %d\n", steps_per_rev);
}

int get_command(int* n_out)
{
    char buffer[32];
    int pos = 0;

    int c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT) return NONE;

    while (c != '\r' && c != '\n' && pos < 31)
    {
        if (c != PICO_ERROR_TIMEOUT)
        {
            putchar(c);
            buffer[pos++] = (char)c;
        }
        c = getchar_timeout_us(1000000); //100ms timeout for next char
    }
    buffer[pos] = '\0';
    printf("\n");

    if (strcmp(buffer, "status") == 0) return STATUS;
    if (strcmp(buffer, "calib") == 0) return CALIB;
    if (strncmp(buffer, "run", 3) == 0)
    {
        if (strlen(buffer) > 4) *n_out = atoi(buffer + 4);
        else *n_out = 8; //full round
        return RUN;
    }
    return NONE;
}

//main

int main()
{
    stdio_init_all();

    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);

    for (int i = 0; i < 4; i++)
    {
        gpio_init(stepper_pins[i]);
        gpio_set_dir(stepper_pins[i], GPIO_OUT);
    }

    gpio_init(OPTO_PIN);
    gpio_pull_up(OPTO_PIN);
    gpio_set_irq_enabled_with_callback(OPTO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &opto_callback);

    while (true)
    {
        int n_val = 0;
        int command = get_command(&n_val);

        if (command == STATUS)
        {
            printf("Calibrated: %s\n", is_calibrated ? "Yes" : "No");
            if (is_calibrated) printf("Steps per revolution: %d\n", steps_per_rev);
            else printf("Steps not available\n");
        }
        else if (command == CALIB)
        {
            calibration();
        }
        else if (command == RUN)
        {
            if (!is_calibrated)
            {
                printf("Error, calibrate first.\n");
            }
            else
            {
                int target = (steps_per_rev * n_val) / 8;
                printf("Running %d steps (%d/8 revs)\n", target, n_val);
                stepper_move(target);

                if (n_val >= 8) //led for testing
                {
                    blink_led(200);
                }
            }
        }
    }
}
