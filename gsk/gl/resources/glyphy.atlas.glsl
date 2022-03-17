uniform ivec4 u_atlas_info;

#define GLYPHY_TEXTURE1D_EXTRA_DECLS , sampler2D _tex, ivec4 _atlas_info, ivec2 _atlas_pos
#define GLYPHY_TEXTURE1D_EXTRA_ARGS , _tex, _atlas_info, _atlas_pos
#define GLYPHY_DEMO_EXTRA_ARGS , u_source, u_atlas_info, gi.atlas_pos

vec4
glyphy_texture1D_func (int offset GLYPHY_TEXTURE1D_EXTRA_DECLS)
{
  ivec2 item_geom = _atlas_info.zw;
  vec2 pos = (vec2 (_atlas_pos.xy * item_geom +
		   ivec2 (mod (float (offset), float (item_geom.x)), offset / item_geom.x)) +
	      + vec2 (.5, .5)) / vec2(_atlas_info.xy);
  return GskTexture (_tex, pos);
}
