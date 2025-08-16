#include "mxdream_patch_config.h"
#include "mxgl/video_present.h"

/* Minimal resource loader placeholder. Not used by current pipeline because
 * the project embeds shaders via genglsl, but provided to keep the shim API. */
static char* read_file(const char* path)
{
    FILE* f = fopen(path, "rb"); if(!f) return NULL;
    fseek(f,0,SEEK_END); long n = ftell(f); fseek(f,0,SEEK_SET);
    char* b = (char*)malloc((size_t)n+1); if(!b){ fclose(f); return NULL; }
    size_t r = fread(b,1,(size_t)n,f); (void)r; b[n]=0; fclose(f); return b;
}

const char* mx_load_text(const char* name)
{
    static char buf[512];
    snprintf(buf,sizeof(buf),"src/mxgl/shaders/%s", name);
    return read_file(buf); /* caller may free if they copy immediately */
}

void video_gl_present_render_buffer(mx_render_buffer_t rb)
{
    if( rb == NULL ) return;
    extern struct display_driver display_gl_driver; /* use common display path */
    extern display_driver_t display_driver;
    if( display_driver && display_driver->display_render_buffer ) {
        display_driver->display_render_buffer(rb);
    } else {
        /* Fallback if no driver: no-op */
    }
}

void mxdream_apply_safe_defaults(void)
{
    if( getenv("LXDREAM_GL_STATE_CACHE") == NULL ) setenv("LXDREAM_GL_STATE_CACHE","0",0);
    if( getenv("LXDREAM_PRESENT_Q") == NULL ) setenv("LXDREAM_PRESENT_Q","2",0);
    if( getenv("LXDREAM_PACE") == NULL ) setenv("LXDREAM_PACE","60",0);
    if( getenv("LXDREAM_TEX_HAZARD") == NULL ) setenv("LXDREAM_TEX_HAZARD","1",0);
    if( getenv("LXDREAM_FSR") == NULL ) setenv("LXDREAM_FSR","0",0);
    if( getenv("LXDREAM_UPSCALE_SIMPLE") == NULL ) setenv("LXDREAM_UPSCALE_SIMPLE","0",0);
}


