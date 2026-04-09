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

volatile bool edge_detected = false;

typedef struct{
    const int stepper_pins[4];
    bool is_calibrated;
    int steps_per_rev;
    int global_step_index;
} SystemState;

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

//irq callback
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
void stepper_move(SystemState *state, int steps)
{
    int direction = (steps > 0) ? 1 : -1;
    int abs_steps = abs(steps);

    for (int i = 0; i < abs_steps; i++)
    {
        state->global_step_index =
            (state->global_step_index + direction + 8) % 8;

        for (int j = 0; j < 4; j++)
            gpio_put(state->stepper_pins[j], turn_seq[state->global_step_index][j]);

        sleep_ms(1);
    }
}

//moves until falling edge is detected
int stepper_find_edge(SystemState *state)
{
    int count = 0;
    edge_detected = false;

    while (gpio_get(OPTO_PIN) == 0)
    {
        stepper_move(state,1);
    }

    while (!edge_detected)
    {
        stepper_move(state, 1);
        count++;
    }
    sleep_us(200); //debounce
    return count;
}

void calibration(SystemState *state)
{
    state->is_calibrated = false;

    stepper_find_edge(state); //alignment

    int total = 0;
    int iterations = 3;

    for (int i = 0; i < iterations; i++)
    {
        total += stepper_find_edge(state);
    }

    state->steps_per_rev = total / iterations;

    int gap_width = 0;
    while (gpio_get(OPTO_PIN) == 0) {
        stepper_move(state, 1);
        gap_width++;
    }

    stepper_move(state, -(gap_width / 2));

    state->global_step_index = 0;
    state->is_calibrated = true;

    printf("Calibration finished. Average steps: %d.\n", state->steps_per_rev);
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
        if (sscanf(buffer, "run %d", n_out) != 1)
        *n_out = 8; //full round
        return RUN;
    }
    return NONE;
}

//main
int main()
{
    stdio_init_all();

    SystemState state = {
        .stepper_pins = {IN4, IN3, IN2, IN1},
        .is_calibrated = false,
        .steps_per_rev = 0,
        .global_step_index = 0
    };

    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);

    for (int i = 0; i < 4; i++)
    {
        gpio_init(state.stepper_pins[i]);
        gpio_set_dir(state.stepper_pins[i], GPIO_OUT);
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
            printf("Calibrated: %s\n", state.is_calibrated ? "Yes" : "No");
            if (state.is_calibrated) printf("Steps per revolution: %d\n", state.steps_per_rev);
            else printf("Steps not available.\n");
        }
        else if (command == CALIB)
        {
            calibration(&state);
        }
        else if (command == RUN)
        {
            if (!state.is_calibrated)
            {
                printf("Error, calibrate first.\n");
            }
            else
            {
                int target = (state.steps_per_rev * n_val) / 8;
                printf("Running %d steps (%d/8 revs)\n", target, n_val);

                for(int i = 0; i < n_val; i++)
                {
                    blink_led(100);
                    sleep_ms(100);
                }
                stepper_move(&state, target);
            }
        }
    }
}
