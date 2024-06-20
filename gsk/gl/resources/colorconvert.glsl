// VERTEX_SHADER:
// colorconvert.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// colorconvert.glsl

uniform int u_to_linear;

float
srgb_transfer_function (float v)
{
  if (v >= 0.04045)
    return pow (((v + 0.055)/(1.0 + 0.055)), 2.4);
  else
    return v / 12.92;
}

float
srgb_inverse_transfer_function (float v)
{
  if (v > 0.0031308)
    return 1.055 * pow (v, 1.0/2.4) - 0.055;
  else
    return 12.92 * v;
}

vec4
srgb_to_linear_srgb (vec4 color)
{
  return vec4 (srgb_transfer_function (color.r),
               srgb_transfer_function (color.g),
               srgb_transfer_function (color.b),
               color.a);
}

vec4
linear_srgb_to_srgb (vec4 color)
{
  return vec4 (srgb_inverse_transfer_function (color.r),
               srgb_inverse_transfer_function (color.g),
               srgb_inverse_transfer_function (color.b),
               color.a);
}

void main() {
  vec4 color = GskTexture(u_source, vUv);

  if (color.a != 0.0)
    color.rgb /= color.a;

  if (u_to_linear != 0)
    color = srgb_to_linear_srgb (color);
  else
    color = linear_srgb_to_srgb (color);

  gskSetOutputColor(gsk_scaled_premultiply(color, u_alpha));
}
