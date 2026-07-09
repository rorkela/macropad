#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include "microui.h"
#include "renderer.h"
#include "json_parser.h"
#include "cJSON.h" // Ensure you compile with cJSON.c
#include "defs.h"
#include "themes.h"
//JSON Functions
#define MAX_BINDING_VALUE 64
#define MAX_LAYERS 100
typedef struct {
    char name[MAX_LAYER_NAME + 1];

    char key_names[NUM_KEYS][MAX_BINDING_NAME + 1];
    char key_binds[NUM_KEYS][MAX_BINDING_VALUE + 1];

    char encoder_ccw[MAX_BINDING_VALUE + 1];
    char encoder_cw[MAX_BINDING_VALUE + 1];
    char encoder_sw[MAX_BINDING_VALUE + 1];
} layer_t;

static layer_t layers[MAX_LAYERS];
static size_t num_layers = 0;
static const char *current_filename = NULL;
static int running = 1;
int parse_layout_json()
{
    char *json = read_file_to_string(current_filename);
    if (!json)
        return -1;

    cJSON *root = cJSON_Parse(json);
    free(json);

    if (!root || !cJSON_IsArray(root))
    {
        cJSON_Delete(root);
        return -1;
    }

    int layer_count = cJSON_GetArraySize(root);
    if (layer_count > MAX_LAYERS)
        layer_count = MAX_LAYERS;

    memset(layers, 0, sizeof(layers));

    for (int i = 0; i < layer_count; i++)
    {
        cJSON *layer = cJSON_GetArrayItem(root, i);

        /* ---------- Layer name ---------- */

        cJSON *name = cJSON_GetObjectItemCaseSensitive(layer, "name");
        if (cJSON_IsString(name))
        {
            strncpy(layers[i].name, name->valuestring, MAX_LAYER_NAME);
        }

        /* ---------- Keys ---------- */

        cJSON *keys = cJSON_GetObjectItemCaseSensitive(layer, "keys");
        if (cJSON_IsObject(keys))
        {
            for (int k = 0; k < NUM_KEYS; k++)
            {
                char key_name[16];
                snprintf(key_name, sizeof(key_name), "key_%d", k);

                cJSON *key = cJSON_GetObjectItemCaseSensitive(keys, key_name);

                if (key)
                {
                    parse_json_value_node(
                        key,
                        layers[i].key_names[k],
                        layers[i].key_binds[k]);
                }
            }
        }


        cJSON *enc = cJSON_GetObjectItemCaseSensitive(layer, "encoder");
        if (cJSON_IsObject(enc))
        {
            char dummy[MAX_BINDING_NAME + 1];

            cJSON *ccw = cJSON_GetObjectItemCaseSensitive(enc, "ccw");
            if (ccw)
                parse_json_value_node(ccw, dummy, layers[i].encoder_ccw);

            cJSON *cw = cJSON_GetObjectItemCaseSensitive(enc, "cw");
            if (cw)
                parse_json_value_node(cw, dummy, layers[i].encoder_cw);

            cJSON *sw = cJSON_GetObjectItemCaseSensitive(enc, "sw");
            if (sw)
                parse_json_value_node(sw, dummy, layers[i].encoder_sw);
        }
    }
    num_layers = layer_count;
    cJSON_Delete(root);
    return 0;
}
int write_layout_json()
{
    cJSON *root = cJSON_CreateArray();
    if (!root)
        return -1;

    for (size_t i = 0; i < num_layers; i++)
    {
        cJSON *layer = cJSON_CreateObject();
        cJSON_AddItemToArray(root, layer);

        cJSON_AddStringToObject(layer, "name", layers[i].name);

        /* Keys */
        cJSON *keys = cJSON_CreateObject();
        cJSON_AddItemToObject(layer, "keys", keys);

        for (int k = 0; k < NUM_KEYS; k++)
        {
            char key[16];
            snprintf(key, sizeof(key), "key_%d", k);

            if (strcmp(layers[i].key_names[k], layers[i].key_binds[k]) == 0)
            {
                cJSON_AddStringToObject(keys, key, layers[i].key_binds[k]);
            }
            else
            {
                cJSON *arr = cJSON_CreateArray();
                cJSON_AddItemToArray(arr,
                    cJSON_CreateString(layers[i].key_names[k]));
                cJSON_AddItemToArray(arr,
                    cJSON_CreateString(layers[i].key_binds[k]));
                cJSON_AddItemToObject(keys, key, arr);
            }
        }

        /* Encoder */
        cJSON *enc = cJSON_CreateObject();
        cJSON_AddItemToObject(layer, "encoder", enc);

        cJSON_AddStringToObject(enc, "ccw", layers[i].encoder_ccw);
        cJSON_AddStringToObject(enc, "cw",  layers[i].encoder_cw);
        cJSON_AddStringToObject(enc, "sw",  layers[i].encoder_sw);
    }

    char *json = cJSON_Print(root);
    cJSON_Delete(root);

    if (!json)
        return -1;

    FILE *fp = fopen(current_filename, "w");
    if (!fp)
    {
        free(json);
        return -1;
    }

    fputs(json, fp);
    fclose(fp);
    free(json);

    return 0;
}
//GUI Initialization
static  char logbuf[64000];
static   int logbuf_updated = 0;
static float bg[3] = { 90, 95, 100 };

