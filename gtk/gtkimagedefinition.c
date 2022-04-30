/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Benjamin Otte <otte@gnome.org>
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

#include "gtkimagedefinitionprivate.h"

#include "deprecated/gtkiconfactory.h"

typedef struct _GtkImageDefinitionEmpty GtkImageDefinitionEmpty;
typedef struct _GtkImageDefinitionPixbuf GtkImageDefinitionPixbuf;
typedef struct _GtkImageDefinitionStock GtkImageDefinitionStock;
typedef struct _GtkImageDefinitionIconSet GtkImageDefinitionIconSet;
typedef struct _GtkImageDefinitionAnimation GtkImageDefinitionAnimation;
typedef struct _GtkImageDefinitionIconName GtkImageDefinitionIconName;
typedef struct _GtkImageDefinitionGIcon GtkImageDefinitionGIcon;
typedef struct _GtkImageDefinitionSurface GtkImageDefinitionSurface;

struct _GtkImageDefinitionEmpty {
  GtkImageType type;
  gint ref_count;
};

struct _GtkImageDefinitionPixbuf {
  GtkImageType type;
  gint ref_count;

  GdkPixbuf *pixbuf;
  int scale;
};

struct _GtkImageDefinitionStock {
  GtkImageType type;
  gint ref_count;

  char *id;
};

struct _GtkImageDefinitionIconSet {
  GtkImageType type;
  gint ref_count;

  GtkIconSet *icon_set;
};

struct _GtkImageDefinitionAnimation {
  GtkImageType type;
  gint ref_count;

  GdkPixbufAnimation *animation;
  int scale;
};

struct _GtkImageDefinitionIconName {
  GtkImageType type;
  gint ref_count;

  char *icon_name;
};

struct _GtkImageDefinitionGIcon {
  GtkImageType type;
  gint ref_count;

  GIcon *gicon;
};

struct _GtkImageDefinitionSurface {
  GtkImageType type;
  gint ref_count;

  cairo_surface_t *surface;
};

union _GtkImageDefinition
{
  GtkImageType type;
  GtkImageDefinitionEmpty empty;
  GtkImageDefinitionPixbuf pixbuf;
  GtkImageDefinitionStock stock;
  GtkImageDefinitionIconSet icon_set;
  GtkImageDefinitionAnimation animation;
  GtkImageDefinitionIconName icon_name;
  GtkImageDefinitionGIcon gicon;
  GtkImageDefinitionSurface surface;
};

GtkImageDefinition *
gtk_image_definition_new_empty (void)
{
  static GtkImageDefinitionEmpty empty = { GTK_IMAGE_EMPTY, 1 };

  return gtk_image_definition_ref ((GtkImageDefinition *) &empty);
}

static inline GtkImageDefinition *
gtk_image_definition_alloc (GtkImageType type)
{
  static gsize sizes[] = {
    sizeof (GtkImageDefinitionEmpty),
    sizeof (GtkImageDefinitionPixbuf),
    sizeof (GtkImageDefinitionStock),
    sizeof (GtkImageDefinitionIconSet),
    sizeof (GtkImageDefinitionAnimation),
    sizeof (GtkImageDefinitionIconName),
    sizeof (GtkImageDefinitionGIcon),
    sizeof (GtkImageDefinitionSurface)
  };
  GtkImageDefinition *def;
  GtkImageDefinitionEmpty *empty_def;

  g_assert (type < G_N_ELEMENTS (sizes));

  def = g_malloc0 (sizes[type]);
  empty_def = (GtkImageDefinitionEmpty *) def;
  empty_def->type = type;
  empty_def->ref_count = 1;

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_pixbuf (GdkPixbuf *pixbuf,
                                 int        scale)
{
  GtkImageDefinition *def;
  GtkImageDefinitionPixbuf *pixbuf_def;

  if (pixbuf == NULL || scale <= 0)
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_PIXBUF);
  pixbuf_def = (GtkImageDefinitionPixbuf *) def;
  pixbuf_def->pixbuf = g_object_ref (pixbuf);
  pixbuf_def->scale = scale;

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_stock (const char *stock_id)
{
  GtkImageDefinition *def;
  GtkImageDefinitionStock *stock_def;

  if (stock_id == NULL || stock_id[0] == '\0')
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_STOCK);
  stock_def = (GtkImageDefinitionStock *) def;
  stock_def->id = g_strdup (stock_id);

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_icon_set (GtkIconSet *icon_set)
{
  GtkImageDefinition *def;
  GtkImageDefinitionIconSet *icon_set_def;

  if (icon_set == NULL)
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_ICON_SET);
  icon_set_def = (GtkImageDefinitionIconSet *) def;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  icon_set_def->icon_set = gtk_icon_set_ref (icon_set);
G_GNUC_END_IGNORE_DEPRECATIONS;

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_animation (GdkPixbufAnimation *animation,
                                    int                 scale)
{
  GtkImageDefinition *def;
  GtkImageDefinitionAnimation *animation_def;

  if (animation == NULL || scale <= 0)
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_ANIMATION);
  animation_def = (GtkImageDefinitionAnimation *) def;
  animation_def->animation = g_object_ref (animation);
  animation_def->scale = scale;

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_icon_name (const char *icon_name)
{
  GtkImageDefinition *def;
  GtkImageDefinitionIconName *icon_name_def;

  if (icon_name == NULL || icon_name[0] == '\0')
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_ICON_NAME);
  icon_name_def = (GtkImageDefinitionIconName *) def;
  icon_name_def->icon_name = g_strdup (icon_name);

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_gicon (GIcon *gicon)
{
  GtkImageDefinition *def;
  GtkImageDefinitionGIcon *gicon_def;

  if (gicon == NULL)
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_GICON);
  gicon_def = (GtkImageDefinitionGIcon *) def;
  gicon_def->gicon = g_object_ref (gicon);

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_surface (cairo_surface_t *surface)
{
  GtkImageDefinition *def;
  GtkImageDefinitionSurface *surface_def;

  if (surface == NULL)
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_SURFACE);
  surface_def = (GtkImageDefinitionSurface *) def;
  surface_def->surface = cairo_surface_reference (surface);

  return def;
}

