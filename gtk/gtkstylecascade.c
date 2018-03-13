/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Benjamin Otte <otte@gnome.org>
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

#include "gtkstylecascadeprivate.h"

#include "gtkstyleprovider.h"
#include "gtkstyleproviderprivate.h"
#include "gtkprivate.h"

typedef struct _GtkStyleCascadeIter GtkStyleCascadeIter;
typedef struct _GtkStyleProviderData GtkStyleProviderData;

struct _GtkStyleCascadeIter {
  int  n_cascades;
  int *cascade_index;  /* each one points at last index that was returned, */
                       /* not next one that should be returned */
};

struct _GtkStyleProviderData
{
  GtkStyleProvider *provider;
  guint priority;
  guint changed_signal_id;
};

static GtkStyleProvider *
gtk_style_cascade_iter_next (GtkStyleCascade     *cascade,
                             GtkStyleCascadeIter *iter)
{
  GtkStyleCascade *cas;
  int ix, highest_priority_index = 0;
  GtkStyleProviderData *highest_priority_data = NULL;

  for (cas = cascade, ix = 0; ix < iter->n_cascades; cas = cas->parent, ix++)
    {
      GtkStyleProviderData *data;

      if (iter->cascade_index[ix] <= 0)
        continue;

      data = &g_array_index (cas->providers,
                             GtkStyleProviderData,
                             iter->cascade_index[ix] - 1);
      if (highest_priority_data == NULL || data->priority > highest_priority_data->priority)
        {
          highest_priority_index = ix;
          highest_priority_data = data;
        }
    }

  if (highest_priority_data != NULL)
    {
      iter->cascade_index[highest_priority_index]--;
      return highest_priority_data->provider;
    }
  return NULL;
}

static GtkStyleProvider *
gtk_style_cascade_iter_init (GtkStyleCascade     *cascade,
                             GtkStyleCascadeIter *iter)
{
  GtkStyleCascade *cas = cascade;
  int ix;

  iter->n_cascades = 1;
  while ((cas = cas->parent) != NULL)
    iter->n_cascades++;

  iter->cascade_index = g_new (int, iter->n_cascades);
  for (cas = cascade, ix = 0; ix < iter->n_cascades; cas = cas->parent, ix++)
    iter->cascade_index[ix] = cas->providers->len;

  return gtk_style_cascade_iter_next (cascade, iter);
}

static void
gtk_style_cascade_iter_clear (GtkStyleCascadeIter *iter)
{
  g_free (iter->cascade_index);
}

static GtkSettings *
gtk_style_cascade_get_settings (GtkStyleProvider *provider)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (provider);
  GtkStyleCascadeIter iter;
  GtkSettings *settings;
  GtkStyleProvider *item;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      if (!GTK_IS_STYLE_PROVIDER (item))
        continue;
          
      settings = gtk_style_provider_get_settings (GTK_STYLE_PROVIDER (item));
      if (settings)
        {
          gtk_style_cascade_iter_clear (&iter);
          return settings;
        }
    }

  gtk_style_cascade_iter_clear (&iter);
  return NULL;
}

static GtkCssValue *
gtk_style_cascade_get_color (GtkStyleProvider *provider,
                             const char              *name)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (provider);
  GtkStyleCascadeIter iter;
  GtkCssValue *color;
  GtkStyleProvider *item;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      if (GTK_IS_STYLE_PROVIDER (item))
        {
          color = gtk_style_provider_get_color (GTK_STYLE_PROVIDER (item), name);
          if (color)
            {
              gtk_style_cascade_iter_clear (&iter);
              return color;
            }
        }
      else
        {
          /* If somebody hits this code path, shout at them */
        }
    }

  gtk_style_cascade_iter_clear (&iter);
  return NULL;
}

