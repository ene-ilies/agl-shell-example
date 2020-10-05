#include "ExampleScene.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <string>

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

static void buffer_release(void *data, struct wl_buffer *buffer) {
	struct client_buffer *client_buffer = (struct client_buffer*) data;
	client_buffer->busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

static struct client_buffer* create_client_buffer(struct wl_shm *shm, int32_t width, int32_t height) {
    struct wl_shm_pool *pool;
    int stride = width * 4; // 4 bytes per pixel
    int size = stride * height;
    struct client_buffer *new_buffer = new client_buffer();

    int fd = os_create_anonymous_file(size);
    if (fd < 0)
    {
        fprintf(stderr, "creating a buffer file for %d B failed: %m\n",
                size);
        exit(1);
    }
    fprintf(stderr, "Created buffer file with fd: %d\n", fd);

    new_buffer->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (new_buffer->data == MAP_FAILED)
    {
        fprintf(stderr, "mmap failed: %m\n");
        close(fd);
        exit(1);
    }
    fprintf(stderr, "Mapped memory to file: %p\n", new_buffer->data);

    pool = wl_shm_create_pool(shm, fd, size);
    fprintf(stderr, "Created pool\n");
    new_buffer->buffer = wl_shm_pool_create_buffer(pool, 0,
                                     width, height,
                                     stride,
                                     WL_SHM_FORMAT_XRGB8888);
    fprintf(stderr, "bufer created\n");
    wl_buffer_add_listener(new_buffer->buffer, &buffer_listener, new_buffer);
    wl_shm_pool_destroy(pool);
    
    return new_buffer;
}

static void toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, 
        int32_t width, int32_t height, struct wl_array *states) {
    struct client_surface *client_surface = (struct client_surface *)data;

    fprintf(stderr, "toplevel configure width: %d, height: %d\n", width, height);

    if (width == 0) {
        if (client_surface->width > 0) {
            width = client_surface->width; 
        } else {
            width = 1920;
        }        
    }
    if (height == 0) {
        if (client_surface->height > 0) {
            height = client_surface->height;
        } else {
            height = 200;
        }
    }

    client_surface->width = width;
    client_surface->height = height;
    fprintf(stderr, "actual width: %d, height: %d\n", client_surface->width, client_surface->height);    
}

static void toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    fprintf(stderr, "toplevel closed.\n");
}

static xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure,
    .close = toplevel_close
};

void redraw(void *data, wl_callback *callback, uint32_t time);

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    struct client_surface *client_surface = (struct client_surface *) data;

    fprintf(stderr, "Got surface configure for serial %d\n", serial);
    if (xdg_surface == nullptr)
    {
        fprintf(stderr, "Provided app xdg surface is null\n");
        return;
    }
    xdg_surface_ack_configure(xdg_surface, serial);
    fprintf(stderr, "Ack app configure for serial %d\n", serial);

    client_surface->content.buffers[0] = create_client_buffer(client_surface->display->shm, client_surface->width, client_surface->height);
    client_surface->content.buffers[1] = create_client_buffer(client_surface->display->shm, client_surface->width, client_surface->height);
    fprintf(stderr, "Buffers created for surface %p\n", client_surface->surface);

    redraw(client_surface, nullptr, 0);
}

struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure
};

static client_surface* create_surface(client_display *display, 
        std::function<void(void*, int32_t, int32_t)> draw, 
        int32_t width, int32_t height) {
    struct client_surface *new_surface = new client_surface();
    new_surface->draw = draw;
    new_surface->display = display;
    new_surface->width = width;
    new_surface->height = height;

    new_surface->surface = wl_compositor_create_surface(display->compositor);
    if (!new_surface->surface) {
        fprintf(stderr, "Can't create surface\n");
        return nullptr;
    }
    fprintf(stderr, "Created surface.\n");
    
    new_surface->xdg_surface = xdg_wm_base_get_xdg_surface(display->xdg_wm_base, new_surface->surface);
    if (new_surface->xdg_surface == nullptr) {
        fprintf(stderr, "Can't create xdg_surface.\n");
        return nullptr;
    }
    fprintf(stderr, "Created xdg_surface.\n");
    xdg_surface_add_listener(new_surface->xdg_surface, &xdg_surface_listener, new_surface);
    fprintf(stderr, "Added listener to xdg_surface.\n");

    new_surface->toplevel = xdg_surface_get_toplevel(new_surface->xdg_surface);
    if (new_surface->toplevel == nullptr) {
        fprintf(stderr, "Can't create toplevel_surface.\n");
        return nullptr;
    }
    fprintf(stderr, "Created toplevel_surface.\n");
    xdg_toplevel_add_listener(new_surface->toplevel, &toplevel_listener, new_surface);
    fprintf(stderr, "Added listener to toplevel_surface\n");

    xdg_toplevel_set_app_id(new_surface->toplevel, "homescreen");
    fprintf(stderr, "Setted app id for xdg_toplevel\n");
    wl_surface_commit(new_surface->surface);

    return new_surface;
}