GtkImageDefinition *
gtk_image_definition_ref (GtkImageDefinition *def)
{
  GtkImageDefinitionEmpty *empty_def;

  empty_def = (GtkImageDefinitionEmpty *) def;
  empty_def->ref_count++;

  return def;
}

void
gtk_image_definition_unref (GtkImageDefinition *def)
{
  GtkImageDefinitionEmpty *empty_def;
  GtkImageDefinitionPixbuf *pixbuf_def;
  GtkImageDefinitionAnimation *animation_def;
  GtkImageDefinitionSurface *surface_def;
  GtkImageDefinitionStock *stock_def;
  GtkImageDefinitionIconSet *icon_set_def;
  GtkImageDefinitionIconName *icon_name_def;
  GtkImageDefinitionGIcon *gicon_def;

  empty_def = (GtkImageDefinitionEmpty *) def;
  empty_def->ref_count--;

  if (empty_def->ref_count > 0)
    return;

  switch (def->type)
    {
    default:
    case GTK_IMAGE_EMPTY:
      g_assert_not_reached ();
      break;
    case GTK_IMAGE_PIXBUF:
      pixbuf_def = (GtkImageDefinitionPixbuf *) def;
      g_object_unref (pixbuf_def->pixbuf);
      break;
    case GTK_IMAGE_ANIMATION:
      animation_def = (GtkImageDefinitionAnimation *) def;
      g_object_unref (animation_def->animation);
      break;
    case GTK_IMAGE_SURFACE:
      surface_def = (GtkImageDefinitionSurface *) def;
      cairo_surface_destroy (surface_def->surface);
      break;
    case GTK_IMAGE_STOCK:
      stock_def = (GtkImageDefinitionStock *) def;
      g_free (stock_def->id);
      break;
    case GTK_IMAGE_ICON_SET:
      icon_set_def = (GtkImageDefinitionIconSet *) def;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_icon_set_unref (icon_set_def->icon_set);
G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case GTK_IMAGE_ICON_NAME:
      icon_name_def = (GtkImageDefinitionIconName *) def;
      g_free (icon_name_def->icon_name);
      break;
    case GTK_IMAGE_GICON:
      gicon_def = (GtkImageDefinitionGIcon *) def;
      g_object_unref (gicon_def->gicon);
      break;
    }

  g_free (def);
}

GtkImageType
gtk_image_definition_get_storage_type (const GtkImageDefinition *def)
{
  return def->type;
}

gint
gtk_image_definition_get_scale (const GtkImageDefinition *def)
{
  switch (def->type)
    {
    default:
      g_assert_not_reached ();
    case GTK_IMAGE_EMPTY:
    case GTK_IMAGE_SURFACE:
    case GTK_IMAGE_STOCK:
    case GTK_IMAGE_ICON_SET:
    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
      return 1;
    case GTK_IMAGE_PIXBUF:
      return def->pixbuf.scale;
    case GTK_IMAGE_ANIMATION:
      return def->animation.scale;
    }
}

GdkPixbuf *
gtk_image_definition_get_pixbuf (const GtkImageDefinition *def)
{
  if (def->type != GTK_IMAGE_PIXBUF)
    return NULL;

  return def->pixbuf.pixbuf;
}

const gchar *
gtk_image_definition_get_stock (const GtkImageDefinition *def)
{
  if (def->type != GTK_IMAGE_STOCK)
    return NULL;

  return def->stock.id;
}

GtkIconSet *
gtk_image_definition_get_icon_set (const GtkImageDefinition *def)
{
  if (def->type != GTK_IMAGE_ICON_SET)
    return NULL;

  return def->icon_set.icon_set;
}

GdkPixbufAnimation *
gtk_image_definition_get_animation (const GtkImageDefinition *def)
{
  if (def->type != GTK_IMAGE_ANIMATION)
    return NULL;

  return def->animation.animation;
}

const gchar *
gtk_image_definition_get_icon_name (const GtkImageDefinition *def)
{
  if (def->type != GTK_IMAGE_ICON_NAME)
    return NULL;

  return def->icon_name.icon_name;
}

GIcon *
gtk_image_definition_get_gicon (const GtkImageDefinition *def)
{
  if (def->type != GTK_IMAGE_GICON)
    return NULL;

  return def->gicon.gicon;
}

cairo_surface_t *
gtk_image_definition_get_surface (const GtkImageDefinition *def)
{
  if (def->type != GTK_IMAGE_SURFACE)
    return NULL;

  return def->surface.surface;
}
