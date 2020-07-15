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
 * @Short_description: Filtering files
 * @Title: GtkFileFilter
 * @see_also: #GtkFileChooser
 *
 * A GtkFileFilter can be used to restrict the files being shown in a
 * #GtkFileChooser. Files can be filtered based on their name (with
 * gtk_file_filter_add_pattern()) or on their mime type (with
 * gtk_file_filter_add_mime_type()).
 *
 * Filtering by mime types handles aliasing and subclassing of mime
 * types; e.g. a filter for text/plain also matches a file with mime
 * type application/rtf, since application/rtf is a subclass of
 * text/plain. Note that #GtkFileFilter allows wildcards for the
 * subtype of a mime type, so you can e.g. filter for image/\*.
 *
 * Normally, file filters are used by adding them to a #GtkFileChooser
 * (see gtk_file_chooser_add_filter()), but it is also possible to
 * manually use a file filter on any #GtkFilterListModel containing
 * #GFileInfo objects.
 *
 * # GtkFileFilter as GtkBuildable
 *
 * The GtkFileFilter implementation of the GtkBuildable interface
 * supports adding rules using the <mime-types> and <patterns>
 * elements and listing the rules within. Specifying a <mime-type>
 * or <pattern> has the same effect as as calling
 * gtk_file_filter_add_mime_type() or gtk_file_filter_add_pattern().
 *
 * An example of a UI definition fragment specifying GtkFileFilter
 * rules:
 * |[
 * <object class="GtkFileFilter">
 *   <property name="name" translatable="yes">Text and Images</property>
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

#include "gtkfilefilterprivate.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkfilter.h"

typedef struct _GtkFileFilterClass GtkFileFilterClass;
typedef struct _FilterRule FilterRule;

#define GTK_FILE_FILTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_FILTER, GtkFileFilterClass))
#define GTK_IS_FILE_FILTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_FILTER))
#define GTK_FILE_FILTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_FILTER, GtkFileFilterClass))

typedef enum {
  FILTER_RULE_PATTERN,
  FILTER_RULE_PATTERN_CI,
  FILTER_RULE_MIME_TYPE,
  FILTER_RULE_PIXBUF_FORMATS
} FilterRuleType;

struct _GtkFileFilterClass
{
  GtkFilterClass parent_class;
};

struct _GtkFileFilter
{
  GtkFilter parent_instance;

  char *name;
  GSList *rules;

  char **attributes;
};

struct _FilterRule
{
  FilterRuleType type;

  union {
    char *pattern;
    char **content_types;
  } u;
};

enum {
  PROP_0,
  PROP_NAME,
  NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };


static gboolean       gtk_file_filter_match          (GtkFilter *filter,
                                                      gpointer   item);
static GtkFilterMatch gtk_file_filter_get_strictness (GtkFilter *filter);

static void           gtk_file_filter_buildable_init (GtkBuildableIface  *iface);


G_DEFINE_TYPE_WITH_CODE (GtkFileFilter, gtk_file_filter, GTK_TYPE_FILTER,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_file_filter_buildable_init))

static void
gtk_file_filter_init (GtkFileFilter *object)
{
}

