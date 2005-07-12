/*
 * Copyright (C) 2000 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef PIXOPS_H
#define PIXOPS_H

#include <glib.h>



/* Interpolation modes; must match GdkInterpType */ 
typedef enum {
	PIXOPS_INTERP_NEAREST,
	PIXOPS_INTERP_TILES,
	PIXOPS_INTERP_BILINEAR,
	PIXOPS_INTERP_HYPER
} PixopsInterpType;

/* Scale src_buf from src_width / src_height by factors scale_x, scale_y
 * and composite the portion corresponding to
 * render_x, render_y, render_width, render_height in the new
 * coordinate system into dest_buf starting at 0, 0
 */
void _pixops_composite (guchar         *dest_buf,
			int             render_x0,
			int             render_y0,
			int             render_x1,
			int             render_y1,
			int             dest_rowstride,
			int             dest_channels,
			int             dest_has_alpha,
			const guchar   *src_buf,
			int             src_width,
			int             src_height,
			int             src_rowstride,
			int             src_channels,
			int             src_has_alpha,
			double          scale_x,
			double          scale_y,
			PixopsInterpType   interp_type,
			int             overall_alpha);

/* Scale src_buf from src_width / src_height by factors scale_x, scale_y
 * and composite the portion corresponding to
 * render_x, render_y, render_width, render_height in the new
 * coordinate system against a checkboard with checks of size check_size
 * of the colors color1 and color2 into dest_buf starting at 0, 0
 */
void _pixops_composite_color (guchar         *dest_buf,
			      int             render_x0,
			      int             render_y0,
			      int             render_x1,
			      int             render_y1,
			      int             dest_rowstride,
			      int             dest_channels,
			      int             dest_has_alpha,
			      const guchar   *src_buf,
			      int             src_width,
			      int             src_height,
			      int             src_rowstride,
			      int             src_channels,
			      int             src_has_alpha,
			      double          scale_x,
			      double          scale_y,
			      PixopsInterpType   interp_type,
			      int             overall_alpha,
			      int             check_x,
			      int             check_y,
			      int             check_size,
			      guint32         color1,
			      guint32         color2);

/* Scale src_buf from src_width / src_height by factors scale_x, scale_y
 * and composite the portion corresponding to
 * render_x, render_y, render_width, render_height in the new
 * coordinate system into dest_buf starting at 0, 0
 */
void _pixops_scale    (guchar         *dest_buf,
		       int             render_x0,
		       int             render_y0,
		       int             render_x1,
		       int             render_y1,
		       int             dest_rowstride,
		       int             dest_channels,
		       int             dest_has_alpha,
		       const guchar   *src_buf,
		       int             src_width,
		       int             src_height,
		       int             src_rowstride,
		       int             src_channels,
		       int             src_has_alpha,
		       double          scale_x,
		       double          scale_y,
		       PixopsInterpType   interp_type);



#endif
