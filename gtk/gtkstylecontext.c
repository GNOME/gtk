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
#include <gobject/gvaluecollector.h>

#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"
#include "gtkthemingengine.h"
#include "gtkintl.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gtkanimationdescription.h"
#include "gtktimeline.h"

typedef struct GtkStyleContextPrivate GtkStyleContextPrivate;
typedef struct GtkStyleProviderData GtkStyleProviderData;
typedef struct GtkStyleInfo GtkStyleInfo;
typedef struct GtkRegion GtkRegion;
typedef struct PropertyValue PropertyValue;
typedef struct AnimationInfo AnimationInfo;

struct GtkRegion
{
  GQuark class_quark;
  GtkRegionFlags flags;
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

struct GtkStyleInfo
{
  GArray *style_classes;
  GArray *regions;
  GtkJunctionSides junction_sides;
};

struct AnimationInfo
{
  GtkTimeline *timeline;

  gpointer region_id;
  GdkWindow *window;
  GtkStateType state;
  gboolean target_value;

  cairo_region_t *invalidation_region;
  GArray *rectangles;
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
  GSList *info_stack;

  GSList *animation_regions;
  GSList *animations;

  guint animations_invalidated : 1;
  guint invalidating_context : 1;

  GtkThemingEngine *theming_engine;

  GtkTextDirection direction;
};

enum {
  PROP_0,
  PROP_SCREEN,
  PROP_DIRECTION
};

enum {
  CHANGED,
  LAST_SIGNAL
};

guint signals[LAST_SIGNAL] = { 0 };

static GQuark provider_list_quark = 0;

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

