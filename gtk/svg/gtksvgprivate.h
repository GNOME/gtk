/*
 * Copyright © 2025 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include "gtksvg.h"
#include "gtkenums.h"
#include "gtkbitmaskprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkeventcontrollerprivate.h"
#include "gtk/svg/gtksvgtypesprivate.h"
#include "gtk/svg/gtksvgenumsprivate.h"
#include "gtk/svg/gtksvgelementtypeprivate.h"
#include "gtk/svg/gtksvgfiltertypeprivate.h"
#include "gtk/svg/gtksvgpropertyprivate.h"

G_BEGIN_DECLS

#define DEFAULT_FONT_SIZE 13.333

/* Max. nesting level of paint calls we allow */
#define NESTING_LIMIT 256

/* This is a mitigation for SVG files which create millions of elements
 * in an attempt to exhaust memory.  We don't allow loading more than
 * this number of elements during the initial streaming load process.
 */
#define LOADING_LIMIT 50000

/* This is a mitigation for SVG files which create deeply nested
 * <use> elements or deeply nested references of patterns.
 */
#define DRAWING_LIMIT 150000


typedef void (* SvgElementCallback) (SvgElement *element,
                                     gpointer    data);

struct _GtkSvg
{
  GObject parent_instance;
  SvgElement *content;

  double current_width, current_height; /* Last snapshot size */

  double width, height, aspect_ratio; /* Paintable properties */

  double weight;
  unsigned int initial_state;
  unsigned int state;
  unsigned int max_state;
  unsigned int n_state_names;
  char **state_names;
  int64_t state_change_delay;
  gboolean has_animations;
  GtkSvgFeatures features;
  GtkSvgUses used;
  gboolean has_use_cycle;

  GBytes *stylesheet;

  GArray *user_styles;
  GArray *author_styles;
  gboolean style_changed;
  gboolean view_changed;

  SvgElement *view;
  SvgElement *focus;
  SvgElement *initial_focus;
  SvgElement *hover;
  SvgElement *active;

  SvgElementCallback hover_callback;
  gpointer hover_data;
  SvgElementCallback activate_callback;
  gpointer activate_data;

  char *resource;

  int64_t load_time;
  int64_t current_time;
  int64_t pause_time;

  GtkOverflow overflow;
  gboolean playing;
  GtkSvgRunMode run_mode;
  GdkFrameClock *clock;
  unsigned long clock_update_id;

  int64_t next_update;
  unsigned int pending_advance;
  gboolean advance_after_snapshot;

  unsigned int gpa_version;
  char *author;
  char *license;
  char *description;
  char *keywords;

  Timeline *timeline;

  GHashTable *images;

  PangoFontMap *fontmap;
  GPtrArray *font_files;

  GskRenderNode *node;

  struct {
    double width, height;
    GdkRGBA colors[5];
    size_t n_colors;
    double weight;
    int64_t time;
    unsigned int state;
  } node_for;
};

/* --- */

gboolean       gtk_svg_set_state_names (GtkSvg                *svg,
                                        const char           **names);

GtkSvg *       gtk_svg_copy            (GtkSvg                *orig);

gboolean       gtk_svg_equal           (GtkSvg                *svg1,
                                        GtkSvg                *svg2);

void           gtk_svg_set_load_time   (GtkSvg                *self,
                                        int64_t                load_time);

void           gtk_svg_set_playing     (GtkSvg                *self,
                                        gboolean               playing);

void           gtk_svg_clear_content   (GtkSvg                *self);

void           gtk_svg_advance         (GtkSvg                *self,
                                        int64_t                current_time);

void           gtk_svg_advance_after_snapshot
                                       (GtkSvg                *self);

GtkSvgRunMode  gtk_svg_get_run_mode    (GtkSvg                *self);

int64_t        gtk_svg_get_next_update (GtkSvg                *self);

void           gtk_svg_set_activate_callback (GtkSvg             *svg,
                                              SvgElementCallback  callback,
                                              gpointer            data);
void           gtk_svg_set_hover_callback    (GtkSvg                *svg,
                                              SvgElementCallback     callback,
                                              gpointer               data);

void           gtk_svg_set_view              (GtkSvg               *self,
                                              SvgElement           *view);

gboolean       gtk_svg_handle_event    (GtkSvg                *svg,
                                        GdkEvent              *event,
                                        double                 x,
                                        double                 y);
void           gtk_svg_handle_crossing (GtkSvg                *svg,
                                        const GtkCrossingData *crossing,
                                        double                 x,
                                        double                 y);

gboolean       gtk_svg_grab_focus      (GtkSvg                *svg);
gboolean       gtk_svg_lose_focus      (GtkSvg                *svg);
gboolean       gtk_svg_move_focus      (GtkSvg                *svg,
                                        GtkDirectionType       direction);

void           gtk_svg_activate_element (GtkSvg               *svg,
                                         SvgElement           *element);

void           gtk_svg_set_active       (GtkSvg               *svg,
                                         SvgElement           *element);
SvgElement *   gtk_svg_get_active       (GtkSvg               *svg);

void           gtk_svg_emit_error        (GtkSvg              *svg,
                                          const GError        *error);
void           gtk_svg_invalid_reference (GtkSvg              *self,
                                          const char          *format,
                                          ...) G_GNUC_PRINTF (2, 3);
void           gtk_svg_invalid_attribute (GtkSvg              *self,
                                          GMarkupParseContext *context,
                                          const char         **attr_names,
                                          const char          *attr_name,
                                          const char          *format,
                                          ...) G_GNUC_PRINTF (5, 6);
void           gtk_svg_rendering_error   (GtkSvg              *self,
                                          const char          *format,
                                          ...) G_GNUC_PRINTF (2, 3);
void           gtk_svg_update_error      (GtkSvg              *self,
                                          const char          *format,
                                          ...) G_GNUC_PRINTF (2, 3);

void           ensure_fontmap            (GtkSvg              *svg);
void           apply_view                (SvgElement          *content,
                                          SvgElement          *view);

gboolean       add_font_from_url         (GtkSvg               *svg,
                                          GMarkupParseContext  *context,
                                          const char          **attr_names,
                                          const char           *attr_name,
                                          const char           *url);

GdkTexture *   get_texture               (GtkSvg               *svg,
                                          const char           *string,
                                          GError              **error);

void           compute_current_values_for_shape
                                         (SvgElement        *shape,
                                          SvgComputeContext *context);

G_END_DECLS
