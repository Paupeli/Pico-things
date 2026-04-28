#include "log.h"
#include "eeprom.h"
#include <stdio.h>
#include <string.h>

static int next_entry_idx = 0;

uint16_t crc16(const uint8_t* data_p, size_t length)
{
    uint8_t x;
    uint16_t crc = 0xFFFF;
    while (length--) {
        x = crc >> 8 ^ *data_p++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x << 5)) ^ ((uint16_t)x);
    }
    return crc;
}

//check if log entry is valid
bool valid_entry(uint8_t *buffer)
{
    if (buffer[0] == 0) return false; //first character not zero

    int null_idx = -1; //null terminator before index 62
    for (int j = 0; j < (LOG_ENTRY_SIZE - 2); j++) {
        if (buffer[j] == 0) {
            null_idx = j;
            break;
        }
    }
    if (null_idx == -1) return false;

    return (crc16(buffer, null_idx + 3) == 0); //crc validation
}

void log_init()
{
    next_entry_idx = 0;
    for (int i = 0; i < LOG_MAX_ENTRIES; i++) {
        uint8_t buffer[LOG_ENTRY_SIZE];
        eeprom_read_bytes(i * LOG_ENTRY_SIZE, buffer, LOG_ENTRY_SIZE);

        if (!valid_entry(buffer)) {
            next_entry_idx = i; //start here if invalid entry is found
            return;
        }
        next_entry_idx = i + 1;
    }
}

void log_write(const char* msg)
{
    if (next_entry_idx >= LOG_MAX_ENTRIES) {
        log_erase(); //reset if full
    }

    uint8_t buffer[LOG_ENTRY_SIZE] = {0};

    size_t s_len = 0;
    while (msg[s_len] != '\0' && s_len < LOG_MAX_STR_LEN) {
        buffer[s_len] = (uint8_t)msg[s_len];
        s_len++;
    }
    buffer[s_len] = 0;

    uint16_t crc = crc16(buffer, s_len + 1);

    buffer[s_len + 1] = (uint8_t)(crc >> 8);   //msb
    buffer[s_len + 2] = (uint8_t)(crc & 0xFF); //lsb

    uint16_t base_addr = (uint16_t)(next_entry_idx * LOG_ENTRY_SIZE); //address calculation
    eeprom_write_page(base_addr, buffer, LOG_ENTRY_SIZE);

    next_entry_idx++;
}

void log_read_all()
{
    printf("EEPROM Log\n");
    for (int i = 0; i < LOG_MAX_ENTRIES; i++) {
        uint8_t buffer[LOG_ENTRY_SIZE];
        eeprom_read_bytes(i * LOG_ENTRY_SIZE, buffer, LOG_ENTRY_SIZE);

        if (!valid_entry(buffer)) break;

        printf("%02d: %s\n", i + 1, (char*)buffer);
    }
}

void log_erase()
{
    uint8_t zero = 0;
    for (int i = 0; i < LOG_MAX_ENTRIES; i++) {
        eeprom_write_bytes(i * LOG_ENTRY_SIZE, &zero, 1); //write a zero at the first byte of every entry
    }
    next_entry_idx = 0;
}

void log_save_state(uint8_t led_mask)
{
    char log_msg[64];
    snprintf(log_msg, sizeof(log_msg), "State: 0x%02X LED0:%s LED1:%s LED2:%s",
             (unsigned int)led_mask,
             (led_mask & 1) ? "ON" : "OFF",
             ((led_mask >> 1) & 1) ? "ON" : "OFF",
             ((led_mask >> 2) & 1) ? "ON" : "OFF");

    log_write(log_msg);
}
