/* gskglglyphylibrary.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/* Some of the glyphy cache is based upon the original glyphy code.
 * It's license is provided below.
 */
/*
 * Copyright 2012 Google, Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Google Author(s): Behdad Esfahbod
 */

#include "config.h"

#include <gdk/gdkglcontextprivate.h>
#include <gdk/gdkmemoryformatprivate.h>
#include <gdk/gdkprofilerprivate.h>

#include "gskglcommandqueueprivate.h"
#include "gskgldriverprivate.h"
#include "gskglglyphylibraryprivate.h"

#include "gskpath.h"

#include <glyphy.h>

#define TOLERANCE (1/2048.)
#define MIN_FONT_SIZE 10
#define ITEM_W 64
#define ITEM_H_QUANTUM 8

G_DEFINE_TYPE (GskGLGlyphyLibrary, gsk_gl_glyphy_library, GSK_TYPE_GL_TEXTURE_LIBRARY)

GskGLGlyphyLibrary *
gsk_gl_glyphy_library_new (GskGLDriver *driver)
{
  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), NULL);

  return g_object_new (GSK_TYPE_GL_GLYPHY_LIBRARY,
                       "driver", driver,
                       NULL);
}

static guint
gsk_gl_glyphy_key_hash (gconstpointer data)
{
  const GskGLGlyphyKey *key = data;

  /* malloc()'d pointers already guarantee 3 bits from the LSB on 64-bit and
   * 2 bits from the LSB on 32-bit. Shift by enough to give us 256 entries
   * in our front cache for the glyph since languages will naturally cluster
   * for us.
   */

  return (key->font << 8) ^ key->glyph;
}

static gboolean
gsk_gl_glyphy_key_equal (gconstpointer v1,
                         gconstpointer v2)
{
  return memcmp (v1, v2, sizeof (GskGLGlyphyKey)) == 0;
}

static void
gsk_gl_glyphy_key_free (gpointer data)
{
  GskGLGlyphyKey *key = data;

  g_slice_free (GskGLGlyphyKey, key);
}

static void
gsk_gl_glyphy_value_free (gpointer data)
{
  g_slice_free (GskGLGlyphyValue, data);
}

static void
gsk_gl_glyphy_library_clear_cache (GskGLTextureLibrary *library)
{
  GskGLGlyphyLibrary *self = (GskGLGlyphyLibrary *)library;

  g_assert (GSK_IS_GL_GLYPHY_LIBRARY (self));

  memset (self->front, 0, sizeof self->front);
}

static void
gsk_gl_glyphy_library_init_atlas (GskGLTextureLibrary *library,
                                  GskGLTextureAtlas   *atlas)
{
  g_assert (GSK_IS_GL_GLYPHY_LIBRARY (library));
  g_assert (atlas != NULL);

  atlas->cursor_x = 0;
  atlas->cursor_y = 0;
}

static gboolean
gsk_gl_glyphy_library_allocate (GskGLTextureLibrary *library,
                                GskGLTextureAtlas   *atlas,
                                int                  width,
                                int                  height,
                                int                 *out_x,
                                int                 *out_y)
{
  GskGLGlyphyLibrary *self = (GskGLGlyphyLibrary *)library;
  int cursor_save_x;
  int cursor_save_y;

  g_assert (GSK_IS_GL_GLYPHY_LIBRARY (self));
  g_assert (atlas != NULL);

  cursor_save_x = atlas->cursor_x;
  cursor_save_y = atlas->cursor_y;

  if ((height & (self->item_h_q-1)) != 0)
    height = (height + self->item_h_q - 1) & ~(self->item_h_q - 1);

  /* Require allocations in columns of 64 and rows of 8 */
  g_assert (width == self->item_w);
  g_assert ((height % self->item_h_q) == 0);

  if (atlas->cursor_y + height > atlas->height)
    {
      /* Go to next column */
      atlas->cursor_x += self->item_w;
      atlas->cursor_y = 0;
    }

  if (atlas->cursor_x + width <= atlas->width &&
      atlas->cursor_y + height <= atlas->height)
    {
      *out_x = atlas->cursor_x;
      *out_y = atlas->cursor_y;
      atlas->cursor_y += height;
      return TRUE;
    }

  atlas->cursor_x = cursor_save_x;
  atlas->cursor_y = cursor_save_y;

  return FALSE;
}

