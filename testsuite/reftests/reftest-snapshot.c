/*
 * Copyright (C) 2011 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@gnome.org>
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

#include "config.h"

#include "reftest-snapshot.h"

#include "reftest-module.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <cairo-xlib.h>
#endif

#include <string.h>

static GtkWidget *
builder_get_toplevel (GtkBuilder *builder)
{
  GSList *list, *walk;
  GtkWidget *window = NULL;

  list = gtk_builder_get_objects (builder);
  for (walk = list; walk; walk = walk->next)
    {
      if (GTK_IS_WINDOW (walk->data) &&
          gtk_widget_get_parent (walk->data) == NULL)
        {
          window = walk->data;
          break;
        }
    }
  
  g_slist_free (list);

  return window;
}

static gboolean
quit_when_idle (gpointer loop)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static gint inhibit_count;
static GMainLoop *loop;

G_MODULE_EXPORT void
reftest_inhibit_snapshot (void)
{
  inhibit_count++;
}

G_MODULE_EXPORT void
reftest_uninhibit_snapshot (void)
{
  g_assert (inhibit_count > 0);
  inhibit_count--;

  if (inhibit_count == 0)
    g_idle_add (quit_when_idle, loop);
}

static void
draw_paintable (GdkPaintable *paintable,
                gpointer      out_surface)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  cairo_surface_t *surface;
  cairo_t *cr;


  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable,
                          snapshot,
                          gdk_paintable_get_intrinsic_width (paintable),
                          gdk_paintable_get_intrinsic_height (paintable));
  node = gtk_snapshot_free_to_node (snapshot);

  /* If the window literally draws nothing, we assume it hasn't been mapped yet and as such
   * the invalidations were only side effects of resizes.
   */
  if (node == NULL)
    return;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        gdk_paintable_get_intrinsic_width (paintable),
                                        gdk_paintable_get_intrinsic_height (paintable));

  cr = cairo_create (surface);
  gsk_render_node_draw (node, cr);
  cairo_destroy (cr);
  gsk_render_node_unref (node);

  reftest_uninhibit_snapshot ();
  g_signal_handlers_disconnect_by_func (paintable, draw_paintable, out_surface);

  *(cairo_surface_t **) out_surface = surface;
}

static cairo_surface_t *
snapshot_widget (GtkWidget *widget)
{
  GdkPaintable *paintable;
  cairo_surface_t *surface;

  g_assert (gtk_widget_get_realized (widget));

  loop = g_main_loop_new (NULL, FALSE);

  /* We wait until the widget is drawn for the first time.
   *
   * We also use an inhibit mechanism, to give module functions a chance
   * to delay the snapshot.
   */
  reftest_inhibit_snapshot ();
  paintable = gtk_widget_paintable_new (widget);
  g_signal_connect (paintable, "invalidate-contents", G_CALLBACK (draw_paintable), &surface);
  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_object_unref (paintable);
  gtk_widget_destroy (widget);

  return surface;
}

static void
connect_signals (GtkBuilder    *builder,
                 GObject       *object,
                 const gchar   *signal_name,
                 const gchar   *handler_name,
                 GObject       *connect_object,
                 GConnectFlags  flags,
                 gpointer       user_data)
{
  ReftestModule *module;
  const char *directory;
  GCallback func;
  GClosure *closure;
  char **split;

  directory = user_data;
  split = g_strsplit (handler_name, ":", -1);

  switch (g_strv_length (split))
    {
    case 1:
      func = gtk_builder_lookup_callback_symbol (builder, split[0]);

      if (func)
        {
          module = NULL;
        }
      else
        {
          module = reftest_module_new_self ();
          if (module == NULL)
            {
              g_error ("glib compiled without module support.");
              return;
            }
          func = reftest_module_lookup (module, split[0]);
          if (!func)
            {
              g_error ("failed to lookup handler for name '%s' when connecting signals", split[0]);
              return;
            }
        }
      break;
    case 2:
      if (g_getenv ("REFTEST_MODULE_DIR"))
        directory = g_getenv ("REFTEST_MODULE_DIR");
      module = reftest_module_new (directory, split[0]);
      if (module == NULL)
        {
          g_error ("Could not load module '%s' from '%s' when looking up '%s'", split[0], directory, handler_name);
          return;
        }
      func = reftest_module_lookup (module, split[1]);
      if (!func)
        {
          g_error ("failed to lookup handler for name '%s' in module '%s'", split[1], split[0]);
          return;
        }
      break;
    default:
      g_error ("Could not connect signal handler named '%s'", handler_name);
      return;
    }

  g_strfreev (split);

  if (connect_object)
    {
      if (flags & G_CONNECT_SWAPPED)
        closure = g_cclosure_new_object_swap (func, connect_object);
      else
        closure = g_cclosure_new_object (func, connect_object);
    }
  else
    {
      if (flags & G_CONNECT_SWAPPED)
        closure = g_cclosure_new_swap (func, NULL, NULL);
      else
        closure = g_cclosure_new (func, NULL, NULL);
    }
  
  if (module)
    g_closure_add_finalize_notifier (closure, module, (GClosureNotify) reftest_module_unref);

  g_signal_connect_closure (object, signal_name, closure, flags & G_CONNECT_AFTER ? TRUE : FALSE);
}

cairo_surface_t *
reftest_snapshot_ui_file (const char *ui_file)
{
  GtkWidget *window;
  GtkBuilder *builder;
  GError *error = NULL;
  char *directory;

  directory = g_path_get_dirname (ui_file);

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, ui_file, &error);
  g_assert_no_error (error);
  gtk_builder_connect_signals_full (builder, connect_signals, directory);
  window = builder_get_toplevel (builder);
  g_object_unref (builder);
  g_free (directory);
  g_assert (window);

  gtk_widget_show (window);

  return snapshot_widget (window);
}