  signals[CHANGED] =
    g_signal_new (I_("changed"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkStyleContextClass, changed),
		  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
				   PROP_SCREEN,
				   g_param_spec_object ("screen",
                                                        P_("Screen"),
                                                        P_("The associated GdkScreen"),
                                                        GDK_TYPE_SCREEN,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_DIRECTION,
				   g_param_spec_enum ("direction",
                                                      P_("Direction"),
                                                      P_("Text direction"),
                                                      GTK_TYPE_TEXT_DIRECTION,
                                                      GTK_TEXT_DIR_LTR,
                                                      GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkStyleContextPrivate));
}

static GtkStyleInfo *
style_info_new (void)
{
  GtkStyleInfo *info;

  info = g_slice_new0 (GtkStyleInfo);
  info->style_classes = g_array_new (FALSE, FALSE, sizeof (GQuark));
  info->regions = g_array_new (FALSE, FALSE, sizeof (GtkRegion));

  return info;
}

static void
style_info_free (GtkStyleInfo *info)
{
  g_array_free (info->style_classes, TRUE);
  g_array_free (info->regions, TRUE);
  g_slice_free (GtkStyleInfo, info);
}

static GtkStyleInfo *
style_info_copy (const GtkStyleInfo *info)
{
  GtkStyleInfo *copy;

  copy = style_info_new ();
  g_array_insert_vals (copy->style_classes, 0,
                       info->style_classes->data,
                       info->style_classes->len);

  g_array_insert_vals (copy->regions, 0,
                       info->regions->data,
                       info->regions->len);

  copy->junction_sides = info->junction_sides;

  return copy;
}

static void
gtk_style_context_init (GtkStyleContext *style_context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;

  priv = style_context->priv = G_TYPE_INSTANCE_GET_PRIVATE (style_context,
                                                            GTK_TYPE_STYLE_CONTEXT,
                                                            GtkStyleContextPrivate);

  priv->store = gtk_style_set_new ();
  priv->theming_engine = g_object_ref ((gpointer) gtk_theming_engine_load (NULL));

  priv->direction = GTK_TEXT_DIR_RTL;

  /* Create default info store */
  info = style_info_new ();
  priv->info_stack = g_slist_prepend (priv->info_stack, info);
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
animation_info_free (AnimationInfo *info)
{
  g_object_unref (info->timeline);
  g_object_unref (info->window);

  if (info->invalidation_region)
    cairo_region_destroy (info->invalidation_region);

  g_array_free (info->rectangles, TRUE);
  g_slice_free (AnimationInfo, info);
}

static void
timeline_frame_cb (GtkTimeline *timeline,
                   gdouble      progress,
                   gpointer     user_data)
{
  AnimationInfo *info;

  info = user_data;

  if (info->invalidation_region &&
      !cairo_region_is_empty (info->invalidation_region))
    gdk_window_invalidate_region (info->window, info->invalidation_region, TRUE);
  else
    gdk_window_invalidate_rect (info->window, NULL, TRUE);
}

static void
timeline_finished_cb (GtkTimeline *timeline,
                      gpointer     user_data)
{
  GtkStyleContextPrivate *priv;
  GtkStyleContext *context;
  AnimationInfo *info;
  GSList *l;

  context = user_data;
  priv = context->priv;

  for (l = priv->animations; l; l = l->next)
    {
      info = l->data;

      if (info->timeline == timeline)
        {
          priv->animations = g_slist_delete_link (priv->animations, l);

          /* Invalidate one last time the area, so the final content is painted */
          if (info->invalidation_region &&
              !cairo_region_is_empty (info->invalidation_region))
            gdk_window_invalidate_region (info->window, info->invalidation_region, TRUE);
          else
            gdk_window_invalidate_rect (info->window, NULL, TRUE);

          animation_info_free (info);
          break;
        }
    }
}

static AnimationInfo *
animation_info_new (GtkStyleContext         *context,
                    gdouble                  duration,
                    GtkTimelineProgressType  progress_type,
                    GtkStateType             state,
                    gboolean                 target_value,
                    GdkWindow               *window)
{
  AnimationInfo *info;

  info = g_slice_new0 (AnimationInfo);

  info->rectangles = g_array_new (FALSE, FALSE, sizeof (cairo_rectangle_int_t));
  info->timeline = gtk_timeline_new (duration);
  info->window = g_object_ref (window);
  info->state = state;
  info->target_value = target_value;

  gtk_timeline_set_progress_type (info->timeline, progress_type);

  if (!target_value)
    {
      gtk_timeline_set_direction (info->timeline, GTK_TIMELINE_DIRECTION_BACKWARD);
      gtk_timeline_rewind (info->timeline);
    }

  g_signal_connect (info->timeline, "frame",
                    G_CALLBACK (timeline_frame_cb), info);
  g_signal_connect (info->timeline, "finished",
                    G_CALLBACK (timeline_finished_cb), context);

  gtk_timeline_start (info->timeline);

  return info;
}

static AnimationInfo *
animation_info_lookup (GtkStyleContext *context,
                       gpointer         region_id,
                       GtkStateType     state)
{
  GtkStyleContextPrivate *priv;
  GSList *l;

  priv = context->priv;

  for (l = priv->animations; l; l = l->next)
    {
      AnimationInfo *info;

      info = l->data;

      if (info->state == state &&
          info->region_id == region_id)
        return info;
    }

  return NULL;
}

static void
gtk_style_context_finalize (GObject *object)
{
  GtkStyleContextPrivate *priv;
  GtkStyleContext *style_context;
  GSList *l;

  style_context = GTK_STYLE_CONTEXT (object);
  priv = style_context->priv;

  if (priv->widget_path)
    gtk_widget_path_free (priv->widget_path);

  g_object_unref (priv->store);

  g_list_foreach (priv->providers, (GFunc) style_provider_data_free, NULL);
  g_list_free (priv->providers);

  clear_property_cache (GTK_STYLE_CONTEXT (object));

  g_slist_foreach (priv->info_stack, (GFunc) style_info_free, NULL);
  g_slist_free (priv->info_stack);

  g_slist_foreach (priv->icon_factories, (GFunc) g_object_unref, NULL);
  g_slist_free (priv->icon_factories);

  g_slist_free (priv->animation_regions);

  for (l = priv->animations; l; l = l->next)
    animation_info_free ((AnimationInfo *) l->data);

  g_slist_free (priv->animations);

  if (priv->theming_engine)
    g_object_unref (priv->theming_engine);

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
    case PROP_DIRECTION:
      gtk_style_context_set_direction (style_context,
                                       g_value_get_enum (value));
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
    case PROP_DIRECTION:
      g_value_set_enum (value, priv->direction);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

GList *
find_next_candidate (GList *local,
                     GList *global)
{
  if (local && global)
    {
      GtkStyleProviderData *local_data, *global_data;

      local_data = local->data;
      global_data = global->data;

      if (local_data->priority >= global_data->priority)
        return local;
      else
        return global;
    }
  else if (local)
    return local;
  else if (global)
    return global;

  return NULL;
}

static void
rebuild_properties (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GList *elem, *list, *global_list = NULL;

  priv = context->priv;
  list = priv->providers;

  if (priv->screen)
    global_list = g_object_get_qdata (G_OBJECT (priv->screen), provider_list_quark);

  gtk_style_set_clear (priv->store);

  while ((elem = find_next_candidate (list, global_list)) != NULL)
    {
      GtkStyleProviderData *data;
      GtkStyleSet *provider_style;

      data = elem->data;

      if (elem == list)
        list = list->next;
      else
        global_list = global_list->next;

      provider_style = gtk_style_provider_get_style (data->provider,
                                                     priv->widget_path);

      if (provider_style)
        {
          gtk_style_set_merge (priv->store, provider_style, TRUE);
          g_object_unref (provider_style);
        }
    }

  if (priv->theming_engine)
    g_object_unref (priv->theming_engine);

  gtk_style_set_get (priv->store, 0,
                     "engine", &priv->theming_engine,
                     NULL);
}

static void
rebuild_icon_factories (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GList *elem, *list, *global_list = NULL;

  priv = context->priv;

  g_slist_foreach (priv->icon_factories, (GFunc) g_object_unref, NULL);
  g_slist_free (priv->icon_factories);
  priv->icon_factories = NULL;

  list = priv->providers_last;

  if (priv->screen)
    {
      global_list = g_object_get_qdata (G_OBJECT (priv->screen), provider_list_quark);
      global_list = g_list_last (global_list);
    }

  while ((elem = find_next_candidate (list, global_list)) != NULL)
    {
      GtkIconFactory *factory;
      GtkStyleProviderData *data;

      data = elem->data;

      if (elem == list)
        list = list->prev;
      else
        global_list = global_list->prev;

      factory = gtk_style_provider_get_icon_factory (data->provider,
						     priv->widget_path);

      if (factory)
	priv->icon_factories = g_slist_prepend (priv->icon_factories, factory);
    }
}

static void
style_provider_add (GList            **list,
                    GtkStyleProvider  *provider,
                    guint              priority)
{
  GtkStyleProviderData *new_data;
  gboolean added = FALSE;
  GList *l = *list;

  new_data = style_provider_data_new (provider, priority);

  while (l)
    {
      GtkStyleProviderData *data;

      data = l->data;

      /* Provider was already attached to the style
       * context, remove in order to add the new data
       */
      if (data->provider == provider)
        {
          GList *link;

          link = l;
          l = l->next;

          /* Remove and free link */
          *list = g_list_remove_link (*list, link);
          style_provider_data_free (link->data);
          g_list_free_1 (link);

          continue;
        }

      if (!added &&
          data->priority > priority)
        {
          *list = g_list_insert_before (*list, l, new_data);
          added = TRUE;
        }

      l = l->next;
    }

  if (!added)
    *list = g_list_append (*list, new_data);
}

static gboolean
style_provider_remove (GList            **list,
                       GtkStyleProvider  *provider)
{
  GList *l = *list;

  while (l)
    {
      GtkStyleProviderData *data;

      data = l->data;

      if (data->provider == provider)
        {
          *list = g_list_remove_link (*list, l);
          style_provider_data_free (l->data);
          g_list_free_1 (l);

          return TRUE;
        }

      l = l->next;
    }

  return FALSE;
}

void
gtk_style_context_add_provider (GtkStyleContext  *context,
                                GtkStyleProvider *provider,
                                guint             priority)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  priv = context->priv;
  style_provider_add (&priv->providers, provider, priority);
  priv->providers_last = g_list_last (priv->providers);

  gtk_style_context_invalidate (context);
}

void
gtk_style_context_remove_provider (GtkStyleContext  *context,
                                   GtkStyleProvider *provider)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  priv = context->priv;

  if (style_provider_remove (&priv->providers, provider))
    {
      priv->providers_last = g_list_last (priv->providers);

      gtk_style_context_invalidate (context);
    }
}

void
gtk_style_context_reset_widgets (GdkScreen *screen)
{
  GList *list, *toplevels;

  toplevels = gtk_window_list_toplevels ();
  g_list_foreach (toplevels, (GFunc) g_object_ref, NULL);

  for (list = toplevels; list; list = list->next)
    {
      if (gtk_widget_get_screen (list->data) == screen)
        gtk_widget_reset_style (list->data);

      g_object_unref (list->data);
    }

  g_list_free (toplevels);
}

void
gtk_style_context_add_provider_for_screen (GdkScreen        *screen,
                                           GtkStyleProvider *provider,
                                           guint             priority)
{
  GList *providers, *list;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  if (G_UNLIKELY (!provider_list_quark))
    provider_list_quark = g_quark_from_static_string ("gtk-provider-list-quark");

  list = providers = g_object_get_qdata (G_OBJECT (screen), provider_list_quark);
  style_provider_add (&list, provider, priority);

  if (list != providers)
    g_object_set_qdata (G_OBJECT (screen), provider_list_quark, list);

  gtk_style_context_reset_widgets (screen);
}

void
gtk_style_context_remove_provider_for_screen (GdkScreen        *screen,
                                              GtkStyleProvider *provider)
{
  GList *providers, *list;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  if (G_UNLIKELY (!provider_list_quark))
    return;

  list = providers = g_object_get_qdata (G_OBJECT (screen), provider_list_quark);

  if (style_provider_remove (&list, provider))
    {
      if (list != providers)
        g_object_set_qdata (G_OBJECT (screen), provider_list_quark, list);

      gtk_style_context_reset_widgets (screen);
    }
}

void
gtk_style_context_get_property (GtkStyleContext *context,
                                const gchar     *property,
                                GtkStateFlags    state,
                                GValue          *value)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (property != NULL);
  g_return_if_fail (value != NULL);