static void
gsk_gl_glyphy_library_finalize (GObject *object)
{
  GskGLGlyphyLibrary *self = (GskGLGlyphyLibrary *)object;

  g_clear_pointer (&self->acc, glyphy_arc_accumulator_destroy);
  g_clear_pointer (&self->acc_endpoints, g_array_unref);

  G_OBJECT_CLASS (gsk_gl_glyphy_library_parent_class)->finalize (object);
}

GQuark quark_glyphy_font_key;

static void
gsk_gl_glyphy_library_class_init (GskGLGlyphyLibraryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GskGLTextureLibraryClass *library_class = GSK_GL_TEXTURE_LIBRARY_CLASS (klass);

  quark_glyphy_font_key = g_quark_from_static_string ("glyphy-font-key");

  object_class->finalize = gsk_gl_glyphy_library_finalize;

  library_class->allocate = gsk_gl_glyphy_library_allocate;
  library_class->clear_cache = gsk_gl_glyphy_library_clear_cache;
  library_class->init_atlas = gsk_gl_glyphy_library_init_atlas;
}

static void
gsk_gl_glyphy_library_init (GskGLGlyphyLibrary *self)
{
  GskGLTextureLibrary *tl = (GskGLTextureLibrary *)self;

  tl->max_entry_size = 0;
  tl->max_frame_age = 512;
  tl->atlas_width = 2048;
  tl->atlas_height = 1024;
  gsk_gl_texture_library_set_funcs (tl,
                                    gsk_gl_glyphy_key_hash,
                                    gsk_gl_glyphy_key_equal,
                                    gsk_gl_glyphy_key_free,
                                    gsk_gl_glyphy_value_free);

  self->acc = glyphy_arc_accumulator_create ();
  self->acc_endpoints = g_array_new (FALSE, FALSE, sizeof (glyphy_arc_endpoint_t));
  self->item_w = ITEM_W;
  self->item_h_q = ITEM_H_QUANTUM;
}

static glyphy_bool_t
accumulate_endpoint (glyphy_arc_endpoint_t *endpoint,
                     GArray                *endpoints)
{
  g_array_append_vals (endpoints, endpoint, 1);
  return TRUE;
}

static void
move_to (hb_draw_funcs_t *dfuncs,
         GskPathBuilder  *builder,
         hb_draw_state_t *st,
         float            x,
         float            y,
         void            *data)
{
  gsk_path_builder_move_to (builder, x, y);
}

static void
line_to (hb_draw_funcs_t *dfuncs,
         GskPathBuilder  *builder,
         hb_draw_state_t *st,
         float            x,
         float            y,
         void            *data)
{
  gsk_path_builder_line_to (builder, x, y);
}

static void
cubic_to (hb_draw_funcs_t *dfuncs,
          GskPathBuilder  *builder,
          hb_draw_state_t *st,
          float            x1,
          float            y1,
          float            x2,
          float            y2,
          float            x3,
          float            y3,
          void            *data)
{
  gsk_path_builder_curve_to (builder, x1, y1, x2, y2, x3, y3);
}

static void
close_path (hb_draw_funcs_t *dfuncs,
            GskPathBuilder  *builder,
            hb_draw_state_t *st,
            void            *data)
{
  gsk_path_builder_close (builder);
}

static hb_draw_funcs_t *
gsk_path_get_draw_funcs (void)
{
  static hb_draw_funcs_t *funcs = NULL;

  if (!funcs)
    {
      funcs = hb_draw_funcs_create ();

      hb_draw_funcs_set_move_to_func (funcs, (hb_draw_move_to_func_t) move_to, NULL, NULL);
      hb_draw_funcs_set_line_to_func (funcs, (hb_draw_line_to_func_t) line_to, NULL, NULL);
      hb_draw_funcs_set_cubic_to_func (funcs, (hb_draw_cubic_to_func_t) cubic_to, NULL, NULL);
      hb_draw_funcs_set_close_path_func (funcs, (hb_draw_close_path_func_t) close_path, NULL, NULL);

      hb_draw_funcs_make_immutable (funcs);
    }

  return funcs;
}

