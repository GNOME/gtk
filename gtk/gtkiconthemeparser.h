/* GtkIconThemeParser - a parser of icon-theme files
 * gtkiconthemeparser.h Copyright (C) 2002, 2003 Red Hat, Inc.
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

#ifndef __GTK_ICON_THEME_PARSER_H__
#define __GTK_ICON_THEME_PARSER_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GtkIconThemeFile GtkIconThemeFile;

typedef void (* GtkIconThemeFileSectionFunc) (GtkIconThemeFile *df,
					      const char       *name,
					      gpointer          data);

/* If @key is %NULL, @value is a comment line. */
/* @value is raw, unescaped data. */
typedef void (* GtkIconThemeFileLineFunc) (GtkIconThemeFile *df,
					   const char       *key,
					   const char       *locale,
					   const char       *value,
					   gpointer          data);

typedef enum 
{
  GTK_ICON_THEME_FILE_PARSE_ERROR_INVALID_SYNTAX,
  GTK_ICON_THEME_FILE_PARSE_ERROR_INVALID_ESCAPES,
  GTK_ICON_THEME_FILE_PARSE_ERROR_INVALID_CHARS
} GtkIconThemeFileParseError;

#define GTK_ICON_THEME_FILE_PARSE_ERROR _gtk_icon_theme_file_parse_error_quark()
GQuark _gtk_icon_theme_file_parse_error_quark (void);

GtkIconThemeFile *_gtk_icon_theme_file_new_from_string (char              *data,
							GError           **error);
char *            _gtk_icon_theme_file_to_string       (GtkIconThemeFile  *df);
void              _gtk_icon_theme_file_free            (GtkIconThemeFile  *df);

void _gtk_icon_theme_file_foreach_section (GtkIconThemeFile            *df,
					   GtkIconThemeFileSectionFunc  func,
					   gpointer                     user_data);
void _gtk_icon_theme_file_foreach_key     (GtkIconThemeFile            *df,
					   const char                  *section,
					   gboolean                     include_localized,
					   GtkIconThemeFileLineFunc     func,
					   gpointer                     user_data);

/* Gets the raw text of the key, unescaped */
gboolean _gtk_icon_theme_file_get_raw           (GtkIconThemeFile  *df,
						 const char        *section,
						 const char        *keyname,
						 const char        *locale,
						 char             **val);
gboolean _gtk_icon_theme_file_get_integer       (GtkIconThemeFile  *df,
						 const char        *section,
						 const char        *keyname,
						 int               *val);
gboolean _gtk_icon_theme_file_get_string        (GtkIconThemeFile  *df,
						 const char        *section,
						 const char        *keyname,
						 char             **val);
gboolean _gtk_icon_theme_file_get_locale_string (GtkIconThemeFile  *df,
						 const char        *section,
						 const char        *keyname,
						 char             **val);

G_END_DECLS

#endif /* GTK_ICON_THEME_PARSER_H */
