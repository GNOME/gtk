/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <math.h>
#include <gtk/gtk.h>

#include <gtk/gtkthemingengine.h>
#include <gtk/gtkstylecontext.h>
#include <gtk/gtkintl.h>

#include "gtkpango.h"

typedef struct GtkThemingEnginePrivate GtkThemingEnginePrivate;

enum {
  SIDE_LEFT   = 1,
  SIDE_BOTTOM = 1 << 1,
  SIDE_RIGHT  = 1 << 2,
  SIDE_TOP    = 1 << 3
};

struct GtkThemingEnginePrivate
{
  GtkStyleContext *context;
};

#define GTK_THEMING_ENGINE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_THEMING_ENGINE, GtkThemingEnginePrivate))

static void gtk_theming_engine_render_check (GtkThemingEngine *engine,
                                             cairo_t          *cr,
                                             gdouble           x,
                                             gdouble           y,
                                             gdouble           width,
                                             gdouble           height);
static void gtk_theming_engine_render_option (GtkThemingEngine *engine,
                                              cairo_t          *cr,
                                              gdouble           x,
                                              gdouble           y,
                                              gdouble           width,
                                              gdouble           height);
static void gtk_theming_engine_render_arrow  (GtkThemingEngine *engine,
                                              cairo_t          *cr,
                                              gdouble           angle,
                                              gdouble           x,
                                              gdouble           y,
                                              gdouble           size);
static void gtk_theming_engine_render_background (GtkThemingEngine *engine,
                                                  cairo_t          *cr,
                                                  gdouble           x,
                                                  gdouble           y,
                                                  gdouble           width,
                                                  gdouble           height);
static void gtk_theming_engine_render_frame  (GtkThemingEngine *engine,
                                              cairo_t          *cr,
                                              gdouble           x,
                                              gdouble           y,
                                              gdouble           width,
                                              gdouble           height);
static void gtk_theming_engine_render_expander (GtkThemingEngine *engine,
                                                cairo_t          *cr,
                                                gdouble           x,
                                                gdouble           y,
                                                gdouble           width,
                                                gdouble           height);
static void gtk_theming_engine_render_focus    (GtkThemingEngine *engine,
                                                cairo_t          *cr,
                                                gdouble           x,
                                                gdouble           y,
                                                gdouble           width,
                                                gdouble           height);
static void gtk_theming_engine_render_layout   (GtkThemingEngine *engine,
                                                cairo_t          *cr,
                                                gdouble           x,
                                                gdouble           y,
                                                PangoLayout      *layout);
static void gtk_theming_engine_render_line     (GtkThemingEngine *engine,
                                                cairo_t          *cr,
                                                gdouble           x0,
                                                gdouble           y0,
                                                gdouble           x1,
                                                gdouble           y1);
static void gtk_theming_engine_render_slider   (GtkThemingEngine *engine,
                                                cairo_t          *cr,
                                                gdouble           x,
                                                gdouble           y,
                                                gdouble           width,
                                                gdouble           height,
                                                GtkOrientation    orientation);
static void gtk_theming_engine_render_frame_gap (GtkThemingEngine *engine,
                                                 cairo_t          *cr,
                                                 gdouble           x,
                                                 gdouble           y,
                                                 gdouble           width,
                                                 gdouble           height,
                                                 GtkPositionType   gap_side,
                                                 gdouble           xy0_gap,
                                                 gdouble           xy1_gap);
static void gtk_theming_engine_render_extension (GtkThemingEngine *engine,
                                                 cairo_t          *cr,
                                                 gdouble           x,
                                                 gdouble           y,
                                                 gdouble           width,
                                                 gdouble           height,
                                                 GtkPositionType   gap_side);
static void gtk_theming_engine_render_handle    (GtkThemingEngine *engine,
                                                 cairo_t          *cr,
                                                 gdouble           x,
                                                 gdouble           y,
                                                 gdouble           width,
                                                 gdouble           height,
                                                 GtkOrientation    orientation);

G_DEFINE_TYPE (GtkThemingEngine, gtk_theming_engine, G_TYPE_OBJECT)


typedef struct GtkThemingModule GtkThemingModule;
typedef struct GtkThemingModuleClass GtkThemingModuleClass;

struct GtkThemingModule
{
  GTypeModule parent_instance;
  gchar *name;

  GtkThemingEngine * (*create_engine) (void);
};

struct GtkThemingModuleClass
{
  GTypeModuleClass parent_class;
};

#define GTK_TYPE_THEMING_MODULE  (gtk_theming_module_get_type ())
#define GTK_THEMING_MODULE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_THEMING_MODULE, GtkThemingModule))
#define GTK_IS_THEMING_MODULE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_THEMING_MODULE))

G_DEFINE_TYPE (GtkThemingModule, gtk_theming_module, G_TYPE_TYPE_MODULE);

static void
gtk_theming_engine_class_init (GtkThemingEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->render_check = gtk_theming_engine_render_check;
  klass->render_option = gtk_theming_engine_render_option;
  klass->render_arrow = gtk_theming_engine_render_arrow;
  klass->render_background = gtk_theming_engine_render_background;
  klass->render_frame = gtk_theming_engine_render_frame;
  klass->render_expander = gtk_theming_engine_render_expander;
  klass->render_focus = gtk_theming_engine_render_focus;
  klass->render_layout = gtk_theming_engine_render_layout;
  klass->render_line = gtk_theming_engine_render_line;
  klass->render_slider = gtk_theming_engine_render_slider;
  klass->render_frame_gap = gtk_theming_engine_render_frame_gap;
  klass->render_extension = gtk_theming_engine_render_extension;
  klass->render_handle = gtk_theming_engine_render_handle;

  g_type_class_add_private (object_class, sizeof (GtkThemingEnginePrivate));
}

static void
gtk_theming_engine_init (GtkThemingEngine *engine)
{
  engine->priv = GTK_THEMING_ENGINE_GET_PRIVATE (engine);
}

void
_gtk_theming_engine_set_context (GtkThemingEngine *engine,
                                 GtkStyleContext  *context)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = engine->priv;
  priv->context = context;
}

void
gtk_theming_engine_register_property (GtkThemingEngine       *engine,
                                      const gchar            *property_name,
                                      GType                   type,
                                      const GValue           *default_value,
                                      GtkStylePropertyParser  parse_func)
{
  gchar *name;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (type != G_TYPE_INVALID);
  g_return_if_fail (default_value != NULL && G_IS_VALUE (default_value));

  name = g_strdup_printf ("-%s-%s", G_OBJECT_TYPE_NAME (engine), property_name);
  gtk_style_set_register_property (name, type, default_value, parse_func);
  g_free (name);
}

