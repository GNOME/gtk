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
  guint ref_count;

  const char **mime_types; /* interned */
  gsize n_mime_types;
  GType *gtypes;
  gsize n_gtypes;
};

G_DEFINE_BOXED_TYPE (GdkContentFormats, gdk_content_formats,
                     gdk_content_formats_ref,
                     gdk_content_formats_unref)


static GdkContentFormats *
gdk_content_formats_new_take (GType *      gtypes,
                              gsize        n_gtypes,
                              const char **mime_types,
                              gsize        n_mime_types)
{
  GdkContentFormats *result = g_slice_new0 (GdkContentFormats);
  result->ref_count = 1;

  result->gtypes = gtypes;
  result->n_gtypes = n_gtypes;
  result->mime_types = mime_types;
  result->n_mime_types = n_mime_types;

  return result;
}

/**
 * gdk_content_formats_new:
 * @mime_types: (array length=n_mime_types) (allow-none): Pointer to an
 *   array of mime types
 * @nmime_types: number of entries in @mime_types.
 * 
 * Creates a new #GdkContentFormats from an array of mime types.
 *
 * The mime types must be different or the behavior of the return value
 * is undefined. If you cannot guarantee this, use #GdkContentFormatsBuilder
 * instead.
 * 
 * Returns: (transfer full): the new #GdkContentFormats.
 **/
