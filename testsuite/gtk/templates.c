/* templates.c
 * Copyright (C) 2013 Openismus GmbH
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
 *
 * Authors: Tristan Van Berkom <tristanvb@openismus.com>
 */
#include <gtk/gtk.h>

#ifdef HAVE_UNIX_PRINT_WIDGETS
#  include <gtk/gtkunixprint.h>
#endif

static gboolean
main_loop_quit_cb (gpointer data)
{
  gtk_main_quit ();

  return FALSE;
}

static void
test_dialog_basic (void)
{
  GtkWidget *dialog;

  dialog = gtk_dialog_new();
  g_assert (GTK_IS_DIALOG (dialog));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_assert (gtk_dialog_get_action_area (GTK_DIALOG (dialog)) != NULL);
G_GNUC_END_IGNORE_DEPRECATIONS
  g_assert (gtk_dialog_get_content_area (GTK_DIALOG (dialog)) != NULL);

  gtk_widget_destroy (dialog);
}

static void
test_dialog_override_property (void)
{
  GtkWidget *dialog;

  dialog = g_object_new (GTK_TYPE_DIALOG,
			 "type-hint", GDK_WINDOW_TYPE_HINT_UTILITY,
			 NULL);
  g_assert (GTK_IS_DIALOG (dialog));
  g_assert (gtk_window_get_type_hint (GTK_WINDOW (dialog)) == GDK_WINDOW_TYPE_HINT_UTILITY);

  gtk_widget_destroy (dialog);
}

