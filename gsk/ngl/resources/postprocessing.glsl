
// VERTEX_SHADER:
// postprocessing.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// postprocessing.glsl
float u_gamma = 2.2;

vec4 gsk_unpremultiply(vec4 c)
{
  if (c.a != 0)
    return vec4(c.rgb / c.a, c.a);
  else
    return c;
}

vec4 gsk_srgb_to_linear(vec4 srgb)
{
  vec3 linear_rgb = pow(srgb.rgb, vec3(u_gamma));
  return vec4(linear_rgb, srgb.a);
}

vec4 gsk_linear_to_srgb(vec4 linear_rgba)
{
  vec3 srgb = pow(linear_rgba.rgb , vec3(1/u_gamma));
  return vec4(srgb, linear_rgba.a);
}
void main() {
  vec4 diffuse = GskTexture(u_source, vUv);

  diffuse = gsk_unpremultiply(diffuse);
  diffuse = gsk_linear_to_srgb(diffuse);
  diffuse = gsk_premultiply(diffuse);

  gskSetOutputColor(diffuse);
}