GdkContentFormats *
gdk_content_formats_new (const char **mime_types,
                         guint        n_mime_types)
{
  GPtrArray *array;
  guint i;

  if (n_mime_types == 0)
      return gdk_content_formats_new_take (NULL, 0, NULL, 0);

  array = g_ptr_array_new ();
  for (i = 0; i < n_mime_types; i++)
    g_ptr_array_add (array, (gpointer) g_intern_string (mime_types[i]));
  g_ptr_array_add (array, NULL);

  return gdk_content_formats_new_take (NULL, 0, (const char **) g_ptr_array_free (array, FALSE), n_mime_types);
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

  g_free (formats->gtypes);
  g_free (formats->mime_types);
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
  gsize i;

  g_return_if_fail (formats != NULL);
  g_return_if_fail (string != NULL);

  g_string_append (string, "{ ");
  for (i = 0; i < formats->n_gtypes; i++)
    {
      if (i > 0)
        g_string_append (string, ", ");
      g_string_append (string, g_type_name (formats->gtypes[i]));
    }
  for (i = 0; i < formats->n_mime_types; i++)
    {
      if (i > 0 || formats->n_gtypes > 0)
        g_string_append (string, ", ");
      g_string_append (string, formats->mime_types[i]);
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

static gboolean
gdk_content_formats_contain_interned_mime_type (const GdkContentFormats *formats,
                                                const char              *mime_type)
{
  gsize i;

  for (i = 0; i < formats->n_mime_types; i++)
    {
      if (mime_type == formats->mime_types[i])
        return TRUE;
    }

  return FALSE;
}

/**
 * gdk_content_formats_match:
 * @first: the primary #GdkContentFormats to intersect
 * @second: the #GdkContentFormats to intersect with
 * @out_gtype: (out) (allow-none): pointer to take the 
 *     matching #GType or %G_TYPE_INVALID if @out_mime_type was set.
 * @out_mime_type: (out) (allow-none) (transfer none): The matching
 *    mime type or %NULL if @out_gtype is set
 *
 * Finds the first element from @first that is also contained
 * in @second. If no matching format is found, %FALSE is returned
 * and @out_gtype and @out_mime_type are set to %G_TYPE_INVALID and
 * %NULL respectively.
 *
 * Returns: %TRUE if a matching format was found.
 */
gboolean
gdk_content_formats_match (const GdkContentFormats *first,
                           const GdkContentFormats *second,
                           GType                   *out_gtype,
                           const char             **out_mime_type)
{
  gsize i;

  g_return_val_if_fail (first != NULL, FALSE);
  g_return_val_if_fail (second != NULL, FALSE);

  if (out_gtype)
    *out_gtype = G_TYPE_INVALID;
  if (out_mime_type)
    *out_mime_type = NULL;

  for (i = 0; i < first->n_gtypes; i++)
    {
      if (gdk_content_formats_contain_gtype (second, first->gtypes[i]))
        {
          if (out_gtype)
            *out_gtype = first->gtypes[i];
          return TRUE;
        }
    }

  for (i = 0; i < first->n_mime_types; i++)
    {
      if (gdk_content_formats_contain_interned_mime_type (second, first->mime_types[i]))
        {
          if (out_mime_type)
            *out_mime_type = first->mime_types[i];
          return TRUE;
        }
    }

  return FALSE;
}

/**
 * gdk_content_formats_contain_gtype:
 * @formats: a #GdkContentFormats
 * @type: the #GType to search for
 *
 * Checks if a given #GType is part of the given @formats.
 *
 * Returns: %TRUE if the #GType was found
 **/
gboolean
gdk_content_formats_contain_gtype (const GdkContentFormats *formats,
                                   GType                    type)
{
  g_return_val_if_fail (formats != NULL, FALSE);

  gsize i;

  for (i = 0; i < formats->n_gtypes; i++)
    {
      if (type == formats->gtypes[i])
        return TRUE;
    }

  return FALSE;
}

/**
 * gdk_content_formats_contain_mime_type:
 * @formats: a #GdkContentFormats
 * @mime_type: the mime type to search for
 *
 * Checks if a given mime type is part of the given @formats.
 *
 * Returns: %TRUE if the mime_type was found
 **/
gboolean
gdk_content_formats_contain_mime_type (const GdkContentFormats *formats,
                                       const char              *mime_type)
{
  g_return_val_if_fail (formats != NULL, FALSE);
  g_return_val_if_fail (mime_type != NULL, FALSE);

  return gdk_content_formats_contain_interned_mime_type (formats,
                                                         g_intern_string (mime_type));
}

/**
 * gdk_content_formats_get_gtypes:
 * @formats: a #GdkContentFormats
 * @n_gtypes: (out) (allow-none): optional pointer to take the
 *     number of #GTypes contained in the return value
 *
 * Gets the #GTypes included in @formats. Note that @formats may not
 * contain any #GTypes, in particular when they are empty. In that
 * case %NULL will be returned. 
 *
 * Returns: (transfer none) (nullable): %G_TYPE_INVALID-terminated array of 
 *     types included in @formats or %NULL if none.
 **/
const GType *
gdk_content_formats_get_gtypes (GdkContentFormats *formats,
                                gsize             *n_gtypes)
{
  g_return_val_if_fail (formats != NULL, NULL);

  if (n_gtypes)
    *n_gtypes = formats->n_gtypes;
  
  return formats->gtypes;
}

/**
 * gdk_content_formats_get_mime_types:
 * @formats: a #GdkContentFormats
 * @n_mime_types: (out) (allow-none): optional pointer to take the
 *     number of mime types contained in the return value
 *
 * Gets the mime types included in @formats. Note that @formats may not
 * contain any mime types, in particular when they are empty. In that
 * case %NULL will be returned. 
 *
 * Returns: (transfer none) (nullable): %NULL-terminated array of 
 *     interned strings of mime types included in @formats or %NULL
 *     if none.
 **/
const char * const *
gdk_content_formats_get_mime_types (GdkContentFormats *formats,
                                    gsize             *n_mime_types)
{
  g_return_val_if_fail (formats != NULL, NULL);

  if (n_mime_types)
    *n_mime_types = formats->n_mime_types;
  
  return formats->mime_types;
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
  GSList *gtypes;
  gsize n_gtypes;
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
  GType *gtypes;
  const char **mime_types;
  GSList *l;
  gsize i;

  g_return_val_if_fail (builder != NULL, NULL);

  gtypes = g_new (GType, builder->n_gtypes + 1);
  i = builder->n_gtypes;
  gtypes[i--] = G_TYPE_INVALID;
  /* add backwards because most important type is last in the list */
  for (l = builder->gtypes; l; l = l->next)
    gtypes[i--] = GPOINTER_TO_SIZE (l->data);

  mime_types = g_new (const char *, builder->n_mime_types + 1);
  i = builder->n_mime_types;
  mime_types[i--] = NULL;
  /* add backwards because most important type is last in the list */
  for (l = builder->mime_types; l; l = l->next)
    mime_types[i--] = l->data;

  result = gdk_content_formats_new_take (gtypes, builder->n_gtypes,
                                         mime_types, builder->n_mime_types);

  g_slist_free (builder->gtypes);
  g_slist_free (builder->mime_types);
  g_slice_free (GdkContentFormatsBuilder, builder);

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
  gsize i;

  g_return_if_fail (builder != NULL);
  g_return_if_fail (formats != NULL);

  for (i = 0; i < formats->n_gtypes; i++)
    gdk_content_formats_builder_add_gtype (builder, formats->gtypes[i]);

  for (i = 0; i < formats->n_mime_types; i++)
    gdk_content_formats_builder_add_mime_type (builder, formats->mime_types[i]);
}

/**
 * gdk_content_formats_builder_add_gtype:
 * @builder: a #GdkContentFormatsBuilder
 * @type: a #GType
 *
 * Appends @gtype to @builder if it has not already been added.
 **/
void
gdk_content_formats_builder_add_gtype (GdkContentFormatsBuilder *builder,
                                       GType                     type)
{
  g_return_if_fail (builder != NULL);

  if (g_slist_find (builder->gtypes, GSIZE_TO_POINTER (type)))
    return;

  builder->gtypes = g_slist_prepend (builder->gtypes, GSIZE_TO_POINTER (type));
  builder->n_gtypes++;
}

/**
 * gdk_content_formats_builder_add_mime_type:
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