static void
test_message_dialog_basic (void)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (NULL, 0,
				   GTK_MESSAGE_INFO,
				   GTK_BUTTONS_CLOSE,
				   "Do it hard !");
  g_assert (GTK_IS_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
test_about_dialog_basic (void)
{
  GtkWidget *dialog;

  dialog = gtk_about_dialog_new ();
  g_assert (GTK_IS_ABOUT_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
test_info_bar_basic (void)
{
  GtkWidget *infobar;

  infobar = gtk_info_bar_new ();
  g_assert (GTK_IS_INFO_BAR (infobar));
  gtk_widget_destroy (infobar);
}

static void
test_lock_button_basic (void)
{
  GtkWidget *button;
  GPermission *permission;

  permission = g_simple_permission_new (TRUE);
  button = gtk_lock_button_new (permission);
  g_assert (GTK_IS_LOCK_BUTTON (button));
  gtk_widget_destroy (button);
  g_object_unref (permission);
}

static void
test_assistant_basic (void)
{
  GtkWidget *widget;

  widget = gtk_assistant_new ();
  g_assert (GTK_IS_ASSISTANT (widget));
  gtk_widget_destroy (widget);
}

static void
test_scale_button_basic (void)
{
  GtkWidget *widget;

  widget = gtk_scale_button_new (GTK_ICON_SIZE_MENU,
				 0, 100, 10, NULL);
  g_assert (GTK_IS_SCALE_BUTTON (widget));
  gtk_widget_destroy (widget);
}

static void
test_volume_button_basic (void)
{
  GtkWidget *widget;

  widget = gtk_volume_button_new ();
  g_assert (GTK_IS_VOLUME_BUTTON (widget));
  gtk_widget_destroy (widget);
}

static void
test_statusbar_basic (void)
{
  GtkWidget *widget;

  widget = gtk_statusbar_new ();
  g_assert (GTK_IS_STATUSBAR (widget));
  gtk_widget_destroy (widget);
}

static void
test_search_bar_basic (void)
{
  GtkWidget *widget;

  widget = gtk_search_bar_new ();
  g_assert (GTK_IS_SEARCH_BAR (widget));
  gtk_widget_destroy (widget);
}

static void
test_action_bar_basic (void)
{
  GtkWidget *widget;

  widget = gtk_action_bar_new ();
  g_assert (GTK_IS_ACTION_BAR (widget));
  gtk_widget_destroy (widget);
}

static void
test_app_chooser_widget_basic (void)
{
  GtkWidget *widget;

  widget = gtk_app_chooser_widget_new (NULL);
  g_assert (GTK_IS_APP_CHOOSER_WIDGET (widget));
  gtk_widget_destroy (widget);
}

static void
test_app_chooser_dialog_basic (void)
{
  GtkWidget *widget;

  widget = gtk_app_chooser_dialog_new_for_content_type (NULL, 0, "text/plain");
  g_assert (GTK_IS_APP_CHOOSER_DIALOG (widget));

  /* GtkAppChooserDialog bug, if destroyed before spinning 
   * the main context then app_chooser_online_get_default_ready_cb()
   * will be eventually called and segfault.
   */
  g_timeout_add (500, main_loop_quit_cb, NULL);
  gtk_main();
  gtk_widget_destroy (widget);
}

static void
test_color_chooser_dialog_basic (void)
{
  GtkWidget *widget;

  /* This test also tests the internal GtkColorEditor widget */
  widget = gtk_color_chooser_dialog_new (NULL, NULL);
  g_assert (GTK_IS_COLOR_CHOOSER_DIALOG (widget));
  gtk_widget_destroy (widget);
}

/* Avoid warnings from GVFS-RemoteVolumeMonitor */
static gboolean
ignore_gvfs_warning (const gchar *log_domain,
		     GLogLevelFlags log_level,
		     const gchar *message,
		     gpointer user_data)
{
  if (g_strcmp0 (log_domain, "GVFS-RemoteVolumeMonitor") == 0)
    return FALSE;

  return TRUE;
}

static void
test_file_chooser_widget_basic (void)
{
  GtkWidget *widget;

  /* This test also tests the internal GtkPathBar widget */
  g_test_log_set_fatal_handler (ignore_gvfs_warning, NULL);

  widget = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  g_assert (GTK_IS_FILE_CHOOSER_WIDGET (widget));

  /* XXX BUG:
   *
   * Spin the mainloop for a bit, this allows the file operations
   * to complete, GtkFileChooserWidget has a bug where it leaks
   * GtkTreeRowReferences to the internal shortcuts_model
   *
   * Since we assert all automated children are finalized we
   * can catch this
   */
  g_timeout_add (100, main_loop_quit_cb, NULL);
  gtk_main();

  gtk_widget_destroy (widget);
}

static void
test_file_chooser_dialog_basic (void)
{
  GtkWidget *widget;

  g_test_log_set_fatal_handler (ignore_gvfs_warning, NULL);

  widget = gtk_file_chooser_dialog_new ("The Dialog", NULL,
					GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					"_OK", GTK_RESPONSE_OK,
					NULL);

  g_assert (GTK_IS_FILE_CHOOSER_DIALOG (widget));
  g_timeout_add (100, main_loop_quit_cb, NULL);
  gtk_main();

  gtk_widget_destroy (widget);
}

static void
test_file_chooser_button_basic (void)
{
  GtkWidget *widget;

  g_test_log_set_fatal_handler (ignore_gvfs_warning, NULL);

  widget = gtk_file_chooser_button_new ("Choose a file !", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  g_assert (GTK_IS_FILE_CHOOSER_BUTTON (widget));
  g_timeout_add (100, main_loop_quit_cb, NULL);
  gtk_main();

  gtk_widget_destroy (widget);
}

static void
test_font_button_basic (void)
{
  GtkWidget *widget;

  widget = gtk_font_button_new ();
  g_assert (GTK_IS_FONT_BUTTON (widget));
  gtk_widget_destroy (widget);
}

static void
test_font_chooser_widget_basic (void)
{
  GtkWidget *widget;

  widget = gtk_font_chooser_widget_new ();
  g_assert (GTK_IS_FONT_CHOOSER_WIDGET (widget));
  gtk_widget_destroy (widget);
}

static void
test_font_chooser_dialog_basic (void)
{
  GtkWidget *widget;

  widget = gtk_font_chooser_dialog_new ("Choose a font !", NULL);
  g_assert (GTK_IS_FONT_CHOOSER_DIALOG (widget));
  gtk_widget_destroy (widget);
}

static void
test_recent_chooser_widget_basic (void)
{
  GtkWidget *widget;

  widget = gtk_recent_chooser_widget_new ();
  g_assert (GTK_IS_RECENT_CHOOSER_WIDGET (widget));
  gtk_widget_destroy (widget);
}

#ifdef HAVE_UNIX_PRINT_WIDGETS
static void
test_page_setup_unix_dialog_basic (void)
{
  GtkWidget *widget;

  widget = gtk_page_setup_unix_dialog_new ("Setup your Page !", NULL);
  g_assert (GTK_IS_PAGE_SETUP_UNIX_DIALOG (widget));
  gtk_widget_destroy (widget);
}

static void
test_print_unix_dialog_basic (void)
{
  GtkWidget *widget;

  widget = gtk_print_unix_dialog_new ("Go Print !", NULL);
  g_assert (GTK_IS_PRINT_UNIX_DIALOG (widget));
  gtk_widget_destroy (widget);
}
#endif

int
main (int argc, char **argv)
{
  gchar *schema_dir;

  /* These must be set before before gtk_test_init */
  g_setenv ("GIO_USE_VFS", "local", TRUE);
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  /* initialize test program */
  gtk_test_init (&argc, &argv);

  /* g_test_build_filename must be called after gtk_test_init */
  schema_dir = g_test_build_filename (G_TEST_BUILT, "", NULL);
  if (g_getenv ("GTK_TEST_MESON") == NULL)
    g_setenv ("GSETTINGS_SCHEMA_DIR", schema_dir, TRUE);

  /* This environment variable cooperates with gtk_widget_destroy()
   * to assert that all automated compoenents are properly finalized
   * when a given composite widget is destroyed.
   */
  g_assert (g_setenv ("GTK_WIDGET_ASSERT_COMPONENTS", "1", TRUE));

  g_test_add_func ("/Template/GtkDialog/Basic", test_dialog_basic);
  g_test_add_func ("/Template/GtkDialog/OverrideProperty", test_dialog_override_property);
  g_test_add_func ("/Template/GtkMessageDialog/Basic", test_message_dialog_basic);
  g_test_add_func ("/Template/GtkAboutDialog/Basic", test_about_dialog_basic);
  g_test_add_func ("/Template/GtkInfoBar/Basic", test_info_bar_basic);
  g_test_add_func ("/Template/GtkLockButton/Basic", test_lock_button_basic);
  g_test_add_func ("/Template/GtkAssistant/Basic", test_assistant_basic);
  g_test_add_func ("/Template/GtkScaleButton/Basic", test_scale_button_basic);
  g_test_add_func ("/Template/GtkVolumeButton/Basic", test_volume_button_basic);
  g_test_add_func ("/Template/GtkStatusBar/Basic", test_statusbar_basic);
  g_test_add_func ("/Template/GtkSearchBar/Basic", test_search_bar_basic);
  g_test_add_func ("/Template/GtkActionBar/Basic", test_action_bar_basic);
  g_test_add_func ("/Template/GtkAppChooserWidget/Basic", test_app_chooser_widget_basic);
  g_test_add_func ("/Template/GtkAppChooserDialog/Basic", test_app_chooser_dialog_basic);
  g_test_add_func ("/Template/GtkColorChooserDialog/Basic", test_color_chooser_dialog_basic);
  g_test_add_func ("/Template/GtkFileChooserWidget/Basic", test_file_chooser_widget_basic);
  g_test_add_func ("/Template/GtkFileChooserDialog/Basic", test_file_chooser_dialog_basic);
  g_test_add_func ("/Template/GtkFileChooserButton/Basic", test_file_chooser_button_basic);
  g_test_add_func ("/Template/GtkFontButton/Basic", test_font_button_basic);
  g_test_add_func ("/Template/GtkFontChooserWidget/Basic", test_font_chooser_widget_basic);
  g_test_add_func ("/Template/GtkFontChooserDialog/Basic", test_font_chooser_dialog_basic);
  g_test_add_func ("/Template/GtkRecentChooserWidget/Basic", test_recent_chooser_widget_basic);

#ifdef HAVE_UNIX_PRINT_WIDGETS
  g_test_add_func ("/Template/UnixPrint/GtkPageSetupUnixDialog/Basic", test_page_setup_unix_dialog_basic);
  g_test_add_func ("/Template/UnixPrint/GtkPrintUnixDialog/Basic", test_print_unix_dialog_basic);
#endif

  g_free (schema_dir);

  return g_test_run();
}
