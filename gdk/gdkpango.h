/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc. 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_PANGO_H__
#define __GDK_PANGO_H__

#include <gdk/gdktypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Pango interaction */

/* FIXME: The following function needs more parameters so that we can
 * properly deal with different visuals, the potential for multiple
 * screens in the future, etc. The PangoContext needs enough information
 * in it to create new GC's and to set the colors on those GC's.
 * A colormap is not sufficient.
 */
PangoContext *gdk_pango_context_get          (void);
void          gdk_pango_context_set_colormap (PangoContext *context,
					      GdkColormap  *colormap);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_FONT_H__ */
