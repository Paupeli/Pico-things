#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

#define SW0_PIN 9
#define UART_ID uart1
#define BAUD_RATE 9600
#define BUFFER_SIZE 80

enum
{
    BUTTON_WAIT,
    CONNECTION,
    VERSION,
    DEVEUI
};

int state = BUTTON_WAIT;

//read uart with timeout
bool read_string_with_timeout(uart_inst_t* uart, char* buffer, int max_len, uint32_t timeout_ms)
{
    int pos = 0;
    absolute_time_t timeout_time = make_timeout_time_ms(timeout_ms);

    while (get_absolute_time() < timeout_time && pos < max_len - 1) //run loop until time is up or buffer is full
    {
        if (uart_is_readable(uart)) //process response immediately
        {
            char c = uart_getc(uart);
            if (c == '\n')
            {
                buffer[pos] = '\0';
                return true;
            }
            if (c != '\r')
            {
                buffer[pos++] = c;
            }
        }
    }
    return false;
}

void print_deveui(const char* buffer)
{
    char* hex_ptr = strchr(buffer, ','); //find comma
    if (hex_ptr)
    {
        hex_ptr++; //move past comma

        while (*hex_ptr)
        {
            if (*hex_ptr != ':' && *hex_ptr != ' ')
            {
                //no colons and spaces
                putchar(tolower((unsigned char)*hex_ptr));
            }
            hex_ptr++;
        }
        printf("\n");
    }
}

int main()
{
    stdio_init_all();

    //uart init
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(4, GPIO_FUNC_UART);
    gpio_set_function(5, GPIO_FUNC_UART);

    //button init
    gpio_init(SW0_PIN);
    gpio_set_dir(SW0_PIN, GPIO_IN);
    gpio_pull_up(SW0_PIN);

    char buffer[BUFFER_SIZE];

    while (true)
    {
        switch (state)
        {
        case BUTTON_WAIT: //wait for button to be pressed
            if (gpio_get(SW0_PIN) == 0)
            {
                sleep_ms(200); //debounce
                state = CONNECTION;
            }
            break;

        case CONNECTION: //connection test
            {
                int attempts;
                for (attempts = 0; attempts < 5; attempts++)
                {
                    uart_puts(UART_ID, "AT\r\n");
                    if (read_string_with_timeout(UART_ID, buffer, sizeof(buffer), 500))
                    {
                        if (strstr(buffer, "OK"))
                        {
                            //check if response contains ok
                            printf("Connected to LoRa module.\n");
                            state = VERSION;
                            break;
                        }
                    }
                }
                if (attempts == 5)
                {
                    printf("Module not responding.\n");
                    state = BUTTON_WAIT;
                }
                break;
            }

        case VERSION: //version
            uart_puts(UART_ID, "AT+VER\r\n");
            if (read_string_with_timeout(UART_ID, buffer, sizeof(buffer), 500))
            {
                printf("Version: %s\n", buffer);
                state = DEVEUI;
            }
            else
            {
                printf("Module stopped responding.\n");
                state = BUTTON_WAIT;
            }
            break;

        case DEVEUI:
            uart_puts(UART_ID, "AT+ID=DevEui\r\n");
            if (read_string_with_timeout(UART_ID, buffer, sizeof(buffer), 500))
            {
                printf("DevEui: ");
                print_deveui(buffer);
                state = BUTTON_WAIT;
            }
            else
            {
                printf("Module stopped responding.\n");
                state = BUTTON_WAIT;
            }
            break;

        default:
            printf("Error, unknown state %d. Press the button again to start.\n", state);
            state = BUTTON_WAIT;
            break;
        }
    }
}
