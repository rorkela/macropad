#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

#include "defs.h"
#include "dictionary.h"

void set_action_payload(usb_action_t *action, uint8_t type, uint8_t modifiers, uint16_t hid_keycode)
{
    action->usb_type = type;
    memset(action->payload, 0, PAYLOAD_SIZE);

    if (type == USB_TYPE_KEY)
    {
        action->payload[0] = modifiers;
        uint8_t byte_idx = ((hid_keycode >> 8) & 0xFF) + 1;
        uint8_t val = hid_keycode & 0xFF;
        action->payload[byte_idx] |= val;
    }
    else if (type == USB_TYPE_CNTRL)
    {
        action->payload[0] = (uint8_t)(hid_keycode & 0xFF);
    }
}

void parse_json_value_node(cJSON *node, char *out_name, char *out_val)
{
    if (cJSON_IsArray(node) && cJSON_GetArraySize(node) == 2)
    {
        cJSON *name_item = cJSON_GetArrayItem(node, 0);
        cJSON *val_item = cJSON_GetArrayItem(node, 1);
        if (cJSON_IsString(name_item) && cJSON_IsString(val_item))
        {
            strncpy(out_name, name_item->valuestring, MAX_BINDING_NAME);
            strncpy(out_val, val_item->valuestring, 64);
        }
    }
    else if (cJSON_IsString(node))
    {
        strncpy(out_name, node->valuestring, MAX_BINDING_NAME);
        strncpy(out_val, node->valuestring, 64);
    }
}

char *read_file_to_string(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
        return NULL;

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *)malloc(length + 1);
    if (buffer)
    {
        fread(buffer, 1, length, file);
        buffer[length] = '\0';
    }
    fclose(file);
    return buffer;
}

int parse_json_layout(const char *filename, mappings_t **out_map, size_t *out_bin_size)
{
    char *json_data = read_file_to_string(filename);
    if (!json_data)
    {
        fprintf(stderr, "Error reading file: %s\n", filename);
        return -1;
    }

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);

    if (!root || !cJSON_IsArray(root))
    {
        fprintf(stderr, "JSON Parse Error: Root element must be an array of layers.\n");
        cJSON_Delete(root);
        return -1;
    }

    int num_layers = cJSON_GetArraySize(root);
    if (num_layers == 0)
    {
        fprintf(stderr, "Error: No layers defined in JSON.\n");
        cJSON_Delete(root);
        return -1;
    }

    *out_bin_size = sizeof(mappings_t) + (sizeof(macro_layer_t) * num_layers);
    mappings_t *map = (mappings_t *)calloc(1, *out_bin_size);
    if (!map)
    {
        cJSON_Delete(root);
        return -1;
    }
    map->magic_number = MAGIC;
    map->total_layers = num_layers;

    for (int i = 0; i < num_layers; i++)
    {
        cJSON *layer_json = cJSON_GetArrayItem(root, i);
        macro_layer_t *active_layer = &map->layers[i];

        cJSON *name_node = cJSON_GetObjectItemCaseSensitive(layer_json, "name");
        if (cJSON_IsString(name_node))
        {
            strncpy(active_layer->layer_name, name_node->valuestring, MAX_LAYER_NAME);
        }

        cJSON *keys_obj = cJSON_GetObjectItemCaseSensitive(layer_json, "keys");
        if (cJSON_IsObject(keys_obj))
        {
            for (int k = 0; k < NUM_KEYS; k++)
            {
                char key_prop_name[16];
                snprintf(key_prop_name, sizeof(key_prop_name), "key_%d", k);

                cJSON *key_node = cJSON_GetObjectItemCaseSensitive(keys_obj, key_prop_name);
                if (key_node)
                {
                    char disp_name[32] = {0};
                    char shortcut[64] = {0};
                    parse_json_value_node(key_node, disp_name, shortcut);

                    if (disp_name[0] != '\0' && shortcut[0] != '\0')
                    {
                        strncpy(active_layer->binding_names[k], disp_name, MAX_BINDING_NAME);

                        uint8_t type = USB_TYPE_KEY, mods = 0;
                        uint16_t keycode = 0;
                        if (parse_shortcut(shortcut, &type, &mods, &keycode))
                        {
                            set_action_payload(&active_layer->actions[k], type, mods, keycode);
                        }
                        else
                        {
                            goto parse_fail;
                        }
                    }
                }
            }
        }

        cJSON *enc_obj = cJSON_GetObjectItemCaseSensitive(layer_json, "encoder");
        if (cJSON_IsObject(enc_obj))
        {
            active_layer->actions[9].usb_type = USB_TYPE_CNTRL;
            cJSON *ccw = cJSON_GetObjectItemCaseSensitive(enc_obj, "ccw");
            cJSON *cw = cJSON_GetObjectItemCaseSensitive(enc_obj, "cw");
            cJSON *sw = cJSON_GetObjectItemCaseSensitive(enc_obj, "sw");

            char disp_name[32];
            char command_str[64];
            uint16_t val = 0;

            if (ccw)
            {
                memset(disp_name, 0, sizeof(disp_name));
                memset(command_str, 0, sizeof(command_str));
                parse_json_value_node(ccw, disp_name, command_str);
                strncpy(active_layer->binding_names[9], disp_name, MAX_BINDING_NAME);
                if (lookup_consumer_value(command_str, &val))
                    active_layer->actions[9].payload[0] = (uint8_t)val;
            }
            if (cw)
            {
                memset(disp_name, 0, sizeof(disp_name));
                memset(command_str, 0, sizeof(command_str));
                parse_json_value_node(cw, disp_name, command_str);
                strncpy(active_layer->binding_names[10], disp_name, MAX_BINDING_NAME);
                if (lookup_consumer_value(command_str, &val))
                    active_layer->actions[9].payload[1] = (uint8_t)val;
            }
            if (sw)
            {
                memset(disp_name, 0, sizeof(disp_name));
                memset(command_str, 0, sizeof(command_str));
                parse_json_value_node(sw, disp_name, command_str);
                if (lookup_consumer_value(command_str, &val))
                    active_layer->actions[9].payload[2] = (uint8_t)val;
            }
        }
    }

    cJSON_Delete(root);

    *out_map = map;
    return 0;

parse_fail:
    cJSON_Delete(root);
    free(map);
    return 1;
}
