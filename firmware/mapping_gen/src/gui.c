#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include "microui.h"
#include "renderer.h"
#include "cJSON.h" // Ensure you compile with cJSON.c

#define MAX_LAYERS 8
#define MAX_NAME_LEN 32
#define MAX_VAL_LEN 64

// Structs to hold UI and Profile State
typedef struct {
    char label[MAX_VAL_LEN];   // What shows on the display
    char command[MAX_VAL_LEN]; // The keys to be pressed
} MacroKey;

typedef struct {
    char name[MAX_NAME_LEN];
    MacroKey keys[9];          // 9 keys, each with 2 fields
    char encoder_ccw[MAX_VAL_LEN];
    char encoder_cw[MAX_VAL_LEN];
    char encoder_sw[MAX_VAL_LEN];
} LayerProfile;

// Global or static state for the configuration
static LayerProfile layers[MAX_LAYERS];
static int layer_count = 3; // Start with 3 layers matching your example
static int current_layer_idx = 0;

// Helper to fill initial mock data matching your JSON structure
// Reads layout.json dynamically and populates the C layout structures
void init_mock_data(void) {
    // Reset global counter and memory space
    layer_count = 0;
    current_layer_idx = 0;
    memset(layers, 0, sizeof(layers));

    // 1. Read layout.json file buffer safely
    FILE *f = fopen("layout.json", "r");
    if (!f) {
        printf("Warning: layout.json not found. Launching with an empty profile.\n");
        // Safe fall back default if file is missing
        layer_count = 1;
        strcpy(layers[0].name, "NEW_LYR1");
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *json_data = (char *)malloc(size + 1);
    if (!json_data) {
        fclose(f);
        return;
    }
    
    size_t read_bytes = fread(json_data, 1, size, f);
    json_data[read_bytes] = '\0';
    fclose(f);

    // 2. Parse file buffer using cJSON
    cJSON *root_array = cJSON_Parse(json_data);
    free(json_data); // Free raw text memory immediately

    if (!root_array || !cJSON_IsArray(root_array)) {
        printf("Error: layout.json format is invalid or corrupted.\n");
        if (root_array) cJSON_Delete(root_array);
        layer_count = 1;
        strcpy(layers[0].name, "NEW_LYR1");
        return;
    }

    // 3. Extract items array sequence loop
    int elements = cJSON_GetArraySize(root_array);
    layer_count = (elements > MAX_LAYERS) ? MAX_LAYERS : elements;

    for (int i = 0; i < layer_count; i++) {
        cJSON *layer_obj = cJSON_GetArrayItem(root_array, i);
        if (!layer_obj) continue;

        // Pull Layer Identifier Label
        cJSON *name_item = cJSON_GetObjectItemCaseSensitive(layer_obj, "name");
        if (cJSON_IsString(name_item)) {
            strncpy(layers[i].name, name_item->valuestring, MAX_NAME_LEN - 1);
        }

        // Pull 9-Key Matrix Bindings maps
        cJSON *keys_obj = cJSON_GetObjectItemCaseSensitive(layer_obj, "keys");
        if (keys_obj && cJSON_IsObject(keys_obj)) {
            for (int k = 0; k < 9; k++) {
                char key_id[16];
                sprintf(key_id, "key_%d", k);
                cJSON *key_item = cJSON_GetObjectItemCaseSensitive(keys_obj, key_id);
                
                if (!key_item) continue;

                if (cJSON_IsArray(key_item) && cJSON_GetArraySize(key_item) == 2) {
                    // Item matches schema: ["LABEL", "COMMAND"]
                    cJSON *lbl = cJSON_GetArrayItem(key_item, 0);
                    cJSON *cmd = cJSON_GetArrayItem(key_item, 1);
                    if (cJSON_IsString(lbl)) strncpy(layers[i].keys[k].label, lbl->valuestring, MAX_VAL_LEN - 1);
                    if (cJSON_IsString(cmd)) strncpy(layers[i].keys[k].command, cmd->valuestring, MAX_VAL_LEN - 1);
                } 
                else if (cJSON_IsString(key_item)) {
                    // Item matches schema: "COMMAND" (No separate presentation display string label)
                    layers[i].keys[k].label[0] = '\0'; // Explicit blank identity 
                    strncpy(layers[i].keys[k].command, key_item->valuestring, MAX_VAL_LEN - 1);
                }
            }
        }

        // Pull Encoder Properties context blocks
        cJSON *enc_obj = cJSON_GetObjectItemCaseSensitive(layer_obj, "encoder");
        if (enc_obj && cJSON_IsObject(enc_obj)) {
            cJSON *ccw = cJSON_GetObjectItemCaseSensitive(enc_obj, "ccw");
            cJSON *cw  = cJSON_GetObjectItemCaseSensitive(enc_obj, "cw");
            cJSON *sw  = cJSON_GetObjectItemCaseSensitive(enc_obj, "sw");

            // Helper lambda handling logic for mixed encoder formats if any exist
            if (cJSON_IsString(ccw)) strncpy(layers[i].encoder_ccw, ccw->valuestring, MAX_VAL_LEN - 1);
            else if (cJSON_IsArray(ccw) && cJSON_GetArraySize(ccw) > 0) {
                cJSON *first = cJSON_GetArrayItem(ccw, 0);
                if (cJSON_IsString(first)) strncpy(layers[i].encoder_ccw, first->valuestring, MAX_VAL_LEN - 1);
            }

            if (cJSON_IsString(cw)) strncpy(layers[i].encoder_cw, cw->valuestring, MAX_VAL_LEN - 1);
            else if (cJSON_IsArray(cw) && cJSON_GetArraySize(cw) > 0) {
                cJSON *first = cJSON_GetArrayItem(cw, 0);
                if (cJSON_IsString(first)) strncpy(layers[i].encoder_cw, first->valuestring, MAX_VAL_LEN - 1);
            }

            if (cJSON_IsString(sw)) strncpy(layers[i].encoder_sw, sw->valuestring, MAX_VAL_LEN - 1);
        }
    }

    cJSON_Delete(root_array);
    printf("Successfully loaded %d layers from layout.json\n", layer_count);
}

// Function to convert state to cJSON and write to file
void save_config_to_json(const char *filename) {
    cJSON *root_array = cJSON_CreateArray();

    for (int i = 0; i < layer_count; i++) {
        cJSON *layer_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(layer_obj, "name", layers[i].name);

        // Keys Object
        cJSON *keys_obj = cJSON_CreateObject();
        for (int k = 0; k < 9; k++) {
            char key_id[16];
            sprintf(key_id, "key_%d", k);

            // If a display label is provided, save as [ "Label", "Command" ]
            if (strlen(layers[i].keys[k].label) > 0) {
                cJSON *pair_arr = cJSON_CreateArray();
                cJSON_AddItemToArray(pair_arr, cJSON_CreateString(layers[i].keys[k].label));
                cJSON_AddItemToArray(pair_arr, cJSON_CreateString(layers[i].keys[k].command));
                cJSON_AddItemToObject(keys_obj, key_id, pair_arr);
            } else {
                // Otherwise, just save the command string directly
                cJSON_AddStringToObject(keys_obj, key_id, layers[i].keys[k].command);
            }
        }
        cJSON_AddItemToObject(layer_obj, "keys", keys_obj);

        // Encoder Object
        cJSON *encoder_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(encoder_obj, "ccw", layers[i].encoder_ccw);
        cJSON_AddStringToObject(encoder_obj, "cw", layers[i].encoder_cw);
        cJSON_AddStringToObject(encoder_obj, "sw", layers[i].encoder_sw);
        cJSON_AddItemToObject(layer_obj, "encoder", encoder_obj);

        cJSON_AddItemToArray(root_array, layer_obj);
    }

    char *json_str = cJSON_Print(root_array);
    FILE *f = fopen(filename, "w");
    if (f) {
        fputs(json_str, f);
        fclose(f);
        printf("Configuration saved successfully to %s!\n", filename);
    }
    cJSON_free(json_str);
    cJSON_Delete(root_array);
}

// --- Text layout callbacks for microui ---
static int text_width(mu_Font font, const char *text, int len) {
    (void)font;
    if (len == -1) len = strlen(text);
    return r_get_text_width(text, len);
}

static int text_height(mu_Font font) {
    (void)font;
    return r_get_text_height();
}

void call_gui(void)
{
    SDL_Init(SDL_INIT_EVERYTHING);
    r_init();
    init_mock_data();

    mu_Context ctx;
    mu_init(&ctx);
    ctx.text_width = text_width;
    ctx.text_height = text_height;

    int running = 1;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_MOUSEMOTION:
                mu_input_mousemove(&ctx, e.motion.x, e.motion.y);
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT)
                    mu_input_mousedown(&ctx, e.button.x, e.button.y, MU_MOUSE_LEFT);
                break;
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT)
                    mu_input_mouseup(&ctx, e.button.x, e.button.y, MU_MOUSE_LEFT);
                break;
            case SDL_TEXTINPUT:
                mu_input_text(&ctx, e.text.text);
                break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_BACKSPACE) mu_input_keydown(&ctx, MU_KEY_BACKSPACE);
                if (e.key.keysym.sym == SDLK_RETURN)    mu_input_keydown(&ctx, MU_KEY_RETURN);
                break;
            case SDL_KEYUP:
                if (e.key.keysym.sym == SDLK_BACKSPACE) mu_input_keyup(&ctx, MU_KEY_BACKSPACE);
                if (e.key.keysym.sym == SDLK_RETURN)    mu_input_keyup(&ctx, MU_KEY_RETURN);
                break;
            }
        }

        mu_begin(&ctx);

        // Define a clean window layout for our Macropad Configurator
        // Define a clean window layout for our Macropad Configurator
        int window_flags = MU_OPT_NOTITLE | MU_OPT_NORESIZE | MU_OPT_NOFRAME;
        if (mu_begin_window_ex(&ctx, "Macropad Keymap Configurator", mu_rect(30, 30, 540, 520),window_flags)) {
            
            // --- TOP LAYER NAVIGATION & CREATION ---
            mu_layout_row(&ctx, 3, (int[]){120, -140, -1}, 30);
            mu_label(&ctx, "Select Layer:");
            
            // Layer cycling selection
            char layer_lbl[32];
            sprintf(layer_lbl, "[%d/%d] - %s", current_layer_idx + 1, layer_count, layers[current_layer_idx].name);
            if (mu_button(&ctx, layer_lbl)) {
                current_layer_idx = (current_layer_idx + 1) % layer_count;
            }
            
            // Add a layer dynamically
            if (mu_button(&ctx, "+ Add Layer")) {
                if (layer_count < MAX_LAYERS) {
                    sprintf(layers[layer_count].name, "NEW_LYR%d", layer_count + 1);
for(int i = 0; i < 9; i++) {
    strcpy(layers[layer_count].keys[i].label, "");       // Leave label blank by default
    strcpy(layers[layer_count].keys[i].command, "NONE"); // Set the command to NONE
}
                    strcpy(layers[layer_count].encoder_ccw, "VOL-");
                    strcpy(layers[layer_count].encoder_cw, "VOL+");
                    strcpy(layers[layer_count].encoder_sw, "MUTE");
                    current_layer_idx = layer_count;
                    layer_count++;
                }
            }

            // --- RENAME CURRENT LAYER ---
            mu_layout_row(&ctx, 2, (int[]){120, -1}, 30);
            mu_label(&ctx, "Layer Name:");
            mu_textbox(&ctx, layers[current_layer_idx].name, MAX_NAME_LEN); // <-- Fixed here

// --- 3x3 MATRIX KEY MAPPINGS ---
            mu_layout_row(&ctx, 1, (int[]){-1}, 25);
            mu_label(&ctx, "Key Matrix Bindings (Left: Display Label | Right: Key Combo):");

            LayerProfile *cur = &layers[current_layer_idx];
            
            // Render 3 rows. Each row has 3 pairs of textboxes (6 textboxes total per row)
            // Layout widths: 3 pairs of [Label (75px), Combo (95px)] = ~510px total width
            int column_widths[] = {75, 95, 75, 95, 75, 95};

            for (int r = 0; r < 3; r++) {
                mu_layout_row(&ctx, 6, column_widths, 35);
                for (int c = 0; c < 3; c++) {
                    int idx = r * 3 + c;
                    
                    // Field 1: Display Word
                    mu_textbox(&ctx, cur->keys[idx].label, MAX_VAL_LEN);
                    
                    // Field 2: Keys to press
                    mu_textbox(&ctx, cur->keys[idx].command, MAX_VAL_LEN);
                }
            }

            // --- ENCODER CONFIGURATION ---
            mu_layout_row(&ctx, 1, (int[]){-1}, 25);
            mu_label(&ctx, "Rotary Encoder Settings:");

            mu_layout_row(&ctx, 2, (int[]){120, -1}, 30);
            mu_label(&ctx, "Counter-CW:");
            mu_textbox(&ctx, cur->encoder_ccw, MAX_VAL_LEN); // <-- Fixed here

            mu_label(&ctx, "Clockwise:");
            mu_textbox(&ctx, cur->encoder_cw, MAX_VAL_LEN);  // <-- Fixed here

            mu_label(&ctx, "Switch (Press):");
            mu_textbox(&ctx, cur->encoder_sw, MAX_VAL_LEN);  // <-- Fixed here

            // --- FINISH & SAVE ACTION ---
            mu_layout_row(&ctx, 1, (int[]){-1}, 40);
            if (mu_button(&ctx, "FINISH AND FLASH MACROPAD")) {
                save_config_to_json("layout.json");
                running = 0; 
            }

            mu_end_window(&ctx);
        }

        mu_end(&ctx);

        // Rendering phase
        r_clear(mu_color(45, 45, 48, 255));
        mu_Command *cmd = NULL;
        while (mu_next_command(&ctx, &cmd)) {
            switch (cmd->type) {
            case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
            case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
            case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
            case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
            }
        }
        r_present();
    }

    SDL_Quit();
}
