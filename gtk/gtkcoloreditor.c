/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
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

/* TODO
 * - custom sliders
 * - pop-up entries
 */

#include "config.h"

#include "gtkcolorchooserprivate.h"
#include "gtkcoloreditor.h"
#include "gtkcolorplane.h"
#include "gtkgrid.h"
#include "gtkscale.h"
#include "gtkaspectframe.h"
#include "gtkdrawingarea.h"
#include "gtkentry.h"
#include "gtkhsv.h"
#include "gtkadjustment.h"
#include "gtkintl.h"

#include <math.h>

struct _GtkColorEditorPrivate
{
  GtkWidget *grid;
  GtkWidget *swatch;
  GtkWidget *entry;
  GtkWidget *h_slider;
  GtkWidget *sv_plane;
  GtkWidget *a_slider;

  GtkAdjustment *h_adj;
  GtkAdjustment *a_adj;
  GdkRGBA color;
  gdouble h, s, v;
  guint text_changed : 1;
  guint show_alpha   : 1;
};

enum
{
  PROP_ZERO,
  PROP_COLOR,
  PROP_SHOW_ALPHA
};

static void gtk_color_editor_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorEditor, gtk_color_editor, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_editor_iface_init))

static guint
scale_round (gdouble value, gdouble scale)
{
  value = floor (value * scale + 0.5);
  value = MAX (value, 0);
  value = MIN (value, scale);
  return (guint)value;
}

static void
update_entry (GtkColorEditor *editor)
{
  gchar *text;

  text = g_strdup_printf ("#%02X%02X%02X",
                          scale_round (editor->priv->color.red, 255),
                          scale_round (editor->priv->color.green, 255),
                          scale_round (editor->priv->color.blue, 255));
  gtk_entry_set_text (GTK_ENTRY (editor->priv->entry), text);
  editor->priv->text_changed = FALSE;
  g_free (text);
}

static void
entry_apply (GtkWidget      *entry,
             GtkColorEditor *editor)
{
  GdkRGBA color;
  gchar *text;

  if (!editor->priv->text_changed)
    return;

  text = gtk_editable_get_chars (GTK_EDITABLE (editor->priv->entry), 0, -1);
  if (gdk_rgba_parse (&color, text))
    {
      color.alpha = editor->priv->color.alpha;
      gtk_color_chooser_set_color (GTK_COLOR_CHOOSER (editor), &color);
    }

  editor->priv->text_changed = FALSE;

  g_free (text);
}

static gboolean
entry_focus_out (GtkWidget      *entry,
                 GdkEventFocus  *event,
                 GtkColorEditor *editor)
{
  entry_apply (entry, editor);
  return FALSE;
}

static void
entry_text_changed (GtkWidget      *entry,
                    GParamSpec     *pspec,
                    GtkColorEditor *editor)
{
  editor->priv->text_changed = TRUE;
}

static void
h_changed (GtkAdjustment  *adj,
           GtkColorEditor *editor)
{
  editor->priv->h = gtk_adjustment_get_value (adj);
  gtk_hsv_to_rgb (editor->priv->h, editor->priv->s, editor->priv->v,
                  &editor->priv->color.red,
                  &editor->priv->color.green,
                  &editor->priv->color.blue);
  gtk_color_plane_set_h (GTK_COLOR_PLANE (editor->priv->sv_plane), editor->priv->h);
  gtk_widget_queue_draw (editor->priv->swatch);
  update_entry (editor);
  g_object_notify (G_OBJECT (editor), "color");
}

static void
sv_changed (GtkColorPlane  *plane,
            GtkColorEditor *editor)
{
  editor->priv->s = gtk_color_plane_get_s (plane);
  editor->priv->v = gtk_color_plane_get_v (plane);
  gtk_hsv_to_rgb (editor->priv->h, editor->priv->s, editor->priv->v,
                  &editor->priv->color.red,
                  &editor->priv->color.green,
                  &editor->priv->color.blue);
  update_entry (editor);
  gtk_widget_queue_draw (editor->priv->swatch);
}

