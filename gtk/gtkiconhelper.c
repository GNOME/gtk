/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
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

#include "gtkiconhelperprivate.h"

#include <math.h>

#include "gtkcssenumvalueprivate.h"
#include "gtkcssiconthemevalueprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransientnodeprivate.h"
#include "gtkiconthemeprivate.h"
#include "gtkrendericonprivate.h"
#include "gtkscalerprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"

struct _GtkIconHelper
{
  GObject parent_instance;

  GtkImageDefinition *def;

  gint pixel_size;

  guint use_fallback : 1;
  guint force_scale_pixbuf : 1;
  guint texture_is_symbolic : 1;

  GtkWidget *owner;
  GtkCssNode *node;
  GdkPaintable *paintable;
};

static GtkIconLookupFlags
get_icon_lookup_flags (GtkIconHelper    *self,
                       GtkCssStyle      *style,
                       GtkTextDirection  dir)
{
  GtkIconLookupFlags flags;
  GtkCssIconStyle icon_style;

  flags = GTK_ICON_LOOKUP_USE_BUILTIN;

  if (self->pixel_size != -1 || self->force_scale_pixbuf)
    flags |= GTK_ICON_LOOKUP_FORCE_SIZE;

  icon_style = _gtk_css_icon_style_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_STYLE));

  switch (icon_style)
    {
    case GTK_CSS_ICON_STYLE_REGULAR:
      flags |= GTK_ICON_LOOKUP_FORCE_REGULAR;
      break;
    case GTK_CSS_ICON_STYLE_SYMBOLIC:
      flags |= GTK_ICON_LOOKUP_FORCE_SYMBOLIC;
      break;
    case GTK_CSS_ICON_STYLE_REQUESTED:
      break;
    default:
      g_assert_not_reached ();
      return 0;
    }

  if (dir == GTK_TEXT_DIR_LTR)
    flags |= GTK_ICON_LOOKUP_DIR_LTR;
  else if (dir == GTK_TEXT_DIR_RTL)
    flags |= GTK_ICON_LOOKUP_DIR_RTL;

  return flags;
}

static GdkPaintable *
ensure_paintable_for_gicon (GtkIconHelper    *self,
                            GtkCssStyle      *style,
                            GtkTextDirection  dir,
                            gint              scale,
                            GIcon            *gicon,
                            gboolean         *symbolic)
{
  GtkIconTheme *icon_theme;
  gint width, height;
  GtkIconInfo *info;
  GtkIconLookupFlags flags;
  GdkPaintable *paintable;

  icon_theme = gtk_css_icon_theme_value_get_icon_theme
    (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_THEME));
  flags = get_icon_lookup_flags (self, style, dir);

  width = height = gtk_icon_helper_get_size (self);

  info = gtk_icon_theme_lookup_by_gicon_for_scale (icon_theme,
                                                   gicon,
                                                   MIN (width, height),
                                                   scale, flags);
  if (info == NULL)
    info = gtk_icon_theme_lookup_icon (icon_theme,
                                       "image-missing",
                                       width,
                                       flags | GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_GENERIC_FALLBACK);

  *symbolic = gtk_icon_info_is_symbolic (info);
  paintable = gtk_icon_info_load_icon (info, NULL);
  g_object_unref (info);

  if (paintable && scale != 1)
    {
      GdkPaintable *orig = paintable;

      paintable = gtk_scaler_new (orig, scale);
      g_object_unref (orig);
    }

  return paintable;
}

