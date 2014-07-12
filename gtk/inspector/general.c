/*
 * Copyright (c) 2014 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "general.h"

#include "gtkdebug.h"
#include "gtklabel.h"
#include "gtkscale.h"
#include "gtkswitch.h"
#include "gtklistbox.h"
#include "gtkprivate.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#endif

#ifdef GDK_WINDOWING_QUARTZ
#include "quartz/gdkquartz.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadway.h"
#endif


struct _GtkInspectorGeneralPrivate
{
  GtkWidget *gtk_version;
  GtkWidget *gdk_backend;

  GtkWidget *prefix;
  GtkWidget *xdg_data_home;
  GtkWidget *xdg_data_dirs;
  GtkWidget *gtk_path;
  GtkWidget *gtk_exe_prefix;
  GtkWidget *gtk_data_prefix;
  GtkWidget *gsettings_schema_dir;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorGeneral, gtk_inspector_general, GTK_TYPE_BOX)

static void
init_version (GtkInspectorGeneral *gen)
{
  const gchar *backend;
  GdkDisplay *display;

  display = gtk_widget_get_display (GTK_WIDGET (gen));

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    backend = "X11";
  else
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    backend = "Wayland";
  else
#endif
#ifdef GDK_WINDOWING_BROADWAY
  if (GDK_IS_BROADWAY_DISPLAY (display))
    backend = "Broadway";
  else
#endif
#ifdef GDK_WINDOWING_WIN32
  if (GDK_IS_WIN32_DISPLAY (display))
    backend = "Windows";
  else
#endif
#ifdef GDK_WINDOWING_QUARTZ
  if (GDK_IS_QUARTZ_DISPLAY (display))
    backend = "Quartz";
  else
#endif
    backend = "Unknown";

  gtk_label_set_text (GTK_LABEL (gen->priv->gtk_version), GTK_VERSION);
  gtk_label_set_text (GTK_LABEL (gen->priv->gdk_backend), backend);
}

static void
set_monospace_font (GtkWidget *w)
{
  PangoFontDescription *font_desc;
  font_desc = pango_font_description_from_string ("monospace");
  gtk_widget_override_font (w, font_desc);
  pango_font_description_free (font_desc);
}

static void
set_path_label (GtkWidget   *w,
                const gchar *var)
{
  const gchar *v;

  v = g_getenv (var);
  if (v != NULL)
    {
      set_monospace_font (w);
      gtk_label_set_text (GTK_LABEL (w), v);
    }
  else
    {
       GtkWidget *r;
       r = gtk_widget_get_ancestor (w, GTK_TYPE_LIST_BOX_ROW);
       gtk_widget_hide (r);
    }
}

static void
init_env (GtkInspectorGeneral *gen)
{
  set_monospace_font (gen->priv->prefix);
  gtk_label_set_text (GTK_LABEL (gen->priv->prefix), _gtk_get_data_prefix ());
  set_path_label (gen->priv->xdg_data_home, "XDG_DATA_HOME");
  set_path_label (gen->priv->xdg_data_dirs, "XDG_DATA_DIRS");
  set_path_label (gen->priv->gtk_path, "GTK_PATH");
  set_path_label (gen->priv->gtk_exe_prefix, "GTK_EXE_PREFIX");
  set_path_label (gen->priv->gtk_data_prefix, "GTK_DATA_PREFIX");
  set_path_label (gen->priv->gsettings_schema_dir, "GSETTINGS_SCHEMA_DIR");
}

static void
gtk_inspector_general_init (GtkInspectorGeneral *gen)
{
  gen->priv = gtk_inspector_general_get_instance_private (gen);
  gtk_widget_init_template (GTK_WIDGET (gen));
  init_version (gen);
  init_env (gen);
}

static void
gtk_inspector_general_class_init (GtkInspectorGeneralClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/general.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gtk_version);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gdk_backend);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, prefix);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, xdg_data_home);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, xdg_data_dirs);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gtk_path);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gtk_exe_prefix);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gtk_data_prefix);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gsettings_schema_dir);
}

// vim: set et sw=2 ts=2:
