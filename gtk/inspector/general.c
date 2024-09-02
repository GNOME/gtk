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
#include "window.h"

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
#include "gtkbinlayout.h"
#include "gtkmediafileprivate.h"
#include "gtkimmoduleprivate.h"
#include "gtkstringpairprivate.h"

#include "gdk/gdkdebugprivate.h"
#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkmonitorprivate.h"
#include "gdk/gdkglcontextprivate.h"
#include "gdk/gdkvulkancontextprivate.h"

#include "profile_conf.h"

#include <epoxy/gl.h>

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#include <epoxy/glx.h>
#include <epoxy/egl.h>
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#include "gdkglcontextprivate.h"
#include <epoxy/wgl.h>
#ifdef GDK_WIN32_ENABLE_EGL
#include <epoxy/egl.h>
#endif
#endif

#ifdef GDK_WINDOWING_MACOS
#include "macos/gdkmacos.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#include <epoxy/egl.h>
#include <xkbcommon/xkbcommon.h>
#include "wayland/gdkdisplay-wayland.h"
#include "wayland/gdkwaylandcolor-private.h"
#include "gtk/gtkimcontextwaylandprivate.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadway.h"
#endif

#ifdef GDK_RENDERING_VULKAN
#include <vulkan/vulkan.h>
#endif

static void gtk_inspector_general_clip (GtkButton *button, GtkInspectorGeneral *gen);

struct _GtkInspectorGeneral
{
  GtkWidget parent;

  GtkWidget *swin;
  GtkWidget *box;
  GtkWidget *version_box;
  GtkWidget *env_box;
  GtkWidget *display_box;
  GtkWidget *display_extensions_row;
  GtkWidget *display_extensions_box;
  GtkWidget *monitor_box;
  GtkWidget *gl_box;
  GtkWidget *gl_features_row;
  GtkWidget *gl_features_box;
  GtkWidget *gl_extensions_row;
  GtkStringList *gl_extensions_list;
  GtkWidget *egl_extensions_row;
  GtkWidget *egl_extensions_row_name;
  GtkStringList *egl_extensions_list;
  GtkWidget *vulkan_box;
  GtkWidget *vulkan_features_row;
  GtkWidget *vulkan_features_box;
  GtkWidget *vulkan_extensions_row;
  GtkStringList *vulkan_extensions_list;
  GtkWidget *vulkan_layers_row;
  GtkStringList *vulkan_layers_list;
  GtkWidget *device_box;
  GtkWidget *os_info;
  GtkWidget *gtk_version;
  GtkWidget *gdk_backend;
  GtkWidget *gsk_renderer;
  GtkWidget *pango_fontmap;
  GtkWidget *media_backend;
  GtkWidget *im_module;
  GtkWidget *a11y_backend;
  GtkWidget *gl_backend_version;
  GtkWidget *gl_backend_version_row;
  GtkWidget *gl_backend_vendor;
  GtkWidget *gl_backend_vendor_row;
  GtkWidget *gl_error;
  GtkWidget *gl_error_row;
  GtkWidget *gl_version;
  GtkWidget *gl_version_row;
  GtkWidget *gl_vendor;
  GtkWidget *gl_vendor_row;
  GtkWidget *gl_renderer;
  GtkWidget *gl_renderer_row;
  GtkWidget *gl_full_version;
  GtkWidget *gl_full_version_row;
  GtkWidget *glsl_version;
  GtkWidget *glsl_version_row;
  GtkWidget *vk_device;
  GtkWidget *vk_api_version;
  GtkWidget *vk_api_version_row;
  GtkWidget *vk_driver_version;
  GtkWidget *vk_driver_version_row;
  GtkWidget *vk_error;
  GtkWidget *vk_error_row;
  GtkWidget *app_id_box;
  GtkWidget *app_id;
  GtkWidget *resource_path;
  GtkWidget *prefix;
  GtkWidget *environment_row;
  GListStore *environment_list;
  GtkWidget *display_name;
  GtkWidget *display_rgba;
  GtkWidget *display_composited;
  GtkWidget *overlay;

  GdkDisplay *display;
};

typedef struct _GtkInspectorGeneralClass
{
  GtkWidgetClass parent_class;
} GtkInspectorGeneralClass;

G_DEFINE_TYPE (GtkInspectorGeneral, gtk_inspector_general, GTK_TYPE_WIDGET)

/* Note that all the information collection functions come in two variants:
 * init_foo() and dump_foo(). The former updates the widgets of the inspector
 * page, the latter creates a markdown dump, to copy-pasted into a gitlab
 * issue.
 *
 * Please keep the two in sync when making changes.
 */

/* {{{ Utilities */

static G_GNUC_UNUSED void
add_check_row (GtkInspectorGeneral *gen,
               GtkListBox          *list,
               const char          *name,
               gboolean             value,
               int                  indent)
{
  GtkWidget *row, *box, *label, *check;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  g_object_set (box, "margin-start", indent, NULL);

  label = gtk_label_new (name);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  check = gtk_image_new_from_icon_name ("object-select-symbolic");
  gtk_widget_set_halign (check, GTK_ALIGN_END);
  gtk_widget_set_valign (check, GTK_ALIGN_BASELINE_FILL);
  gtk_widget_set_opacity (check, value ? 1.0 : 0.0);
  gtk_box_append (GTK_BOX (box), check);

  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  gtk_widget_set_hexpand (box, FALSE);
  gtk_list_box_insert (list, row, -1);
}

static void
add_label_row (GtkInspectorGeneral *gen,
               GtkListBox          *list,
               const char          *name,
               const char          *value,
               int                  indent)
{
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *row;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  g_object_set (box, "margin-start", indent, NULL);

  label = gtk_label_new (name);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  label = gtk_label_new (value);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 25);
  gtk_box_append (GTK_BOX (box), label);

  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  gtk_widget_set_hexpand (box, FALSE);
  gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);
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

/* }}} */
/* {{{ OS Info */

static void
init_os_info (GtkInspectorGeneral *gen)
{
  char *os_info = g_get_os_info (G_OS_INFO_KEY_PRETTY_NAME);
  gtk_label_set_text (GTK_LABEL (gen->os_info), os_info);
  g_free (os_info);
}

static void
dump_os_info (GdkDisplay *display,
              GString    *string)
{
  char *os_info = g_get_os_info (G_OS_INFO_KEY_PRETTY_NAME);
  g_string_append_printf (string, "| Operating System | %s |\n", os_info);
  g_free (os_info);
}


/* }}} */
/* {{{ Version */

static const char *
get_display_kind (GdkDisplay *display)
{
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    return "X11";
  else
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    return "Wayland";
  else
#endif
#ifdef GDK_WINDOWING_BROADWAY
  if (GDK_IS_BROADWAY_DISPLAY (display))
    return "Broadway";
  else
#endif
#ifdef GDK_WINDOWING_WIN32
  if (GDK_IS_WIN32_DISPLAY (display))
    return "Windows";
  else
#endif
#ifdef GDK_WINDOWING_MACOS
  if (GDK_IS_MACOS_DISPLAY (display))
    return "MacOS";
  else
#endif
    return "Unknown";
}

static const char *
get_renderer_kind (GdkDisplay *display)
{
  GdkSurface *surface;
  GskRenderer *gsk_renderer;
  const char *renderer;

  surface = gdk_surface_new_toplevel (display);
  gsk_renderer = gsk_renderer_new_for_surface (surface);

  if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskVulkanRenderer") == 0)
    renderer = "Vulkan";
  else if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskGLRenderer") == 0)
    renderer = "GL";
  else if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskCairoRenderer") == 0)
    renderer = "Cairo";
  else if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskNglRenderer") == 0)
    renderer = "GL (new)";
  else
    renderer = "Unknown";

  gsk_renderer_unrealize (gsk_renderer);
  g_object_unref (gsk_renderer);
  gdk_surface_destroy (surface);

  return renderer;
}

static char *
get_version_string (void)
{
  if (g_strcmp0 (PROFILE, "devel") == 0)
    return g_strdup_printf ("%s-%s", GTK_VERSION, VCS_TAG);
  else
    return g_strdup (GTK_VERSION);
}

static void
init_version (GtkInspectorGeneral *gen)
{
  char *version;

  version = get_version_string ();
  gtk_label_set_text (GTK_LABEL (gen->gtk_version), version);
  g_free (version);

  gtk_label_set_text (GTK_LABEL (gen->gdk_backend), get_display_kind (gen->display));
  gtk_label_set_text (GTK_LABEL (gen->gsk_renderer), get_renderer_kind (gen->display));
}

static void
dump_version (GdkDisplay *display,
              GString    *string)
{
  char *version;

  version = get_version_string ();
  g_string_append_printf (string, "| GTK Version | %s |\n", version);
  g_free (version);
  g_string_append_printf (string, "| GDK Backend | %s |\n", get_display_kind (display));
  g_string_append_printf (string, "| GSK Renderer | %s |\n", get_renderer_kind (display));
}