static GdkPaintable *
gtk_icon_helper_load_paintable (GtkIconHelper   *self,
                                gboolean        *out_symbolic)
{
  GdkPaintable *paintable;
  GIcon *gicon;
  gboolean symbolic;

  switch (gtk_image_definition_get_storage_type (self->def))
    {
    case GTK_IMAGE_PAINTABLE:
      paintable = g_object_ref (gtk_image_definition_get_paintable (self->def));
      symbolic = FALSE;
      break;

    case GTK_IMAGE_ICON_NAME:
      if (self->use_fallback)
        gicon = g_themed_icon_new_with_default_fallbacks (gtk_image_definition_get_icon_name (self->def));
      else
        gicon = g_themed_icon_new (gtk_image_definition_get_icon_name (self->def));
      paintable = ensure_paintable_for_gicon (self,
                                              gtk_css_node_get_style (self->node),
                                              gtk_widget_get_direction (self->owner),
                                              gtk_widget_get_scale_factor (self->owner),
                                              gicon,
                                              &symbolic);
      g_object_unref (gicon);
      break;

    case GTK_IMAGE_GICON:
      paintable = ensure_paintable_for_gicon (self, 
                                              gtk_css_node_get_style (self->node),
                                              gtk_widget_get_direction (self->owner),
                                              gtk_widget_get_scale_factor (self->owner),
                                              gtk_image_definition_get_gicon (self->def),
                                              &symbolic);
      break;

    case GTK_IMAGE_EMPTY:
    default:
      paintable = NULL;
      symbolic = FALSE;
      break;
    }

  *out_symbolic = symbolic;

  return paintable;
}

static void
gtk_icon_helper_ensure_paintable (GtkIconHelper *self)
{
  gboolean symbolic;

  if (self->paintable)
    return;

  self->paintable = gtk_icon_helper_load_paintable (self, &symbolic);
  self->texture_is_symbolic = symbolic;
}

static void
gtk_icon_helper_paintable_snapshot (GdkPaintable *paintable,
                                    GdkSnapshot  *snapshot,
                                    double        width,
                                    double        height)
{
  GtkIconHelper *self = GTK_ICON_HELPER (paintable);
  GtkCssStyle *style;

  style = gtk_css_node_get_style (self->node);

  gtk_icon_helper_ensure_paintable (self);
  if (self->paintable == NULL)
    return;

  switch (gtk_image_definition_get_storage_type (self->def))
    {
    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
      {
        double x, y, w, h;

        /* Never scale up icons. */
        w = gdk_paintable_get_intrinsic_width (self->paintable);
        h = gdk_paintable_get_intrinsic_height (self->paintable);
        w = MIN (w, width);
        h = MIN (h, height);
        x = (width - w) / 2;
        y = (height - h) / 2;

        if (w == 0 || h == 0)
          return;

        if (x  != 0 || y != 0)
          {
            gtk_snapshot_save (snapshot);
            gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
            gtk_css_style_snapshot_icon_paintable (style,
                                                   snapshot,
                                                   self->paintable,
                                                   w, h,
                                                   self->texture_is_symbolic);
            gtk_snapshot_restore (snapshot);
          }
        else
          {
            gtk_css_style_snapshot_icon_paintable (style,
                                                   snapshot,
                                                   self->paintable,
                                                   w, h,
                                                   self->texture_is_symbolic);

          }

      }
      break;

    case GTK_IMAGE_EMPTY:
      break;

    case GTK_IMAGE_PAINTABLE:
    default:
      {
        double image_ratio = (double) width / height;
        double ratio;
        double x, y, w, h;

        if (self->paintable == NULL)
          break;

        ratio = gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);
        if (ratio == 0)
          {
            w = width;
            h = height;
          }
        else if (ratio > image_ratio)
          {
            w = width;
            h = width / ratio;
          }
        else
          {
            w = height * ratio;
            h = height;
          }

        x = floor (width - ceil (w)) / 2;
        y = floor (height - ceil (h)) / 2;

        if (x != 0 || y != 0)
          {
            gtk_snapshot_save (snapshot);
            gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
            gtk_css_style_snapshot_icon_paintable (style,
                                                   snapshot,
                                                   self->paintable,
                                                   w, h,
                                                   self->texture_is_symbolic);
            gtk_snapshot_restore (snapshot);
          }
        else
          {
            gtk_css_style_snapshot_icon_paintable (style,
                                                   snapshot,
                                                   self->paintable,
                                                   w, h,
                                                   self->texture_is_symbolic);
          }
      }
      break;
    }
}

