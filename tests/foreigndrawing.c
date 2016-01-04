/* foreign-drawing.c
 * Copyright (C) 2015 Red Hat, Inc
 * Author: Matthias Clasen
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

#include <gtk/gtk.h>

#include <string.h>

static void
append_element (GtkWidgetPath *path,
                const char    *selector)
{
  static const struct {
    const char    *name;
    GtkStateFlags  state_flag;
  } pseudo_classes[] = {
    { "active",        GTK_STATE_FLAG_ACTIVE },
    { "hover",         GTK_STATE_FLAG_PRELIGHT },
    { "selected",      GTK_STATE_FLAG_SELECTED },
    { "disabled",      GTK_STATE_FLAG_INSENSITIVE },
    { "indeterminate", GTK_STATE_FLAG_INCONSISTENT },
    { "focus",         GTK_STATE_FLAG_FOCUSED },
    { "backdrop",      GTK_STATE_FLAG_BACKDROP },
    { "dir(ltr)",      GTK_STATE_FLAG_DIR_LTR },
    { "dir(rtl)",      GTK_STATE_FLAG_DIR_RTL },
    { "link",          GTK_STATE_FLAG_LINK },
    { "visited",       GTK_STATE_FLAG_VISITED },
    { "checked",       GTK_STATE_FLAG_CHECKED },
    { "drop(active)",  GTK_STATE_FLAG_DROP_ACTIVE }
  };
  const char *next;
  char *name;
  char type;
  guint i;

  next = strpbrk (selector, "#.:");
  if (next == NULL)
    next = selector + strlen (selector);

  name = g_strndup (selector, next - selector);
  if (g_ascii_isupper (selector[0]))
    {
      GType gtype;
      gtype = g_type_from_name (name);
      if (gtype == G_TYPE_INVALID)
        {
          g_critical ("Unknown type name `%s'", name);
          g_free (name);
          return;
        }
      gtk_widget_path_append_type (path, gtype);
    }
  else
    {
      /* Omit type, we're using name */
      gtk_widget_path_append_type (path, G_TYPE_NONE);
      gtk_widget_path_iter_set_object_name (path, -1, name);
    }
  g_free (name);

  while (*next != '\0')
    {
      type = *next;
      selector = next + 1;
      next = strpbrk (selector, "#.:");
      if (next == NULL)
        next = selector + strlen (selector);
      name = g_strndup (selector, next - selector);

      switch (type)
        {
        case '#':
          gtk_widget_path_iter_set_name (path, -1, name);
          break;

        case '.':
          gtk_widget_path_iter_add_class (path, -1, name);
          break;

        case ':':
          for (i = 0; i < G_N_ELEMENTS (pseudo_classes); i++)
            {
              if (g_str_equal (pseudo_classes[i].name, name))
                {
                  gtk_widget_path_iter_set_state (path,
                                                  -1,
                                                  gtk_widget_path_iter_get_state (path, -1)
                                                  | pseudo_classes[i].state_flag);
                  break;
                }
            }
          if (i == G_N_ELEMENTS (pseudo_classes))
            g_critical ("Unknown pseudo-class :%s", name);
          break;

        default:
          g_assert_not_reached ();
          break;
        }

      g_free (name);
    }
}

static GtkStyleContext *
get_style (GtkStyleContext *parent,
           const char      *selector)
{
  GtkWidgetPath *path;
  GtkStyleContext *context;

  if (parent)
    path = gtk_widget_path_copy (gtk_style_context_get_path (parent));
  else
    path = gtk_widget_path_new ();

  append_element (path, selector);

  context = gtk_style_context_new ();
  gtk_style_context_set_path (context, path);
  gtk_style_context_set_parent (context, parent);
  /* XXX: Why is this necessary? */
  gtk_style_context_set_state (context, gtk_widget_path_iter_get_state (path, -1));
  gtk_widget_path_unref (path);

  return context;
}

static void
draw_horizontal_scrollbar (GtkWidget     *widget,
                           cairo_t       *cr,
                           gint           x,
                           gint           y,
                           gint           width,
                           gint           height,
                           gint           position,
                           GtkStateFlags  state)
{
  GtkStyleContext *scrollbar_context;
  GtkStyleContext *trough_context;
  GtkStyleContext *slider_context;

  /* This information is taken from the GtkScrollbar docs, see "CSS nodes" */
  const char *path[3] = {
    "scrollbar.horizontal",
    "trough",
    "slider"
  };

  scrollbar_context = get_style (NULL, path[0]);
  trough_context = get_style (scrollbar_context, path[1]);
  slider_context = get_style (trough_context, path[2]);

  gtk_style_context_set_state (scrollbar_context, state);
  gtk_style_context_set_state (trough_context, state);
  gtk_style_context_set_state (slider_context, state);

  gtk_render_background (trough_context, cr, x, y, width, height);
  gtk_render_frame (trough_context, cr, x, y, width, height);
  gtk_render_slider (slider_context, cr, x + position, y + 1, 30, height - 2, GTK_ORIENTATION_HORIZONTAL);

  g_object_unref (slider_context);
  g_object_unref (trough_context);
  g_object_unref (scrollbar_context);
}

