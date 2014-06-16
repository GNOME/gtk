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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <math.h>
#include <gtk/gtk.h>

#include "gtkthemingengineprivate.h"

#include <gtk/gtkstylecontext.h>
#include <gtk/gtkintl.h>

#include "gtkprivate.h"
#include "gtkmodulesprivate.h"
#include "gtkpango.h"
#include "gtkrenderprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkthemingengineprivate.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * SECTION:gtkthemingengine
 * @Short_description: Theming renderers
 * @Title: GtkThemingEngine
 * @See_also: #GtkStyleContext
 *
 * #GtkThemingEngine was the object used for rendering themed content
 * in GTK+ widgets. It used to allow overriding GTK+'s default
 * implementation of rendering functions by allowing engines to be
 * loaded as modules.
 *
 * #GtkThemingEngine has been deprecated in GTK+ 3.14 and will be
 * ignored for rendering. The advancements in CSS theming are good
 * enough to allow themers to achieve their goals without the need
 * to modify source code.
 */

enum {
  PROP_0,
  PROP_NAME
};

struct GtkThemingEnginePrivate
{
  GtkStyleContext *context;
  gchar *name;
};

static void gtk_theming_engine_finalize          (GObject      *object);
static void gtk_theming_engine_impl_set_property (GObject      *object,
                                                  guint         prop_id,
                                                  const GValue *value,
                                                  GParamSpec   *pspec);
static void gtk_theming_engine_impl_get_property (GObject      *object,
                                                  guint         prop_id,
                                                  GValue       *value,
                                                  GParamSpec   *pspec);

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
                                                 gdouble           height);
static void gtk_theming_engine_render_activity  (GtkThemingEngine *engine,
                                                 cairo_t          *cr,
                                                 gdouble           x,
                                                 gdouble           y,
                                                 gdouble           width,
                                                 gdouble           height);
static GdkPixbuf * gtk_theming_engine_render_icon_pixbuf (GtkThemingEngine    *engine,
                                                          const GtkIconSource *source,
                                                          GtkIconSize          size);
static void gtk_theming_engine_render_icon (GtkThemingEngine *engine,
                                            cairo_t *cr,
					    GdkPixbuf *pixbuf,
                                            gdouble x,
                                            gdouble y);
static void gtk_theming_engine_render_icon_surface (GtkThemingEngine *engine,
						    cairo_t *cr,
						    cairo_surface_t *surface,
						    gdouble x,
						    gdouble y);

G_DEFINE_TYPE_WITH_PRIVATE (GtkThemingEngine, gtk_theming_engine, G_TYPE_OBJECT)

typedef struct GtkThemingModule GtkThemingModule;
typedef struct GtkThemingModuleClass GtkThemingModuleClass;

struct GtkThemingModule
{
  GTypeModule parent_instance;
  GModule *module;
  gchar *name;

  void (*init) (GTypeModule *module);
  void (*exit) (void);
  GtkThemingEngine * (*create_engine) (void);
};

struct GtkThemingModuleClass
{
  GTypeModuleClass parent_class;
};

#define GTK_TYPE_THEMING_MODULE  (gtk_theming_module_get_type ())
#define GTK_THEMING_MODULE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_THEMING_MODULE, GtkThemingModule))
#define GTK_IS_THEMING_MODULE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_THEMING_MODULE))

_GDK_EXTERN
GType gtk_theming_module_get_type (void);

G_DEFINE_TYPE (GtkThemingModule, gtk_theming_module, G_TYPE_TYPE_MODULE);