/* }}} */
/* {{{ Pango */

static const char *
get_fontmap_kind (void)
{
  PangoFontMap *fontmap;
  const char *type;

  fontmap = pango_cairo_font_map_get_default ();
  type = G_OBJECT_TYPE_NAME (fontmap);

  if (strcmp (type, "PangoCairoFcFontMap") == 0)
    return "fontconfig";
  else if (strcmp (type, "PangoCairoCoreTextFontMap") == 0)
    return "coretext";
  else if (strcmp (type, "PangoCairoWin32FontMap") == 0)
    return  "win32";
  else
    return type;
}

static void
init_pango (GtkInspectorGeneral *gen)
{
  gtk_label_set_label (GTK_LABEL (gen->pango_fontmap), get_fontmap_kind ());
}

static void
dump_pango (GdkDisplay *display,
            GString    *string)
{
  g_string_append_printf (string, "| Pango Fontmap | %s |\n", get_fontmap_kind ());
}

/* }}} */
/* {{{ Media */

static const char *
get_media_backend_kind (void)
{
  GIOExtension *e;

  e = gtk_media_file_get_extension ();
  return g_io_extension_get_name (e);
}

static void
init_media (GtkInspectorGeneral *gen)
{
  gtk_label_set_label (GTK_LABEL (gen->media_backend), get_media_backend_kind ());
}

static void
dump_media (GdkDisplay *display,
            GString    *string)
{
  g_string_append_printf (string, "| Media Backend | %s |\n", get_media_backend_kind ());
}

/* }}} */
/* {{{ Input Method */

static void
im_module_changed (GtkSettings         *settings,
                   GParamSpec          *pspec,
                   GtkInspectorGeneral *gen)
{
  if (!gen->display)
    return;

  gtk_label_set_label (GTK_LABEL (gen->im_module),
                       _gtk_im_module_get_default_context_id (gen->display));
}

static const char *
get_im_module_kind (GdkDisplay *display)
{
  return _gtk_im_module_get_default_context_id (display);
}

static void
init_im_module (GtkInspectorGeneral *gen)
{
  GtkSettings *settings = gtk_settings_get_for_display (gen->display);

  gtk_label_set_label (GTK_LABEL (gen->im_module), get_im_module_kind (gen->display));

  if (g_getenv ("GTK_IM_MODULE") != NULL)
    {
      /* This can't update if GTK_IM_MODULE envvar is set */
      gtk_widget_set_tooltip_text (gen->im_module,
                                   _("IM Context is hardcoded by GTK_IM_MODULE"));
      gtk_widget_set_sensitive (gen->im_module, FALSE);
      return;
    }

  g_signal_connect_object (settings,
                           "notify::gtk-im-module",
                           G_CALLBACK (im_module_changed),
                           gen, 0);
}

static void
dump_im_module (GdkDisplay *display,
                GString    *string)
{
  g_string_append_printf (string, "| Input Method | %s |\n", get_im_module_kind (display));
}

/* }}} */
/* {{{ Accessibility */

static const char *
get_a11y_backend (GdkDisplay *display)
{
  GtkWidget *widget;
  GtkATContext *ctx;
  const char *backend;

  widget = gtk_label_new ("");
  g_object_ref_sink (widget);
  ctx = gtk_at_context_create (GTK_ACCESSIBLE_ROLE_LABEL, GTK_ACCESSIBLE (widget), display);

  if (ctx == NULL)
    backend = "none";
  else if (strcmp (G_OBJECT_TYPE_NAME (ctx), "GtkAtSpiContext") == 0)
    backend = "atspi";
  else if (strcmp (G_OBJECT_TYPE_NAME (ctx), "GtkAccessKitContext") == 0)
    backend = "accesskit";
  else if (strcmp (G_OBJECT_TYPE_NAME (ctx), "GtkTestATContext") == 0)
    backend = "test";
  else
    backend = "unknown";

  g_clear_object (&ctx);
  g_clear_object (&widget);

  return backend;
}

static void
init_a11y_backend (GtkInspectorGeneral *gen)
{
  gtk_label_set_label (GTK_LABEL (gen->a11y_backend), get_a11y_backend (gen->display));
}

static void
dump_a11y_backend (GdkDisplay *display,
                   GString    *string)
{
  g_string_append_printf (string, "| Accessibility backend | %s |\n", get_a11y_backend (display));
}

/* }}} */
/* {{{ Application data */

static void
init_app_id (GtkInspectorGeneral *gen)
{
  GApplication *app;

  app = g_application_get_default ();
  if (!app)
    {
      gtk_widget_set_visible (gen->app_id_box, FALSE);
      return;
    }

  gtk_label_set_text (GTK_LABEL (gen->app_id),
                      g_application_get_application_id (app));
  gtk_label_set_text (GTK_LABEL (gen->resource_path),
                      g_application_get_resource_base_path (app));
}

static void
dump_app_id (GdkDisplay *display,
             GString    *string)
{
  GApplication *app;

  app = g_application_get_default ();
  if (!app)
    return;

  g_string_append_printf (string, "| Application ID | %s |\n", g_application_get_application_id (app));
  g_string_append_printf (string, "| Resource Path | %s |\n", g_application_get_resource_base_path (app));
}

/* }}} */
/* {{{ GL */

static void
add_gl_features (GtkInspectorGeneral *gen)
{
  GdkGLContext *context;

  context = gdk_display_get_gl_context (gen->display);

  for (int i = 0; i < GDK_GL_N_FEATURES; i++)
    {
      add_check_row (gen, GTK_LIST_BOX (gen->gl_features_box),
                     gdk_gl_feature_keys[i].key,
                     gdk_gl_context_has_feature (context, gdk_gl_feature_keys[i].value),
                     0);
    }
}

/* unused on some setup, like Mac */
static void G_GNUC_UNUSED
append_extensions (GtkStringList *list,
                   const char    *extensions)
{
  char **items;
  gsize i;

  if (extensions == NULL)
    return;
  
  items = g_strsplit (extensions, " ", -1);
  for (i = 0; items[i]; i++)
    {
      if (items[i] == NULL || items[i][0] == 0)
        continue;

      gtk_string_list_append (list, items[i]);
    }

  g_strfreev (items);
}

#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND) || (defined(GDK_WINDOWING_WIN32) && defined(GDK_WIN32_ENABLE_EGL))
static EGLDisplay
get_egl_display (GdkDisplay *display)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    return gdk_wayland_display_get_egl_display (display);
  else
#endif
#ifdef GDK_WINDOWING_X11
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (GDK_IS_X11_DISPLAY (display))
    return gdk_x11_display_get_egl_display (display);
  else
G_GNUC_END_IGNORE_DEPRECATIONS
#endif
#ifdef GDK_WINDOWING_WIN32
  if (GDK_IS_WIN32_DISPLAY (display))
    return gdk_win32_display_get_egl_display (display);
  else
#endif
   return NULL;
}
#endif

