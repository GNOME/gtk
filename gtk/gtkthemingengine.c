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

#include <gtk/gtk.h>

#include <gtk/gtkthemingengine.h>
#include <gtk/gtkstylecontext.h>
#include <gtk/gtkintl.h>

#include "gtkalias.h"

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
gtk_theming_engine_get_property (GtkThemingEngine *engine,
                                 const gchar      *property,
                                 GtkStateType      state,
                                 GValue           *value)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));
  g_return_if_fail (property != NULL);
  g_return_if_fail (state < GTK_STATE_LAST);
  g_return_if_fail (value != NULL);

  priv = engine->priv;
  gtk_style_context_get_property (priv->context, property, state, value);
}

void
gtk_theming_engine_get_valist (GtkThemingEngine *engine,
                               GtkStateType      state,
                               va_list           args)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));
  g_return_if_fail (state < GTK_STATE_LAST);

  priv = engine->priv;
  gtk_style_context_get_valist (priv->context, state, args);
}

void
gtk_theming_engine_get (GtkThemingEngine *engine,
                        GtkStateType      state,
                        ...)
{
  GtkThemingEnginePrivate *priv;
  va_list args;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));
  g_return_if_fail (state < GTK_STATE_LAST);

  priv = engine->priv;

  va_start (args, state);
  gtk_style_context_get_valist (priv->context, state, args);
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
                                 GtkStateType      state)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), 0);

  priv = engine->priv;
  return gtk_style_context_is_state_set (priv->context, state);
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
gtk_theming_engine_has_child_class (GtkThemingEngine   *engine,
                                    const gchar        *style_class,
                                    GtkChildClassFlags *flags)
{
  GtkThemingEnginePrivate *priv;

  if (flags)
    *flags = 0;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), FALSE);

  priv = engine->priv;
  return gtk_style_context_has_child_class (priv->context, style_class, flags);
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
      if (!default_engine)
        default_engine = g_object_new (GTK_TYPE_THEMING_ENGINE, NULL);

      engine = default_engine;
    }

  return engine;
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
  GtkStateType state;

  flags = gtk_theming_engine_get_state (engine);
  path = gtk_theming_engine_get_path (engine);
  cairo_save (cr);

  if (flags & GTK_STATE_FLAG_PRELIGHT)
    state = GTK_STATE_PRELIGHT;
  else
    state = GTK_STATE_NORMAL;

  gtk_theming_engine_get (engine, state,
                          "foreground-color", &fg_color,
                          "base-color", &base_color,
                          "text-color", &text_color,
                          NULL);

  if (!gtk_widget_path_has_parent (path, GTK_TYPE_MENU))
    {
      cairo_set_line_width (cr, 1);

      cairo_rectangle (cr,
                       x + 0.5, y + 0.5,
                       width - 1, height - 1);

      gdk_cairo_set_source_color (cr, base_color);
      cairo_fill_preserve (cr);

      if (gtk_widget_path_has_parent (path, GTK_TYPE_TREE_VIEW))
        gdk_cairo_set_source_color (cr, text_color);
      else
        gdk_cairo_set_source_color (cr, fg_color);

      cairo_stroke (cr);
    }

  cairo_set_line_width (cr, 1.5);
  gdk_cairo_set_source_color (cr, text_color);

  if (gtk_theming_engine_is_state_set (engine, GTK_STATE_INCONSISTENT))
    {
      cairo_move_to (cr, x + (width * 0.2), y + (height / 2));
      cairo_line_to (cr, x + (width * 0.8), y + (height / 2));
    }
  else if (gtk_theming_engine_is_state_set (engine, GTK_STATE_ACTIVE))
    {
      cairo_move_to (cr, x + (width * 0.2), y + (height / 2));
      cairo_line_to (cr, x + (width * 0.4), y + (height * 0.8));
      cairo_line_to (cr, x + (width * 0.8), y + (height * 0.2));
    }

  cairo_stroke (cr);

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
  GtkStateType state;
  gdouble radius;

  flags = gtk_theming_engine_get_state (engine);
  path = gtk_theming_engine_get_path (engine);
  radius = MIN (width, height) / 2 - 0.5;

  cairo_save (cr);
  cairo_set_line_width (cr, 1);

  if (flags & GTK_STATE_FLAG_PRELIGHT)
    state = GTK_STATE_PRELIGHT;
  else
    state = GTK_STATE_NORMAL;

  gtk_theming_engine_get (engine, state,
                          "foreground-color", &fg_color,
                          "base-color", &base_color,
                          "text-color", &text_color,
                          NULL);

  if (!gtk_widget_path_has_parent (path, GTK_TYPE_MENU))
    {
      cairo_arc (cr,
                 x + (width / 2),
                 y + (height / 2),
                 radius,
                 0, 2 * G_PI);

      gdk_cairo_set_source_color (cr, base_color);
      cairo_fill_preserve (cr);

      if (gtk_widget_path_has_parent (path, GTK_TYPE_TREE_VIEW))
        gdk_cairo_set_source_color (cr, text_color);
      else
        gdk_cairo_set_source_color (cr, fg_color);

      cairo_stroke (cr);
    }

  if (gtk_widget_path_has_parent (path, GTK_TYPE_MENU))
    gdk_cairo_set_source_color (cr, fg_color);
  else
    gdk_cairo_set_source_color (cr, text_color);

  if (gtk_theming_engine_is_state_set (engine, GTK_STATE_INCONSISTENT))
    {
      cairo_move_to (cr, x + (width * 0.2), y + (height / 2));
      cairo_line_to (cr, x + (width * 0.8), y + (height / 2));
      cairo_stroke (cr);
    }
  if (gtk_theming_engine_is_state_set (engine, GTK_STATE_ACTIVE))
    {
      cairo_arc (cr,
                 x + (width / 2),
                 y + (height / 2),
                 radius / 2,
                 0, 2 * G_PI);

      cairo_fill (cr);
    }

  cairo_restore (cr);
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

  cairo_move_to (cr, 0, - (size / 4) + 0.5);
  cairo_line_to (cr, - (size / 2), (size / 4) + 0.5);
  cairo_line_to (cr, (size / 2), (size / 4) + 0.5);
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
  GtkStateType state;
  GdkColor *fg_color;

  cairo_save (cr);

  flags = gtk_theming_engine_get_state (engine);

  if (flags & GTK_STATE_FLAG_PRELIGHT)
    state = GTK_STATE_PRELIGHT;
  else if (flags & GTK_STATE_FLAG_INSENSITIVE)
    state = GTK_STATE_INSENSITIVE;
  else
    state = GTK_STATE_NORMAL;

  gtk_theming_engine_get (engine, state,
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
add_path_rounded_rectangle (cairo_t           *cr,
                            gdouble            radius,
                            guint              sides,
                            gdouble            x,
                            gdouble            y,
                            gdouble            width,
                            gdouble            height)
{
  gdouble r = 0;

  if (sides & SIDE_BOTTOM)
    {
      /* Bottom left corner */
      if (r == 0)
        cairo_move_to (cr, x + 0.5, y + height - 0.5);
      else
        cairo_arc_negative (cr,
                            x + r + 0.5,
                            y + height - r - 0.5,
                            r,
                            135 * (G_PI / 180),
                            90 * (G_PI / 180));

      /* Bottom side */
      cairo_line_to (cr, x + width - r - 0.5, y + height - 0.5);

      /* Bottom right corner */
      if (r > 0)
        cairo_arc_negative (cr,
                            x + width - r - 0.5,
                            y + height - r - 0.5,
                            r,
                            90 * (G_PI / 180),
                            45 * (G_PI / 180));
    }

  if (sides & SIDE_RIGHT)
    {
      /* Bottom right corner */
      if (r == 0)
        {
          if ((sides & SIDE_BOTTOM) == 0)
            cairo_move_to (cr, x + width - 0.5, y + height - 0.5);
        }
      else
        cairo_arc_negative (cr,
                            x + width - r - 0.5,
                            y + height - r - 0.5,
                            r,
                            45 * (G_PI / 180), 0);

      /* Right side */
      cairo_line_to (cr, x + width - 0.5, y + r);

      /* Top right corner */
      if (r > 0)
        cairo_arc_negative (cr,
                            x + width - r - 0.5,
                            y + r + 0.5,
                            r,
                            0, 315 * (G_PI / 180));
    }

  if (sides & SIDE_TOP)
    {
      /* Top right corner */
      if (r == 0)
        {
          if ((sides & SIDE_RIGHT) == 0)
            cairo_move_to (cr, x + width - 1, y + 0.5);
        }
      else
        cairo_arc_negative (cr,
                            x + width - r - 0.5,
                            y + r + 0.5,
                            r,
                            315 * (G_PI / 180),
                            270 * (G_PI / 180));

      /* Top side */
      cairo_line_to (cr, x + 0.5 + r, y + 0.5);

      /* Top left corner */
      if (r > 0)
        cairo_arc_negative (cr,
                            x + r + 0.5,
                            y + r + 0.5,
                            r,
                            270 * (G_PI / 180),
                            225 * (G_PI / 180));
    }

  if (sides & SIDE_LEFT)
    {
      /* Top left corner */
      if (r == 0)
        {
          if ((sides & SIDE_TOP) == 0)
            cairo_move_to (cr, x + 0.5, y + 0.5);
        }
      else
        cairo_arc_negative (cr,
                            x + + r + 0.5,
                            y + r + 0.5,
                            r,
                            225 * (G_PI / 180),
                            180 * (G_PI / 180));

      /* Left side */
      cairo_line_to (cr, x + 0.5, y + height - r);

      if (r > 0)
        cairo_arc_negative (cr,
                            x + r + 0.5,
                            y + height - r + 0.5,
                            r,
                            180 * (G_PI / 180),
                            135 * (G_PI / 180));
    }
}

static void
add_path_gap_side (cairo_t           *cr,
                   GtkPositionType    gap_side,
                   gdouble            radius,
                   gdouble            x,
                   gdouble            y,
                   gdouble            width,
                   gdouble            height,
                   gdouble            xy0_gap,
                   gdouble            xy1_gap)
{
  if (gap_side == GTK_POS_TOP)
    {
      cairo_move_to (cr, x, y);
      cairo_line_to (cr, x + xy0_gap, y);

      cairo_move_to (cr, x + xy1_gap, y);
      cairo_line_to (cr, x + width, y);
    }
  else if (gap_side == GTK_POS_BOTTOM)
    {
      cairo_move_to (cr, x, y + height);
      cairo_line_to (cr, x + xy0_gap, y + height);

      cairo_move_to (cr, x + xy1_gap, y + height);
      cairo_line_to (cr, x + width, y + height);
    }
  else if (gap_side == GTK_POS_LEFT)
    {
      cairo_move_to (cr, x, y);
      cairo_line_to (cr, x, y + xy0_gap);

      cairo_move_to (cr, x, y + xy1_gap);
      cairo_line_to (cr, x, y + height);
    }
  else
    {
      cairo_move_to (cr, x + width, y);
      cairo_line_to (cr, x + width, y + xy0_gap);

      cairo_move_to (cr, x + width, y + xy1_gap);
      cairo_line_to (cr, x + width, y + height);
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
  GtkStateFlags flags;
  GtkStateType state;
  GdkColor *bg_color;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  if (flags & GTK_STATE_FLAG_PRELIGHT)
    state = GTK_STATE_PRELIGHT;
  else if (flags & GTK_STATE_FLAG_INSENSITIVE)
    state = GTK_STATE_INSENSITIVE;
  else
    state = GTK_STATE_NORMAL;

  gtk_theming_engine_get (engine, state,
                          "background-color", &bg_color,
                          NULL);

  add_path_rounded_rectangle (cr, 0,
                              SIDE_BOTTOM | SIDE_RIGHT | SIDE_TOP | SIDE_LEFT,
                              x, y, width, height);
  cairo_close_path (cr);

  gdk_cairo_set_source_color (cr, bg_color);
  cairo_fill (cr);

  cairo_restore (cr);
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
  GtkStateType state;
  GdkColor *bg_color;
  GdkColor lighter, darker;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  if (flags & GTK_STATE_FLAG_PRELIGHT)
    state = GTK_STATE_PRELIGHT;
  else if (flags & GTK_STATE_FLAG_INSENSITIVE)
    state = GTK_STATE_INSENSITIVE;
  else
    state = GTK_STATE_NORMAL;

  cairo_set_line_width (cr, 1);

  gtk_theming_engine_get (engine, state,
                          "background-color", &bg_color,
                          NULL);
  color_shade (bg_color, 0.7, &darker);
  color_shade (bg_color, 1.3, &lighter);

  if (flags & GTK_STATE_FLAG_ACTIVE)
    {
      add_path_rounded_rectangle (cr, 0,
                                  SIDE_BOTTOM | SIDE_RIGHT,
                                  x, y, width, height);

      gdk_cairo_set_source_color (cr, &lighter);
      cairo_stroke (cr);

      add_path_rounded_rectangle (cr, 0,
                                  SIDE_TOP | SIDE_LEFT,
                                  x + 1, y + 1, width - 2, height - 2);
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_stroke (cr);

      add_path_rounded_rectangle (cr, 0,
                                  SIDE_TOP | SIDE_LEFT,
                                  x, y, width, height);
      gdk_cairo_set_source_color (cr, &darker);
      cairo_stroke (cr);
    }
  else
    {
      add_path_rounded_rectangle (cr, 0,
                                  SIDE_BOTTOM | SIDE_RIGHT,
                                  x, y, width, height);

      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_stroke (cr);

      add_path_rounded_rectangle (cr, 0,
                                  SIDE_BOTTOM | SIDE_RIGHT,
                                  x, y, width - 1, height - 1);

      gdk_cairo_set_source_color (cr, &darker);
      cairo_stroke (cr);

      add_path_rounded_rectangle (cr, 0,
                                  SIDE_TOP | SIDE_LEFT,
                                  x, y, width, height);

      gdk_cairo_set_source_color (cr, &lighter);
      cairo_stroke (cr);
    }

  cairo_restore (cr);
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
  GtkStateType state;
  gdouble angle;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  if (flags & GTK_STATE_FLAG_PRELIGHT)
    state = GTK_STATE_PRELIGHT;
  else if (flags & GTK_STATE_FLAG_INSENSITIVE)
    state = GTK_STATE_INSENSITIVE;
  else
    state = GTK_STATE_NORMAL;

  gtk_theming_engine_get (engine, state,
                          "foreground-color", &fg_color,
                          "background-color", &bg_color,
                          "base-color", &base_color,
                          NULL);

  if (flags & GTK_STATE_FLAG_ACTIVE)
    angle = G_PI;
  else
    angle = G_PI / 2;

  cairo_set_line_width (cr, 1);
  add_path_arrow (cr, angle, x + 2, y + 2,
                  MIN (width - 1, height - 1) - 4);

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
  const double dashes[] = { 0.5, 1.5 };
  GdkColor *base_color;
  GtkStateFlags flags;
  GtkStateType state;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  if (flags & GTK_STATE_FLAG_PRELIGHT)
    state = GTK_STATE_PRELIGHT;
  else if (flags & GTK_STATE_FLAG_INSENSITIVE)
    state = GTK_STATE_INSENSITIVE;
  else
    state = GTK_STATE_NORMAL;

  gtk_theming_engine_get (engine, state,
                          "base-color", &base_color,
                          NULL);

  cairo_set_line_width (cr, 1.0);
  cairo_set_dash (cr, dashes, 2, 0);

  cairo_rectangle (cr,
                   x + 0.5,
                   y + 0.5,
                   width - 1,
                   height - 1);

  gdk_cairo_set_source_color (cr, base_color);
  cairo_stroke (cr);

  cairo_restore (cr);

  gdk_color_free (base_color);
}

static void
add_path_line (cairo_t        *cr,
               gdouble         x1,
               gdouble         y1,
               gdouble         x2,
               gdouble         y2)
{
  cairo_move_to (cr, x1 + 0.5, y1 + 0.5);
  cairo_line_to (cr, x2 + 0.5, y2 + 0.5);
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
  GtkStateType state;
  gint thickness, thickness_dark, thickness_light;
  gint i;

  thickness = 2;
  thickness_dark = thickness / 2;
  thickness_light = thickness - thickness_dark;

  flags = gtk_theming_engine_get_state (engine);
  cairo_save (cr);

  if (flags & GTK_STATE_FLAG_PRELIGHT)
    state = GTK_STATE_PRELIGHT;
  else if (flags & GTK_STATE_FLAG_INSENSITIVE)
    state = GTK_STATE_INSENSITIVE;
  else
    state = GTK_STATE_NORMAL;

  gtk_theming_engine_get (engine, state,
                          "background-color", &bg_color,
                          NULL);
  color_shade (bg_color, 0.7, &darker);
  color_shade (bg_color, 1.3, &lighter);

  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_width (cr, 1);

  if (y0 == y1)
    {
      /* Horizontal line */
      for (i = 0; i < thickness_dark; i++)
        {
          gdk_cairo_set_source_color (cr, &darker);
          add_path_line (cr, x0, y0, x1 - i - 1, y0);
          cairo_stroke (cr);

          gdk_cairo_set_source_color (cr, &lighter);
          add_path_line (cr, x1 - i, y0, x1, y0 + 1);
          cairo_stroke (cr);
          y0++;
        }

      for (i = 0; i < thickness_light; i++)
        {
          gdk_cairo_set_source_color (cr, &darker);
          add_path_line (cr, x0, y0, x0 + thickness_light - i, y0);
          cairo_stroke (cr);

          gdk_cairo_set_source_color (cr, &lighter);
          add_path_line (cr, x0 + thickness_light - i, y0, x1, y0);
          cairo_stroke (cr);
          y0++;
        }
    }
  else if (x0 == x1)
    {
      /* Vertical line */
      for (i = 0; i < thickness_dark; i++)
        {
          gdk_cairo_set_source_color (cr, &darker);
          add_path_line (cr, x0, y0, x0, y1 - i - 1);
          cairo_stroke (cr);

          gdk_cairo_set_source_color (cr, &lighter);
          add_path_line (cr, x0, y1 - i, x0, y1);
          cairo_stroke (cr);
          x0++;
        }

      for (i = 0; i < thickness_light; i++)
        {
          gdk_cairo_set_source_color (cr, &darker);
          add_path_line (cr, x0, y0, x0, y0 + thickness_light - i - 1);
          cairo_stroke (cr);

          gdk_cairo_set_source_color (cr, &lighter);
          add_path_line (cr, x0, y0 + thickness_light - i, x0, y1);
          cairo_stroke (cr);
          x0++;
        }
    }
  else
    {
      /* Arbitrary line */
      /* FIXME: implement thickness, etc */
      gdk_cairo_set_source_color (cr, &darker);
      add_path_line (cr, x0, y0, x1, y1);
      cairo_stroke (cr);
    }

  cairo_restore (cr);
}

static void
gtk_theming_engine_render_layout (GtkThemingEngine *engine,
                                  cairo_t          *cr,
                                  gdouble           x,
                                  gdouble           y,
                                  PangoLayout      *layout)
{
  GdkColor *fg_color;
  GtkStateFlags flags;
  GtkStateType state;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  if (flags & GTK_STATE_FLAG_PRELIGHT)
    state = GTK_STATE_PRELIGHT;
  else if (flags & GTK_STATE_FLAG_INSENSITIVE)
    state = GTK_STATE_INSENSITIVE;
  else
    state = GTK_STATE_NORMAL;

  gtk_theming_engine_get (engine, state,
                          "foreground-color", &fg_color,
                          NULL);

  cairo_move_to (cr, x, y);
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

  path = gtk_theming_engine_get_path (engine);

  gtk_theming_engine_render_background (engine, cr, x, y, width, height);
  gtk_theming_engine_render_frame (engine, cr, x, y, width, height);

  if (gtk_widget_path_has_parent (path, GTK_TYPE_SCALE))
    {
      if (orientation == GTK_ORIENTATION_VERTICAL)
        gtk_theming_engine_render_line (engine, cr,
                                        x + 2, y + height / 2 - 1,
                                        x + width - 4, y + height / 2 - 1);
      else
        gtk_theming_engine_render_line (engine, cr,
                                        x + width / 2 - 1, y + 4,
                                        x + width / 2 - 1, y + height - 4);
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
  GtkStateType state;
  GdkColor *bg_color;
  GdkColor lighter, darker;
  guint sides;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  if (flags & GTK_STATE_FLAG_PRELIGHT)
    state = GTK_STATE_PRELIGHT;
  else if (flags & GTK_STATE_FLAG_INSENSITIVE)
    state = GTK_STATE_INSENSITIVE;
  else
    state = GTK_STATE_NORMAL;

  cairo_set_line_width (cr, 1);

  gtk_theming_engine_get (engine, state,
                          "background-color", &bg_color,
                          NULL);
  color_shade (bg_color, 0.7, &darker);
  color_shade (bg_color, 1.3, &lighter);

  if (gap_side == GTK_POS_RIGHT)
    sides = SIDE_BOTTOM;
  else if (gap_side == GTK_POS_BOTTOM)
    sides = SIDE_RIGHT;
  else
    sides = SIDE_BOTTOM | SIDE_RIGHT;

  if (gap_side == GTK_POS_RIGHT ||
      gap_side == GTK_POS_BOTTOM)
    add_path_gap_side (cr, gap_side, 0,
                       x, y, width, height,
                       xy0_gap, xy1_gap);

  add_path_rounded_rectangle (cr, 0, sides,
                              x, y, width, height);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_stroke (cr);

  if (gap_side == GTK_POS_RIGHT ||
      gap_side == GTK_POS_BOTTOM)
    add_path_gap_side (cr, gap_side, 0,
                       x, y, width, height,
                       xy0_gap, xy1_gap);

  add_path_rounded_rectangle (cr, 0, sides,
                              x, y, width - 1, height - 1);

  gdk_cairo_set_source_color (cr, &darker);
  cairo_stroke (cr);

  if (gap_side == GTK_POS_LEFT)
    sides = SIDE_TOP;
  else if (gap_side == GTK_POS_TOP)
    sides = SIDE_LEFT;
  else
    sides = SIDE_TOP | SIDE_LEFT;

  if (gap_side == GTK_POS_TOP ||
      gap_side == GTK_POS_LEFT)
    add_path_gap_side (cr, gap_side, 0,
                       x, y, width, height,
                       xy0_gap, xy1_gap);

  add_path_rounded_rectangle (cr, 0, sides,
                              x, y, width, height);

  gdk_cairo_set_source_color (cr, &lighter);
  cairo_stroke (cr);

  cairo_restore (cr);

  gdk_color_free (bg_color);
}

#define __GTK_THEMING_ENGINE_C__
#include "gtkaliasdef.c"
