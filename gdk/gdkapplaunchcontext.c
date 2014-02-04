/* gdkapplaunchcontext.c - Gtk+ implementation for GAppLaunchContext

   Copyright (C) 2007 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library. If not, see <http://www.gnu.org/licenses/>.

   Author: Alexander Larsson <alexl@redhat.com>
*/

#include "config.h"

#include "gdkapplaunchcontextprivate.h"
#include "gdkscreen.h"
#include "gdkintl.h"


/**
 * SECTION:gdkapplaunchcontext
 * @Short_description: Startup notification for applications
 * @Title: Application launching
 *
 * GdkAppLaunchContext is an implementation of #GAppLaunchContext that
 * handles launching an application in a graphical context. It provides
 * startup notification and allows to launch applications on a specific
 * screen or workspace.
 *
 * ## Launching an application
 *
 * |[<!-- language="C" -->
 * GdkAppLaunchContext *context;
 *
 * context = gdk_display_get_app_launch_context (display);
 *
 * gdk_app_launch_context_set_screen (screen);
 * gdk_app_launch_context_set_timestamp (event->time);
 *
 * if (!g_app_info_launch_default_for_uri ("http://www.gtk.org", context, &error))
 *   g_warning ("Launching failed: %s\n", error->message);
 *
 * g_object_unref (context);
 * ]|
 */


static void    gdk_app_launch_context_finalize    (GObject           *object);
static gchar * gdk_app_launch_context_get_display (GAppLaunchContext *context,
                                                   GAppInfo          *info,
                                                   GList             *files);
static gchar * gdk_app_launch_context_get_startup_notify_id (GAppLaunchContext *context,
                                                             GAppInfo          *info,
                                                             GList             *files);
static void    gdk_app_launch_context_launch_failed (GAppLaunchContext *context,
                                                     const gchar       *startup_notify_id);


enum
{
  PROP_0,
  PROP_DISPLAY
};

G_DEFINE_TYPE (GdkAppLaunchContext, gdk_app_launch_context, G_TYPE_APP_LAUNCH_CONTEXT)

