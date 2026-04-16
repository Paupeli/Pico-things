#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/util/queue.h"

#define OPTO_PIN 28
#define IN1 2
#define IN2 3
#define IN3 6
#define IN4 13

#define NONE 0
#define STATUS 1
#define CALIB 2
#define RUN 3
#define BUFFER_SIZE 32

//stepper pins& half stepping sequence
static const int stepper_pins[4] = {IN1, IN2, IN3, IN4};
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

typedef struct
{
    bool is_calibrated;
    int steps_per_rev;
    int current_step_idx;
} SystemState;

SystemState sys = {false, 0, 0};

static queue_t edge_fifo;

//interrupt handler
void opto_callback(uint gpio, uint32_t event_mask)
{
    if (event_mask & GPIO_IRQ_EDGE_FALL)
    {
        bool event = true;
        queue_try_add(&edge_fifo, &event);
    }
}

//check for falling edge
bool check_for_edge()
{
    bool temp;
    if (queue_try_remove(&edge_fifo, &temp))
    {
        return true;
    }
    return false;
}

//clear the queue
void clear_edges()
{
    bool temp;
    while (queue_try_remove(&edge_fifo, &temp));
}

void motor_step(int direction) //move the motor one half step
{
    sys.current_step_idx = (sys.current_step_idx + direction + 8) % 8; //update index and wrap around
    for (int i = 0; i < 4; i++) //loop 4 times to set high/low state
    {
        gpio_put(stepper_pins[i], turn_seq[sys.current_step_idx][i]);
    }
    sleep_ms(1);
}

//calibration
void calibration()
{
    printf("Calibration started.\n");
    sys.is_calibrated = false;

    //move until first falling edge is found
    clear_edges();
    while (!check_for_edge())
    {
        motor_step(1);
    }

    //3 revolutions
    int total_steps = 0;
    for (int i = 0; i < 3; i++)
    {
        int steps_this_rev = 0;
        clear_edges();


        for (int j = 0; j < 100; j++) //move slightly before looking for next falling edge
        {
            motor_step(1);
            steps_this_rev++;
        }

        while (!check_for_edge()) //count until next falling edge is found
        {
            motor_step(1);
            steps_this_rev++;
        }
        total_steps += steps_this_rev;
        printf("Rev %d: %d steps\n", i + 1, steps_this_rev);
    }

    sys.steps_per_rev = total_steps / 3;

    int gap_width = 0; //measure gap width and centre

    while (gpio_get(OPTO_PIN) == 0)
    {
        motor_step(1);
        gap_width++;
        if (gap_width > sys.steps_per_rev / 4) break; //timeout
    }

    printf("Gap width: %d steps.\n", gap_width);
    for (int i = 0; i < (gap_width / 2); i++)
    {
        motor_step(-1);
    }

    sys.is_calibrated = true;
    printf("Calibration finished. Average steps: %d\n", sys.steps_per_rev);
}

//parser
int get_command(int* n_out)
{
    static char buffer[BUFFER_SIZE]; //character store
    static int pos = 0; //tracks cursor position in the buffer

    int c = getchar_timeout_us(0); //character check & return if no character was entered
    if (c == PICO_ERROR_TIMEOUT) return NONE;

    if (c == '\r' || c == '\n') //null terminator, cursor reset & newline
    {
        buffer[pos] = '\0';
        pos = 0;
        printf("\n");

        if (strcmp(buffer, "status") == 0) return STATUS;
        if (strcmp(buffer, "calib") == 0) return CALIB;
        if (strncmp(buffer, "run", 3) == 0)
        {
            if (buffer[3] == '\0')
            {
                *n_out = 8;
                return RUN;
            }
            if (sscanf(buffer + 3, "%d", n_out) == 1)
            {
                if (*n_out >= 1 && *n_out <= 8)
                {
                    return RUN;
                }
            }
        }
        printf("Error, unknown command: %s\n", buffer);
        return NONE;
    }
    else
    {
        if (pos < BUFFER_SIZE - 1)
        {
            buffer[pos++] = (char)c;
            putchar(c);
        }
    }
    return NONE;
}

int main()
{
    stdio_init_all();

    queue_init(&edge_fifo, sizeof(bool), 8);

    for (int i = 0; i < 4; i++)
    {
        gpio_init(stepper_pins[i]);
        gpio_set_dir(stepper_pins[i], GPIO_OUT);
    }
    gpio_init(OPTO_PIN);
    gpio_set_dir(OPTO_PIN, GPIO_IN);
    gpio_pull_up(OPTO_PIN);

    gpio_set_irq_enabled_with_callback(OPTO_PIN, GPIO_IRQ_EDGE_FALL, true, &opto_callback);

    while (true)
    {
        int n_val = 0;
        int command = get_command(&n_val);

        if (command == STATUS)
        {
            if (sys.is_calibrated)
            {
                printf("State: Calibrated\nSteps/Rev: %d\n", sys.steps_per_rev);
            }
            else
            {
                printf("State: Not Calibrated\nSteps/Rev: Not available\n");
            }
        }
        else if (command == CALIB)
        {
            calibration();
        }
        else if (command == RUN)
        {
            if (!sys.is_calibrated)
            {
                printf("Error, calibrate first.\n");
            }
            else
            {
                int target_steps = (sys.steps_per_rev * n_val) / 8; //calculate steps for fraction of a turn n/8
                printf("Running %d/8 revolution: %d steps.\n", n_val, target_steps);
                for (int i = 0; i < target_steps; i++) motor_step(1);
            }
        }
        sleep_ms(1);
    }
}