static void write_log(const char *text) {
  if (logbuf[0]) { strcat(logbuf, "\n"); }
  strcat(logbuf, text);
  logbuf_updated = 1;
}


static void test_window(mu_Context *ctx) {
  /* do window */
  if (mu_begin_window(ctx, "Demo Window", mu_rect(40, 40, 300, 450))) {
    mu_Container *win = mu_get_current_container(ctx);
    win->rect.w = mu_max(win->rect.w, 240);
    win->rect.h = mu_max(win->rect.h, 300);

    /* window info */
    if (mu_header(ctx, "Window Info")) {
      mu_Container *win = mu_get_current_container(ctx);
      char buf[64];
      mu_layout_row(ctx, 2, (int[]) { 54, -1 }, 0);
      mu_label(ctx,"Position:");
      sprintf(buf, "%d, %d", win->rect.x, win->rect.y); mu_label(ctx, buf);
      mu_label(ctx, "Size:");
      sprintf(buf, "%d, %d", win->rect.w, win->rect.h); mu_label(ctx, buf);
    }

    /* labels + buttons */
    if (mu_header_ex(ctx, "Test Buttons", MU_OPT_EXPANDED)) {
      mu_layout_row(ctx, 3, (int[]) { 86, -110, -1 }, 0);
      mu_label(ctx, "Test buttons 1:");
      if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
      if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
      mu_label(ctx, "Test buttons 2:");
      if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
      if (mu_button(ctx, "Popup")) { mu_open_popup(ctx, "Test Popup"); }
      if (mu_begin_popup(ctx, "Test Popup")) {
        mu_button(ctx, "Hello");
        mu_button(ctx, "World");
        mu_end_popup(ctx);
      }
    }

    /* tree */
    if (mu_header_ex(ctx, "Tree and Text", MU_OPT_EXPANDED)) {
      mu_layout_row(ctx, 2, (int[]) { 140, -1 }, 0);
      mu_layout_begin_column(ctx);
      if (mu_begin_treenode(ctx, "Test 1")) {
        if (mu_begin_treenode(ctx, "Test 1a")) {
          mu_label(ctx, "Hello");
          mu_label(ctx, "world");
          mu_end_treenode(ctx);
        }
        if (mu_begin_treenode(ctx, "Test 1b")) {
          if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
          if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
          mu_end_treenode(ctx);
        }
        mu_end_treenode(ctx);
      }
      if (mu_begin_treenode(ctx, "Test 2")) {
        mu_layout_row(ctx, 2, (int[]) { 54, 54 }, 0);
        if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
        if (mu_button(ctx, "Button 4")) { write_log("Pressed button 4"); }
        if (mu_button(ctx, "Button 5")) { write_log("Pressed button 5"); }
        if (mu_button(ctx, "Button 6")) { write_log("Pressed button 6"); }
        mu_end_treenode(ctx);
      }
      if (mu_begin_treenode(ctx, "Test 3")) {
        static int checks[3] = { 1, 0, 1 };
        mu_checkbox(ctx, "Checkbox 1", &checks[0]);
        mu_checkbox(ctx, "Checkbox 2", &checks[1]);
        mu_checkbox(ctx, "Checkbox 3", &checks[2]);
        mu_end_treenode(ctx);
      }
      mu_layout_end_column(ctx);

      mu_layout_begin_column(ctx);
      mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
      mu_text(ctx, "Lorem ipsum dolor sit amet, consectetur adipiscing "
        "elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
        "ipsum, eu varius magna felis a nulla.");
      mu_layout_end_column(ctx);
    }

    /* background color sliders */
    if (mu_header_ex(ctx, "Background Color", MU_OPT_EXPANDED)) {
      mu_layout_row(ctx, 2, (int[]) { -78, -1 }, 74);
      /* sliders */
      mu_layout_begin_column(ctx);
      mu_layout_row(ctx, 2, (int[]) { 46, -1 }, 0);
      mu_label(ctx, "Red:");   mu_slider(ctx, &bg[0], 0, 255);
      mu_label(ctx, "Green:"); mu_slider(ctx, &bg[1], 0, 255);
      mu_label(ctx, "Blue:");  mu_slider(ctx, &bg[2], 0, 255);
      mu_layout_end_column(ctx);
      /* color preview */
      mu_Rect r = mu_layout_next(ctx);
      mu_draw_rect(ctx, r, mu_color(bg[0], bg[1], bg[2], 255));
      char buf[32];
      sprintf(buf, "#%02X%02X%02X", (int) bg[0], (int) bg[1], (int) bg[2]);
      mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
    }

    mu_end_window(ctx);
  }
}


