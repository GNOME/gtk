/* gtkshortcutsgroup.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkshortcutsgroupprivate.h"

#include "gtklabel.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkintl.h"

struct _GtkShortcutsGroup
{
  GtkBox    parent_instance;

  GtkLabel *title;
  gchar *view;
};

struct _GtkShortcutsGroupClass
{
  GtkBoxClass parent_class;
};

G_DEFINE_TYPE (GtkShortcutsGroup, gtk_shortcuts_group, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_TITLE,
  PROP_VIEW,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

static void
gtk_shortcuts_group_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkShortcutsGroup *self = GTK_SHORTCUTS_GROUP (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    case PROP_VIEW:
      g_value_set_string (value, self->view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_group_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkShortcutsGroup *self = GTK_SHORTCUTS_GROUP (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    case PROP_VIEW:
      g_free (self->view);
      self->view = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_group_finalize (GObject *object)
{
  GtkShortcutsGroup *self = GTK_SHORTCUTS_GROUP (object);

  g_free (self->view);

  G_OBJECT_CLASS (gtk_shortcuts_group_parent_class)->finalize (object);
}

static void
gtk_shortcuts_group_class_init (GtkShortcutsGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_shortcuts_group_finalize;
  object_class->get_property = gtk_shortcuts_group_get_property;
  object_class->set_property = gtk_shortcuts_group_set_property;

  properties[PROP_TITLE] =
    g_param_spec_string ("title", P_("Title"), P_("Title"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  properties[PROP_VIEW] =
    g_param_spec_string ("view", P_("View"), P_("View"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gtk_shortcuts_group_init (GtkShortcutsGroup *self)
{
  PangoAttrList *attrs;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing (GTK_BOX (self), 10);

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  self->title = g_object_new (GTK_TYPE_LABEL,
                              "attributes", attrs,
                              "visible", TRUE,
                              "xalign", 0.0f,
                              NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->title));
  pango_attr_list_unref (attrs);
}