  priv = context->priv;
  gtk_style_set_get_property (priv->store, property, state, value);
}

void
gtk_style_context_get_valist (GtkStyleContext *context,
                              GtkStateFlags    state,
                              va_list          args)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  gtk_style_set_get_valist (priv->store, state, args);
}

void
gtk_style_context_get (GtkStyleContext *context,
                       GtkStateFlags    state,
                       ...)
{
  GtkStyleContextPrivate *priv;
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

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

static gboolean
context_has_animatable_region (GtkStyleContext *context,
                               gpointer         region_id)
{
  GtkStyleContextPrivate *priv;
  GSList *r;

  /* NULL region_id means everything
   * rendered through the style context
   */
  if (!region_id)
    return TRUE;

  priv = context->priv;

  for (r = priv->animation_regions; r; r = r->next)
    {
      if (r->data == region_id)
        return TRUE;
    }

  return FALSE;
}

gboolean
gtk_style_context_is_state_set (GtkStyleContext *context,
                                GtkStateType     state,
                                gdouble         *progress)
{
  GtkStyleContextPrivate *priv;
  gboolean state_set;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);

  priv = context->priv;

  switch (state)
    {
    case GTK_STATE_NORMAL:
      state_set = (priv->state_flags == 0);
      break;
    case GTK_STATE_ACTIVE:
      state_set = (priv->state_flags & GTK_STATE_FLAG_ACTIVE);
      break;
    case GTK_STATE_PRELIGHT:
      state_set = (priv->state_flags & GTK_STATE_FLAG_PRELIGHT);
      break;
    case GTK_STATE_SELECTED:
      state_set = (priv->state_flags & GTK_STATE_FLAG_SELECTED);
      break;
    case GTK_STATE_INSENSITIVE:
      state_set = (priv->state_flags & GTK_STATE_FLAG_INSENSITIVE);
      break;
    case GTK_STATE_INCONSISTENT:
      state_set = (priv->state_flags & GTK_STATE_FLAG_INCONSISTENT);
      break;
    case GTK_STATE_FOCUSED:
      state_set = (priv->state_flags & GTK_STATE_FLAG_FOCUSED);
      break;
    default:
      g_assert_not_reached ();
    }

  if (progress)
    {
      AnimationInfo *info;
      GSList *l;

      *progress = (state_set) ? 1 : 0;

      for (l = priv->animations; l; l = l->next)
        {
          info = l->data;

          if (info->state == state &&
              context_has_animatable_region (context, info->region_id))
            {
              *progress = gtk_timeline_get_progress (info->timeline);
              break;
            }
        }
    }

  return state_set;
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
      gtk_style_context_invalidate (context);
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
  GtkStyleInfo *info;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  g_assert (priv->info_stack != NULL);

  info = style_info_copy (priv->info_stack->data);
  priv->info_stack = g_slist_prepend (priv->info_stack, info);
}

