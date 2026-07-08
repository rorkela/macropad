#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "dictionary.h"
#include "keys.h"

const key_lookup_t standard_key_dict[] = {{"NONE", KEY_NONE},
                                          {"A", KEY_A},
                                          {"B", KEY_B},
                                          {"C", KEY_C},
                                          {"D", KEY_D},
                                          {"E", KEY_E},
                                          {"F", KEY_F},
                                          {"G", KEY_G},
                                          {"H", KEY_H},
                                          {"I", KEY_I},
                                          {"J", KEY_J},
                                          {"K", KEY_K},
                                          {"L", KEY_L},
                                          {"M", KEY_M},
                                          {"N", KEY_N},
                                          {"O", KEY_O},
                                          {"P", KEY_P},
                                          {"Q", KEY_Q},
                                          {"R", KEY_R},
                                          {"S", KEY_S},
                                          {"T", KEY_T},
                                          {"U", KEY_U},
                                          {"V", KEY_V},
                                          {"W", KEY_W},
                                          {"X", KEY_X},
                                          {"Y", KEY_Y},
                                          {"Z", KEY_Z},
                                          {"1", KEY_1},
                                          {"2", KEY_2},
                                          {"3", KEY_3},
                                          {"4", KEY_4},
                                          {"5", KEY_5},
                                          {"6", KEY_6},
                                          {"7", KEY_7},
                                          {"8", KEY_8},
                                          {"9", KEY_9},
                                          {"0", KEY_0},
                                          {"ENTER", KEY_ENTER},
                                          {"ESCAPE", KEY_ESCAPE},
                                          {"ESC", KEY_ESCAPE},
                                          {"BACKSPACE", KEY_BACKSPACE},
                                          {"TAB", KEY_TAB},
                                          {"SPACE", KEY_SPACE},
                                          {"-", KEY_MINUS},
                                          {"=", KEY_EQUAL},
                                          {"[", KEY_LEFT_BRACKET},
                                          {"]", KEY_RIGHT_BRACKET},
                                          {"\\", KEY_BACKSLASH},
                                          {"~", KEY_HASHTILDE},
                                          {";", KEY_SEMICOLON},
                                          {"'", KEY_APOSTROPHE},
                                          {"`", KEY_GRAVE},
                                          {",", KEY_COMMA},
                                          {".", KEY_PERIOD},
                                          {"/", KEY_SLASH},
                                          {"CAPSLOCK", KEY_CAPSLOCK},
                                          {"CAPS", KEY_CAPSLOCK},
                                          {"F1", KEY_F1},
                                          {"F2", KEY_F2},
                                          {"F3", KEY_F3},
                                          {"F4", KEY_F4},
                                          {"F5", KEY_F5},
                                          {"F6", KEY_F6},
                                          {"F7", KEY_F7},
                                          {"F8", KEY_F8},
                                          {"F9", KEY_F9},
                                          {"F10", KEY_F10},
                                          {"F11", KEY_F11},
                                          {"F12", KEY_F12},
                                          {"PRINTSCREEN", KEY_PRINTSCREEN},
                                          {"SCROLLLOCK", KEY_SCROLLLOCK},
                                          {"PAUSE", KEY_PAUSE},
                                          {"INSERT", KEY_INSERT},
                                          {"HOME", KEY_HOME},
                                          {"PAGEUP", KEY_PAGEUP},
                                          {"DELETE", KEY_DELETE},
                                          {"DEL", KEY_DELETE},
                                          {"END", KEY_END},
                                          {"PAGEDOWN", KEY_PAGEDOWN},
                                          {"RIGHT", KEY_RIGHT},
                                          {"LEFT", KEY_LEFT},
                                          {"DOWN", KEY_DOWN},
                                          {"UP", KEY_UP},
                                          {"NUMLOCK", KEY_NUMLOCK}};
const size_t standard_key_dict_size = sizeof(standard_key_dict) / sizeof(standard_key_dict[0]);

const key_lookup_t consumer_key_dict[] = {{"MUTE", AUDIO_MUTE},
                                          {"VOL_UP", AUDIO_VOL_UP},
                                          {"VOL+", AUDIO_VOL_UP},
                                          {"VOL_DOWN", AUDIO_VOL_DOWN},
                                          {"VOL-", AUDIO_VOL_DOWN},
                                          {"NEXT_TRACK", MEDIA_NEXT_TRACK},
                                          {"NEXT", MEDIA_NEXT_TRACK},
                                          {"PREV_TRACK", MEDIA_PREV_TRACK},
                                          {"PREV", MEDIA_PREV_TRACK},
                                          {"STOP", MEDIA_STOP},
                                          {"PLAY_PAUSE", MEDIA_PLAY_PAUSE},
                                          {"PLAY", MEDIA_PLAY_PAUSE},
                                          {"BRIGHTNESS_UP", DISPLAY_BRIGHTNESS_UP},
                                          {"BRT+", DISPLAY_BRIGHTNESS_UP},
                                          {"BRIGHTNESS_DOWN", DISPLAY_BRIGHTNESS_DOWN},
                                          {"BRT-", DISPLAY_BRIGHTNESS_DOWN}};