void
gtk_theming_engine_get_property (GtkThemingEngine *engine,
                                 const gchar      *property,
                                 GtkStateFlags     state,
                                 GValue           *value)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));
  g_return_if_fail (property != NULL);
  g_return_if_fail (value != NULL);

  priv = engine->priv;
  gtk_style_context_get_property (priv->context, property, state, value);
}

void
gtk_theming_engine_get_valist (GtkThemingEngine *engine,
                               GtkStateFlags     state,
                               va_list           args)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));

  priv = engine->priv;
  gtk_style_context_get_valist (priv->context, state, args);
}

void
gtk_theming_engine_get (GtkThemingEngine *engine,
                        GtkStateFlags     state,
                        ...)
{
  GtkThemingEnginePrivate *priv;
  va_list args;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));

  priv = engine->priv;

  va_start (args, state);
  gtk_style_context_get_valist (priv->context, state, args);
  va_end (args);
}

void
gtk_theming_engine_get_style_property (GtkThemingEngine *engine,
                                       const gchar      *property_name,
                                       GValue           *value)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));
  g_return_if_fail (property_name != NULL);

  priv = engine->priv;
  gtk_style_context_get_style_property (priv->context, property_name, value);
}

void
gtk_theming_engine_get_style_valist (GtkThemingEngine *engine,
                                     va_list           args)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));

  priv = engine->priv;
  gtk_style_context_get_style_valist (priv->context, args);
}

void
gtk_theming_engine_get_style (GtkThemingEngine *engine,
                              ...)
{
  GtkThemingEnginePrivate *priv;
  va_list args;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));

  priv = engine->priv;

  va_start (args, engine);
  gtk_style_context_get_style_valist (priv->context, args);
  va_end (args);
}

GtkStateFlags
gtk_theming_engine_get_state (GtkThemingEngine *engine)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), 0);

  priv = engine->priv;
  return gtk_style_context_get_state (priv->context);
}

gboolean
gtk_theming_engine_is_state_set (GtkThemingEngine *engine,
                                 GtkStateType      state,
                                 gdouble          *progress)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), 0);

  priv = engine->priv;
  return gtk_style_context_is_state_set (priv->context, state, progress);
}

G_CONST_RETURN GtkWidgetPath *
gtk_theming_engine_get_path (GtkThemingEngine *engine)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), NULL);

  priv = engine->priv;
  return gtk_style_context_get_path (priv->context);
}

gboolean
gtk_theming_engine_has_class (GtkThemingEngine *engine,
                              const gchar      *style_class)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), FALSE);

  priv = engine->priv;
  return gtk_style_context_has_class (priv->context, style_class);
}

gboolean
gtk_theming_engine_has_region (GtkThemingEngine *engine,
                               const gchar      *style_class,
                               GtkRegionFlags   *flags)
{
  GtkThemingEnginePrivate *priv;

  if (flags)
    *flags = 0;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), FALSE);

  priv = engine->priv;
  return gtk_style_context_has_region (priv->context, style_class, flags);
}

GtkTextDirection
gtk_theming_engine_get_direction (GtkThemingEngine *engine)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), GTK_TEXT_DIR_LTR);

  priv = engine->priv;
  return gtk_style_context_get_direction (priv->context);
}

GtkJunctionSides
gtk_theming_engine_get_junction_sides (GtkThemingEngine *engine)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), 0);

  priv = engine->priv;
  return gtk_style_context_get_junction_sides (priv->context);
}

/* GtkThemingModule */

static gboolean
gtk_theming_module_load (GTypeModule *type_module)
{
  GtkThemingModule *theming_module;
  GModule *module;
  gchar *name, *module_path;

  theming_module = GTK_THEMING_MODULE (type_module);
  name = theming_module->name;
  module_path = _gtk_find_module (name, "theming-engines");

  if (!module_path)
    {
      g_warning (_("Unable to locate theme engine in module path: \"%s\","), name);
      return FALSE;
    }

  module = g_module_open (module_path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  g_free (module_path);

  if (!module)
    {
      g_warning ("%s", g_module_error ());
      return FALSE;
    }

  if (!g_module_symbol (module, "create_engine",
                        (gpointer *) &theming_module->create_engine))
    {
      g_warning ("%s", g_module_error());
      g_module_close (module);

      return FALSE;
    }

  g_module_make_resident (module);

  return TRUE;
}

static void
gtk_theming_module_class_init (GtkThemingModuleClass *klass)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (klass);

  module_class->load = gtk_theming_module_load;
}

static void
gtk_theming_module_init (GtkThemingModule *module)
{
}

G_CONST_RETURN GtkThemingEngine *
gtk_theming_engine_load (const gchar *name)
{
  static GHashTable *engines = NULL;
  static GtkThemingEngine *default_engine;
  GtkThemingEngine *engine = NULL;

  if (name)
    {
      if (!engines)
        engines = g_hash_table_new (g_str_hash, g_str_equal);

      engine = g_hash_table_lookup (engines, name);

      if (!engine)
        {
          GtkThemingModule *module;

          module = g_object_new (GTK_TYPE_THEMING_MODULE, NULL);
          g_type_module_set_name (G_TYPE_MODULE (module), name);
          module->name = g_strdup (name);

          if (module && g_type_module_use (G_TYPE_MODULE (module)))
            {
              engine = (module->create_engine) ();

              if (engine)
                g_hash_table_insert (engines, module->name, engine);
            }
        }
    }

  if (!engine)
    {
      if (G_UNLIKELY (!default_engine))
        default_engine = g_object_new (GTK_TYPE_THEMING_ENGINE, NULL);

      engine = default_engine;
    }

  return engine;
}

GdkScreen *
gtk_theming_engine_get_screen (GtkThemingEngine *engine)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), NULL);

  priv = engine->priv;
  return gtk_style_context_get_screen (priv->context);
}

