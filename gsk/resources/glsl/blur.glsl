// VERTEX_SHADER:
void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
uniform float u_blur_radius;
uniform vec2 u_blur_size;
uniform vec2 u_blur_dir;

const float PI = 3.14159265;
const float RADIUS_MULTIPLIER = 3.0;

// blur_radius 0 is NOT supported and MUST be caught before.

// Partially from http://callumhay.blogspot.com/2010/09/gaussian-blur-shader-glsl.html
void main() {
  float sigma = u_blur_radius; // *shrug*
  float blur_radius = u_blur_radius * RADIUS_MULTIPLIER;
  vec3 incrementalGaussian;
  incrementalGaussian.x = 1.0 / (sqrt(2.0 * PI) * sigma);
  incrementalGaussian.y = exp(-0.5 / (sigma * sigma));
  incrementalGaussian.z = incrementalGaussian.y * incrementalGaussian.y;

  vec2 pixel_step = vec2(1.0) / u_blur_size;

  float coefficientSum = 0.0;
  vec4 sum = Texture(u_source, vUv) * incrementalGaussian.x;
  coefficientSum += incrementalGaussian.x;
  incrementalGaussian.xy *= incrementalGaussian.yz;

  int pixels_per_side = int(floor(blur_radius / 2.0));
  for (int i = 1; i <= pixels_per_side; i++) {
    vec2 p = float(i) * pixel_step * u_blur_dir;

    sum += Texture(u_source, vUv - p) * incrementalGaussian.x;
    sum += Texture(u_source, vUv + p) * incrementalGaussian.x;

    coefficientSum += 2.0 * incrementalGaussian.x;
    incrementalGaussian.xy *= incrementalGaussian.yz;
  }

  setOutputColor(sum / coefficientSum);
}
