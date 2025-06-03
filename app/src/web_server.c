#include "web_server.h"
#include "input_manager.h"
#include "control_msg.h"
#include "util/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
#include "mongoose.h"
#include "util/intmap.h"

#define API_PREFIX "/api/v1"

struct sc_web_server web_server;

void 
send_keycode(struct sc_input_manager *im, enum android_keycode keycode, 
             enum sc_action action, const char *name);
void 
action_home(struct sc_input_manager *im, enum sc_action action);
void
action_back(struct sc_input_manager *im, enum sc_action action);
void
action_app_switch(struct sc_input_manager *im, enum sc_action action);
void
action_power(struct sc_input_manager *im, enum sc_action action);
void
action_volume_up(struct sc_input_manager *im, enum sc_action action);
void
action_volume_down(struct sc_input_manager *im, enum sc_action action);
void
action_menu(struct sc_input_manager *im, enum sc_action action);
void
press_back_or_turn_screen_on(struct sc_input_manager *im,
                             enum sc_action action);
void
expand_notification_panel(struct sc_input_manager *im);
void
expand_settings_panel(struct sc_input_manager *im);
void
collapse_panels(struct sc_input_manager *im);
bool
get_device_clipboard(struct sc_input_manager *im, enum sc_copy_key copy_key);
void
clipboard_paste(struct sc_input_manager *im);
void
set_display_power(struct sc_input_manager *im, bool on);
void
rotate_device(struct sc_input_manager *im);
void
open_hard_keyboard_settings(struct sc_input_manager *im);
void
sc_input_manager_process_text_input(struct sc_input_manager *im,
                                    const SDL_TextInputEvent *event);
bool
simulate_virtual_finger(struct sc_input_manager *im,
                        enum android_motionevent_action action,
                        struct sc_point point);