/* Paint method implementations */
static void
gtk_theming_engine_render_check (GtkThemingEngine *engine,
                                 cairo_t          *cr,
                                 gdouble           x,
                                 gdouble           y,
                                 gdouble           width,
                                 gdouble           height)
{
  GdkColor *fg_color, *base_color, *text_color;
  const GtkWidgetPath *path;
  GtkStateFlags flags;
  gint exterior_size, interior_size, thickness, pad;

  flags = gtk_theming_engine_get_state (engine);
  path = gtk_theming_engine_get_path (engine);
  cairo_save (cr);

  gtk_theming_engine_get (engine, flags,
                          "foreground-color", &fg_color,
                          "base-color", &base_color,
                          "text-color", &text_color,
                          NULL);

  exterior_size = MIN (width, height);

  if (exterior_size % 2 == 0) /* Ensure odd */
    exterior_size -= 1;

  /* FIXME: thickness */
  thickness = 1;
  pad = thickness + MAX (1, (exterior_size - 2 * thickness) / 9);
  interior_size = MAX (1, exterior_size - 2 * pad);

  if (interior_size < 7)
    {
      interior_size = 7;
      pad = MAX (0, (exterior_size - interior_size) / 2);
    }

  x -= (1 + exterior_size - (gint) width) / 2;
  y -= (1 + exterior_size - (gint) height) / 2;

  if (!gtk_theming_engine_has_class (engine, "menu"))
    {
      cairo_set_line_width (cr, 1.0);

      cairo_rectangle (cr, x + 0.5, y + 0.5, exterior_size - 1, exterior_size - 1);
      gdk_cairo_set_source_color (cr, base_color);
      cairo_fill_preserve (cr);

      if (gtk_theming_engine_has_class (engine, "cell"))
	gdk_cairo_set_source_color (cr, text_color);
      else
	gdk_cairo_set_source_color (cr, fg_color);

      cairo_stroke (cr);
    }

  if (gtk_theming_engine_has_class (engine, "menu"))
    gdk_cairo_set_source_color (cr, fg_color);
  else
    gdk_cairo_set_source_color (cr, text_color);

  if (gtk_theming_engine_is_state_set (engine, GTK_STATE_INCONSISTENT, NULL))
    {
      int line_thickness = MAX (1, (3 + interior_size * 2) / 7);

      cairo_rectangle (cr,
		       x + pad,
		       y + pad + (1 + interior_size - line_thickness) / 2,
		       interior_size,
		       line_thickness);
      cairo_fill (cr);
    }
  else
    {
      gdouble progress;
      gboolean active;

      active = gtk_theming_engine_is_state_set (engine, GTK_STATE_ACTIVE, &progress);

      if (active || progress > 0)
        {
          cairo_translate (cr,
                           x + pad, y + pad);

          cairo_scale (cr, interior_size / 7., interior_size / 7.);

          cairo_rectangle (cr, 0, 0, 7 * progress, 7);
          cairo_clip (cr);

          cairo_move_to  (cr, 7.0, 0.0);
          cairo_line_to  (cr, 7.5, 1.0);
          cairo_curve_to (cr, 5.3, 2.0,
                          4.3, 4.0,
                          3.5, 7.0);
          cairo_curve_to (cr, 3.0, 5.7,
                          1.3, 4.7,
                          0.0, 4.7);
          cairo_line_to  (cr, 0.2, 3.5);
          cairo_curve_to (cr, 1.1, 3.5,
                          2.3, 4.3,
                          3.0, 5.0);
          cairo_curve_to (cr, 1.0, 3.9,
                          2.4, 4.1,
                          3.2, 4.9);
          cairo_curve_to (cr, 3.5, 3.1,
                          5.2, 2.0,
                          7.0, 0.0);

          cairo_fill (cr);
        }
    }

  cairo_restore (cr);

  gdk_color_free (fg_color);
  gdk_color_free (base_color);
  gdk_color_free (text_color);
}

static void
gtk_theming_engine_render_option (GtkThemingEngine *engine,
                                  cairo_t          *cr,
                                  gdouble           x,
                                  gdouble           y,
                                  gdouble           width,
                                  gdouble           height)
{
  GtkStateFlags flags;
  GdkColor *base_color, *fg_color, *text_color;
  const GtkWidgetPath *path;
  gint exterior_size, interior_size, pad, thickness;
  gdouble radius;

  /* FIXME: set clipping */

  flags = gtk_theming_engine_get_state (engine);
  path = gtk_theming_engine_get_path (engine);
  radius = MIN (width, height) / 2 - 0.5;

  cairo_save (cr);

  gtk_theming_engine_get (engine, flags,
                          "foreground-color", &fg_color,
                          "base-color", &base_color,
                          "text-color", &text_color,
                          NULL);

  exterior_size = MIN (width, height);

  if (exterior_size % 2 == 0) /* Ensure odd */
    exterior_size -= 1;

  x -= (1 + exterior_size - width) / 2;
  y -= (1 + exterior_size - height) / 2;

  if (!gtk_theming_engine_has_class (engine, "menu"))
    {
      gdk_cairo_set_source_color (cr, base_color);

      cairo_arc (cr,
		 x + exterior_size / 2.,
		 y + exterior_size / 2.,
		 (exterior_size - 1) / 2.,
		 0, 2 * G_PI);

      cairo_fill_preserve (cr);

      if (gtk_theming_engine_has_class (engine, "cell"))
	gdk_cairo_set_source_color (cr, text_color);
      else
	gdk_cairo_set_source_color (cr, fg_color);

      cairo_set_line_width (cr, 1.);
      cairo_stroke (cr);
    }

  if (gtk_theming_engine_has_class (engine, "menu"))
    gdk_cairo_set_source_color (cr, fg_color);
  else
    gdk_cairo_set_source_color (cr, text_color);

  /* FIXME: thickness */
  thickness = 1;

  if (gtk_theming_engine_is_state_set (engine, GTK_STATE_INCONSISTENT, NULL))
    {
      gint line_thickness;

      pad = thickness + MAX (1, (exterior_size - 2 * thickness) / 9);
      interior_size = MAX (1, exterior_size - 2 * pad);

      if (interior_size < 7)
        {
          interior_size = 7;
          pad = MAX (0, (exterior_size - interior_size) / 2);
        }

      line_thickness = MAX (1, (3 + interior_size * 2) / 7);

      cairo_rectangle (cr,
		       x + pad,
		       y + pad + (interior_size - line_thickness) / 2.,
		       interior_size,
		       line_thickness);
      cairo_fill (cr);
    }
  if (gtk_theming_engine_is_state_set (engine, GTK_STATE_ACTIVE, NULL))
    {
      pad = thickness + MAX (1, 2 * (exterior_size - 2 * thickness) / 9);
      interior_size = MAX (1, exterior_size - 2 * pad);

      if (interior_size < 5)
	{
	  interior_size = 7;
	  pad = MAX (0, (exterior_size - interior_size) / 2);
	}

      cairo_arc (cr,
		 x + pad + interior_size / 2.,
		 y + pad + interior_size / 2.,
		 interior_size / 2.,
		 0, 2 * G_PI);
      cairo_fill (cr);
    }

  cairo_restore (cr);

  gdk_color_free (fg_color);
  gdk_color_free (base_color);
  gdk_color_free (text_color);
}

static void
add_path_arrow (cairo_t *cr,
                gdouble  angle,
                gdouble  x,
                gdouble  y,
                gdouble  size)
{
  cairo_save (cr);

  cairo_translate (cr, x + (size / 2), y + (size / 2));
  cairo_rotate (cr, angle);

  cairo_move_to (cr, 0, - (size / 4));
  cairo_line_to (cr, - (size / 2), (size / 4));
  cairo_line_to (cr, (size / 2), (size / 4));
  cairo_close_path (cr);

  cairo_restore (cr);
}

