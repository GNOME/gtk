/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
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

#include "gtkcsssectionprivate.h"

#include "gtkcssparserprivate.h"
#include "gtkprivate.h"

struct _GtkCssSection
{
  gint                ref_count;
  GtkCssSection      *parent;
  GFile              *file;
  GtkCssLocation      start_location;
  GtkCssParser       *parser;         /* parser if section isn't finished parsing yet or %NULL */
  GtkCssLocation      end_location;   /* end location if parser is %NULL */
};

G_DEFINE_BOXED_TYPE (GtkCssSection, gtk_css_section, gtk_css_section_ref, gtk_css_section_unref)

GtkCssSection *
gtk_css_section_new_for_parser (GtkCssSection     *parent,
                                GtkCssParser      *parser)
{
  GtkCssSection *section;

  gtk_internal_return_val_if_fail (parser != NULL, NULL);

  section = g_slice_new0 (GtkCssSection);

  section->ref_count = 1;
  if (parent)
    section->parent = gtk_css_section_ref (parent);
  section->file = gtk_css_parser_get_file (parser);
  if (section->file)
    g_object_ref (section->file);
  section->parser = parser;
  gtk_css_parser_get_location (section->parser, &section->start_location);

  return section;
}

void
_gtk_css_section_end (GtkCssSection *section)
{
  gtk_internal_return_if_fail (section != NULL);
  gtk_internal_return_if_fail (section->parser != NULL);

  gtk_css_parser_get_location (section->parser, &section->end_location);
  section->parser = NULL;
}

/**
 * gtk_css_section_ref:
 * @section: a #GtkCssSection
 *
 * Increments the reference count on @section.
 *
 * Returns: @section itself.
 **/
GtkCssSection *
gtk_css_section_ref (GtkCssSection *section)
{
  gtk_internal_return_val_if_fail (section != NULL, NULL);

  section->ref_count += 1;

  return section;
}

/**
 * gtk_css_section_unref:
 * @section: a #GtkCssSection
 *
 * Decrements the reference count on @section, freeing the
 * structure if the reference count reaches 0.
 **/
void
gtk_css_section_unref (GtkCssSection *section)
{
  gtk_internal_return_if_fail (section != NULL);

  section->ref_count -= 1;
  if (section->ref_count > 0)
    return;

  if (section->parent)
    gtk_css_section_unref (section->parent);
  if (section->file)
    g_object_unref (section->file);

  g_slice_free (GtkCssSection, section);
}

/**
 * gtk_css_section_get_parent:
 * @section: the section
 *
 * Gets the parent section for the given @section. The parent section is
 * the section that contains this @section. A special case are sections of
 * type #GTK_CSS_SECTION_DOCUMENT. Their parent will either be %NULL
 * if they are the original CSS document that was loaded by
 * gtk_css_provider_load_from_file() or a section of type
 * #GTK_CSS_SECTION_IMPORT if it was loaded with an import rule from
 * a different file.
 *
 * Returns: (nullable) (transfer none): the parent section or %NULL if none
 **/
GtkCssSection *
gtk_css_section_get_parent (const GtkCssSection *section)
{
  gtk_internal_return_val_if_fail (section != NULL, NULL);

  return section->parent;
}

/**
 * gtk_css_section_get_file:
 * @section: the section
 *
 * Gets the file that @section was parsed from. If no such file exists,
 * for example because the CSS was loaded via
 * @gtk_css_provider_load_from_data(), then %NULL is returned.
 *
 * Returns: (transfer none): the #GFile that @section was parsed from
 *     or %NULL if @section was parsed from other data
 **/
GFile *
gtk_css_section_get_file (const GtkCssSection *section)
{
  gtk_internal_return_val_if_fail (section != NULL, NULL);

  return section->file;
}

/**
 * gtk_css_section_get_start_location:
 * @section: the section
 *
 * Returns the location in the CSS document where this section starts.
 *
 * Returns: (tranfer none) (not nullable): The start location of
 *     this section
 **/
const GtkCssLocation *
gtk_css_section_get_start_location (const GtkCssSection *section)
{
  gtk_internal_return_val_if_fail (section != NULL, NULL);

  return &section->start_location;
}

/**
 * gtk_css_section_get_end_location:
 * @section: the section
 *
 * Returns the location in the CSS document where this section ends.
 *
 * Returns: (tranfer none) (not nullable): The end location of
 *     this section
 **/
const GtkCssLocation *
gtk_css_section_get_end_location (const GtkCssSection *section)
{
  gtk_internal_return_val_if_fail (section != NULL, NULL);

  if (section->parser)
    gtk_css_parser_get_location (section->parser, (GtkCssLocation *) &section->end_location);

  return &section->end_location;
}

void
_gtk_css_section_print (const GtkCssSection  *section,
                        GString              *string)
{
  if (section->file)
    {
      GFileInfo *info;

      info = g_file_query_info (section->file, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 0, NULL, NULL);

      if (info)
        {
          g_string_append (string, g_file_info_get_display_name (info));
          g_object_unref (info);
        }
      else
        {
          g_string_append (string, "<broken file>");
        }
    }
  else
    {
      g_string_append (string, "<data>");
    }

  g_string_append_printf (string, ":%zu:%zu", 
                          section->end_location.lines + 1,
                          section->end_location.line_chars + 1);
}

char *
_gtk_css_section_to_string (const GtkCssSection *section)
{
  GString *string;

  gtk_internal_return_val_if_fail (section != NULL, NULL);

  string = g_string_new (NULL);
  _gtk_css_section_print (section, string);

  return g_string_free (string, FALSE);
}