void
gtk_style_context_restore (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  if (priv->info_stack)
    {
      info = priv->info_stack->data;
      priv->info_stack = g_slist_remove (priv->info_stack, info);
      style_info_free (info);
    }

  if (!priv->info_stack)
    {
      g_warning ("Unpaired gtk_style_context_restore() call");

      /* Create default region */
      info = style_info_new ();
      priv->info_stack = g_slist_prepend (priv->info_stack, info);
    }

  if (priv->widget_path)
    {
      guint i;

      info = priv->info_stack->data;

      /* Update widget path regions */
      gtk_widget_path_iter_clear_regions (priv->widget_path, 0);

      for (i = 0; i < info->regions->len; i++)
        {
          GtkRegion *region;

          region = &g_array_index (info->regions, GtkRegion, i);
          gtk_widget_path_iter_add_region (priv->widget_path, 0,
                                           g_quark_to_string (region->class_quark),
                                           region->flags);
        }

      /* Update widget path classes */
      gtk_widget_path_iter_clear_classes (priv->widget_path, 0);

      for (i = 0; i < info->style_classes->len; i++)
        {
          GQuark quark;

          quark = g_array_index (info->style_classes, GQuark, i);
          gtk_widget_path_iter_add_class (priv->widget_path, 0,
                                          g_quark_to_string (quark));
        }
    }
}

