// placeholder: pipeline uses embedded shaders; this file is optional
attribute vec2 in_vertex;
attribute vec2 in_texcoord;
uniform mat4 view_matrix;
varying vec2 frag_texcoord;
void main(){ gl_Position = view_matrix * vec4(in_vertex.xy,0.0,1.0); frag_texcoord = in_texcoord; }