static void
a_changed (GtkAdjustment  *adj,
           GtkColorEditor *editor)
{
  editor->priv->color.alpha = gtk_adjustment_get_value (adj);
  gtk_widget_queue_draw (editor->priv->swatch);
  g_object_notify (G_OBJECT (editor), "color");
}

static cairo_pattern_t *
get_checkered_pattern (void)
{
  /* need to respect pixman's stride being a multiple of 4 */
  static unsigned char data[8] = { 0xFF, 0x00, 0x00, 0x00,
                                   0x00, 0xFF, 0x00, 0x00 };
  static cairo_surface_t *checkered = NULL;
  cairo_pattern_t *pattern;

  if (checkered == NULL)
    checkered = cairo_image_surface_create_for_data (data,
                                                     CAIRO_FORMAT_A8,
                                                     2, 2, 4);

  pattern = cairo_pattern_create_for_surface (checkered);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);

  return pattern;
}

static gboolean
swatch_draw (GtkWidget      *widget,
             cairo_t        *cr,
             GtkColorEditor *editor)
{
  cairo_pattern_t *checkered;

  if (editor->priv->show_alpha)
    {
      cairo_set_source_rgb (cr, 0.33, 0.33, 0.33);
      cairo_paint (cr);

      cairo_set_source_rgb (cr, 0.66, 0.66, 0.66);
      cairo_scale (cr, 8, 8);

      checkered = get_checkered_pattern ();
      cairo_mask (cr, checkered);
      cairo_pattern_destroy (checkered);
    }

  gdk_cairo_set_source_rgba (cr, &editor->priv->color);
  cairo_paint (cr);

  return TRUE;
}

static void
gtk_color_editor_init (GtkColorEditor *editor)
{
  GtkWidget *grid;
  GtkAdjustment *adj;

  editor->priv = G_TYPE_INSTANCE_GET_PRIVATE (editor,
                                              GTK_TYPE_COLOR_EDITOR,
                                              GtkColorEditorPrivate);
  editor->priv->show_alpha = TRUE;

  gtk_widget_push_composite_child ();

  editor->priv->grid = grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);

  editor->priv->swatch = gtk_drawing_area_new ();
  g_signal_connect (editor->priv->swatch, "draw", G_CALLBACK (swatch_draw), editor);
  editor->priv->entry = gtk_entry_new ();
  g_signal_connect (editor->priv->entry, "activate",
                    G_CALLBACK (entry_apply), editor);
  g_signal_connect (editor->priv->entry, "notify::text",
                    G_CALLBACK (entry_text_changed), editor);
  g_signal_connect (editor->priv->entry, "focus-out-event",
                    G_CALLBACK (entry_focus_out), editor);

  adj = gtk_adjustment_new (0, 0, 1, 0.01, 0.1, 0);
  g_signal_connect (adj, "value-changed", G_CALLBACK (h_changed), editor);
  editor->priv->h_slider = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adj);
  editor->priv->h_adj = adj;

  gtk_scale_set_draw_value (GTK_SCALE (editor->priv->h_slider), FALSE);
  editor->priv->sv_plane = gtk_color_plane_new ();
  gtk_widget_set_size_request (editor->priv->sv_plane, 300, 300);
  gtk_widget_set_hexpand (editor->priv->sv_plane, TRUE);
  gtk_widget_set_vexpand (editor->priv->sv_plane, TRUE);

  g_signal_connect (editor->priv->sv_plane, "changed", G_CALLBACK (sv_changed), editor);

  adj = gtk_adjustment_new (1, 0, 1, 0.01, 0.1, 0);
  editor->priv->a_slider = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adj);
  g_signal_connect (adj, "value-changed", G_CALLBACK (a_changed), editor);
  gtk_scale_set_draw_value (GTK_SCALE (editor->priv->a_slider), FALSE);
  editor->priv->a_adj = adj;

  gtk_grid_attach (GTK_GRID (grid), editor->priv->swatch,   1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), editor->priv->entry,    2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), editor->priv->h_slider, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), editor->priv->sv_plane, 1, 1, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), editor->priv->a_slider, 1, 2, 2, 1);
  gtk_widget_show_all (grid);

  gtk_container_add (GTK_CONTAINER (editor), grid);
  gtk_widget_pop_composite_child ();
}

