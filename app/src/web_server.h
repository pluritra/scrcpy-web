#ifndef SC_WEB_SERVER_H
#define SC_WEB_SERVER_H

#include <stdbool.h>
#include <libavcodec/avcodec.h>
#include "input_manager.h"

struct sc_web_server {
    struct sc_input_manager *input_manager;
    void *mongoose_ctx;  // mongoose context (opaque)
    const char *listening_addr;
    bool running;
    AVFrame *current_frame;  // Store the current frame
};

// Initialize the web server
bool
sc_web_server_init(struct sc_web_server *server,
                   struct sc_input_manager *input_manager,
                   const char *listening_addr);

// Start the web server (non-blocking)
bool
sc_web_server_start(struct sc_web_server *server);

// Stop the web server
void
sc_web_server_stop(struct sc_web_server *server);

// Destroy the web server and free resources
void
sc_web_server_destroy(struct sc_web_server *server);

// Update the current frame
void
sc_web_server_set_frame(struct sc_web_server *server, const AVFrame *frame);

#endif  // SC_WEB_SERVER_H