static void
gtk_theming_engine_class_init (GtkThemingEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_theming_engine_finalize;
  object_class->set_property = gtk_theming_engine_impl_set_property;
  object_class->get_property = gtk_theming_engine_impl_get_property;

  klass->render_icon = gtk_theming_engine_render_icon;
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
  klass->render_activity = gtk_theming_engine_render_activity;
  klass->render_icon_pixbuf = gtk_theming_engine_render_icon_pixbuf;
  klass->render_icon_surface = gtk_theming_engine_render_icon_surface;

  /**
   * GtkThemingEngine:name:
   *
   * The theming engine name, this name will be used when registering
   * custom properties, for a theming engine named "Clearlooks" registering
   * a "glossy" custom property, it could be referenced in the CSS file as
   *
   * |[
   * -Clearlooks-glossy: true;
   * ]|
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
				   PROP_NAME,
				   g_param_spec_string ("name",
                                                        P_("Name"),
                                                        P_("Theming engine name"),
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | GTK_PARAM_READWRITE));
}

static void
gtk_theming_engine_init (GtkThemingEngine *engine)
{
  engine->priv = gtk_theming_engine_get_instance_private (engine);
}

static void
gtk_theming_engine_finalize (GObject *object)
{
  GtkThemingEnginePrivate *priv;

  priv = GTK_THEMING_ENGINE (object)->priv;
  g_free (priv->name);

  G_OBJECT_CLASS (gtk_theming_engine_parent_class)->finalize (object);
}

static void
gtk_theming_engine_impl_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkThemingEnginePrivate *priv;

  priv = GTK_THEMING_ENGINE (object)->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      g_free (priv->name);

      priv->name = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_theming_engine_impl_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkThemingEnginePrivate *priv;

  priv = GTK_THEMING_ENGINE (object)->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
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

/**
 * gtk_theming_engine_get_property:
 * @engine: a #GtkThemingEngine
 * @property: the property name
 * @state: state to retrieve the value for
 * @value: (out) (transfer full): return location for the property value,
 *         you must free this memory using g_value_unset() once you are
 *         done with it.
 *
 * Gets a property value as retrieved from the style settings that apply
 * to the currently rendered element.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
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

/**
 * gtk_theming_engine_get_valist:
 * @engine: a #GtkThemingEngine
 * @state: state to retrieve values for
 * @args: va_list of property name/return location pairs, followed by %NULL
 *
 * Retrieves several style property values that apply to the currently
 * rendered element.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
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

/**
 * gtk_theming_engine_get:
 * @engine: a #GtkThemingEngine
 * @state: state to retrieve values for
 * @...: property name /return value pairs, followed by %NULL
 *
 * Retrieves several style property values that apply to the currently
 * rendered element.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
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

/**
 * gtk_theming_engine_get_style_property:
 * @engine: a #GtkThemingEngine
 * @property_name: the name of the widget style property
 * @value: (out): Return location for the property value, free with
 *         g_value_unset() after use.
 *
 * Gets the value for a widget style property.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
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

/**
 * gtk_theming_engine_get_style_valist:
 * @engine: a #GtkThemingEngine
 * @args: va_list of property name/return location pairs, followed by %NULL
 *
 * Retrieves several widget style properties from @engine according to the
 * currently rendered content’s style.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
void
gtk_theming_engine_get_style_valist (GtkThemingEngine *engine,
                                     va_list           args)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));

  priv = engine->priv;
  gtk_style_context_get_style_valist (priv->context, args);
}

/**
 * gtk_theming_engine_get_style:
 * @engine: a #GtkThemingEngine
 * @...: property name /return value pairs, followed by %NULL
 *
 * Retrieves several widget style properties from @engine according
 * to the currently rendered content’s style.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
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

/**
 * gtk_theming_engine_lookup_color:
 * @engine: a #GtkThemingEngine
 * @color_name: color name to lookup
 * @color: (out): Return location for the looked up color
 *
 * Looks up and resolves a color name in the current style’s color map.
 *
 * Returns: %TRUE if @color_name was found and resolved, %FALSE otherwise
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
gboolean
gtk_theming_engine_lookup_color (GtkThemingEngine *engine,
                                 const gchar      *color_name,
                                 GdkRGBA          *color)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), FALSE);
  g_return_val_if_fail (color_name != NULL, FALSE);

  priv = engine->priv;
  return gtk_style_context_lookup_color (priv->context, color_name, color);
}

/**
 * gtk_theming_engine_get_state:
 * @engine: a #GtkThemingEngine
 *
 * returns the state used when rendering.
 *
 * Returns: the state flags
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
GtkStateFlags
gtk_theming_engine_get_state (GtkThemingEngine *engine)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), 0);

  priv = engine->priv;
  return gtk_style_context_get_state (priv->context);
}

/**
 * gtk_theming_engine_state_is_running:
 * @engine: a #GtkThemingEngine
 * @state: a widget state
 * @progress: (out): return location for the transition progress
 *
 * Returns %TRUE if there is a transition animation running for the
 * current region (see gtk_style_context_push_animatable_region()).
 *
 * If @progress is not %NULL, the animation progress will be returned
 * there, 0.0 means the state is closest to being %FALSE, while 1.0 means
 * it’s closest to being %TRUE. This means transition animations will
 * run from 0 to 1 when @state is being set to %TRUE and from 1 to 0 when
 * it’s being set to %FALSE.
 *
 * Returns: %TRUE if there is a running transition animation for @state.
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: Always returns %FALSE
 **/
gboolean
gtk_theming_engine_state_is_running (GtkThemingEngine *engine,
                                     GtkStateType      state,
                                     gdouble          *progress)
{
  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), FALSE);

  return FALSE;
}