static void log_window(mu_Context *ctx) {
  if (mu_begin_window(ctx, "Log Window", mu_rect(350, 40, 300, 200))) {
    /* output text panel */
    mu_layout_row(ctx, 1, (int[]) { -1 }, -25);
    mu_begin_panel(ctx, "Log Output");
    mu_Container *panel = mu_get_current_container(ctx);
    mu_layout_row(ctx, 1, (int[]) { -1 }, -1);
    mu_text(ctx, logbuf);
    mu_end_panel(ctx);
    if (logbuf_updated) {
      panel->scroll.y = panel->content_size.y;
      logbuf_updated = 0;
    }

    /* input textbox + submit button */
    static char buf[128];
    int submitted = 0;
    mu_layout_row(ctx, 2, (int[]) { -70, -1 }, 0);
    if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
      mu_set_focus(ctx, ctx->last_id);
      submitted = 1;
    }
    if (mu_button(ctx, "Submit")) { submitted = 1; }
    if (submitted) {
      write_log(buf);
      buf[0] = '\0';
    }

    mu_end_window(ctx);
  }
}


static int uint8_slider(mu_Context *ctx, unsigned char *value, int low, int high) {
  static float tmp;
  mu_push_id(ctx, &value, sizeof(value));
  tmp = *value;
  int res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
  *value = tmp;
  mu_pop_id(ctx);
  return res;
}


static void style_window(mu_Context *ctx) {
  static struct { const char *label; int idx; } colors[] = {
    { "text:",         MU_COLOR_TEXT        },
    { "border:",       MU_COLOR_BORDER      },
    { "windowbg:",     MU_COLOR_WINDOWBG    },
    { "titlebg:",      MU_COLOR_TITLEBG     },
    { "titletext:",    MU_COLOR_TITLETEXT   },
    { "panelbg:",      MU_COLOR_PANELBG     },
    { "button:",       MU_COLOR_BUTTON      },
    { "buttonhover:",  MU_COLOR_BUTTONHOVER },
    { "buttonfocus:",  MU_COLOR_BUTTONFOCUS },
    { "base:",         MU_COLOR_BASE        },
    { "basehover:",    MU_COLOR_BASEHOVER   },
    { "basefocus:",    MU_COLOR_BASEFOCUS   },
    { "scrollbase:",   MU_COLOR_SCROLLBASE  },
    { "scrollthumb:",  MU_COLOR_SCROLLTHUMB },
    { NULL }
  };

  if (mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 240))) {
    int sw = mu_get_current_container(ctx)->body.w * 0.14;
    mu_layout_row(ctx, 6, (int[]) { 80, sw, sw, sw, sw, -1 }, 0);
    for (int i = 0; colors[i].label; i++) {
      mu_label(ctx, colors[i].label);
      uint8_slider(ctx, &ctx->style->colors[i].r, 0, 255);
      uint8_slider(ctx, &ctx->style->colors[i].g, 0, 255);
      uint8_slider(ctx, &ctx->style->colors[i].b, 0, 255);
      uint8_slider(ctx, &ctx->style->colors[i].a, 0, 255);
      mu_draw_rect(ctx, mu_layout_next(ctx), ctx->style->colors[i]);
    }
    mu_end_window(ctx);
  }
}

