/* Thin shims over drivers/gl_state.* so call-sites can standardize on gls_* */
#include "mxdream_patch_config.h"
#include "drivers/gl_state.h"
#include "mxgl/gl_state.h"

void gls_init(void)
{
    gl_state_cache_init();
}

void gls_reset_frame(void)
{
    gl_state_cache_reset();
}

void gls_reset_pass(void)
{
    /* placeholder: per-pass cache invalidations could go here if needed */
}

void gls_validate_and_resync(void)
{
    /* placeholder: could assert against raw GL drift in debug builds */
}


