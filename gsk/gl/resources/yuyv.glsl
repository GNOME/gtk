// VERTEX_SHADER:
// y_xuxv_to_rgba.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// y_xuxv_to_rgba.glsl

uniform sampler2D u_source2;

vec4 yuv_to_rgb(vec4 yuv)
{
  vec4 res;

  float Y = 1.16438356 * (yuv.x - 0.0625);
  float su = yuv.y - 0.5;
  float sv = yuv.z - 0.5;

  res.r = Y                   + 1.59602678 * sv;
  res.g = Y - 0.39176229 * su - 0.81296764 * sv;
  res.b = Y + 2.01723214 * su;

  res.rgb *= yuv.w;
  res.a = yuv.w;

  return res;
}

/* This shader converts single-plane yuv (DRM_FORMAT_YUYV) into rgb.
 * It assumes that the buffer has been imported twice - once as RG
 * texture with full size, and once as RGBA textures with half the
 * width (to account for the subsampling).
 */
void main() {
  vec4 y = GskTexture(u_source, vUv);
  vec4 uv = GskTexture(u_source2, vUv);

  vec4 yuv;

  yuv.x = y.x;
  yuv.yz = uv.yw;
  yuv.w = 1.0;

  gskSetOutputColor(yuv_to_rgb(yuv));
}
