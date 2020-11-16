/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_VITA && SDL_VIDEO_OPENGL_VITAGL

#include <stdlib.h>
#include <string.h>

#include "SDL_error.h"
#include "SDL_vitavideo.h"
#include "SDL_vitaopenglvgl.h"
#include "SDL_hints.h"

// only one instance of vitaGL can run at the same time
static int vgl_initialized = 0;

// we'll try to automatically start and end scenes
static int vgl_rendering = 0;

int
VITA_VGL_LoadLibrary(_THIS, const char *path)
{
    // we'll init later because we need that sweet gl_config
    _this->gl_config.driver_loaded = 1;
    return 0;
}

void
VITA_VGL_UnloadLibrary(_THIS)
{
    if (vgl_initialized)
    {
        vgl_initialized = 0;
        vglEnd();
    }
    _this->gl_config.driver_loaded = 0;
}

void *
VITA_VGL_GetProcAddress(_THIS, const char *proc)
{
    return vglGetProcAddress(proc);
}

SDL_GLContext
VITA_VGL_CreateContext(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;

    // if it's above 2.0, bail since we're definitely unable to satisfy that
    const double gl_ver = (double)_this->gl_config.major_version +
                          (double)_this->gl_config.minor_version * 0.1;
    if ((gl_ver > 2.05) ||
        (_this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES)) {
        SDL_SetError("unsupported GL version");
        return NULL;
    }

    if (!vgl_initialized) {
        // init vitaGL once and never deinit it again until the driver dies
        enum SceGxmMultisampleMode gxm_ms;
        const char *vmem_hint = SDL_GetHint(SDL_HINT_VITAGL_GPU_POOL_SIZE);
        const char *umem_hint = SDL_GetHint(SDL_HINT_VITAGL_RAM_THRESHOLD);
        GLuint vmem = 0, umem = 0;

        switch (_this->gl_config.multisamplesamples) { 
            case 2:  gxm_ms = SCE_GXM_MULTISAMPLE_2X; break;
            case 4:
            case 8:
            case 16: gxm_ms = SCE_GXM_MULTISAMPLE_4X; break;
            default: gxm_ms = SCE_GXM_MULTISAMPLE_NONE; break;
        }

        if (vmem_hint) vmem = strtoul(vmem_hint, NULL, 0);
        if (umem_hint) umem = strtoul(umem_hint, NULL, 0);
        if (!vmem) vmem = 0x800000;
        if (!umem) umem = 0x1000000;

        vglInitExtended(vmem, 960, 544, umem, gxm_ms);

        vgl_initialized = 1;
    }

    if (SDL_GetHintBoolean(SDL_HINT_VITAGL_USE_VRAM, SDL_TRUE)) {
        vglUseVram(GL_TRUE);
    }

    _this->gl_config.red_size = 8;
    _this->gl_config.green_size = 8;
    _this->gl_config.blue_size = 8;
    _this->gl_config.alpha_size = 8;
    _this->gl_config.depth_size = 32;
    _this->gl_config.stencil_size = 8;

    // force context version to what we actually support
    _this->gl_config.major_version = 2;
    _this->gl_config.minor_version = 0;
    _this->gl_config.profile_mask = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;

    _this->gl_config.accelerated = 1;

    wdata->uses_gl = SDL_TRUE;
    window->flags |= SDL_WINDOW_FULLSCREEN;

    // return a dummy pointer and pretend that it's a GL context
    return &vgl_initialized;
}

int
VITA_VGL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
    if (!vgl_initialized) {
        SDL_SetError("vitaGL is not initialized");
        return -1;
    }

    if (vgl_rendering) {
        vglStopRendering();
        glFinish();
        vgl_rendering = 0;
    }

    // clear the buffers a couple times so that all buffers have something in them
    glClearColor(0.f, 0.f, 0.f, 0.f);
    vglStartRendering();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    vglStopRendering();
    vglStartRendering();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    vglStopRendering();

    glFinish();

    // immediately restart rendering again
    vglStartRendering();
    vgl_rendering = 1;

    return 0;
}

int
VITA_VGL_SetSwapInterval(_THIS, int interval)
{
    if (!vgl_initialized) {
        SDL_SetError("vitaGL is not initialized");
        return -1;
    }
    _this->gl_data->swapinterval = interval;
    vglWaitVblankStart(interval);
    return 0;
}

int
VITA_VGL_GetSwapInterval(_THIS)
{
    if (!vgl_initialized) {
        SDL_SetError("vitaGL is not initialized");
        return -1;
    }
    return _this->gl_data->swapinterval;
}

int
VITA_VGL_SwapWindow(_THIS, SDL_Window * window)
{
    if (!vgl_initialized) {
        SDL_SetError("vitaGL is not initialized");
        return -1;
    }

    if (vgl_rendering) {
        vglStopRendering();
        vgl_rendering = 0;
    }

    // immediately restart rendering
    vglStartRendering();
    vgl_rendering = 1;

    return 0;
}

void
VITA_VGL_DeleteContext(_THIS, SDL_GLContext context)
{
    if (!vgl_initialized) {
        SDL_SetError("vitaGL is not initialized");
        return;
    }

    if (vgl_rendering) {
        vglStopRendering();
        glFinish();
        vgl_rendering = 0;
    }
}

void 
VITA_VGL_DefaultProfileConfig(_THIS, int *mask, int *major, int *minor)
{
    *mask = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
    *major = 2;
    *minor = 0;
}

#endif /* SDL_VIDEO_DRIVER_VITA && SDL_VIDEO_OPENGL_VITAGL */

/* vi: set ts=4 sw=4 expandtab: */
