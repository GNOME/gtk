/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#include "config.h"

#include <gdk/gdk.h>
#include <stdlib.h>

#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"
#include "gtkthemingengine.h"
#include "gtkintl.h"
#include "gtkwidget.h"
#include "gtkprivate.h"

#include "gtkalias.h"

typedef struct GtkStyleContextPrivate GtkStyleContextPrivate;
typedef struct GtkStyleProviderData GtkStyleProviderData;
typedef struct GtkStyleRegion GtkStyleRegion;
typedef struct GtkChildClass GtkChildClass;
typedef struct PropertyValue PropertyValue;

struct GtkChildClass
{
  GQuark class_quark;
  GtkChildClassFlags flags;
};

struct GtkStyleProviderData
{
  GtkStyleProvider *provider;
  guint priority;
};

struct PropertyValue
{
  GType       widget_type;
  GParamSpec *pspec;
  GValue      value;
};

struct GtkStyleRegion
{
  GArray *style_classes;
  GArray *child_style_classes;
};

struct GtkStyleContextPrivate
{
  GdkScreen *screen;

  GList *providers;
  GList *providers_last;

  GSList *icon_factories;

  GtkStyleSet *store;
  GtkWidgetPath *widget_path;

  GArray *property_cache;

  GtkStateFlags state_flags;
  GSList *regions;

  GtkThemingEngine *theming_engine;
};

enum {
  PROP_0,
  PROP_SCREEN
};

static void gtk_style_context_finalize (GObject *object);

static void gtk_style_context_impl_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec);
static void gtk_style_context_impl_get_property (GObject      *object,
                                                 guint         prop_id,
                                                 GValue       *value,
                                                 GParamSpec   *pspec);


G_DEFINE_TYPE (GtkStyleContext, gtk_style_context, G_TYPE_OBJECT)

static void
gtk_style_context_class_init (GtkStyleContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_style_context_finalize;
  object_class->set_property = gtk_style_context_impl_set_property;
  object_class->get_property = gtk_style_context_impl_get_property;

  g_object_class_install_property (object_class,
				   PROP_SCREEN,
				   g_param_spec_object ("screen",
                                                        P_("Screen"),
                                                        P_("The associated GdkScreen"),
                                                        GDK_TYPE_SCREEN,
                                                        GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkStyleContextPrivate));
}

static GtkStyleRegion *
style_region_new (void)
{
  GtkStyleRegion *region;

  region = g_slice_new0 (GtkStyleRegion);
  region->style_classes = g_array_new (FALSE, FALSE, sizeof (GQuark));
  region->child_style_classes = g_array_new (FALSE, FALSE, sizeof (GtkChildClass));

  return region;
}

static void
style_region_free (GtkStyleRegion *region)
{
  g_array_free (region->style_classes, TRUE);
  g_array_free (region->child_style_classes, TRUE);
  g_slice_free (GtkStyleRegion, region);
}

static GtkStyleRegion *
style_region_copy (const GtkStyleRegion *region)
{
  GtkStyleRegion *copy;

  copy = style_region_new ();
  g_array_insert_vals (copy->style_classes, 0,
                       region->style_classes->data,
                       region->style_classes->len);

  g_array_insert_vals (copy->child_style_classes, 0,
                       region->child_style_classes->data,
                       region->child_style_classes->len);
  return copy;
}

static void
gtk_style_context_init (GtkStyleContext *style_context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleRegion *region;

  priv = style_context->priv = G_TYPE_INSTANCE_GET_PRIVATE (style_context,
                                                            GTK_TYPE_STYLE_CONTEXT,
                                                            GtkStyleContextPrivate);

  priv->store = gtk_style_set_new ();
  priv->theming_engine = (GtkThemingEngine *) gtk_theming_engine_load (NULL);

  /* Create default region */
  region = style_region_new ();
  priv->regions = g_slist_prepend (priv->regions, region);
}

static GtkStyleProviderData *
style_provider_data_new (GtkStyleProvider *provider,
                         guint             priority)
{
  GtkStyleProviderData *data;

  data = g_slice_new (GtkStyleProviderData);
  data->provider = g_object_ref (provider);
  data->priority = priority;

  return data;
}