static void
init_gl (GtkInspectorGeneral *gen)
{
  GdkGLContext *context;
  GError *error = NULL;
  int major, minor;
  int num_extensions, i;
  char *s;

  if (!gdk_display_prepare_gl (gen->display, &error))
    {
      gtk_label_set_text (GTK_LABEL (gen->gl_renderer), C_("GL renderer", "None"));
      gtk_widget_set_visible (gen->gl_error_row, TRUE);
      gtk_widget_set_visible (gen->gl_version_row, FALSE);
      gtk_widget_set_visible (gen->gl_backend_version_row, FALSE);
      gtk_widget_set_visible (gen->gl_backend_vendor_row, FALSE);
      gtk_widget_set_visible (gen->gl_vendor_row, FALSE);
      gtk_widget_set_visible (gen->gl_full_version_row, FALSE);
      gtk_widget_set_visible (gen->glsl_version_row, FALSE);
      gtk_widget_set_visible (gen->gl_features_row, FALSE);
      gtk_widget_set_visible (gen->gl_extensions_row, FALSE);
      gtk_widget_set_visible (gen->egl_extensions_row, FALSE);
      gtk_label_set_text (GTK_LABEL (gen->gl_error), error->message);
      g_error_free (error);
      return;
    }

  gdk_gl_context_make_current (gdk_display_get_gl_context (gen->display));

  glGetIntegerv (GL_NUM_EXTENSIONS, &num_extensions);
  for (i = 0; i < num_extensions; i++)
    {
      const char *gl_ext = (const char *) glGetStringi (GL_EXTENSIONS, i);
      if (!gl_ext)
        break;
      gtk_string_list_append (GTK_STRING_LIST (gen->gl_extensions_list), gl_ext);
    }

#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND) || (defined(GDK_WINDOWING_WIN32) && defined(GDK_WIN32_ENABLE_EGL))
  EGLDisplay egl_display = get_egl_display (gen->display);
  if (egl_display)
    {
      char *version;

      version = g_strconcat ("EGL ", eglQueryString (egl_display, EGL_VERSION), NULL);
      gtk_label_set_text (GTK_LABEL (gen->gl_backend_version), version);
      g_free (version);

      gtk_label_set_text (GTK_LABEL (gen->gl_backend_vendor), eglQueryString (egl_display, EGL_VENDOR));

      gtk_label_set_text (GTK_LABEL (gen->egl_extensions_row_name), "EGL extensions");
      append_extensions (gen->egl_extensions_list, eglQueryString (egl_display, EGL_EXTENSIONS));
    }
  else
#endif
#ifdef GDK_WINDOWING_X11

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  if (GDK_IS_X11_DISPLAY (gen->display))
    {
      Display *dpy = GDK_DISPLAY_XDISPLAY (gen->display);
      int error_base, event_base, screen;
      char *version;

      if (!glXQueryExtension (dpy, &error_base, &event_base))
        return;

      version = g_strconcat ("GLX ", glXGetClientString (dpy, GLX_VERSION), NULL);
      gtk_label_set_text (GTK_LABEL (gen->gl_backend_version), version);
      g_free (version);
      gtk_label_set_text (GTK_LABEL (gen->gl_backend_vendor), glXGetClientString (dpy, GLX_VENDOR));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      screen = XScreenNumberOfScreen (gdk_x11_display_get_xscreen (gen->display));
G_GNUC_END_IGNORE_DEPRECATIONS
      gtk_label_set_text (GTK_LABEL (gen->egl_extensions_row_name), "GLX extensions");
      append_extensions (gen->egl_extensions_list, glXQueryExtensionsString (dpy, screen));
    }
  else

G_GNUC_END_IGNORE_DEPRECATIONS

#endif
#ifdef GDK_WINDOWING_WIN32
  if (GDK_IS_WIN32_DISPLAY (gen->display) &&
      gdk_gl_backend_can_be_used (GDK_GL_WGL, NULL))
    {
      PFNWGLGETEXTENSIONSSTRINGARBPROC my_wglGetExtensionsStringARB;

      gtk_label_set_text (GTK_LABEL (gen->gl_backend_vendor), "Microsoft WGL");
      gtk_widget_set_visible (gen->gl_backend_version, FALSE);

      my_wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC) wglGetProcAddress("wglGetExtensionsStringARB");

      if (my_wglGetExtensionsStringARB)
        {
          gtk_label_set_text (GTK_LABEL (gen->egl_extensions_row_name), "WGL extensions");
          append_extensions (gen->egl_extensions_list, my_wglGetExtensionsStringARB (wglGetCurrentDC ()));
        }
      else
        {
          gtk_label_set_text (GTK_LABEL (gen->egl_extensions_row_name), "WGL extensions: none");
        }
    }
  else
#endif
    {
      gtk_label_set_text (GTK_LABEL (gen->gl_backend_version), C_("GL version", "Unknown"));
      gtk_widget_set_visible (gen->egl_extensions_row, FALSE);
    }

  context = gdk_display_get_gl_context (gen->display);
  gdk_gl_context_make_current (context);
  gdk_gl_context_get_version (context, &major, &minor);
  s = g_strdup_printf ("%s %u.%u",
                       gdk_gl_context_get_use_es (context) ? "GLES " : "OpenGL ",
                       major, minor);
  gtk_label_set_text (GTK_LABEL (gen->gl_version), s);
  g_free (s);
  gtk_label_set_text (GTK_LABEL (gen->gl_vendor), (const char *) glGetString (GL_VENDOR));
  gtk_label_set_text (GTK_LABEL (gen->gl_renderer), (const char *) glGetString (GL_RENDERER));
  gtk_label_set_text (GTK_LABEL (gen->gl_full_version), (const char *) glGetString (GL_VERSION));
  gtk_label_set_text (GTK_LABEL (gen->glsl_version), (const char *) glGetString (GL_SHADING_LANGUAGE_VERSION));

  add_gl_features (gen);
}

static void
dump_gl (GdkDisplay *display,
         GString    *string)
{
  GdkGLContext *context;
  GError *error = NULL;
  int major, minor;
  int num_extensions;
  char *s;
  GString *ext;

  if (!gdk_display_prepare_gl (display, &error))
    {
      g_string_append (string, "| GL Renderer | None |\n");
      g_string_append_printf (string, "| | %s |\n", error->message);
      g_error_free (error);
      return;
    }

  gdk_gl_context_make_current (gdk_display_get_gl_context (display));

  ext = g_string_new ("");

#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND) || (defined(GDK_WINDOWING_WIN32) && defined(GDK_WIN32_ENABLE_EGL))
  EGLDisplay egl_display = get_egl_display (display);
  if (egl_display)
    {
      char *version;
      guint count;
      char *prefix;

      version = g_strconcat ("EGL ", eglQueryString (egl_display, EGL_VERSION), NULL);
      g_string_append_printf (string, "| GL Backend Version | %s |\n", version);
      g_free (version);

      g_string_append_printf (string, "| GL Backend Vendor | %s |\n", eglQueryString (egl_display, EGL_VENDOR));

      g_string_assign (ext, eglQueryString (egl_display, EGL_EXTENSIONS));
      count = g_string_replace (ext, " ", "<br>", 0);
      prefix = g_strdup_printf ("| EGL Extensions | <details><summary>%u Extensions</summary>", count + 1);
      g_string_prepend (ext, prefix);
      g_string_append (ext, "</details> |\n");
      g_free (prefix);
    }
  else
#endif
#ifdef GDK_WINDOWING_X11

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  if (GDK_IS_X11_DISPLAY (display))
    {
      Display *dpy = GDK_DISPLAY_XDISPLAY (display);
      int error_base, event_base, screen;
      char *version;
      guint count;
      char *prefix;

      if (!glXQueryExtension (dpy, &error_base, &event_base))
        return;

      version = g_strconcat ("GLX ", glXGetClientString (dpy, GLX_VERSION), NULL);
      g_string_append_printf (string, "| GL Backend Version | %s |\n", version);
      g_free (version);

      g_string_append_printf (string, "| GL Backend Vendor | %s |\n", glXGetClientString (dpy, GLX_VENDOR));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      screen = XScreenNumberOfScreen (gdk_x11_display_get_xscreen (display));
G_GNUC_END_IGNORE_DEPRECATIONS
      g_string_assign (ext, glXQueryExtensionsString (dpy, screen));
      count = g_string_replace (ext, " ", "<br>", 0);
      prefix = g_strdup_printf ("| GLX Extensions | <details><summary>%u Extensions</summary>", count + 1);
      g_string_prepend (ext, prefix);
      g_string_append (ext, "</details> |\n");
      g_free (prefix);
    }
  else

G_GNUC_END_IGNORE_DEPRECATIONS

#endif
#ifdef GDK_WINDOWING_WIN32
  if (GDK_IS_WIN32_DISPLAY (display) &&
      gdk_gl_backend_can_be_used (GDK_GL_WGL, NULL))
    {
      PFNWGLGETEXTENSIONSSTRINGARBPROC my_wglGetExtensionsStringARB;

      g_string_append (string, "| GL Backend Vendor | Microsoft WGL |\n");

      my_wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC) wglGetProcAddress("wglGetExtensionsStringARB");

      if (my_wglGetExtensionsStringARB)
        {
          guint count;
          char *prefix;

          g_string_assign (ext, my_wglGetExtensionsStringARB (wglGetCurrentDC ()));
          count = g_string_replace (ext, " ", "<br>", 0);
          prefix = g_strdup_printf ("| WGL Extensions | <details><summary>%u Extensions</summary>", count + 1);
          g_string_prepend (ext, prefix);
          g_string_append (ext, "</details> |\n");
          g_free (prefix);
        }
      else
        {
          g_string_append (ext, "| WGL Extensions | None |\n");
        }
    }
  else
#endif
    {
      g_string_append (string, "| GL Backend Version | Unknown |\n");
    }

  context = gdk_display_get_gl_context (display);
  gdk_gl_context_make_current (context);
  gdk_gl_context_get_version (context, &major, &minor);
  s = g_strdup_printf ("%s %u.%u",
                       gdk_gl_context_get_use_es (context) ? "GLES " : "OpenGL ",
                       major, minor);
  g_string_append_printf (string, "| GL Version | %s |\n", s);
  g_free (s);
  g_string_append_printf (string, "| GL Vendor | %s |\n", (const char *) glGetString (GL_VENDOR));
  g_string_append_printf (string, "| GL Renderer | %s |\n", (const char *) glGetString (GL_RENDERER));
  g_string_append_printf (string, "| GL Full Version | %s |\n", (const char *) glGetString (GL_VERSION));
  g_string_append_printf (string, "| GLSL Version | %s |\n", (const char *) glGetString (GL_SHADING_LANGUAGE_VERSION));

  glGetIntegerv (GL_NUM_EXTENSIONS, &num_extensions);
  g_string_append_printf (string, "| GL Extensions | <details><summary>%d Extensions</summary>", num_extensions);
  for (int i = 0; i < num_extensions; i++)
    {
      const char *gl_ext = (const char *) glGetStringi (GL_EXTENSIONS, i);
      if (gl_ext)
        g_string_append_printf (string, "%s%s", i > 0 ? "<br>" : "", gl_ext);
    }
  g_string_append (string, "</details> |\n");

  g_string_append (string, ext->str);
  g_string_free (ext, TRUE);

  g_string_append (string, "| Features | ");
  for (int i = 0; i < GDK_GL_N_FEATURES; i++)
    {
      if (gdk_gl_context_has_feature (context, gdk_gl_feature_keys[i].value))
        g_string_append_printf (string, "%s%s", i > 0 ? "<br>" : "", gdk_gl_feature_keys[i].key);
    }
  g_string_append (string, " |\n");
}

