#include <gtk/gtk.h>

static char *icon_names[] = {
  /*** stock icons, from gtkiconfactory.c:get_default_icons() ***/
  "application-exit",
  "dialog-error",
  "dialog-information",
  "dialog-password",
  "dialog-question",
  "dialog-warning",
  "document-new",
  "document-open",
  "document-print",
  "document-print-preview",
  "document-properties",
  "document-revert",
  "document-save",
  "document-save-as",
  "drive-harddisk",
  "edit-clear",
  "edit-copy",
  "edit-cut",
  "edit-delete",
  "edit-find",
  "edit-find-replace",
  "edit-paste",
  "edit-redo",
  "edit-select-all",
  "edit-undo",
  "folder",
  "format-indent-less",
  "format-indent-more",
  "format-justify-center",
  "format-justify-fill",
  "format-justify-left",
  "format-justify-right",
  "format-text-bold",
  "format-text-italic",
  "format-text-strikethrough",
  "format-text-underline",
  "go-bottom",
  "go-down",
  "go-first",
  "go-home",
  "go-jump",
  "go-top",
  "go-up",
  "go-last",
  "go-next",
  "go-previous",
  "help-about",
  "help-contents",
  "image-missing",
  "list-add",
  "list-remove",
  "media-floppy",
  "media-optical",
  "media-playback-pause",
  "media-playback-start",
  "media-playback-stop",
  "media-record",
  "media-seek-backward",
  "media-seek-forward",
  "media-skip-backward",
  "media-skip-forward",
  "network-idle",
  "printer-error",
  "process-stop",
  "system-run",
  "text-x-generic",
  "tools-check-spelling",
  "view-fullscreen",
  "view-sort-ascending",
  "view-sort-descending",
  "view-refresh",
  "view-restore",
  "window-close",
  "zoom-fit-best",
  "zoom-in",
  "zoom-original",
  "zoom-out",

  /*** Icons used in code or templates, sorted alphabetically ***/
  "audio-volume-high",
  "audio-volume-high-symbolic",
  "audio-volume-low",
  "audio-volume-low-symbolic",
  "audio-volume-medium",
  "audio-volume-medium-symbolic",
  "audio-volume-muted",
  "audio-volume-muted-symbolic",
  "application-x-executable-symbolic",
  "applications-other",
  "appointment-soon-symbolic",
  "bookmark-new-symbolic",
  "changes-allow-symbolic",
  "changes-prevent-symbolic",
  "dialog-password-symbolic",
  "dialog-warning-symbolic",
  "document-open-symbolic",
  "document-save-as-symbolic",
  "document-save-symbolic",
  "edit-clear-symbolic",
  "edit-clear-all-symbolic",
  "edit-cut-symbolic",
  "edit-delete-symbolic",
  "edit-find-symbolic",
  "edit-paste-symbolic",
  "emblem-important-symbolic",
  "emblem-system-symbolic",
  "emoji-activities-symbolic",
  "emoji-body-symbolic",
  "emoji-flags-symbolic",
  "emoji-food-symbolic",
  "emoji-nature-symbolic",
  "emoji-objects-symbolic",
  "emoji-people-symbolic",
  "emoji-recent-symbolic",
  "emoji-symbols-symbolic",
  "emoji-travel-symbolic",
  "face-cool-symbolic",
  "face-laugh-symbolic",
  "find-location-symbolic",
  "folder-new-symbolic",
  "folder-pictures-symbolic",
  "go-down-symbolic",
  "go-up-symbolic",
  "gtk-orientation-landscape",
  "gtk-orientation-portrait",
  "gtk-orientation-reverse-landscape",
  "gtk-orientation-reverse-portrait",
  "insert-image",
  "insert-object-symbolic",
  "list-add-symbolic",
  "list-remove-symbolic",
  "media-eject-symbolic",
  "media-playback-pause-symbolic",
  "media-playback-start-symbolic",
  "media-playlist-repeat",
  "media-record-symbolic",
  "network-server-symbolic",
  "object-select-symbolic",
  "open-menu-symbolic",
  "pan-down-symbolic",
  "pan-end-symbolic",
  "pan-start-symbolic",
  "pan-up-symbolic",
  "preferences-desktop-font",
  "preferences-desktop-locale-symbolic",
  "send-to-symbolic",
  "star-new-symbolic",
  "user-trash-full-symbolic",
  "user-trash-symbolic",
  "view-fullscreen-symbolic",
  "view-grid-symbolic",
  "view-list-symbolic",
  "view-refresh-symbolic",
  "window-close-symbolic",
  "window-maximize-symbolic",
  "window-minimize-symbolic",
  "window-restore-symbolic",
  "zoom-in-symbolic",
  "zoom-original-symbolic",
  "zoom-out-symbolic"
};

static void
test_icon_existence (gconstpointer icon_name)
{
  GtkIconInfo *info;

  /* Not using generic fallback and builtins here, as we explicitly want to check the
   * icon theme.
   * The icon size is randomly chosen.
   */
  info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (), icon_name, 16, GTK_ICON_LOOKUP_DIR_LTR);
  if (info == NULL)
    {
      g_test_message ("Failed to look up icon for \"%s\"", (char *) icon_name);
      g_test_fail ();
      return;
    }

  g_object_unref (info);
}

int
main (int argc, char *argv[])
{
  guint i;
  char *test_name;
  char *theme;

  gtk_test_init (&argc, &argv);

  g_object_get (gtk_settings_get_default (), "gtk-icon-theme-name", &theme, NULL);
  g_test_message ("Testing icon theme: %s", theme);
  g_free (theme);

  for (i = 0; i < G_N_ELEMENTS (icon_names); i++)
    {
      test_name = g_strdup_printf ("/check-icon-names/%s", icon_names[i]);
      g_test_add_data_func (test_name, icon_names[i], test_icon_existence);
      g_free (test_name);
    }

  return g_test_run();
}
