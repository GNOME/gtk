/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.          See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gdkcontentformats
 * @Title: Content Formats
 * @Short_description: Advertising and negotiating of content
 *     exchange formats
   @See_also: #GdkDragContext, #GtkClipboard
 *
 * This section describes the #GdkContentFormats structure that is used to
 * advertise and negotiate the format of content passed between different
 * widgets, windows or applications using for example the clipboard or
 * drag'n'drop.
 *
 * GDK supports content in 2 forms: #GType and mime type.
 * Using #GTypes is meant only for in-process content transfers. Mime types
 * are meant to be used for data passing both in-process and out-of-process.
 * The details of how data is passed is described in the documentation of
 * the actual implementations.
 *
 * A #GdkContentFormats describes a set of possible formats content can be
 * exchanged in. It is assumed that this set is ordered. #GTypes are more
 * important than mime types. Order between different #Gtypes or mime types
 * is the order they were added in, most important first. Functions that
 * care about order, such as gdk_content_formats_union() will describe in
 * their documentation how they interpret that order, though in general the
 * order of the first argument is considered the primary order of the result,
 * followed by the order of further arguments.
 *
 * For debugging purposes, the function gdk_content_formats_to_string() exists.
 * It will print a comma-seperated formats of formats from most important to least
 * important.
 *
 * #GdkContentFormats is an immutable struct. After creation, you cannot change
 * the types it represents. Instead, new #GdkContentFormats have to be created.
 * The #GdkContentFormatsBuilder structure is meant to help in this endeavor.
 */

/**
 * GdkContentFormats:
 *
 * A #GdkContentFormats struct is a reference counted struct
 * and should be treated as opaque.
 */

#include "config.h"

#include "gdkcontentformats.h"
#include "gdkcontentformatsprivate.h"

#include "gdkproperty.h"

struct _GdkContentFormats
{
  /*< private >*/
  GList *formats;
  guint ref_count;
};

G_DEFINE_BOXED_TYPE (GdkContentFormats, gdk_content_formats,
                     gdk_content_formats_ref,
                     gdk_content_formats_unref)


static void
gdk_content_formats_add_table (GdkContentFormats  *formats,
                               const char        **mime_types,
                               guint               n_mime_types)
{
  gint i;

  for (i = n_mime_types - 1; i >= 0; i--)
    {
      formats->formats = g_list_prepend (formats->formats, (gpointer) gdk_atom_intern (mime_types[i], FALSE));
    }
}

/**
 * gdk_content_formats_new:
 * @mime_types: (array length=n_mime_types) (allow-none): Pointer to an
 *   array of mime types
 * @nmime_types: number of entries in @mime_types.
 * 
 * Creates a new #GdkContentFormats from an array of mime types.
 * 
 * Returns: (transfer full): the new #GdkContentFormats.
 **/
GdkContentFormats *
gdk_content_formats_new (const char **mime_types,
                         guint        n_mime_types)
{
  GdkContentFormats *result = g_slice_new (GdkContentFormats);
  result->formats = NULL;
  result->ref_count = 1;

  if (mime_types)
    gdk_content_formats_add_table (result, mime_types, n_mime_types);
  
  return result;
}

/**
 * gdk_content_formats_ref:
 * @formats:  a #GdkContentFormats
 * 
 * Increases the reference count of a #GdkContentFormats by one.
 *
 * Returns: the passed in #GdkContentFormats.
 **/
GdkContentFormats *
gdk_content_formats_ref (GdkContentFormats *formats)
{
  g_return_val_if_fail (formats != NULL, NULL);

  formats->ref_count++;

  return formats;
}

/**
 * gdk_content_formats_unref:
 * @formats: a #GdkContentFormats
 * 
 * Decreases the reference count of a #GdkContentFormats by one.
 * If the resulting reference count is zero, frees the formats.
 **/
void               
gdk_content_formats_unref (GdkContentFormats *formats)
{
  g_return_if_fail (formats != NULL);
  g_return_if_fail (formats->ref_count > 0);

  formats->ref_count--;
  if (formats->ref_count > 0)
    return;

  g_list_free (formats->formats);
  g_slice_free (GdkContentFormats, formats);
}

/**
 * gdk_content_formats_print:
 * @formats: a #GdkContentFormats
 * @string: a #GString to print into
 *
 * Prints the given @formats into a string for human consumption.
 * This is meant for debugging and logging.
 *
 * The form of the representation may change at any time and is
 * not guranteed to stay identical.
 **/
void
gdk_content_formats_print (GdkContentFormats *formats,
                           GString           *string)
{
  GList *l;

  g_return_if_fail (formats != NULL);
  g_return_if_fail (string != NULL);

  g_string_append (string, "{ ");
  for (l = formats->formats; l; l = l->next)
    {
      if (l != formats->formats)
        g_string_append (string, ", ");
      g_string_append (string, l->data);
    }
  g_string_append (string, " }");
}

/**
 * gdk_content_formats_to_string:
 * @formats: a #GdkContentFormats
 *
 * Prints the given @formats into a human-readable string.
 * This is a small wrapper around gdk_content_formats_print() to help
 * when debugging.
 *
 * Returns: (transfer full): a new string
 **/
char *
gdk_content_formats_to_string (GdkContentFormats *formats)
{
  GString *string;

  g_return_val_if_fail (formats != NULL, NULL);

  string = g_string_new (NULL);
  gdk_content_formats_print (formats, string);

  return g_string_free (string, FALSE);
}