static GdkPaintable *
gtk_icon_helper_paintable_get_current_image (GdkPaintable *paintable)
{
  GtkIconHelper *self = GTK_ICON_HELPER (paintable);

  gtk_icon_helper_ensure_paintable (self);
  if (self->paintable == NULL)
    return NULL;

  return gtk_icon_helper_paintable_get_current_image (self->paintable);
}

static int
gtk_icon_helper_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkIconHelper *self = GTK_ICON_HELPER (paintable);

  return gtk_icon_helper_get_size (self);
}

static int
gtk_icon_helper_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkIconHelper *self = GTK_ICON_HELPER (paintable);

  return gtk_icon_helper_get_size (self);
}

static double gtk_icon_helper_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  return 1.0;
};

static void
gtk_icon_helper_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_icon_helper_paintable_snapshot;
  iface->get_current_image = gtk_icon_helper_paintable_get_current_image;
  iface->get_intrinsic_width = gtk_icon_helper_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_icon_helper_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_icon_helper_paintable_get_intrinsic_aspect_ratio;
}

G_DEFINE_TYPE_WITH_CODE (GtkIconHelper, gtk_icon_helper, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
						gtk_icon_helper_paintable_init))

void
gtk_icon_helper_invalidate (GtkIconHelper *self)
{
  g_clear_object (&self->paintable);
  self->texture_is_symbolic = FALSE;

  if (!GTK_IS_CSS_TRANSIENT_NODE (self->node))
    gtk_widget_queue_resize (self->owner);
}

void
gtk_icon_helper_invalidate_for_change (GtkIconHelper     *self,
                                       GtkCssStyleChange *change)
{
  if (change == NULL ||
      ((gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_SYMBOLIC_ICON) &&
        self->texture_is_symbolic) ||
       (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_ICON) &&
        !self->texture_is_symbolic)))
    {
      /* Avoid the queue_resize in gtk_icon_helper_invalidate */
      g_clear_object (&self->paintable);
      self->texture_is_symbolic = FALSE;

      if (change == NULL ||
          (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_ICON_SIZE) &&
          !GTK_IS_CSS_TRANSIENT_NODE (self->node)))
        gtk_widget_queue_resize (self->owner);
    }
}

static void
gtk_icon_helper_take_definition (GtkIconHelper      *self,
                                 GtkImageDefinition *def)
{
  _gtk_icon_helper_clear (self);

  if (def == NULL)
    return;

  gtk_image_definition_unref (self->def);
  self->def = def;

  gtk_icon_helper_invalidate (self);
}

void
_gtk_icon_helper_clear (GtkIconHelper *self)
{
  g_clear_object (&self->paintable);
  self->texture_is_symbolic = FALSE;

  if (gtk_image_definition_get_storage_type (self->def) != GTK_IMAGE_EMPTY)
    {
      gtk_image_definition_unref (self->def);
      self->def = gtk_image_definition_new_empty ();
      gtk_icon_helper_invalidate (self);
    }
}

static void
gtk_icon_helper_finalize (GObject *object)
{
  GtkIconHelper *self = GTK_ICON_HELPER (object);

  _gtk_icon_helper_clear (self);
  g_signal_handlers_disconnect_by_func (self->owner, G_CALLBACK (gtk_icon_helper_invalidate), self);
  gtk_image_definition_unref (self->def);

  G_OBJECT_CLASS (gtk_icon_helper_parent_class)->finalize (object);
}

void
gtk_icon_helper_class_init (GtkIconHelperClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_icon_helper_finalize;
}

void
gtk_icon_helper_init (GtkIconHelper *self)
{
  self->def = gtk_image_definition_new_empty ();
}

GtkIconHelper *
gtk_icon_helper_new (GtkCssNode *css_node,
                     GtkWidget  *owner)
{
  GtkIconHelper *self;
  
  self = g_object_new (GTK_TYPE_ICON_HELPER, NULL);

  self->pixel_size = -1;
  self->texture_is_symbolic = FALSE;

  self->node = css_node;
  self->owner = owner;
  g_signal_connect_swapped (owner, "direction-changed", G_CALLBACK (gtk_icon_helper_invalidate), self);
  g_signal_connect_swapped (owner, "notify::scale-factor", G_CALLBACK (gtk_icon_helper_invalidate), self);

  return self;
}

