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
 * - pop-up entries
 */

#include "config.h"

#include "gtkcolorchooserprivate.h"
#include "gtkcoloreditor.h"
#include "gtkcolorplane.h"
#include "gtkcolorscale.h"
#include "gtkcolorswatch.h"
#include "gtkgrid.h"
#include "gtkorientable.h"
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
  update_entry (editor);
  gtk_color_swatch_set_color (GTK_COLOR_SWATCH (editor->priv->swatch), &editor->priv->color);
  gtk_color_scale_set_color (GTK_COLOR_SCALE (editor->priv->a_slider), &editor->priv->color);
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
  gtk_color_swatch_set_color (GTK_COLOR_SWATCH (editor->priv->swatch), &editor->priv->color);
  gtk_color_scale_set_color (GTK_COLOR_SCALE (editor->priv->a_slider), &editor->priv->color);
  g_object_notify (G_OBJECT (editor), "color");
}

static void
a_changed (GtkAdjustment  *adj,
           GtkColorEditor *editor)
{
  editor->priv->color.alpha = gtk_adjustment_get_value (adj);
  gtk_color_swatch_set_color (GTK_COLOR_SWATCH (editor->priv->swatch), &editor->priv->color);
  g_object_notify (G_OBJECT (editor), "color");
}

static void
gtk_color_editor_init (GtkColorEditor *editor)
{
  GtkWidget *grid;
  GtkWidget *slider;

  editor->priv = G_TYPE_INSTANCE_GET_PRIVATE (editor,
                                              GTK_TYPE_COLOR_EDITOR,
                                              GtkColorEditorPrivate);
  editor->priv->show_alpha = TRUE;

  gtk_widget_push_composite_child ();

  editor->priv->grid = grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);

  editor->priv->swatch = gtk_color_swatch_new ();
  gtk_widget_set_sensitive (editor->priv->swatch, FALSE);
  gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (editor->priv->swatch), 2, 2, 2, 2);

  editor->priv->entry = gtk_entry_new ();
  g_signal_connect (editor->priv->entry, "activate",
                    G_CALLBACK (entry_apply), editor);
  g_signal_connect (editor->priv->entry, "notify::text",
                    G_CALLBACK (entry_text_changed), editor);
  g_signal_connect (editor->priv->entry, "focus-out-event",
                    G_CALLBACK (entry_focus_out), editor);

  editor->priv->h_slider = slider = gtk_color_scale_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (slider), GTK_ORIENTATION_VERTICAL);
  gtk_color_scale_set_type (GTK_COLOR_SCALE (slider), GTK_COLOR_SCALE_HUE);
  gtk_style_context_add_class (gtk_widget_get_style_context (slider), GTK_STYLE_CLASS_SCALE_HAS_MARKS_BELOW);
  editor->priv->h_adj = gtk_range_get_adjustment (GTK_RANGE (slider));
  g_signal_connect (editor->priv->h_adj, "value-changed", G_CALLBACK (h_changed), editor);

  editor->priv->sv_plane = gtk_color_plane_new ();
  gtk_widget_set_size_request (editor->priv->sv_plane, 300, 300);
  gtk_widget_set_hexpand (editor->priv->sv_plane, TRUE);
  gtk_widget_set_vexpand (editor->priv->sv_plane, TRUE);

  g_signal_connect (editor->priv->sv_plane, "changed", G_CALLBACK (sv_changed), editor);

  editor->priv->a_slider = slider = gtk_color_scale_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (slider), GTK_ORIENTATION_HORIZONTAL);
  gtk_color_scale_set_type (GTK_COLOR_SCALE (slider), GTK_COLOR_SCALE_ALPHA);
  gtk_style_context_add_class (gtk_widget_get_style_context (slider), GTK_STYLE_CLASS_SCALE_HAS_MARKS_ABOVE);
  editor->priv->a_adj = gtk_range_get_adjustment (GTK_RANGE (slider));
  g_signal_connect (editor->priv->a_adj, "value-changed", G_CALLBACK (a_changed), editor);

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

      gtk_widget_set_visible (editor->priv->a_slider, show_alpha);

      gtk_color_swatch_set_show_alpha (GTK_COLOR_SWATCH (editor->priv->swatch), show_alpha);
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
gtk_color_editor_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_color_editor_parent_class)->finalize (object);
}

static void
gtk_color_editor_class_init (GtkColorEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_color_editor_finalize;
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
  gdouble h, s, v;

  editor->priv->color.red = color->red;
  editor->priv->color.green = color->green;
  editor->priv->color.blue = color->blue;
  editor->priv->color.alpha = color->alpha;
  gtk_rgb_to_hsv (color->red, color->green, color->blue, &h, &s, &v);
  editor->priv->h = h;
  editor->priv->s = s;
  editor->priv->v = v;
  gtk_color_plane_set_h (GTK_COLOR_PLANE (editor->priv->sv_plane), h);
  gtk_color_plane_set_s (GTK_COLOR_PLANE (editor->priv->sv_plane), s);
  gtk_color_plane_set_v (GTK_COLOR_PLANE (editor->priv->sv_plane), v);
  gtk_color_swatch_set_color (GTK_COLOR_SWATCH (editor->priv->swatch), color);

  gtk_adjustment_set_value (editor->priv->h_adj, h);
  gtk_adjustment_set_value (editor->priv->a_adj, color->alpha);
  gtk_color_scale_set_color (GTK_COLOR_SCALE (editor->priv->a_slider), &editor->priv->color);
  update_entry (editor);

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
