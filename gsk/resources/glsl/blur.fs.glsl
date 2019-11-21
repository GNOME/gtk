uniform float u_blur_radius;
uniform vec2 u_blur_size;
uniform vec2 u_blur_dir;

const float PI = 3.141;

// Partially from http://callumhay.blogspot.com/2010/09/gaussian-blur-shader-glsl.html
void main() {
  float sigma = u_blur_radius / 2.0;
  vec3 incrementalGaussian;
  incrementalGaussian.x = 1.0 / (sqrt(2.0 * PI) * sigma);
  incrementalGaussian.y = exp(-0.5 / (sigma * sigma));
  incrementalGaussian.z = incrementalGaussian.y * incrementalGaussian.y;

  vec2 blur = vec2(1.0) / u_blur_size;
  float coefficientSum = 0;

  vec4 sum = Texture(u_source, vUv) * incrementalGaussian.x;
  coefficientSum += incrementalGaussian.x;
  incrementalGaussian.xy *= incrementalGaussian.yz;

  int pixels_per_side = int(floor(u_blur_radius / 2.0));
  for (int i = 1; i <= pixels_per_side; i++) {
    sum += Texture(u_source, vUv.xy - i * blur * u_blur_dir) * incrementalGaussian.x;
    sum += Texture(u_source, vUv.xy + i * blur * u_blur_dir) * incrementalGaussian.x;

    coefficientSum += 2 * incrementalGaussian.x;
    incrementalGaussian.xy *= incrementalGaussian.yz;
  }

  sum /= coefficientSum;

  setOutputColor(sum);
}
