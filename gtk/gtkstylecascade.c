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

typedef struct _GtkStyleCascadeIter GtkStyleCascadeIter;
typedef struct _GtkStyleProviderData GtkStyleProviderData;

struct _GtkStyleCascadeIter {
  int parent_index; /* pointing at last index that was returned, not next one that should be returned */
  int index;        /* pointing at last index that was returned, not next one that should be returned */
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
  if (iter->parent_index > 0)
    {
      if (iter->index > 0)
        {
          GtkStyleProviderData *data, *parent_data;

          data = &g_array_index (cascade->providers, GtkStyleProviderData, iter->index - 1);
          parent_data = &g_array_index (cascade->parent->providers, GtkStyleProviderData, iter->parent_index - 1);

          if (data->priority >= parent_data->priority)
            {
              iter->index--;
              return data->provider;
            }
          else
            {
              iter->parent_index--;
              return parent_data->provider;
            }
        }
      else
        {
          iter->parent_index--;
          return g_array_index (cascade->parent->providers, GtkStyleProviderData, iter->parent_index).provider;
        }
    }
  else
    {
      if (iter->index > 0)
        {
          iter->index--;
          return g_array_index (cascade->providers, GtkStyleProviderData, iter->index).provider;
        }
      else
        {
          return NULL;
        }
    }
}

static GtkStyleProvider *
gtk_style_cascade_iter_init (GtkStyleCascade     *cascade,
                             GtkStyleCascadeIter *iter)
{
  iter->parent_index = cascade->parent ? cascade->parent->providers->len : 0;
  iter->index = cascade->providers->len;

  return gtk_style_cascade_iter_next (cascade, iter);
}

static gboolean
gtk_style_cascade_get_style_property (GtkStyleProvider *provider,
                                      GtkWidgetPath    *path,
                                      GtkStateFlags     state,
                                      GParamSpec       *pspec,
                                      GValue           *value)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (provider);
  GtkStyleCascadeIter iter;
  GtkStyleProvider *item;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      if (gtk_style_provider_get_style_property (item,
                                                 path,
                                                 state,
                                                 pspec,
                                                 value))
        return TRUE;
    }

  return FALSE;
}

static void
gtk_style_cascade_provider_iface_init (GtkStyleProviderIface *iface)
{
  iface->get_style_property = gtk_style_cascade_get_style_property;
}

static GtkSettings *
gtk_style_cascade_get_settings (GtkStyleProviderPrivate *provider)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (provider);
  GtkStyleCascadeIter iter;
  GtkSettings *settings;
  GtkStyleProvider *item;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      if (!GTK_IS_STYLE_PROVIDER_PRIVATE (item))
        continue;
          
      settings = _gtk_style_provider_private_get_settings (GTK_STYLE_PROVIDER_PRIVATE (item));
      if (settings)
        return settings;
    }

  return NULL;
}

static GtkCssValue *
gtk_style_cascade_get_color (GtkStyleProviderPrivate *provider,
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
      if (GTK_IS_STYLE_PROVIDER_PRIVATE (item))
        {
          color = _gtk_style_provider_private_get_color (GTK_STYLE_PROVIDER_PRIVATE (item), name);
          if (color)
            return color;
        }
      else
        {
          /* If somebody hits this code path, shout at them */
        }
    }

  return NULL;
}

static GtkCssKeyframes *
gtk_style_cascade_get_keyframes (GtkStyleProviderPrivate *provider,
                                 const char              *name)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (provider);
  GtkStyleCascadeIter iter;
  GtkCssKeyframes *keyframes;
  GtkStyleProvider *item;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      if (!GTK_IS_STYLE_PROVIDER_PRIVATE (item))
        continue;
          
      keyframes = _gtk_style_provider_private_get_keyframes (GTK_STYLE_PROVIDER_PRIVATE (item), name);
      if (keyframes)
        return keyframes;
    }

  return NULL;
}

static void
gtk_style_cascade_lookup (GtkStyleProviderPrivate *provider,
                          const GtkCssMatcher     *matcher,
                          GtkCssLookup            *lookup)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (provider);
  GtkStyleCascadeIter iter;
  GtkStyleProvider *item;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      if (GTK_IS_STYLE_PROVIDER_PRIVATE (item))
        {
          _gtk_style_provider_private_lookup (GTK_STYLE_PROVIDER_PRIVATE (item),
                                              matcher,
                                              lookup);
        }
      else
        {
          /* you lose */
          g_warn_if_reached ();
        }
    }
}