/**
 * gtk_theming_engine_get_path:
 * @engine: a #GtkThemingEngine
 *
 * Returns the widget path used for style matching.
 *
 * Returns: (transfer none): A #GtkWidgetPath
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
const GtkWidgetPath *
gtk_theming_engine_get_path (GtkThemingEngine *engine)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), NULL);

  priv = engine->priv;
  return gtk_style_context_get_path (priv->context);
}

/**
 * gtk_theming_engine_has_class:
 * @engine: a #GtkThemingEngine
 * @style_class: class name to look up
 *
 * Returns %TRUE if the currently rendered contents have
 * defined the given class name.
 *
 * Returns: %TRUE if @engine has @class_name defined
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
gboolean
gtk_theming_engine_has_class (GtkThemingEngine *engine,
                              const gchar      *style_class)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), FALSE);

  priv = engine->priv;
  return gtk_style_context_has_class (priv->context, style_class);
}

/**
 * gtk_theming_engine_has_region:
 * @engine: a #GtkThemingEngine
 * @style_region: a region name
 * @flags: (out) (allow-none): return location for region flags
 *
 * Returns %TRUE if the currently rendered contents have the
 * region defined. If @flags_return is not %NULL, it is set
 * to the flags affecting the region.
 *
 * Returns: %TRUE if region is defined
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
gboolean
gtk_theming_engine_has_region (GtkThemingEngine *engine,
                               const gchar      *style_region,
                               GtkRegionFlags   *flags)
{
  GtkThemingEnginePrivate *priv;

  if (flags)
    *flags = 0;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), FALSE);

  priv = engine->priv;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return gtk_style_context_has_region (priv->context, style_region, flags);
G_GNUC_END_IGNORE_DEPRECATIONS
}

/**
 * gtk_theming_engine_get_direction:
 * @engine: a #GtkThemingEngine
 *
 * Returns the widget direction used for rendering.
 *
 * Returns: the widget direction
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: Use gtk_theming_engine_get_state() and
 *   check for #GTK_STATE_FLAG_DIR_LTR and
 *   #GTK_STATE_FLAG_DIR_RTL instead.
 **/
GtkTextDirection
gtk_theming_engine_get_direction (GtkThemingEngine *engine)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), GTK_TEXT_DIR_LTR);

  priv = engine->priv;
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  return gtk_style_context_get_direction (priv->context);
  G_GNUC_END_IGNORE_DEPRECATIONS;
}