/* }}} */
/* {{{ Vulkan */

#ifdef GDK_RENDERING_VULKAN
static gboolean
gdk_vulkan_has_feature (GdkDisplay        *display,
                        GdkVulkanFeatures  feature)
{
  return (display->vulkan_features & feature) ? TRUE : FALSE;
}

static void
add_vulkan_features (GtkInspectorGeneral *gen)
{
  for (int i = 0; i < GDK_VULKAN_N_FEATURES; i++)
    {
      add_check_row (gen, GTK_LIST_BOX (gen->vulkan_features_box),
                     gdk_vulkan_feature_keys[i].key,
                     gdk_vulkan_has_feature (gen->display, gdk_vulkan_feature_keys[i].value),
                     0);
    }
}

static void
add_instance_extensions (GtkStringList *list)
{
  uint32_t i;
  uint32_t n_extensions;
  VkExtensionProperties *extensions;

  vkEnumerateInstanceExtensionProperties (NULL, &n_extensions, NULL);
  extensions = g_newa (VkExtensionProperties, n_extensions);
  vkEnumerateInstanceExtensionProperties (NULL, &n_extensions, extensions);

  for (i = 0; i < n_extensions; i++)
    gtk_string_list_append (list, extensions[i].extensionName);
}

static void
add_device_extensions (VkPhysicalDevice  device,
                       GtkStringList    *list)
{
  uint32_t i;
  uint32_t n_extensions;
  VkExtensionProperties *extensions;

  vkEnumerateDeviceExtensionProperties (device, NULL, &n_extensions, NULL);
  extensions = g_newa (VkExtensionProperties, n_extensions);
  vkEnumerateDeviceExtensionProperties (device, NULL, &n_extensions, extensions);

  for (i = 0; i < n_extensions; i++)
    gtk_string_list_append (list, extensions[i].extensionName);
}

static void
add_layers (GtkStringList *list)
{
  uint32_t i;
  uint32_t n_layers;
  VkLayerProperties *layers;

  vkEnumerateInstanceLayerProperties (&n_layers, NULL);
  layers = g_newa (VkLayerProperties, n_layers);
  vkEnumerateInstanceLayerProperties (&n_layers, layers);

  for (i = 0; i < n_layers; i++)
    gtk_string_list_append (list, layers[i].layerName);
}
#endif

static void
init_vulkan (GtkInspectorGeneral *gen)
{
#ifdef GDK_RENDERING_VULKAN
  VkPhysicalDevice vk_device;
  VkPhysicalDeviceProperties props;
  char *device_name;
  char *api_version;
  char *driver_version;
  const char *types[] = { "other", "integrated GPU", "discrete GPU", "virtual GPU", "CPU" };
  GError *error = NULL;

  if (!gdk_display_prepare_vulkan (gen->display, &error))
    {
      gtk_label_set_text (GTK_LABEL (gen->vk_device), C_("Vulkan device", "None"));
      gtk_widget_set_visible (gen->vk_error_row, TRUE);
      gtk_label_set_text (GTK_LABEL (gen->vk_error), error->message);
      g_error_free (error);

      gtk_widget_set_visible (gen->vk_api_version_row, FALSE);
      gtk_widget_set_visible (gen->vk_driver_version_row, FALSE);
      gtk_widget_set_visible (gen->vulkan_features_row, FALSE);
      gtk_widget_set_visible (gen->vulkan_layers_row, FALSE);
      gtk_widget_set_visible (gen->vulkan_extensions_row, FALSE);
      return;
    }

  vk_device = gen->display->vk_physical_device;
  vkGetPhysicalDeviceProperties (vk_device, &props);

  device_name = g_strdup_printf ("%s (%s)", props.deviceName, types[props.deviceType]);
  api_version = g_strdup_printf ("%d.%d.%d",
                                 VK_VERSION_MAJOR (props.apiVersion),
                                 VK_VERSION_MINOR (props.apiVersion),
                                 VK_VERSION_PATCH (props.apiVersion));
  driver_version = g_strdup_printf ("%d.%d.%d",
                                    VK_VERSION_MAJOR (props.driverVersion),
                                    VK_VERSION_MINOR (props.driverVersion),
                                    VK_VERSION_PATCH (props.driverVersion));

  gtk_label_set_text (GTK_LABEL (gen->vk_device), device_name);
  gtk_label_set_text (GTK_LABEL (gen->vk_api_version), api_version);
  gtk_label_set_text (GTK_LABEL (gen->vk_driver_version), driver_version);

  g_free (device_name);
  g_free (api_version);
  g_free (driver_version);

  add_vulkan_features (gen);
  add_instance_extensions (gen->vulkan_extensions_list);
  add_device_extensions (gen->display->vk_physical_device, gen->vulkan_extensions_list);
  add_layers (gen->vulkan_layers_list);
#else
  gtk_label_set_text (GTK_LABEL (gen->vk_device), C_("Vulkan device", "None"));
  gtk_widget_set_visible (gen->vk_api_version_row, FALSE);
  gtk_widget_set_visible (gen->vk_driver_version_row, FALSE);
  gtk_widget_set_visible (gen->vulkan_layers_row, FALSE);
  gtk_widget_set_visible (gen->vulkan_extensions_row, FALSE);
#endif
}

static void
dump_vulkan (GdkDisplay *display,
             GString    *string)
{
#ifdef GDK_RENDERING_VULKAN
  VkPhysicalDevice vk_device;
  VkPhysicalDeviceProperties props;
  char *device_name;
  char *api_version;
  char *driver_version;
  const char *types[] = { "other", "integrated GPU", "discrete GPU", "virtual GPU", "CPU" };
  GError *error = NULL;
  uint32_t n_layers;
  VkLayerProperties *layers;
  uint32_t n_instance_extensions;
  uint32_t n_device_extensions;
  VkExtensionProperties *instance_extensions;
  VkExtensionProperties *device_extensions;

  if (!gdk_display_prepare_vulkan (display, &error))
    {
      g_string_append (string, "| Vulkan Device | None |\n");
      g_string_append_printf (string, "| | %s |\n", error->message);
      g_error_free (error);
      return;
    }

  vk_device = display->vk_physical_device;
  vkGetPhysicalDeviceProperties (vk_device, &props);

  device_name = g_strdup_printf ("%s (%s)", props.deviceName, types[props.deviceType]);
  api_version = g_strdup_printf ("%d.%d.%d",
                                 VK_VERSION_MAJOR (props.apiVersion),
                                 VK_VERSION_MINOR (props.apiVersion),
                                 VK_VERSION_PATCH (props.apiVersion));
  driver_version = g_strdup_printf ("%d.%d.%d",
                                    VK_VERSION_MAJOR (props.driverVersion),
                                    VK_VERSION_MINOR (props.driverVersion),
                                    VK_VERSION_PATCH (props.driverVersion));

  g_string_append_printf (string, "| Vulkan Device | %s |\n", device_name);
  g_string_append_printf (string, "| Vulkan API Version | %s |\n", api_version);
  g_string_append_printf (string, "| Vulkan Driver Version | %s |\n", driver_version);

  g_free (device_name);
  g_free (api_version);
  g_free (driver_version);

  vkEnumerateInstanceLayerProperties (&n_layers, NULL);
  layers = g_newa (VkLayerProperties, n_layers);
  vkEnumerateInstanceLayerProperties (&n_layers, layers);

  g_string_append (string, "| Layers | ");
  for (uint32_t i = 0; i < n_layers; i++)
    g_string_append_printf (string, "%s%s", i > 0 ? "<br>" : "", layers[i].layerName);
  g_string_append (string, " |\n");

  vkEnumerateInstanceExtensionProperties (NULL, &n_instance_extensions, NULL);
  instance_extensions = g_newa (VkExtensionProperties, n_instance_extensions);
  vkEnumerateInstanceExtensionProperties (NULL, &n_instance_extensions, instance_extensions);

  vkEnumerateDeviceExtensionProperties (vk_device, NULL, &n_device_extensions, NULL);
  device_extensions = g_newa (VkExtensionProperties, n_device_extensions);
  vkEnumerateDeviceExtensionProperties (vk_device, NULL, &n_device_extensions, device_extensions);

  g_string_append_printf (string, "| Vulkan Extensions | <details><summary>%u Extensions</summary>", n_instance_extensions + n_device_extensions);

  for (uint32_t i = 0; i < n_instance_extensions; i++)
    g_string_append_printf (string, "%s%s", i > 0 ? "<br>" : "", instance_extensions[i].extensionName);

  for (uint32_t i = 0; i < n_device_extensions; i++)
    g_string_append_printf (string, "%s%s", i > 0 ? "<br>" : "", device_extensions[i].extensionName);

  g_string_append (string, "</details> |\n");

  g_string_append (string, "| Features | ");
  for (int i = 0; i < GDK_VULKAN_N_FEATURES; i++)
    {
      if (gdk_vulkan_has_feature (display, gdk_vulkan_feature_keys[i].value))
        g_string_append_printf (string, "%s%s", i > 0 ? "<br>" : "", gdk_vulkan_feature_keys[i].key);
    }
  g_string_append (string, " |\n");
#endif
}