static void
gtk_theming_engine_render_arrow (GtkThemingEngine *engine,
                                 cairo_t          *cr,
                                 gdouble           angle,
                                 gdouble           x,
                                 gdouble           y,
                                 gdouble           size)
{
  GtkStateFlags flags;
  GdkColor *fg_color;

  cairo_save (cr);

  flags = gtk_theming_engine_get_state (engine);

  gtk_theming_engine_get (engine, flags,
                          "foreground-color", &fg_color,
                          NULL);

  if (flags & GTK_STATE_FLAG_INSENSITIVE)
    {
      add_path_arrow (cr, angle, x + 1, y + 1, size);
      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_fill (cr);
    }

  add_path_arrow (cr, angle, x, y, size);
  gdk_cairo_set_source_color (cr, fg_color);
  cairo_fill (cr);

  cairo_restore (cr);

  gdk_color_free (fg_color);
}

static void
add_path_line (cairo_t        *cr,
               gdouble         x1,
               gdouble         y1,
               gdouble         x2,
               gdouble         y2)
{
  /* Adjust endpoints */
  if (y1 == y2)
    {
      y1 += 0.5;
      y2 += 0.5;
      x2 += 1;
    }
  else if (x1 == x2)
    {
      x1 += 0.5;
      x2 += 0.5;
      y2 += 1;
    }

  cairo_move_to (cr, x1, y1);
  cairo_line_to (cr, x2, y2);
}

static void
add_path_rectangle_sides (cairo_t  *cr,
                          gdouble   x,
                          gdouble   y,
                          gdouble   width,
                          gdouble   height,
                          guint     sides)
{
  if (sides & SIDE_TOP)
    {
      cairo_move_to (cr, x, y + 0.5);
      cairo_line_to (cr, x + width, y + 0.5);
    }

  if (sides & SIDE_RIGHT)
    {
      cairo_move_to (cr, x + width - 0.5, y);
      cairo_line_to (cr, x + width - 0.5, y + height);
    }

  if (sides & SIDE_BOTTOM)
    {
      cairo_move_to (cr, x, y + height - 0.5);
      cairo_line_to (cr, x + width, y + height - 0.5);
    }

  if (sides & SIDE_LEFT)
    {
      cairo_move_to (cr, x + 0.5, y + height);
      cairo_line_to (cr, x + 0.5, y);
    }
}

static void
add_path_gap_side (cairo_t           *cr,
                   GtkPositionType    gap_side,
                   gdouble            x,
                   gdouble            y,
                   gdouble            width,
                   gdouble            height,
                   gdouble            xy0_gap,
                   gdouble            xy1_gap)
{
  switch (gap_side)
    {
    case GTK_POS_TOP:
      add_path_line (cr, x, y, x + xy0_gap, y);
      add_path_line (cr, x + xy1_gap, y, x + width, y);
      break;
    case GTK_POS_BOTTOM:
      add_path_line (cr, x, y + height, x + xy0_gap, y + height);
      add_path_line (cr, x + xy1_gap, y + height, x + width, y + height);
      break;
    case GTK_POS_LEFT:
      add_path_line (cr, x, y, x, y + xy0_gap);
      add_path_line (cr, x, y + xy1_gap, x, y + height);
      break;
    case GTK_POS_RIGHT:
      add_path_line (cr, x + width, y, x + width, y + xy0_gap);
      add_path_line (cr, x + width, y + xy1_gap, x + width, y + height);
      break;
    }
}

static void
color_shade (const GdkColor *color,
             gdouble         factor,
             GdkColor       *color_return)
{
  color_return->red = CLAMP (color->red * factor, 0, 65535);
  color_return->green = CLAMP (color->green * factor, 0, 65535);
  color_return->blue = CLAMP (color->blue * factor, 0, 65535);
}

static void
gtk_theming_engine_render_background (GtkThemingEngine *engine,
                                      cairo_t          *cr,
                                      gdouble           x,
                                      gdouble           y,
                                      gdouble           width,
                                      gdouble           height)
{
  GdkColor *bg_color, *base_color;
  cairo_pattern_t *pattern;
  GtkStateFlags flags;

  flags = gtk_theming_engine_get_state (engine);
  cairo_save (cr);

  if (gtk_theming_engine_has_class (engine, "spinbutton") &&
      gtk_theming_engine_has_class (engine, "button"))
    {
      x += 2;
      y += 2;
      width -= 4;
      height -= 4;
    }

  gtk_theming_engine_get (engine, flags,
                          "background-image", &pattern,
                          "background-color", &bg_color,
                          "base-color", &base_color,
                          NULL);

  if (pattern)
    {
      cairo_translate (cr, x, y);
      cairo_scale (cr, width, height);

      cairo_rectangle (cr, 0, 0, 1, 1);
      cairo_set_source (cr, pattern);

      cairo_pattern_destroy (pattern);
    }
  else
    {
      if (gtk_theming_engine_has_class (engine, "entry"))
        gdk_cairo_set_source_color (cr, base_color);
      else
        gdk_cairo_set_source_color (cr, bg_color);

      cairo_rectangle (cr, x, y, width, height);
    }

  if (gtk_theming_engine_has_class (engine, "tooltip"))
    {
      cairo_fill_preserve (cr);

      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_stroke (cr);
    }
  else
    cairo_fill (cr);

  cairo_restore (cr);

  gdk_color_free (base_color);
  gdk_color_free (bg_color);
}