static void
gdk_app_launch_context_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GdkAppLaunchContext *context = GDK_APP_LAUNCH_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, context->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_app_launch_context_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GdkAppLaunchContext *context = GDK_APP_LAUNCH_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      context->display = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_app_launch_context_class_init (GdkAppLaunchContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GAppLaunchContextClass *context_class = G_APP_LAUNCH_CONTEXT_CLASS (klass);

  gobject_class->set_property = gdk_app_launch_context_set_property,
  gobject_class->get_property = gdk_app_launch_context_get_property;

  gobject_class->finalize = gdk_app_launch_context_finalize;

  context_class->get_display = gdk_app_launch_context_get_display;
  context_class->get_startup_notify_id = gdk_app_launch_context_get_startup_notify_id;
  context_class->launch_failed = gdk_app_launch_context_launch_failed;

  g_object_class_install_property (gobject_class, PROP_DISPLAY,
    g_param_spec_object ("display", P_("Display"), P_("Display"),
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
gdk_app_launch_context_init (GdkAppLaunchContext *context)
{
  context->workspace = -1;
}

static void
gdk_app_launch_context_finalize (GObject *object)
{
  GdkAppLaunchContext *context = GDK_APP_LAUNCH_CONTEXT (object);

  if (context->display)
    g_object_unref (context->display);

  if (context->screen)
    g_object_unref (context->screen);

  if (context->icon)
    g_object_unref (context->icon);

  g_free (context->icon_name);

  G_OBJECT_CLASS (gdk_app_launch_context_parent_class)->finalize (object);
}

static gchar *
gdk_app_launch_context_get_display (GAppLaunchContext *context,
                                    GAppInfo          *info,
                                    GList             *files)
{
  GdkAppLaunchContext *ctx = GDK_APP_LAUNCH_CONTEXT (context);
  GdkDisplay *display;

  if (ctx->screen)
    return gdk_screen_make_display_name (ctx->screen);

  if (ctx->display)
    display = ctx->display;
  else
    display = gdk_display_get_default ();

  return g_strdup (gdk_display_get_name (display));
}

/**
 * gdk_app_launch_context_set_display:
 * @context: a #GdkAppLaunchContext
 * @display: a #GdkDisplay
 *
 * Sets the display on which applications will be launched when
 * using this context. See also gdk_app_launch_context_set_screen().
 *
 * Since: 2.14
 *
 * Deprecated: 3.0: Use gdk_display_get_app_launch_context() instead
 */
void
gdk_app_launch_context_set_display (GdkAppLaunchContext *context,
                                    GdkDisplay          *display)
{
  g_return_if_fail (GDK_IS_APP_LAUNCH_CONTEXT (context));
  g_return_if_fail (display == NULL || GDK_IS_DISPLAY (display));

  g_warn_if_fail (display == NULL || display == context->display);
}

/**
 * gdk_app_launch_context_set_screen:
 * @context: a #GdkAppLaunchContext
 * @screen: a #GdkScreen
 *
 * Sets the screen on which applications will be launched when
 * using this context. See also gdk_app_launch_context_set_display().
 *
 * If both @screen and @display are set, the @screen takes priority.
 * If neither @screen or @display are set, the default screen and
 * display are used.
 *
 * Since: 2.14
 */
void
gdk_app_launch_context_set_screen (GdkAppLaunchContext *context,
                                   GdkScreen           *screen)
{
  g_return_if_fail (GDK_IS_APP_LAUNCH_CONTEXT (context));
  g_return_if_fail (screen == NULL || GDK_IS_SCREEN (screen));

  g_return_if_fail (screen == NULL || gdk_screen_get_display (screen) == context->display);

  if (context->screen)
    {
      g_object_unref (context->screen);
      context->screen = NULL;
    }

  if (screen)
    context->screen = g_object_ref (screen);
}

/**
 * gdk_app_launch_context_set_desktop:
 * @context: a #GdkAppLaunchContext
 * @desktop: the number of a workspace, or -1
 *
 * Sets the workspace on which applications will be launched when
 * using this context when running under a window manager that
 * supports multiple workspaces, as described in the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec).
 *
 * When the workspace is not specified or @desktop is set to -1,
 * it is up to the window manager to pick one, typically it will
 * be the current workspace.
 *
 * Since: 2.14
 */
void
gdk_app_launch_context_set_desktop (GdkAppLaunchContext *context,
                                    gint                 desktop)
{
  g_return_if_fail (GDK_IS_APP_LAUNCH_CONTEXT (context));

  context->workspace = desktop;
}

/**
 * gdk_app_launch_context_set_timestamp:
 * @context: a #GdkAppLaunchContext
 * @timestamp: a timestamp
 *
 * Sets the timestamp of @context. The timestamp should ideally
 * be taken from the event that triggered the launch.
 *
 * Window managers can use this information to avoid moving the
 * focus to the newly launched application when the user is busy
 * typing in another window. This is also known as 'focus stealing
 * prevention'.
 *
 * Since: 2.14
 */
void
gdk_app_launch_context_set_timestamp (GdkAppLaunchContext *context,
                                      guint32              timestamp)
{
  g_return_if_fail (GDK_IS_APP_LAUNCH_CONTEXT (context));

  context->timestamp = timestamp;
}

/**
 * gdk_app_launch_context_set_icon:
 * @context: a #GdkAppLaunchContext
 * @icon: (allow-none): a #GIcon, or %NULL
 *
 * Sets the icon for applications that are launched with this
 * context.
 *
 * Window Managers can use this information when displaying startup
 * notification.
 *
 * See also gdk_app_launch_context_set_icon_name().
 *
 * Since: 2.14
 */
void
gdk_app_launch_context_set_icon (GdkAppLaunchContext *context,
                                 GIcon               *icon)
{
  g_return_if_fail (GDK_IS_APP_LAUNCH_CONTEXT (context));
  g_return_if_fail (icon == NULL || G_IS_ICON (icon));

  if (context->icon)
    {
      g_object_unref (context->icon);
      context->icon = NULL;
    }

  if (icon)
    context->icon = g_object_ref (icon);
}

/**
 * gdk_app_launch_context_set_icon_name:
 * @context: a #GdkAppLaunchContext
 * @icon_name: (allow-none): an icon name, or %NULL
 *
 * Sets the icon for applications that are launched with this context.
 * The @icon_name will be interpreted in the same way as the Icon field
 * in desktop files. See also gdk_app_launch_context_set_icon().
 *
 * If both @icon and @icon_name are set, the @icon_name takes priority.
 * If neither @icon or @icon_name is set, the icon is taken from either
 * the file that is passed to launched application or from the #GAppInfo
 * for the launched application itself.
 *
 * Since: 2.14
 */
void
gdk_app_launch_context_set_icon_name (GdkAppLaunchContext *context,
                                      const char          *icon_name)
{
  g_return_if_fail (GDK_IS_APP_LAUNCH_CONTEXT (context));

  g_free (context->icon_name);
  context->icon_name = g_strdup (icon_name);
}

/**
 * gdk_app_launch_context_new:
 *
 * Creates a new #GdkAppLaunchContext.
 *
 * Returns: a new #GdkAppLaunchContext
 *
 * Since: 2.14
 *
 * Deprecated: 3.0: Use gdk_display_get_app_launch_context() instead
 */
GdkAppLaunchContext *
gdk_app_launch_context_new (void)
{
  return gdk_display_get_app_launch_context (gdk_display_get_default ());
}

static char *
gdk_app_launch_context_get_startup_notify_id (GAppLaunchContext *context,
                                              GAppInfo          *info,
                                              GList             *files)
{
 return NULL;
}

static void
gdk_app_launch_context_launch_failed (GAppLaunchContext *context,
                                      const gchar       *startup_notify_id)
{
}
