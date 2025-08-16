#include "mxdream_patch_config.h"

void mx_fbo_begin(mx_render_buffer_t rb)
{
    (void)rb; /* Existing driver binds targets via display_driver->set_render_target */
}

void mx_fbo_end(mx_render_buffer_t rb)
{
    if( rb == NULL ) return;
    if( rb->fence ) {
        glDeleteSync(rb->fence);
        rb->fence = 0;
    }
    rb->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();
}

bool mx_fbo_is_ready(const mx_render_buffer_t rb)
{
    if( rb == NULL || rb->fence == 0 ) return true;
    GLenum w = glClientWaitSync(rb->fence, 0, 0);
    return (w == GL_ALREADY_SIGNALED || w == GL_CONDITION_SATISFIED);
}

void mx_fbo_wait_and_consume(mx_render_buffer_t rb)
{
    if( rb == NULL || rb->fence == 0 ) return;
    GLenum w = glClientWaitSync(rb->fence, GL_SYNC_FLUSH_COMMANDS_BIT, 5*1000*1000);
    if( w == GL_TIMEOUT_EXPIRED ) {
        glClientWaitSync(rb->fence, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
    }
    glDeleteSync(rb->fence);
    rb->fence = 0;
}