static void
gtk_file_filter_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkFileFilter *filter = GTK_FILE_FILTER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      gtk_file_filter_set_name (filter, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_file_filter_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkFileFilter *filter = GTK_FILE_FILTER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, filter->name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
filter_rule_free (FilterRule *rule)
{
  switch (rule->type)
    {
    case FILTER_RULE_PATTERN:
    case FILTER_RULE_PATTERN_CI:
      g_free (rule->u.pattern);
      break;
    case FILTER_RULE_MIME_TYPE:
    case FILTER_RULE_PIXBUF_FORMATS:
      g_strfreev (rule->u.content_types);
      break;
    default:
      break;
    }
  g_slice_free (FilterRule, rule);
}

static void
gtk_file_filter_finalize (GObject  *object)
{
  GtkFileFilter *filter = GTK_FILE_FILTER (object);

  g_slist_free_full (filter->rules, (GDestroyNotify)filter_rule_free);
  g_strfreev (filter->attributes);

  g_free (filter->name);

  G_OBJECT_CLASS (gtk_file_filter_parent_class)->finalize (object);
}

static void
gtk_file_filter_class_init (GtkFileFilterClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkFilterClass *filter_class = GTK_FILTER_CLASS (class);

  gobject_class->set_property = gtk_file_filter_set_property;
  gobject_class->get_property = gtk_file_filter_get_property;
  gobject_class->finalize = gtk_file_filter_finalize;

  filter_class->get_strictness = gtk_file_filter_get_strictness;
  filter_class->match = gtk_file_filter_match;

  /**
   * GtkFileFilter:name:
   *
   * The human-readable name of the filter.
   *
   * This is the string that will be displayed in the file selector user
   * interface if there is a selectable list of filters.
   */
  props[PROP_NAME] =
      g_param_spec_string ("name",
                           P_("Name"),
                           P_("The human-readable name for this filter"),
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, props);
}

/*
 * GtkBuildable implementation
 */

typedef enum
{
  PARSE_MIME_TYPES,
  PARSE_PATTERNS
} ParserType;

typedef struct
{
  GtkFileFilter *filter;
  GtkBuilder    *builder;
  ParserType     type;
  GString       *string;
  gboolean       parsing;
  gboolean       ignore_case;
} SubParserData;

static void
parser_start_element (GtkBuildableParseContext  *context,
                      const char                *element_name,
                      const char               **names,
                      const char               **values,
                      gpointer                   user_data,
                      GError                   **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (strcmp (element_name, "patterns") == 0)
    {
      gboolean ignore_case = FALSE;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "ignore-case", &ignore_case,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->ignore_case = ignore_case;
    }
  else if (!g_markup_collect_attributes (element_name, names, values, error,
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
parser_text_element (GtkBuildableParseContext  *context,
                     const char                *text,
                     gsize                      text_len,
                     gpointer                   user_data,
                     GError                   **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (data->parsing)
    g_string_append_len (data->string, text, text_len);
}

static void
parser_end_element (GtkBuildableParseContext  *context,
                    const char                *element_name,
                    gpointer                   user_data,
                    GError                   **error)
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
          gtk_file_filter_add_pattern (data->filter, data->string->str, data->ignore_case);
          break;
        default:
          break;
        }
    }

  g_string_set_size (data->string, 0);
  data->parsing = FALSE;
}

static const GtkBuildableParser sub_parser =
  {
    parser_start_element,
    parser_end_element,
    parser_text_element,
  };

static gboolean
gtk_file_filter_buildable_custom_tag_start (GtkBuildable       *buildable,
                                            GtkBuilder         *builder,
                                            GObject            *child,
                                            const char         *tagname,
                                            GtkBuildableParser *parser,
                                            gpointer           *parser_data)
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
                                          const char   *tagname,
                                          gpointer      user_data)
{
  if (strcmp (tagname, "mime-types") == 0 ||
      strcmp (tagname, "patterns") == 0)
    {
      SubParserData *data = (SubParserData*)user_data;

      g_string_free (data->string, TRUE);
      g_slice_free (SubParserData, data);
    }
}

static void
gtk_file_filter_buildable_init (GtkBuildableIface *iface)
{
  iface->custom_tag_start = gtk_file_filter_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_file_filter_buildable_custom_tag_end;
}

/*
 * Public api
 */

/**
 * gtk_file_filter_new:
 *
 * Creates a new #GtkFileFilter with no rules added to it.
 *
 * Such a filter doesn’t accept any files, so is not
 * particularly useful until you add rules with
 * gtk_file_filter_add_mime_type(), gtk_file_filter_add_pattern(),
 * or gtk_file_filter_add_pixbuf_formats().
 *
 * To create a filter that accepts any file, use:
 * |[<!-- language="C" -->
 * GtkFileFilter *filter = gtk_file_filter_new ();
 * gtk_file_filter_add_pattern (filter, "*");
 * ]|
 *
 * Returns: a new #GtkFileFilter
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
 * Sets a human-readable name of the filter; this is the string
 * that will be displayed in the file chooser if there is a selectable
 * list of filters.
 **/
void
gtk_file_filter_set_name (GtkFileFilter *filter,
                          const char    *name)
{
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));

  if (g_strcmp0 (filter->name, name) == 0)
    return;

  g_free (filter->name);
  filter->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (filter), props[PROP_NAME]);
}

/**
 * gtk_file_filter_get_name:
 * @filter: a #GtkFileFilter
 *
 * Gets the human-readable name for the filter. See gtk_file_filter_set_name().
 *
 * Returns: (nullable): The human-readable name of the filter,
 *   or %NULL. This value is owned by GTK and must not
 *   be modified or freed.
 **/
