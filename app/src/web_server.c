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
bool
simulate_virtual_finger(struct sc_input_manager *im,
                        enum android_motionevent_action action,
                        struct sc_point point);

// Helper function to send JSON response
static void send_json_response(struct mg_connection *nc, int status_code, const char *json) {
    mg_printf(nc, "HTTP/1.1 %d OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
              status_code, strlen(json), json);
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
    SDL_RWops *rw = SDL_RWFromMem(NULL, 0);
    if (!rw) {
        SDL_FreeSurface(surface);
        goto error;
    }
    
    // Save to memory in requested format
    bool success;
    if (strcmp(format, "bmp") == 0) {
        success = SDL_SaveBMP_RW(surface, rw, 0) == 0;
    } else if (strcmp(format, "png") == 0) {
        success = IMG_SavePNG_RW(surface, rw) == 0;
    } else if (strcmp(format, "jpg") == 0 || strcmp(format, "jpeg") == 0) {
        success = IMG_SaveJPG_RW(surface, rw, 90) == 0;
    } else {
        success = false;
    }
    
    if (success) {
        *out_size = SDL_RWsize(rw);
        *out_buffer = malloc(*out_size);
        SDL_RWread(rw, *out_buffer, *out_size, 1);
    }
    
    SDL_RWclose(rw);
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
    LOGI("Handling frame request");
    if (!server->current_frame) {
        send_error_response(nc, 503, "No frame available");
        return;
    }
    
    char format[8] = "png";  // default format
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
    
    mg_printf(nc, "HTTP/1.1 200 OK\r\n"
                  "Content-Type: %s\r\n"
                  "Content-Length: %zu\r\n\r\n",
                  content_type, size);
    mg_send(nc, buffer, size);
    
    free(buffer);
}

