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

#include <config.h>
#include <errno.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include "gtkfilechoosersettings.h"
#include "gtkalias.h"

/* Increment this every time you change the configuration format */
#define CONFIG_VERSION 0

#define ELEMENT_TOPLEVEL	"gtkfilechooser"
#define ELEMENT_LOCATION	"location"
#define ELEMENT_SHOW_HIDDEN     "show_hidden"
#define ELEMENT_EXPAND_FOLDERS  "expand_folders"
#define ATTRIBUTE_VERSION       "version"
#define ATTRIBUTE_MODE		"mode"
#define ATTRIBUTE_VALUE         "value"
#define MODE_PATH_BAR		"path-bar"
#define MODE_FILENAME_ENTRY	"filename-entry"
#define VALUE_TRUE              "true"
#define VALUE_FALSE             "false"

#define EQ(a, b) (g_ascii_strcasecmp ((a), (b)) == 0)

static char *
get_config_dirname (void)
{
  return g_build_filename (g_get_user_config_dir (), "gtk-2.0", NULL);
}

static char *
get_config_filename (void)
{
  return g_build_filename (g_get_user_config_dir (), "gtk-2.0", "gtkfilechooser", NULL);
}

static void
set_defaults (GtkFileChooserSettings *settings)
{
  settings->location_mode = LOCATION_MODE_PATH_BAR;
  settings->show_hidden = FALSE;
  settings->expand_folders = FALSE;
}

typedef enum {
  STATE_START,
  STATE_END,
  STATE_ERROR,
  STATE_IN_TOPLEVEL,
  STATE_IN_LOCATION,
  STATE_IN_SHOW_HIDDEN,
  STATE_IN_EXPAND_FOLDERS
} State;

struct parse_state {
  GtkFileChooserSettings *settings;
  int version;
  State state;
};

static const char *
get_attribute_value (const char **attribute_names,
		     const char **attribute_values,
		     const char *attribute)
{
  const char **name;
  const char **value;

  name = attribute_names;
  value = attribute_values;

  while (*name)
    {
      if (EQ (*name, attribute))
	return *value;

      name++;
      value++;
    }

  return NULL;
}

static void
set_missing_attribute_error (struct parse_state *state,
			     int line,
			     int col,
			     const char *attribute,
			     GError **error)
{
  state->state = STATE_ERROR;
  g_set_error (error,
	       G_MARKUP_ERROR,
	       G_MARKUP_ERROR_INVALID_CONTENT,
	       _("Line %d, column %d: missing attribute \"%s\""),
	       line,
	       col,
	       attribute);
}

static void
set_unexpected_element_error (struct parse_state *state,
			      int line,
			      int col,
			      const char *element,
			      GError **error)
{
  state->state = STATE_ERROR;
  g_set_error (error,
	       G_MARKUP_ERROR,
	       G_MARKUP_ERROR_UNKNOWN_ELEMENT,
	       _("Line %d, column %d: unexpected element \"%s\""),
	       line,
	       col,
	       element);
}

static void
set_unexpected_element_end_error (struct parse_state *state,
				  int line,
				  int col,
				  const char *expected_element,
				  const char *unexpected_element,
				  GError **error)
{
  state->state = STATE_ERROR;
  g_set_error (error,
	       G_MARKUP_ERROR,
	       G_MARKUP_ERROR_UNKNOWN_ELEMENT,
	       _("Line %d, column %d: expected end of element \"%s\", but got element for \"%s\" instead"),
	       line,
	       col,
	       expected_element,
	       unexpected_element);
}


