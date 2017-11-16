uniform vec4 uColorStops[8];
uniform float uColorOffsets[8];
uniform int uNumColorStops;
uniform vec2 uStartPoint;
uniform vec2 uEndPoint;

vec4 fragCoord() {
  vec4 f = gl_FragCoord;
  f.x += uViewport.x;
  f.y = (uViewport.y + uViewport.w) - f.y;
  return f;
}

void main() {
  vec2 startPoint = (vec4(uStartPoint, 0, 1) * uModelview).xy;
  vec2 endPoint   = (vec4(uEndPoint,   0, 1) * uModelview).xy;
  float maxDist   = length(endPoint - startPoint);

  // Position relative to startPoint
  vec2 pos = fragCoord().xy - startPoint;

  // Gradient direction
  vec2 gradient = endPoint - startPoint;
  float gradientLength = length(gradient);

  // Current pixel, projected onto the line between the start point and the end point
  // The projection will be relative to the start point!
  vec2 proj = (dot(gradient, pos) / (gradientLength * gradientLength)) * gradient;

  // Offset of the current pixel
  float offset = length(proj) / maxDist;

  vec4 color = uColorStops[0];
  for (int i = 1; i < uNumColorStops; i ++) {
    if (offset >= uColorOffsets[i - 1])  {
      float o = (offset - uColorOffsets[i - 1]) / (uColorOffsets[i] - uColorOffsets[i - 1]);
      color = mix(uColorStops[i - 1], uColorStops[i], o);
    }
  }

  /* Pre-multiply */
  color.rgb *= color.a;

  setOutputColor(color * uAlpha);
}
