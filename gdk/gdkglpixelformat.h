/* GDK - The GIMP Drawing Kit
 *
 * gdkglpixelformat.h: GL pixel formats
 * 
 * Copyright © 2014  Emmanuele Bassi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_GL_PIXEL_FORMAT_H__
#define __GDK_GL_PIXEL_FORMAT_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_GL_PIXEL_FORMAT        (gdk_gl_pixel_format_get_type ())
#define GDK_GL_PIXEL_FORMAT(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_GL_PIXEL_FORMAT, GdkGLPixelFormat))
#define GDK_IS_GL_PIXEL_FORMAT(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_GL_PIXEL_FORMAT))

#define GDK_GL_PIXEL_FORMAT_ERROR       (gdk_gl_pixel_format_error_quark ())

typedef struct _GdkGLPixelFormatClass   GdkGLPixelFormatClass;

GDK_AVAILABLE_IN_3_14
GType gdk_gl_pixel_format_get_type (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_3_14
GQuark gdk_gl_pixel_format_error_quark (void);

GDK_AVAILABLE_IN_3_14
GdkGLPixelFormat *      gdk_gl_pixel_format_new                 (const char *first_property,
                                                                 ...);

GDK_AVAILABLE_IN_3_14
gboolean                gdk_gl_pixel_format_get_double_buffer   (GdkGLPixelFormat *format);
GDK_AVAILABLE_IN_3_14
gboolean                gdk_gl_pixel_format_get_multi_sample    (GdkGLPixelFormat *format);
GDK_AVAILABLE_IN_3_14
gboolean                gdk_gl_pixel_format_get_stereo          (GdkGLPixelFormat *format);
GDK_AVAILABLE_IN_3_14
gint                    gdk_gl_pixel_format_get_color_size      (GdkGLPixelFormat *format);
GDK_AVAILABLE_IN_3_14
gint                    gdk_gl_pixel_format_get_alpha_size      (GdkGLPixelFormat *format);
GDK_AVAILABLE_IN_3_14
gint                    gdk_gl_pixel_format_get_depth_size      (GdkGLPixelFormat *format);
GDK_AVAILABLE_IN_3_14
gint                    gdk_gl_pixel_format_get_stencil_size    (GdkGLPixelFormat *format);
GDK_AVAILABLE_IN_3_14
gint                    gdk_gl_pixel_format_get_accum_size      (GdkGLPixelFormat *format);
GDK_AVAILABLE_IN_3_14
gint                    gdk_gl_pixel_format_get_aux_buffers     (GdkGLPixelFormat *format);
GDK_AVAILABLE_IN_3_14
gint                    gdk_gl_pixel_format_get_sample_buffers  (GdkGLPixelFormat *format);
GDK_AVAILABLE_IN_3_14
gint                    gdk_gl_pixel_format_get_samples         (GdkGLPixelFormat *format);

G_END_DECLS

#endif /* __GDK_GL_PIXEL_FORMAT_H__ */