static void macropad_window(mu_Context *ctx) {
  /* Persistent UI state for 4 layers */
  static int max_layers = 100;
  static int current_layer = 0;

  /* Open window with a reasonable default size for configuration */
  if (mu_begin_window(ctx, "Macropad Configurator", mu_rect(40, 40, 680, 480))) {
    
    /* Master layout split: Left column (200), Right column (fills remaining space) */
    mu_layout_row(ctx, 2, (int[]) { 200, -1 }, -1);

    /* ==========================================
       LEFT COLUMN: Layer Selection Stack
       ========================================== */
    mu_begin_panel(ctx, "layers");
    //mu_layout_begin_column(ctx);
    mu_layout_row(ctx, 2, (int[]){70,-1}, 0);
    mu_label(ctx, "Layer No.");
    mu_label(ctx, "Name");
    for (int i=0;i<num_layers;i++) {
      char layer_indicator[32];
      sprintf(layer_indicator, "Layer %d", i);
      if (mu_button(ctx,layer_indicator))  { current_layer = i; }
      mu_label(ctx,layers[i].name);
    }
    if (mu_button(ctx,"Add Layer"))  { num_layers++; }
    if (mu_button(ctx,"Delete Layer"))  { num_layers--; }
    //mu_layout_end_column(ctx);

    mu_end_panel(ctx);

    /* ==========================================
       RIGHT COLUMN: Key & Encoder Configuration
       ========================================== */
    mu_layout_begin_column(ctx);

    /* Row 1: Current Layer Header Name */
    char layer_title[32];
    sprintf(layer_title, "Layer %d Configuration", current_layer);
    mu_header_ex(ctx, layer_title, MU_OPT_EXPANDED);

    mu_layout_row(ctx, 2, (int[]) { 60, -1 }, 0);
    mu_label(ctx, "Layer Name");
    mu_textbox(ctx, layers[current_layer].name, sizeof(layers[current_layer].name));


    /* Sub-header labels for the key grid */
    mu_layout_row(ctx, 3, (int[]) { 60, 150, -1 }, 0);
    mu_label(ctx, "Target");
    mu_label(ctx, "Bind Name");
    mu_label(ctx, "Key Mapping / Macro");

    /* 9 Rows for individual macro keys */
    for (int i = 0; i < 9; i++) {
      mu_layout_row(ctx, 3, (int[]) { 60, 150, -1 }, 0);
      
      /* Column 1: Key Label */
      char key_lbl[16];
      sprintf(key_lbl, "Key %d:", i + 1);
      mu_label(ctx, key_lbl);
      
      /* Column 2: Bind Name Textbox */
      mu_textbox(ctx, layers[current_layer].key_names[i], sizeof(layers[current_layer].key_names[i]));
      
      /* Column 3: Bind Keys Textbox */
      mu_textbox(ctx, layers[current_layer].key_binds[i], sizeof(layers[current_layer].key_binds[i]));
    }

    /* Rotary Encoder Section Separator */
    mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
    mu_label(ctx, "--- Rotary Encoder Configuration ---");

    /* Encoder Press Bind row */
    mu_layout_row(ctx, 2, (int[]) { 130, -1 }, 0);
    mu_label(ctx, "Encoder Press:");
    mu_textbox(ctx, layers[current_layer].encoder_sw, sizeof(layers[current_layer].encoder_sw));

    /* Encoder Rotate Bind row */
    mu_layout_row(ctx, 3, (int[]) { 130, 150,-1 }, 0);
    mu_label(ctx, "Encoder Rotate:");
    mu_textbox(ctx, layers[current_layer].encoder_ccw, sizeof(layers[current_layer].encoder_ccw));
    mu_textbox(ctx, layers[current_layer].encoder_cw, sizeof(layers[current_layer].encoder_cw));

    /* Bottom Action: Flash Button */
    mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
    if (mu_button(ctx, "Flash to Macropad Hardware")) {
      /* Hardware flashing logic hooks here later */
      write_layout_json();
      running=0;
    }


    mu_layout_end_column(ctx);
    mu_end_window(ctx);
  }
}