/**
 * gdk_content_formats_union:
 * @first: (transfer full): the #GdkContentFormats to merge into
 * @second: (transfer none): the #GdkContentFormats to merge from
 *
 * Append all missing types from @second to @first, in the order
 * they had in @second.
 *
 * Returns: a new #GdkContentFormats
 */
GdkContentFormats *
gdk_content_formats_union (GdkContentFormats       *first,
                           const GdkContentFormats *second)
{
  GdkContentFormatsBuilder *builder;

  g_return_val_if_fail (first != NULL, NULL);
  g_return_val_if_fail (second != NULL, NULL);

  builder = gdk_content_formats_builder_new ();

  gdk_content_formats_builder_add_formats (builder, first);
  gdk_content_formats_unref (first);
  gdk_content_formats_builder_add_formats (builder, second);

  return gdk_content_formats_builder_free (builder);
}

/**
 * gdk_content_formats_intersects:
 * @first: the primary #GdkContentFormats to intersect
 * @second: the #GdkContentFormats to intersect with
 *
 * Finds the first element from @first that is also contained
 * in @second.
 *
 * Returns: The first matching #GdkAtom or %NULL if the formatss
 *     do not intersect.
 */
GdkAtom
gdk_content_formats_intersects (const GdkContentFormats *first,
                                const GdkContentFormats *second)
{
  GList *l;

  g_return_val_if_fail (first != NULL, NULL);
  g_return_val_if_fail (second != NULL, NULL);

  for (l = first->formats; l; l = l->next)
    {
      if (g_list_find (second->formats, l->data))
        return l->data;
    }

  return NULL;
}

/**
 * gdk_content_formats_contains:
 * @formats: a #GdkContentFormats
 * @mime_type: the mime type to search for
 *
 * Checks if a given mime type is part of the given @formats.
 *
 * Returns: %TRUE if the mime_type was found, otherwise %FALSE
 **/
gboolean
gdk_content_formats_contains (const GdkContentFormats *formats,
                              const char              *mime_type)
{
  g_return_val_if_fail (formats != NULL, FALSE);
  g_return_val_if_fail (mime_type != NULL, FALSE);

  return g_list_find (formats->formats, (gpointer) gdk_atom_intern (mime_type, FALSE)) != NULL;
}

GdkAtom *
gdk_content_formats_get_atoms (GdkContentFormats *formats,
                               guint             *n_atoms)
{
  GdkAtom *atoms;
  GList *l;
  guint i, n;

  n = g_list_length (formats->formats);
  atoms = g_new (GdkAtom, n);

  i = 0;
  for (l = formats->formats; l; l = l->next)
    atoms[i++] = l->data;

  if (n_atoms)
    *n_atoms = n;

  return atoms;
}

/**
 * GdkContentFormatsBuilder:
 *
 * A #GdkContentFormatsBuilder struct is an opaque struct. It is meant to
 * not be kept around and only be used to create new #GdkContentFormats
 * objects.
 */

struct _GdkContentFormatsBuilder
{
  GSList *mime_types;
  gsize n_mime_types;
};

/**
 * gdk_content_formats_builder_new:
 *
 * Create a new #GdkContentFormatsBuilder object. The resulting builder
 * would create an empty #GdkContentFormats. Use addition functions to add
 * types to it.
 *
 * Returns: a new #GdkContentFormatsBuilder
 **/
GdkContentFormatsBuilder *
gdk_content_formats_builder_new (void)
{
  return g_slice_new0 (GdkContentFormatsBuilder);
}

/**
 * gdk_content_formats_builder_free:
 * @builder: a #GdkContentFormatsBuilder
 *
 * Frees @builder and creates a new #GdkContentFormats from it.
 *
 * Returns: a new #GdkContentFormats with all the formats added to @builder
 **/
GdkContentFormats *
gdk_content_formats_builder_free (GdkContentFormatsBuilder *builder)
{
  GdkContentFormats *result;
  const char **mime_types;
  GSList *l;
  gsize i;

  g_return_val_if_fail (builder != NULL, NULL);

  mime_types = g_new (const char *, builder->n_mime_types + 1);
  i = builder->n_mime_types;
  mime_types[i--] = NULL;
  /* add backwards because most important type is last in the list */
  for (l = builder->mime_types; l; l = l->next)
    mime_types[i--] = l->data;

  result = gdk_content_formats_new (mime_types, builder->n_mime_types);
  g_free (mime_types);

  return result;
}

/**
 * gdk_content_formats_builder_add_formats:
 * @builder: a #GdkContentFormatsBuilder
 * @formats: the formats to add
 *
 * Appends all formats from @formats to @builder, skipping those that
 * already exist.
 **/
void
gdk_content_formats_builder_add_formats (GdkContentFormatsBuilder *builder,
                                         const GdkContentFormats  *formats)
{
  GList *l;

  g_return_if_fail (builder != NULL);
  g_return_if_fail (formats != NULL);

  for (l = formats->formats; l; l = l->next)
    gdk_content_formats_builder_add_mime_type (builder, l->data);
}

/**
 * gdk_content_formats_builder_add_formats:
 * @builder: a #GdkContentFormatsBuilder
 * @mime_type: a mime type
 *
 * Appends @mime_type to @builder if it has not already been added.
 **/
void
gdk_content_formats_builder_add_mime_type (GdkContentFormatsBuilder *builder,
                                           const char               *mime_type)
{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (mime_type != NULL);

  mime_type = g_intern_string (mime_type);

  if (g_slist_find (builder->mime_types, mime_type))
    return;

  builder->mime_types = g_slist_prepend (builder->mime_types, (gpointer) mime_type);
  builder->n_mime_types++;
}