static gboolean
acc_callback (GskPathOperation        op,
              const graphene_point_t *pts,
              gsize                   n_pts,
              float                   weight,
              gpointer                user_data)
{
  glyphy_arc_accumulator_t *acc = user_data;
  glyphy_point_t p0, p1, p2, p3;

  switch (op)
    {
    case GSK_PATH_MOVE:
      p0.x = pts[0].x; p0.y = pts[0].y;
      glyphy_arc_accumulator_move_to (acc, &p0);
      break;
    case GSK_PATH_CLOSE:
      glyphy_arc_accumulator_close_path (acc);
      break;
    case GSK_PATH_LINE:
      p1.x = pts[1].x; p1.y = pts[1].y;
      glyphy_arc_accumulator_line_to (acc, &p1);
      break;
    case GSK_PATH_CURVE:
      p1.x = pts[1].x; p1.y = pts[1].y;
      p2.x = pts[2].x; p2.y = pts[2].y;
      p3.x = pts[3].x; p3.y = pts[3].y;
      glyphy_arc_accumulator_cubic_to (acc, &p1, &p2, &p3);
      break;
    case GSK_PATH_CONIC:
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static inline gboolean
encode_glyph (GskGLGlyphyLibrary *self,
              hb_font_t          *font,
              unsigned int        glyph_index,
              double              tolerance_per_em,
              glyphy_rgba_t      *buffer,
              guint               buffer_len,
              guint              *output_len,
              guint              *nominal_width,
              guint              *nominal_height,
              glyphy_extents_t   *extents)
{
  hb_face_t *face = hb_font_get_face (font);
  guint upem = hb_face_get_upem (face);
  double tolerance = upem * tolerance_per_em;
  double faraway = (double)upem / (MIN_FONT_SIZE * M_SQRT2);
  double avg_fetch_achieved;
  GskPathBuilder *builder;
  GskPath *path, *simplified;

  self->acc_endpoints->len = 0;

  glyphy_arc_accumulator_reset (self->acc);
  glyphy_arc_accumulator_set_tolerance (self->acc, tolerance);
  glyphy_arc_accumulator_set_callback (self->acc,
                                       (glyphy_arc_endpoint_accumulator_callback_t)accumulate_endpoint,
                                       self->acc_endpoints);

  builder = gsk_path_builder_new ();
  hb_font_get_glyph_shape (font, glyph_index, gsk_path_get_draw_funcs (), builder);
  path = gsk_path_builder_free_to_path (builder);
  simplified = gsk_path_simplify (path);

  gsk_path_foreach (simplified, GSK_PATH_FOREACH_ALLOW_CURVE, acc_callback, self->acc);
  gsk_path_unref (simplified);
  gsk_path_unref (path);

  if (!glyphy_arc_accumulator_successful (self->acc))
    return FALSE;

  g_assert (glyphy_arc_accumulator_get_error (self->acc) <= tolerance);

  if (self->acc_endpoints->len > 0)
    glyphy_outline_winding_from_even_odd ((gpointer)self->acc_endpoints->data,
                                          self->acc_endpoints->len,
                                          FALSE);

  if (!glyphy_arc_list_encode_blob ((gpointer)self->acc_endpoints->data,
                                    self->acc_endpoints->len,
                                    buffer,
                                    buffer_len,
                                    faraway,
                                    4, /* UNUSED */
                                    &avg_fetch_achieved,
                                    output_len,
                                    nominal_width,
                                    nominal_height,
                                    extents))
    return FALSE;

  glyphy_extents_scale (extents, 1./upem, 1./upem);

  return TRUE;
}

static inline hb_font_t *
get_nominal_size_hb_font (PangoFont *font)
{
  hb_font_t *hbfont;
  const float *coords;
  unsigned int length;

  hbfont = (hb_font_t *) g_object_get_data ((GObject *)font, "glyph-nominal-size-font");
  if (hbfont == NULL)
    {
      hbfont = hb_font_create (hb_font_get_face (pango_font_get_hb_font (font)));
      coords = hb_font_get_var_coords_design (pango_font_get_hb_font (font), &length);
      if (length > 0)
        hb_font_set_var_coords_design (hbfont, coords, length);

      g_object_set_data_full ((GObject *)font, "glyphy-nominal-size-font",
                              hbfont, (GDestroyNotify)hb_font_destroy);
    }

  return hbfont;
}

gboolean
gsk_gl_glyphy_library_add (GskGLGlyphyLibrary      *self,
                           GskGLGlyphyKey          *key,
                           PangoFont               *font,
                           const GskGLGlyphyValue **out_value)
{
  static glyphy_rgba_t buffer[4096 * 16];
  GskGLTextureLibrary *tl = (GskGLTextureLibrary *)self;
  GskGLGlyphyValue *value;
  glyphy_extents_t extents;
  hb_font_t *hbfont;
  guint packed_x;
  guint packed_y;
  guint nominal_w, nominal_h;
  guint output_len = 0;
  guint texture_id;
  guint width, height;

  g_assert (GSK_IS_GL_GLYPHY_LIBRARY (self));
  g_assert (key != NULL);
  g_assert (font != NULL);
  g_assert (out_value != NULL);

  hbfont = get_nominal_size_hb_font (font);

  /* Convert the glyph to a list of arcs */
  if (!encode_glyph (self, hbfont, key->glyph, TOLERANCE,
                     buffer, sizeof buffer, &output_len,
                     &nominal_w, &nominal_h, &extents))
    return FALSE;

  /* Allocate space for list within atlas */
  width = self->item_w;
  height = (output_len + width - 1) / width;
  value = gsk_gl_texture_library_pack (tl, key, sizeof *value,
                                       width, height, 0,
                                       &packed_x, &packed_y);

  g_assert (packed_x % ITEM_W == 0);
  g_assert (packed_y % ITEM_H_QUANTUM == 0);

  /* Make sure we found space to pack */
  texture_id = GSK_GL_TEXTURE_ATLAS_ENTRY_TEXTURE (value);
  if (texture_id == 0)
    return FALSE;

  if (!glyphy_extents_is_empty (&extents))
    {
      /* Connect the texture for data upload */
      glActiveTexture (GL_TEXTURE0);
      glBindTexture (GL_TEXTURE_2D, texture_id);

      g_assert (width > 0);
      g_assert (height > 0);

      /* Upload the arc list */
      if (width * height == output_len)
        {
          glTexSubImage2D (GL_TEXTURE_2D, 0,
                           packed_x, packed_y,
                           width, height,
                           GL_RGBA, GL_UNSIGNED_BYTE,
                           buffer);
        }
      else
        {
          glTexSubImage2D (GL_TEXTURE_2D, 0,
                           packed_x, packed_y,
                           width, height - 1,
                           GL_RGBA, GL_UNSIGNED_BYTE,
                           buffer);
          /* Upload the last row separately */
          glTexSubImage2D (GL_TEXTURE_2D, 0,
                           packed_x, packed_y + height - 1,
                           output_len - (width * (height - 1)), 1,
                           GL_RGBA, GL_UNSIGNED_BYTE,
                           buffer + (width * (height - 1)));
        }
    }

  value->extents.min_x = extents.min_x;
  value->extents.min_y = extents.min_y;
  value->extents.max_x = extents.max_x;
  value->extents.max_y = extents.max_y;
  value->nominal_w = nominal_w;
  value->nominal_h = nominal_h;
  value->atlas_x = packed_x / self->item_w;
  value->atlas_y = packed_y / self->item_h_q;

  *out_value = value;

  return TRUE;
}