static void
style_provider_data_free (GtkStyleProviderData *data)
{
  g_object_unref (data->provider);
  g_slice_free (GtkStyleProviderData, data);
}

static void
clear_property_cache (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  priv = context->priv;

  if (priv->property_cache)
    {
      guint i;

      for (i = 0; i < priv->property_cache->len; i++)
	{
	  PropertyValue *node = &g_array_index (priv->property_cache, PropertyValue, i);

	  g_param_spec_unref (node->pspec);
	  g_value_unset (&node->value);
	}

      g_array_free (priv->property_cache, TRUE);
      priv->property_cache = NULL;
    }
}

static void
gtk_style_context_finalize (GObject *object)
{
  GtkStyleContextPrivate *priv;
  GtkStyleContext *style_context;

  style_context = GTK_STYLE_CONTEXT (object);
  priv = style_context->priv;

  g_list_foreach (priv->providers, (GFunc) style_provider_data_free, NULL);
  g_list_free (priv->providers);

  clear_property_cache (GTK_STYLE_CONTEXT (object));

  g_slist_foreach (priv->regions, (GFunc) style_region_free, NULL);
  g_slist_free (priv->regions);

  g_slist_foreach (priv->icon_factories, (GFunc) g_object_unref, NULL);
  g_slist_free (priv->icon_factories);

  G_OBJECT_CLASS (gtk_style_context_parent_class)->finalize (object);
}

static void
gtk_style_context_impl_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkStyleContextPrivate *priv;
  GtkStyleContext *style_context;

  style_context = GTK_STYLE_CONTEXT (object);
  priv = style_context->priv;

  switch (prop_id)
    {
    case PROP_SCREEN:
      gtk_style_context_set_screen (style_context,
                                    g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_style_context_impl_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkStyleContextPrivate *priv;
  GtkStyleContext *style_context;

  style_context = GTK_STYLE_CONTEXT (object);
  priv = style_context->priv;

  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, priv->screen);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
rebuild_properties (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GList *list;

  priv = context->priv;
  list = priv->providers;

  gtk_style_set_clear (priv->store);

  while (list)
    {
      GtkStyleProviderData *data;
      GtkStyleSet *provider_style;

      data = list->data;
      list = list->next;

      provider_style = gtk_style_provider_get_style (data->provider,
                                                     priv->widget_path);

      if (provider_style)
        {
          gtk_style_set_merge (priv->store, provider_style, TRUE);
          g_object_unref (provider_style);
        }
    }

  gtk_style_set_get (priv->store, GTK_STATE_NORMAL,
                     "engine", &priv->theming_engine,
                     NULL);
}

static void
rebuild_icon_factories (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GList *providers;

  priv = context->priv;

  g_slist_foreach (priv->icon_factories, (GFunc) g_object_unref, NULL);
  g_slist_free (priv->icon_factories);
  priv->icon_factories = NULL;

  for (providers = priv->providers_last; providers; providers = providers->prev)
    {
      GtkIconFactory *factory;
      GtkStyleProviderData *data;

      data = providers->data;
      factory = gtk_style_provider_get_icon_factory (data->provider,
						     priv->widget_path);

      if (factory)
	priv->icon_factories = g_slist_prepend (priv->icon_factories, factory);
    }
}

void
gtk_style_context_add_provider (GtkStyleContext  *context,
                                GtkStyleProvider *provider,
                                guint             priority)
{
  GtkStyleContextPrivate *priv;
  GtkStyleProviderData *new_data;
  gboolean added = FALSE;
  GList *list;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  priv = context->priv;
  new_data = style_provider_data_new (provider, priority);
  list = priv->providers;

  while (list)
    {
      GtkStyleProviderData *data;

      data = list->data;

      /* Provider was already attached to the style
       * context, remove in order to add the new data
       */
      if (data->provider == provider)
        {
          GList *link;

          link = list;
          list = list->next;

          /* Remove and free link */
          priv->providers = g_list_remove_link (priv->providers, link);
          style_provider_data_free (link->data);
          g_list_free_1 (link);

          continue;
        }

      if (!added &&
          data->priority > priority)
        {
          priv->providers = g_list_insert_before (priv->providers, list, new_data);
          added = TRUE;
        }

      list = list->next;
    }

  if (!added)
    priv->providers = g_list_append (priv->providers, new_data);

  priv->providers_last = g_list_last (priv->providers);

  if (priv->widget_path)
    {
      rebuild_properties (context);
      clear_property_cache (context);
      rebuild_icon_factories (context);
    }
}

void
gtk_style_context_remove_provider (GtkStyleContext  *context,
                                   GtkStyleProvider *provider)
{
  GtkStyleContextPrivate *priv;
  gboolean removed = FALSE;
  GList *list;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  priv = context->priv;
  list = priv->providers;

  while (list)
    {
      GtkStyleProviderData *data;

      data = list->data;

      if (data->provider == provider)
        {
          priv->providers = g_list_remove_link (priv->providers, list);
          style_provider_data_free (list->data);
          g_list_free_1 (list);

          removed = TRUE;

          break;
        }

      list = list->next;
    }

  if (removed)
    {
      priv->providers_last = g_list_last (priv->providers);

      if (priv->widget_path)
        {
          rebuild_properties (context);
          clear_property_cache (context);
	  rebuild_icon_factories (context);
        }
    }
}

void
gtk_style_context_get_property (GtkStyleContext *context,
                                const gchar     *property,
                                GtkStateType     state,
                                GValue          *value)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (property != NULL);
  g_return_if_fail (state < GTK_STATE_LAST);
  g_return_if_fail (value != NULL);

  priv = context->priv;
  gtk_style_set_get_property (priv->store, property, state, value);
}

