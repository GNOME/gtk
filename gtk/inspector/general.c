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

#ifdef GDK_RENDERING_VULKAN
#include <vulkan/vulkan.h>
#endif

struct _GtkInspectorGeneralPrivate
{
  GtkWidget *swin;
  GtkWidget *box;
  GtkWidget *version_box;
  GtkWidget *env_box;
  GtkWidget *display_box;
  GtkWidget *gl_box;
  GtkWidget *vulkan_box;
  GtkWidget *device_box;
  GtkWidget *gtk_version;
  GtkWidget *gdk_backend;
  GtkWidget *gsk_renderer;
  GtkWidget *pango_fontmap;
  GtkWidget *gl_version;
  GtkWidget *gl_vendor;
  GtkWidget *vk_device;
  GtkWidget *vk_api_version;
  GtkWidget *vk_driver_version;
  GtkWidget *prefix;
  GtkWidget *xdg_data_home;
  GtkWidget *xdg_data_dirs;
  GtkWidget *gtk_path;
  GtkWidget *gtk_exe_prefix;
  GtkWidget *gtk_data_prefix;
  GtkWidget *gsettings_schema_dir;
  GtkWidget *display_name;
  GtkWidget *display_rgba;
  GtkWidget *display_composited;
  GtkSizeGroup *labels;
  GtkAdjustment *focus_adjustment;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorGeneral, gtk_inspector_general, GTK_TYPE_WIDGET)

static void
init_version (GtkInspectorGeneral *gen)
{
  const char *backend;
  GdkDisplay *display;
  GdkSurface *surface;
  GskRenderer *gsk_renderer;
  const char *renderer;

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

  surface = gdk_surface_new_toplevel (display, 10, 10);
  gsk_renderer = gsk_renderer_new_for_surface (surface);
  if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskVulkanRenderer") == 0)
    renderer = "Vulkan";
  else if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskGLRenderer") == 0)
    renderer = "GL";
  else if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskCairoRenderer") == 0)
    renderer = "Cairo";
  else
    renderer = "Unknown";

  gsk_renderer_unrealize (gsk_renderer);
  g_object_unref (gsk_renderer);
  gdk_surface_destroy (surface);

  gtk_label_set_text (GTK_LABEL (gen->priv->gtk_version), GTK_VERSION);
  gtk_label_set_text (GTK_LABEL (gen->priv->gdk_backend), backend);
  gtk_label_set_text (GTK_LABEL (gen->priv->gsk_renderer), renderer);
}

static G_GNUC_UNUSED void
add_check_row (GtkInspectorGeneral *gen,
               GtkListBox          *list,
               const gchar         *name,
               gboolean             value,
               gint                 indent)
{
  GtkWidget *row, *box, *label, *check;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  g_object_set (box,
                "margin", 10,
                "margin-start", 10 + indent,
                NULL);

  label = gtk_label_new (name);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_container_add (GTK_CONTAINER (box), label);

  check = gtk_image_new_from_icon_name ("object-select-symbolic");
  gtk_widget_set_halign (check, GTK_ALIGN_END);
  gtk_widget_set_valign (check, GTK_ALIGN_BASELINE);
  gtk_widget_set_opacity (check, value ? 1.0 : 0.0);
  gtk_container_add (GTK_CONTAINER (box), check);

  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  gtk_widget_set_hexpand (box, FALSE);
  gtk_list_box_insert (list, row, -1);

  gtk_size_group_add_widget (GTK_SIZE_GROUP (gen->priv->labels), label);
}

static void
add_label_row (GtkInspectorGeneral *gen,
               GtkListBox          *list,
               const char          *name,
               const char          *value,
               gint                 indent)
{
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *row;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  g_object_set (box,
                "margin", 10,
                "margin-start", 10 + indent,
                NULL);

  label = gtk_label_new (name);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_container_add (GTK_CONTAINER (box), label);

  label = gtk_label_new (value);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_container_add (GTK_CONTAINER (box), label);

  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  gtk_widget_set_hexpand (box, FALSE);
  gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);

  gtk_size_group_add_widget (GTK_SIZE_GROUP (gen->priv->labels), label);
}

#ifdef GDK_WINDOWING_X11
static void
append_glx_extension_row (GtkInspectorGeneral *gen,
                          Display             *dpy,
                          const gchar         *ext)
{
  add_check_row (gen, GTK_LIST_BOX (gen->priv->gl_box), ext, epoxy_has_glx_extension (dpy, 0, ext), 0);
}
#endif

