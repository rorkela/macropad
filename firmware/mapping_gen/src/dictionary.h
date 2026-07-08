#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    const char *token;
    uint16_t val;
} key_lookup_t;

extern const key_lookup_t standard_key_dict[];
extern const size_t standard_key_dict_size;
extern const key_lookup_t consumer_key_dict[];
extern const size_t consumer_key_dict_size;
extern const key_lookup_t mod_dict[];
extern const size_t mod_dict_size;

char *trim_whitespace(char *str);
bool lookup_consumer_value(const char *token, uint16_t *out_val);
bool parse_shortcut(const char *token, uint8_t *out_type, uint8_t *out_mods, uint16_t *out_keycode);
