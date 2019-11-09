/*
 * Copyright © 2019 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkfontlist.h"

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtksettings.h"

/**
 * SECTION:gtkfontlist
 * @title: GtkFontList
 * @short_description: A list model for all installed fonts
 * @see_also: #GListModel, #GtkFontChooser
 *
 * #GtkFontList is a list model of #PangoFontFace that contains the current
 * fonts installed on the system. It updates itself automatically when new
 * fonts get installed.
 *
 * This list is used by GTK's #GtkFontChooser implementations, so the fonts
 * listed by either do match.
 */

enum {
  PROP_0,
  PROP_DISPLAY,
  PROP_FAMILIES_ONLY,
  PROP_FONT_MAP,
  NUM_PROPERTIES
};

struct _GtkFontList
{
  GObject parent_instance;

  GdkDisplay *display;
  PangoFontMap *font_map; /* may be NULL */
  gboolean families_only;

  GSequence *faces; /* Use GPtrArray or GListStore here? */
};

struct _GtkFontListClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType
gtk_font_list_get_item_type (GListModel *list)
{
  return PANGO_TYPE_FONT_FACE;
}

static guint
gtk_font_list_get_n_items (GListModel *list)
{
  GtkFontList *self = GTK_FONT_LIST (list);

  return g_sequence_get_length (self->faces);
}

static gpointer
gtk_font_list_get_item (GListModel *list,
                        guint       position)
{
  GtkFontList *self = GTK_FONT_LIST (list);
  GSequenceIter *iter;

  iter = g_sequence_get_iter_at_pos (self->faces, position);

  if (g_sequence_iter_is_end (iter))
    return NULL;
  else
    return g_object_ref (g_sequence_get (iter));
}

static void
gtk_font_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_font_list_get_item_type;
  iface->get_n_items = gtk_font_list_get_n_items;
  iface->get_item = gtk_font_list_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkFontList, gtk_font_list, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_font_list_model_init))