// Route handler for /api/v1/keycode
static void handle_keycode(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im) {
    LOGI("Handling keycode request");
    char keycode[32], action[32];
    
    mg_http_get_var(&hm->body, "keycode", keycode, sizeof(keycode));
    mg_http_get_var(&hm->body, "action", action, sizeof(action));
    
    enum android_keycode key = atoi(keycode);
    enum sc_action act = strcmp(action, "up") == 0 ? SC_ACTION_UP : SC_ACTION_DOWN;
    
    send_keycode(im, key, act, "KEY");
    send_json_response(nc, 200, "{\"status\": \"success\"}");
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
static void handle_panel_action(struct mg_connection *nc, struct mg_http_message *hm, struct sc_input_manager *im, const char *action) {
    LOGI("Handling panel action request: %s", action);
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
    char action[32];
    mg_http_get_var(&hm->body, "action", action, sizeof(action));
    
    if (strcmp(action, "get") == 0) {
        get_device_clipboard(im, SC_COPY_KEY_COPY);
        send_json_response(nc, 200, "{\"status\": \"success\", \"message\": \"Clipboard request sent\"}");
    } else if (strcmp(action, "paste") == 0) {
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
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    
    if (ev != MG_EV_HTTP_MSG) { // XXX
        return;
    }
    
    // Define the routes    
    struct {
        const char *uri;
        void (*handler)(struct mg_connection *, struct mg_http_message *, struct sc_input_manager *);
    } routes[] = {
        { API_PREFIX "/keycode", handle_keycode},
        { API_PREFIX "/home", handle_home},
        { API_PREFIX "/back", handle_back},
        { API_PREFIX "/app_switch", handle_app_switch},
        { API_PREFIX "/power", handle_power},
        { API_PREFIX "/volume", handle_volume},
        { API_PREFIX "/menu", handle_menu},
        { API_PREFIX "/back_or_screen_on", handle_back_or_screen_on},
        { API_PREFIX "/virtual_finger", handle_virtual_finger}
    };
    
    // Find and execute the appropriate handler
    for (size_t i = 0; i < sizeof(routes); i++) {
        if (mg_vcmp(&hm->uri, routes[i].uri) == 0) {
            if (mg_vcmp(&hm->method, "POST") == 0) {
                routes[i].handler(nc, hm, server->input_manager);
                return;
            }
            send_error_response(nc, 405, "Method not allowed");
            return;
        }
    }

    // Special handling for panel actions
    if (mg_vcmp(&hm->uri, API_PREFIX "/panel") == 0) {
        char action[32];
        mg_http_get_var(&hm->body, "action", action, sizeof(action));
        handle_panel_action(nc, hm, server->input_manager, action);
        return;
    }
    
    // Handle frame endpoint
    if (mg_vcmp(&hm->uri, API_PREFIX "/frame") == 0) {
        if (mg_vcmp(&hm->method, "GET") == 0) {
            handle_frame(nc, hm, server);
            return;
        }
        send_error_response(nc, 405, "Method not allowed");
        return;
    }
    
    // Handle other routes
    if (mg_vcmp(&hm->uri, API_PREFIX "/clipboard") == 0) {
        handle_clipboard(nc, hm, server->input_manager);
        return;
    }
    
    if (mg_vcmp(&hm->uri, API_PREFIX "/display/power") == 0) {
        handle_display_power(nc, hm, server->input_manager);
        return;
    }
    
    if (mg_vcmp(&hm->uri, API_PREFIX "/device/rotate") == 0) {
        handle_rotate_device(nc, hm, server->input_manager);
        return;
    }
    
    if (mg_vcmp(&hm->uri, API_PREFIX "/keyboard/settings") == 0) {
        handle_keyboard_settings(nc, hm, server->input_manager);
        return;
    }
    
    if (mg_vcmp(&hm->uri, API_PREFIX "/virtual_finger") == 0) {
        handle_virtual_finger(nc, hm, server->input_manager);
        return;
    }
    
    send_error_response(nc, 404, "Not found");
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
                        struct sc_input_manager *input_manager,
                        const char *listening_addr) {
    server->input_manager = input_manager;
    server->listening_addr = listening_addr;
    server->running = false;
    server->mongoose_ctx = NULL;
    server->current_frame = NULL;
    
    return true;
}

// Thread function for mongoose event loop
static int mongoose_poll_thread(void *arg) {
    struct sc_web_server *server = (struct sc_web_server *)arg;
    struct mg_mgr *mgr = (struct mg_mgr *)server->mongoose_ctx;
    
    while (server->running) {
        mg_mgr_poll(mgr, 100);  // 100ms timeout
    }
    
    return 0;
}

bool sc_web_server_start(struct sc_web_server *server) {
    struct mg_mgr *mgr = malloc(sizeof(struct mg_mgr));
    if (!mgr) {
        LOGE("Could not allocate mongoose manager");
        return false;
    }
    
    mg_mgr_init(mgr);
    struct mg_connection *nc = mg_http_listen(mgr, server->listening_addr, ev_handler, server);
    
    if (!nc) {
        LOGE("Could not bind to %s", server->listening_addr);
        mg_mgr_free(mgr);
        free(mgr);
        return false;
    }
    
    //mg_set_protocol_http_websocket(nc); // XXX
    server->mongoose_ctx = mgr;
    server->running = true;
    
    // Start the event loop in a new thread
    SDL_Thread *thread = SDL_CreateThread(mongoose_poll_thread, "web_server", server);
    if (!thread) {
        LOGE("Could not create web server thread");
        mg_mgr_free(mgr);
        free(mgr);
        return false;
    }
    
    LOGI("Web server started on %s", server->listening_addr);
    return true;
}

void sc_web_server_stop(struct sc_web_server *server) {
    server->running = false;
}

void sc_web_server_destroy(struct sc_web_server *server) {
    if (server->mongoose_ctx) {
        struct mg_mgr *mgr = (struct mg_mgr *)server->mongoose_ctx;
        mg_mgr_free(mgr);
        free(mgr);
        server->mongoose_ctx = NULL;
    }
}