static GtkCssChange
gtk_style_cascade_get_change (GtkStyleProviderPrivate *provider,
                              const GtkCssMatcher     *matcher)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (provider);
  GtkStyleCascadeIter iter;
  GtkStyleProvider *item;
  GtkCssChange change = 0;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      if (GTK_IS_STYLE_PROVIDER_PRIVATE (item))
        {
          change |= _gtk_style_provider_private_get_change (GTK_STYLE_PROVIDER_PRIVATE (item),
                                                            matcher);
        }
      else
        {
          g_return_val_if_reached (GTK_CSS_CHANGE_ANY);
        }
    }

  return change;
}

static void
gtk_style_cascade_provider_private_iface_init (GtkStyleProviderPrivateInterface *iface)
{
  iface->get_color = gtk_style_cascade_get_color;
  iface->get_settings = gtk_style_cascade_get_settings;
  iface->get_keyframes = gtk_style_cascade_get_keyframes;
  iface->lookup = gtk_style_cascade_lookup;
  iface->get_change = gtk_style_cascade_get_change;
}

G_DEFINE_TYPE_EXTENDED (GtkStyleCascade, _gtk_style_cascade, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_style_cascade_provider_iface_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER_PRIVATE,
                                               gtk_style_cascade_provider_private_iface_init));

static void
gtk_style_cascade_dispose (GObject *object)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (object);

  _gtk_style_cascade_set_parent (cascade, NULL);
  g_array_unref (cascade->providers);

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
  cascade->providers = g_array_new (FALSE, FALSE, sizeof (GtkStyleProviderData));
  g_array_set_clear_func (cascade->providers, style_provider_data_clear);
}

GtkStyleCascade *
_gtk_style_cascade_new (void)
{
  return g_object_new (GTK_TYPE_STYLE_CASCADE, NULL);
}

GtkStyleCascade *
_gtk_style_cascade_get_for_screen (GdkScreen *screen)
{
  GtkStyleCascade *cascade;
  static GQuark quark = 0;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  if (G_UNLIKELY (!quark))
    quark = g_quark_from_static_string ("gtk-style-cascade");

  cascade = g_object_get_qdata (G_OBJECT (screen), quark);
  if (cascade == NULL)
    {
      cascade = _gtk_style_cascade_new ();
      g_object_set_qdata_full (G_OBJECT (screen), quark, cascade, g_object_unref);
    }

  return cascade;
}

void
_gtk_style_cascade_set_parent (GtkStyleCascade *cascade,
                               GtkStyleCascade *parent)
{
  g_return_if_fail (GTK_IS_STYLE_CASCADE (cascade));
  g_return_if_fail (parent == NULL || GTK_IS_STYLE_CASCADE (parent));
  if (parent)
    g_return_if_fail (parent->parent == NULL);

  if (cascade->parent == parent)
    return;

  if (parent)
    {
      g_object_ref (parent);
      g_signal_connect_swapped (parent,
                                "-gtk-private-changed",
                                G_CALLBACK (_gtk_style_provider_private_changed),
                                cascade);
    }

  if (cascade->parent)
    {
      g_signal_handlers_disconnect_by_func (cascade->parent, 
                                            _gtk_style_provider_private_changed,
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

  g_return_if_fail (GTK_IS_STYLE_CASCADE (cascade));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));
  g_return_if_fail (GTK_STYLE_PROVIDER (cascade) != provider);

  data.provider = g_object_ref (provider);
  data.priority = priority;
  data.changed_signal_id = g_signal_connect_swapped (provider,
                                                     "-gtk-private-changed",
                                                     G_CALLBACK (_gtk_style_provider_private_changed),
                                                     cascade);

  /* ensure it gets removed first */
  _gtk_style_cascade_remove_provider (cascade, provider);

  for (i = 0; i < cascade->providers->len; i++)
    {
      if (g_array_index (cascade->providers, GtkStyleProviderData, i).priority > priority)
        break;
    }
  g_array_insert_val (cascade->providers, i, data);

  _gtk_style_provider_private_changed (GTK_STYLE_PROVIDER_PRIVATE (cascade));
}

void
_gtk_style_cascade_remove_provider (GtkStyleCascade  *cascade,
                                    GtkStyleProvider *provider)
{
  guint i;

  g_return_if_fail (GTK_IS_STYLE_CASCADE (cascade));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  for (i = 0; i < cascade->providers->len; i++)
    {
      GtkStyleProviderData *data = &g_array_index (cascade->providers, GtkStyleProviderData, i);

      if (data->provider == provider)
        {
          g_array_remove_index (cascade->providers, i);
  
          _gtk_style_provider_private_changed (GTK_STYLE_PROVIDER_PRIVATE (cascade));
          break;
        }
    }
}

