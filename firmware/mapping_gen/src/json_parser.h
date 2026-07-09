#pragma once

#include "stddef.h"
#include "cJSON.h"
#include "defs.h"
char *read_file_to_string(const char *filename);
void parse_json_value_node(cJSON *node, char *out_name, char *out_val);
int parse_json_layout(const char *filename, mappings_t **out_map, size_t *out_bin_size);
