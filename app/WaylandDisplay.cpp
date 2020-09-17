#include "WaylandDisplay.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <string>
#include "wayland-agl-shell-client-protocol.h"
#include "wayland-agl-shell-desktop-client-protocol.h"
#include "xdg-shell-client-protocol.h"


static char fileNo = 'a';

static int
set_cloexec_or_close(int fd)
{
    long flags;

    if (fd == -1)
        return -1;

    flags = fcntl(fd, F_GETFD);
    if (flags == -1)
        goto err;

    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
        goto err;

    return fd;

err:
    close(fd);
    return -1;
}

static int
create_tmpfile_cloexec(char *tmpname)
{
    int fd;

#ifdef HAVE_MKOSTEMP
    fd = mkostemp(tmpname, O_CLOEXEC);
    if (fd >= 0)
        unlink(tmpname);
#else
    fd = mkstemp(tmpname);
    if (fd >= 0)
    {
        fd = set_cloexec_or_close(fd);
        unlink(tmpname);
    }
#endif

    return fd;
}

/*
 * Create a new, unique, anonymous file of the given size, and
 * return the file descriptor for it. The file descriptor is set
 * CLOEXEC. The file is immediately suitable for mmap()'ing
 * the given size at offset zero.
 *
 * The file should not have a permanent backing store like a disk,
 * but may have if XDG_RUNTIME_DIR is not properly implemented in OS.
 *
 * The file name is deleted from the file system.
 *
 * The file is suitable for buffer sharing between processes by
 * transmitting the file descriptor over Unix sockets using the
 * SCM_RIGHTS methods.
 */
int os_create_anonymous_file(off_t size)
{
    static char templ[] = "/weston-shared-XXXXXX";
    //templ[20] = fileNo++;
    const char *path;
    char *name;
    int fd;

    path = getenv("XDG_RUNTIME_DIR");
    if (!path)
    {
        errno = ENOENT;
        return -1;
    }

    name = (char *)malloc(strlen(path) + sizeof(templ));
    if (!name)
        return -1;
    strcpy(name, path);
    strcat(name, templ);

    fprintf(stderr, "creating tmp file with name: %s\n", name);
    fd = create_tmpfile_cloexec(name);

    free(name);

    if (fd < 0)
        return -1;

    if (ftruncate(fd, size) < 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

static void
paint_pixels(int width, int height, void *shm_data, uint32_t color)
{
    int n;
    uint32_t *pixel = (uint32_t *)shm_data;

    fprintf(stderr, "Painting pixels width: %d, height: %d\n", width, height);
    for (n = 0; n < width * height; n++)
    {
        *pixel++ = 0xFFFFFF;
    }
}

static struct wl_buffer *
create_buffer(int32_t width, int32_t height, struct wl_shm *shm, void **shm_data)
{
    struct wl_shm_pool *pool;
    int stride = width * 4; // 4 bytes per pixel
    int size = stride * height;
    int fd;
    struct wl_buffer *buff;

    fd = os_create_anonymous_file(size);
    if (fd < 0)
    {
        fprintf(stderr, "creating a buffer file for %d B failed: %m\n",
                size);
        exit(1);
    }
    fprintf(stderr, "Created buffer file with fd: %d\n", fd);

    (*shm_data) = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if ((*shm_data) == MAP_FAILED)
    {
        fprintf(stderr, "mmap failed: %m\n");
        close(fd);
        exit(1);
    }
    fprintf(stderr, "Mapped memory to file: %p\n", shm_data);

    pool = wl_shm_create_pool(shm, fd, size);
    fprintf(stderr, "Created pool\n");
    buff = wl_shm_pool_create_buffer(pool, 0,
                                     width, height,
                                     stride,
                                     WL_SHM_FORMAT_XRGB8888);
    fprintf(stderr, "Created pool\n");
    //wl_buffer_add_listener(buffer, &buffer_listener, buffer);
    wl_shm_pool_destroy(pool);
    fprintf(stderr, "bufer created\n");
    return buff;
}

static void
create_window(int32_t width, int32_t height, wl_surface *surface, struct wl_shm *shm, void **shm_data)
{
    fprintf(stderr, "Creating window\n");
    struct wl_buffer *buffer = create_buffer(width, height, shm, shm_data);

    fprintf(stderr, "Attaching bufer\n");
    wl_surface_attach(surface, buffer, 0, 0);
    fprintf(stderr, "Buffer attached\n");
    //wl_surface_damage(surface, 0, 0, WIDTH, HEIGHT);
    wl_surface_commit(surface);
    fprintf(stderr, "Window created.\n");
}

static void
app_toplevel_configure(void *data,
                       struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height,
                       struct wl_array *states)
{
    struct display_state *state = (struct display_state *)data;

    fprintf(stderr, "app toplevel configure width: %d, height: %d\n", width, height);
    ///if (width == 0 || height == 0)
    //{
        /* Compositor is deferring to us */
    //    return;
    //}

    if (width == 0) {
        width = 1920;
    }
    if (height == 0) {
        height = 200;
    }

    fprintf(stderr, "app toplevel configure width: %d, height: %d\n", width, height);
    state->app_width = width;
    state->app_height = height;
}

static void
app_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
    fprintf(stderr, "app toplevel close\n");
}

static xdg_toplevel_listener app_toplevel_listener = {
    .configure = app_toplevel_configure,
    .close = app_toplevel_close};

void app_xdg_surface_configure(void *data,
                           struct xdg_surface *xdg_surface, uint32_t serial)
{
    struct display_state *state = (struct display_state *)data;

    fprintf(stderr, "Got app surface configure for serial %d\n", serial);
    if (xdg_surface == nullptr)
    {
        fprintf(stderr, "Provided app xdg surface is null\n");
        return;
    }
    xdg_surface_ack_configure(xdg_surface, serial);
    fprintf(stderr, "Ack app configure for serial %d\n", serial);

    create_window(state->app_width, state->app_height, state->app_surface, state->shm, &state->app_shm_data);
    wl_surface_commit(state->app_surface);
    paint_pixels(state->app_width, state->app_height, state->app_shm_data, 0xFF0000);
    wl_surface_damage(state->app_surface, 0, 0, state->app_width, state->app_height);
    wl_surface_commit(state->app_surface);
}

struct xdg_surface_listener app_xdg_surface_listener = {
    .configure = app_xdg_surface_configure,
};

static void
bg_toplevel_configure(void *data,
                       struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height,
                       struct wl_array *states)
{
    struct display_state *state = (struct display_state *)data;

    fprintf(stderr, "toplevel configure width: %d, height: %d\n", width, height);
    ///if (width == 0 || height == 0)
    //{
        /* Compositor is deferring to us */
    //    return;
    //}

    if (width == 0) {
        width = 1920;
    }
    if (height == 0) {
        height = 1080;
    }

    fprintf(stderr, "toplevel configure width: %d, height: %d\n", width, height);
    state->bg_width = width;
    state->bg_height = height;
}

static void
bg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
    fprintf(stderr, "toplevel close\n");
}