void
gtk_style_context_get_valist (GtkStyleContext *context,
                              GtkStateType     state,
                              va_list          args)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (state < GTK_STATE_LAST);

  priv = context->priv;
  gtk_style_set_get_valist (priv->store, state, args);
}

void
gtk_style_context_get (GtkStyleContext *context,
                       GtkStateType     state,
                       ...)
{
  GtkStyleContextPrivate *priv;
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (state < GTK_STATE_LAST);

  priv = context->priv;

  va_start (args, state);
  gtk_style_set_get_valist (priv->store, state, args);
  va_end (args);
}

void
gtk_style_context_set_state (GtkStyleContext *context,
                             GtkStateFlags    flags)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  priv->state_flags = flags;
}

GtkStateFlags
gtk_style_context_get_state (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  priv = context->priv;
  return priv->state_flags;
}

gboolean
gtk_style_context_is_state_set (GtkStyleContext *context,
                                GtkStateType     state)
{
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);

  priv = context->priv;

  switch (state)
    {
    case GTK_STATE_NORMAL:
      return priv->state_flags == 0;
    case GTK_STATE_ACTIVE:
      return priv->state_flags & GTK_STATE_FLAG_ACTIVE;
    case GTK_STATE_PRELIGHT:
      return priv->state_flags & GTK_STATE_FLAG_PRELIGHT;
    case GTK_STATE_SELECTED:
      return priv->state_flags & GTK_STATE_FLAG_SELECTED;
    case GTK_STATE_INSENSITIVE:
      return priv->state_flags & GTK_STATE_FLAG_INSENSITIVE;
    case GTK_STATE_INCONSISTENT:
      return priv->state_flags & GTK_STATE_FLAG_INCONSISTENT;
    case GTK_STATE_FOCUSED:
      return priv->state_flags & GTK_STATE_FLAG_FOCUSED;
    default:
      return FALSE;
    }
}

void
gtk_style_context_set_path (GtkStyleContext *context,
                            GtkWidgetPath   *path)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (path != NULL);

  priv = context->priv;

  if (priv->widget_path)
    {
      gtk_widget_path_free (priv->widget_path);
      priv->widget_path = NULL;
    }

  if (path)
    {
      priv->widget_path = gtk_widget_path_copy (path);
      rebuild_properties (context);
      clear_property_cache (context);
      rebuild_icon_factories (context);
    }
}

G_CONST_RETURN GtkWidgetPath *
gtk_style_context_get_path (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  priv = context->priv;
  return priv->widget_path;
}