enum android_keycode
convert_keycode2(enum sc_keycode from) {
    static const struct sc_intmap_entry keys[] = {
        {SC_KEYCODE_RETURN,    AKEYCODE_ENTER},
        {SC_KEYCODE_KP_ENTER,  AKEYCODE_NUMPAD_ENTER},
        {SC_KEYCODE_ESCAPE,    AKEYCODE_ESCAPE},
        {SC_KEYCODE_BACKSPACE, AKEYCODE_DEL},
        {SC_KEYCODE_TAB,       AKEYCODE_TAB},
        {SC_KEYCODE_PAGEUP,    AKEYCODE_PAGE_UP},
        {SC_KEYCODE_DELETE,    AKEYCODE_FORWARD_DEL},
        {SC_KEYCODE_HOME,      AKEYCODE_MOVE_HOME},
        {SC_KEYCODE_END,       AKEYCODE_MOVE_END},
        {SC_KEYCODE_PAGEDOWN,  AKEYCODE_PAGE_DOWN},
        {SC_KEYCODE_RIGHT,     AKEYCODE_DPAD_RIGHT},
        {SC_KEYCODE_LEFT,      AKEYCODE_DPAD_LEFT},
        {SC_KEYCODE_DOWN,      AKEYCODE_DPAD_DOWN},
        {SC_KEYCODE_UP,        AKEYCODE_DPAD_UP},
        {SC_KEYCODE_LCTRL,     AKEYCODE_CTRL_LEFT},
        {SC_KEYCODE_RCTRL,     AKEYCODE_CTRL_RIGHT},
        {SC_KEYCODE_LSHIFT,    AKEYCODE_SHIFT_LEFT},
        {SC_KEYCODE_RSHIFT,    AKEYCODE_SHIFT_RIGHT},
        {SC_KEYCODE_LALT,      AKEYCODE_ALT_LEFT},
        {SC_KEYCODE_RALT,      AKEYCODE_ALT_RIGHT},
        {SC_KEYCODE_LGUI,      AKEYCODE_META_LEFT},
        {SC_KEYCODE_RGUI,      AKEYCODE_META_RIGHT},
        
        {SC_KEYCODE_KP_0,      AKEYCODE_INSERT},
        {SC_KEYCODE_KP_1,      AKEYCODE_MOVE_END},
        {SC_KEYCODE_KP_2,      AKEYCODE_DPAD_DOWN},
        {SC_KEYCODE_KP_3,      AKEYCODE_PAGE_DOWN},
        {SC_KEYCODE_KP_4,      AKEYCODE_DPAD_LEFT},
        {SC_KEYCODE_KP_6,      AKEYCODE_DPAD_RIGHT},
        {SC_KEYCODE_KP_7,      AKEYCODE_MOVE_HOME},
        {SC_KEYCODE_KP_8,      AKEYCODE_DPAD_UP},
        {SC_KEYCODE_KP_9,      AKEYCODE_PAGE_UP},
        {SC_KEYCODE_KP_PERIOD, AKEYCODE_FORWARD_DEL},
        
        {SC_KEYCODE_a,         AKEYCODE_A},
        {SC_KEYCODE_b,         AKEYCODE_B},
        {SC_KEYCODE_c,         AKEYCODE_C},
        {SC_KEYCODE_d,         AKEYCODE_D},
        {SC_KEYCODE_e,         AKEYCODE_E},
        {SC_KEYCODE_f,         AKEYCODE_F},
        {SC_KEYCODE_g,         AKEYCODE_G},
        {SC_KEYCODE_h,         AKEYCODE_H},
        {SC_KEYCODE_i,         AKEYCODE_I},
        {SC_KEYCODE_j,         AKEYCODE_J},
        {SC_KEYCODE_k,         AKEYCODE_K},
        {SC_KEYCODE_l,         AKEYCODE_L},
        {SC_KEYCODE_m,         AKEYCODE_M},
        {SC_KEYCODE_n,         AKEYCODE_N},
        {SC_KEYCODE_o,         AKEYCODE_O},
        {SC_KEYCODE_p,         AKEYCODE_P},
        {SC_KEYCODE_q,         AKEYCODE_Q},
        {SC_KEYCODE_r,         AKEYCODE_R},
        {SC_KEYCODE_s,         AKEYCODE_S},
        {SC_KEYCODE_t,         AKEYCODE_T},
        {SC_KEYCODE_u,         AKEYCODE_U},
        {SC_KEYCODE_v,         AKEYCODE_V},
        {SC_KEYCODE_w,         AKEYCODE_W},
        {SC_KEYCODE_x,         AKEYCODE_X},
        {SC_KEYCODE_y,         AKEYCODE_Y},
        {SC_KEYCODE_z,         AKEYCODE_Z},
        {SC_KEYCODE_SPACE,     AKEYCODE_SPACE},

        {SC_KEYCODE_HASH,          AKEYCODE_POUND},
        {SC_KEYCODE_PERCENT,       AKEYCODE_PERIOD},
        {SC_KEYCODE_QUOTE,         AKEYCODE_APOSTROPHE},
        {SC_KEYCODE_ASTERISK,      AKEYCODE_STAR},
        {SC_KEYCODE_PLUS,          AKEYCODE_PLUS},
        {SC_KEYCODE_COMMA,         AKEYCODE_COMMA},
        {SC_KEYCODE_MINUS,         AKEYCODE_MINUS},
        {SC_KEYCODE_PERIOD,        AKEYCODE_PERIOD},
        {SC_KEYCODE_SLASH,         AKEYCODE_SLASH},
        {SC_KEYCODE_0,             AKEYCODE_0},
        {SC_KEYCODE_1,             AKEYCODE_1},
        {SC_KEYCODE_2,             AKEYCODE_2},
        {SC_KEYCODE_3,             AKEYCODE_3},
        {SC_KEYCODE_4,             AKEYCODE_4},
        {SC_KEYCODE_5,             AKEYCODE_5},
        {SC_KEYCODE_6,             AKEYCODE_6},
        {SC_KEYCODE_7,             AKEYCODE_7},
        {SC_KEYCODE_8,             AKEYCODE_8},
        {SC_KEYCODE_9,             AKEYCODE_9},
        {SC_KEYCODE_SEMICOLON,     AKEYCODE_SEMICOLON},
        {SC_KEYCODE_EQUALS,        AKEYCODE_EQUALS},
        {SC_KEYCODE_AT,            AKEYCODE_AT},
        {SC_KEYCODE_LEFTBRACKET,   AKEYCODE_LEFT_BRACKET},
        {SC_KEYCODE_BACKSLASH,     AKEYCODE_BACKSLASH},
        {SC_KEYCODE_RIGHTBRACKET,  AKEYCODE_RIGHT_BRACKET},
        {SC_KEYCODE_BACKQUOTE,     AKEYCODE_GRAVE},
        {SC_KEYCODE_KP_1,          AKEYCODE_NUMPAD_1},
        {SC_KEYCODE_KP_2,          AKEYCODE_NUMPAD_2},
        {SC_KEYCODE_KP_3,          AKEYCODE_NUMPAD_3},
        {SC_KEYCODE_KP_4,          AKEYCODE_NUMPAD_4},
        {SC_KEYCODE_KP_5,          AKEYCODE_NUMPAD_5},
        {SC_KEYCODE_KP_6,          AKEYCODE_NUMPAD_6},
        {SC_KEYCODE_KP_7,          AKEYCODE_NUMPAD_7},
        {SC_KEYCODE_KP_8,          AKEYCODE_NUMPAD_8},
        {SC_KEYCODE_KP_9,          AKEYCODE_NUMPAD_9},
        {SC_KEYCODE_KP_0,          AKEYCODE_NUMPAD_0},
        {SC_KEYCODE_KP_DIVIDE,     AKEYCODE_NUMPAD_DIVIDE},
        {SC_KEYCODE_KP_MULTIPLY,   AKEYCODE_NUMPAD_MULTIPLY},
        {SC_KEYCODE_KP_MINUS,      AKEYCODE_NUMPAD_SUBTRACT},
        {SC_KEYCODE_KP_PLUS,       AKEYCODE_NUMPAD_ADD},
        {SC_KEYCODE_KP_PERIOD,     AKEYCODE_NUMPAD_DOT},
        {SC_KEYCODE_KP_EQUALS,     AKEYCODE_NUMPAD_EQUALS},
        {SC_KEYCODE_KP_LEFTPAREN,  AKEYCODE_NUMPAD_LEFT_PAREN},
        {SC_KEYCODE_KP_RIGHTPAREN, AKEYCODE_NUMPAD_RIGHT_PAREN},
    };

    const struct sc_intmap_entry *entry =
        SC_INTMAP_FIND_ENTRY(keys, from);
    if (entry) {
        return entry->value;
    }

    return AKEYCODE_UNKNOWN;
}

