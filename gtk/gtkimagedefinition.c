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

typedef struct _GtkImageDefinitionEmpty GtkImageDefinitionEmpty;
typedef struct _GtkImageDefinitionPixbuf GtkImageDefinitionPixbuf;
typedef struct _GtkImageDefinitionStock GtkImageDefinitionStock;
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
    sizeof (GtkImageDefinitionAnimation),
    sizeof (GtkImageDefinitionIconName),
    sizeof (GtkImageDefinitionGIcon),
    sizeof (GtkImageDefinitionSurface)
  };
  GtkImageDefinition *def;

  g_assert (type < G_N_ELEMENTS (sizes));

  def = g_malloc0 (sizes[type]);
  def->type = type;
  def->empty.ref_count = 1;

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_pixbuf (GdkPixbuf *pixbuf,
                                 int        scale)
{
  GtkImageDefinition *def;

  if (pixbuf == NULL || scale <= 0)
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_PIXBUF);
  def->pixbuf.pixbuf = g_object_ref (pixbuf);
  def->pixbuf.scale = scale;

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_animation (GdkPixbufAnimation *animation,
                                    int                 scale)
{
  GtkImageDefinition *def;

  if (animation == NULL || scale <= 0)
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_ANIMATION);
  def->animation.animation = g_object_ref (animation);
  def->animation.scale = scale;

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_icon_name (const char *icon_name)
{
  GtkImageDefinition *def;

  if (icon_name == NULL || icon_name[0] == '\0')
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_ICON_NAME);
  def->icon_name.icon_name = g_strdup (icon_name);

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_gicon (GIcon *gicon)
{
  GtkImageDefinition *def;

  if (gicon == NULL)
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_GICON);
  def->gicon.gicon = g_object_ref (gicon);

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_surface (cairo_surface_t *surface)
{
  GtkImageDefinition *def;

  if (surface == NULL)
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_SURFACE);
  def->surface.surface = cairo_surface_reference (surface);

  return def;
}

GtkImageDefinition *
gtk_image_definition_ref (GtkImageDefinition *def)
{
  def->empty.ref_count++;

  return def;
}

void
gtk_image_definition_unref (GtkImageDefinition *def)
{
  def->empty.ref_count--;

  if (def->empty.ref_count > 0)
    return;

  switch (def->type)
    {
    default:
    case GTK_IMAGE_EMPTY:
      g_assert_not_reached ();
      break;
    case GTK_IMAGE_PIXBUF:
      g_object_unref (def->pixbuf.pixbuf);
      break;
    case GTK_IMAGE_ANIMATION:
      g_object_unref (def->animation.animation);
      break;
    case GTK_IMAGE_SURFACE:
      cairo_surface_destroy (def->surface.surface);
      break;
    case GTK_IMAGE_ICON_NAME:
      g_free (def->icon_name.icon_name);
      break;
    case GTK_IMAGE_GICON:
      g_object_unref (def->gicon.gicon);
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
