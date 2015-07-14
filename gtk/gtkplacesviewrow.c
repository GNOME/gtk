/* gtkplacesviewrow.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "gtkintl.h"
#include "gtkplacesviewrowprivate.h"
#include "gtktypebuiltins.h"

struct _GtkPlacesViewRow
{
  GtkListBoxRow  parent_instance;

  GtkSpinner    *busy_spinner;
  GtkButton     *eject_button;
  GtkEventBox   *event_box;
  GtkImage      *icon_image;
  GtkLabel      *name_label;
  GtkLabel      *path_label;

  GVolume       *volume;
  GMount        *mount;
};

G_DEFINE_TYPE (GtkPlacesViewRow, gtk_places_view_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_ICON,
  PROP_NAME,
  PROP_PATH,
  PROP_VOLUME,
  PROP_MOUNT,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
gtk_places_view_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkPlacesViewRow *self;
  GIcon *icon;

  self = GTK_PLACES_VIEW_ROW (object);
  icon = NULL;

  switch (prop_id)
    {
    case PROP_ICON:
      gtk_image_get_gicon (self->icon_image, &icon, NULL);
      g_value_set_object (value, icon);
      break;

    case PROP_NAME:
      g_value_set_string (value, gtk_label_get_label (self->name_label));
      break;

    case PROP_PATH:
      g_value_set_string (value, gtk_label_get_label (self->path_label));
      break;

    case PROP_VOLUME:
      g_value_set_object (value, self->volume);
      break;

    case PROP_MOUNT:
      g_value_set_object (value, self->mount);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_places_view_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkPlacesViewRow *self = GTK_PLACES_VIEW_ROW (object);

  switch (prop_id)
    {
    case PROP_ICON:
      gtk_image_set_from_gicon (self->icon_image,
                                g_value_get_object (value),
                                GTK_ICON_SIZE_LARGE_TOOLBAR);
      break;

    case PROP_NAME:
      gtk_label_set_label (self->name_label, g_value_get_string (value));
      break;

    case PROP_PATH:
      gtk_label_set_label (self->path_label, g_value_get_string (value));
      break;

    case PROP_VOLUME:
      self->volume = g_value_get_object (value);
      break;

    case PROP_MOUNT:
      self->mount = g_value_get_object (value);
      gtk_widget_set_visible (GTK_WIDGET (self->eject_button), self->mount != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_places_view_row_class_init (GtkPlacesViewRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gtk_places_view_row_get_property;
  object_class->set_property = gtk_places_view_row_set_property;

  properties[PROP_ICON] =
          g_param_spec_object ("icon",
                               P_("Icon of the row"),
                               P_("The icon representing the volume"),
                               G_TYPE_ICON,
                               G_PARAM_READWRITE);

  properties[PROP_NAME] =
          g_param_spec_string ("name",
                               P_("Name of the volume"),
                               P_("The name of the volume"),
                               "",
                               G_PARAM_READWRITE);

  properties[PROP_PATH] =
          g_param_spec_string ("path",
                               P_("Path of the volume"),
                               P_("The path of the volume"),
                               "",
                               G_PARAM_READWRITE);

  properties[PROP_VOLUME] =
          g_param_spec_object ("volume",
                               P_("Volume represented by the row"),
                               P_("The volume represented by the row"),
                               G_TYPE_VOLUME,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_MOUNT] =
          g_param_spec_object ("mount",
                               P_("Mount represented by the row"),
                               P_("The mount point represented by the row, if any"),
                               G_TYPE_MOUNT,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkplacesviewrow.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, busy_spinner);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, eject_button);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, event_box);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, icon_image);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, path_label);
}

static void
gtk_places_view_row_init (GtkPlacesViewRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gtk_places_view_row_new (GVolume *volume,
                         GMount  *mount)
{
  return g_object_new (GTK_TYPE_PLACES_VIEW_ROW,
                       "volume", volume,
                       "mount", mount,
                       NULL);
}

GMount*
gtk_places_view_row_get_mount (GtkPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW_ROW (row), NULL);

  return row->mount;
}

GVolume*
gtk_places_view_row_get_volume (GtkPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW_ROW (row), NULL);

  return row->volume;
}

GtkWidget*
gtk_places_view_row_get_eject_button (GtkPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW_ROW (row), NULL);

  return GTK_WIDGET (row->eject_button);
}

GtkWidget*
gtk_places_view_row_get_event_box (GtkPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW_ROW (row), NULL);

  return GTK_WIDGET (row->event_box);
}

void
gtk_places_view_row_set_busy (GtkPlacesViewRow *row,
                              gboolean          is_busy)
{
  g_return_if_fail (GTK_IS_PLACES_VIEW_ROW (row));

  gtk_widget_set_visible (GTK_WIDGET (row->busy_spinner), is_busy);
}
