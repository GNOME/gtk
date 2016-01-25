#include <gtk/gtk.h>

#include "iconbrowserapp.h"
#include "iconbrowserwin.h"

struct _IconBrowserApp
{
  GtkApplication parent;
};

struct _IconBrowserAppClass
{
  GtkApplicationClass parent_class;
};

G_DEFINE_TYPE(IconBrowserApp, icon_browser_app, GTK_TYPE_APPLICATION);

static void
icon_browser_app_init (IconBrowserApp *app)
{
}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       app)
{
  g_application_quit (G_APPLICATION (app));
}

static GActionEntry app_entries[] =
{
  { "quit", quit_activated, NULL, NULL, NULL }
};

static void
icon_browser_app_startup (GApplication *app)
{
  const gchar *quit_accels[2] = { "<Ctrl>Q", NULL };

  G_APPLICATION_CLASS (icon_browser_app_parent_class)->startup (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "app.quit",
                                         quit_accels);
}

static void
icon_browser_app_activate (GApplication *app)
{
  IconBrowserWindow *win;

  win = icon_browser_window_new (ICON_BROWSER_APP (app));
  gtk_window_present (GTK_WINDOW (win));
}

static void
icon_browser_app_class_init (IconBrowserAppClass *class)
{
  G_APPLICATION_CLASS (class)->startup = icon_browser_app_startup;
  G_APPLICATION_CLASS (class)->activate = icon_browser_app_activate;
}

IconBrowserApp *
icon_browser_app_new (void)
{
  return g_object_new (ICON_BROWSER_APP_TYPE,
                       "application-id", "org.gtk.IconBrowser",
                       NULL);
}