static void
parse_start_element_cb (GMarkupParseContext *context,
			const char *element_name,
			const char **attribute_names,
			const char **attribute_values,
			gpointer data,
			GError **error)
{
  struct parse_state *state;
  int line, col;

  state = data;
  g_markup_parse_context_get_position (context, &line, &col);

  switch (state->state)
    {
    case STATE_START:
      if (EQ (element_name, ELEMENT_TOPLEVEL))
	{
	  const char *version_str;

	  state->state = STATE_IN_TOPLEVEL;

	  version_str = get_attribute_value (attribute_names, attribute_values, ATTRIBUTE_VERSION);
	  if (!version_str)
	    state->version = -1;
	  else
	    if (sscanf (version_str, "%d", &state->version) != 1 || state->version < 0)
	      state->version = -1;
	}
      else
	{
	  state->state = STATE_ERROR;
	  g_set_error (error,
		       G_MARKUP_ERROR,
		       G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		       _("Line %d, column %d: expected \"%s\" at the toplevel, but found \"%s\" instead"),
		       line,
		       col,
		       ELEMENT_TOPLEVEL,
		       element_name);
	}
      break;

    case STATE_END:
      g_assert_not_reached ();
      break;

    case STATE_ERROR:
      g_assert_not_reached ();
      break;

    case STATE_IN_TOPLEVEL:
      if (EQ (element_name, ELEMENT_LOCATION))
	{
	  const char *location_mode_str;

	  state->state = STATE_IN_LOCATION;

	  location_mode_str = get_attribute_value (attribute_names, attribute_values, ATTRIBUTE_MODE);
	  if (!location_mode_str)
	    set_missing_attribute_error (state, line, col, ATTRIBUTE_MODE, error);
	  else if (EQ (location_mode_str, MODE_PATH_BAR))
	    state->settings->location_mode = LOCATION_MODE_PATH_BAR;
	  else if (EQ (location_mode_str, MODE_FILENAME_ENTRY))
	    state->settings->location_mode = LOCATION_MODE_FILENAME_ENTRY;
	  else
	    {
	      state->state = STATE_ERROR;
	      g_set_error (error,
			   G_MARKUP_ERROR,
			   G_MARKUP_ERROR_INVALID_CONTENT,
			   _("Line %d, column %d: expected \"%s\" or \"%s\", but found \"%s\" instead"),
			   line,
			   col,
			   MODE_PATH_BAR,
			   MODE_FILENAME_ENTRY,
			   location_mode_str);
	    }
	}
      else if (EQ (element_name, ELEMENT_SHOW_HIDDEN))
	{
	  const char *value_str;

	  state->state = STATE_IN_SHOW_HIDDEN;

	  value_str = get_attribute_value (attribute_names, attribute_values, ATTRIBUTE_VALUE);

	  if (!value_str)
	    set_missing_attribute_error (state, line, col, ATTRIBUTE_VALUE, error);
	  else if (EQ (value_str, VALUE_TRUE))
	    state->settings->show_hidden = TRUE;
	  else if (EQ (value_str, VALUE_FALSE))
	    state->settings->show_hidden = FALSE;
	  else
	    {
	      state->state = STATE_ERROR;
	      g_set_error (error,
			   G_MARKUP_ERROR,
			   G_MARKUP_ERROR_INVALID_CONTENT,
			   _("Line %d, column %d: expected \"%s\" or \"%s\", but found \"%s\" instead"),
			   line,
			   col,
			   VALUE_FALSE,
			   VALUE_TRUE,
			   value_str);
	    }
	}
      else if (EQ (element_name, ELEMENT_EXPAND_FOLDERS))
	{
	  const char *value_str;

	  state->state = STATE_IN_EXPAND_FOLDERS;

	  value_str = get_attribute_value (attribute_names, attribute_values, ATTRIBUTE_VALUE);

	  if (!value_str)
	    set_missing_attribute_error (state, line, col, ATTRIBUTE_VALUE, error);
	  else if (EQ (value_str, VALUE_TRUE))
	    state->settings->expand_folders = TRUE;
	  else if (EQ (value_str, VALUE_FALSE))
	    state->settings->expand_folders = FALSE;
	  else
	    {
	      state->state = STATE_ERROR;
	      g_set_error (error,
			   G_MARKUP_ERROR,
			   G_MARKUP_ERROR_INVALID_CONTENT,
			   _("Line %d, column %d: expected \"%s\" or \"%s\", but found \"%s\" instead"),
			   line,
			   col,
			   VALUE_FALSE,
			   VALUE_TRUE,
			   value_str);
	    }
	}
      else
	set_unexpected_element_error (state, line, col, element_name, error);

      break;

    case STATE_IN_LOCATION:
    case STATE_IN_SHOW_HIDDEN:
      set_unexpected_element_error (state, line, col, element_name, error);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
parse_end_element_cb (GMarkupParseContext *context,
		      const char *element_name,
		      gpointer data,
		      GError **error)
{
  struct parse_state *state;
  int line, col;

  state = data;
  g_markup_parse_context_get_position (context, &line, &col);

  switch (state->state)
    {
    case STATE_START:
      g_assert_not_reached ();
      break;

    case STATE_END:
      g_assert_not_reached ();
      break;

    case STATE_ERROR:
      g_assert_not_reached ();
      break;

    case STATE_IN_TOPLEVEL:
      if (EQ (element_name, ELEMENT_TOPLEVEL))
	state->state = STATE_END;
      else
	set_unexpected_element_end_error (state, line, col, ELEMENT_TOPLEVEL, element_name, error);

      break;

    case STATE_IN_LOCATION:
      if (EQ (element_name, ELEMENT_LOCATION))
	state->state = STATE_IN_TOPLEVEL;
      else
	set_unexpected_element_end_error (state, line, col, ELEMENT_LOCATION, element_name, error);

      break;

    case STATE_IN_SHOW_HIDDEN:
      if (EQ (element_name, ELEMENT_SHOW_HIDDEN))
	state->state = STATE_IN_TOPLEVEL;
      else
	set_unexpected_element_end_error (state, line, col, ELEMENT_SHOW_HIDDEN, element_name, error);

      break;

    case STATE_IN_EXPAND_FOLDERS:
      if (EQ (element_name, ELEMENT_EXPAND_FOLDERS))
	state->state = STATE_IN_TOPLEVEL;
      else
	set_unexpected_element_end_error (state, line, col, ELEMENT_EXPAND_FOLDERS, element_name, error);

      break;

    default:
      g_assert_not_reached ();
    }
}

static gboolean
parse_config (GtkFileChooserSettings *settings,
	      const char *contents,
	      GError **error)
{
  GMarkupParser parser = { 0, };
  GMarkupParseContext *context;
  struct parse_state state;
  gboolean retval;

  parser.start_element = parse_start_element_cb;
  parser.end_element = parse_end_element_cb;

  state.settings = settings;
  state.version = -1;
  state.state = STATE_START;

  context = g_markup_parse_context_new (&parser,
					0,
					&state,
					NULL);

  retval = g_markup_parse_context_parse (context, contents, -1, error);
  g_markup_parse_context_free (context);

  return retval;
}

static gboolean
read_config (GtkFileChooserSettings *settings,
	     GError **error)
{
  char *filename;
  char *contents;
  gsize contents_len;
  gboolean success;

  filename = get_config_filename ();

  success = g_file_get_contents (filename, &contents, &contents_len, error);
  g_free (filename);

  if (!success)
    {
      set_defaults (settings);
      return FALSE;
    }

  success = parse_config (settings, contents, error);

  g_free (contents);

  return success;
}

static void
ensure_settings_read (GtkFileChooserSettings *settings)
{
  if (settings->settings_read)
    return;

  /* NULL GError */
  read_config (settings, NULL);

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
  settings->show_hidden = show_hidden ? TRUE : FALSE;
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
  settings->expand_folders = expand_folders ? TRUE : FALSE;
}

static char *
settings_to_markup (GtkFileChooserSettings *settings)
{
  const char *location_mode_str;
  const char *show_hidden_str;
  const char *expand_folders_str;

  if (settings->location_mode == LOCATION_MODE_PATH_BAR)
    location_mode_str = MODE_PATH_BAR;
  else if (settings->location_mode == LOCATION_MODE_FILENAME_ENTRY)
    location_mode_str = MODE_FILENAME_ENTRY;
  else
    {
      g_assert_not_reached ();
      return NULL;
    }

  show_hidden_str = settings->show_hidden ? VALUE_TRUE : VALUE_FALSE;
  expand_folders_str = settings->expand_folders ? VALUE_TRUE : VALUE_FALSE;

  return g_strdup_printf
    ("<" ELEMENT_TOPLEVEL ">\n"						/* <gtkfilechooser>               */
     "  <" ELEMENT_LOCATION " " ATTRIBUTE_MODE "=\"%s\"/>\n"		/*   <location mode="path-bar"/>  */
     "  <" ELEMENT_SHOW_HIDDEN " " ATTRIBUTE_VALUE "=\"%s\"/>\n"	/*   <show_hidden value="false"/> */
     "  <" ELEMENT_EXPAND_FOLDERS " " ATTRIBUTE_VALUE "=\"%s\"/>\n"	/*   <expand_folders value="false"/> */
     "</" ELEMENT_TOPLEVEL ">\n",					/* </gtkfilechooser>              */
     location_mode_str,
     show_hidden_str,
     expand_folders_str);
}

gboolean
_gtk_file_chooser_settings_save (GtkFileChooserSettings *settings,
				 GError                **error)
{
  char *contents;
  char *filename;
  char *dirname;
  gboolean retval;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  contents = settings_to_markup (settings);

  filename = get_config_filename ();
  dirname = NULL;

  retval = FALSE;

  if (!g_file_set_contents (filename, contents, -1, NULL))
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
		       _("Could not create directory: %s"),
		       dirname);
	  goto out;
	}

      if (!g_file_set_contents (filename, contents, -1, error))
	goto out;
    }

  retval = TRUE;

 out:

  g_free (contents);
  g_free (dirname);
  g_free (filename);

  return retval;
}
