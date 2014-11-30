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
#include "gtksizegroup.h"
#include "gtkimage.h"
#include "gtkadjustment.h"
#include "gtkbox.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#include <epoxy/glx.h>
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#endif

#ifdef GDK_WINDOWING_QUARTZ
#include "quartz/gdkquartz.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#include <epoxy/egl.h>
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadway.h"
#endif

struct _GtkInspectorGeneralPrivate
{
  GtkWidget *version_box;
  GtkWidget *env_box;
  GtkWidget *gl_box;
  GtkWidget *gtk_version;
  GtkWidget *gdk_backend;
  GtkWidget *gl_version;
  GtkWidget *gl_vendor;
  GtkWidget *prefix;
  GtkWidget *xdg_data_home;
  GtkWidget *xdg_data_dirs;
  GtkWidget *gtk_path;
  GtkWidget *gtk_exe_prefix;
  GtkWidget *gtk_data_prefix;
  GtkWidget *gsettings_schema_dir;
  GtkSizeGroup *labels;
  GtkAdjustment *focus_adjustment;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorGeneral, gtk_inspector_general, GTK_TYPE_SCROLLED_WINDOW)

static void
init_version (GtkInspectorGeneral *gen)
{
  const gchar *backend;
  GdkDisplay *display;

  display = gdk_display_get_default ();

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
append_extension_row (GtkInspectorGeneral *gen,
                      const gchar         *ext,
                      gboolean             have_ext)
{
  GtkWidget *row, *box, *label, *check;

  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  g_object_set (box, "margin", 10, NULL);
  gtk_container_add (GTK_CONTAINER (row), box);
  label = gtk_label_new (ext);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  check = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_MENU);
  gtk_widget_set_halign (check, GTK_ALIGN_END);
  gtk_widget_set_valign (check, GTK_ALIGN_BASELINE);
  gtk_widget_set_opacity (check, have_ext ? 1.0 : 0.0);
  gtk_box_pack_start (GTK_BOX (box), check, TRUE, TRUE, 0);
  gtk_widget_show_all (row);
  gtk_list_box_insert (GTK_LIST_BOX (gen->priv->gl_box), row, -1);

  gtk_size_group_add_widget (GTK_SIZE_GROUP (gen->priv->labels), label);
}

#ifdef GDK_WINDOWING_X11
static void
append_glx_extension_row (GtkInspectorGeneral *gen,
                          Display             *dpy,
                          const gchar         *ext)
{
  append_extension_row (gen, ext, epoxy_has_glx_extension (dpy, 0, ext));
}
#endif

#ifdef GDK_WINDOWING_WAYLAND
static void
append_egl_extension_row (GtkInspectorGeneral *gen,
                          EGLDisplay          *dpy,
                          const gchar         *ext)
{
  append_extension_row (gen, ext, epoxy_has_egl_extension (dpy, ext));
}
#endif

