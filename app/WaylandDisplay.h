#ifndef WAYLAND_DISPLAY_H
#define WAYLAND_DISPLAY_H

#include <string.h>
#include <stdio.h>
#include <stdint.h>

struct display_state {
    struct wl_display* display = nullptr;
    struct wl_compositor* compositor = nullptr;
    struct wl_registry* registry = nullptr;    
    struct wl_output* output;
    struct wl_shm *shm;

    struct agl_shell *agl_shell;
    struct agl_shell_desktop *agl_shell_desktop; 

    struct xdg_wm_base* xdg_wm_base;

    struct wl_surface* bg_surface;
    struct xdg_surface* bg_xdg_surface;
    struct xdg_toplevel* bg_toplevel;
    void *bg_shm_data;
    int32_t bg_width;
    int32_t bg_height;

    struct wl_surface* app_surface;
    struct xdg_surface* app_xdg_surface;
    struct xdg_toplevel* app_toplevel;
    void *app_shm_data;
    int32_t app_width;
    int32_t app_height;
};

class WaylandDisplay
{
private:
    struct display_state* state;
public:
    WaylandDisplay();
    void loop();
    ~WaylandDisplay();
};

#endif /* WAYLAND_DISPLAY_H */