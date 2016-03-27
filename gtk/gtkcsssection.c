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
  GtkCssSectionType   section_type;
  GtkCssSection      *parent;
  GFile              *file;
  guint               start_line;
  guint               start_position;
  GtkCssParser       *parser;         /* parser if section isn't finished parsing yet or %NULL */
  guint               end_line;       /* end line if parser is %NULL */
  guint               end_position;   /* end position if parser is %NULL */
};

G_DEFINE_BOXED_TYPE (GtkCssSection, gtk_css_section, gtk_css_section_ref, gtk_css_section_unref)

GtkCssSection *
_gtk_css_section_new (GtkCssSection     *parent,
                      GtkCssSectionType  type,
                      GtkCssParser      *parser)
{
  GtkCssSection *section;

  gtk_internal_return_val_if_fail (parser != NULL, NULL);

  section = g_slice_new0 (GtkCssSection);

  section->ref_count = 1;
  section->section_type = type;
  if (parent)
    section->parent = gtk_css_section_ref (parent);
  section->file = _gtk_css_parser_get_file (parser);
  if (section->file)
    g_object_ref (section->file);
  section->start_line = _gtk_css_parser_get_line (parser);
  section->start_position = _gtk_css_parser_get_position (parser);
  section->parser = parser;

  return section;
}

GtkCssSection *
_gtk_css_section_new_for_file (GtkCssSectionType  type,
                               GFile             *file)
{
  GtkCssSection *section;

  gtk_internal_return_val_if_fail (G_IS_FILE (file), NULL);

  section = g_slice_new0 (GtkCssSection);

  section->ref_count = 1;
  section->section_type = type;
  section->file = g_object_ref (file);

  return section;
}

void
_gtk_css_section_end (GtkCssSection *section)
{
  gtk_internal_return_if_fail (section != NULL);
  gtk_internal_return_if_fail (section->parser != NULL);

  section->end_line = _gtk_css_parser_get_line (section->parser);
  section->end_position = _gtk_css_parser_get_position (section->parser);
  section->parser = NULL;
}

/**
 * gtk_css_section_ref:
 * @section: a #GtkCssSection
 *
 * Increments the reference count on @section.
 *
 * Returns: @section itself.
 *
 * Since: 3.2
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
 *
 * Since: 3.2
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
 * gtk_css_section_get_section_type:
 * @section: the section
 *
 * Gets the type of information that @section describes.
 *
 * Returns: the type of @section
 *
 * Since: 3.2
 **/
GtkCssSectionType
gtk_css_section_get_section_type (const GtkCssSection *section)
{
  gtk_internal_return_val_if_fail (section != NULL, GTK_CSS_SECTION_DOCUMENT);

  return section->section_type;
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
 *
 * Since: 3.2
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
 *
 * Since: 3.2
 **/
GFile *
gtk_css_section_get_file (const GtkCssSection *section)
{
  gtk_internal_return_val_if_fail (section != NULL, NULL);

  return section->file;
}

/**
 * gtk_css_section_get_start_line:
 * @section: the section
 *
 * Returns the line in the CSS document where this section starts.
 * The line number is 0-indexed, so the first line of the document
 * will return 0.
 *
 * Returns: the line number
 *
 * Since: 3.2
 **/
guint
gtk_css_section_get_start_line (const GtkCssSection *section)
{
  gtk_internal_return_val_if_fail (section != NULL, 0);

  return section->start_line;
}

/**
 * gtk_css_section_get_start_position:
 * @section: the section
 *
 * Returns the offset in bytes from the start of the current line
 * returned via gtk_css_section_get_start_line().
 *
 * Returns: the offset in bytes from the start of the line.
 *
 * Since: 3.2
 **/
guint
gtk_css_section_get_start_position (const GtkCssSection *section)
{
  gtk_internal_return_val_if_fail (section != NULL, 0);

  return section->start_position;
}

/**
 * gtk_css_section_get_end_line:
 * @section: the section
 *
 * Returns the line in the CSS document where this section end.
 * The line number is 0-indexed, so the first line of the document
 * will return 0.
 * This value may change in future invocations of this function if
 * @section is not yet parsed completely. This will for example 
 * happen in the GtkCssProvider::parsing-error signal.
 * The end position and line may be identical to the start
 * position and line for sections which failed to parse anything
 * successfully.
 *
 * Returns: the line number
 *
 * Since: 3.2
 **/
guint
gtk_css_section_get_end_line (const GtkCssSection *section)
{
  gtk_internal_return_val_if_fail (section != NULL, 0);

  if (section->parser)
    return _gtk_css_parser_get_line (section->parser);
  else
    return section->end_line;
}

/**
 * gtk_css_section_get_end_position:
 * @section: the section
 *
 * Returns the offset in bytes from the start of the current line
 * returned via gtk_css_section_get_end_line().
 * This value may change in future invocations of this function if
 * @section is not yet parsed completely. This will for example
 * happen in the GtkCssProvider::parsing-error signal.
 * The end position and line may be identical to the start
 * position and line for sections which failed to parse anything
 * successfully.
 *
 * Returns: the offset in bytes from the start of the line.
 *
 * Since: 3.2
 **/
guint
gtk_css_section_get_end_position (const GtkCssSection *section)
{
  gtk_internal_return_val_if_fail (section != NULL, 0);

  if (section->parser)
    return _gtk_css_parser_get_position (section->parser);
  else
    return section->end_position;
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

  g_string_append_printf (string, ":%u:%u", 
                          gtk_css_section_get_end_line (section) + 1,
                          gtk_css_section_get_end_position (section));
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