void
gtk_style_context_save (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleRegion *region;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  g_assert (priv->regions != NULL);

  region = style_region_copy (priv->regions->data);
  priv->regions = g_slist_prepend (priv->regions, region);
}

void
gtk_style_context_restore (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleRegion *region;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  if (priv->regions)
    {
      region = priv->regions->data;
      priv->regions = g_slist_remove (priv->regions, region);
      style_region_free (region);
    }

  if (!priv->regions)
    {
      g_warning ("Unpaired gtk_style_context_restore() call");

      /* Create default region */
      region = style_region_new ();
      priv->regions = g_slist_prepend (priv->regions, region);
    }
}

static gboolean
style_class_find (GArray *array,
                  GQuark  class_quark,
                  guint  *position)
{
  guint min, max, mid;
  gboolean found = FALSE;

  if (position)
    *position = 0;

  if (!array || array->len == 0)
    return FALSE;

  min = 0;
  max = array->len - 1;

  do
    {
      GQuark item;

      mid = min + max / 2;
      item = g_array_index (array, GQuark, mid);

      if (class_quark == item)
        found = TRUE;
      else if (class_quark > item)
        min = mid + 1;
      else
        max = mid - 1;
    }
  while (!found && min < max);

  if (position)
    *position = mid;

  return found;
}

static gboolean
child_style_class_find (GArray *array,
                        GQuark  class_quark,
                        guint  *position)
{
  guint min, max, mid;
  gboolean found = FALSE;

  if (position)
    *position = 0;

  if (!array || array->len == 0)
    return FALSE;

  min = 0;
  max = array->len - 1;

  do
    {
      GtkChildClass *child_class;

      mid = min + max / 2;
      child_class = &g_array_index (array, GtkChildClass, mid);

      if (child_class->class_quark == class_quark)
        found = TRUE;
      else if (child_class->class_quark > class_quark)
        min = mid + 1;
      else
        max = mid - 1;
    }
  while (!found && min < max);

  if (position)
    *position = mid;

  return found;
}

void
gtk_style_context_set_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleRegion *region;
  GQuark class_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  priv = context->priv;
  class_quark = g_quark_from_string (class_name);

  g_assert (priv->regions != NULL);
  region = priv->regions->data;

  if (!style_class_find (region->style_classes, class_quark, &position))
    g_array_insert_val (region->style_classes, position, class_quark);
}

void
gtk_style_context_unset_class (GtkStyleContext *context,
                               const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleRegion *region;
  GQuark class_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return;

  priv = context->priv;

  g_assert (priv->regions != NULL);
  region = priv->regions->data;

  if (style_class_find (region->style_classes, class_quark, &position))
    g_array_remove_index (region->style_classes, position);
}

gboolean
gtk_style_context_has_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleRegion *region;
  GQuark class_quark;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (class_name != NULL, FALSE);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return FALSE;

  priv = context->priv;

  g_assert (priv->regions != NULL);
  region = priv->regions->data;

  if (style_class_find (region->style_classes, class_quark, NULL))
    return TRUE;

  return FALSE;
}

GList *
gtk_style_context_list_child_classes (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleRegion *region;
  GList *classes = NULL;
  guint i;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;

  g_assert (priv->regions != NULL);
  region = priv->regions->data;

  for (i = 0; i < region->child_style_classes->len; i++)
    {
      GtkChildClass *child_class;
      const gchar *class_name;

      child_class = &g_array_index (region->child_style_classes,
                                    GtkChildClass,
                                    i);

      class_name = g_quark_to_string (child_class->class_quark);
      classes = g_list_prepend (classes, (gchar *) class_name);
    }

  return classes;
}

void
gtk_style_context_set_child_class (GtkStyleContext    *context,
                                   const gchar        *class_name,
                                   GtkChildClassFlags  flags)
{
  GtkStyleContextPrivate *priv;
  GtkStyleRegion *region;
  GQuark class_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  priv = context->priv;
  class_quark = g_quark_from_string (class_name);

  g_assert (priv->regions != NULL);
  region = priv->regions->data;

  if (!child_style_class_find (region->child_style_classes, class_quark, &position))
    {
      GtkChildClass child_class;

      child_class.class_quark = class_quark;
      child_class.flags = flags;

      g_array_insert_val (region->child_style_classes, position, child_class);
    }
}

