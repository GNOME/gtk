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
  GtkCssStyleSheet *stylesheet;
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

      data = &g_array_index (cas->stylesheets,
                             GtkStyleProviderData,
                             iter->cascade_index[ix] - 1);
      if (highest_priority_data == NULL ||
          gtk_css_style_sheet_get_priority (data->stylesheet) > gtk_css_style_sheet_get_priority (highest_priority_data->stylesheet))
        {
          highest_priority_index = ix;
          highest_priority_data = data;
        }
    }

  if (highest_priority_data != NULL)
    {
      iter->cascade_index[highest_priority_index]--;
      return GTK_STYLE_PROVIDER (highest_priority_data->stylesheet);
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
    iter->cascade_index[ix] = cas->stylesheets->len;

  return gtk_style_cascade_iter_next (cascade, iter);
}

static void
gtk_style_cascade_iter_clear (GtkStyleCascadeIter *iter)
{
  g_free (iter->cascade_index);
}

static GtkCssValue *
gtk_style_cascade_get_color (GtkStyleProvider *stylesheet,
                             const char              *name)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (stylesheet);
  GtkStyleCascadeIter iter;
  GtkCssValue *color;
  GtkStyleProvider *item;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      color = gtk_style_provider_get_color (item, name);
      if (color)
        {
          gtk_style_cascade_iter_clear (&iter);
          return color;
        }
    }

  gtk_style_cascade_iter_clear (&iter);
  return NULL;
}

static GtkCssKeyframes *
gtk_style_cascade_get_keyframes (GtkStyleProvider *stylesheet,
                                 const char       *name)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (stylesheet);
  GtkStyleCascadeIter iter;
  GtkCssKeyframes *keyframes;
  GtkStyleProvider *item;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      keyframes = gtk_style_provider_get_keyframes (item, name);
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
gtk_style_cascade_lookup (GtkStyleProvider             *stylesheet,
                          const GtkCountingBloomFilter *filter,
                          GtkCssNode                   *node,
                          GtkCssLookup                 *lookup,
                          GtkCssChange                 *change)
{
  GtkStyleCascade *cascade = GTK_STYLE_CASCADE (stylesheet);
  GtkStyleCascadeIter iter;
  GtkStyleProvider *item;
  GtkCssChange iter_change;

  for (item = gtk_style_cascade_iter_init (cascade, &iter);
       item;
       item = gtk_style_cascade_iter_next (cascade, &iter))
    {
      gtk_style_provider_lookup (item, filter, node, lookup,
                                 change ? &iter_change : NULL);
      if (change)
        *change |= iter_change;
    }
  gtk_style_cascade_iter_clear (&iter);
}

static void
gtk_style_cascade_provider_iface_init (GtkStyleProviderInterface *iface)
{
  iface->get_color = gtk_style_cascade_get_color;
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
  g_array_unref (cascade->stylesheets);

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

  g_signal_handler_disconnect (data->stylesheet, data->changed_signal_id);
  g_object_unref (data->stylesheet);
}

static void
_gtk_style_cascade_init (GtkStyleCascade *cascade)
{
  cascade->stylesheets = g_array_new (FALSE, FALSE, sizeof (GtkStyleProviderData));
  g_array_set_clear_func (cascade->stylesheets, style_provider_data_clear);
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
                                "gtk-private-changed",
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
gtk_style_cascade_add_style_sheet (GtkStyleCascade  *cascade,
                                   GtkCssStyleSheet *stylesheet)
{
  GtkStyleProviderData data;
  guint i;

  gtk_internal_return_if_fail (GTK_IS_STYLE_CASCADE (cascade));
  gtk_internal_return_if_fail (GTK_IS_CSS_STYLE_SHEET (stylesheet));

  data.stylesheet = g_object_ref (stylesheet);
  data.changed_signal_id = g_signal_connect_swapped (stylesheet,
                                                     "gtk-private-changed",
                                                     G_CALLBACK (gtk_style_provider_changed),
                                                     cascade);

  /* ensure it gets removed first */
  gtk_style_cascade_remove_style_sheet (cascade, stylesheet);

  for (i = 0; i < cascade->stylesheets->len; i++)
    {
      GtkStyleProviderData *adata = &g_array_index (cascade->stylesheets, GtkStyleProviderData, i);

      if (gtk_css_style_sheet_get_priority (adata->stylesheet) > gtk_css_style_sheet_get_priority (stylesheet))
        break;
    }
  g_array_insert_val (cascade->stylesheets, i, data);

  gtk_style_provider_changed (GTK_STYLE_PROVIDER (cascade));
}

void
gtk_style_cascade_remove_style_sheet (GtkStyleCascade  *cascade,
                                      GtkCssStyleSheet *stylesheet)
{
  guint i;

  gtk_internal_return_if_fail (GTK_IS_STYLE_CASCADE (cascade));
  gtk_internal_return_if_fail (GTK_IS_CSS_STYLE_SHEET (stylesheet));

  for (i = 0; i < cascade->stylesheets->len; i++)
    {
      GtkStyleProviderData *data = &g_array_index (cascade->stylesheets, GtkStyleProviderData, i);

      if (data->stylesheet == stylesheet)
        {
          g_array_remove_index (cascade->stylesheets, i);
  
          gtk_style_provider_changed (GTK_STYLE_PROVIDER (cascade));
          break;
        }
    }
}

