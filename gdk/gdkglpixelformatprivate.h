/* GDK - The GIMP Drawing Kit
 *
 * gdkglpixelformatprivate.h: GL pixel formats private API
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

#ifndef __GDK_GL_PIXEL_FORMAT_PRIVATE_H__
#define __GDK_GL_PIXEL_FORMAT_PRIVATE_H__

#include "gdkglpixelformat.h"

G_BEGIN_DECLS

#define GDK_GL_PIXEL_FORMAT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_GL_PIXEL_FORMAT, GdkGLPixelFormatClass))
#define GDK_IS_GL_PIXEL_FORMAT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_GL_PIXEL_FORMAT))
#define GDK_GL_PIXEL_FORMAT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_GL_PIXEL_FORMAT, GdkGLPixelFormatClass))

struct _GdkGLPixelFormat
{
  GObject parent_instance;

  gboolean double_buffer;
  gboolean multi_sample;
  gboolean stereo;

  int aux_buffers;
  int color_size;
  int alpha_size;
  int depth_size;
  int stencil_size;
  int accum_size;
  int sample_buffers;
  int samples;

  GdkGLPixelFormatProfile profile;
};

struct _GdkGLPixelFormatClass
{
  GObjectClass parent_class;
};

G_END_DECLS

#endif /* __GDK_GL_PIXEL_FORMAT_PRIVATE_H__ */
