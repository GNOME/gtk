uniform float u_blur_radius;// = 40.0;
uniform vec2 u_blur_size;// = vec2(393, 393);

float Gaussian (float sigma, float x) {
  return exp ( - (x * x) / (2.0 * sigma * sigma));
}

vec4 blur_pixel (in vec2 uv) {
  float total = 0.0;
  vec4 ret = vec4 (0);
  float pixel_size_x = (1.0 / u_blur_size.x);
  float pixel_size_y = (1.0 / u_blur_size.y);

  // XXX The magic value here is GAUSSIAN_SCALE_FACTOR from gskcairoblur.c
  float radius = u_blur_radius  * 2.30348;

  int half_radius = max(int(radius / 2.0), 1);

  for (int y = -half_radius; y < half_radius; y ++) {
    float fy = Gaussian (radius / 2.0, float(y));
    float offset_y = float(y) * pixel_size_y;

    for (int x = -half_radius; x < half_radius; x ++) {
      float fx = Gaussian (radius / 2.0, float(x));
      float offset_x = float(x) * pixel_size_x;

      vec4 c = Texture(u_source, uv + vec2(offset_x, offset_y));
      total += fx * fy;
      ret += c * fx * fy;
    }
  }

  return ret / total;
}

void main() {
  vec4 color = blur_pixel(vUv);
  setOutputColor(color);
}