/* }}} */
/* {{{ Environment */

static const char *env_list[] = {
  "AT_SPI_BUS_ADDRESS",
  "BROADWAY_DISPLAY",
  "DESKTOP_AUTOSTART_ID",
  "DISPLAY",
  "GDK_BACKEND",
  "GDK_DEBUG",
  "GDK_DISABLE",
  "GDK_GL_DISABLE",
  "GDK_SCALE",
  "GDK_SYNCHRONIZE",
  "GDK_VULKAN_DISABLE",
  "GDK_WAYLAND_DISABLE",
  "GDK_WIN32_CAIRO_DB",
  "GDK_WIN32_DISABLE_HIDPI",
  "GDK_WIN32_PER_MONITOR_HIDPI",
  "GDK_WIN32_TABLET_INPUT_API",
  "GOBJECT_DEBUG",
  "GSETINGS_SCHEMA_DIR",
  "GSK_CACHE_TIMEOUT",
  "GSK_DEBUG",
  "GSK_GPU_DISABLE",
  "GSK_RENDERER",
  "GSK_SUBSET_FONTS",
  "GTK_A11Y",
  "GTK_CSD",
  "GTK_CSS_DEBUG",
  "GTK_DATA_PREFIX",
  "GTK_DEBUG",
  "GTK_DEBUG_AUTO_QUIT",
  "GTK_EXE_PREFIX",
  "GTK_IM_MODULE",
  "GTK_INSPECTOR_DISPLAY",
  "GTK_INSPECTOR_RENDERER",
  "GTK_MEDIA",
  "GTK_PATH",
  "GTK_RTL",
  "GTK_SLOWDOWN",
  "GTK_THEME",
  "GTK_WIDGET_ASSERT_COMPONENTS",
  "LANG",
  "LANGUAGE",
  "LC_ALL",
  "LC_CTYPE",
  "LIBGL_ALWAYS_SOFTWARE",
  "LPDEST",
  "MESA_VK_DEVICE_SELECT",
  "PANGOCAIRO_BACKEND",
  "PANGO_LANGUAGE",
  "PRINTER",
  "SECMEM_FORCE_FALLBACK",
  "WAYLAND_DISPLAY",
  "XDG_ACTIVATION_TOKEN",
  "XDG_DATA_HOME",
  "XDG_DATA_DIRS",
  "XDG_RUNTIME_DIR",
};

static void
init_env (GtkInspectorGeneral *gen)
{
  set_monospace_font (gen->prefix);
  gtk_label_set_text (GTK_LABEL (gen->prefix), _gtk_get_data_prefix ());

  for (guint i = 0; i < G_N_ELEMENTS (env_list); i++)
    {
      const char *val = g_getenv (env_list[i]);

      if (val)
        {
          GtkStringPair *pair = gtk_string_pair_new (env_list[i], val);
          g_list_store_append (gen->environment_list, pair);
          g_object_unref (pair);
        }
    }
}

static void
dump_env (GdkDisplay *display,
          GString    *s)
{
  g_string_append_printf (s, "| Prefix | %s |\n", _gtk_get_data_prefix ());

  g_string_append (s, "| Environment| ");
  for (guint i = 0; i < G_N_ELEMENTS (env_list); i++)
    {
      const char *val = g_getenv (env_list[i]);

      if (val)
        g_string_append_printf (s, "%s%s=%s", i > 0 ? "<br>" : "", env_list[i], val);
    }
  g_string_append (s, " |\n");
}

/* }}} */
/* {{{ Display */

static const char *
translate_subpixel_layout (GdkSubpixelLayout subpixel)
{
  switch (subpixel)
    {
    case GDK_SUBPIXEL_LAYOUT_NONE: return "none";
    case GDK_SUBPIXEL_LAYOUT_UNKNOWN: return "unknown";
    case GDK_SUBPIXEL_LAYOUT_HORIZONTAL_RGB: return "horizontal rgb";
    case GDK_SUBPIXEL_LAYOUT_HORIZONTAL_BGR: return "horizontal bgr";
    case GDK_SUBPIXEL_LAYOUT_VERTICAL_RGB: return "vertical rgb";
    case GDK_SUBPIXEL_LAYOUT_VERTICAL_BGR: return "vertical bgr";
    default: g_assert_not_reached (); return "none";
    }
}

#ifdef GDK_WINDOWING_WAYLAND
static void
append_wayland_protocol_row (GtkInspectorGeneral *gen,
                             struct wl_proxy     *proxy)
{
  const char *id;
  unsigned int version;
  char buf[64];
  GtkListBox *list;

  if (proxy == NULL)
    return;

  list = GTK_LIST_BOX (gen->display_extensions_box);

  id = wl_proxy_get_class (proxy);
  version = wl_proxy_get_version (proxy);

  g_snprintf (buf, sizeof (buf), "%u", version);

  add_label_row (gen, list, id, buf, 10);
}

static void
append_wayland_protocol (GString         *string,
                         struct wl_proxy *proxy,
                         guint           *count)
{
  if (proxy == NULL)
    return;

  g_string_append_printf (string, "%s%s (%u)",
                          *count == 0 ? "" : "<br>",
                          wl_proxy_get_class (proxy),
                          wl_proxy_get_version (proxy));
  (*count)++;
}

static void
add_wayland_protocols (GdkDisplay          *display,
                       GtkInspectorGeneral *gen)
{
  if (GDK_IS_WAYLAND_DISPLAY (display))
    {
      GdkWaylandDisplay *d = (GdkWaylandDisplay *) display;

      append_wayland_protocol_row (gen, (struct wl_proxy *)d->compositor);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->shm);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->linux_dmabuf);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_wm_base);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->zxdg_shell_v6);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->gtk_shell);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->data_device_manager);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->subcompositor);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->pointer_gestures);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->primary_selection_manager);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->tablet_manager);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_exporter);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_exporter_v2);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_importer);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_importer_v2);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->keyboard_shortcuts_inhibit);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->server_decoration_manager);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_output_manager);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->idle_inhibit_manager);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_activation);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->fractional_scale);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->viewporter);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->presentation);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->single_pixel_buffer);
      append_wayland_protocol_row (gen, d->color ? gdk_wayland_color_get_color_manager (d->color) : NULL);
      append_wayland_protocol_row (gen, d->color ? gdk_wayland_color_get_color_representation_manager (d->color) : NULL);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->system_bell);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->cursor_shape);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->toplevel_icon);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xx_session_manager);
      append_wayland_protocol_row (gen, gtk_im_context_wayland_get_text_protocol (display));
    }
}

static void
dump_wayland_protocols (GdkDisplay *display,
                        GString    *string)
{
  if (GDK_IS_WAYLAND_DISPLAY (display))
    {
      GdkWaylandDisplay *d = (GdkWaylandDisplay *) display;
      guint count = 0;

      g_string_append (string, "| Protocols | ");

      append_wayland_protocol (string, (struct wl_proxy *)d->compositor, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->shm, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->linux_dmabuf, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->xdg_wm_base, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->zxdg_shell_v6, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->gtk_shell, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->data_device_manager, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->subcompositor, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->pointer_gestures, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->primary_selection_manager, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->tablet_manager, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->xdg_exporter, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->xdg_exporter_v2, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->xdg_importer, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->xdg_importer_v2, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->keyboard_shortcuts_inhibit, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->server_decoration_manager, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->xdg_output_manager, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->idle_inhibit_manager, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->xdg_activation, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->fractional_scale, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->viewporter, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->presentation, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->single_pixel_buffer, &count);
      append_wayland_protocol (string, d->color ? gdk_wayland_color_get_color_manager (d->color) : NULL, &count);
      append_wayland_protocol (string, d->color ? gdk_wayland_color_get_color_representation_manager (d->color) : NULL, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->system_bell, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->cursor_shape, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->toplevel_icon, &count);
      append_wayland_protocol (string, (struct wl_proxy *)d->xx_session_manager, &count);
      append_wayland_protocol (string , gtk_im_context_wayland_get_text_protocol (display), &count);

      g_string_append (string, " |\n");
    }
}
#endif