const char *
gtk_file_filter_get_name (GtkFileFilter *filter)
{
  g_return_val_if_fail (GTK_IS_FILE_FILTER (filter), NULL);

  return filter->name;
}

static void
file_filter_add_rule (GtkFileFilter *filter,
                      FilterRule    *rule)
{
  filter->rules = g_slist_append (filter->rules, rule);

  gtk_filter_changed (GTK_FILTER (filter), GTK_FILTER_CHANGE_LESS_STRICT);
}

static void
file_filter_add_attribute (GtkFileFilter *filter,
                           const char    *attribute)
{
  int i;

  if (filter->attributes)
    for (i = 0; filter->attributes[i]; i++)
      {
        if (strcmp (filter->attributes[i], attribute) == 0)
          return;
      }
  else
    i = 0;

  filter->attributes = (char **)g_renew (char **, filter->attributes, i + 2);
  filter->attributes[i] = g_strdup (attribute);
  filter->attributes[i + 1] = NULL;
}

/**
 * gtk_file_filter_add_mime_type:
 * @filter: A #GtkFileFilter
 * @mime_type: name of a MIME type
 *
 * Adds a rule allowing a given mime type to @filter.
 **/
void
gtk_file_filter_add_mime_type (GtkFileFilter *filter,
                               const char    *mime_type)
{
  FilterRule *rule;

  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  g_return_if_fail (mime_type != NULL);

  rule = g_slice_new (FilterRule);
  rule->type = FILTER_RULE_MIME_TYPE;
  rule->u.content_types = g_new0 (char *, 2);
  rule->u.content_types[0] = g_content_type_from_mime_type (mime_type);

  file_filter_add_attribute (filter, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
  file_filter_add_rule (filter, rule);
}

/**
 * gtk_file_filter_add_pattern:
 * @filter: a #GtkFileFilter
 * @pattern: a shell style glob
 * @ignore_case: whether to ignore case differences
 *
 * Adds a rule allowing a shell style glob to a filter.
 **/
void
gtk_file_filter_add_pattern (GtkFileFilter *filter,
                             const char    *pattern,
                             gboolean       ignore_case)
{
  FilterRule *rule;

  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  g_return_if_fail (pattern != NULL);

  rule = g_slice_new (FilterRule);
  rule->type = ignore_case ? FILTER_RULE_PATTERN_CI : FILTER_RULE_PATTERN;
  rule->u.pattern = g_strdup (pattern);

  file_filter_add_attribute (filter, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
  file_filter_add_rule (filter, rule);
}

/**
 * gtk_file_filter_add_pixbuf_formats:
 * @filter: a #GtkFileFilter
 *
 * Adds a rule allowing image files in the formats supported
 * by GdkPixbuf.
 *
 * This is equivalent to calling gtk_file_filter_add_mime_type()
 * for all the supported mime types.
 **/
void
gtk_file_filter_add_pixbuf_formats (GtkFileFilter *filter)
{
  FilterRule *rule;
  GPtrArray *array;
  GSList *formats, *l;

  g_return_if_fail (GTK_IS_FILE_FILTER (filter));

  rule = g_slice_new (FilterRule);
  rule->type = FILTER_RULE_PIXBUF_FORMATS;

  array = g_ptr_array_new ();

  formats = gdk_pixbuf_get_formats ();
  for (l = formats; l; l = l->next)
    {
      int i;
      char **mime_types;

      mime_types = gdk_pixbuf_format_get_mime_types (l->data);

      for (i = 0; mime_types[i] != NULL; i++)
        {
          g_ptr_array_add (array, g_content_type_from_mime_type (mime_types[i]));
        }
    }
  g_slist_free (formats);

  g_ptr_array_add (array, NULL);

  rule->u.content_types = (char **)g_ptr_array_free (array, FALSE);

  file_filter_add_attribute (filter, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
  file_filter_add_rule (filter, rule);
}

/**
 * gtk_file_filter_get_attributes:
 * @filter: a #GtkFileFilter
 *
 * Gets the attributes that need to be filled in for the #GFileInfo
 * passed to gtk_file_filter_filter()
 *
 * This function will not typically be used by applications;
 * it is intended principally for use in the implementation
 * of #GtkFileChooser.
 *
 * Returns: (transfer none): the attributes
 **/
const char **
gtk_file_filter_get_attributes (GtkFileFilter *filter)
{
  return (const char **)filter->attributes;
}

#ifdef GDK_WINDOWING_QUARTZ

#import <Foundation/Foundation.h>

NSArray * _gtk_file_filter_get_as_pattern_nsstrings (GtkFileFilter *filter)
{
  NSMutableArray *array = [[NSMutableArray alloc] init];
  GSList *tmp_list;

  for (tmp_list = filter->rules; tmp_list; tmp_list = tmp_list->next)
    {
      FilterRule *rule = tmp_list->data;

      switch (rule->type)
        {
        case FILTER_RULE_MIME_TYPE:
          {
            // convert mime-types to UTI
            NSString *mime_type_nsstring = [NSString stringWithUTF8String: rule->u.content_types[0]];
            NSString *uti_nsstring = (NSString *) UTTypeCreatePreferredIdentifierForTag (kUTTagClassMIMEType, (CFStringRef) mime_type_nsstring, NULL);
            if (uti_nsstring == NULL)
              {
                [array release];
                return NULL;
              }
            [array addObject:uti_nsstring];
          }
          break;

        case FILTER_RULE_PATTERN:
        case FILTER_RULE_PATTERN_CI:
          {
            // patterns will need to be stripped of their leading *.
            GString *pattern = g_string_new (rule->u.pattern);
            if (strncmp (pattern->str, "*.", 2) == 0)
              {
                pattern = g_string_erase (pattern, 0, 2);
              }
            else if (strncmp (pattern->str, "*", 1) == 0)
              {
                pattern = g_string_erase (pattern, 0, 1);
              }
            gchar *pattern_c = g_string_free (pattern, FALSE);
            NSString *pattern_nsstring = [NSString stringWithUTF8String:pattern_c];
            g_free (pattern_c);
            [pattern_nsstring retain];
            [array addObject:pattern_nsstring];
          }
          break;

        case FILTER_RULE_PIXBUF_FORMATS:
          {
            GSList *formats, *l;

            formats = gdk_pixbuf_get_formats ();
            for (l = formats; l; l = l->next)
              {
                int i;
                gchar **extensions;

                extensions = gdk_pixbuf_format_get_extensions (l->data);

                for (i = 0; extensions[i] != NULL; i++)
                  {
                    NSString *extension = [NSString stringWithUTF8String: extensions[i]];
                    [extension retain];
                    [array addObject:extension];
                  }
                g_strfreev (extensions);
              }
            g_slist_free (formats);
            break;
          }
       }
    }
  return array;
}
#endif

char **
_gtk_file_filter_get_as_patterns (GtkFileFilter *filter)
{
  GPtrArray *array;
  GSList *tmp_list;

  array = g_ptr_array_new_with_free_func (g_free);

  for (tmp_list = filter->rules; tmp_list; tmp_list = tmp_list->next)
    {
      FilterRule *rule = tmp_list->data;

      switch (rule->type)
        {
        case FILTER_RULE_MIME_TYPE:
          g_ptr_array_free (array, TRUE);
          return NULL;
          break;

        case FILTER_RULE_PATTERN:
        case FILTER_RULE_PATTERN_CI:
          g_ptr_array_add (array, g_strdup (rule->u.pattern));
          break;

        case FILTER_RULE_PIXBUF_FORMATS:
          {
            GSList *formats, *l;

            formats = gdk_pixbuf_get_formats ();
            for (l = formats; l; l = l->next)
              {
                int i;
                char **extensions;

                extensions = gdk_pixbuf_format_get_extensions (l->data);

                for (i = 0; extensions[i] != NULL; i++)
                  g_ptr_array_add (array, g_strdup_printf ("*.%s", extensions[i]));

                g_strfreev (extensions);
              }
            g_slist_free (formats);
            break;
          }
        default:
          break;
        }
    }

  g_ptr_array_add (array, NULL); /* Null terminate */
  return (char **)g_ptr_array_free (array, FALSE);
}

static GtkFilterMatch
gtk_file_filter_get_strictness (GtkFilter *filter)
{
  GtkFileFilter *file_filter = GTK_FILE_FILTER (filter);

  /* Handle only the documented cases for 'match all'
   * and match none. There are of course other ways to
   * make filters that do this.
   */
  if (file_filter->rules == NULL)
    return GTK_FILTER_MATCH_NONE;

  if (file_filter->rules->next == NULL)
    {
      FilterRule *rule = file_filter->rules->data;
      if (rule->type == FILTER_RULE_PATTERN &&
          strcmp (rule->u.pattern, "*") == 0)
        return GTK_FILTER_MATCH_ALL;
    }

  return GTK_FILTER_MATCH_SOME;
}

static gboolean
gtk_file_filter_match (GtkFilter *filter,
                       gpointer   item)
{
  GtkFileFilter *file_filter = GTK_FILE_FILTER (filter);
  GFileInfo *info = item;
  GSList *tmp_list;

  if (!G_IS_FILE_INFO (item))
    return TRUE;

  for (tmp_list = file_filter->rules; tmp_list; tmp_list = tmp_list->next)
    {
      FilterRule *rule = tmp_list->data;
      gboolean casefold = FALSE;

      switch (rule->type)
        {
        case FILTER_RULE_PATTERN_CI:
          casefold = TRUE;
          G_GNUC_FALLTHROUGH;
        case FILTER_RULE_PATTERN:
          {
            const char *display_name;

            display_name = g_file_info_get_display_name (info);
            if (display_name)
              {
                if (_gtk_fnmatch (rule->u.pattern, display_name, FALSE, casefold))
                  return TRUE;
              }
          }
          break;

        case FILTER_RULE_MIME_TYPE:
        case FILTER_RULE_PIXBUF_FORMATS:
          {
            const char *filter_content_type;

            filter_content_type = g_file_info_get_content_type (info);
            if (filter_content_type)
              {
                int i;

                for (i = 0; rule->u.content_types[i]; i++)
                  {
                    if (g_content_type_is_a (filter_content_type, rule->u.content_types[i]))
                      return TRUE;
                  }
              }
          }
          break;

        default:
          break;
        }
    }

  return FALSE;
}

/**
 * gtk_file_filter_to_gvariant:
 * @filter: a #GtkFileFilter
 *
 * Serialize a file filter to an a{sv} variant.
 *
 * Returns: (transfer none): a new, floating, #GVariant
 */
GVariant *
gtk_file_filter_to_gvariant (GtkFileFilter *filter)
{
  GVariantBuilder builder;
  GSList *l;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(us)"));
  for (l = filter->rules; l; l = l->next)
    {
      FilterRule *rule = l->data;
      int i;

      switch (rule->type)
        {
        case FILTER_RULE_PATTERN:
          g_variant_builder_add (&builder, "(us)", 0, rule->u.pattern);
          break;

        case FILTER_RULE_PATTERN_CI:
          g_variant_builder_add (&builder, "(us)", 2, rule->u.pattern);
          break;

        case FILTER_RULE_MIME_TYPE:
        case FILTER_RULE_PIXBUF_FORMATS:
          for (i = 0; rule->u.content_types[i]; i++)
            g_variant_builder_add (&builder, "(us)", 1, rule->u.content_types[i]);
          break;

        default:
          break;
        }
    }

  return g_variant_new ("(s@a(us))", filter->name, g_variant_builder_end (&builder));
}

/**
 * gtk_file_filter_new_from_gvariant:
 * @variant: an a{sv} #GVariant
 *
 * Deserialize a file filter from an a{sv} variant in
 * the format produced by gtk_file_filter_to_gvariant().
 *
 * Returns: (transfer full): a new #GtkFileFilter object
 */
GtkFileFilter *
gtk_file_filter_new_from_gvariant (GVariant *variant)
{
  GtkFileFilter *filter;
  GVariantIter *iter;
  const char *name;
  int type;
  char *tmp;

  filter = gtk_file_filter_new ();

  g_variant_get (variant, "(&sa(us))", &name, &iter);

  gtk_file_filter_set_name (filter, name);

  while (g_variant_iter_next (iter, "(u&s)", &type, &tmp))
    {
      switch (type)
        {
        case 0:
          gtk_file_filter_add_pattern (filter, tmp, FALSE);
          break;
        case 2:
          gtk_file_filter_add_pattern (filter, tmp, TRUE);
          break;
        case 1:
          gtk_file_filter_add_mime_type (filter, tmp);
          break;
        default:
          break;
       }
    }
  g_variant_iter_free (iter);

  return filter;
}