static const char *mgx_http_status_code_str(int status_code) {
  switch (status_code) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 102: return "Processing";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-authoritative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";
    case 207: return "Multi-Status";
    case 208: return "Already Reported";
    case 226: return "IM Used";
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 305: return "Use Proxy";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Payload Too Large";
    case 414: return "Request-URI Too Long";
    case 415: return "Unsupported Media Type";
    case 416: return "Requested Range Not Satisfiable";
    case 417: return "Expectation Failed";
    case 418: return "I'm a teapot";
    case 421: return "Misdirected Request";
    case 422: return "Unprocessable Entity";
    case 423: return "Locked";
    case 424: return "Failed Dependency";
    case 426: return "Upgrade Required";
    case 428: return "Precondition Required";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    case 444: return "Connection Closed Without Response";
    case 451: return "Unavailable For Legal Reasons";
    case 499: return "Client Closed Request";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    case 505: return "HTTP Version Not Supported";
    case 506: return "Variant Also Negotiates";
    case 507: return "Insufficient Storage";
    case 508: return "Loop Detected";
    case 510: return "Not Extended";
    case 511: return "Network Authentication Required";
    case 599: return "Network Connect Timeout Error";
    default: return "";
  }
}                        

// Helper function to send JSON response
static void send_json_response(struct mg_connection *nc, int status_code, const char *json) {
    mg_printf(nc, "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\n\r\n%s\r\n",
              status_code, mgx_http_status_code_str(status_code), strlen(json), json);
    nc->is_draining = 1;
}