static xdg_toplevel_listener bg_toplevel_listener = {
    .configure = bg_toplevel_configure,
    .close = bg_toplevel_close};

void bg_xdg_surface_configure(void *data,
                           struct xdg_surface *xdg_surface, uint32_t serial)
{
    struct display_state *state = (struct display_state *)data;

    fprintf(stderr, "Got surface configure for serial %d\n", serial);
    if (xdg_surface == nullptr)
    {
        fprintf(stderr, "Provided xdg surface is null\n");
        return;
    }
    xdg_surface_ack_configure(xdg_surface, serial);
    fprintf(stderr, "Ack configure for serial %d\n", serial);

    create_window(state->bg_width, state->bg_height, state->bg_surface, state->shm, &state->bg_shm_data);
    wl_surface_commit(state->bg_surface);
    paint_pixels(state->bg_width, state->bg_height, state->bg_shm_data, 0xFFFFFF);
    wl_surface_commit(state->bg_surface);
}

struct xdg_surface_listener bg_xdg_surface_listener = {
    .configure = bg_xdg_surface_configure,
};

void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    fprintf(stderr, "Got ping on xdg base for serial %d\n", serial);
    xdg_wm_base_pong(xdg_wm_base, serial);
}

struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

void global_registry_handler(void *data, struct wl_registry *registry, uint32_t id,
                             const char *interface, uint32_t version)
{
    fprintf(stderr, "Got a registry event for %s id %d\n", interface, id);
    struct display_state *state = (struct display_state *)data;

    if (strcmp(interface, wl_output_interface.name) == 0)
    {
        state->output = (struct wl_output *)wl_registry_bind(state->registry, id, &wl_output_interface, 1);
    }
    else if (strcmp(interface, wl_compositor_interface.name) == 0)
    {
        state->compositor = (struct wl_compositor *)wl_registry_bind(state->registry, id, &wl_compositor_interface, 1);
    }
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
    {
        state->xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(state->registry, id, &xdg_wm_base_interface, 1);
    }
    else if (strcmp(interface, wl_shm_interface.name) == 0)
    {
        state->shm = (struct wl_shm *) wl_registry_bind(registry, id,
                               &wl_shm_interface, 1);
    }
    else if (strcmp(interface, agl_shell_interface.name) == 0)
    {
        state->agl_shell = (struct agl_shell *) wl_registry_bind(registry, id,
                               &agl_shell_interface, 1);
    }
    /*else if (strcmp(interface, agl_shell_desktop_interface.name) == 0)
    {
        state->agl_shell_desktop = (struct agl_shell_desktop *) wl_registry_bind(registry, id,
                               &agl_shell_desktop_interface, 1);
    }*/
}

void global_registry_remover(void *data, struct wl_registry *registry, uint32_t id)
{
    fprintf(stderr, "Got a registry losing event for %d\n", id);
}

const struct wl_registry_listener registry_listener = {
    global_registry_handler,
    global_registry_remover};

