/*
 * Copyright (c) 2018, Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "logs.h"

#include "gtktextview.h"
#include "gtkmessagedialog.h"
#include "gtkfilechooserdialog.h"
#include "gtktogglebutton.h"
#include "gtklabel.h"
#include "gtktooltip.h"
#include "gtktextiter.h"
#include "gtkprivate.h"
#include "gtkdebug.h"
#include "gdkinternals.h"
#include "gtknative.h"
#include "gskdebugprivate.h"
#include "gskrendererprivate.h"


struct _GtkInspectorLogsPrivate
{
  GtkWidget *events;
  GtkWidget *misc;
  GtkWidget *dnd;
  GtkWidget *input;
  GtkWidget *eventloop;
  GtkWidget *frames;
  GtkWidget *settings;
  GtkWidget *opengl;
  GtkWidget *vulkan;
  GtkWidget *selection;
  GtkWidget *clipboard;

  GtkWidget *renderer;
  GtkWidget *cairo;
  GtkWidget *opengl_gsk;
  GtkWidget *vulkan_gsk;
  GtkWidget *shaders;
  GtkWidget *surface;
  GtkWidget *fallback;
  GtkWidget *glyphcache;

  GtkWidget *actions;
  GtkWidget *builder;
  GtkWidget *sizes;
  GtkWidget *icons;
  GtkWidget *keybindings;
  GtkWidget *modules;
  GtkWidget *printing;
  GtkWidget *tree;
  GtkWidget *text;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorLogs, gtk_inspector_logs, GTK_TYPE_BOX)

static void
gtk_inspector_logs_init (GtkInspectorLogs *logs)
{
  logs->priv = gtk_inspector_logs_get_instance_private (logs);
  gtk_widget_init_template (GTK_WIDGET (logs));
}

static void
finalize (GObject *object)
{
  //GtkInspectorLogs *logs = GTK_INSPECTOR_LOGS (object);

  G_OBJECT_CLASS (gtk_inspector_logs_parent_class)->finalize (object);
}

static void
update_flag (GtkWidget *widget,
             guint     *flags,
             guint      flag)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    *flags = *flags | flag;
  else
    *flags = *flags & ~flag;
}

static void
flag_toggled (GtkWidget        *button,
              GtkInspectorLogs *logs)
{
  guint flags;
  GdkDisplay *display;
  GList *toplevels, *l;

  display = gdk_display_get_default ();

  flags = gdk_display_get_debug_flags (display);
  update_flag (logs->priv->events, &flags, GDK_DEBUG_EVENTS);
  update_flag (logs->priv->misc, &flags, GDK_DEBUG_MISC);
  update_flag (logs->priv->dnd, &flags, GDK_DEBUG_DND);
  update_flag (logs->priv->input, &flags, GDK_DEBUG_INPUT);
  update_flag (logs->priv->eventloop, &flags, GDK_DEBUG_EVENTLOOP);
  update_flag (logs->priv->frames, &flags, GDK_DEBUG_FRAMES);
  update_flag (logs->priv->settings, &flags, GDK_DEBUG_SETTINGS);
  update_flag (logs->priv->opengl, &flags, GDK_DEBUG_OPENGL);
  update_flag (logs->priv->vulkan, &flags, GDK_DEBUG_VULKAN);
  update_flag (logs->priv->selection, &flags, GDK_DEBUG_SELECTION);
  update_flag (logs->priv->clipboard, &flags, GDK_DEBUG_CLIPBOARD);
  gdk_display_set_debug_flags (display, flags);

  flags = gsk_get_debug_flags ();
  update_flag (logs->priv->renderer, &flags, GSK_DEBUG_RENDERER);
  update_flag (logs->priv->cairo, &flags, GSK_DEBUG_CAIRO);
  update_flag (logs->priv->opengl_gsk, &flags, GSK_DEBUG_OPENGL);
  update_flag (logs->priv->vulkan_gsk, &flags, GSK_DEBUG_VULKAN);
  update_flag (logs->priv->shaders, &flags, GSK_DEBUG_SHADERS);
  update_flag (logs->priv->surface, &flags, GSK_DEBUG_SURFACE);
  update_flag (logs->priv->fallback, &flags, GSK_DEBUG_FALLBACK);
  update_flag (logs->priv->glyphcache, &flags, GSK_DEBUG_GLYPH_CACHE);
  gsk_set_debug_flags (flags);

  toplevels = gtk_window_list_toplevels ();
  for (l = toplevels; l; l = l->next)
    {
      GtkWidget *toplevel = l->data;
      GskRenderer *renderer;

      if (toplevel == gtk_widget_get_toplevel (button)) /* skip the inspector */
        continue;

      renderer = gtk_native_get_renderer (GTK_NATIVE (toplevel));
      if (!renderer)
        continue;

      gsk_renderer_set_debug_flags (renderer, flags);
    }
  g_list_free (toplevels);

  flags = gtk_get_display_debug_flags (display);
  update_flag (logs->priv->actions, &flags, GTK_DEBUG_ACTIONS);
  update_flag (logs->priv->builder, &flags, GTK_DEBUG_BUILDER);
  update_flag (logs->priv->sizes, &flags, GTK_DEBUG_SIZE_REQUEST);
  update_flag (logs->priv->icons, &flags, GTK_DEBUG_ICONTHEME);
  update_flag (logs->priv->keybindings, &flags, GTK_DEBUG_KEYBINDINGS);
  update_flag (logs->priv->modules, &flags, GTK_DEBUG_MODULES);
  update_flag (logs->priv->printing, &flags, GTK_DEBUG_PRINTING);
  update_flag (logs->priv->tree, &flags, GTK_DEBUG_TREE);
  update_flag (logs->priv->text, &flags, GTK_DEBUG_TEXT);
  gtk_set_display_debug_flags (display, flags);
}

static void
gtk_inspector_logs_class_init (GtkInspectorLogsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/logs.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, events);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, misc);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, dnd);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, input);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, eventloop);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, frames);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, settings);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, opengl);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, vulkan);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, selection);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, clipboard);

  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, cairo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, opengl_gsk);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, vulkan_gsk);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, shaders);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, surface);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, fallback);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, glyphcache);

  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, actions);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, builder);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, sizes);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, icons);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, keybindings);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, modules);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, printing);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, tree);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorLogs, text);
  gtk_widget_class_bind_template_callback (widget_class, flag_toggled);
}

// vim: set et sw=2 ts=2:
