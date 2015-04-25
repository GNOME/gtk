/* GTK - The GIMP Toolkit
 * gtkfilefilter.c: Filters for selecting a file subset
 * Copyright (C) 2003, Red Hat, Inc.
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

/**
 * SECTION:gtkfilefilter
 * @Short_description: A filter for selecting a file subset
 * @Title: GtkFileFilter
 * @see_also: #GtkFileChooser
 *
 * A GtkFileFilter can be used to restrict the files being shown in a
 * #GtkFileChooser. Files can be filtered based on their name (with
 * gtk_file_filter_add_pattern()), on their mime type (with
 * gtk_file_filter_add_mime_type()), or by a custom filter function
 * (with gtk_file_filter_add_custom()).
 *
 * Filtering by mime types handles aliasing and subclassing of mime
 * types; e.g. a filter for text/plain also matches a file with mime
 * type application/rtf, since application/rtf is a subclass of
 * text/plain. Note that #GtkFileFilter allows wildcards for the
 * subtype of a mime type, so you can e.g. filter for image/\*.
 *
 * Normally, filters are used by adding them to a #GtkFileChooser,
 * see gtk_file_chooser_add_filter(), but it is also possible
 * to manually use a filter on a file with gtk_file_filter_filter().
 *
 * # GtkFileFilter as GtkBuildable
 *
 * The GtkFileFilter implementation of the GtkBuildable interface
 * supports adding rules using the <mime-types>, <patterns> and
 * <applications> elements and listing the rules within. Specifying
 * a <mime-type> or <pattern> has the same effect as as calling
 * gtk_file_filter_add_mime_type() or gtk_file_filter_add_pattern().
 *
 * An example of a UI definition fragment specifying GtkFileFilter
 * rules:
 * |[
 * <object class="GtkFileFilter">
 *   <mime-types>
 *     <mime-type>text/plain</mime-type>
 *     <mime-type>image/ *</mime-type>
 *   </mime-types>
 *   <patterns>
 *     <pattern>*.txt</pattern>
 *     <pattern>*.png</pattern>
 *   </patterns>
 * </object>
 * ]|
 */

#include "config.h"
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gtkfilefilter.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"

typedef struct _GtkFileFilterClass GtkFileFilterClass;
typedef struct _FilterRule FilterRule;

#define GTK_FILE_FILTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_FILTER, GtkFileFilterClass))
#define GTK_IS_FILE_FILTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_FILTER))
#define GTK_FILE_FILTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_FILTER, GtkFileFilterClass))

typedef enum {
  FILTER_RULE_PATTERN,
  FILTER_RULE_MIME_TYPE,
  FILTER_RULE_PIXBUF_FORMATS,
  FILTER_RULE_CUSTOM
} FilterRuleType;

struct _GtkFileFilterClass
{
  GInitiallyUnownedClass parent_class;
};

struct _GtkFileFilter
{
  GInitiallyUnowned parent_instance;

  gchar *name;
  GSList *rules;

  GtkFileFilterFlags needed;
};

struct _FilterRule
{
  FilterRuleType type;
  GtkFileFilterFlags needed;
  
  union {
    gchar *pattern;
    gchar *mime_type;
    GSList *pixbuf_formats;
    struct {
      GtkFileFilterFunc func;
      gpointer data;
      GDestroyNotify notify;
    } custom;
  } u;
};

static void gtk_file_filter_finalize   (GObject            *object);


static void     gtk_file_filter_buildable_init                 (GtkBuildableIface *iface);
static void     gtk_file_filter_buildable_set_name             (GtkBuildable *buildable,
                                                                const gchar  *name);
static const gchar* gtk_file_filter_buildable_get_name         (GtkBuildable *buildable);


static gboolean gtk_file_filter_buildable_custom_tag_start     (GtkBuildable  *buildable,
								GtkBuilder    *builder,
								GObject       *child,
								const gchar   *tagname,
								GMarkupParser *parser,
								gpointer      *data);
static void     gtk_file_filter_buildable_custom_tag_end       (GtkBuildable  *buildable,
								GtkBuilder    *builder,
								GObject       *child,
								const gchar   *tagname,
								gpointer      *data);

G_DEFINE_TYPE_WITH_CODE (GtkFileFilter, gtk_file_filter, G_TYPE_INITIALLY_UNOWNED,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_file_filter_buildable_init))

static void
gtk_file_filter_init (GtkFileFilter *object)
{
}

static void
gtk_file_filter_class_init (GtkFileFilterClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = gtk_file_filter_finalize;
}

static void
filter_rule_free (FilterRule *rule)
{
  switch (rule->type)
    {
    case FILTER_RULE_MIME_TYPE:
      g_free (rule->u.mime_type);
      break;
    case FILTER_RULE_PATTERN:
      g_free (rule->u.pattern);
      break;
    case FILTER_RULE_CUSTOM:
      if (rule->u.custom.notify)
	rule->u.custom.notify (rule->u.custom.data);
      break;
    case FILTER_RULE_PIXBUF_FORMATS:
      g_slist_free (rule->u.pixbuf_formats);
      break;
    default:
      g_assert_not_reached ();
    }

  g_slice_free (FilterRule, rule);
}

static void
gtk_file_filter_finalize (GObject  *object)
{
  GtkFileFilter *filter = GTK_FILE_FILTER (object);

  g_slist_foreach (filter->rules, (GFunc)filter_rule_free, NULL);
  g_slist_free (filter->rules);

  g_free (filter->name);

  G_OBJECT_CLASS (gtk_file_filter_parent_class)->finalize (object);
}

/*
 * GtkBuildable implementation
 */
static void
gtk_file_filter_buildable_init (GtkBuildableIface *iface)
{
  iface->custom_tag_start = gtk_file_filter_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_file_filter_buildable_custom_tag_end;
  iface->set_name = gtk_file_filter_buildable_set_name;
  iface->get_name = gtk_file_filter_buildable_get_name;
}

static void
gtk_file_filter_buildable_set_name (GtkBuildable *buildable,
                                    const gchar  *name)
{
  gtk_file_filter_set_name (GTK_FILE_FILTER (buildable), name);
}

static const gchar *
gtk_file_filter_buildable_get_name (GtkBuildable *buildable)
{
  return gtk_file_filter_get_name (GTK_FILE_FILTER (buildable));
}

typedef enum {
  PARSE_MIME_TYPES,
  PARSE_PATTERNS
} ParserType;

typedef struct {
  GtkFileFilter *filter;
  GtkBuilder    *builder;
  ParserType     type;
  GString       *string;
  gboolean       parsing;
} SubParserData;

static void
parser_start_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **names,
                      const gchar         **values,
                      gpointer              user_data,
                      GError              **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (!g_markup_collect_attributes (element_name, names, values, error,
                                    G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                    G_MARKUP_COLLECT_INVALID))
    {
      _gtk_builder_prefix_error (data->builder, context, error);
      return;
    }

  if (strcmp (element_name, "mime-types") == 0 ||
      strcmp (element_name, "patterns") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;
    }
  else if (strcmp (element_name, "mime-type") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "mime-types", error))
        return;

      data->parsing = TRUE;
    }
  else if (strcmp (element_name, "pattern") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "patterns", error))
        return;

      data->parsing = TRUE;
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkFileFilter", element_name,
                                        error);
    }
}

static void
parser_text_element (GMarkupParseContext *context,
                     const gchar         *text,
                     gsize                text_len,
                     gpointer             user_data,
                     GError             **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (data->parsing)
    g_string_append_len (data->string, text, text_len);
}

static void
parser_end_element (GMarkupParseContext *context,
                    const gchar         *element_name,
                    gpointer             user_data,
                    GError             **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (data->string)
    {
      switch (data->type)
        {
        case PARSE_MIME_TYPES:
          gtk_file_filter_add_mime_type (data->filter, data->string->str);
          break;
        case PARSE_PATTERNS:
          gtk_file_filter_add_pattern (data->filter, data->string->str);
          break;
        default:
          break;
        }
    }

  g_string_set_size (data->string, 0);
  data->parsing = FALSE;
}

static const GMarkupParser sub_parser =
  {
    parser_start_element,
    parser_end_element,
    parser_text_element,
  };

static gboolean
gtk_file_filter_buildable_custom_tag_start (GtkBuildable  *buildable,
                                            GtkBuilder    *builder,
                                            GObject       *child,
                                            const gchar   *tagname,
                                            GMarkupParser *parser,
                                            gpointer      *parser_data)
{
  SubParserData *data = NULL;

  if (strcmp (tagname, "mime-types") == 0)
    {
      data = g_slice_new0 (SubParserData);
      data->string = g_string_new ("");
      data->type = PARSE_MIME_TYPES;
      data->filter = GTK_FILE_FILTER (buildable);
      data->builder = builder;

      *parser = sub_parser;
      *parser_data = data;
    }
  else if (strcmp (tagname, "patterns") == 0)
    {
      data = g_slice_new0 (SubParserData);
      data->string = g_string_new ("");
      data->type = PARSE_PATTERNS;
      data->filter = GTK_FILE_FILTER (buildable);
      data->builder = builder;

      *parser = sub_parser;
      *parser_data = data;
    }

  return data != NULL;
}

static void
gtk_file_filter_buildable_custom_tag_end (GtkBuildable *buildable,
                                          GtkBuilder   *builder,
                                          GObject      *child,
                                          const gchar  *tagname,
                                          gpointer     *user_data)
{
  if (strcmp (tagname, "mime-types") == 0 ||
      strcmp (tagname, "patterns") == 0)
    {
      SubParserData *data = (SubParserData*)user_data;

      g_string_free (data->string, TRUE);
      g_slice_free (SubParserData, data);
    }
}


/**
 * gtk_file_filter_new:
 * 
 * Creates a new #GtkFileFilter with no rules added to it.
 * Such a filter doesn’t accept any files, so is not
 * particularly useful until you add rules with
 * gtk_file_filter_add_mime_type(), gtk_file_filter_add_pattern(),
 * or gtk_file_filter_add_custom(). To create a filter
 * that accepts any file, use:
 * |[<!-- language="C" -->
 * GtkFileFilter *filter = gtk_file_filter_new ();
 * gtk_file_filter_add_pattern (filter, "*");
 * ]|
 * 
 * Returns: a new #GtkFileFilter
 * 
 * Since: 2.4
 **/
GtkFileFilter *
gtk_file_filter_new (void)
{
  return g_object_new (GTK_TYPE_FILE_FILTER, NULL);
}

/**
 * gtk_file_filter_set_name:
 * @filter: a #GtkFileFilter
 * @name: (allow-none): the human-readable-name for the filter, or %NULL
 *   to remove any existing name.
 * 
 * Sets the human-readable name of the filter; this is the string
 * that will be displayed in the file selector user interface if
 * there is a selectable list of filters.
 * 
 * Since: 2.4
 **/
void
gtk_file_filter_set_name (GtkFileFilter *filter,
			  const gchar   *name)
{
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  
  g_free (filter->name);

  filter->name = g_strdup (name);
}

/**
 * gtk_file_filter_get_name:
 * @filter: a #GtkFileFilter
 * 
 * Gets the human-readable name for the filter. See gtk_file_filter_set_name().
 * 
 * Returns: The human-readable name of the filter,
 *   or %NULL. This value is owned by GTK+ and must not
 *   be modified or freed.
 * 
 * Since: 2.4
 **/
const gchar *
gtk_file_filter_get_name (GtkFileFilter *filter)
{
  g_return_val_if_fail (GTK_IS_FILE_FILTER (filter), NULL);
  
  return filter->name;
}

static void
file_filter_add_rule (GtkFileFilter *filter,
		      FilterRule    *rule)
{
  filter->needed |= rule->needed;
  filter->rules = g_slist_append (filter->rules, rule);
}

/**
 * gtk_file_filter_add_mime_type:
 * @filter: A #GtkFileFilter
 * @mime_type: name of a MIME type
 * 
 * Adds a rule allowing a given mime type to @filter.
 * 
 * Since: 2.4
 **/
void
gtk_file_filter_add_mime_type (GtkFileFilter *filter,
			       const gchar   *mime_type)
{
  FilterRule *rule;
  
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  g_return_if_fail (mime_type != NULL);

  rule = g_slice_new (FilterRule);
  rule->type = FILTER_RULE_MIME_TYPE;
  rule->needed = GTK_FILE_FILTER_MIME_TYPE;
  rule->u.mime_type = g_strdup (mime_type);

  file_filter_add_rule (filter, rule);
}

/**
 * gtk_file_filter_add_pattern:
 * @filter: a #GtkFileFilter
 * @pattern: a shell style glob
 * 
 * Adds a rule allowing a shell style glob to a filter.
 * 
 * Since: 2.4
 **/
void
gtk_file_filter_add_pattern (GtkFileFilter *filter,
			     const gchar   *pattern)
{
  FilterRule *rule;
  
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  g_return_if_fail (pattern != NULL);

  rule = g_slice_new (FilterRule);
  rule->type = FILTER_RULE_PATTERN;
  rule->needed = GTK_FILE_FILTER_DISPLAY_NAME;
  rule->u.pattern = g_strdup (pattern);

  file_filter_add_rule (filter, rule);
}

/**
 * gtk_file_filter_add_pixbuf_formats:
 * @filter: a #GtkFileFilter
 * 
 * Adds a rule allowing image files in the formats supported
 * by GdkPixbuf.
 * 
 * Since: 2.6
 **/