void
gtk_style_context_unset_child_class (GtkStyleContext    *context,
                                     const gchar        *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleRegion *region;
  GQuark class_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return;

  priv = context->priv;

  g_assert (priv->regions != NULL);
  region = priv->regions->data;

  if (child_style_class_find (region->child_style_classes, class_quark, &position))
    g_array_remove_index (region->child_style_classes, position);
}

gboolean
gtk_style_context_has_child_class (GtkStyleContext    *context,
                                   const gchar        *class_name,
                                   GtkChildClassFlags *flags_return)
{
  GtkStyleContextPrivate *priv;
  GtkStyleRegion *region;
  GQuark class_quark;
  guint position;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (class_name != NULL, FALSE);

  if (flags_return)
    *flags_return = 0;

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return FALSE;

  priv = context->priv;

  g_assert (priv->regions != NULL);
  region = priv->regions->data;

  if (child_style_class_find (region->child_style_classes, class_quark, &position))
    {
      if (flags_return)
        {
          GtkChildClass *child_class;

          child_class = &g_array_index (region->child_style_classes,
					GtkChildClass, position);

          *flags_return = child_class->flags;
        }
      return TRUE;
    }

  return FALSE;
}

static gint
style_property_values_cmp (gconstpointer bsearch_node1,
			   gconstpointer bsearch_node2)
{
  const PropertyValue *val1 = bsearch_node1;
  const PropertyValue *val2 = bsearch_node2;

  if (val1->widget_type == val2->widget_type)
    return val1->pspec < val2->pspec ? -1 : val1->pspec == val2->pspec ? 0 : 1;
  else
    return val1->widget_type < val2->widget_type ? -1 : 1;
}

const GValue *
_gtk_style_context_peek_style_property (GtkStyleContext *context,
                                        GType            widget_type,
                                        GParamSpec      *pspec)
{
  GtkStyleContextPrivate *priv;
  PropertyValue *pcache, key = { 0 };
  GList *list;
  guint i;

  priv = context->priv;

  key.widget_type = widget_type;
  key.pspec = pspec;

  /* need value cache array */
  if (!priv->property_cache)
    priv->property_cache = g_array_new (FALSE, FALSE, sizeof (PropertyValue));
  else
    {
      pcache = bsearch (&key,
                        priv->property_cache->data, priv->property_cache->len,
                        sizeof (PropertyValue), style_property_values_cmp);
      if (pcache)
        return &pcache->value;
    }

  i = 0;
  while (i < priv->property_cache->len &&
	 style_property_values_cmp (&key, &g_array_index (priv->property_cache, PropertyValue, i)) >= 0)
    i++;

  g_array_insert_val (priv->property_cache, i, key);
  pcache = &g_array_index (priv->property_cache, PropertyValue, i);

  /* cache miss, initialize value type, then set contents */
  g_param_spec_ref (pcache->pspec);
  g_value_init (&pcache->value, G_PARAM_SPEC_VALUE_TYPE (pspec));

  if (priv->widget_path)
    {
      for (list = priv->providers_last; list; list = list->prev)
        {
          GtkStyleProviderData *data;

          data = list->data;

          if (gtk_style_provider_get_style_property (data->provider, priv->widget_path,
                                                     pspec->name, &pcache->value))
            return &pcache->value;
        }
    }

  /* not supplied by any provider, revert to default */
  g_param_value_set_default (pspec, &pcache->value);

  return &pcache->value;
}

