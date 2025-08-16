// placeholder
#ifdef GL_ES
precision mediump float;
#endif
uniform sampler2D primary_texture;
varying vec2 frag_texcoord;
void main(){ gl_FragColor = texture2D(primary_texture, frag_texcoord); }