int setUpDisplay(struct display_state *state)
{
    state->display = wl_display_connect(nullptr);
    if (state->display == NULL)
    {
        fprintf(stderr, "Can't connect to display\n");
        return 1;
    }
    fprintf(stderr, "connected to display\n");

    state->registry = wl_display_get_registry(state->display);
    wl_registry_add_listener(state->registry, &registry_listener, state);

    wl_display_dispatch(state->display);
    wl_display_roundtrip(state->display);

    if (state->compositor == NULL)
    {
        fprintf(stderr, "Can't find compositor\n");
        return 1;
    }
    else
    {
        fprintf(stderr, "Found compositor\n");
    }

    state->bg_surface = wl_compositor_create_surface(state->compositor);
    if (state->bg_surface == NULL)
    {
        fprintf(stderr, "Can't create bg_surface\n");
        return 1;
    }
    else
    {
        fprintf(stderr, "Created bg_surface: \n");
    }

    xdg_wm_base_add_listener(state->xdg_wm_base, &xdg_wm_base_listener, state);
    fprintf(stderr, "Added listener to xdg_wm_base\n");
    state->bg_xdg_surface = xdg_wm_base_get_xdg_surface(state->xdg_wm_base, state->bg_surface);
    fprintf(stderr, "Created bg_xdg_surface\n");  
    xdg_surface_add_listener(state->bg_xdg_surface, &bg_xdg_surface_listener, state);
    fprintf(stderr, "Added listener to bg_xdg_surface\n");

    state->bg_toplevel = xdg_surface_get_toplevel(state->bg_xdg_surface);
    fprintf(stderr, "Created bg_toplevel\n");
    xdg_toplevel_add_listener(state->bg_toplevel, &bg_toplevel_listener, state);
    fprintf(stderr, "Added listener to xdg_toplevel\n");
    xdg_toplevel_set_app_id(state->bg_toplevel, "homescreen");
    fprintf(stderr, "Setted app id for xdg_toplevel\n");
    xdg_toplevel_set_title(state->bg_toplevel, "home window");
    wl_surface_commit(state->bg_surface);

    state->app_surface = wl_compositor_create_surface(state->compositor);
    if (state->app_surface == NULL)
    {
        fprintf(stderr, "Can't create app_surface\n");
        return 1;
    }
    else
    {
        fprintf(stderr, "Created app_surface: \n");
    }

    state->app_xdg_surface = xdg_wm_base_get_xdg_surface(state->xdg_wm_base, state->app_surface);
    fprintf(stderr, "Created app_xdg_surface\n");  
    xdg_surface_add_listener(state->app_xdg_surface, &app_xdg_surface_listener, state);
    fprintf(stderr, "Added listener to app_xdg_surface\n");

    state->app_toplevel = xdg_surface_get_toplevel(state->app_xdg_surface);
    fprintf(stderr, "Created app_toplevel\n");
    xdg_toplevel_add_listener(state->app_toplevel, &app_toplevel_listener, state);
    fprintf(stderr, "Added listener to app_toplevel\n");
    wl_surface_commit(state->app_surface);

    xdg_surface_set_window_geometry(state->app_xdg_surface, 0, 0, 1920, 200);
    wl_surface_commit(state->app_surface);

    xdg_toplevel_set_app_id(state->app_toplevel, "homescreen");
    fprintf(stderr, "Setted app id for app_toplevel\n");
    xdg_toplevel_set_title(state->app_toplevel, "home window");
    wl_surface_commit(state->app_surface);

    agl_shell_set_background(state->agl_shell, state->bg_surface, state->output);
    agl_shell_set_panel(state->agl_shell, state->app_surface, state->output, AGL_SHELL_EDGE_TOP);
    wl_surface_commit(state->app_surface);
    //agl_shell_set_panel(state->agl_shell, state->surface, state->output, AGL_SHELL_EDGE_BOTTOM);
    //agl_shell_set_panel(state->agl_shell, state->surface, state->output, AGL_SHELL_EDGE_LEFT);
    //agl_shell_set_panel(state->agl_shell, state->surface, state->output, AGL_SHELL_EDGE_RIGHT);
    
    //xdg_toplevel_set_fullscreen(state->xdg_toplevel, state->output);
    
    agl_shell_ready(state->agl_shell);

    fprintf(stderr, "Activatting app.\n");
    agl_shell_activate_app(state->agl_shell, "homescreen", state->output);
    //agl_shell_activate_app(state->agl_shell, "test-agl-native", state->output);
    //fprintf(stderr, "Activated app\n");

    return 0;
}

WaylandDisplay::WaylandDisplay()
{
    state = new display_state();
    int rc = setUpDisplay(state);
    if (rc != 0)
    {
        fprintf(stderr, "Unable to set up display\n");
    }
}

void WaylandDisplay::loop()
{
    while (wl_display_dispatch(state->display) != -1)
    {
        /* This space deliberately left blank */
    }
}

WaylandDisplay::~WaylandDisplay()
{
    wl_display_disconnect(state->display);
    fprintf(stderr, "disconnected from display\n");
}