static void
populate_display (GdkDisplay *display, GtkInspectorGeneral *gen)
{
  GList *children, *l;
  GtkWidget *child;
  GtkListBox *list;

  list = GTK_LIST_BOX (gen->display_box);
  children = NULL;
  for (child = gtk_widget_get_first_child (GTK_WIDGET (list));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_LIST_BOX_ROW (child))
        children = g_list_prepend (children, child);
    }
  for (l = children; l; l = l->next)
    {
      child = l->data;
      if (gtk_widget_is_ancestor (gen->display_name, child) ||
          gtk_widget_is_ancestor (gen->display_rgba, child) ||
          gtk_widget_is_ancestor (gen->display_composited, child) ||
          child == gen->display_extensions_row)
        continue;

      gtk_list_box_remove (list, child);
    }
  g_list_free (children);

  gtk_label_set_label (GTK_LABEL (gen->display_name), gdk_display_get_name (display));

  gtk_widget_set_visible (gen->display_rgba,
                          gdk_display_is_rgba (display));
  gtk_widget_set_visible (gen->display_composited,
                          gdk_display_is_composited (display));

  list = GTK_LIST_BOX (gen->display_extensions_box);
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (list))) != NULL)
    gtk_list_box_remove (list, child);

#ifdef GDK_WINDOWING_WAYLAND
  add_wayland_protocols (display, gen);
#endif

  gtk_widget_set_visible (gen->display_extensions_row,
                          gtk_widget_get_first_child (gen->display_extensions_box) != NULL);
}

static void
populate_display_notify_cb (GdkDisplay          *display,
                            GParamSpec          *pspec,
                            GtkInspectorGeneral *gen)
{
  populate_display (display, gen);
}

static void
init_display (GtkInspectorGeneral *gen)
{
  g_signal_connect (gen->display, "notify", G_CALLBACK (populate_display_notify_cb), gen);

  populate_display (gen->display, gen);
}

static void
dump_display (GdkDisplay *display,
              GString    *string)
{
  g_string_append_printf (string, "| Display | %s |\n", gdk_display_get_name (display));
  g_string_append_printf (string, "| RGBA Visual | %s |\n", gdk_display_is_rgba (display) ? "✓" : "✗");
  g_string_append_printf (string, "| Composited | %s |\n", gdk_display_is_composited (display) ? "✓" : "✗");
#ifdef GDK_WINDOWING_WAYLAND
  dump_wayland_protocols (display, string);
#endif
}

/* }}} */
/* {{{ Monitors */

static void
add_monitor (GtkInspectorGeneral *gen,
             GdkMonitor          *monitor,
             guint                i)
{
  GtkListBox *list;
  char *value;
  GdkRectangle rect;
  double scale;
  char *name;
  char *scale_str = NULL;
  const char *manufacturer;
  const char *model;

  list = GTK_LIST_BOX (gen->monitor_box);

  name = g_strdup_printf ("Monitor %u", i);
  manufacturer = gdk_monitor_get_manufacturer (monitor);
  model = gdk_monitor_get_model (monitor);
  value = g_strdup_printf ("%s%s%s",
                           manufacturer ? manufacturer : "",
                           manufacturer || model ? " " : "",
                           model ? model : "");
  add_label_row (gen, list, name, value, 0);
  g_free (value);
  g_free (name);

  add_label_row (gen, list, "Description", gdk_monitor_get_description (monitor), 10);
  add_label_row (gen, list, "Connector", gdk_monitor_get_connector (monitor), 10);

  gdk_monitor_get_geometry (monitor, &rect);
  scale = gdk_monitor_get_scale (monitor);
  if (scale != 1.0)
    scale_str = g_strdup_printf (" @ %.2f", scale);

  value = g_strdup_printf ("%d × %d%s at %d, %d",
                           rect.width, rect.height,
                           scale_str ? scale_str : "",
                           rect.x, rect.y);
  add_label_row (gen, list, "Geometry", value, 10);
  g_free (value);
  g_free (scale_str);

  value = g_strdup_printf ("%d × %d",
                           (int) (rect.width * scale),
                           (int) (rect.height * scale));
  add_label_row (gen, list, "Pixels", value, 10);
  g_free (value);

  value = g_strdup_printf ("%d × %d mm²",
                           gdk_monitor_get_width_mm (monitor),
                           gdk_monitor_get_height_mm (monitor));
  add_label_row (gen, list, "Size", value, 10);
  g_free (value);

  value = g_strdup_printf ("%.1f dpi", gdk_monitor_get_dpi (monitor));
  add_label_row (gen, list, "Resolution", value, 10);
  g_free (value);

  if (gdk_monitor_get_refresh_rate (monitor) != 0)
    {
      value = g_strdup_printf ("%.2f Hz",
                               0.001 * gdk_monitor_get_refresh_rate (monitor));
      add_label_row (gen, list, "Refresh rate", value, 10);
      g_free (value);
    }

  if (gdk_monitor_get_subpixel_layout (monitor) != GDK_SUBPIXEL_LAYOUT_UNKNOWN)
    {
      add_label_row (gen, list, "Subpixel layout",
                     translate_subpixel_layout (gdk_monitor_get_subpixel_layout (monitor)),
                     10);
    }
}

static void
dump_monitor (GdkMonitor *monitor,
              guint       i,
              GString    *string)
{
  const char *manufacturer;
  const char *model;
  char *value;
  GdkRectangle rect;
  double scale;
  char *scale_str = NULL;

  manufacturer = gdk_monitor_get_manufacturer (monitor);
  model = gdk_monitor_get_model (monitor);
  value = g_strdup_printf ("%s%s%s",
                           manufacturer ? manufacturer : "",
                           manufacturer || model ? " " : "",
                           model ? model : "");
  g_string_append_printf (string, "| Monitor %u | %s |\n", i, value);
  g_free (value);

  g_string_append_printf (string, "| Description | %s |\n", gdk_monitor_get_description (monitor));
  g_string_append_printf (string, "| Connector | %s |\n", gdk_monitor_get_connector (monitor));

  gdk_monitor_get_geometry (monitor, &rect);
  scale = gdk_monitor_get_scale (monitor);
  if (scale != 1.0)
    scale_str = g_strdup_printf (" @ %.2f", scale);

  value = g_strdup_printf ("%d × %d%s at %d, %d",
                           rect.width, rect.height,
                           scale_str ? scale_str : "",
                           rect.x, rect.y);
  g_string_append_printf (string, "| Geometry | %s |\n", value);
  g_free (value);
  g_free (scale_str);

  value = g_strdup_printf ("%d × %d",
                           (int) (rect.width * scale),
                           (int) (rect.height * scale));
  g_string_append_printf (string, "| Pixels | %s |\n", value);
  g_free (value);

  value = g_strdup_printf ("%d × %d mm²",
                           gdk_monitor_get_width_mm (monitor),
                           gdk_monitor_get_height_mm (monitor));
  g_string_append_printf (string, "| Size | %s |\n", value);
  g_free (value);

  value = g_strdup_printf ("%.1f dpi", gdk_monitor_get_dpi (monitor));
  g_string_append_printf (string, "| Resolution | %s |\n", value);
  g_free (value);

  if (gdk_monitor_get_refresh_rate (monitor) != 0)
    {
      value = g_strdup_printf ("%.2f Hz",
                               0.001 * gdk_monitor_get_refresh_rate (monitor));
      g_string_append_printf (string, "| Refresh Rate | %s |\n", value);
      g_free (value);
    }

  if (gdk_monitor_get_subpixel_layout (monitor) != GDK_SUBPIXEL_LAYOUT_UNKNOWN)
    {
      g_string_append_printf (string, "| Subpixel Layout | %s |\n",
                              translate_subpixel_layout (gdk_monitor_get_subpixel_layout (monitor)));
    }
}

static void
populate_monitors (GdkDisplay          *display,
                   GtkInspectorGeneral *gen)
{
  GtkWidget *child;
  GListModel *list;

  while ((child = gtk_widget_get_first_child (gen->monitor_box)))
    gtk_list_box_remove (GTK_LIST_BOX (gen->monitor_box), child);

  list = gdk_display_get_monitors (gen->display);

  for (guint i = 0; i < g_list_model_get_n_items (list); i++)
    {
      GdkMonitor *monitor = g_list_model_get_item (list, i);
      add_monitor (gen, monitor, i);
      g_object_unref (monitor);
    }
}

static void
dump_monitors (GdkDisplay *display,
               GString    *string)
{
  GListModel *list;

  list = gdk_display_get_monitors (display);

  for (guint i = 0; i < g_list_model_get_n_items (list); i++)
    {
      GdkMonitor *monitor = g_list_model_get_item (list, i);
      dump_monitor (monitor, i, string);
      g_object_unref (monitor);
    }
}

static void
monitors_changed_cb (GListModel          *monitors,
                     guint                position,
                     guint                removed,
                     guint                added,
                     GtkInspectorGeneral *gen)
{
  populate_monitors (gen->display, gen);
}