static void
gtk_theming_engine_render_frame (GtkThemingEngine *engine,
                                 cairo_t          *cr,
                                 gdouble           x,
                                 gdouble           y,
                                 gdouble           width,
                                 gdouble           height)
{
  GtkStateFlags flags;
  GdkColor lighter, darker;
  GdkColor *bg_color;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  cairo_set_line_width (cr, 1);

  gtk_theming_engine_get (engine, flags,
                          "background-color", &bg_color,
                          NULL);
  color_shade (bg_color, 0.7, &darker);
  color_shade (bg_color, 1.3, &lighter);

  if (gtk_theming_engine_has_class (engine, "entry") ||
      gtk_theming_engine_has_class (engine, "scrolled-window") ||
      gtk_theming_engine_has_class (engine, "viewport"))
    {
      gdk_cairo_set_source_color (cr, bg_color);
      add_path_rectangle_sides (cr, x + 1, y + 1, width - 2, height - 2,
                                SIDE_BOTTOM | SIDE_RIGHT);
      cairo_stroke (cr);

      cairo_set_source_rgb (cr, 0, 0, 0);
      add_path_rectangle_sides (cr, x + 1, y + 1, width - 2, height - 2,
                                SIDE_TOP | SIDE_LEFT);
      cairo_stroke (cr);

      cairo_set_source_rgb (cr, 1, 1, 1);
      add_path_rectangle_sides (cr, x, y, width, height,
                                SIDE_BOTTOM | SIDE_RIGHT);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, &darker);
      add_path_rectangle_sides (cr, x, y, width, height,
                                SIDE_TOP | SIDE_LEFT);
      cairo_stroke (cr);
    }
  else if (gtk_theming_engine_has_class (engine, "button") &&
           gtk_theming_engine_has_class (engine, "default"))
    {
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_rectangle (cr, x + 0.5, x + 0.5, width - 1, height - 1);
      cairo_stroke (cr);
    }
  else if (gtk_theming_engine_has_class (engine, "scrollbar") &&
           gtk_theming_engine_has_class (engine, "trough"))
    {
      gdk_cairo_set_source_color (cr, &darker);
      add_path_rectangle_sides (cr, x, y, width, height,
                                SIDE_TOP | SIDE_LEFT);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, &lighter);
      add_path_rectangle_sides (cr, x, y, width, height,
                                SIDE_BOTTOM | SIDE_RIGHT);
      cairo_stroke (cr);
    }
  else if (gtk_theming_engine_has_class (engine, "spinbutton"))
    {
      if (gtk_theming_engine_has_class (engine, "button"))
        {
          GtkJunctionSides sides;

          sides = gtk_theming_engine_get_junction_sides (engine);

          if (sides & GTK_JUNCTION_BOTTOM)
            y += 2;

          width -= 3;
          height -= 2;

          if (gtk_theming_engine_get_direction (engine) == GTK_TEXT_DIR_RTL)
            x += 2;
          else
            x += 1;

          gdk_cairo_set_source_color (cr, &lighter);
          add_path_rectangle_sides (cr, x, y, width, height, SIDE_TOP);
          cairo_stroke (cr);

          gdk_cairo_set_source_color (cr, &darker);
          add_path_rectangle_sides (cr, x, y, width, height, SIDE_BOTTOM);
          cairo_stroke (cr);
        }
      else
        {
          gdk_cairo_set_source_color (cr, &lighter);
          add_path_rectangle_sides (cr, x, y, width, height,
                                    SIDE_BOTTOM | SIDE_RIGHT);
          cairo_stroke (cr);

          gdk_cairo_set_source_color (cr, &darker);
          add_path_rectangle_sides (cr, x, y, width, height, SIDE_TOP);
          cairo_stroke (cr);

          gdk_cairo_set_source_color (cr, bg_color);
          add_path_rectangle_sides (cr, x, y, width - 1, height - 1, SIDE_BOTTOM);
          cairo_stroke (cr);

          cairo_set_source_rgb (cr, 0, 0, 0);
          add_path_rectangle_sides (cr, x, y + 1, width - 1, height - 3,
                                    SIDE_TOP | SIDE_LEFT | SIDE_RIGHT);
          cairo_stroke (cr);
        }
    }
  else
    {
      if (flags & GTK_STATE_FLAG_ACTIVE)
        {
          cairo_set_source_rgb (cr, 0, 0, 0);
          add_path_rectangle_sides (cr, x + 1, y + 1, width - 2, height - 2,
                                    SIDE_TOP | SIDE_LEFT);
          cairo_stroke (cr);

          gdk_cairo_set_source_color (cr, &lighter);
          add_path_rectangle_sides (cr, x, y, width, height,
                                    SIDE_BOTTOM | SIDE_RIGHT);
          cairo_stroke (cr);

          gdk_cairo_set_source_color (cr, &darker);
          add_path_rectangle_sides (cr, x, y, width, height,
                                    SIDE_TOP | SIDE_LEFT);
          cairo_stroke (cr);
        }
      else
        {
          gdk_cairo_set_source_color (cr, &darker);
          add_path_rectangle_sides (cr, x, y, width - 1, height - 1,
                                    SIDE_BOTTOM | SIDE_RIGHT);
          cairo_stroke (cr);

          gdk_cairo_set_source_color (cr, &lighter);
          add_path_rectangle_sides (cr, x, y, width, height,
                                    SIDE_TOP | SIDE_LEFT);
          cairo_stroke (cr);

          cairo_set_source_rgb (cr, 0, 0, 0);
          add_path_rectangle_sides (cr, x, y, width, height,
                                    SIDE_BOTTOM | SIDE_RIGHT);
          cairo_stroke (cr);
        }
    }

  cairo_restore (cr);

  gdk_color_free (bg_color);
}

static void
gtk_theming_engine_render_expander (GtkThemingEngine *engine,
                                    cairo_t          *cr,
                                    gdouble           x,
                                    gdouble           y,
                                    gdouble           width,
                                    gdouble           height)
{
  GtkStateFlags flags;
  GdkColor *bg_color, *fg_color, *base_color;
  double vertical_overshoot;
  int diameter;
  double radius;
  double interp;		/* interpolation factor for center position */
  double x_double_horz, y_double_horz;
  double x_double_vert, y_double_vert;
  double x_double, y_double;
  gdouble angle;
  gint line_width;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  gtk_theming_engine_get (engine, flags,
                          "foreground-color", &fg_color,
                          "background-color", &bg_color,
                          "base-color", &base_color,
                          NULL);

  line_width = 1;

  /* FIXME: LTR/RTL */
  if (flags & GTK_STATE_FLAG_ACTIVE)
    {
      angle = G_PI / 2;
      interp = 1.0;
    }
  else
    {
      angle = 0;
      interp = 0;
    }

  /* Compute distance that the stroke extends beyonds the end
   * of the triangle we draw.
   */
  vertical_overshoot = line_width / 2.0 * (1. / tan (G_PI / 8));

  /* For odd line widths, we end the vertical line of the triangle
   * at a half pixel, so we round differently.
   */
  if (line_width % 2 == 1)
    vertical_overshoot = ceil (0.5 + vertical_overshoot) - 0.5;
  else
    vertical_overshoot = ceil (vertical_overshoot);

  /* Adjust the size of the triangle we draw so that the entire stroke fits
   */
  diameter = (gint) MAX (3, width - 2 * vertical_overshoot);

  /* If the line width is odd, we want the diameter to be even,
   * and vice versa, so force the sum to be odd. This relationship
   * makes the point of the triangle look right.
   */
  diameter -= (1 - (diameter + line_width) % 2);

  radius = diameter / 2.;

  /* Adjust the center so that the stroke is properly aligned with
   * the pixel grid. The center adjustment is different for the
   * horizontal and vertical orientations. For intermediate positions
   * we interpolate between the two.
   */
  x_double_vert = floor ((x + width / 2) - (radius + line_width) / 2.) + (radius + line_width) / 2.;
  y_double_vert = (y + height / 2) - 0.5;

  x_double_horz = (x + width / 2) - 0.5;
  y_double_horz = floor ((y + height / 2) - (radius + line_width) / 2.) + (radius + line_width) / 2.;

  x_double = x_double_vert * (1 - interp) + x_double_horz * interp;
  y_double = y_double_vert * (1 - interp) + y_double_horz * interp;

  cairo_translate (cr, x_double, y_double);
  cairo_rotate (cr, angle);

  cairo_move_to (cr, - radius / 2., - radius);
  cairo_line_to (cr,   radius / 2.,   0);
  cairo_line_to (cr, - radius / 2.,   radius);
  cairo_close_path (cr);

  cairo_set_line_width (cr, line_width);

  if (flags & GTK_STATE_FLAG_PRELIGHT)
    gdk_cairo_set_source_color (cr, fg_color);
  else
    gdk_cairo_set_source_color (cr, base_color);

  cairo_fill_preserve (cr);

  gdk_cairo_set_source_color (cr, fg_color);
  cairo_stroke (cr);

  cairo_restore (cr);

  gdk_color_free (base_color);
  gdk_color_free (fg_color);
  gdk_color_free (bg_color);
}