// Helper function to send error response
static void send_error_response(struct mg_connection *nc, int status_code, const char *message) {
    char json[256];
    snprintf(json, sizeof(json), "{\"error\": \"%s\"}", message);
    send_json_response(nc, status_code, json);
}

// Convert AVFrame to RGB and write to BMP/PNG/JPG
static bool write_frame_to_memory(const AVFrame *frame, const char *format, 
                                uint8_t **out_buffer, size_t *out_size) {
    struct SwsContext *sws_ctx = sws_getContext(
        frame->width, frame->height, frame->format,
        frame->width, frame->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL);
    
    if (!sws_ctx) {
        LOGE("Could not create sws context");
        return false;
    }

    // Allocate RGB frame
    AVFrame *rgb_frame = av_frame_alloc();
    if (!rgb_frame) {
        sws_freeContext(sws_ctx);
        return false;
    }

    rgb_frame->width = frame->width;
    rgb_frame->height = frame->height;
    rgb_frame->format = AV_PIX_FMT_RGB24;
    
    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, 
        frame->width, frame->height, 1);
    uint8_t *rgb_buffer = av_malloc(buffer_size);
    
    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buffer,
        AV_PIX_FMT_RGB24, frame->width, frame->height, 1);
    
    // Convert to RGB
    sws_scale(sws_ctx, (const uint8_t * const*)frame->data, frame->linesize,
        0, frame->height, rgb_frame->data, rgb_frame->linesize);

    // Create SDL surface from RGB data
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(rgb_buffer,
        frame->width, frame->height, 24, frame->width * 3,
        0x0000FF, 0x00FF00, 0xFF0000, 0);
    
    if (!surface) {
        LOGE("Could not create SDL surface: %s", SDL_GetError());
        goto error;
    }

    // Create RWops for memory output
    size_t rw_buffer_size;
    if (strcmp(format, "bmp") == 0) {
        rw_buffer_size = surface->h * surface->pitch + 54; // BMP header size + pixel data
    } else if (strcmp(format, "png") == 0 || strcmp(format, "jpg") == 0) {
        rw_buffer_size = surface->h * surface->pitch; // Rough estimate, actual size may vary
    }
    uint8_t *rw_buffer = malloc(rw_buffer_size);
    SDL_RWops *rw = SDL_RWFromMem(rw_buffer, rw_buffer_size);
    if (!rw) {
        LOGE("Could not create SDL_RWops: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        goto error;
    }
    
    // Save to memory in requested format
    bool success;
    if (strcmp(format, "bmp") == 0) {
        success = SDL_SaveBMP_RW(surface, rw, 0) == 0;
    // TODO: Uncomment and implement PNG/JPG saving when available
    // } else if (strcmp(format, "png") == 0) {
    //     success = IMG_SavePNG_RW(surface, rw) == 0;
    // } else if (strcmp(format, "jpg") == 0 || strcmp(format, "jpeg") == 0) {
    //     success = IMG_SaveJPG_RW(surface, rw, 90) == 0;
    } else {
        success = false;
    }

    if (success) {
        *out_size = SDL_RWsize(rw);
        *out_buffer = malloc(*out_size);
        memcpy(*out_buffer, rw_buffer, *out_size);

        // For unknown reasons, SDL_RWread does not return the data
        // SDL_RWread(rw, *out_buffer, *out_size, 1);
    }

    SDL_RWclose(rw);
    free(rw_buffer);
    SDL_FreeSurface(surface);
    av_free(rgb_buffer);
    av_frame_free(&rgb_frame);
    sws_freeContext(sws_ctx);
    return success;
    
error:
    av_free(rgb_buffer);
    av_frame_free(&rgb_frame);
    sws_freeContext(sws_ctx);
    return false;
}