const size_t consumer_key_dict_size = sizeof(consumer_key_dict) / sizeof(consumer_key_dict[0]);

const key_lookup_t mod_dict[] = {{"NONE", KEY_MOD_NONE},     {"LCTRL", KEY_MOD_LCTRL},  {"CTRL", KEY_MOD_LCTRL},
                                 {"LSHIFT", KEY_MOD_LSHIFT}, {"SHIFT", KEY_MOD_LSHIFT}, {"LALT", KEY_MOD_LALT},
                                 {"ALT", KEY_MOD_LALT},      {"LGUI", KEY_MOD_LGUI},    {"GUI", KEY_MOD_LGUI},
                                 {"WIN", KEY_MOD_LGUI},      {"CMD", KEY_MOD_LGUI},     {"RCTRL", KEY_MOD_RCTRL},
                                 {"RSHIFT", KEY_MOD_RSHIFT}, {"RALT", KEY_MOD_RALT},    {"RGUI", KEY_MOD_RGUI}};
const size_t mod_dict_size = sizeof(mod_dict) / sizeof(mod_dict[0]);

char *trim_whitespace(char *str)
{
    char *end;
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
    return str;
}

bool lookup_consumer_value(const char *token, uint16_t *out_val)
{
    for (size_t i = 0; i < consumer_key_dict_size; i++)
    {
        if (strcmp(token, consumer_key_dict[i].token) == 0)
        {
            *out_val = consumer_key_dict[i].val;
            return true;
        }
    }
    return false;
}

bool parse_shortcut(const char *token, uint8_t *type, uint8_t payload[PAYLOAD_SIZE])
{
    *type = USB_TYPE_KEY;

    for (int i = 0; i < PAYLOAD_SIZE; i++)
        payload[i] = 0;

    char token_buf[256];
    strncpy(token_buf, token, sizeof(token_buf) - 1);
    token_buf[sizeof(token_buf) - 1] = '\0';

    bool holds_modifier = false;
    bool holds_consumer = false;
    bool holds_standard = false;

    int num_consumer = 0;

    char *sub_token = strtok(token_buf, "+");
    while (sub_token != NULL)
    {
        char *clean_sub = trim_whitespace(sub_token);
        bool identified = false;

        for (size_t i = 0; i < mod_dict_size; i++)
        {
            if (strcmp(clean_sub, mod_dict[i].token) == 0)
            {
                if (holds_consumer)
                {
                    holds_modifier = true;
                    goto parse_fail;
                }
                payload[0] |= (uint8_t)mod_dict[i].val;
                holds_modifier = true;
                identified = true;
                break;
            }
        }
        if (identified)
        {
            sub_token = strtok(NULL, "+");
            continue;
        }

        for (size_t i = 0; i < consumer_key_dict_size; i++)
        {
            if (strcmp(clean_sub, consumer_key_dict[i].token) == 0)
            {
                if (holds_modifier)
                {
                    holds_consumer = true;
                    goto parse_fail;
                }
                if (holds_consumer)
                {
                    num_consumer = 2;
                    goto parse_fail;
                }
                if (holds_standard)
                {
                    holds_consumer = true;
                    goto parse_fail;
                }
                payload[0] = consumer_key_dict[i].val;
                *type = USB_TYPE_CNTRL;
                holds_consumer = true;
                identified = true;
                num_consumer++;
                break;
            }
        }
        if (identified)
        {
            sub_token = strtok(NULL, "+");
            continue;
        }

        for (size_t i = 0; i < standard_key_dict_size; i++)
        {
            if (strcmp(clean_sub, standard_key_dict[i].token) == 0)
            {
                if (holds_consumer)
                {
                    holds_standard = true;
                    goto parse_fail;
                }
                uint16_t keycode = standard_key_dict[i].val;
                uint8_t byte_idx = ((keycode >> 8) & 0xFF) + 1;
                uint8_t val = keycode & 0xFF;
                payload[byte_idx] |= val;
                break;
            }
        }
        sub_token = strtok(NULL, "+");
    }

    return true;

parse_fail:

    if (holds_modifier && holds_consumer)
    {
        fprintf(stderr, "Syntax Error: Cannot combine modifiers (CTRL/SHIFT) with consumer keys like '%s'\n", token);
    }
    else if (holds_consumer && num_consumer > 1)
    {
        fprintf(stderr, "Syntax Error: Cannot combine multiple media keys like in '%s'\n", token);
    }
    else if (holds_consumer && holds_standard)
    {
        fprintf(stderr, "Syntax Error: Cannot mix media keys with normal keys like in '%s'\n", token);
    }

    return false;
}