#ifdef GDK_WINDOWING_WAYLAND
static void
append_egl_extension_row (GtkInspectorGeneral *gen,
                          EGLDisplay          dpy,
                          const gchar         *ext)
{
  add_check_row (gen, GTK_LIST_BOX (gen->priv->gl_box), ext, epoxy_has_egl_extension (dpy, ext), 0);
}

static EGLDisplay
wayland_get_display (struct wl_display *wl_display)
{
  EGLDisplay dpy = NULL;

  if (epoxy_has_egl_extension (NULL, "EGL_KHR_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYPROC getPlatformDisplay =
        (void *) eglGetProcAddress ("eglGetPlatformDisplay");

      if (getPlatformDisplay)
        dpy = getPlatformDisplay (EGL_PLATFORM_WAYLAND_EXT,
                                  wl_display,
                                  NULL);
      if (dpy)
        return dpy;
    }

  if (epoxy_has_egl_extension (NULL, "EGL_EXT_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
        (void *) eglGetProcAddress ("eglGetPlatformDisplayEXT");

      if (getPlatformDisplay)
        dpy = getPlatformDisplay (EGL_PLATFORM_WAYLAND_EXT,
                                  wl_display,
                                  NULL);
      if (dpy)
        return dpy;
    }

  return eglGetDisplay ((EGLNativeDisplayType)wl_display);
}
#endif


static void
init_gl (GtkInspectorGeneral *gen)
{
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      GdkDisplay *display = gdk_display_get_default ();
      Display *dpy = GDK_DISPLAY_XDISPLAY (display);
      int error_base, event_base;
      gchar *version;
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
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      GdkDisplay *display = gdk_display_get_default ();
      EGLDisplay dpy;
      EGLint major, minor;
      gchar *version;

      dpy = wayland_get_display (gdk_wayland_display_get_wl_display (display));

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
      gtk_label_set_text (GTK_LABEL (gen->priv->gl_version), C_("GL version", "None"));
      gtk_label_set_text (GTK_LABEL (gen->priv->gl_vendor), C_("GL vendor", "None"));
    }
}

#ifdef GDK_RENDERING_VULKAN
static gboolean
has_debug_extension (GdkVulkanContext *context)
{
  uint32_t i;
  uint32_t n_extensions;
  vkEnumerateInstanceExtensionProperties (NULL, &n_extensions, NULL);
  VkExtensionProperties *extensions = g_newa (VkExtensionProperties, n_extensions);
  vkEnumerateInstanceExtensionProperties (NULL, &n_extensions, extensions);

  for (i = 0; i < n_extensions; i++)
    {
      if (g_str_equal (extensions[i].extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
        return TRUE;
    }

  return FALSE;
}

static gboolean
has_validation_layer (GdkVulkanContext *context)
{
  uint32_t i;
  uint32_t n_layers;
  vkEnumerateInstanceLayerProperties (&n_layers, NULL);
  VkLayerProperties *layers = g_newa (VkLayerProperties, n_layers);
  vkEnumerateInstanceLayerProperties (&n_layers, layers);

  for (i = 0; i < n_layers; i++)
    {
      if (g_str_equal (layers[i].layerName, "VK_LAYER_LUNARG_standard_validation"))
        return TRUE;
    }

  return FALSE;
}
#endif

static void
init_vulkan (GtkInspectorGeneral *gen)
{
#ifdef GDK_RENDERING_VULKAN
  GdkSurface *surface;
  GdkVulkanContext *context;
  GdkDisplay *display = gdk_display_get_default ();

  surface = gdk_surface_new_toplevel (display, 10, 10);
  context = gdk_surface_create_vulkan_context (surface, NULL);
  gdk_surface_destroy (surface);

  if (context)
    {
      VkPhysicalDevice vk_device;
      VkPhysicalDeviceProperties props;
      char *device_name;
      char *api_version;
      char *driver_version;

      vk_device = gdk_vulkan_context_get_physical_device (context);
      vkGetPhysicalDeviceProperties (vk_device, &props);

      device_name = g_strdup_printf ("%s (%d)", props.deviceName, props.deviceType);
      api_version = g_strdup_printf ("%d.%d.%d",
                                     VK_VERSION_MAJOR (props.apiVersion),
                                     VK_VERSION_MINOR (props.apiVersion),
                                     VK_VERSION_PATCH (props.apiVersion));
      driver_version = g_strdup_printf ("%d.%d.%d",
                                        VK_VERSION_MAJOR (props.driverVersion),
                                        VK_VERSION_MINOR (props.driverVersion),
                                        VK_VERSION_PATCH (props.driverVersion));

      gtk_label_set_text (GTK_LABEL (gen->priv->vk_device), device_name);
      gtk_label_set_text (GTK_LABEL (gen->priv->vk_api_version), api_version);
      gtk_label_set_text (GTK_LABEL (gen->priv->vk_driver_version), driver_version);

      g_free (device_name);
      g_free (api_version);
      g_free (driver_version);

      add_check_row (gen, GTK_LIST_BOX (gen->priv->vulkan_box), VK_KHR_SURFACE_EXTENSION_NAME, TRUE, 0);
#ifdef GDK_WINDOWING_X11
      if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
        add_check_row (gen, GTK_LIST_BOX (gen->priv->vulkan_box), "VK_KHR_xlib_surface", TRUE, 0);
#endif
#ifdef GDK_WINDOWING_WAYLAND
      if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
        add_check_row (gen, GTK_LIST_BOX (gen->priv->vulkan_box), "VK_KHR_wayland_surface", TRUE, 0);
#endif
      add_check_row (gen, GTK_LIST_BOX (gen->priv->vulkan_box), VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
                     has_debug_extension (context), 0);
      add_check_row (gen, GTK_LIST_BOX (gen->priv->vulkan_box), "VK_LAYER_LUNARG_standard_validation",
                     has_validation_layer (context), 0);

      g_object_unref (context);
    }
  else
#endif
    {
      gtk_label_set_text (GTK_LABEL (gen->priv->vk_device), C_("Vulkan device", "None"));
      gtk_label_set_text (GTK_LABEL (gen->priv->vk_api_version), C_("Vulkan version", "None"));
      gtk_label_set_text (GTK_LABEL (gen->priv->vk_driver_version), C_("Vulkan version", "None"));
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

static void
populate_display (GdkDisplay *display, GtkInspectorGeneral *gen)
{
  gint i;
  GList *children, *l;
  GtkWidget *child;
  int n_monitors;
  GtkListBox *list;

  gtk_widget_show (gen->priv->display_composited);
  list = GTK_LIST_BOX (gen->priv->display_box);
  children = gtk_container_get_children (GTK_CONTAINER (list));
  for (l = children; l; l = l->next)
    {
      child = l->data;
      if (gtk_widget_is_ancestor (gen->priv->display_name, child) ||
          gtk_widget_is_ancestor (gen->priv->display_rgba, child) ||
          gtk_widget_is_ancestor (gen->priv->display_composited, child))
        continue;

      gtk_widget_destroy (child);
    }
  g_list_free (children);

  gtk_label_set_label (GTK_LABEL (gen->priv->display_name), gdk_display_get_name (display));

  gtk_widget_set_visible (gen->priv->display_rgba,
                          gdk_display_is_rgba (display));
  gtk_widget_set_visible (gen->priv->display_composited,
                          gdk_display_is_composited (display));

  n_monitors = gdk_display_get_n_monitors (display);
  for (i = 0; i < n_monitors; i++)
    {
      gchar *name;
      gchar *value;
      GdkRectangle rect;
      gint scale;
      const char *manufacturer;
      const char *model;
      GdkMonitor *monitor;

      monitor = gdk_display_get_monitor (display, i);

      name = g_strdup_printf ("Monitor %d", i);
      manufacturer = gdk_monitor_get_manufacturer (monitor);
      model = gdk_monitor_get_model (monitor);
      value = g_strdup_printf ("%s%s%s",
                               manufacturer ? manufacturer : "",
                               manufacturer || model ? " " : "",
                               model ? model : "");
      add_label_row (gen, list, name, value, 0);
      g_free (name);
      g_free (value);

      gdk_monitor_get_geometry (monitor, &rect);
      scale = gdk_monitor_get_scale_factor (monitor);

      value = g_strdup_printf ("%d × %d%s at %d, %d",
                               rect.width, rect.height,
                               scale == 2 ? " @ 2" : "",
                               rect.x, rect.y);
      add_label_row (gen, list, "Geometry", value, 10);
      g_free (value);

      value = g_strdup_printf ("%d × %d mm²",
                               gdk_monitor_get_width_mm (monitor),
                               gdk_monitor_get_height_mm (monitor));
      add_label_row (gen, list, "Size", value, 10);
      g_free (value);

      add_check_row (gen, list, "Primary", gdk_monitor_is_primary (monitor), 10);

      if (gdk_monitor_get_refresh_rate (monitor) != 0)
        value = g_strdup_printf ("%.2f Hz",
                                 0.001 * gdk_monitor_get_refresh_rate (monitor));
      else
        value = g_strdup ("unknown");
      add_label_row (gen, list, "Refresh rate", value, 10);
      g_free (value);

      value = g_strdup (translate_subpixel_layout (gdk_monitor_get_subpixel_layout (monitor)));
      add_label_row (gen, list, "Subpixel layout", value, 10);
      g_free (value);
    }
}

static void
populate_display_notify_cb (GdkDisplay          *display,
                            GParamSpec          *pspec,
                            GtkInspectorGeneral *gen)
{
  populate_display (display, gen);
}

static void
populate_display_monitor_cb (GdkDisplay          *display,
                             GdkMonitor          *monitor,
                             GtkInspectorGeneral *gen)
{
  populate_display (display, gen);
}

static void
init_display (GtkInspectorGeneral *gen)
{
  GdkDisplay *display;

  display = gdk_display_get_default ();

  g_signal_connect (display, "notify", G_CALLBACK (populate_display_notify_cb), gen);
  g_signal_connect (display, "monitor-added", G_CALLBACK (populate_display_monitor_cb), gen);
  g_signal_connect (display, "monitor-removed", G_CALLBACK (populate_display_monitor_cb), gen);

  populate_display (display, gen);
}

static void
init_pango (GtkInspectorGeneral *gen)
{
  PangoFontMap *fontmap;
  const char *type;
  const char *name;

  fontmap = pango_cairo_font_map_get_default ();
  type = G_OBJECT_TYPE_NAME (fontmap);
  if (strcmp (type, "PangoCairoFcFontMap") == 0)
    name = "fontconfig";
  else if (strcmp (type, "PangoCairoCoreTextFontMap") == 0)
    name = "coretext";
  else if (strcmp (type, "PangoCairoWin32FontMap") == 0)
    name = "win32";
  else
    name = type;

  gtk_label_set_label (GTK_LABEL (gen->priv->pango_fontmap), name);
}

static void populate_seats (GtkInspectorGeneral *gen);

static void
add_device (GtkInspectorGeneral *gen,
            GdkDevice           *device)
{
  const gchar *name, *value;
  GString *str;
  int i;
  guint n_touches;
  gchar *text;
  GdkAxisFlags axes;
  const char *axis_name[] = {
    "Ignore",
    "X",
    "Y",
    "Pressure",
    "X Tilt",
    "Y Tilt",
    "Wheel",
    "Distance",
    "Rotation",
    "Slider"
  };
  const char *source_name[] = {
    "Mouse",
    "Pen",
    "Eraser",
    "Cursor",
    "Keyboard",
    "Touchscreen",
    "Touchpad",
    "Trackpoint"
  };

  name = gdk_device_get_name (device);
  value = source_name[gdk_device_get_source (device)];
  add_label_row (gen, GTK_LIST_BOX (gen->priv->device_box), name, value, 10);

  str = g_string_new ("");

  axes = gdk_device_get_axes (device);
  for (i = GDK_AXIS_X; i < GDK_AXIS_LAST; i++)
    {
      if ((axes & (1 << i)) != 0)
        {
          if (str->len > 0)
            g_string_append (str, ", ");
          g_string_append (str, axis_name[i]);
        }
    }

  if (str->len > 0)
    add_label_row (gen, GTK_LIST_BOX (gen->priv->device_box), "Axes", str->str, 20);

  g_string_free (str, TRUE);

  g_object_get (device, "num-touches", &n_touches, NULL);
  if (n_touches > 0)
    {
      text = g_strdup_printf ("%d", n_touches);
      add_label_row (gen, GTK_LIST_BOX (gen->priv->device_box), "Touches", text, 20);
      g_free (text);
    }
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
    }

  text = g_strdup_printf ("Seat %d", num);
  caps = get_seat_capabilities (seat);

  add_label_row (gen, GTK_LIST_BOX (gen->priv->device_box), text, caps, 0);
  g_free (text);
  g_free (caps);

  list = gdk_seat_get_slaves (seat, GDK_SEAT_CAPABILITY_ALL);

  for (l = list; l; l = l->next)
    add_device (gen, GDK_DEVICE (l->data));

  g_list_free (list);
}

static void
populate_seats (GtkInspectorGeneral *gen)
{
  GdkDisplay *display = gdk_display_get_default ();
  GList *list, *l;
  int i;

  list = gtk_container_get_children (GTK_CONTAINER (gen->priv->device_box));
  for (l = list; l; l = l->next)
    gtk_widget_destroy (GTK_WIDGET (l->data));
  g_list_free (list);

  list = gdk_display_list_seats (display);

  for (l = list, i = 0; l; l = l->next, i++)
    add_seat (gen, GDK_SEAT (l->data), i);

  g_list_free (list);
}

static void
init_device (GtkInspectorGeneral *gen)
{
  GdkDisplay *display = gdk_display_get_default ();

  g_signal_connect_swapped (display, "seat-added", G_CALLBACK (populate_seats), gen);
  g_signal_connect_swapped (display, "seat-removed", G_CALLBACK (populate_seats), gen);

  populate_seats (gen);
}

static void
gtk_inspector_general_init (GtkInspectorGeneral *gen)
{
  gen->priv = gtk_inspector_general_get_instance_private (gen);
  gtk_widget_init_template (GTK_WIDGET (gen));
  init_version (gen);
  init_env (gen);
  init_display (gen);
  init_pango (gen);
  init_gl (gen);
  init_vulkan (gen);
  init_device (gen);
}

static gboolean
keynav_failed (GtkWidget *widget, GtkDirectionType direction, GtkInspectorGeneral *gen)
{
  GtkWidget *next;
  gdouble value, lower, upper, page;

  if (direction == GTK_DIR_DOWN && widget == gen->priv->version_box)
    next = gen->priv->env_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->priv->env_box)
    next = gen->priv->display_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->priv->display_box)
    next = gen->priv->gl_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->priv->gl_box)
    next = gen->priv->vulkan_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->priv->vulkan_box)
    next = gen->priv->device_box;
  else if (direction == GTK_DIR_UP && widget == gen->priv->device_box)
    next = gen->priv->vulkan_box;
  else if (direction == GTK_DIR_UP && widget == gen->priv->vulkan_box)
    next = gen->priv->gl_box;
  else if (direction == GTK_DIR_UP && widget == gen->priv->gl_box)
    next = gen->priv->display_box;
  else if (direction == GTK_DIR_UP && widget == gen->priv->display_box)
    next = gen->priv->env_box;
  else if (direction == GTK_DIR_UP && widget == gen->priv->env_box)
    next = gen->priv->version_box;
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

  gen->priv->focus_adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (gen->priv->swin));
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (gen->priv->box),
                                       gen->priv->focus_adjustment);

   g_signal_connect (gen->priv->version_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->priv->env_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->priv->display_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->priv->gl_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->priv->vulkan_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->priv->device_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
}