static void
init_gl (GtkInspectorGeneral *gen)
{
  GdkDisplay *display;
  gchar *version;

  display = gdk_display_get_default ();

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    {
      Display *dpy = GDK_DISPLAY_XDISPLAY (display);
      int error_base, event_base;

      if (!glXQueryExtension (dpy, &error_base, &event_base))
        return;

      version = g_strconcat ("GLX ", glXGetClientString (dpy, GLX_VERSION), NULL);
      gtk_label_set_text (GTK_LABEL (gen->priv->gl_version), version);
      g_free (version);
      gtk_label_set_text (GTK_LABEL (gen->priv->gl_vendor), glXGetClientString (dpy, GLX_VENDOR));

      append_glx_extension_row (gen, dpy, "GLX_ARB_create_context_profile");
      append_glx_extension_row (gen, dpy, "GLX_SGI_swap_control");
      append_glx_extension_row (gen, dpy, "GLX_EXT_texture_from_pixmap");
      append_glx_extension_row (gen, dpy, "GLX_SGI_video_sync");
      append_glx_extension_row (gen, dpy, "GLX_EXT_buffer_age");
      append_glx_extension_row (gen, dpy, "GLX_OML_sync_control");
      append_glx_extension_row (gen, dpy, "GLX_ARB_multisample");
      append_glx_extension_row (gen, dpy, "GLX_EXT_visual_rating");
    }
  else
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    {
      EGLDisplay *dpy;
      EGLint major, minor;

      dpy = eglGetDisplay ((EGLNativeDisplayType)gdk_wayland_display_get_wl_display (display));

      if (!eglInitialize (dpy, &major, &minor))
        return;

      version = g_strconcat ("EGL ", eglQueryString (dpy, EGL_VERSION), NULL);
      gtk_label_set_text (GTK_LABEL (gen->priv->gl_version), version);
      g_free (version);
      gtk_label_set_text (GTK_LABEL (gen->priv->gl_vendor), eglQueryString (dpy, EGL_VENDOR));

      append_egl_extension_row (gen, dpy, "EGL_KHR_create_context");
      append_egl_extension_row (gen, dpy, "EGL_EXT_buffer_age");
      append_egl_extension_row (gen, dpy, "EGL_EXT_swap_buffers_with_damage");
      append_egl_extension_row (gen, dpy, "EGL_KHR_surfaceless_context");
    }
  else
#endif
    {
      gtk_label_set_text (GTK_LABEL (gen->priv->gl_version), _("None"));
      gtk_label_set_text (GTK_LABEL (gen->priv->gl_vendor), _("None"));
    }
}

static void
set_monospace_font (GtkWidget *w)
{
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_fallback_new (FALSE));
  pango_attr_list_insert (attrs, pango_attr_family_new ("Monospace"));
  gtk_label_set_attributes (GTK_LABEL (w), attrs);
  pango_attr_list_unref (attrs);
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
  init_gl (gen);
  init_env (gen);
}

static gboolean
keynav_failed (GtkWidget *widget, GtkDirectionType direction, GtkInspectorGeneral *gen)
{
  GtkWidget *next;
  gdouble value, lower, upper, page;

  if (direction == GTK_DIR_DOWN &&
      widget == gen->priv->version_box)
    next = gen->priv->env_box;
  else if (direction == GTK_DIR_DOWN &&
      widget == gen->priv->env_box)
    next = gen->priv->gl_box;
  else if (direction == GTK_DIR_UP &&
           widget == gen->priv->env_box)
    next = gen->priv->version_box;
  else if (direction == GTK_DIR_UP &&
           widget == gen->priv->gl_box)
    next = gen->priv->env_box;
  else
    next = NULL;

  if (next)
    {
      gtk_widget_child_focus (next, direction);
      return TRUE;
    }

  value = gtk_adjustment_get_value (gen->priv->focus_adjustment);
  lower = gtk_adjustment_get_lower (gen->priv->focus_adjustment);
  upper = gtk_adjustment_get_upper (gen->priv->focus_adjustment);
  page  = gtk_adjustment_get_page_size (gen->priv->focus_adjustment);

  if (direction == GTK_DIR_UP && value > lower)
    {
      gtk_adjustment_set_value (gen->priv->focus_adjustment, lower);
      return TRUE;
    }
  else if (direction == GTK_DIR_DOWN && value < upper - page)
    {
      gtk_adjustment_set_value (gen->priv->focus_adjustment, upper - page);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_inspector_general_constructed (GObject *object)
{
  GtkInspectorGeneral *gen = GTK_INSPECTOR_GENERAL (object);

  G_OBJECT_CLASS (gtk_inspector_general_parent_class)->constructed (object);

  gen->priv->focus_adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (gen));
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (gtk_bin_get_child (GTK_BIN (gen))),
                                       gen->priv->focus_adjustment);

   g_signal_connect (gen->priv->version_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->priv->env_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->priv->gl_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
}

static void
gtk_inspector_general_class_init (GtkInspectorGeneralClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gtk_inspector_general_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/general.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, version_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, env_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gl_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gtk_version);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gdk_backend);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gl_version);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gl_vendor);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, prefix);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, xdg_data_home);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, xdg_data_dirs);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gtk_path);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gtk_exe_prefix);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gtk_data_prefix);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gsettings_schema_dir);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, labels);
}

// vim: set et sw=2 ts=2:
