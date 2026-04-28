#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "eeprom.h"
#include "log.h"

#define BUTTON_DEBOUNCE 200000
#define CMD_BUFFER_SIZE 16

const uint BUTTON_PINS[] = {9, 8, 7};
const uint LED_PINS[] = {20, 21, 22};

typedef enum
{
    EVENT_BUTTON_PRESSED,
    EVENT_UART_READ,
    EVENT_UART_ERASE
} event_type_t;

typedef struct
{
    event_type_t type;
    uint8_t index;
} system_event_t;

queue_t event_queue;

void gpio_callback(uint gpio, uint32_t events)
{
    static uint64_t last_interrupt_time = 0;
    uint64_t current_time = time_us_64();

    if (current_time - last_interrupt_time < BUTTON_DEBOUNCE) {
        return;
    }
    last_interrupt_time = current_time;

    for (int i = 0; i < 3; i++) {
        if (gpio == BUTTON_PINS[i]) {
            system_event_t ev = {.type = EVENT_BUTTON_PRESSED, .index = i};
            queue_try_add(&event_queue, &ev);
        }
    }
}

void parse_input()
{
    static char cmd_buf[CMD_BUFFER_SIZE];
    static int ptr = 0;

    int c = getchar_timeout_us(0);

    while (c != PICO_ERROR_TIMEOUT) {
        if (c == '\r' || c == '\n') {
            if (ptr > 0) {
                cmd_buf[ptr] = '\0';
                system_event_t ev;
                bool valid = false;

                if (strcmp(cmd_buf, "read") == 0) {
                    ev.type = EVENT_UART_READ;
                    valid = true;
                }
                else if (strcmp(cmd_buf, "erase") == 0) {
                    ev.type = EVENT_UART_ERASE;
                    valid = true;
                } else {
                    printf("Error, unknown command %s\n", cmd_buf);
                }

                if (valid) {
                    queue_try_add(&event_queue, &ev);
                }
                ptr = 0;
            }
        }
        else if (ptr < (CMD_BUFFER_SIZE - 1)) {
            cmd_buf[ptr++] = (char)c;
        }
        c = getchar_timeout_us(0);
    }
}

int main()
{
    stdio_init_all();
    eeprom_init();

    log_init();

    queue_init(&event_queue, sizeof(system_event_t), 16);

    for (int i = 0; i < 3; i++) {
        gpio_init(LED_PINS[i]);
        gpio_set_dir(LED_PINS[i], GPIO_OUT);

        gpio_init(BUTTON_PINS[i]);
        gpio_set_dir(BUTTON_PINS[i], GPIO_IN);
        gpio_pull_up(BUTTON_PINS[i]);
    }

    ledstate current_ls;
    read_led_state(&current_ls);

    uint8_t led_mask = led_state_is_valid(&current_ls) ? current_ls.state : 0b010;

    for (int i = 0; i < 3; i++) {
        gpio_put(LED_PINS[i], (led_mask >> i) & 1);
    }

    log_write("Boot");
    print_led_state(led_mask);

    for (int i = 0; i < 3; i++) {
        gpio_set_irq_enabled_with_callback(BUTTON_PINS[i],
        GPIO_IRQ_EDGE_FALL,
        true, &gpio_callback);
    }

    while (true)
    {
        parse_input();
        system_event_t ev;
        if (queue_try_remove(&event_queue, &ev)) {
            if (ev.type == EVENT_BUTTON_PRESSED) {
                led_mask ^= (1 << ev.index);
                gpio_put(LED_PINS[ev.index], (led_mask >> ev.index) & 1);

                //save to eeprom and print
                write_led_state(led_mask);
                print_led_state(led_mask);
                log_save_state(led_mask);
            }
            else if (ev.type == EVENT_UART_READ) {
                log_read_all();
            }
            else if (ev.type == EVENT_UART_ERASE) {
                log_erase();
                printf("Log erased.\n");
            }
        }
    }
}
