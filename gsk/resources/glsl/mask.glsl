// VERTEX_SHADER:
void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
uniform sampler2D u_mask;
uniform vec4 u_texture_rect;

void main() {
  vec4 source = GskTexture(u_source, vUv);
  vec2 tc;

  if (vUv.x < u_texture_rect.x || vUv.x > u_texture_rect.z)
    {
      gskSetOutputColor(vec4 (0, 0, 0, 0));
      return;
    }

  tc.x = (vUv.x - u_texture_rect.x) / (u_texture_rect.z - u_texture_rect.x);

  if (u_texture_rect.y <= u_texture_rect.w)
    {
      if (vUv.y < u_texture_rect.y || vUv.y > u_texture_rect.w)
        {
          gskSetOutputColor(vec4 (0, 0, 0, 0));
          return;
        }
      tc.y = (vUv.y - u_texture_rect.y) / (u_texture_rect.w - u_texture_rect.y);
    }
  else
    {
      if (vUv.y < u_texture_rect.w || vUv.y > u_texture_rect.y)
        {
          gskSetOutputColor(vec4 (0, 0, 0, 0));
          return;
        }
      tc.y =  1.0 - (vUv.y - u_texture_rect.w) / (u_texture_rect.y - u_texture_rect.w);
    }

  vec4 mask = GskTexture(u_mask, tc);
  gskSetOutputColor(vec4 (source * mask.a));
}