static void
measure (GtkWidget      *widget,
         GtkOrientation  orientation,
         int             for_size,
         int            *minimum,
         int            *natural,
         int            *minimum_baseline,
         int            *natural_baseline)
{
  GtkInspectorGeneral *gen = GTK_INSPECTOR_GENERAL (widget);

  gtk_widget_measure (gen->priv->swin,
                      orientation,
                      for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
size_allocate (GtkWidget *widget,
               int        width,
               int        height,
               int        baseline)
{
  GtkInspectorGeneral *gen = GTK_INSPECTOR_GENERAL (widget);

  gtk_widget_size_allocate (gen->priv->swin,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);
}

static void
gtk_inspector_general_class_init (GtkInspectorGeneralClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gtk_inspector_general_constructed;

  widget_class->measure = measure;
  widget_class->size_allocate = size_allocate;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/general.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, swin);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, version_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, env_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, display_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gl_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, vulkan_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gtk_version);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gdk_backend);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gsk_renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, pango_fontmap);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gl_version);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gl_vendor);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, vk_device);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, vk_api_version);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, vk_driver_version);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, prefix);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, xdg_data_home);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, xdg_data_dirs);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gtk_path);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gtk_exe_prefix);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gtk_data_prefix);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, gsettings_schema_dir);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, labels);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, display_name);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, display_composited);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, display_rgba);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorGeneral, device_box);
}

// vim: set et sw=2 ts=2:
