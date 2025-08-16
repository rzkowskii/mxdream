// placeholder
#ifdef GL_ES
precision mediump float;
#endif
uniform sampler2D primary_texture;
uniform vec2 texel_size;
uniform float sharp;
varying vec2 frag_texcoord;
void main(){
  vec3 c = texture2D(primary_texture, frag_texcoord).rgb;
  vec3 cx = texture2D(primary_texture, frag_texcoord + vec2(texel_size.x,0.0)).rgb +
            texture2D(primary_texture, frag_texcoord - vec2(texel_size.x,0.0)).rgb;
  vec3 cy = texture2D(primary_texture, frag_texcoord + vec2(0.0,texel_size.y)).rgb +
            texture2D(primary_texture, frag_texcoord - vec2(0.0,texel_size.y)).rgb;
  vec3 avg = (cx + cy) * 0.25;
  vec3 hp = c - avg;
  vec3 outc = c + clamp(hp * (0.5 + sharp), -0.25, 0.25);
  gl_FragColor = vec4(outc, 1.0);
}


