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

#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"
#include "gtkthemingengine.h"
#include "gtkintl.h"

#include "gtkalias.h"

typedef struct GtkStyleContextPrivate GtkStyleContextPrivate;
typedef struct GtkStyleProviderData GtkStyleProviderData;
typedef struct GtkChildClass GtkChildClass;

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

struct GtkStyleContextPrivate
{
  GList *providers;
  GtkStyleSet *store;
  GtkWidgetPath *widget_path;

  GtkStateFlags state_flags;
  GList *style_classes;
  GList *child_style_classes;

  GtkThemingEngine *theming_engine;
};

#define GTK_STYLE_CONTEXT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_STYLE_CONTEXT, GtkStyleContextPrivate))

static void gtk_style_context_finalize (GObject *object);


G_DEFINE_TYPE (GtkStyleContext, gtk_style_context, G_TYPE_OBJECT)

static void
gtk_style_context_class_init (GtkStyleContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_style_context_finalize;

  g_type_class_add_private (object_class, sizeof (GtkStyleContextPrivate));
}

static void
gtk_style_context_init (GtkStyleContext *style_context)
{
  GtkStyleContextPrivate *priv;

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (style_context);
  priv->store = gtk_style_set_new ();
  priv->theming_engine = gtk_theming_engine_load (NULL);
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
gtk_style_context_finalize (GObject *object)
{
  GtkStyleContextPrivate *priv;

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (object);

  g_list_foreach (priv->providers, (GFunc) style_provider_data_free, NULL);
  g_list_free (priv->providers);

  G_OBJECT_CLASS (gtk_style_context_parent_class)->finalize (object);
}

static void
rebuild_properties (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GList *list;

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
  list = priv->providers;

  gtk_style_set_clear (priv->store);

  while (list)
    {
      GtkStyleProviderData *data;
      GtkStyleSet *provider_style;

      data = list->data;
      list = list->next;

      provider_style = gtk_style_provider_get_style (data->provider);
      gtk_style_set_merge (priv->store, provider_style, TRUE);
      g_object_unref (provider_style);
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

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
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

  rebuild_properties (context);
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

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
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
    rebuild_properties (context);
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

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
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

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
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

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);

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

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
  priv->state_flags = flags;
}

GtkStateFlags
gtk_style_context_get_state (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
  return priv->state_flags;
}

gboolean
gtk_style_context_is_state_set (GtkStyleContext *context,
                                GtkStateType     state)
{
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);

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

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);

  if (priv->widget_path)
    {
      gtk_widget_path_free (priv->widget_path);
      priv->widget_path = NULL;
    }

  if (path)
    priv->widget_path = gtk_widget_path_copy (path);
}

G_CONST_RETURN GtkWidgetPath *
gtk_style_context_get_path (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
  return priv->widget_path;
}

void
gtk_style_context_set_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GQuark class_quark;
  GList *link;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
  class_quark = g_quark_from_string (class_name);

  link = priv->style_classes;

  while (link)
    {
      GQuark link_quark;

      link_quark = GUINT_TO_POINTER (link->data);

      if (link_quark == class_quark)
        return;
      else if (link_quark > class_quark)
        {
          priv->style_classes = g_list_insert_before (priv->style_classes,
                                                      link, GUINT_TO_POINTER (class_quark));
          return;
        }

      link = link->next;
    }

  priv->style_classes = g_list_append (priv->style_classes,
                                       GUINT_TO_POINTER (class_quark));
}

void
gtk_style_context_unset_class (GtkStyleContext *context,
                               const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GQuark class_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return;

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
  priv->style_classes = g_list_remove (priv->style_classes,
                                       GUINT_TO_POINTER (class_quark));
}

gboolean
gtk_style_context_has_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GQuark class_quark;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (class_name != NULL, FALSE);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return FALSE;

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);

  if (g_list_find (priv->style_classes,
                   GUINT_TO_POINTER (class_quark)))
    return TRUE;

  return FALSE;
}

static gint
child_style_class_compare (gconstpointer p1,
                           gconstpointer p2)
{
  const GtkChildClass *c1, *c2;

  c1 = p1;
  c2 = p2;

  return (gint) c1->class_quark - c2->class_quark;
}

void
gtk_style_context_set_child_class (GtkStyleContext    *context,
                                   const gchar        *class_name,
                                   GtkChildClassFlags  flags)
{
  GtkStyleContextPrivate *priv;
  GtkChildClass *child_class;
  GQuark class_quark;
  GList *link;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
  class_quark = g_quark_from_string (class_name);
  link = priv->style_classes;

  while (link)
    {
      GtkChildClass *link_class;

      link_class = link->data;

      if (link_class->class_quark == class_quark)
        {
          link_class->flags = flags;
          return;
        }
      else if (link_class->class_quark > class_quark)
        break;

      link = link->next;
    }

  child_class = g_slice_new0 (GtkChildClass);
  child_class->class_quark = class_quark;
  child_class->flags = flags;

  if (link)
    priv->style_classes = g_list_insert_before (priv->style_classes,
                                                link, child_class);
  else
    priv->style_classes = g_list_append (priv->style_classes, child_class);
}

void
gtk_style_context_unset_child_class (GtkStyleContext    *context,
                                     const gchar        *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkChildClass child_class;
  GQuark class_quark;
  GList *link;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return;

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
  child_class.class_quark = class_quark;

  link = g_list_find_custom (priv->child_style_classes,
                             &child_class, child_style_class_compare);

  if (link)
    {
      priv->child_style_classes = g_list_remove_link (priv->child_style_classes, link);
      g_slice_free (GtkChildClass, link->data);
      g_list_free1 (link);
    }
}

gboolean
gtk_style_context_has_child_class (GtkStyleContext    *context,
                                   const gchar        *class_name,
                                   GtkChildClassFlags *flags_return)
{
  GtkStyleContextPrivate *priv;
  GtkChildClass child_class;
  GQuark class_quark;
  GList *link;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (class_name != NULL, FALSE);

  if (flags_return)
    *flags_return = 0;

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return FALSE;

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
  child_class.class_quark = class_quark;

  link = g_list_find_custom (priv->child_style_classes,
                             &child_class, child_style_class_compare);

  if (link)
    {
      if (*flags_return)
        {
          GtkChildClass *found_class;

          found_class = link->data;
          *flags_return = found_class->flags;
        }

      return TRUE;
    }

  return FALSE;
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

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
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

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
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

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
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

  priv = GTK_STYLE_CONTEXT_GET_PRIVATE (context);
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_background (priv->theming_engine, cr, x, y, width, height);
}

#define __GTK_STYLE_CONTEXT_C__
#include "gtkaliasdef.c"
