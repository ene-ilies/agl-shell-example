#ifndef WAYLAND_DISPLAY_H
#define WAYLAND_DISPLAY_H

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <list>
#include "wayland-agl-shell-client-protocol.h"
#include "xdg-shell-client-protocol.h"

struct client_display {
    struct wl_display* display = nullptr;
    struct wl_compositor* compositor = nullptr;
    struct wl_registry* registry = nullptr;    
    struct wl_output* output;
    struct wl_shm *shm;
    struct xdg_wm_base* xdg_wm_base;
    struct agl_shell *agl_shell; 
};

struct client_buffer {
    wl_buffer *buffer;
    void *data;
    bool busy;
};

struct client_content {
    client_buffer* buffers[2];    
};
    
struct client_surface {
    struct client_display* display;
    struct wl_surface* surface;
    struct xdg_surface* xdg_surface;
    struct xdg_toplevel* toplevel;
    struct wl_callback* frameCalback;
    int32_t width;
    int32_t height;

    client_content content;
    void (*draw)(void* data, int32_t width, int32_t height);
};

class ExampleScene
{
private:
    struct client_display *display;
    std::list<struct client_surface *> surfaces;
public:
    ExampleScene();
    void loop();
    ~ExampleScene();
private:
    int init();
};

#endif /* WAYLAND_DISPLAY_H */