static gboolean
style_class_find (GArray *array,
                  GQuark  class_quark,
                  guint  *position)
{
  gint min, max, mid;
  gboolean found = FALSE;
  guint pos;

  if (position)
    *position = 0;

  if (!array || array->len == 0)
    return FALSE;

  min = 0;
  max = array->len - 1;

  do
    {
      GQuark item;

      mid = (min + max) / 2;
      item = g_array_index (array, GQuark, mid);

      if (class_quark == item)
        {
          found = TRUE;
          pos = mid;
        }
      else if (class_quark > item)
        min = pos = mid + 1;
      else
        {
          max = mid - 1;
          pos = mid;
        }
    }
  while (!found && min <= max);

  if (position)
    *position = pos;

  return found;
}

static gboolean
region_find (GArray *array,
             GQuark  class_quark,
             guint  *position)
{
  gint min, max, mid;
  gboolean found = FALSE;
  guint pos;

  if (position)
    *position = 0;

  if (!array || array->len == 0)
    return FALSE;

  min = 0;
  max = array->len - 1;

  do
    {
      GtkRegion *region;

      mid = (min + max) / 2;
      region = &g_array_index (array, GtkRegion, mid);

      if (region->class_quark == class_quark)
        {
          found = TRUE;
          pos = mid;
        }
      else if (region->class_quark > class_quark)
        min = pos = mid + 1;
      else
        {
          max = mid - 1;
          pos = mid;
        }
    }
  while (!found && min <= max);

  if (position)
    *position = pos;

  return found;
}