static GdkPaintable *
gtk_style_cascade_get_paint (GtkStyleProvider *provider,
                             const char       *name)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (provider);
  GtkStyleCascadeIter iter;
  GdkPaintable *paintable;
  GtkStyleProvider *item;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      if (GTK_IS_STYLE_PROVIDER (item))
        {
          paintable = gtk_style_provider_get_paint (GTK_STYLE_PROVIDER (item), name);
          if (paintable)
            {
              gtk_style_cascade_iter_clear (&iter);
              return paintable;
            }
        }
      else
        {
          /* If somebody hits this code path, shout at them */
        }
    }

  gtk_style_cascade_iter_clear (&iter);

  if (cascade->paints == NULL)
    return NULL;

  return g_hash_table_lookup (cascade->paints, name);
}

static int
gtk_style_cascade_get_scale (GtkStyleProvider *provider)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (provider);

  return cascade->scale;
}

static GtkCssKeyframes *
gtk_style_cascade_get_keyframes (GtkStyleProvider *provider,
                                 const char       *name)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (provider);
  GtkStyleCascadeIter iter;
  GtkCssKeyframes *keyframes;
  GtkStyleProvider *item;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      if (!GTK_IS_STYLE_PROVIDER (item))
        continue;
          
      keyframes = gtk_style_provider_get_keyframes (GTK_STYLE_PROVIDER (item), name);
      if (keyframes)
        {
          gtk_style_cascade_iter_clear (&iter);
          return keyframes;
        }
    }

  gtk_style_cascade_iter_clear (&iter);
  return NULL;
}

static void
gtk_style_cascade_lookup (GtkStyleProvider    *provider,
                          const GtkCssMatcher *matcher,
                          GtkCssLookup        *lookup,
                          GtkCssChange        *change)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (provider);
  GtkStyleCascadeIter iter;
  GtkStyleProvider *item;
  GtkCssChange iter_change;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      GtkStyleProvider *sp = (GtkStyleProvider *) item;
      if (GTK_IS_STYLE_PROVIDER (sp))
        {
          gtk_style_provider_lookup (sp, matcher, lookup,
                                              change ? &iter_change : NULL);
          if (change)
            *change |= iter_change;
        }
      else
        {
          /* you lose */
          g_warn_if_reached ();
        }
    }
  gtk_style_cascade_iter_clear (&iter);
}

static void
gtk_style_cascade_provider_iface_init (GtkStyleProviderInterface *iface)
{
  iface->get_color = gtk_style_cascade_get_color;
  iface->get_paint = gtk_style_cascade_get_paint;
  iface->get_settings = gtk_style_cascade_get_settings;
  iface->get_scale = gtk_style_cascade_get_scale;
  iface->get_keyframes = gtk_style_cascade_get_keyframes;
  iface->lookup = gtk_style_cascade_lookup;
}

G_DEFINE_TYPE_EXTENDED (GtkStyleCascade, _gtk_style_cascade, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_style_cascade_provider_iface_init));

static void
gtk_style_cascade_dispose (GObject *object)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (object);

  _gtk_style_cascade_set_parent (cascade, NULL);
  g_array_unref (cascade->providers);
  g_clear_pointer (&cascade->paints, g_hash_table_unref);

  G_OBJECT_CLASS (_gtk_style_cascade_parent_class)->dispose (object);
}

static void
_gtk_style_cascade_class_init (GtkStyleCascadeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_style_cascade_dispose;
}

static void
style_provider_data_clear (gpointer data_)
{
  GtkStyleProviderData *data = data_;

  g_signal_handler_disconnect (data->provider, data->changed_signal_id);
  g_object_unref (data->provider);
}

static void
_gtk_style_cascade_init (GtkStyleCascade *cascade)
{
  cascade->scale = 1;

  cascade->providers = g_array_new (FALSE, FALSE, sizeof (GtkStyleProviderData));
  g_array_set_clear_func (cascade->providers, style_provider_data_clear);
}

GtkStyleCascade *
_gtk_style_cascade_new (void)
{
  return g_object_new (GTK_TYPE_STYLE_CASCADE, NULL);
}

