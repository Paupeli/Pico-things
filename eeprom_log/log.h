#ifndef EEPROM_LOG_LOG_H
#define EEPROM_LOG_LOG_H

#include <stdint.h>
#include <stddef.h>

#define LOG_START_ADDR 0
#define LOG_ENTRY_SIZE 64
#define LOG_MAX_ENTRIES 32
#define LOG_MAX_STR_LEN 61

void log_init();
void log_write(const char *msg);
void log_read_all();
void log_erase();
void log_save_state(uint8_t led_mask);

uint16_t crc16(const uint8_t *data_p, size_t length);

#endif //EEPROM_LOG_LOG_H