void
gtk_style_context_set_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark class_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  priv = context->priv;
  class_quark = g_quark_from_string (class_name);

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  if (!style_class_find (info->style_classes, class_quark, &position))
    {
      g_array_insert_val (info->style_classes, position, class_quark);

      if (priv->widget_path)
        {
          gtk_widget_path_iter_add_class (priv->widget_path, 0, class_name);
          gtk_style_context_invalidate (context);
        }
    }
}

void
gtk_style_context_unset_class (GtkStyleContext *context,
                               const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark class_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return;

  priv = context->priv;

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  if (style_class_find (info->style_classes, class_quark, &position))
    {
      g_array_remove_index (info->style_classes, position);

      if (priv->widget_path)
        {
          gtk_widget_path_iter_remove_class (priv->widget_path, 0, class_name);
          gtk_style_context_invalidate (context);
        }
    }
}

gboolean
gtk_style_context_has_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark class_quark;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (class_name != NULL, FALSE);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return FALSE;

  priv = context->priv;

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  if (style_class_find (info->style_classes, class_quark, NULL))
    return TRUE;

  return FALSE;
}

GList *
gtk_style_context_list_classes (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GList *classes = NULL;
  guint i;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  for (i = 0; i < info->style_classes->len; i++)
    {
      GQuark quark;

      quark = g_array_index (info->style_classes, GQuark, i);
      classes = g_list_prepend (classes, (gchar *) g_quark_to_string (quark));
    }

  return classes;
}

GList *
gtk_style_context_list_regions (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GList *classes = NULL;
  guint i;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  for (i = 0; i < info->regions->len; i++)
    {
      GtkRegion *region;
      const gchar *class_name;

      region = &g_array_index (info->regions, GtkRegion, i);

      class_name = g_quark_to_string (region->class_quark);
      classes = g_list_prepend (classes, (gchar *) class_name);
    }

  return classes;
}

void
gtk_style_context_set_region (GtkStyleContext *context,
                              const gchar     *class_name,
                              GtkRegionFlags   flags)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark class_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  priv = context->priv;
  class_quark = g_quark_from_string (class_name);

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  if (!region_find (info->regions, class_quark, &position))
    {
      GtkRegion region;

      region.class_quark = class_quark;
      region.flags = flags;

      g_array_insert_val (info->regions, position, region);

      if (priv->widget_path)
        {
          gtk_widget_path_iter_add_region (priv->widget_path, 0, class_name, flags);
          gtk_style_context_invalidate (context);
        }
    }
}

void
gtk_style_context_unset_region (GtkStyleContext    *context,
                                const gchar        *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark class_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return;

  priv = context->priv;

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  if (region_find (info->regions, class_quark, &position))
    {
      g_array_remove_index (info->regions, position);

      if (priv->widget_path)
        {
          gtk_widget_path_iter_remove_region (priv->widget_path, 0, class_name);
          gtk_style_context_invalidate (context);
        }
    }
}

gboolean
gtk_style_context_has_region (GtkStyleContext *context,
                              const gchar     *class_name,
                              GtkRegionFlags  *flags_return)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
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

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  if (region_find (info->regions, class_quark, &position))
    {
      if (flags_return)
        {
          GtkRegion *region;

          region = &g_array_index (info->regions, GtkRegion, position);
          *flags_return = region->flags;
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

void
gtk_style_context_get_style_valist (GtkStyleContext *context,
                                    va_list          args)
{
  GtkStyleContextPrivate *priv;
  const gchar *prop_name;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  prop_name = va_arg (args, const gchar *);
  priv = context->priv;

  if (!priv->widget_path)
    return;

  while (prop_name)
    {
      GtkWidgetClass *widget_class;
      GParamSpec *pspec;
      const GValue *peek_value;
      GType widget_type;
      gchar *error;

      widget_type = gtk_widget_path_get_widget_type (priv->widget_path);

      widget_class = g_type_class_ref (widget_type);
      pspec = gtk_widget_class_find_style_property (widget_class, prop_name);
      g_type_class_unref (widget_class);

      if (!pspec)
        {
          g_warning ("%s: widget class `%s' has no style property named `%s'",
                     G_STRLOC,
                     g_type_name (widget_type),
                     prop_name);
          continue;
        }

      peek_value = _gtk_style_context_peek_style_property (context,
                                                           widget_type,
                                                           pspec);

      G_VALUE_LCOPY (peek_value, args, 0, &error);

      if (error)
        {
          g_warning ("can't retrieve style property `%s' of type `%s': %s",
                     pspec->name,
                     G_VALUE_TYPE_NAME (peek_value),
                     error);
          g_free (error);
        }

      prop_name = va_arg (args, const gchar *);
    }
}

void
gtk_style_context_get_style (GtkStyleContext *context,
                             ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  va_start (args, context);
  gtk_style_context_get_style_valist (context, args);
  va_end (args);
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

  gtk_style_context_invalidate (context);
}

GdkScreen *
gtk_style_context_get_screen (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;
  return priv->screen;
}

void
gtk_style_context_set_direction (GtkStyleContext  *context,
                                 GtkTextDirection  direction)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  priv->direction = direction;

  g_object_notify (G_OBJECT (context), "direction");
}

GtkTextDirection
gtk_style_context_get_direction (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), GTK_TEXT_DIR_LTR);

  priv = context->priv;
  return priv->direction;
}