// Route handler for /api/v1/frame
static void handle_frame(struct mg_connection *nc, struct mg_http_message *hm, struct sc_web_server *server) {
    if (!server->current_frame) {
        send_error_response(nc, 503, "No frame available");
        return;
    }

    char format[8] = "bmp";  // default format
    struct mg_str *format_header = mg_http_get_header(hm, "Accept");
    if (format_header) {
        if (mg_strstr(*format_header, mg_str("image/jpeg"))) {
            strcpy(format, "jpg");
        } else if (mg_strstr(*format_header, mg_str("image/bmp"))) {
            strcpy(format, "bmp");
        }
    }

    uint8_t *buffer;
    size_t size;
    if (!write_frame_to_memory(server->current_frame, format, &buffer, &size)) {
        send_error_response(nc, 500, "Could not convert frame");
        return;
    }
    const char *content_type = strcmp(format, "jpg") == 0 ? "image/jpeg" :
                              strcmp(format, "bmp") == 0 ? "image/bmp" :
                              "image/png";

    LOGI("Frame size: %u", size);

    mg_printf(nc, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n\r\n",
              200, mgx_http_status_code_str(200), content_type, size);
    mg_send(nc, buffer, size);
    mg_send(nc, "\r\n", 2);
    nc->is_draining = 1;
    
    free(buffer);
}

// Route handler for /api/v1/keycode
static void handle_keycode(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling keycode request");
    char keycode[32], action[32];
    
    mg_http_get_var(&hm->body, "keycode", keycode, sizeof(keycode));
    mg_http_get_var(&hm->body, "action", action, sizeof(action));
    
    enum sc_keycode sc_keycode = atoi(keycode);
    enum android_keycode a_keycode = convert_keycode2(sc_keycode);
    LOGI("Keycode: %s, Keycode sc enum: %d, Keycode android enum: %d, Action: %s", keycode, sc_keycode, a_keycode, action);
    enum sc_action act = strcmp(action, "up") == 0 ? SC_ACTION_UP : SC_ACTION_DOWN;
    
    send_keycode(im, a_keycode, act, "KEY");
    send_json_response(nc, 200, "{\"status\": \"success\"}");
}

static void handle_text_input(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling text input request");
    char text[256];
    mg_http_get_var(&hm->body, "text", text, sizeof(text));
    
    if (strlen(text) > 0) {
        // Send the text input to the device
        SDL_TextInputEvent event;
        event.type = SDL_TEXTINPUT;
        event.timestamp = SDL_GetTicks();
        strncpy(event.text, text, min(sizeof(event.text) - 1, 32)); // Ensure we don't overflow
        event.text[sizeof(event.text) - 1] = '\0'; // Ensure null-termination

        sc_input_manager_process_text_input(im, &event);
        send_json_response(nc, 200, "{\"status\": \"success\"}");
    } else {
        send_error_response(nc, 400, "Text input cannot be empty");
    }
}

// Route handler for /api/v1/home
static void handle_home(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling home request");
    char action[32];
    mg_http_get_var(&hm->body, "action", action, sizeof(action));
    enum sc_action act = strcmp(action, "up") == 0 ? SC_ACTION_UP : SC_ACTION_DOWN;
    
    action_home(im, act);
    send_json_response(nc, 200, "{\"status\": \"success\"}");
}

// Route handler for /api/v1/back
static void handle_back(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling back request");
    char action[32];
    mg_http_get_var(&hm->body, "action", action, sizeof(action));
    enum sc_action act = strcmp(action, "up") == 0 ? SC_ACTION_UP : SC_ACTION_DOWN;
    
    action_back(im, act);
    send_json_response(nc, 200, "{\"status\": \"success\"}");
}

// Route handler for /api/v1/app_switch
static void handle_app_switch(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling app switch request");
    char action[32];
    mg_http_get_var(&hm->body, "action", action, sizeof(action));
    enum sc_action act = strcmp(action, "up") == 0 ? SC_ACTION_UP : SC_ACTION_DOWN;
    
    action_app_switch(im, act);
    send_json_response(nc, 200, "{\"status\": \"success\"}");
}