static void
gtk_color_editor_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkColorEditor *ce = GTK_COLOR_EDITOR (object);
  GtkColorChooser *cc = GTK_COLOR_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      {
        GdkRGBA color;
        gtk_color_chooser_get_color (cc, &color);
        g_value_set_boxed (value, &color);
      }
      break;
    case PROP_SHOW_ALPHA:
      g_value_set_boolean (value, gtk_widget_get_visible (ce->priv->a_slider));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_editor_set_show_alpha (GtkColorEditor *editor,
                                 gboolean        show_alpha)
{
  if (editor->priv->show_alpha != show_alpha)
    {
      editor->priv->show_alpha = show_alpha;

      if (show_alpha)
        gtk_widget_show (editor->priv->a_slider);
      else
        gtk_widget_hide (editor->priv->a_slider);
    }
}

static void
gtk_color_editor_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkColorEditor *ce = GTK_COLOR_EDITOR (object);
  GtkColorChooser *cc = GTK_COLOR_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      gtk_color_chooser_set_color (cc, g_value_get_boxed (value));
      break;
    case PROP_SHOW_ALPHA:
      gtk_color_editor_set_show_alpha (ce, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_editor_class_init (GtkColorEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gtk_color_editor_get_property;
  object_class->set_property = gtk_color_editor_set_property;

  g_object_class_override_property (object_class, PROP_COLOR, "color");
  g_object_class_override_property (object_class, PROP_SHOW_ALPHA, "show-alpha");

  g_type_class_add_private (class, sizeof (GtkColorEditorPrivate));
}

static void
gtk_color_editor_get_color (GtkColorChooser *chooser,
                            GdkRGBA         *color)
{
  GtkColorEditor *editor = GTK_COLOR_EDITOR (chooser);

  color->red = editor->priv->color.red;
  color->green = editor->priv->color.green;
  color->blue = editor->priv->color.blue;
  color->alpha = editor->priv->color.alpha;
}

static void
gtk_color_editor_set_color (GtkColorChooser *chooser,
                            const GdkRGBA   *color)
{
  GtkColorEditor *editor = GTK_COLOR_EDITOR (chooser);

  editor->priv->color.red = color->red;
  editor->priv->color.green = color->green;
  editor->priv->color.blue = color->blue;
  gtk_rgb_to_hsv (editor->priv->color.red,
                  editor->priv->color.green,
                  editor->priv->color.blue,
                  &editor->priv->h, &editor->priv->s, &editor->priv->v);
  gtk_color_plane_set_h (GTK_COLOR_PLANE (editor->priv->sv_plane), editor->priv->h);
  gtk_color_plane_set_s (GTK_COLOR_PLANE (editor->priv->sv_plane), editor->priv->s);
  gtk_color_plane_set_v (GTK_COLOR_PLANE (editor->priv->sv_plane), editor->priv->v);
  gtk_adjustment_set_value (editor->priv->h_adj, editor->priv->h);
  update_entry (editor);

  editor->priv->color.alpha = color->alpha;
  gtk_adjustment_set_value (editor->priv->a_adj, editor->priv->color.alpha);

  gtk_widget_queue_draw (GTK_WIDGET (editor));

  g_object_notify (G_OBJECT (editor), "color");
}

static void
gtk_color_editor_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_color = gtk_color_editor_get_color;
  iface->set_color = gtk_color_editor_set_color;
}

GtkWidget *
gtk_color_editor_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_COLOR_EDITOR, NULL);
}
