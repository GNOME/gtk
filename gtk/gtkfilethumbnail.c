/* gtkfilethumbnail.c
 *
 * Copyright 2022 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "config.h"

#include "gtkfilethumbnail.h"

#include "gtkbinlayout.h"
#include "gtkfilechooserutils.h"
#include "gtkimage.h"
#include "gtkprivate.h"
#include "gtkwidget.h"

#define ICON_SIZE 16

struct _GtkFileThumbnail
{
  GtkWidget parent;

  GtkWidget *image;

  GtkFileSystemItem *item;
  GCancellable *cancellable;
};

typedef struct
{
  GtkWidgetClass parent;
} GtkFileThumbnailClass;

G_DEFINE_FINAL_TYPE (GtkFileThumbnail, _gtk_file_thumbnail, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_ITEM,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];

static void
copy_attribute (GFileInfo  *to,
                GFileInfo  *from,
                const char *attribute)
{
  GFileAttributeType type;
  gpointer value;

  if (g_file_info_get_attribute_data (from, attribute, &type, &value, NULL))
    g_file_info_set_attribute (to, attribute, type, value);
}

static gboolean
update_image (GtkFileThumbnail *self)
{
  GtkIconTheme *icon_theme;
  GFileInfo *info;
  GIcon *icon;
  int scale;

  info = _gtk_file_system_item_get_file_info (self->item);
  if (!g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_ICON))
    return FALSE;

  scale = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (self)));

  icon = _gtk_file_info_get_icon (info, ICON_SIZE, scale, icon_theme);

  gtk_image_set_from_gicon (GTK_IMAGE (self->image), icon);

  g_object_unref (icon);

  return TRUE;

}

static void
thumbnail_queried_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GtkFileThumbnail *self = user_data; /* might be unreffed if operation was cancelled */
  GFile *file = G_FILE (object);
  GFileInfo *queried;
  GFileInfo *info;

  queried = g_file_query_info_finish (file, result, NULL);
  if (queried == NULL)
    return;

  info = _gtk_file_system_item_get_file_info (self->item);
  copy_attribute (info, queried, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  copy_attribute (info, queried, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
  copy_attribute (info, queried, G_FILE_ATTRIBUTE_STANDARD_ICON);

  update_image (self);

  g_clear_object (&queried);

  g_clear_object (&self->cancellable);
}

static void
cancel_thumbnail (GtkFileThumbnail *self)
{
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
}

static void
get_thumbnail (GtkFileThumbnail *self)
{
  if (!self->item)
    return;

  if (!update_image (self))
    {
      GFileInfo *info;
      GFile *file;

      info = _gtk_file_system_item_get_file_info (self->item);
      if (g_file_info_has_attribute (info, "filechooser::queried"))
        return;

      g_assert (self->cancellable == NULL);
      self->cancellable = g_cancellable_new ();

      file = _gtk_file_system_item_get_file (self->item);
      g_file_info_set_attribute_boolean (info, "filechooser::queried", TRUE);
      g_file_query_info_async (file,
                               G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","
                               G_FILE_ATTRIBUTE_THUMBNAILING_FAILED ","
                               G_FILE_ATTRIBUTE_STANDARD_ICON,
                               G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT,
                               self->cancellable,
                               thumbnail_queried_cb,
                               self);
    }
}

static void
_gtk_file_thumbnail_dispose (GObject *object)
{
  GtkFileThumbnail *self = (GtkFileThumbnail *)object;

  _gtk_file_thumbnail_set_item (self, NULL);

  g_clear_pointer (&self->image, gtk_widget_unparent);

  G_OBJECT_CLASS (_gtk_file_thumbnail_parent_class)->dispose (object);
}

static void
_gtk_file_thumbnail_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkFileThumbnail *self = GTK_FILE_THUMBNAIL (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
_gtk_file_thumbnail_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkFileThumbnail *self = GTK_FILE_THUMBNAIL (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      _gtk_file_thumbnail_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
_gtk_file_thumbnail_class_init (GtkFileThumbnailClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = _gtk_file_thumbnail_dispose;
  object_class->get_property = _gtk_file_thumbnail_get_property;
  object_class->set_property = _gtk_file_thumbnail_set_property;

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         GTK_TYPE_FILE_SYSTEM_ITEM,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, I_("filethumbnail"));

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
_gtk_file_thumbnail_init (GtkFileThumbnail *self)
{
  self->image = gtk_image_new ();
  gtk_widget_set_parent (self->image, GTK_WIDGET (self));
}

GtkFileSystemItem *
_gtk_file_thumbnail_get_item (GtkFileThumbnail *self)
{
  g_assert (GTK_IS_FILE_THUMBNAIL (self));

  return self->item;
}

void
_gtk_file_thumbnail_set_item (GtkFileThumbnail  *self,
                              GtkFileSystemItem *item)
{
  g_assert (GTK_IS_FILE_THUMBNAIL (self));
  g_assert (item == NULL || GTK_IS_FILE_SYSTEM_ITEM (item));

  if (g_set_object (&self->item, item))
    {
      cancel_thumbnail (self);
      get_thumbnail (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
    }
}

