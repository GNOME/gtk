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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>

#include "gtkfilefilter.h"
#include "gtkobject.h"

typedef struct _GtkFileFilterClass GtkFileFilterClass;
typedef struct _FilterRule FilterRule;

#define GTK_FILE_FILTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_FILTER, GtkFileFilterClass))
#define GTK_IS_FILE_FILTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_FILTER))
#define GTK_FILE_FILTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_FILTER, GtkFileFilterClass))

typedef enum {
  FILTER_RULE_PATTERN,
  FILTER_RULE_MIME_TYPE,
  FILTER_RULE_CUSTOM,
} FilterRuleType;

struct _GtkFileFilterClass
{
  GtkObjectClass parent_class;
};

struct _GtkFileFilter
{
  GtkObject parent_instance;
  
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
    struct {
      GtkFileFilterFunc func;
      gpointer data;
      GDestroyNotify notify;
    } custom;
  } u;
};

static void gtk_file_filter_class_init (GtkFileFilterClass *class);
static void gtk_file_filter_finalize   (GObject            *object);

static GObjectClass *parent_class;

GType
gtk_file_filter_get_type (void)
{
  static GType file_filter_type = 0;

  if (!file_filter_type)
    {
      static const GTypeInfo file_filter_info =
      {
	sizeof (GtkFileFilterClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_file_filter_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkFileFilter),
	0,		/* n_preallocs */
	NULL            /* init */
      };
      
      file_filter_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkFileFilter",
						 &file_filter_info, 0);
    }

  return file_filter_type;
}

static void
gtk_file_filter_class_init (GtkFileFilterClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

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
    }

  g_free (rule);
}

static void
gtk_file_filter_finalize (GObject  *object)
{
  GtkFileFilter *filter = GTK_FILE_FILTER (filter);

  g_slist_foreach (filter->rules, (GFunc)filter_rule_free, NULL);

  parent_class->finalize (object);
}

/**
 * gtk_file_filter_new:
 * 
 * Creates a new #GtkFileFilter with no rules added to it.
 * Such a filter doesn't accept any files, so is not
 * particularly useful until you add rules with
 * gtk_file_filter_add_mime_type(), gtk_file_filter_add_pattern(),
 * or gtk_file_filter_add_custom(). To create a filter
 * that accepts any file, use:
 *
 * <informalexample><programlisting>
 * GtkFileFilter *filter = gtk_file_filter_new ();
 * gtk_file_filter_add_pattern (filter, "*");
 * </programlisting></programlisting>
 * 
 * Return value: a new #GtkFileFilter
 **/
GtkFileFilter *
gtk_file_filter_new (void)
{
  return g_object_new (GTK_TYPE_FILE_FILTER, NULL);
}

/**
 * gtk_file_filter_set_name:
 * @filter: a #GtkFileFilter
 * @name: the human-readable-name for the filter, or %NULL
 *   to remove any existing name.
 * 
 * Sets the human-readable name of the filter; this is the string
 * that will be displayed in the file selector user interface if
 * there is a selectable list of filters.
 **/
void
gtk_file_filter_set_name (GtkFileFilter *filter,
			  const gchar   *name)
{
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  
  if (filter->name)
    g_free (filter->name);

  filter->name = g_strdup (name);
}

/**
 * gtk_file_filter_get_name:
 * @filter: a #GtkFileFilter
 * 
 * Gets the human-readable name for the filter. See gtk_file_filter_set_name().
 * 
 * Return value: The human-readable name of the filter,
 *   or %NULL. This value is owned by GTK+ and must not
 *   be modified or freed.
 **/
G_CONST_RETURN gchar *
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
 **/
void
gtk_file_filter_add_mime_type (GtkFileFilter *filter,
			       const gchar   *mime_type)
{
  FilterRule *rule;
  
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  g_return_if_fail (mime_type != NULL);

  rule = g_new (FilterRule, 1);
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
 **/
void
gtk_file_filter_add_pattern (GtkFileFilter *filter,
			     const gchar   *pattern)
{
  FilterRule *rule;
  
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  g_return_if_fail (pattern != NULL);

  rule = g_new (FilterRule, 1);
  rule->type = FILTER_RULE_PATTERN;
  rule->needed = GTK_FILE_FILTER_DISPLAY_NAME;
  rule->u.pattern = g_strdup (pattern);

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
 * it isn't needed by the filter.
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

  rule = g_new (FilterRule, 1);
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
 * Gets the fields that need to be filled in for the structure
 * passed to gtk_file_filter_filter()
 * 
 * This function will not typically be used by applications; it
 * is intended principally for use in the implementation of
 * #GtkFileChooser.
 * 
 * Return value: bitfield of flags indicating needed fields when
 *   calling gtk_file_filter_filter()
 **/
GtkFileFilterFlags
gtk_file_filter_get_needed (GtkFileFilter *filter)
{
  return filter->needed;
}

/**
 * gtk_file_filter_filter:
 * @filter: a #GtkFileFilter
 * @filter_info: a #GtkFileFilterInfo structure containing information
 *  about a file.
 * 
 * Tests whether a file should be displayed according to @filter.
 * The #GtkFileFilterInfo structure @filter_info should include
 * the fields returned feom gtk_file_filter_get_needed().
 *
 * This function will not typically be used by applications; it
 * is intended principally for use in the implementation of
 * #GtkFileChooser.
 * 
 * Return value: %TRUE if the file should be displayed
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
	  if (filter_info->mime_type != NULL
	      && strcmp (rule->u.mime_type, filter_info->mime_type) == 0)
	    return TRUE;
	  break;
	case FILTER_RULE_PATTERN:
	  if (_gtk_fnmatch (rule->u.pattern, filter_info->display_name))
	    return TRUE;
	  break;
	case FILTER_RULE_CUSTOM:
	  if (rule->u.custom.func (filter_info, rule->u.custom.data))
	    return TRUE;
	  break;
	}
    }

  return FALSE;
}
