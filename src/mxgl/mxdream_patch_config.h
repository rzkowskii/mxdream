#pragma once

/*
 * mxdream_patch_config.h
 *
 * Central shim configuration for the mxgl compatibility layer. This header
 * adapts the shim to the existing lxdream/mxdream codebase without requiring
 * widespread renames.
 *
 * Nothing in this folder is compiled by default; it is safe to add the files
 * and wire call-sites later. The shim conforms to the current tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>

/* Use the project GL glue which selects the right headers per platform */
#include "pvr2/glutil.h"

/* On macOS, avoid hard-including gl3.h here to prevent conflicts with CGLMacro
 * in existing files. The project’s glutil.h handles GL header selection. */

/* Your tree’s headers */
#include "pvr2/pvr2.h"
#include "pvr2/scene.h"
#include "display.h"

/* ---------- type aliases to existing types ---------- */
typedef struct pvr2_scene_struct mx_scene_t;
typedef render_buffer_t mx_render_buffer_t; /* pointer type from lxdream.h */

/* ---------- logging ---------- */
#ifndef MX_LOGI
#define MX_LOGI(...) fprintf(stderr, __VA_ARGS__)
#endif
#ifndef MX_LOGE
#define MX_LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

/* ---------- resource loading for shaders (optional) ---------- */
/* Returns heap-allocated buffer with file contents, or NULL on error.
 * Caller may free(). Not used by current build; provided for completeness. */
const char* mx_load_text(const char* name);

/* Optional compile-time tripwire. Define MX_TRIPWIRE_GL=1 before including
 * this header to ban stray raw GL state calls at compile time. */
/* #define MX_TRIPWIRE_GL 1 */


