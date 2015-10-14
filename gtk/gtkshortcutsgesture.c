 /* gtkshortcutsgesture.c
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

#include "gtkshortcutsgestureprivate.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtksizegroup.h"
#include "gtkorientable.h"
#include "gtkstylecontext.h"
#include "gtkprivate.h"
#include "gtkintl.h"

struct _GtkShortcutsGesture
{
  GtkBox    parent_instance;

  GtkImage *image;
  GtkLabel *title;
  GtkLabel *subtitle;
  GtkBox   *desc_box;

  GtkSizeGroup *desc_size_group;
  GtkSizeGroup *icon_size_group;
};

struct _GtkShortcutsGestureClass
{
  GtkBoxClass parent_class;
};

G_DEFINE_TYPE (GtkShortcutsGesture, gtk_shortcuts_gesture, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_DESC_SIZE_GROUP,
  PROP_ICON,
  PROP_ICON_SIZE_GROUP,
  PROP_SUBTITLE,
  PROP_TITLE,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

static void
gtk_shortcuts_gesture_set_desc_size_group (GtkShortcutsGesture *self,
                                           GtkSizeGroup       *group)
{
  if (self->desc_size_group)
    gtk_size_group_remove_widget (self->desc_size_group, GTK_WIDGET (self->desc_box));
  if (group)
    gtk_size_group_add_widget (group, GTK_WIDGET (self->desc_box));

  g_set_object (&self->desc_size_group, group);
}

static void
gtk_shortcuts_gesture_set_icon_size_group (GtkShortcutsGesture *self,
                                           GtkSizeGroup        *group)
{
  if (self->icon_size_group)
    gtk_size_group_remove_widget (self->icon_size_group, GTK_WIDGET (self->image));
  if (group)
    gtk_size_group_add_widget (group, GTK_WIDGET (self->image));

  g_set_object (&self->icon_size_group, group);
}

static void
gtk_shortcuts_gesture_set_icon (GtkShortcutsGesture *self,
                                GIcon               *gicon)
{
  gtk_image_set_from_gicon (self->image, gicon, GTK_ICON_SIZE_DIALOG);
}

static void
gtk_shortcuts_gesture_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkShortcutsGesture *self = GTK_SHORTCUTS_GESTURE (object);

  switch (prop_id)
    {
    case PROP_SUBTITLE:
      g_value_set_string (value, gtk_label_get_label (self->subtitle));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    case PROP_ICON:
      {
        GIcon *icon;

        gtk_image_get_gicon (self->image, &icon, NULL);
        g_value_set_object (value, icon);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_gesture_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkShortcutsGesture *self = GTK_SHORTCUTS_GESTURE (object);

  switch (prop_id)
    {
    case PROP_DESC_SIZE_GROUP:
      gtk_shortcuts_gesture_set_desc_size_group (self, GTK_SIZE_GROUP (g_value_get_object (value)));
      break;

    case PROP_ICON:
      gtk_shortcuts_gesture_set_icon (self, g_value_get_object (value));
      break;

    case PROP_ICON_SIZE_GROUP:
      gtk_shortcuts_gesture_set_icon_size_group (self, GTK_SIZE_GROUP (g_value_get_object (value)));
      break;

    case PROP_SUBTITLE:
      gtk_label_set_label (self->subtitle, g_value_get_string (value));
      break;

    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_gesture_finalize (GObject *object)
{
  GtkShortcutsGesture *self = GTK_SHORTCUTS_GESTURE (object);

  g_clear_object (&self->desc_size_group);
  g_clear_object (&self->icon_size_group);

  G_OBJECT_CLASS (gtk_shortcuts_gesture_parent_class)->finalize (object);
}

static void
gtk_shortcuts_gesture_class_init (GtkShortcutsGestureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_shortcuts_gesture_finalize;
  object_class->get_property = gtk_shortcuts_gesture_get_property;
  object_class->set_property = gtk_shortcuts_gesture_set_property;

  properties[PROP_DESC_SIZE_GROUP] =
    g_param_spec_object ("desc-size-group",
                         P_("Description Size Group"),
                         P_("Description Size Group"),
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_ICON] =
    g_param_spec_object ("icon",
                         P_("Icon"),
                         P_("Icon"),
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_ICON_SIZE_GROUP] =
    g_param_spec_object ("icon-size-group",
                         P_("Icon Size Group"),
                         P_("Icon Size Group"),
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         P_("Subtitle"),
                         P_("Subtitle"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         P_("Title"),
                         P_("Title"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gtk_shortcuts_gesture_init (GtkShortcutsGesture *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (self), 12);

  self->image = g_object_new (GTK_TYPE_IMAGE,
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->image));

  self->desc_box = g_object_new (GTK_TYPE_BOX,
                                 "hexpand", TRUE,
                                 "orientation", GTK_ORIENTATION_VERTICAL,
                                 "visible", TRUE,
                                 NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->desc_box));

  self->title = g_object_new (GTK_TYPE_LABEL,
                              "visible", TRUE,
                              "xalign", 0.0f,
                              NULL);
  gtk_container_add (GTK_CONTAINER (self->desc_box), GTK_WIDGET (self->title));

  self->subtitle = g_object_new (GTK_TYPE_LABEL,
                                 "visible", TRUE,
                                 "xalign", 0.0f,
                                 NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self->subtitle)),
                               "dim-label");
  gtk_container_add (GTK_CONTAINER (self->desc_box), GTK_WIDGET (self->subtitle));
}