// Route handler for /api/v1/power
static void handle_power(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling power request");
    char action[32];
    mg_http_get_var(&hm->body, "action", action, sizeof(action));
    enum sc_action act = strcmp(action, "up") == 0 ? SC_ACTION_UP : SC_ACTION_DOWN;
    
    action_power(im, act);
    send_json_response(nc, 200, "{\"status\": \"success\"}");
}

// Route handler for /api/v1/volume
static void handle_volume(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling volume request");
    char direction[32], action[32];
    mg_http_get_var(&hm->body, "direction", direction, sizeof(direction));
    mg_http_get_var(&hm->body, "action", action, sizeof(action));
    
    enum sc_action act = strcmp(action, "up") == 0 ? SC_ACTION_UP : SC_ACTION_DOWN;
    
    if (strcmp(direction, "up") == 0) {
        action_volume_up(im, act);
    } else {
        action_volume_down(im, act);
    }
    send_json_response(nc, 200, "{\"status\": \"success\"}");
}

// Route handler for /api/v1/menu
static void handle_menu(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling menu request");
    char action[32];
    mg_http_get_var(&hm->body, "action", action, sizeof(action));
    enum sc_action act = strcmp(action, "up") == 0 ? SC_ACTION_UP : SC_ACTION_DOWN;
    
    action_menu(im, act);
    send_json_response(nc, 200, "{\"status\": \"success\"}");
}

// Route handler for /api/v1/back_or_screen_on
static void handle_back_or_screen_on(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling back or screen on request");
    char action[32];
    mg_http_get_var(&hm->body, "action", action, sizeof(action));
    enum sc_action act = strcmp(action, "up") == 0 ? SC_ACTION_UP : SC_ACTION_DOWN;
    
    press_back_or_turn_screen_on(im, act);
    send_json_response(nc, 200, "{\"status\": \"success\"}");
}

// Route handler for various panel actions
static void handle_panel_action(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling panel action request");
    char action[32];
    mg_http_get_var(&hm->body, "action", action, sizeof(action));    

    if (strcmp(action, "expand_notification") == 0) {
        expand_notification_panel(im);
    } else if (strcmp(action, "expand_settings") == 0) {
        expand_settings_panel(im);
    } else if (strcmp(action, "collapse") == 0) {
        collapse_panels(im);
    }
    send_json_response(nc, 200, "{\"status\": \"success\"}");
}

// Route handler for clipboard operations
static void handle_clipboard(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling clipboard request");
    if (mg_vcmp(&hm->method, "GET")) {
        get_device_clipboard(im, SC_COPY_KEY_COPY);
        send_json_response(nc, 200, "{\"status\": \"success\", \"message\": \"Clipboard request sent\"}");

    } else if (mg_vcmp(&hm->method, "PUT")) {
        clipboard_paste(im);
        send_json_response(nc, 200, "{\"status\": \"success\", \"message\": \"Paste request sent\"}");
    }
}

// Route handler for display power
static void handle_display_power(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling display power request");
    char state[32];
    mg_http_get_var(&hm->body, "state", state, sizeof(state));
    bool power_on = strcmp(state, "on") == 0;
    
    set_display_power(im, power_on);
    send_json_response(nc, 200, "{\"status\": \"success\"}");
}

// Route handler for device rotation
static void handle_rotate_device(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling rotate device request");
    rotate_device(im);
    send_json_response(nc, 200, "{\"status\": \"success\"}");
}

// Route handler for keyboard settings
static void handle_keyboard_settings(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling keyboard settings request");
    open_hard_keyboard_settings(im);
    send_json_response(nc, 200, "{\"status\": \"success\"}");
}