void
gtk_style_context_set_junction_sides (GtkStyleContext  *context,
				      GtkJunctionSides  sides)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  info = priv->info_stack->data;
  info->junction_sides = sides;
}

GtkJunctionSides
gtk_style_context_get_junction_sides (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  priv = context->priv;
  info = priv->info_stack->data;
  return info->junction_sides;
}

gboolean
gtk_style_context_lookup_color (GtkStyleContext *context,
                                const gchar     *color_name,
                                GdkColor        *color)
{
  GtkStyleContextPrivate *priv;
  GtkSymbolicColor *sym_color;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (color_name != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  priv = context->priv;
  sym_color = gtk_style_set_lookup_color (priv->store, color_name);

  if (!sym_color)
    return FALSE;

  return gtk_symbolic_color_resolve (sym_color, priv->store, color);
}

void
gtk_style_context_notify_state_change (GtkStyleContext *context,
                                       GdkWindow       *window,
                                       gpointer         region_id,
                                       GtkStateType     state,
                                       gboolean         state_value)
{
  GtkStyleContextPrivate *priv;
  GtkAnimationDescription *desc;
  AnimationInfo *info;
  GtkStateFlags flags;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (state < GTK_STATE_LAST);

  state_value = (state_value == TRUE);
  priv = context->priv;

  switch (state)
    {
    case GTK_STATE_ACTIVE:
      flags = GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags = GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags = GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags = GTK_STATE_FLAG_INSENSITIVE;
      break;
    case GTK_STATE_INCONSISTENT:
      flags = GTK_STATE_FLAG_INCONSISTENT;
      break;
    case GTK_STATE_FOCUSED:
      flags = GTK_STATE_FLAG_FOCUSED;
      break;
    case GTK_STATE_NORMAL:
    default:
      flags = 0;
      break;
    }

  /* Find out if there is any animation description for the given
   * state, it will fallback to the normal state as well if necessary.
   */
  gtk_style_set_get (priv->store, flags,
                     "transition", &desc,
                     NULL);

  if (!desc)
    return;

  if (gtk_animation_description_get_duration (desc) == 0)
    {
      gtk_animation_description_unref (desc);
      return;
    }

  info = animation_info_lookup (context, region_id, state);

  if (info)
    {
      /* Reverse the animation if target values are the opposite */
      if (info->target_value != state_value)
        {
          if (gtk_timeline_get_direction (info->timeline) == GTK_TIMELINE_DIRECTION_FORWARD)
            gtk_timeline_set_direction (info->timeline, GTK_TIMELINE_DIRECTION_BACKWARD);
          else
            gtk_timeline_set_direction (info->timeline, GTK_TIMELINE_DIRECTION_FORWARD);

          info->target_value = state_value;
        }
    }
  else
    {
      info = animation_info_new (context,
                                 gtk_animation_description_get_duration (desc),
                                 gtk_animation_description_get_progress_type (desc),
                                 state, state_value, window);

      priv->animations = g_slist_prepend (priv->animations, info);
      priv->animations_invalidated = TRUE;
    }

  gtk_animation_description_unref (desc);
}

void
gtk_style_context_push_animatable_region (GtkStyleContext *context,
                                          gpointer         region_id)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (region_id != NULL);

  priv = context->priv;
  priv->animation_regions = g_slist_prepend (priv->animation_regions, region_id);
}

