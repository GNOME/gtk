#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_filterlevel.h>

/* Scale src_buf from src_width / src_height by factors scale_x, scale_y
 * and composite the portion corresponding to
 * render_x, render_y, render_width, render_height in the new
 * coordinate system into dest_buf starting at 0, 0
 */
void pixops_composite (art_u8         *dest_buf,
		       int             render_x0,
		       int             render_y0,
		       int             render_x1,
		       int             render_y1,
		       int             dest_rowstride,
		       int             dest_channels,
		       int             dest_has_alpha,
		       const art_u8   *src_buf,
		       int             src_width,
		       int             src_height,
		       int             src_rowstride,
		       int             src_channels,
		       int             src_has_alpha,
		       double          scale_x,
		       double          scale_y,
		       ArtFilterLevel  filter_level,
		       int             overall_alpha);

/* Scale src_buf from src_width / src_height by factors scale_x, scale_y
 * and composite the portion corresponding to
 * render_x, render_y, render_width, render_height in the new
 * coordinate system against a checkboard with checks of size check_size
 * of the colors color1 and color2 into dest_buf starting at 0, 0
 */
void pixops_composite_color (art_u8         *dest_buf,
			     int             render_x0,
			     int             render_y0,
			     int             render_x1,
			     int             render_y1,
			     int             dest_rowstride,
			     int             dest_channels,
			     int             dest_has_alpha,
			     const art_u8   *src_buf,
			     int             src_width,
			     int             src_height,
			     int             src_rowstride,
			     int             src_channels,
			     int             src_has_alpha,
			     double          scale_x,
			     double          scale_y,
			     ArtFilterLevel  filter_level,
			     int             overall_alpha,
			     int             check_x,
			     int             check_y,
			     int             check_size,
			     art_u32         color1,
			     art_u32         color2);

/* Scale src_buf from src_width / src_height by factors scale_x, scale_y
 * and composite the portion corresponding to
 * render_x, render_y, render_width, render_height in the new
 * coordinate system into dest_buf starting at 0, 0
 */
void pixops_scale     (art_u8         *dest_buf,
		       int             render_x0,
		       int             render_y0,
		       int             render_x1,
		       int             render_y1,
		       int             dest_rowstride,
		       int             dest_channels,
		       int             dest_has_alpha,
		       const art_u8   *src_buf,
		       int             src_width,
		       int             src_height,
		       int             src_rowstride,
		       int             src_channels,
		       int             src_has_alpha,
		       double          scale_x,
		       double          scale_y,
		       ArtFilterLevel  filter_level);