// Route handler for virtual finger simulation
static void handle_virtual_finger(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling virtual finger request");
    char action[32], x[32], y[32];

    mg_http_get_var(&hm->body, "action", action, sizeof(action));
    mg_http_get_var(&hm->body, "x", x, sizeof(x));
    mg_http_get_var(&hm->body, "y", y, sizeof(y));
    LOGI("Virtual finger action: %s, x: %s, y: %s", action, x, y);
    
    enum android_motionevent_action act;
    if (strcmp(action, "down") == 0) {
        act = AMOTION_EVENT_ACTION_DOWN;
    } else if (strcmp(action, "up") == 0) {
        act = AMOTION_EVENT_ACTION_UP;
    } else if (strcmp(action, "move") == 0) {
        act = AMOTION_EVENT_ACTION_MOVE;
    } else {
        send_error_response(nc, 400, "Invalid action. Must be 'down', 'up', or 'move'");
        return;
    }
    
    if (!x[0] || !y[0]) {
        send_error_response(nc, 400, "x and y coordinates are required");
        return;
    }
    
    struct sc_point point = {
        .x = atoi(x),
        .y = atoi(y)
    };
    
    if (simulate_virtual_finger(im, act, point)) {
        send_json_response(nc, 200, "{\"status\": \"success\"}");
    } else {
        send_error_response(nc, 500, "Failed to simulate virtual finger");
    }
}

// Main event handler for all HTTP requests
static void ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
    struct sc_web_server *server = (struct sc_web_server *)user_data;
    
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        
        // Log the request URI
        char *uri = (char*)malloc(hm->uri.len + 1);
        strncpy(uri, hm->uri.ptr, hm->uri.len);
        uri[hm->uri.len] = '\0';
        LOGI("Received HTTP request: %s", uri);
        free(uri);
        
        // Define the routes
        struct {
            const char *uri;
            void (*handler)(struct mg_connection *, struct mg_http_message *, struct sc_input_manager *);
        } routes[] = {
            { API_PREFIX "/keycode", handle_keycode},
            { API_PREFIX "/text", handle_text_input},
            { API_PREFIX "/home", handle_home},
            { API_PREFIX "/back", handle_back},
            { API_PREFIX "/app_switch", handle_app_switch},
            { API_PREFIX "/power", handle_power},
            { API_PREFIX "/volume", handle_volume},
            { API_PREFIX "/menu", handle_menu},
            { API_PREFIX "/back_or_screen_on", handle_back_or_screen_on},
            { API_PREFIX "/panel", handle_panel_action},        
            { API_PREFIX "/virtual_finger", handle_virtual_finger}
        };
        const size_t num_routes = sizeof(routes) / sizeof(routes[0]);

        // Find and execute the appropriate handler
        for (size_t i = 0; i < num_routes; i++) {
            if (mg_vcmp(&hm->uri, routes[i].uri) == 0) {
                if (mg_vcmp(&hm->method, "POST") == 0) {
                    routes[i].handler(nc, hm, server->input_manager);
                    nc->is_draining = 1;  // Mark connection for closing after sending
                    return;
                }
                LOGE("Invalid method for %s: %s", routes[i].uri, hm->method.ptr);
                send_error_response(nc, 405, "Method not allowed");
                nc->is_draining = 1;  // Mark connection for closing after sending
                return;
            }
        }
        
        // Handle frame endpoints
        if (mg_vcmp(&hm->uri, API_PREFIX "/frame") == 0) {
            if (mg_vcmp(&hm->method, "GET") == 0) {
                handle_frame(nc, hm, server);
                nc->is_draining = 1;  // Mark connection for closing after sending
                return;
            }
            send_error_response(nc, 405, "Method not allowed");
            nc->is_draining = 1;  // Mark connection for closing after sending
            return;
        }
        
        // Handle other routes
        if (mg_vcmp(&hm->uri, API_PREFIX "/clipboard") == 0) {
            if (mg_vcmp(&hm->method, "GET") == 0 || mg_vcmp(&hm->method, "PUT") == 0) {
                handle_clipboard(nc, hm, server->input_manager);
                nc->is_draining = 1;  // Mark connection for closing after sending
                return;
            }
            send_error_response(nc, 405, "Method not allowed");
            nc->is_draining = 1;  // Mark connection for closing after sending
            return;
        }
        
        if (mg_vcmp(&hm->uri, API_PREFIX "/display/power") == 0) {
            handle_display_power(nc, hm, server->input_manager);
            nc->is_draining = 1;  // Mark connection for closing after sending
            return;
        }
        
        if (mg_vcmp(&hm->uri, API_PREFIX "/device/rotate") == 0) {
            handle_rotate_device(nc, hm, server->input_manager);
            nc->is_draining = 1;  // Mark connection for closing after sending
            return;
        }
        
        if (mg_vcmp(&hm->uri, API_PREFIX "/keyboard/settings") == 0) {
            handle_keyboard_settings(nc, hm, server->input_manager);
            nc->is_draining = 1;  // Mark connection for closing after sending
            return;
        }
        
        LOGE("No handler for %s", hm->uri.ptr);
        send_error_response(nc, 404, "Not found");
        nc->is_draining = 1;  // Mark connection for closing after sending

    } else if (ev == MG_EV_ERROR) {
        // Handle connection error
        LOGE("Connection error: %s", (char *)ev_data);
    }
}