void
gtk_style_context_pop_animatable_region (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  priv->animation_regions = g_slist_delete_link (priv->animation_regions,
                                                 priv->animation_regions);
}

void
_gtk_style_context_invalidate_animation_areas (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GSList *l;

  priv = context->priv;

  for (l = priv->animations; l; l = l->next)
    {
      AnimationInfo *info;

      info = l->data;

      /* A NULL invalidation region means it has to be recreated on
       * the next expose event, this happens usually after a widget
       * allocation change, so the next expose after it will update
       * the invalidation region.
       */
      if (info->invalidation_region)
        {
          cairo_region_destroy (info->invalidation_region);
          info->invalidation_region = NULL;
        }
    }

  priv->animations_invalidated = TRUE;
}

void
_gtk_style_context_coalesce_animation_areas (GtkStyleContext *context,
                                             gint             rel_x,
                                             gint             rel_y)
{
  GtkStyleContextPrivate *priv;
  GSList *l;

  priv = context->priv;

  if (!priv->animations_invalidated)
    return;

  for (l = priv->animations; l; l = l->next)
    {
      AnimationInfo *info;
      guint i;

      info = l->data;

      if (info->invalidation_region)
        continue;

      /* FIXME: If this happens there's not much
       * point in keeping the animation running.
       */
      if (info->rectangles->len == 0)
        continue;

      info->invalidation_region = cairo_region_create ();

      for (i = 0; i <info->rectangles->len; i++)
        {
          cairo_rectangle_int_t *rect;

          rect = &g_array_index (info->rectangles, cairo_rectangle_int_t, i);
          rect->x += rel_x;
          rect->y += rel_y;

          cairo_region_union_rectangle (info->invalidation_region, rect);
        }

      g_array_remove_range (info->rectangles, 0, info->rectangles->len);
    }

  priv->animations_invalidated = FALSE;
}

static void
store_animation_region (GtkStyleContext *context,
                        gdouble          x,
                        gdouble          y,
                        gdouble          width,
                        gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GSList *l;

  priv = context->priv;

  if (!priv->animations_invalidated)
    return;

  for (l = priv->animations; l; l = l->next)
    {
      AnimationInfo *info;

      info = l->data;

      /* The animation doesn't need updatring
       * the invalidation area, bail out.
       */
      if (info->invalidation_region)
        continue;

      if (context_has_animatable_region (context, info->region_id))
        {
          cairo_rectangle_int_t rect;

          rect.x = (gint) x;
          rect.y = (gint) y;
          rect.width = (gint) width;
          rect.height = (gint) height;

          g_array_append_val (info->rectangles, rect);
        }
    }
}

void
gtk_style_context_invalidate (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  /* Avoid reentrancy */
  if (priv->invalidating_context)
    return;

  if (!priv->widget_path)
    return;

  priv->invalidating_context = TRUE;

  rebuild_properties (context);
  clear_property_cache (context);
  rebuild_icon_factories (context);

  g_signal_emit (context, signals[CHANGED], 0);

  priv->invalidating_context = FALSE;
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

  store_animation_region (context, x, y, width, height);

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

  store_animation_region (context, x, y, width, height);

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

  store_animation_region (context, x, y, size, size);

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

  store_animation_region (context, x, y, width, height);

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

  store_animation_region (context, x, y, width, height);

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

  store_animation_region (context, x, y, width, height);

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

  store_animation_region (context, x, y, width, height);

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

  store_animation_region (context, x, y, width, height);

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

  store_animation_region (context, x, y, width, height);

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

  store_animation_region (context, x, y, width, height);

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

  store_animation_region (context, x, y, width, height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_handle (priv->theming_engine, cr, x, y, width, height, orientation);
}