/**
 * gtk_theming_engine_get_junction_sides:
 * @engine: a #GtkThemingEngine
 *
 * Returns the widget direction used for rendering.
 *
 * Returns: the widget direction
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
GtkJunctionSides
gtk_theming_engine_get_junction_sides (GtkThemingEngine *engine)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), 0);

  priv = engine->priv;
  return gtk_style_context_get_junction_sides (priv->context);
}

/**
 * gtk_theming_engine_get_color:
 * @engine: a #GtkThemingEngine
 * @state: state to retrieve the color for
 * @color: (out): return value for the foreground color
 *
 * Gets the foreground color for a given state.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
void
gtk_theming_engine_get_color (GtkThemingEngine *engine,
                              GtkStateFlags     state,
                              GdkRGBA          *color)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));

  priv = engine->priv;
  gtk_style_context_get_color (priv->context, state, color);
}

/**
 * gtk_theming_engine_get_background_color:
 * @engine: a #GtkThemingEngine
 * @state: state to retrieve the color for
 * @color: (out): return value for the background color
 *
 * Gets the background color for a given state.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
void
gtk_theming_engine_get_background_color (GtkThemingEngine *engine,
                                         GtkStateFlags     state,
                                         GdkRGBA          *color)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));

  priv = engine->priv;
  gtk_style_context_get_background_color (priv->context, state, color);
}

/**
 * gtk_theming_engine_get_border_color:
 * @engine: a #GtkThemingEngine
 * @state: state to retrieve the color for
 * @color: (out): return value for the border color
 *
 * Gets the border color for a given state.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
void
gtk_theming_engine_get_border_color (GtkThemingEngine *engine,
                                     GtkStateFlags     state,
                                     GdkRGBA          *color)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));

  priv = engine->priv;
  gtk_style_context_get_border_color (priv->context, state, color);
}

/**
 * gtk_theming_engine_get_border:
 * @engine: a #GtkThemingEngine
 * @state: state to retrieve the border for
 * @border: (out): return value for the border settings
 *
 * Gets the border for a given state as a #GtkBorder.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
void
gtk_theming_engine_get_border (GtkThemingEngine *engine,
                               GtkStateFlags     state,
                               GtkBorder        *border)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));

  priv = engine->priv;
  gtk_style_context_get_border (priv->context, state, border);
}

/**
 * gtk_theming_engine_get_padding:
 * @engine: a #GtkThemingEngine
 * @state: state to retrieve the padding for
 * @padding: (out): return value for the padding settings
 *
 * Gets the padding for a given state as a #GtkBorder.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
void
gtk_theming_engine_get_padding (GtkThemingEngine *engine,
                                GtkStateFlags     state,
                                GtkBorder        *padding)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));

  priv = engine->priv;
  gtk_style_context_get_padding (priv->context, state, padding);
}

/**
 * gtk_theming_engine_get_margin:
 * @engine: a #GtkThemingEngine
 * @state: state to retrieve the border for
 * @margin: (out): return value for the margin settings
 *
 * Gets the margin for a given state as a #GtkBorder.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
void
gtk_theming_engine_get_margin (GtkThemingEngine *engine,
                               GtkStateFlags     state,
                               GtkBorder        *margin)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));

  priv = engine->priv;
  gtk_style_context_get_margin (priv->context, state, margin);
}

/**
 * gtk_theming_engine_get_font:
 * @engine: a #GtkThemingEngine
 * @state: state to retrieve the font for
 *
 * Returns the font description for a given state.
 *
 * Returns: (transfer none): the #PangoFontDescription for the given
 *          state. This object is owned by GTK+ and should not be
 *          freed.
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: Use gtk_theming_engine_get()
 **/
const PangoFontDescription *
gtk_theming_engine_get_font (GtkThemingEngine *engine,
                             GtkStateFlags     state)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  priv = engine->priv;
  return gtk_style_context_get_font (priv->context, state);
  G_GNUC_END_IGNORE_DEPRECATIONS;
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
    return FALSE;

  module = g_module_open (module_path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  g_free (module_path);

  if (!module)
    return FALSE;

  if (!g_module_symbol (module, "theme_init",
                        (gpointer *) &theming_module->init) ||
      !g_module_symbol (module, "theme_exit",
                        (gpointer *) &theming_module->exit) ||
      !g_module_symbol (module, "create_engine",
                        (gpointer *) &theming_module->create_engine))
    {
      g_module_close (module);

      return FALSE;
    }

  theming_module->module = module;

  theming_module->init (G_TYPE_MODULE (theming_module));

  return TRUE;
}

static void
gtk_theming_module_unload (GTypeModule *type_module)
{
  GtkThemingModule *theming_module;

  theming_module = GTK_THEMING_MODULE (type_module);

  theming_module->exit ();

  g_module_close (theming_module->module);

  theming_module->module = NULL;
  theming_module->init = NULL;
  theming_module->exit = NULL;
  theming_module->create_engine = NULL;
}

static void
gtk_theming_module_class_init (GtkThemingModuleClass *klass)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (klass);

  module_class->load = gtk_theming_module_load;
  module_class->unload = gtk_theming_module_unload;
}

static void
gtk_theming_module_init (GtkThemingModule *module)
{
}

/**
 * gtk_theming_engine_load:
 * @name: Theme engine name to load
 *
 * Loads and initializes a theming engine module from the
 * standard directories.
 *
 * Returns: (transfer none): A theming engine, or %NULL if
 * the engine @name doesn’t exist.
 *
 * Deprecated: 3.14
 **/
GtkThemingEngine *
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
  else
    {
      if (G_UNLIKELY (!default_engine))
        default_engine = g_object_new (GTK_TYPE_THEMING_ENGINE, NULL);

      engine = default_engine;
    }

  return engine;
}

/**
 * gtk_theming_engine_get_screen:
 * @engine: a #GtkThemingEngine
 *
 * Returns the #GdkScreen to which @engine currently rendering to.
 *
 * Returns: (transfer none): a #GdkScreen, or %NULL.
 *
 * Deprecated: 3.14
 **/
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
  gtk_do_render_check (engine->priv->context, cr, x, y, width, height);
}