static void
gtk_font_list_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkFontList *self = GTK_FONT_LIST (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      gtk_font_list_set_display (self, g_value_get_object (value));
      break;

    case PROP_FAMILIES_ONLY:
      gtk_font_list_set_families_only (self, g_value_get_boolean(value));
      break;

    case PROP_FONT_MAP:
      gtk_font_list_set_font_map (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_font_list_get_property (GObject     *object,
                            guint        prop_id,
                            GValue      *value,
                            GParamSpec  *pspec)
{
  GtkFontList *self = GTK_FONT_LIST (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, self->display);
      break;

    case PROP_FAMILIES_ONLY:
      g_value_set_boolean (value, self->families_only);
      break;

    case PROP_FONT_MAP:
      g_value_set_object (value, self->font_map);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static int
gtk_font_list_compare_families (PangoFontFamily *a,
                                PangoFontFamily *b)
{
  const char *a_name = pango_font_family_get_name (a);
  const char *b_name = pango_font_family_get_name (b);

  return g_utf8_collate (a_name, b_name);
}

static int
gtk_font_list_compare_families_qsort (const void *a,
                                      const void *b)
{
  return gtk_font_list_compare_families (*(PangoFontFamily **) a, *(PangoFontFamily **) b);
}

static void
gtk_font_list_rescan (GtkFontList *self)
{
  gint n_families, i;
  PangoFontFamily **families;
  PangoFontMap *font_map;
  GSequenceIter *iter;
  guint first, last, n_before, n_after;
#define MARK_CHANGED(iter,insert) G_STMT_START {\
  guint iter_pos = g_sequence_iter_get_position (iter); \
  first = MIN (first, iter_pos); \
  last = iter_pos + ((insert) ? 1 : 0); \
} G_STMT_END

  if (self->font_map)
    font_map = self->font_map;
  else
    font_map = pango_cairo_font_map_get_default ();
  pango_font_map_list_families (font_map, &families, &n_families);

  qsort (families, n_families, sizeof (PangoFontFamily *), gtk_font_list_compare_families_qsort);

  n_before = g_sequence_get_length (self->faces);

  iter = g_sequence_get_begin_iter (self->faces);
  first = G_MAXUINT;
  last = 0;

  /* Iterate over families and faces */
  for (i = 0; i < n_families; i++)
    {
      PangoFontFace **faces;
      int j, n_faces;

      pango_font_family_list_faces (families[i], &faces, &n_faces);
      while (!g_sequence_iter_is_end (iter) && 
             gtk_font_list_compare_families (families[i], pango_font_face_get_family (g_sequence_get (iter))) > 0)
        {
          GSequenceIter *remove = iter;
          MARK_CHANGED (iter, FALSE);
          iter = g_sequence_iter_next (iter);
          g_sequence_remove (remove);
        }

      if (self->families_only)
        n_faces = MIN (1, n_faces);

      for (j = 0; j < n_faces; j++)
        {
          if (g_sequence_iter_is_end (iter))
            {
              /* nothing to see */
            }
          else if (faces[j] == g_sequence_get (iter))
            {
              iter = g_sequence_iter_next (iter);
              continue;
            }
          else
            {
              while (!g_sequence_iter_is_end (iter) && 
                     gtk_font_list_compare_families (families[i], pango_font_face_get_family (g_sequence_get (iter))) >= 0)
                {
                  GSequenceIter *remove = iter;
                  MARK_CHANGED (iter, FALSE);
                  iter = g_sequence_iter_next (iter);
                  g_sequence_remove (remove);
                }
            }
          MARK_CHANGED (iter, TRUE);
          g_sequence_insert_before (iter, g_object_ref (faces[j]));
        }

      g_free (faces);
    }

  g_free (families);

  if (!g_sequence_iter_is_end (iter))
    g_sequence_remove_range (iter, g_sequence_get_end_iter (self->faces));
  n_after = g_sequence_get_length (self->faces);

  if (first <= last)
    {
      last = n_after - MIN (n_after, last + 1);
      g_list_model_items_changed (G_LIST_MODEL (self),
                                  first,
                                  n_before - first - last,
                                  n_after - first - last);
    }
  else if (n_after > n_before)
    {
      g_list_model_items_changed (G_LIST_MODEL (self), n_before, 0, n_after - n_before);
    }
  else if (n_after < n_before)
    {
      g_list_model_items_changed (G_LIST_MODEL (self), n_after, n_before - n_after, 0);
    }
}

static void
gtk_font_list_unset_display (GtkFontList *self)
{
  GtkSettings *settings;

  if (self->display == NULL)
    return;

  settings = gtk_settings_get_for_display (self->display);
  g_signal_handlers_disconnect_by_func (settings, gtk_font_list_rescan, self);
  g_clear_object (&self->display);
}

static void
gtk_font_list_dispose (GObject *object)
{
  GtkFontList *self = GTK_FONT_LIST (object);

  gtk_font_list_unset_display (self);

  g_clear_object (&self->font_map);

  G_OBJECT_CLASS (gtk_font_list_parent_class)->dispose (object);
}

static void
gtk_font_list_class_init (GtkFontListClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_font_list_set_property;
  gobject_class->get_property = gtk_font_list_get_property;
  gobject_class->dispose = gtk_font_list_dispose;

  /**
   * GtkFontList:display:
   *
   * The display to list fonts for
   */
  properties[PROP_DISPLAY] =
      g_param_spec_object ("display",
                           P_("Display"),
                           P_("Display to list fonts for"),
                           GDK_TYPE_DISPLAY,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFontList:font-map:
   *
   * Font map to use
   */
  properties[PROP_FONT_MAP] =
      g_param_spec_object ("font-map",
                          P_("Font map"),
                          P_("Font map to use"),
                          PANGO_TYPE_FONT_MAP,
                          GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFontList:families-only:
   *
   * %TRUE if only one face per font family is loaded
   */
  properties[PROP_FAMILIES_ONLY] =
      g_param_spec_boolean ("families-only",
                            P_("Families only"),
                            P_("Set to load only one face per family"),
                            FALSE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_font_list_init (GtkFontList *self)
{
  self->faces = g_sequence_new (g_object_unref);
}

/**
 * gtk_font_list_new:
 * @file: (allow-none): The file to query
 * @attributes: (allow-none): The attributes to query with
 *
 * Creates a new #GtkFontList querying the given @file with the given
 * @attributes.
 *
 * Returns: a new #GtkFontList
 **/
GtkFontList *
gtk_font_list_new (void)
{
  return g_object_new (GTK_TYPE_FONT_LIST,
                       NULL);
}

/**
 * gtk_font_list_set_display:
 * @self: a #GtkFontList
 * @display: (allow-none) (transfer none): the #GdkDisplay to load fonts
 *     for or %NULL to use gdk_display_get_default()
 *
 * Sets the @display to enumerate fonts for.
 *
 * If @display is %NULL, then the defualt display will be used instead.
 */
void
gtk_font_list_set_display (GtkFontList *self,
                           GdkDisplay  *display)
{
  GtkSettings *settings;

  g_return_if_fail (GTK_IS_FONT_LIST (self));
  g_return_if_fail (display == NULL || GDK_IS_DISPLAY (display));

  if (display == NULL)
    display = gdk_display_get_default ();

  if (self->display == display)
    return;

  gtk_font_list_unset_display (self);

  self->display = g_object_ref (display);

  settings = gtk_settings_get_for_display (self->display);
  g_signal_connect_swapped (settings,
                            "notify::gtk-fontconfig-timestamp",
                            G_CALLBACK (gtk_font_list_rescan),
                            self);

  gtk_font_list_rescan (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DISPLAY]);
}

/**
 * gtk_font_list_get_display:
 * @self: a #GtkFontList
 *
 * Gets the display fonts are listed for.
 *
 * Returns: (transfer none): The display fonts are enumerated for
 **/
GdkDisplay *
gtk_font_list_get_display (GtkFontList *self)
{
  g_return_val_if_fail (GTK_IS_FONT_LIST (self), NULL);

  return self->display;
}

/**
 * gtk_font_list_set_families_only:
 * @self: a #GtkFontList
 * @families_only: %TRUE to only load one face per family
 *
 * Set to %TRUE to only list one face per #PangoFontFamily. If set
 * to %FALSE (the default), @self with list all faces for each family.
 */
void
gtk_font_list_set_families_only (GtkFontList *self,
                                 gboolean     families_only)
{
  g_return_if_fail (GTK_IS_FONT_LIST (self));

  if (self->families_only == families_only)
    return;

  self->families_only = families_only;

  gtk_font_list_rescan (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FAMILIES_ONLY]);
}

/**
 * gtk_font_list_get_families_only:
 * @self: a #GtkFontList
 *
 * Gets the value set via gtk_font_list_set_families_only().
 *
 * Returns: %TRUE if only one face is listed for each #PangoFontFamily
 */
gboolean
gtk_font_list_get_families_only (GtkFontList *self)
{
  g_return_val_if_fail (GTK_IS_FONT_LIST (self), FALSE);

  return self->families_only;
}

/**
 * gtk_font_list_set_font_map:
 * @self: a #GtkFontList
 * @font_map: (nullable) (transfer none): A custom #PangoFontMap to use
 *     for enumerating fonts or %NULL to use the default
 *
 * Sets the #PangoFontMap used to enumerate fonts.
 * If set to %NULL (the default), GTK will use the display's default way
 * to enumerate fonts.
 *
 * Note that even when a custom @font_map is loaded, GTK will still
 * monitor the GtkFontMap::display for changes to installed fonts,
 * because it assumes that the @font_map still contains those fonts.
 */
void
gtk_font_list_set_font_map (GtkFontList  *self,
                            PangoFontMap *font_map)
{
  g_return_if_fail (GTK_IS_FONT_LIST (self));
  g_return_if_fail (font_map == NULL || PANGO_IS_FONT_MAP (font_map));

  if (g_set_object (&self->font_map, font_map))
    return;

  gtk_font_list_rescan (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_MAP]);
}

/**
 * gtk_font_list_get_font_map:
 * @self: a #GtkFontList
 *
 * Returns the #PangoFontMap set via gtk_font_list_set_font_map().
 *
 * Returns: (nullable) (transfer none): The custom font map used by @self
 *     or %NULL if no custom font map is in use.
 */
PangoFontMap *
gtk_font_list_get_font_map (GtkFontList *self)
{
  g_return_val_if_fail (GTK_IS_FONT_LIST (self), NULL);

  return self->font_map;
}