void
gtk_file_filter_add_pixbuf_formats (GtkFileFilter *filter)
{
  FilterRule *rule;
  
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));

  rule = g_slice_new (FilterRule);
  rule->type = FILTER_RULE_PIXBUF_FORMATS;
  rule->needed = GTK_FILE_FILTER_MIME_TYPE;
  rule->u.pixbuf_formats = gdk_pixbuf_get_formats ();
  file_filter_add_rule (filter, rule);
}


/**
 * gtk_file_filter_add_custom:
 * @filter: a #GtkFileFilter
 * @needed: bitfield of flags indicating the information that the custom
 *          filter function needs.
 * @func: callback function; if the function returns %TRUE, then
 *   the file will be displayed.
 * @data: data to pass to @func
 * @notify: function to call to free @data when it is no longer needed.
 * 
 * Adds rule to a filter that allows files based on a custom callback
 * function. The bitfield @needed which is passed in provides information
 * about what sorts of information that the filter function needs;
 * this allows GTK+ to avoid retrieving expensive information when
 * it isn’t needed by the filter.
 * 
 * Since: 2.4
 **/
void
gtk_file_filter_add_custom (GtkFileFilter         *filter,
			    GtkFileFilterFlags     needed,
			    GtkFileFilterFunc      func,
			    gpointer               data,
			    GDestroyNotify         notify)
{
  FilterRule *rule;
  
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  g_return_if_fail (func != NULL);

  rule = g_slice_new (FilterRule);
  rule->type = FILTER_RULE_CUSTOM;
  rule->needed = needed;
  rule->u.custom.func = func;
  rule->u.custom.data = data;
  rule->u.custom.notify = notify;

  file_filter_add_rule (filter, rule);
}

/**
 * gtk_file_filter_get_needed:
 * @filter: a #GtkFileFilter
 * 
 * Gets the fields that need to be filled in for the #GtkFileFilterInfo
 * passed to gtk_file_filter_filter()
 * 
 * This function will not typically be used by applications; it
 * is intended principally for use in the implementation of
 * #GtkFileChooser.
 * 
 * Returns: bitfield of flags indicating needed fields when
 *   calling gtk_file_filter_filter()
 * 
 * Since: 2.4
 **/
GtkFileFilterFlags
gtk_file_filter_get_needed (GtkFileFilter *filter)
{
  return filter->needed;
}

/**
 * gtk_file_filter_filter:
 * @filter: a #GtkFileFilter
 * @filter_info: a #GtkFileFilterInfo containing information
 *  about a file.
 * 
 * Tests whether a file should be displayed according to @filter.
 * The #GtkFileFilterInfo @filter_info should include
 * the fields returned from gtk_file_filter_get_needed().
 *
 * This function will not typically be used by applications; it
 * is intended principally for use in the implementation of
 * #GtkFileChooser.
 * 
 * Returns: %TRUE if the file should be displayed
 * 
 * Since: 2.4
 **/
gboolean
gtk_file_filter_filter (GtkFileFilter           *filter,
			const GtkFileFilterInfo *filter_info)
{
  GSList *tmp_list;

  for (tmp_list = filter->rules; tmp_list; tmp_list = tmp_list->next)
    {
      FilterRule *rule = tmp_list->data;

      if ((filter_info->contains & rule->needed) != rule->needed)
	continue;
      
      switch (rule->type)
	{
	case FILTER_RULE_MIME_TYPE:
	  if (filter_info->mime_type != NULL &&
              g_content_type_is_a (filter_info->mime_type, rule->u.mime_type))
	    return TRUE;
	  break;
	case FILTER_RULE_PATTERN:
	  if (filter_info->display_name != NULL &&
	      _gtk_fnmatch (rule->u.pattern, filter_info->display_name, FALSE))
	    return TRUE;
	  break;
	case FILTER_RULE_PIXBUF_FORMATS:
	  {
	    GSList *list;

	    if (!filter_info->mime_type)
	      break;

	    for (list = rule->u.pixbuf_formats; list; list = list->next)
	      {
		int i;
		gchar **mime_types;

		mime_types = gdk_pixbuf_format_get_mime_types (list->data);

		for (i = 0; mime_types[i] != NULL; i++)
		  {
		    if (strcmp (mime_types[i], filter_info->mime_type) == 0)
		      {
			g_strfreev (mime_types);
			return TRUE;
		      }
		  }

		g_strfreev (mime_types);
	      }
	    break;
	  }
	case FILTER_RULE_CUSTOM:
	  if (rule->u.custom.func (filter_info, rule->u.custom.data))
	    return TRUE;
	  break;
	}
    }

  return FALSE;
}
