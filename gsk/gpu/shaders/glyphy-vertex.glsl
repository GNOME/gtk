/*
 * Copyright 2026 Behdad Esfahbod.
 * Copyright 2017 Eric Lengyel.
 * Based on the Slug algorithm by Eric Lengyel:
 * https://github.com/EricLengyel/Slug
 */


/* Requires GLSL 3.30 */


/* Dilate a glyph quad vertex by half a pixel on screen.
 *
 * position:  object-space vertex position (modified in place)
 * texcoord:  em-space sample coordinates (modified in place)
 * corner:    corner direction, each component -1 or +1
 * texPerPos: ratio of texcoord units to position units (e.g. upem / font_size)
 * mvp:       model-view-projection matrix
 * viewport:  viewport size in pixels
 */
void glyphy_dilate (inout vec2 position, inout vec2 texcoord,
			 vec2 corner, vec2 texPerPos,
			 mat4 mvp, vec2 viewport)
{
  vec4 clipPos = mvp * vec4 (position, 0.0, 1.0);

  /* Dilate by half a pixel in NDC. Under perspective, ndc = clip.xy / clip.w,
   * so the object-space delta must be computed from the projective Jacobian,
   * not just the affine clip-space transform. */
  float invW = 1.0 / clipPos.w;
  vec2 deltaNdc = corner / viewport;

  vec2 dNdcDx = (mvp[0].xy * clipPos.w - clipPos.xy * mvp[0].w) * (invW * invW);
  vec2 dNdcDy = (mvp[1].xy * clipPos.w - clipPos.xy * mvp[1].w) * (invW * invW);
  mat2 ndcJacobian = mat2 (dNdcDx, dNdcDy);

  float det = dNdcDx.x * dNdcDy.y - dNdcDx.y * dNdcDy.x;
  vec2 dPos = abs (det) > 1.0 / 16777216.0
	    ? inverse (ndcJacobian) * deltaNdc
	    : vec2 (0.0);

  position += dPos;
  texcoord += dPos * texPerPos;
}
