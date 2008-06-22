/* GTK - The GIMP Toolkit
 * gtkfilechoosersettings.c: Internal settings for the GtkFileChooser widget
 * Copyright (C) 2006, Novell, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@novell.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* TODO:
 *
 * - Persist these:
 *   - hpaned position
 *   - browse_for_other_folders?
 *
 * - Do we want lockdown?
 */

#include "config.h"
#include <errno.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include "gtkfilechoosersettings.h"
#include "gtkalias.h"

#define SETTINGS_GROUP		"Filechooser Settings"
#define LOCATION_MODE_KEY	"LocationMode"
#define SHOW_HIDDEN_KEY		"ShowHidden"
#define EXPAND_FOLDERS_KEY	"ExpandFolders"

#define MODE_PATH_BAR          "path-bar"
#define MODE_FILENAME_ENTRY    "filename-entry"

#define EQ(a, b) (g_ascii_strcasecmp ((a), (b)) == 0)

static char *
get_config_dirname (void)
{
  return g_build_filename (g_get_user_config_dir (), "gtk-2.0", NULL);
}

static char *
get_config_filename (void)
{
  return g_build_filename (g_get_user_config_dir (), "gtk-2.0", "gtkfilechooser.ini", NULL);
}

static void
ensure_settings_read (GtkFileChooserSettings *settings)
{
  GError *error;
  GKeyFile *key_file;
  gchar *location_mode_str, *filename;
  gboolean value;

  if (settings->settings_read)
    return;

  key_file = g_key_file_new ();

  filename = get_config_filename ();

  error = NULL;
  if (!g_key_file_load_from_file (key_file, filename, 0, &error))
    {
      /* Don't warn on non-existent file */
      if (error->domain != G_FILE_ERROR ||
	  error->code != G_FILE_ERROR_NOENT)
        g_warning ("Failed to read filechooser settings from \"%s\": %s",
                   filename, error->message);

      g_error_free (error);
      goto out;
    }

  if (!g_key_file_has_group (key_file, SETTINGS_GROUP))
    goto out;

  location_mode_str = g_key_file_get_string (key_file, SETTINGS_GROUP,
					     LOCATION_MODE_KEY, NULL);
  if (location_mode_str)
    {
      if (EQ (location_mode_str, MODE_PATH_BAR))
        settings->location_mode = LOCATION_MODE_PATH_BAR;
      else if (EQ (location_mode_str, MODE_FILENAME_ENTRY))
        settings->location_mode = LOCATION_MODE_FILENAME_ENTRY;
      else
        g_warning ("Unknown location mode '%s' encountered in filechooser settings",
		   location_mode_str);

      g_free (location_mode_str);
    }

  value = g_key_file_get_boolean (key_file, SETTINGS_GROUP,
				  SHOW_HIDDEN_KEY, &error);
  if (error)
    {
      g_warning ("Failed to read show-hidden setting in filechooser settings: %s",
		 error->message);
      g_clear_error (&error);
    }
  else
    settings->show_hidden = value != FALSE;

  value = g_key_file_get_boolean (key_file, SETTINGS_GROUP,
				  EXPAND_FOLDERS_KEY, &error);
  if (error)
    {
      g_warning ("Failed to read expand-folders setting in filechooser settings: %s",
		 error->message);
      g_clear_error (&error);
    }
  else
    settings->expand_folders = value != FALSE;

 out:

  g_key_file_free (key_file);
  g_free (filename);

  settings->settings_read = TRUE;
}

G_DEFINE_TYPE (GtkFileChooserSettings, _gtk_file_chooser_settings, G_TYPE_OBJECT)

static void
_gtk_file_chooser_settings_class_init (GtkFileChooserSettingsClass *class)
{
}

static void
_gtk_file_chooser_settings_init (GtkFileChooserSettings *settings)
{
  settings->location_mode = LOCATION_MODE_PATH_BAR;
  settings->show_hidden = FALSE;
  settings->expand_folders = FALSE;
}

GtkFileChooserSettings *
_gtk_file_chooser_settings_new (void)
{
  return g_object_new (GTK_FILE_CHOOSER_SETTINGS_TYPE, NULL);
}

LocationMode
_gtk_file_chooser_settings_get_location_mode (GtkFileChooserSettings *settings)
{
  ensure_settings_read (settings);
  return settings->location_mode;
}

void
_gtk_file_chooser_settings_set_location_mode (GtkFileChooserSettings *settings,
					      LocationMode location_mode)
{
  settings->location_mode = location_mode;
}

gboolean
_gtk_file_chooser_settings_get_show_hidden (GtkFileChooserSettings *settings)
{
  ensure_settings_read (settings);
  return settings->show_hidden;
}

void
_gtk_file_chooser_settings_set_show_hidden (GtkFileChooserSettings *settings,
					    gboolean show_hidden)
{
  settings->show_hidden = show_hidden != FALSE;
}

gboolean
_gtk_file_chooser_settings_get_expand_folders (GtkFileChooserSettings *settings)
{
  ensure_settings_read (settings);
  return settings->expand_folders;
}

void
_gtk_file_chooser_settings_set_expand_folders (GtkFileChooserSettings *settings,
					       gboolean expand_folders)
{
  settings->expand_folders = expand_folders != FALSE;
}

gboolean
_gtk_file_chooser_settings_save (GtkFileChooserSettings *settings,
				 GError                **error)
{
  const gchar *location_mode_str;
  gchar *filename;
  gchar *dirname;
  gchar *contents;
  gsize len;
  gboolean retval;
  GKeyFile *key_file;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  filename = get_config_filename ();
  dirname = NULL;

  retval = FALSE;

  if (settings->location_mode == LOCATION_MODE_PATH_BAR)
    location_mode_str = MODE_PATH_BAR;
  else if (settings->location_mode == LOCATION_MODE_FILENAME_ENTRY)
    location_mode_str = MODE_FILENAME_ENTRY;
  else
    {
      g_assert_not_reached ();
      return FALSE;
    }

  key_file = g_key_file_new ();

  /* Initialise with the on-disk keyfile, so we keep unknown options */
  g_key_file_load_from_file (key_file, filename, 0, NULL);

  g_key_file_set_string (key_file, SETTINGS_GROUP,
			 LOCATION_MODE_KEY, location_mode_str);
  g_key_file_set_boolean (key_file, SETTINGS_GROUP,
			  SHOW_HIDDEN_KEY, settings->show_hidden);
  g_key_file_set_boolean (key_file, SETTINGS_GROUP,
			  EXPAND_FOLDERS_KEY, settings->expand_folders);

  contents = g_key_file_to_data (key_file, &len, error);
  g_key_file_free (key_file);

  if (!contents)
    goto out;

  if (!g_file_set_contents (filename, contents, len, NULL))
    {
      char *dirname;
      int saved_errno;

      /* Directory is not there? */

      dirname = get_config_dirname ();
      if (g_mkdir_with_parents (dirname, 0700) != 0) /* 0700 per the XDG basedir spec */
	{
	  saved_errno = errno;
	  g_set_error (error,
		       G_FILE_ERROR,
		       g_file_error_from_errno (saved_errno),
		       _("Error creating folder '%s': %s"),
		       dirname, g_strerror (saved_errno));
	  goto out;
	}

      if (!g_file_set_contents (filename, contents, len, error))
	goto out;
    }

  retval = TRUE;

 out:

  g_free (contents);
  g_free (dirname);
  g_free (filename);

  return retval;
}