static void
draw_text (GtkWidget     *widget,
           cairo_t       *cr,
           gint           x,
           gint           y,
           gint           width,
           gint           height,
           const gchar   *text,
           GtkStateFlags  state)
{
  GtkStyleContext *label_context;
  GtkStyleContext *selection_context;
  GtkStyleContext *context;
  PangoLayout *layout;

  /* This information is taken from the GtkLabel docs, see "CSS nodes" */
  const char *path[2] = {
    "label.view",
    "selection"
  };

  label_context = get_style (NULL, path[0]);
  selection_context = get_style (label_context, path[1]);

  gtk_style_context_set_state (label_context, state);

  if (state & GTK_STATE_FLAG_SELECTED)
    context = selection_context;
  else
    context = label_context;

  layout = gtk_widget_create_pango_layout (widget, text);

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);
  gtk_render_layout (context, cr, x, y, layout);

  g_object_unref (layout);

  g_object_unref (selection_context);
  g_object_unref (label_context);
}

static void
draw_check (GtkWidget     *widget,
            cairo_t       *cr,
            gint           x,
            gint           y,
            GtkStateFlags  state)
{
  GtkStyleContext *button_context;
  GtkStyleContext *check_context;

  /* This information is taken from the GtkCheckButton docs, see "CSS nodes" */
  const char *path[2] = {
    "checkbutton",
    "check"
  };

  button_context = get_style (NULL, path[0]);
  check_context = get_style (button_context, path[1]);

  gtk_style_context_set_state (check_context, state);

  gtk_render_background (check_context, cr, x, y, 20, 20);
  gtk_render_frame (check_context, cr, x, y, 20, 20);
  gtk_render_check (check_context, cr, x, y, 20, 20);

  g_object_unref (check_context);
  g_object_unref (button_context);

}

static void
draw_radio (GtkWidget     *widget,
            cairo_t       *cr,
            gint           x,
            gint           y,
            GtkStateFlags  state)
{
  GtkStyleContext *button_context;
  GtkStyleContext *check_context;

  /* This information is taken from the GtkRadioButton docs, see "CSS nodes" */
  const char *path[2] = {
    "radiobutton",
    "radio"
  };

  button_context = get_style (NULL, path[0]);
  check_context = get_style (button_context, path[1]);

  gtk_style_context_set_state (check_context, state);

  gtk_render_background (check_context, cr, x, y, 20, 20);
  gtk_render_frame (check_context, cr, x, y, 20, 20);
  gtk_render_option (check_context, cr, x, y, 20, 20);

  g_object_unref (check_context);
  g_object_unref (button_context);

}

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr)
{
  gint width, height;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  cairo_rectangle (cr, 0, 0, width, height);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_fill (cr);

  draw_horizontal_scrollbar (widget, cr, 10, 10, width - 20, 10, 30, GTK_STATE_FLAG_NORMAL);
  draw_horizontal_scrollbar (widget, cr, 10, 30, width - 20, 10, 40, GTK_STATE_FLAG_PRELIGHT);
  draw_horizontal_scrollbar (widget, cr, 10, 50, width - 20, 10, 50, GTK_STATE_FLAG_ACTIVE|GTK_STATE_FLAG_PRELIGHT);

  draw_text (widget, cr, 10,  70, width - 20, 20, "Not selected", GTK_STATE_FLAG_NORMAL);
  draw_text (widget, cr, 10, 100, width - 20, 20, "Selected", GTK_STATE_FLAG_SELECTED);

  draw_check (widget, cr,  10, 130, GTK_STATE_FLAG_NORMAL);
  draw_check (widget, cr,  40, 130, GTK_STATE_FLAG_CHECKED);
  draw_radio (widget, cr,  70, 130, GTK_STATE_FLAG_NORMAL);
  draw_radio (widget, cr, 100, 130, GTK_STATE_FLAG_CHECKED);

  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *da;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (window), box);
  da = gtk_drawing_area_new ();
  gtk_widget_set_size_request (da, 200, 200);
  gtk_widget_set_hexpand (da, TRUE);
  gtk_widget_set_vexpand (da, TRUE);
  gtk_widget_set_app_paintable (da, TRUE);
  gtk_container_add (GTK_CONTAINER (box), da);

  g_signal_connect (da, "draw", G_CALLBACK (draw_cb), NULL);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