static void
init_monitors (GtkInspectorGeneral *gen)
{
  g_signal_connect (gdk_display_get_monitors (gen->display), "items-changed",
                    G_CALLBACK (monitors_changed_cb), gen);

  populate_monitors (gen->display, gen);
}

/* }}} */
/* {{{ Seats */

static void populate_seats (GtkInspectorGeneral *gen);

static void
add_tool (GtkInspectorGeneral *gen,
          GdkDeviceTool       *tool)
{
  GdkAxisFlags axes;
  GString *str;
  char *val;
  GEnumClass *eclass;
  GEnumValue *evalue;
  GFlagsClass *fclass;
  GFlagsValue *fvalue;

  val = g_strdup_printf ("Serial %" G_GUINT64_FORMAT, gdk_device_tool_get_serial (tool));
  add_label_row (gen, GTK_LIST_BOX (gen->device_box), "Tool", val, 10);
  g_free (val);

  eclass = g_type_class_ref (GDK_TYPE_DEVICE_TOOL_TYPE);
  evalue = g_enum_get_value (eclass, gdk_device_tool_get_tool_type (tool));
  add_label_row (gen, GTK_LIST_BOX (gen->device_box), "Type", evalue->value_nick, 20);
  g_type_class_unref (eclass);

  fclass = g_type_class_ref (GDK_TYPE_AXIS_FLAGS);
  str = g_string_new ("");
  axes = gdk_device_tool_get_axes (tool);
  while ((fvalue = g_flags_get_first_value (fclass, axes)))
    {
      if (str->len > 0)
        g_string_append (str, ", ");
      g_string_append (str, fvalue->value_nick);
      axes &= ~fvalue->value;
    }
  g_type_class_unref (fclass);

  if (str->len > 0)
    add_label_row (gen, GTK_LIST_BOX (gen->device_box), "Axes", str->str, 20);

  g_string_free (str, TRUE);
}

static void
dump_tool (GdkDeviceTool *tool,
           GString       *string)
{
  GdkAxisFlags axes;
  GString *str;
  char *val;
  GEnumClass *eclass;
  GEnumValue *evalue;
  GFlagsClass *fclass;
  GFlagsValue *fvalue;

  val = g_strdup_printf ("Serial %" G_GUINT64_FORMAT, gdk_device_tool_get_serial (tool));
  g_string_append_printf (string, "| Tool | %s |\n", val);
  g_free (val);

  eclass = g_type_class_ref (GDK_TYPE_DEVICE_TOOL_TYPE);
  evalue = g_enum_get_value (eclass, gdk_device_tool_get_tool_type (tool));
  g_string_append_printf (string, "| Type | %s |\n", evalue->value_nick);
  g_type_class_unref (eclass);

  fclass = g_type_class_ref (GDK_TYPE_AXIS_FLAGS);
  str = g_string_new ("");
  axes = gdk_device_tool_get_axes (tool);
  while ((fvalue = g_flags_get_first_value (fclass, axes)))
    {
      if (str->len > 0)
        g_string_append (str, "<br>");
      g_string_append (str, fvalue->value_nick);
      axes &= ~fvalue->value;
    }
  g_type_class_unref (fclass);

  if (str->len > 0)
    g_string_append_printf (string, "| Axes | %s |\n", str->str);

  g_string_free (str, TRUE);
}

static void
add_device (GtkInspectorGeneral *gen,
            GdkDevice           *device)
{
  const char *name;
  guint n_touches;
  char *text;
  GEnumClass *class;
  GEnumValue *value;

  name = gdk_device_get_name (device);

  class = g_type_class_ref (GDK_TYPE_INPUT_SOURCE);
  value = g_enum_get_value (class, gdk_device_get_source (device));

  add_label_row (gen, GTK_LIST_BOX (gen->device_box), name, value->value_nick, 10);

  g_object_get (device, "num-touches", &n_touches, NULL);
  if (n_touches > 0)
    {
      text = g_strdup_printf ("%d", n_touches);
      add_label_row (gen, GTK_LIST_BOX (gen->device_box), "Touches", text, 20);
      g_free (text);
    }

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      GString *s;
      char **layout_names;

      s = g_string_new ("");
      layout_names = gdk_device_get_layout_names (device);
      if (layout_names)
        {
          int n_layouts, active_layout;

          active_layout = gdk_device_get_active_layout_index (device);
          n_layouts = g_strv_length (layout_names);
          for (int i = 0; i < n_layouts; i++)
            {
              if (s->len > 0)
                g_string_append (s, ", ");
              g_string_append (s, layout_names[i]);
              if (i == active_layout)
                g_string_append (s, "*");
            }
        }
      else
        {
          g_string_append (s, "Unknown");
        }

      add_label_row (gen, GTK_LIST_BOX (gen->device_box), "Layouts", s->str, 20);
      g_string_free (s, TRUE);
    }

  g_type_class_unref (class);
}

static void
dump_device (GdkDevice *device,
             GString   *string)
{
  const char *name;
  guint n_touches;
  GEnumClass *class;
  GEnumValue *value;

  name = gdk_device_get_name (device);

  class = g_type_class_ref (GDK_TYPE_INPUT_SOURCE);
  value = g_enum_get_value (class, gdk_device_get_source (device));

  g_string_append_printf (string, "| %s | %s |\n", name, value->value_nick);

  g_object_get (device, "num-touches", &n_touches, NULL);
  if (n_touches > 0)
    g_string_append_printf (string, "| Touches | %d |\n", n_touches);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      char **layout_names;
      int n_layouts, active_layout;
      GString *s;

      layout_names = gdk_device_get_layout_names (device);
      active_layout = gdk_device_get_active_layout_index (device);
      n_layouts = g_strv_length (layout_names);
      s = g_string_new ("");
      for (int i = 0; i < n_layouts; i++)
        {
          if (s->len > 0)
            g_string_append (s, "<br>");
          g_string_append (s, layout_names[i]);
          if (i == active_layout)
            g_string_append (s, "*");
        }

       g_string_append_printf (string, "| Layouts | %s |\n", s->str);
       g_string_free (s, TRUE);
    }

  g_type_class_unref (class);
}

static char *
get_seat_capabilities (GdkSeat *seat)
{
  struct {
    GdkSeatCapabilities cap;
    const char *name;
  } caps[] = {
    { GDK_SEAT_CAPABILITY_POINTER,       "Pointer" },
    { GDK_SEAT_CAPABILITY_TOUCH,         "Touch" },
    { GDK_SEAT_CAPABILITY_TABLET_STYLUS, "Tablet" },
    { GDK_SEAT_CAPABILITY_KEYBOARD,      "Keyboard" },
    { 0, NULL }
  };
  GString *str;
  GdkSeatCapabilities capabilities;
  int i;

  str = g_string_new ("");
  capabilities = gdk_seat_get_capabilities (seat);
  for (i = 0; caps[i].cap != 0; i++)
    {
      if (capabilities & caps[i].cap)
        {
          if (str->len > 0)
            g_string_append (str, ", ");
          g_string_append (str, caps[i].name);
        }
    }

  return g_string_free (str, FALSE);
}

static void
add_seat (GtkInspectorGeneral *gen,
          GdkSeat             *seat,
          int                  num)
{
  char *text;
  char *caps;
  GList *list, *l;

  if (!g_object_get_data (G_OBJECT (seat), "inspector-connected"))
    {
      g_object_set_data (G_OBJECT (seat), "inspector-connected", GINT_TO_POINTER (1));
      g_signal_connect_swapped (seat, "device-added", G_CALLBACK (populate_seats), gen);
      g_signal_connect_swapped (seat, "device-removed", G_CALLBACK (populate_seats), gen);
      g_signal_connect_swapped (seat, "tool-added", G_CALLBACK (populate_seats), gen);
      g_signal_connect_swapped (seat, "tool-removed", G_CALLBACK (populate_seats), gen);
    }

  text = g_strdup_printf ("Seat %d", num);
  caps = get_seat_capabilities (seat);

  add_label_row (gen, GTK_LIST_BOX (gen->device_box), text, caps, 0);
  g_free (text);
  g_free (caps);

  list = gdk_seat_get_devices (seat, GDK_SEAT_CAPABILITY_ALL);

  for (l = list; l; l = l->next)
    add_device (gen, GDK_DEVICE (l->data));

  g_list_free (list);

  list = gdk_seat_get_tools (seat);

  for (l = list; l; l = l->next)
    add_tool (gen, l->data);

  g_list_free (list);
}

static void
dump_seat (GdkSeat *seat,
           int      i,
           GString *string)
{
  char *caps;
  GList *list, *l;

  caps = get_seat_capabilities (seat);
  g_string_append_printf (string, "| Seat %d | %s |\n", i, caps);
  g_free (caps);

  list = gdk_seat_get_devices (seat, GDK_SEAT_CAPABILITY_ALL);

  for (l = list; l; l = l->next)
    dump_device (GDK_DEVICE (l->data), string);

  g_list_free (list);

  list = gdk_seat_get_tools (seat);

  for (l = list; l; l = l->next)
    dump_tool (l->data, string);

  g_list_free (list);
}