static void process_frame(mu_Context *ctx) {
  mu_begin(ctx);
  style_window(ctx);
  log_window(ctx);
  test_window(ctx);
  macropad_window(ctx);
  mu_end(ctx);
}



static const char button_map[256] = {
  [ SDL_BUTTON_LEFT   & 0xff ] =  MU_MOUSE_LEFT,
  [ SDL_BUTTON_RIGHT  & 0xff ] =  MU_MOUSE_RIGHT,
  [ SDL_BUTTON_MIDDLE & 0xff ] =  MU_MOUSE_MIDDLE,
};

static const char key_map[256] = {
  [ SDLK_LSHIFT       & 0xff ] = MU_KEY_SHIFT,
  [ SDLK_RSHIFT       & 0xff ] = MU_KEY_SHIFT,
  [ SDLK_LCTRL        & 0xff ] = MU_KEY_CTRL,
  [ SDLK_RCTRL        & 0xff ] = MU_KEY_CTRL,
  [ SDLK_LALT         & 0xff ] = MU_KEY_ALT,
  [ SDLK_RALT         & 0xff ] = MU_KEY_ALT,
  [ SDLK_RETURN       & 0xff ] = MU_KEY_RETURN,
  [ SDLK_BACKSPACE    & 0xff ] = MU_KEY_BACKSPACE,
};


static int text_width(mu_Font font, const char *text, int len) {
  if (len == -1) { len = strlen(text); }
  return r_get_text_width(text, len);
}

static int text_height(mu_Font font) {
  return r_get_text_height();
}


void call_gui(char * filename) {
  current_filename=filename;
  parse_layout_json();
  /* init SDL and renderer */
  SDL_Init(SDL_INIT_EVERYTHING);
  r_init();

  /* init microui */
  mu_Context *ctx = malloc(sizeof(mu_Context));
  mu_init(ctx);
  memcpy(ctx->style->colors, theme_tokyonight, sizeof(theme_tokyonight));
  ctx->text_width = text_width;
  ctx->text_height = text_height;

  /* main loop */
  for (;;) {
    /* handle SDL events */
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT: exit(EXIT_SUCCESS); break;
        case SDL_MOUSEMOTION: mu_input_mousemove(ctx, e.motion.x, e.motion.y); break;
        case SDL_MOUSEWHEEL: mu_input_scroll(ctx, 0, e.wheel.y * -30); break;
        case SDL_TEXTINPUT: mu_input_text(ctx, e.text.text); break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
          int b = button_map[e.button.button & 0xff];
          if (b && e.type == SDL_MOUSEBUTTONDOWN) { mu_input_mousedown(ctx, e.button.x, e.button.y, b); }
          if (b && e.type ==   SDL_MOUSEBUTTONUP) { mu_input_mouseup(ctx, e.button.x, e.button.y, b);   }
          break;
        }

        case SDL_KEYDOWN:
        case SDL_KEYUP: {
          int c = key_map[e.key.keysym.sym & 0xff];
          if (c && e.type == SDL_KEYDOWN) { mu_input_keydown(ctx, c); }
          if (c && e.type ==   SDL_KEYUP) { mu_input_keyup(ctx, c);   }
          break;
        }
      }
    }

    /* process frame */
    process_frame(ctx);

    /* render */
    r_clear(mu_color(bg[0], bg[1], bg[2], 255));
    mu_Command *cmd = NULL;
    while (mu_next_command(ctx, &cmd)) {
      switch (cmd->type) {
        case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
        case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
        case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
        case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
      }
    }
    r_present();
    if(running==0) break;
  }

}