static void
gtk_theming_engine_render_option (GtkThemingEngine *engine,
                                  cairo_t          *cr,
                                  gdouble           x,
                                  gdouble           y,
                                  gdouble           width,
                                  gdouble           height)
{
  gtk_do_render_option (engine->priv->context, cr, x, y, width, height);
}

static void
gtk_theming_engine_render_arrow (GtkThemingEngine *engine,
                                 cairo_t          *cr,
                                 gdouble           angle,
                                 gdouble           x,
                                 gdouble           y,
                                 gdouble           size)
{
  gtk_do_render_arrow (engine->priv->context, cr, angle, x, y, size);
}

static void
gtk_theming_engine_render_background (GtkThemingEngine *engine,
                                      cairo_t          *cr,
                                      gdouble           x,
                                      gdouble           y,
                                      gdouble           width,
                                      gdouble           height)
{
  gtk_do_render_background (engine->priv->context, cr, x, y, width, height);
}

static void
gtk_theming_engine_render_frame (GtkThemingEngine *engine,
                                 cairo_t          *cr,
                                 gdouble           x,
                                 gdouble           y,
                                 gdouble           width,
                                 gdouble           height)
{
  gtk_do_render_frame (engine->priv->context, cr, x, y, width, height);
}

static void
gtk_theming_engine_render_expander (GtkThemingEngine *engine,
                                    cairo_t          *cr,
                                    gdouble           x,
                                    gdouble           y,
                                    gdouble           width,
                                    gdouble           height)
{
  gtk_do_render_expander (engine->priv->context, cr, x, y, width, height);
}

static void
gtk_theming_engine_render_focus (GtkThemingEngine *engine,
                                 cairo_t          *cr,
                                 gdouble           x,
                                 gdouble           y,
                                 gdouble           width,
                                 gdouble           height)
{
  gtk_do_render_focus (engine->priv->context, cr, x, y, width, height);
}

static void
gtk_theming_engine_render_line (GtkThemingEngine *engine,
                                cairo_t          *cr,
                                gdouble           x0,
                                gdouble           y0,
                                gdouble           x1,
                                gdouble           y1)
{
  gtk_do_render_line (engine->priv->context, cr, x0, y0, x1, y1);
}

static void
gtk_theming_engine_render_layout (GtkThemingEngine *engine,
                                  cairo_t          *cr,
                                  gdouble           x,
                                  gdouble           y,
                                  PangoLayout      *layout)
{
  gtk_do_render_layout (engine->priv->context, cr, x, y, layout);
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
  gtk_do_render_slider (engine->priv->context, cr, x, y, width, height, orientation);
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
  gtk_do_render_frame_gap (engine->priv->context, cr, x, y, width, height, gap_side, xy0_gap, xy1_gap);
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
  gtk_do_render_extension (engine->priv->context, cr, x, y, width, height, gap_side);
}

static void
gtk_theming_engine_render_handle (GtkThemingEngine *engine,
                                  cairo_t          *cr,
                                  gdouble           x,
                                  gdouble           y,
                                  gdouble           width,
                                  gdouble           height)
{
  gtk_do_render_handle (engine->priv->context, cr, x, y, width, height);
}

static void
gtk_theming_engine_render_activity (GtkThemingEngine *engine,
                                    cairo_t          *cr,
                                    gdouble           x,
                                    gdouble           y,
                                    gdouble           width,
                                    gdouble           height)
{
  gtk_do_render_activity (engine->priv->context, cr, x, y, width, height);
}

static GdkPixbuf *
gtk_theming_engine_render_icon_pixbuf (GtkThemingEngine    *engine,
                                       const GtkIconSource *source,
                                       GtkIconSize          size)
{
  return gtk_do_render_icon_pixbuf (engine->priv->context, source, size);
}

static void
gtk_theming_engine_render_icon (GtkThemingEngine *engine,
                                cairo_t *cr,
				GdkPixbuf *pixbuf,
                                gdouble x,
                                gdouble y)
{
  gtk_do_render_icon (engine->priv->context, cr, pixbuf, x, y);
}

static void
gtk_theming_engine_render_icon_surface (GtkThemingEngine *engine,
					cairo_t *cr,
					cairo_surface_t *surface,
					gdouble x,
					gdouble y)
{
  gtk_do_render_icon_surface (engine->priv->context, cr, surface, x, y);
}

G_GNUC_END_IGNORE_DEPRECATIONS
