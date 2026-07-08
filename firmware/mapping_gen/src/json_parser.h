#pragma once

#include "stddef.h"

#include "defs.h"

int parse_json_layout(const char *filename, mappings_t **out_map, size_t *out_bin_size);