int
gtk_icon_helper_get_size (GtkIconHelper *self)
{
  GtkCssStyle *style;

  if (self->pixel_size != -1)
    return self->pixel_size;

  style = gtk_css_node_get_style (self->node);
  return _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_SIZE), 100);
}

void
_gtk_icon_helper_set_definition (GtkIconHelper *self,
                                 GtkImageDefinition *def)
{
  if (def)
    gtk_icon_helper_take_definition (self, gtk_image_definition_ref (def));
  else
    _gtk_icon_helper_clear (self);
}

void
_gtk_icon_helper_set_gicon (GtkIconHelper *self,
                            GIcon         *gicon)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_gicon (gicon));
}

void
_gtk_icon_helper_set_icon_name (GtkIconHelper *self,
                                const gchar   *icon_name)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_icon_name (icon_name));
}

void
_gtk_icon_helper_set_paintable (GtkIconHelper *self,
			        GdkPaintable  *paintable)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_paintable (paintable));
}

gboolean
_gtk_icon_helper_set_pixel_size (GtkIconHelper *self,
                                 gint           pixel_size)
{
  if (self->pixel_size != pixel_size)
    {
      self->pixel_size = pixel_size;
      gtk_icon_helper_invalidate (self);
      return TRUE;
    }
  return FALSE;
}

gboolean
_gtk_icon_helper_set_use_fallback (GtkIconHelper *self,
                                   gboolean       use_fallback)
{
  if (self->use_fallback != use_fallback)
    {
      self->use_fallback = use_fallback;
      gtk_icon_helper_invalidate (self);
      return TRUE;
    }
  return FALSE;
}

GtkImageType
_gtk_icon_helper_get_storage_type (GtkIconHelper *self)
{
  return gtk_image_definition_get_storage_type (self->def);
}

gboolean
_gtk_icon_helper_get_use_fallback (GtkIconHelper *self)
{
  return self->use_fallback;
}

gint
_gtk_icon_helper_get_pixel_size (GtkIconHelper *self)
{
  return self->pixel_size;
}

GtkImageDefinition *
gtk_icon_helper_get_definition (GtkIconHelper *self)
{
  return self->def;
}

GIcon *
_gtk_icon_helper_peek_gicon (GtkIconHelper *self)
{
  return gtk_image_definition_get_gicon (self->def);
}

GdkPaintable *
_gtk_icon_helper_peek_paintable (GtkIconHelper *self)
{
  return gtk_image_definition_get_paintable (self->def);
}

const gchar *
_gtk_icon_helper_get_icon_name (GtkIconHelper *self)
{
  return gtk_image_definition_get_icon_name (self->def);
}

gboolean
_gtk_icon_helper_get_is_empty (GtkIconHelper *self)
{
  return gtk_image_definition_get_storage_type (self->def) == GTK_IMAGE_EMPTY;
}

gboolean
_gtk_icon_helper_get_force_scale_pixbuf (GtkIconHelper *self)
{
  return self->force_scale_pixbuf;
}

void
_gtk_icon_helper_set_force_scale_pixbuf (GtkIconHelper *self,
                                         gboolean       force_scale)
{
  if (self->force_scale_pixbuf != force_scale)
    {
      self->force_scale_pixbuf = force_scale;
      gtk_icon_helper_invalidate (self);
    }
}

void
gtk_icon_size_set_style_classes (GtkCssNode  *cssnode,
                                 GtkIconSize  icon_size)
{
  struct {
    GtkIconSize icon_size;
    const char *class_name;
  } class_names[] = {
    { GTK_ICON_SIZE_NORMAL, "normal-icons" },
    { GTK_ICON_SIZE_LARGE, "large-icons" }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (class_names); i++)
    {
      if (icon_size == class_names[i].icon_size)
        gtk_css_node_add_class (cssnode, g_quark_from_static_string (class_names[i].class_name));
      else
        gtk_css_node_remove_class (cssnode, g_quark_from_static_string (class_names[i].class_name));
    }
}
