// VERTEX_SHADER:
// glyphy.vs.glsl

_OUT_ vec4 v_glyph;
_OUT_ vec4 final_color;

vec4
glyph_vertex_transcode (vec2 v)
{
  ivec2 g = ivec2 (v);
  ivec2 corner = ivec2 (mod (v, 2.));
  g /= 2;
  ivec2 nominal_size = ivec2 (mod (vec2(g), 64.));
  return vec4 (corner * nominal_size, g * 4);
}

void
main()
{
  v_glyph = glyph_vertex_transcode(aUv);
  vUv = v_glyph.zw;
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);
  final_color = gsk_scaled_premultiply(aColor, u_alpha);
}