void
_gtk_style_cascade_set_parent (GtkStyleCascade *cascade,
                               GtkStyleCascade *parent)
{
  gtk_internal_return_if_fail (GTK_IS_STYLE_CASCADE (cascade));
  gtk_internal_return_if_fail (parent == NULL || GTK_IS_STYLE_CASCADE (parent));

  if (cascade->parent == parent)
    return;

  if (parent)
    {
      g_object_ref (parent);
      g_signal_connect_swapped (parent,
                                "-gtk-private-changed",
                                G_CALLBACK (gtk_style_provider_changed),
                                cascade);
    }

  if (cascade->parent)
    {
      g_signal_handlers_disconnect_by_func (cascade->parent, 
                                            gtk_style_provider_changed,
                                            cascade);
      g_object_unref (cascade->parent);
    }

  cascade->parent = parent;
}

void
_gtk_style_cascade_add_provider (GtkStyleCascade  *cascade,
                                 GtkStyleProvider *provider,
                                 guint             priority)
{
  GtkStyleProviderData data;
  guint i;

  gtk_internal_return_if_fail (GTK_IS_STYLE_CASCADE (cascade));
  gtk_internal_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));
  gtk_internal_return_if_fail (GTK_STYLE_PROVIDER (cascade) != provider);

  data.provider = g_object_ref (provider);
  data.priority = priority;
  data.changed_signal_id = g_signal_connect_swapped (provider,
                                                     "-gtk-private-changed",
                                                     G_CALLBACK (gtk_style_provider_changed),
                                                     cascade);

  /* ensure it gets removed first */
  _gtk_style_cascade_remove_provider (cascade, provider);

  for (i = 0; i < cascade->providers->len; i++)
    {
      if (g_array_index (cascade->providers, GtkStyleProviderData, i).priority > priority)
        break;
    }
  g_array_insert_val (cascade->providers, i, data);

  gtk_style_provider_changed (GTK_STYLE_PROVIDER (cascade));
}

void
_gtk_style_cascade_remove_provider (GtkStyleCascade  *cascade,
                                    GtkStyleProvider *provider)
{
  guint i;

  gtk_internal_return_if_fail (GTK_IS_STYLE_CASCADE (cascade));
  gtk_internal_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  for (i = 0; i < cascade->providers->len; i++)
    {
      GtkStyleProviderData *data = &g_array_index (cascade->providers, GtkStyleProviderData, i);

      if (data->provider == provider)
        {
          g_array_remove_index (cascade->providers, i);
  
          gtk_style_provider_changed (GTK_STYLE_PROVIDER (cascade));
          break;
        }
    }
}

void
_gtk_style_cascade_set_scale (GtkStyleCascade *cascade,
                              int              scale)
{
  gtk_internal_return_if_fail (GTK_IS_STYLE_CASCADE (cascade));

  if (cascade->scale == scale)
    return;

  cascade->scale = scale;

  gtk_style_provider_changed (GTK_STYLE_PROVIDER (cascade));
}

int
_gtk_style_cascade_get_scale (GtkStyleCascade *cascade)
{
  gtk_internal_return_val_if_fail (GTK_IS_STYLE_CASCADE (cascade), 1);

  return cascade->scale;
}

gboolean
gtk_style_cascade_add_paint (GtkStyleCascade *cascade,
                             const char      *name,
                             GdkPaintable    *paintable)
{
  if (cascade->paints == NULL)
    {
      cascade->paints = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
    }
  else
    {
      if (g_hash_table_contains (cascade->paints, name))
        return FALSE;
    }

  g_hash_table_insert (cascade->paints, g_strdup (name), g_object_ref (paintable));
  gtk_style_provider_changed (GTK_STYLE_PROVIDER (cascade));

  return TRUE;
}

gboolean
gtk_style_cascade_remove_paint (GtkStyleCascade *cascade,
                                const char      *name)
{
  gboolean result;

  if (cascade->paints == NULL)
    return FALSE;

  result = g_hash_table_remove (cascade->paints, name);
  if (result)
    gtk_style_provider_changed (GTK_STYLE_PROVIDER (cascade));
  return result;
}
