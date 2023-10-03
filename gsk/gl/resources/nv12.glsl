// VERTEX_SHADER:
// nv12.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// nv12.glsl

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

/* This shader converts 2-plane yuv (DRM_FORMAT_NV12 or DRM_FORMAT_P010)
 * into rgb. It assumes that the two planes have been imported separately,
 *  the first one as R texture, the second one as RG.
 */

void main() {
  vec4 y = GskTexture(u_source, vUv);
  vec4 uv = GskTexture(u_source2, vUv);

  vec4 yuv;

  yuv.x = y.x;
  yuv.yz = uv.xy;
  yuv.w = 1.0;

  gskSetOutputColor(yuv_to_rgb(yuv));
}