void
gtk_style_context_get_style_property (GtkStyleContext *context,
                                      const gchar     *property_name,
                                      GValue          *value)
{
  GtkStyleContextPrivate *priv;
  GtkWidgetClass *widget_class;
  GParamSpec *pspec;
  const GValue *peek_value;
  GType widget_type;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (value != NULL);

  priv = context->priv;

  if (!priv->widget_path)
    return;

  widget_type = gtk_widget_path_get_widget_type (priv->widget_path);

  widget_class = g_type_class_ref (widget_type);
  pspec = gtk_widget_class_find_style_property (widget_class, property_name);
  g_type_class_unref (widget_class);

  if (!pspec)
    {
      g_warning ("%s: widget class `%s' has no style property named `%s'",
                 G_STRLOC,
                 g_type_name (widget_type),
                 property_name);
      return;
    }

  peek_value = _gtk_style_context_peek_style_property (context,
                                                       widget_type,
                                                       pspec);

  if (G_VALUE_TYPE (value) == G_VALUE_TYPE (peek_value))
    g_value_copy (peek_value, value);
  else if (g_value_type_transformable (G_VALUE_TYPE (peek_value), G_VALUE_TYPE (value)))
    g_value_transform (peek_value, value);
  else
    g_warning ("can't retrieve style property `%s' of type `%s' as value of type `%s'",
               pspec->name,
               G_VALUE_TYPE_NAME (peek_value),
               G_VALUE_TYPE_NAME (value));
}

GtkIconSet *
gtk_style_context_lookup_icon_set (GtkStyleContext *context,
				   const gchar     *stock_id)
{
  GtkStyleContextPrivate *priv;
  GSList *list;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);

  priv = context->priv;

  for (list = priv->icon_factories; list; list = list->next)
    {
      GtkIconFactory *factory;
      GtkIconSet *icon_set;

      factory = list->data;
      icon_set = gtk_icon_factory_lookup (factory, stock_id);

      if (icon_set)
	return icon_set;
    }

  return gtk_icon_factory_lookup_default (stock_id);
}

void
gtk_style_context_set_screen (GtkStyleContext *context,
                              GdkScreen       *screen)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  priv->screen = screen;

  g_object_notify (G_OBJECT (context), "screen");
}

GdkScreen *
gtk_style_context_get_screen (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;
  return priv->screen;
}

/* Paint methods */
void
gtk_render_check (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_check (priv->theming_engine, cr,
                              x, y, width, height);
}

void
gtk_render_option (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_option (priv->theming_engine, cr,
                               x, y, width, height);
}

void
gtk_render_arrow (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          angle,
                  gdouble          x,
                  gdouble          y,
                  gdouble          size)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_arrow (priv->theming_engine, cr,
                              angle, x, y, size);
}

void
gtk_render_background (GtkStyleContext *context,
                       cairo_t         *cr,
                       gdouble          x,
                       gdouble          y,
                       gdouble          width,
                       gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_background (priv->theming_engine, cr, x, y, width, height);
}

void
gtk_render_frame (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_frame (priv->theming_engine, cr, x, y, width, height);
}

void
gtk_render_expander (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          x,
                     gdouble          y,
                     gdouble          width,
                     gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_expander (priv->theming_engine, cr, x, y, width, height);
}

void
gtk_render_focus (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_focus (priv->theming_engine, cr, x, y, width, height);
}

void
gtk_render_layout (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   PangoLayout     *layout)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_layout (priv->theming_engine, cr, x, y, layout);
}

void
gtk_render_line (GtkStyleContext *context,
                 cairo_t         *cr,
                 gdouble          x0,
                 gdouble          y0,
                 gdouble          x1,
                 gdouble          y1)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_line (priv->theming_engine, cr, x0, y0, x1, y1);
}

void
gtk_render_slider (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height,
                   GtkOrientation   orientation)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_slider (priv->theming_engine, cr, x, y, width, height, orientation);
}

void
gtk_render_frame_gap (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height,
                      GtkPositionType  gap_side,
                      gdouble          xy0_gap,
                      gdouble          xy1_gap)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_frame_gap (priv->theming_engine, cr,
                                  x, y, width, height, gap_side,
                                  xy0_gap, xy1_gap);
}

void
gtk_render_extension (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height,
                      GtkPositionType  gap_side)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_extension (priv->theming_engine, cr, x, y, width, height, gap_side);
}

void
gtk_render_handle (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height,
                   GtkOrientation   orientation)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_handle (priv->theming_engine, cr, x, y, width, height, orientation);
}

#define __GTK_STYLE_CONTEXT_C__
#include "gtkaliasdef.c"