static void
disconnect_seat (GtkInspectorGeneral *gen,
                 GdkSeat             *seat)
{
  g_signal_handlers_disconnect_by_func (seat, G_CALLBACK (populate_seats), gen);
}

static void
populate_seats (GtkInspectorGeneral *gen)
{
  GtkWidget *child;
  GList *list, *l;
  int i;

  while ((child = gtk_widget_get_first_child (gen->device_box)))
    gtk_list_box_remove (GTK_LIST_BOX (gen->device_box), child);

  list = gdk_display_list_seats (gen->display);

  for (l = list, i = 0; l; l = l->next, i++)
    add_seat (gen, GDK_SEAT (l->data), i);

  g_list_free (list);
}

static void
dump_seats (GdkDisplay *display,
            GString    *string)
{
  GList *list, *l;
  int i;

  list = gdk_display_list_seats (display);

  for (l = list, i = 0; l; l = l->next, i++)
    dump_seat (GDK_SEAT (l->data), i, string);

  g_list_free (list);
}

static void
seat_added (GdkDisplay          *display,
            GdkSeat             *seat,
            GtkInspectorGeneral *gen)
{
  populate_seats (gen);
}

static void
seat_removed (GdkDisplay          *display,
              GdkSeat             *seat,
              GtkInspectorGeneral *gen)
{
  disconnect_seat (gen, seat);
  populate_seats (gen);
}

static void
init_seats (GtkInspectorGeneral *gen)
{
  g_signal_connect (gen->display, "seat-added", G_CALLBACK (seat_added), gen);
  g_signal_connect (gen->display, "seat-removed", G_CALLBACK (seat_removed), gen);

  populate_seats (gen);
}

 /* }}} */

static void
gtk_inspector_general_init (GtkInspectorGeneral *gen)
{
  gtk_widget_init_template (GTK_WIDGET (gen));
}

static gboolean
keynav_failed (GtkWidget *widget, GtkDirectionType direction, GtkInspectorGeneral *gen)
{
  GtkWidget *next;

  if (direction == GTK_DIR_DOWN && widget == gen->version_box)
    next = gen->env_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->env_box)
    next = gen->display_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->display_box)
    next = gen->monitor_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->monitor_box)
    next = gen->device_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->device_box)
    next = gen->gl_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->gl_box)
    next = gen->vulkan_box;
  else if (direction == GTK_DIR_UP && widget == gen->vulkan_box)
    next = gen->gl_box;
  else if (direction == GTK_DIR_UP && widget == gen->gl_box)
    next = gen->device_box;
  else if (direction == GTK_DIR_UP && widget == gen->device_box)
    next = gen->monitor_box;
  else if (direction == GTK_DIR_UP && widget == gen->monitor_box)
    next = gen->display_box;
  else if (direction == GTK_DIR_UP && widget == gen->display_box)
    next = gen->env_box;
  else if (direction == GTK_DIR_UP && widget == gen->env_box)
    next = gen->version_box;
  else
    next = NULL;

  if (next)
    {
      gtk_widget_child_focus (next, direction);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_inspector_general_constructed (GObject *object)
{
  GtkInspectorGeneral *gen = GTK_INSPECTOR_GENERAL (object);

  G_OBJECT_CLASS (gtk_inspector_general_parent_class)->constructed (object);

   g_signal_connect (gen->version_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->env_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->display_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->monitor_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->gl_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->vulkan_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->device_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
}

static void
gtk_inspector_general_dispose (GObject *object)
{
  GtkInspectorGeneral *gen = GTK_INSPECTOR_GENERAL (object);
  GList *list, *l;

  g_signal_handlers_disconnect_by_func (gen->display, G_CALLBACK (seat_added), gen);
  g_signal_handlers_disconnect_by_func (gen->display, G_CALLBACK (seat_removed), gen);
  g_signal_handlers_disconnect_by_func (gen->display, G_CALLBACK (populate_display_notify_cb), gen);
  g_signal_handlers_disconnect_by_func (gdk_display_get_monitors (gen->display), G_CALLBACK (monitors_changed_cb), gen);

  list = gdk_display_list_seats (gen->display);
  for (l = list; l; l = l->next)
    disconnect_seat (gen, GDK_SEAT (l->data));
  g_list_free (list);

  gtk_widget_dispose_template (GTK_WIDGET (gen), GTK_TYPE_INSPECTOR_GENERAL);

  G_OBJECT_CLASS (gtk_inspector_general_parent_class)->dispose (object);
}

static void
gtk_inspector_general_class_init (GtkInspectorGeneralClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (GTK_TYPE_STRING_PAIR);

  object_class->constructed = gtk_inspector_general_constructed;
  object_class->dispose = gtk_inspector_general_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/general.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, swin);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, version_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, env_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, display_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, display_extensions_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, display_extensions_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, monitor_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_features_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_features_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_extensions_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_extensions_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, egl_extensions_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, egl_extensions_row_name);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, egl_extensions_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vulkan_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vulkan_features_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vulkan_features_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vulkan_extensions_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vulkan_extensions_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vulkan_layers_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vulkan_layers_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, os_info);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gtk_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gdk_backend);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gsk_renderer);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, pango_fontmap);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, media_backend);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, im_module);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, a11y_backend);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_error);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_error_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_version_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_backend_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_backend_version_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_backend_vendor);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_backend_vendor_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_vendor);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_vendor_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_renderer);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_renderer_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_full_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_full_version_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, glsl_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, glsl_version_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vk_device);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vk_api_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vk_api_version_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vk_driver_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vk_driver_version_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vk_error);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vk_error_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, app_id_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, app_id);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, resource_path);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, prefix);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, environment_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, environment_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, display_name);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, display_composited);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, display_rgba);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, device_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, overlay);

  gtk_widget_class_bind_template_callback (widget_class, gtk_inspector_general_clip);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

void
gtk_inspector_general_set_display (GtkInspectorGeneral *gen,
                                   GdkDisplay *display)
{
  gen->display = display;

  init_os_info (gen);
  init_version (gen);
  init_pango (gen);
  init_media (gen);
  init_im_module (gen);
  init_a11y_backend (gen);
  init_app_id (gen);
  init_env (gen);
  init_display (gen);
  init_monitors (gen);
  init_seats (gen);
  init_gl (gen);
  init_vulkan (gen);
}

static char *
generate_dump (GdkDisplay *display)
{
  GString *string;

  string = g_string_new ("");

  g_string_append (string, "\n<details open=\"true\"><summary>General Information</summary>\n\n");
  g_string_append (string, "| Name | Value |\n");
  g_string_append (string, "| - | - |\n");
  dump_os_info (display, string);
  dump_version (display, string);
  dump_pango (display, string);
  dump_media (display, string);
  dump_im_module (display, string);
  dump_a11y_backend (display, string);
  g_string_append (string, "\n</details>\n");

  g_string_append (string, "\n<details><summary>Application</summary>\n\n");
  g_string_append (string, "| Name | Value |\n");
  g_string_append (string, "| - | - |\n");
  dump_app_id (display, string);
  g_string_append (string, "\n</details>\n");

  g_string_append (string, "<details><summary>Environment</summary>\n\n");
  g_string_append (string, "| Name | Value |\n");
  g_string_append (string, "| - | - |\n");
  dump_env (display, string);
  g_string_append (string, "\n</details>\n");

  g_string_append (string, "<details><summary>Display</summary>\n\n");
  g_string_append (string, "| Name | Value |\n");
  g_string_append (string, "| - | - |\n");
  dump_display (display, string);
  g_string_append (string, "\n</details>\n");

  g_string_append (string, "<details><summary>Monitors</summary>\n\n");
  g_string_append (string, "| Name | Value |\n");
  g_string_append (string, "| - | - |\n");
  dump_monitors (display, string);
  g_string_append (string, "\n</details>\n");

  g_string_append (string, "<details><summary>Seats</summary>\n\n");
  g_string_append (string, "| Name | Value |\n");
  g_string_append (string, "| - | - |\n");
  dump_seats (display, string);
  g_string_append (string, "\n</details>\n");

  g_string_append (string, "<details><summary>OpenGL</summary>\n\n");
  g_string_append (string, "| Name | Value |\n");
  g_string_append (string, "| - | - |\n");
  dump_gl (display, string);
  g_string_append (string, "\n</details>\n");

  g_string_append (string, "<details><summary>Vulkan</summary>\n\n");
  g_string_append (string, "| Name | Value |\n");
  g_string_append (string, "| - | - |\n");
  dump_vulkan (display, string);
  g_string_append (string, "\n</details>\n");

  return g_string_free (string, FALSE);
}

static void
gtk_inspector_general_clip (GtkButton           *button,
                            GtkInspectorGeneral *gen)
{
  char *text;
  GdkClipboard *clipboard;

  text = generate_dump (gen->display);

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (gen));
  gdk_clipboard_set_text (clipboard, text);

  g_free (text);
}

/* vim:set foldmethod=marker: */