static void
gtk_theming_engine_render_focus (GtkThemingEngine *engine,
                                 cairo_t          *cr,
                                 gdouble           x,
                                 gdouble           y,
                                 gdouble           width,
                                 gdouble           height)
{
  GtkStateFlags flags;
  GdkColor *color;
  gint line_width;
  gint8 *dash_list;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  gtk_theming_engine_get (engine, flags,
                          "foreground-color", &color,
                          NULL);

  gtk_theming_engine_get_style (engine,
				"focus-line-width", &line_width,
				"focus-line-pattern", (gchar *) &dash_list,
				NULL);

  cairo_set_line_width (cr, (gdouble) line_width);

  if (dash_list[0])
    {
      gint n_dashes = strlen ((const gchar *) dash_list);
      gdouble *dashes = g_new (gdouble, n_dashes);
      gdouble total_length = 0;
      gdouble dash_offset;
      gint i;

      for (i = 0; i < n_dashes; i++)
	{
	  dashes[i] = dash_list[i];
	  total_length += dash_list[i];
	}

      /* The dash offset here aligns the pattern to integer pixels
       * by starting the dash at the right side of the left border
       * Negative dash offsets in cairo don't work
       * (https://bugs.freedesktop.org/show_bug.cgi?id=2729)
       */
      dash_offset = - line_width / 2.;

      while (dash_offset < 0)
	dash_offset += total_length;

      cairo_set_dash (cr, dashes, n_dashes, dash_offset);
      g_free (dashes);
    }

  cairo_rectangle (cr,
                   x + line_width / 2.,
                   y + line_width / 2.,
                   width - line_width,
                   height - line_width);

  gdk_cairo_set_source_color (cr, color);
  cairo_stroke (cr);

  cairo_restore (cr);

  gdk_color_free (color);
  g_free (dash_list);
}

static void
gtk_theming_engine_render_line (GtkThemingEngine *engine,
                                cairo_t          *cr,
                                gdouble           x0,
                                gdouble           y0,
                                gdouble           x1,
                                gdouble           y1)
{
  GdkColor *bg_color, darker, lighter;
  GtkStateFlags flags;
  gint i, thickness, thickness_dark, thickness_light, len;
  cairo_matrix_t matrix;
  gdouble angle;

  /* FIXME: thickness */
  thickness = 2;
  thickness_dark = thickness / 2;
  thickness_light = thickness - thickness_dark;

  flags = gtk_theming_engine_get_state (engine);
  cairo_save (cr);

  gtk_theming_engine_get (engine, flags,
                          "background-color", &bg_color,
                          NULL);
  color_shade (bg_color, 0.7, &darker);
  color_shade (bg_color, 1.3, &lighter);

  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_width (cr, 1);

  angle = atan2 (x1 - x0, y1 - y0);
  angle = (2 * G_PI) - angle;
  angle += G_PI / 2;

  cairo_get_matrix (cr, &matrix);
  cairo_matrix_translate (&matrix, x0, y0);
  cairo_matrix_rotate (&matrix, angle);
  cairo_set_matrix (cr, &matrix);

  x1 -= x0;
  y1 -= y0;

  len = (gint) sqrt ((x1 * x1) + (y1 * y1));

  y0 = -thickness_dark;

  for (i = 0; i < thickness_dark; i++)
    {
      gdk_cairo_set_source_color (cr, &lighter);
      add_path_line (cr, len - i - 1.5, y0, len - 0.5, y0);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, &darker);
      add_path_line (cr, 0.5, y0, len - i - 1.5, y0);
      cairo_stroke (cr);

      y0++;
    }

  for (i = 0; i < thickness_light; i++)
    {
      gdk_cairo_set_source_color (cr, &darker);
      add_path_line (cr, 0.5, y0, thickness_light - i + 0.5, y0);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, &lighter);
      add_path_line (cr, thickness_light - i + 0.5, y0, len - 0.5, y0);
      cairo_stroke (cr);

      y0++;
    }

  cairo_restore (cr);

  gdk_color_free (bg_color);
}

static void
gtk_theming_engine_render_layout (GtkThemingEngine *engine,
                                  cairo_t          *cr,
                                  gdouble           x,
                                  gdouble           y,
                                  PangoLayout      *layout)
{
  const PangoMatrix *matrix;
  GdkColor *fg_color;
  GtkStateFlags flags;
  GdkScreen *screen;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  /* FIXME: Set clipping */

  gtk_theming_engine_get (engine, flags,
                          "foreground-color", &fg_color,
                          NULL);

  screen = gtk_theming_engine_get_screen (engine);
  matrix = pango_context_get_matrix (pango_layout_get_context (layout));

  if (matrix)
    {
      cairo_matrix_t cairo_matrix;
      PangoMatrix tmp_matrix;
      PangoRectangle rect;

      cairo_matrix_init (&cairo_matrix,
                         matrix->xx, matrix->yx,
                         matrix->xy, matrix->yy,
                         matrix->x0, matrix->y0);

      pango_layout_get_extents (layout, NULL, &rect);
      pango_matrix_transform_rectangle (matrix, &rect);
      pango_extents_to_pixels (&rect, NULL);

      tmp_matrix = *matrix;
      cairo_matrix.x0 += x - rect.x;
      cairo_matrix.y0 += y - rect.y;

      cairo_set_matrix (cr, &cairo_matrix);
    }
  else
    cairo_translate (cr, x, y);

  if (gtk_theming_engine_is_state_set (engine, GTK_STATE_INSENSITIVE, NULL))
    {
      cairo_save (cr);
      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_move_to (cr, 1, 1);
      _gtk_pango_fill_layout (cr, layout);
      cairo_restore (cr);
    }

  gdk_cairo_set_source_color (cr, fg_color);
  pango_cairo_show_layout (cr, layout);

  cairo_restore (cr);

  gdk_color_free (fg_color);
}