static const struct wl_callback_listener frame_listener = {
	redraw
};

void redraw(void *data, wl_callback *callback, uint32_t time) {
    struct client_surface *surface = (struct client_surface *)data;

    client_buffer* next_buffer = surface->content.buffers[0];
    if (next_buffer->busy) {
        next_buffer = surface->content.buffers[1];
    }

    next_buffer->busy = true;
    surface->draw(next_buffer->data, surface->width, surface->height);

    wl_surface_attach(surface->surface, next_buffer->buffer, 0, 0);
    wl_surface_damage(surface->surface, 0, 0, surface->width, surface->height);
    wl_surface_commit(surface->surface);

    if (callback) {
        wl_callback_destroy(callback);
    }

    wl_callback *nextCallback = wl_surface_frame(surface->surface);
	wl_callback_add_listener(nextCallback, &frame_listener, surface);
	wl_surface_commit(surface->surface);

    surface->frameCalback = nextCallback;
}

void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    fprintf(stderr, "Got ping on xdg base for serial %d\n", serial);
    xdg_wm_base_pong(xdg_wm_base, serial);
}

struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping
};

void global_registry_handler(void *data, struct wl_registry *registry, uint32_t id,
                             const char *interface, uint32_t version) {
    fprintf(stderr, "Got a registry event for %s id %d\n", interface, id);
    struct client_display *client_display = (struct client_display *)data;

    if (strcmp(interface, wl_output_interface.name) == 0)
    {
        client_display->output = (struct wl_output *)wl_registry_bind(client_display->registry, id, &wl_output_interface, 1);
    }
    else if (strcmp(interface, wl_compositor_interface.name) == 0)
    {
        client_display->compositor = (struct wl_compositor *)wl_registry_bind(client_display->registry, id, &wl_compositor_interface, 1);
    }
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
    {
        client_display->xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(client_display->registry, id, &xdg_wm_base_interface, 1);
    }
    else if (strcmp(interface, wl_shm_interface.name) == 0)
    {
        client_display->shm = (struct wl_shm *) wl_registry_bind(client_display->registry, id,
                               &wl_shm_interface, 1);
    }
    else if (strcmp(interface, agl_shell_interface.name) == 0)
    {
        client_display->agl_shell = (struct agl_shell *) wl_registry_bind(client_display->registry, id,
                               &agl_shell_interface, 1);
    }
}

void global_registry_remover(void *data, struct wl_registry *registry, uint32_t id)
{
    fprintf(stderr, "Got a registry losing event for %d\n", id);
}

const struct wl_registry_listener registry_listener = {
    global_registry_handler,
    global_registry_remover
};

void destroy_display(client_display* display);

