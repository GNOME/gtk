/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontextprivate.h: GL context abstraction
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

#ifndef __GDK_GL_CONTEXT_PRIVATE_H__
#define __GDK_GL_CONTEXT_PRIVATE_H__

#include "gdkglcontext.h"
#include "gdkdrawcontextprivate.h"
#include "gdkmemorytexture.h"

G_BEGIN_DECLS

#define GDK_GL_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_GL_CONTEXT, GdkGLContextClass))
#define GDK_IS_GL_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_GL_CONTEXT))
#define GDK_GL_CONTEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_GL_CONTEXT, GdkGLContextClass))

typedef struct _GdkGLContextClass       GdkGLContextClass;

struct _GdkGLContext
{
  GdkDrawContext parent_instance;

  /* We store the old drawn areas to support buffer-age optimizations */
  cairo_region_t *old_updated_area[2];
};

struct _GdkGLContextClass
{
  GdkDrawContextClass parent_class;

  gboolean (* realize) (GdkGLContext *context,
                        GError **error);

  cairo_region_t * (* get_damage) (GdkGLContext *context);
};

typedef struct {
  guint program;
  guint position_location;
  guint uv_location;
  guint map_location;
  guint flip_location;
} GdkGLContextProgram;

typedef struct {
  guint vertex_array_object;
  guint tmp_framebuffer;
  guint tmp_vertex_buffer;

  GdkGLContextProgram texture_2d_quad_program;
  GdkGLContextProgram texture_rect_quad_program;

  GdkGLContextProgram *current_program;

  guint is_legacy : 1;
  guint use_es : 1;
} GdkGLContextPaintData;

void                    gdk_gl_context_set_is_legacy            (GdkGLContext    *context,
                                                                 gboolean         is_legacy);

void                    gdk_gl_context_upload_texture           (GdkGLContext    *context,
                                                                 const guchar    *data,
                                                                 int              width,
                                                                 int              height,
                                                                 int              stride,
                                                                 GdkMemoryFormat  data_format,
                                                                 guint            texture_target);
GdkGLContextPaintData * gdk_gl_context_get_paint_data           (GdkGLContext    *context);
gboolean                gdk_gl_context_use_texture_rectangle    (GdkGLContext    *context);
gboolean                gdk_gl_context_has_unpack_subimage      (GdkGLContext    *context);
void                    gdk_gl_context_push_debug_group         (GdkGLContext    *context,
                                                                 const char      *message);
void                    gdk_gl_context_push_debug_group_printf  (GdkGLContext    *context,
                                                                 const char      *format,
                                                                 ...)  G_GNUC_PRINTF (2, 3);
void                    gdk_gl_context_pop_debug_group          (GdkGLContext    *context);
void                    gdk_gl_context_label_object             (GdkGLContext    *context,
                                                                 guint            identifier,
                                                                 guint            name,
                                                                 const char      *label);
void                    gdk_gl_context_label_object_printf      (GdkGLContext    *context,
                                                                 guint            identifier,
                                                                 guint            name,
                                                                 const char      *format,
                                                                ...)  G_GNUC_PRINTF (4, 5);

gboolean                gdk_gl_context_has_debug                (GdkGLContext    *self) G_GNUC_PURE;

gboolean                gdk_gl_context_use_es_bgra              (GdkGLContext    *context);

typedef struct {
  float x1, y1, x2, y2;
  float u1, v1, u2, v2;
} GdkTexturedQuad;

void                    gdk_gl_texture_quads                    (GdkGLContext    *context,
                                                                 guint            texture_target,
                                                                 int              n_quads,
                                                                 GdkTexturedQuad *quads,
                                                                 gboolean         flip_colors);


G_END_DECLS

#endif /* __GDK_GL_CONTEXT_PRIVATE_H__ */