static void
gtk_theming_engine_render_slider (GtkThemingEngine *engine,
                                  cairo_t          *cr,
                                  gdouble           x,
                                  gdouble           y,
                                  gdouble           width,
                                  gdouble           height,
                                  GtkOrientation    orientation)
{
  const GtkWidgetPath *path;
  gint thickness;

  path = gtk_theming_engine_get_path (engine);

  gtk_theming_engine_render_background (engine, cr, x, y, width, height);
  gtk_theming_engine_render_frame (engine, cr, x, y, width, height);

  /* FIXME: thickness */
  thickness = 2;

  if (gtk_widget_path_is_type (path, GTK_TYPE_SCALE))
    {
      if (orientation == GTK_ORIENTATION_VERTICAL)
        gtk_theming_engine_render_line (engine, cr,
                                        x + thickness,
                                        y + (gint) height / 2,
                                        x + width - thickness - 1,
                                        y + (gint) height / 2);
      else
        gtk_theming_engine_render_line (engine, cr,
                                        x + (gint) width / 2,
                                        y + thickness,
                                        x + (gint) width / 2,
                                        y + height - thickness - 1);
    }
}

static void
gtk_theming_engine_render_frame_gap (GtkThemingEngine *engine,
                                     cairo_t          *cr,
                                     gdouble           x,
                                     gdouble           y,
                                     gdouble           width,
                                     gdouble           height,
                                     GtkPositionType   gap_side,
                                     gdouble           xy0_gap,
                                     gdouble           xy1_gap)
{
  GtkStateFlags flags;
  GdkColor *bg_color;
  GdkColor lighter, darker;
  guint sides;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  cairo_set_line_width (cr, 1);

  gtk_theming_engine_get (engine, flags,
                          "background-color", &bg_color,
                          NULL);
  color_shade (bg_color, 0.7, &darker);
  color_shade (bg_color, 1.3, &lighter);

  if (gtk_theming_engine_has_class (engine, "frame"))
    {
      if (gap_side == GTK_POS_RIGHT)
        sides = SIDE_BOTTOM;
      else if (gap_side == GTK_POS_BOTTOM)
        sides = SIDE_RIGHT;
      else
        sides = SIDE_BOTTOM | SIDE_RIGHT;

      gdk_cairo_set_source_color (cr, &lighter);
      add_path_rectangle_sides (cr, x , y, width , height, sides);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, &darker);
      add_path_rectangle_sides (cr, x, y, width - 1, height - 1, sides);
      cairo_stroke (cr);

      if (gap_side == GTK_POS_RIGHT ||
	  gap_side == GTK_POS_BOTTOM)
        {
          gdk_cairo_set_source_color (cr, &darker);
          add_path_gap_side (cr, gap_side,
                             x + 1, y + 1, width - 4, height - 4,
                             xy0_gap, xy1_gap);
          cairo_stroke (cr);

          gdk_cairo_set_source_color (cr, &lighter);
          add_path_gap_side (cr, gap_side,
                             x, y, width, height,
                             xy0_gap, xy1_gap);
          cairo_stroke (cr);
        }

      if (gap_side == GTK_POS_LEFT)
        sides = SIDE_TOP;
      else if (gap_side == GTK_POS_TOP)
        sides = SIDE_LEFT;
      else
        sides = SIDE_TOP | SIDE_LEFT;

      gdk_cairo_set_source_color (cr, &lighter);
      add_path_rectangle_sides (cr, x + 1, y + 1, width - 2, height - 3, sides);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, &darker);
      add_path_rectangle_sides (cr, x, y, width - 1, height - 1, sides);
      cairo_stroke (cr);

      if (gap_side == GTK_POS_LEFT ||
          gap_side == GTK_POS_TOP)
        {
          gdk_cairo_set_source_color (cr, &lighter);
          add_path_gap_side (cr, gap_side,
                             x + 1, y + 1, width - 4, height - 4,
                             xy0_gap, xy1_gap);
          cairo_stroke (cr);

          gdk_cairo_set_source_color (cr, &darker);
          add_path_gap_side (cr, gap_side,
                             x, y, width - 2, height - 2,
                             xy0_gap, xy1_gap);
          cairo_stroke (cr);
        }
    }
  else
    {
      if (gap_side == GTK_POS_RIGHT)
        sides = SIDE_BOTTOM;
      else if (gap_side == GTK_POS_BOTTOM)
        sides = SIDE_RIGHT;
      else
        sides = SIDE_BOTTOM | SIDE_RIGHT;

      gdk_cairo_set_source_color (cr, &darker);
      add_path_rectangle_sides (cr, x + 1, y, width - 2, height, sides);
      add_path_rectangle_sides (cr, x, y + 1, width, height - 2, sides);
      cairo_stroke (cr);

      cairo_set_source_rgb (cr, 0, 0, 0);
      add_path_rectangle_sides (cr, x, y, width, height, sides);
      cairo_stroke (cr);

      if (gap_side == GTK_POS_RIGHT ||
          gap_side == GTK_POS_BOTTOM)
        {
          gdk_cairo_set_source_color (cr, &darker);
          add_path_gap_side (cr, gap_side,
                             x, y, width - 2, height - 2,
                             xy0_gap, xy1_gap);
          cairo_stroke (cr);

          cairo_set_source_rgb (cr, 0, 0, 0);
          add_path_gap_side (cr, gap_side,
                             x, y, width - 1, height - 1,
                             xy0_gap, xy1_gap);
          cairo_stroke (cr);
        }

      if (gap_side == GTK_POS_LEFT)
        sides = SIDE_TOP;
      else if (gap_side == GTK_POS_TOP)
        sides = SIDE_LEFT;
      else
        sides = SIDE_TOP | SIDE_LEFT;

      gdk_cairo_set_source_color (cr, &lighter);
      add_path_rectangle_sides (cr, x, y, width, height, sides);
      cairo_stroke (cr);

      if (gap_side == GTK_POS_LEFT ||
          gap_side == GTK_POS_TOP)
        {
          add_path_gap_side (cr, gap_side,
                             x, y, width, height,
                             xy0_gap, xy1_gap);
          cairo_stroke (cr);
        }
    }

  cairo_restore (cr);

  gdk_color_free (bg_color);
}