client_display* create_display() {
    struct client_display* new_display = new client_display();
    new_display->display = wl_display_connect(nullptr);
    if (!new_display->display) {
        fprintf(stderr, "Can't connect to display.\n");
        destroy_display(new_display);
        return nullptr;
    }
    fprintf(stderr, "Connected to display.\n");

    new_display->registry = wl_display_get_registry(new_display->display);
    if (!new_display->registry) {
        fprintf(stderr, "Can't obtain registry object.\n");
        destroy_display(new_display);
        return nullptr;
    }
    fprintf(stderr, "Obtained registry object.\n");
    wl_registry_add_listener(new_display->registry, &registry_listener, new_display);

    wl_display_dispatch(new_display->display);
    wl_display_roundtrip(new_display->display);

    if (!new_display->output) {
        fprintf(stderr, "Output not available.\n");
        destroy_display(new_display);
        return nullptr;
    }

    if (!new_display->shm) {
        fprintf(stderr, "SHM not available.\n");
        destroy_display(new_display);
        return nullptr;
    }

    if (!new_display->compositor) {
        fprintf(stderr, "Compositor not available.\n");
        destroy_display(new_display);
        return nullptr;
    }

    if (!new_display->xdg_wm_base) {
        fprintf(stderr, "xdg_wm_base not available.\n");
        destroy_display(new_display);
        return nullptr;
    }
    xdg_wm_base_add_listener(new_display->xdg_wm_base, &xdg_wm_base_listener, new_display);
    fprintf(stderr, "Added listener to xdg_wm_base\n");

    if (!new_display->agl_shell) {
        fprintf(stderr, "AGL shell not available.\n");
        destroy_display(new_display);
        return nullptr;
    }

    return new_display;
}

void destroy_surface(client_surface* surface) {
    if (surface->frameCalback)
		wl_callback_destroy(surface->frameCalback);

	if (surface->content.buffers[0]->buffer)
		wl_buffer_destroy(surface->content.buffers[0]->buffer);

	if (surface->content.buffers[1]->buffer)
		wl_buffer_destroy(surface->content.buffers[1]->buffer);

	if (surface->toplevel)
		xdg_toplevel_destroy(surface->toplevel);

	if (surface->xdg_surface)
		xdg_surface_destroy(surface->xdg_surface);

    if (surface->surface)
        wl_surface_destroy(surface->surface);

    delete surface;
}

void destroy_display(client_display* display) {
    if (display->shm)
		wl_shm_destroy(display->shm);

	if (display->xdg_wm_base)
		xdg_wm_base_destroy(display->xdg_wm_base);

	if (display->compositor)
		wl_compositor_destroy(display->compositor);

    if (display->registry)
        wl_registry_destroy(display->registry);
	    
    if (display->output)
        wl_output_destroy(display->output);

    if (display->agl_shell)
        agl_shell_destroy(display->agl_shell);

    if (display->display) {
        wl_display_flush(display->display);
	    wl_display_disconnect(display->display);
    }
	
	delete display;
}

void top_draw(void* data, int32_t width, int32_t height) {
    memset(data, 0xff, width * height * 4);
}

void bg_draw(void* data, int32_t width, int32_t height) {
    memset(data, 0xaf, width * height * 4);
}

int ExampleScene::init() {
    this->display = create_display();
    if (!this->display) {
        fprintf(stderr, "Unable to initialize display.\n");
        return 1;
    }

    client_surface* top_surface = create_surface(this->display, top_draw, 200, 100);
    if (!top_surface) {
        fprintf(stderr, "Unable to create top surface.\n");
        destroy_surface(top_surface);
        return 2;
    }
    this->surfaces.push_back(top_surface);

    client_surface* background = create_surface(this->display, bg_draw, 1920, 1080);
    if (!background) {
        fprintf(stderr, "Unable to initialize background.\n");
        destroy_surface(background);
        return 3;
    }
    this->surfaces.push_back(background);

    agl_shell_set_panel(this->display->agl_shell, top_surface->surface, this->display->output, AGL_SHELL_EDGE_TOP);
    agl_shell_set_background(this->display->agl_shell, background->surface, this->display->output);
    agl_shell_ready(this->display->agl_shell);

    return 0;
}

ExampleScene::ExampleScene()
{
    this->display = new client_display();
    int rc = init();
    if (rc) {
        fprintf(stderr, "Unable to set up display.\n");
    }
}

void ExampleScene::loop(std::function<bool()> stillRunning)
{
    while (stillRunning() && wl_display_dispatch(this->display->display) != -1)
    {
        /* This space deliberately left blank */
    }
}

ExampleScene::~ExampleScene()
{
    for (auto surface : this->surfaces) {
        destroy_surface(surface);
    }
    fprintf(stderr, "Cleaned up all the surfaces.\n");

    destroy_display(this->display);
    fprintf(stderr, "Cleaned up display related objects.\n");
}