void
sc_web_server_set_frame(struct sc_web_server *server, const AVFrame *frame) {
    if (server->current_frame) {
        av_frame_free(&server->current_frame);
    }
    
    if (frame) {
        server->current_frame = av_frame_alloc();
        av_frame_ref(server->current_frame, frame);
    } else {
        server->current_frame = NULL;
    }
}

bool sc_web_server_init(struct sc_web_server *server,
                        const char *listening_addr) {
    LOGI("Initializing web server...");
    if (!server || !listening_addr) {
        LOGE("Invalid parameters passed to web_server_init");
        return false;
    }
    server->listening_addr = listening_addr;
    server->running = false;
    server->mongoose_ctx = NULL;
    server->current_frame = NULL;
    
    LOGI("Web server initialized successfully");
    return true;
}

void sc_web_server_set_input_manager(struct sc_web_server *server,
                                      struct sc_input_manager *input_manager) {
    if (server) {
        server->input_manager = input_manager;
    }
}

int mongoose_poll_thread(void *arg) {
    struct sc_web_server *server = (struct sc_web_server *)arg;
    if (!server || server->mongoose_ctx) {
        LOGE("Invalid server state in poll thread");
        return -1;
    }

    struct mg_mgr *mgr = calloc(1, sizeof(struct mg_mgr));
    if (!mgr) {
        LOGE("Could not allocate mongoose manager: out of memory");
        return -2;
    }
    
    LOGI("Initializing mongoose manager");
    mg_mgr_init(mgr);

    LOGI("Creating HTTP listener on %s", server->listening_addr);
    struct mg_connection *nc = mg_http_listen(mgr, server->listening_addr, ev_handler, server);
    
    if (!nc) {
        LOGE("Could not bind to %s (mongoose error)", server->listening_addr);
        mg_mgr_free(mgr);
        free(mgr);
        return -3;
    }

    server->mongoose_ctx = mgr;
    server->running = true;
    
    LOGI("Starting mongoose poll loop");
    while (server->running) {
        mg_mgr_poll(mgr, 100);
    }
    
    LOGI("Mongoose poll loop ended");
    return 0;
}

bool sc_web_server_start(struct sc_web_server *server) {   
    if (!server) {
        LOGE("NULL server passed to start");
        return false;
    }

    LOGI("Starting web server thread");
    SDL_Thread *thread = SDL_CreateThread(mongoose_poll_thread, "web_server", server);
    if (!thread) {
        LOGE("Could not create web server thread: %s", SDL_GetError());
        server->running = false;
        mg_mgr_free(server->mongoose_ctx);
        free(server->mongoose_ctx);
        return false;
    }

    return true;
}

void sc_web_server_stop(struct sc_web_server *server) {
    LOGI("Stopping web server...");
    server->running = false;
}

void sc_web_server_destroy(struct sc_web_server *server) {
    LOGI("Destroying web server...");
    if (server->mongoose_ctx) {
        struct mg_mgr *mgr = (struct mg_mgr *)server->mongoose_ctx;
        mg_mgr_free(mgr);
        free(mgr);
        server->mongoose_ctx = NULL;
    }
}