static void
gtk_theming_engine_render_extension (GtkThemingEngine *engine,
                                     cairo_t          *cr,
                                     gdouble           x,
                                     gdouble           y,
                                     gdouble           width,
                                     gdouble           height,
                                     GtkPositionType   gap_side)
{
  GtkStateFlags flags;
  GdkColor *bg_color;
  GdkColor lighter, darker;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  cairo_set_line_width (cr, 1);

  gtk_theming_engine_get (engine, flags,
                          "background-color", &bg_color,
                          NULL);
  color_shade (bg_color, 0.7, &darker);
  color_shade (bg_color, 1.3, &lighter);

  switch (gap_side)
    {
    case GTK_POS_TOP:
      gdk_cairo_set_source_color (cr, bg_color);
      cairo_rectangle (cr, x + 1, y, width - 2, height);
      cairo_fill (cr);

      gdk_cairo_set_source_color (cr, &lighter);
      add_path_line (cr, x, y, x, y + height - 2);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, bg_color);
      add_path_line (cr, x + 1, y, x + 1, y + height - 2);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, &darker);
      add_path_line (cr, x + 2, y + height - 2, x + width - 2, y + height - 2);
      add_path_line (cr, x + width - 2, y, x + width - 2, y + height - 2);
      cairo_stroke (cr);

      cairo_set_source_rgb (cr, 0, 0, 0);
      add_path_line (cr, x + 1, y + height - 1, x + width - 2, y + height - 1);
      add_path_line (cr, x + width - 1, y, x + width - 1, y + height - 2);
      cairo_stroke (cr);

      break;
    case GTK_POS_BOTTOM:
      gdk_cairo_set_source_color (cr, bg_color);
      cairo_rectangle (cr, x + 1, y, width - 2, height);
      cairo_fill (cr);

      gdk_cairo_set_source_color (cr, &lighter);
      add_path_line (cr, x + 1, y, x + width - 2, y);
      add_path_line (cr, x, y + 1, x, y + height - 1);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, bg_color);
      add_path_line (cr, x + 1, y + 1, x + width - 2, y + 1);
      add_path_line (cr, x + 1, y + 1, x + 1, y + height - 1);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, &darker);
      add_path_line (cr, x + width - 2, y + 2, x + width - 2, y + height - 1);
      cairo_stroke (cr);

      cairo_set_source_rgb (cr, 0, 0, 0);
      add_path_line (cr, x + width - 1, y + 1, x + width - 1, y + height - 1);
      cairo_stroke (cr);

      break;
    case GTK_POS_LEFT:
      gdk_cairo_set_source_color (cr, bg_color);
      cairo_rectangle (cr, x, y + 1, width, height - 2);
      cairo_fill (cr);

      gdk_cairo_set_source_color (cr, &lighter);
      add_path_line (cr, x, y, x + width - 2, y);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, bg_color);
      add_path_line (cr, x + 1, y + 1, x + width - 2, y + 1);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, &darker);
      add_path_line (cr, x, y + height - 2, x + width - 2, y + height - 2);
      add_path_line (cr, x + width - 2, y + 2, x + width - 2, y + height - 2);
      cairo_stroke (cr);

      cairo_set_source_rgb (cr, 0, 0, 0);
      add_path_line (cr, x, y + height - 1, x + width - 2, y + height - 1);
      add_path_line (cr, x + width - 1, y + 1, x + width - 1, y + height - 2);
      cairo_stroke (cr);

      break;
    case GTK_POS_RIGHT:
      gdk_cairo_set_source_color (cr, bg_color);
      cairo_rectangle (cr, x, y + 1, width, height - 2);
      cairo_fill (cr);

      gdk_cairo_set_source_color (cr, &lighter);
      add_path_line (cr, x + 1, y, x + width - 1, y);
      add_path_line (cr, x, y + 1, x, y + height - 2);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, bg_color);
      add_path_line (cr, x + 1, y + 1, x + width - 1, y + 1);
      add_path_line (cr, x + 1, y + 1, x + 1, y + height - 2);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, &darker);
      add_path_line (cr, x + 2, y + height - 2, x + width - 1, y + height - 2);
      cairo_stroke (cr);

      cairo_set_source_rgb (cr, 0, 0, 0);
      add_path_line (cr, x + 1, y + height - 1, x + width - 1, y + height - 1);
      cairo_stroke (cr);

      break;
    }

  cairo_restore (cr);

  gdk_color_free (bg_color);
}

static void
render_dot (cairo_t        *cr,
            const GdkColor *lighter,
            const GdkColor *darker,
            gdouble         x,
            gdouble         y,
            gdouble         size)
{
  size = CLAMP ((gint) size, 2, 3);

  if (size == 2)
    {
      gdk_cairo_set_source_color (cr, lighter);
      cairo_rectangle (cr, x, y, 1, 1);
      cairo_rectangle (cr, x + 1, y + 1, 1, 1);
      cairo_fill (cr);
    }
  else if (size == 3)
    {
      gdk_cairo_set_source_color (cr, lighter);
      cairo_rectangle (cr, x, y, 2, 1);
      cairo_rectangle (cr, x, y, 1, 2);
      cairo_fill (cr);

      gdk_cairo_set_source_color (cr, darker);
      cairo_rectangle (cr, x + 1, y + 1, 2, 1);
      cairo_rectangle (cr, x + 2, y, 1, 2);
      cairo_fill (cr);
    }
}

static void
gtk_theming_engine_render_handle (GtkThemingEngine *engine,
                                  cairo_t          *cr,
                                  gdouble           x,
                                  gdouble           y,
                                  gdouble           width,
                                  gdouble           height,
                                  GtkOrientation    orientation)
{
  GtkStateFlags flags;
  GdkColor *bg_color;
  GdkColor lighter, darker;
  gint xx, yy;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  cairo_set_line_width (cr, 1);

  gtk_theming_engine_get (engine, flags,
                          "background-color", &bg_color,
                          NULL);
  color_shade (bg_color, 0.7, &darker);
  color_shade (bg_color, 1.3, &lighter);

  gdk_cairo_set_source_color (cr, bg_color);
  cairo_rectangle (cr, x, y, width, height);
  cairo_fill (cr);

  if (gtk_theming_engine_has_class (engine, "paned"))
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        for (xx = x + width / 2 - 15; xx <= x + width / 2 + 15; xx += 5)
          render_dot (cr, &lighter, &darker, xx, y + height / 2 - 1, 3);
      else
        for (yy = y + height / 2 - 15; yy <= y + height / 2 + 15; yy += 5)
          render_dot (cr, &lighter, &darker, x + width / 2 - 1, yy, 3);
    }
  else
    {
      for (yy = y; yy < y + height; yy += 3)
	for (xx = x; xx < x + width; xx += 6)
	  {
	    render_dot (cr, &lighter, &darker, xx, yy, 2);
	    render_dot (cr, &lighter, &darker, xx + 3, yy + 1, 2);
	  }
    }

  cairo_restore (cr);

  gdk_color_free (bg_color);
}
