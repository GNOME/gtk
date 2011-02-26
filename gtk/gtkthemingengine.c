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

#include "gtkprivate.h"
#include "gtk9slice.h"
#include "gtkpango.h"

/**
 * SECTION:gtkthemingengine
 * @Short_description: Theming renderers
 * @Title: GtkThemingEngine
 * @See_also: #GtkStyleContext
 *
 * #GtkThemingEngine is the object used for rendering themed content
 * in GTK+ widgets. Even though GTK+ has a default implementation,
 * it can be overridden in CSS files by enforcing a #GtkThemingEngine
 * object to be loaded as a module.
 *
 * In order to implement a theming engine, a #GtkThemingEngine subclass
 * must be created, alongside the CSS file that will reference it, the
 * theming engine would be created as an .so library, and installed in
 * $(gtk-modules-dir)/theming-engines/.
 *
 * #GtkThemingEngine<!-- -->s have limited access to the object they are
 * rendering, the #GtkThemingEngine API has read-only accessors to the
 * style information contained in the rendered object's #GtkStyleContext.
 */

typedef struct GtkThemingEnginePrivate GtkThemingEnginePrivate;

enum {
  SIDE_LEFT   = 1,
  SIDE_BOTTOM = 1 << 1,
  SIDE_RIGHT  = 1 << 2,
  SIDE_TOP    = 1 << 3,
  SIDE_ALL    = 0xF
};

enum {
  PROP_0,
  PROP_NAME
};

struct GtkThemingEnginePrivate
{
  GtkStyleContext *context;
  gchar *name;
};

#define GTK_THEMING_ENGINE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_THEMING_ENGINE, GtkThemingEnginePrivate))

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

G_DEFINE_TYPE (GtkThemingEngine, gtk_theming_engine, G_TYPE_OBJECT)


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

G_DEFINE_TYPE (GtkThemingModule, gtk_theming_module, G_TYPE_TYPE_MODULE);

static void
gtk_theming_engine_class_init (GtkThemingEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_theming_engine_finalize;
  object_class->set_property = gtk_theming_engine_impl_set_property;
  object_class->get_property = gtk_theming_engine_impl_get_property;

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

  /**
   * GtkThemingEngine:name:
   *
   * The theming engine name, this name will be used when registering
   * custom properties, for a theming engine named "Clearlooks" registering
   * a "glossy" custom property, it could be referenced in the CSS file as
   *
   * <programlisting>
   * -Clearlooks-glossy: true;
   * </programlisting>
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

  g_type_class_add_private (object_class, sizeof (GtkThemingEnginePrivate));
}

static void
gtk_theming_engine_init (GtkThemingEngine *engine)
{
  engine->priv = GTK_THEMING_ENGINE_GET_PRIVATE (engine);
}

static void
gtk_theming_engine_finalize (GObject *object)
{
  GtkThemingEnginePrivate *priv;

  priv = GTK_THEMING_ENGINE (object)->priv;
  g_free (priv->name);

  G_OBJECT_GET_CLASS (gtk_theming_engine_parent_class)->finalize (object);
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
      if (priv->name)
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
 * gtk_theming_engine_register_property: (skip)
 * @name_space: namespace for the property name
 * @parse_func: parsing function to use, or %NULL
 * @pspec: the #GParamSpec for the new property
 *
 * Registers a property so it can be used in the CSS file format,
 * on the CSS file the property will look like
 * "-${@name_space}-${property_name}". being
 * ${property_name} the given to @pspec. @name_space will usually
 * be the theme engine name.
 *
 * For any type a @parse_func may be provided, being this function
 * used for turning any property value (between ':' and ';') in
 * CSS to the #GValue needed. For basic types there is already
 * builtin parsing support, so %NULL may be provided for these
 * cases.
 *
 * <note>
 * Engines must ensure property registration happens exactly once,
 * usually GTK+ deals with theming engines as singletons, so this
 * should be guaranteed to happen once, but bear this in mind
 * when creating #GtkThemeEngine<!-- -->s yourself.
 * </note>
 *
 * <note>
 * In order to make use of the custom registered properties in
 * the CSS file, make sure the engine is loaded first by specifying
 * the engine property, either in a previous rule or within the same
 * one.
 * <programlisting>
 * &ast; {
 *     engine: someengine;
 *     -SomeEngine-custom-property: 2;
 * }
 * </programlisting>
 * </note>
 *
 * Since: 3.0
 **/
void
gtk_theming_engine_register_property (const gchar            *name_space,
                                      GtkStylePropertyParser  parse_func,
                                      GParamSpec             *pspec)
{
  gchar *name;

  g_return_if_fail (name_space != NULL);
  g_return_if_fail (strchr (name_space, ' ') == NULL);
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  /* FIXME: hack hack hack, replacing pspec->name to include namespace */
  name = g_strdup_printf ("-%s-%s", name_space, pspec->name);
  g_free (pspec->name);
  pspec->name = name;

  gtk_style_properties_register_property (parse_func, pspec);
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
 * @value: Return location for the property value, free with
 *         g_value_unset() after use.
 *
 * Gets the value for a widget style property.
 *
 * Since: 3.0
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
 * currently rendered content's style.
 *
 * Since: 3.0
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
 * to the currently rendered content's style.
 *
 * Since: 3.0
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
 * Looks up and resolves a color name in the current style's color map.
 *
 * Returns: %TRUE if @color_name was found and resolved, %FALSE otherwise
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
 * it's closest to being %TRUE. This means transition animations will
 * run from 0 to 1 when @state is being set to %TRUE and from 1 to 0 when
 * it's being set to %FALSE.
 *
 * Returns: %TRUE if there is a running transition animation for @state.
 *
 * Since: 3.0
 **/
gboolean
gtk_theming_engine_state_is_running (GtkThemingEngine *engine,
                                     GtkStateType      state,
                                     gdouble          *progress)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), FALSE);

  priv = engine->priv;
  return gtk_style_context_state_is_running (priv->context, state, progress);
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
 **/
G_CONST_RETURN GtkWidgetPath *
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
  return gtk_style_context_has_region (priv->context, style_region, flags);
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
 **/
GtkTextDirection
gtk_theming_engine_get_direction (GtkThemingEngine *engine)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), GTK_TEXT_DIR_LTR);

  priv = engine->priv;
  return gtk_style_context_get_direction (priv->context);
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
 * @engine: a #GtkthemingEngine
 * @state: state to retrieve the border for
 * @border: (out): return value for the border settings
 *
 * Gets the border for a given state as a #GtkBorder.
 *
 * Since: 3.0
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
 * @engine: a #GtkthemingEngine
 * @state: state to retrieve the padding for
 * @padding: (out): return value for the padding settings
 *
 * Gets the padding for a given state as a #GtkBorder.
 *
 * Since: 3.0
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
 **/
const PangoFontDescription *
gtk_theming_engine_get_font (GtkThemingEngine *engine,
                             GtkStateFlags     state)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), NULL);

  priv = engine->priv;
  return gtk_style_context_get_font (priv->context, state);
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
 * the engine @name doesn't exist.
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

  if (!engine)
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
  GdkRGBA *fg_color, *bg_color, *border_color;
  GtkStateFlags flags;
  gint exterior_size, interior_size, thickness, pad;
  GtkBorderStyle border_style;
  GtkBorder *border;
  gint border_width;

  flags = gtk_theming_engine_get_state (engine);
  cairo_save (cr);

  gtk_theming_engine_get (engine, flags,
                          "color", &fg_color,
                          "background-color", &bg_color,
                          "border-color", &border_color,
                          "border-style", &border_style,
                          "border-width", &border,
                          NULL);

  border_width = MIN (MIN (border->top, border->bottom),
                      MIN (border->left, border->right));
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

  if (border_style == GTK_BORDER_STYLE_SOLID)
    {
      cairo_set_line_width (cr, border_width);

      cairo_rectangle (cr, x + 0.5, y + 0.5, exterior_size - 1, exterior_size - 1);
      gdk_cairo_set_source_rgba (cr, bg_color);
      cairo_fill_preserve (cr);

      if (border_color)
        gdk_cairo_set_source_rgba (cr, border_color);
      else
        gdk_cairo_set_source_rgba (cr, fg_color);

      cairo_stroke (cr);
    }

  gdk_cairo_set_source_rgba (cr, fg_color);

  if (flags & GTK_STATE_FLAG_INCONSISTENT)
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
      gboolean running;

      running = gtk_theming_engine_state_is_running (engine, GTK_STATE_ACTIVE, &progress);

      if ((flags & GTK_STATE_FLAG_ACTIVE) || running)
        {
          if (!running)
            progress = 1;

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

  gdk_rgba_free (fg_color);
  gdk_rgba_free (bg_color);
  gdk_rgba_free (border_color);
  gtk_border_free (border);
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
  GdkRGBA *fg_color, *bg_color, *border_color;
  gint exterior_size, interior_size, pad, thickness, border_width;
  GtkBorderStyle border_style;
  GtkBorder *border;

  flags = gtk_theming_engine_get_state (engine);

  cairo_save (cr);

  gtk_theming_engine_get (engine, flags,
                          "color", &fg_color,
                          "background-color", &bg_color,
                          "border-color", &border_color,
                          "border-style", &border_style,
                          "border-width", &border,
                          NULL);

  exterior_size = MIN (width, height);
  border_width = MIN (MIN (border->top, border->bottom),
                      MIN (border->left, border->right));

  if (exterior_size % 2 == 0) /* Ensure odd */
    exterior_size -= 1;

  x -= (1 + exterior_size - width) / 2;
  y -= (1 + exterior_size - height) / 2;

  if (border_style == GTK_BORDER_STYLE_SOLID)
    {
      cairo_set_line_width (cr, border_width);

      cairo_new_sub_path (cr);
      cairo_arc (cr,
                 x + exterior_size / 2.,
                 y + exterior_size / 2.,
                 (exterior_size - 1) / 2.,
                 0, 2 * G_PI);

      gdk_cairo_set_source_rgba (cr, bg_color);
      cairo_fill_preserve (cr);

      if (border_color)
        gdk_cairo_set_source_rgba (cr, border_color);
      else
        gdk_cairo_set_source_rgba (cr, fg_color);

      cairo_stroke (cr);
    }

  gdk_cairo_set_source_rgba (cr, fg_color);

  /* FIXME: thickness */
  thickness = 1;

  if (flags & GTK_STATE_FLAG_INCONSISTENT)
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
  if (flags & GTK_STATE_FLAG_ACTIVE)
    {
      pad = thickness + MAX (1, 2 * (exterior_size - 2 * thickness) / 9);
      interior_size = MAX (1, exterior_size - 2 * pad);

      if (interior_size < 5)
        {
          interior_size = 7;
          pad = MAX (0, (exterior_size - interior_size) / 2);
        }

      cairo_new_sub_path (cr);
      cairo_arc (cr,
                 x + pad + interior_size / 2.,
                 y + pad + interior_size / 2.,
                 interior_size / 2.,
                 0, 2 * G_PI);
      cairo_fill (cr);
    }

  cairo_restore (cr);

  gdk_rgba_free (fg_color);
  gdk_rgba_free (bg_color);
  gdk_rgba_free (border_color);
  gtk_border_free (border);
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
  GdkRGBA *fg_color;

  cairo_save (cr);

  flags = gtk_theming_engine_get_state (engine);

  gtk_theming_engine_get (engine, flags,
                          "color", &fg_color,
                          NULL);

  if (flags & GTK_STATE_FLAG_INSENSITIVE)
    {
      add_path_arrow (cr, angle, x + 1, y + 1, size);
      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_fill (cr);
    }

  add_path_arrow (cr, angle, x, y, size);
  gdk_cairo_set_source_rgba (cr, fg_color);
  cairo_fill (cr);

  cairo_restore (cr);

  gdk_rgba_free (fg_color);
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
color_shade (const GdkRGBA *color,
             gdouble        factor,
             GdkRGBA       *color_return)
{
  GtkSymbolicColor *literal, *shade;

  literal = gtk_symbolic_color_new_literal (color);
  shade = gtk_symbolic_color_new_shade (literal, factor);
  gtk_symbolic_color_unref (literal);

  gtk_symbolic_color_resolve (shade, NULL, color_return);
  gtk_symbolic_color_unref (shade);
}

static void
_cairo_round_rectangle_sides (cairo_t          *cr,
                              gdouble           radius,
                              gdouble           x,
                              gdouble           y,
                              gdouble           width,
                              gdouble           height,
                              guint             sides,
                              GtkJunctionSides  junction)
{
  radius = CLAMP (radius, 0, MIN (width / 2, height / 2));

  if (sides & SIDE_RIGHT)
    {
      if (radius == 0 ||
          (junction & GTK_JUNCTION_CORNER_TOPRIGHT))
        cairo_move_to (cr, x + width, y);
      else
        {
          cairo_new_sub_path (cr);
          cairo_arc (cr, x + width - radius, y + radius, radius, - G_PI / 4, 0);
        }

      if (radius == 0 ||
          (junction & GTK_JUNCTION_CORNER_BOTTOMRIGHT))
        cairo_line_to (cr, x + width, y + height);
      else
        cairo_arc (cr, x + width - radius, y + height - radius, radius, 0, G_PI / 4);
    }

  if (sides & SIDE_BOTTOM)
    {
      if (radius != 0 &&
          ! (junction & GTK_JUNCTION_CORNER_BOTTOMRIGHT))
        {
          if ((sides & SIDE_RIGHT) == 0)
            cairo_new_sub_path (cr);

          cairo_arc (cr, x + width - radius, y + height - radius, radius, G_PI / 4, G_PI / 2);
        }
      else if ((sides & SIDE_RIGHT) == 0)
        cairo_move_to (cr, x + width, y + height);

      if (radius == 0 ||
          (junction & GTK_JUNCTION_CORNER_BOTTOMLEFT))
        cairo_line_to (cr, x, y + height);
      else
        cairo_arc (cr, x + radius, y + height - radius, radius, G_PI / 2, 3 * (G_PI / 4));
    }
  else
    cairo_move_to (cr, x, y + height);

  if (sides & SIDE_LEFT)
    {
      if (radius != 0 &&
          ! (junction & GTK_JUNCTION_CORNER_BOTTOMLEFT))
        {
          if ((sides & SIDE_BOTTOM) == 0)
            cairo_new_sub_path (cr);

          cairo_arc (cr, x + radius, y + height - radius, radius, 3 * (G_PI / 4), G_PI);
        }
      else if ((sides & SIDE_BOTTOM) == 0)
        cairo_move_to (cr, x, y + height);

      if (radius == 0 ||
          (junction & GTK_JUNCTION_CORNER_TOPLEFT))
        cairo_line_to (cr, x, y);
      else
        cairo_arc (cr, x + radius, y + radius, radius, G_PI, G_PI + G_PI / 4);
    }

  if (sides & SIDE_TOP)
    {
      if (radius != 0 &&
          ! (junction & GTK_JUNCTION_CORNER_TOPLEFT))
        {
          if ((sides & SIDE_LEFT) == 0)
            cairo_new_sub_path (cr);

          cairo_arc (cr, x + radius, y + radius, radius, 5 * (G_PI / 4), 3 * (G_PI / 2));
        }
      else if ((sides & SIDE_LEFT) == 0)
        cairo_move_to (cr, x, y);

      if (radius == 0 ||
          (junction & GTK_JUNCTION_CORNER_TOPRIGHT))
        cairo_line_to (cr, x + width, y);
      else
        cairo_arc (cr, x + width - radius, y + radius, radius, 3 * (G_PI / 2), - G_PI / 4);
    }
}

/* Set the appropriate matrix for
 * patterns coming from the style context
 */
static void
style_pattern_set_matrix (cairo_pattern_t *pattern,
                          gdouble          width,
                          gdouble          height)
{
  cairo_matrix_t matrix;
  gint w, h;

  if (cairo_pattern_get_type (pattern) == CAIRO_PATTERN_TYPE_SURFACE)
    {
      cairo_surface_t *surface;

      cairo_pattern_get_surface (pattern, &surface);
      w = cairo_image_surface_get_width (surface);
      h = cairo_image_surface_get_height (surface);
    }
  else
    w = h = 1;

  cairo_matrix_init_scale (&matrix, (gdouble) w / width, (gdouble) h / height);
  cairo_pattern_set_matrix (pattern, &matrix);
}

static void
render_background_internal (GtkThemingEngine *engine,
                            cairo_t          *cr,
                            gdouble           x,
                            gdouble           y,
                            gdouble           width,
                            gdouble           height,
                            GtkJunctionSides  junction)
{
  GdkRGBA *bg_color;
  cairo_pattern_t *pattern;
  GtkStateFlags flags;
  gboolean running;
  gdouble progress, alpha = 1;
  GtkBorder *border;
  gint radius, border_width;
  GtkBorderStyle border_style;
  gdouble mat_w, mat_h;
  cairo_matrix_t identity;

  /* Use unmodified size for pattern scaling */
  mat_w = width;
  mat_h = height;

  flags = gtk_theming_engine_get_state (engine);

  cairo_matrix_init_identity (&identity);

  gtk_theming_engine_get (engine, flags,
                          "background-image", &pattern,
                          "background-color", &bg_color,
                          "border-radius", &radius,
                          "border-width", &border,
                          "border-style", &border_style,
                          NULL);

  border_width = MIN (MIN (border->top, border->bottom),
                      MIN (border->left, border->right));

  if (border_width > 1 &&
      border_style == GTK_BORDER_STYLE_NONE)
    {
      cairo_set_line_width (cr, border_width);

      x += (gdouble) border_width / 2;
      y += (gdouble) border_width / 2;
      width -= border_width;
      height -= border_width;
    }
  else
    {
      x += border->left;
      y += border->top;
      width -= border->left + border->right;
      height -= border->top + border->bottom;
    }

  if (width <= 0 || height <= 0)
    return;

  cairo_save (cr);
  cairo_translate (cr, x, y);

  running = gtk_theming_engine_state_is_running (engine, GTK_STATE_PRELIGHT, &progress);

  if (gtk_theming_engine_has_class (engine, "background"))
    {
      cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0); /* transparent */
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_paint (cr);
    }

  if (running)
    {
      cairo_pattern_t *other_pattern;
      GtkStateFlags other_flags;
      GdkRGBA *other_bg;
      cairo_pattern_t *new_pattern = NULL;

      if (flags & GTK_STATE_FLAG_PRELIGHT)
        {
          other_flags = flags & ~(GTK_STATE_FLAG_PRELIGHT);
          progress = 1 - progress;
        }
      else
        other_flags = flags | GTK_STATE_FLAG_PRELIGHT;

      gtk_theming_engine_get (engine, other_flags,
                              "background-image", &other_pattern,
                              "background-color", &other_bg,
                              NULL);

      if (pattern && other_pattern)
        {
          cairo_pattern_type_t type, other_type;
          gint n0, n1;

          cairo_pattern_get_color_stop_count (pattern, &n0);
          cairo_pattern_get_color_stop_count (other_pattern, &n1);
          type = cairo_pattern_get_type (pattern);
          other_type = cairo_pattern_get_type (other_pattern);

          if (type == other_type && n0 == n1)
            {
              gdouble offset0, red0, green0, blue0, alpha0;
              gdouble offset1, red1, green1, blue1, alpha1;
              gdouble x00, x01, y00, y01, x10, x11, y10, y11;
              gdouble r00, r01, r10, r11;
              guint i;

              if (type == CAIRO_PATTERN_TYPE_LINEAR)
                {
                  cairo_pattern_get_linear_points (pattern, &x00, &y00, &x01, &y01);
                  cairo_pattern_get_linear_points (other_pattern, &x10, &y10, &x11, &y11);

                  new_pattern = cairo_pattern_create_linear (x00 + (x10 - x00) * progress,
                                                             y00 + (y10 - y00) * progress,
                                                             x01 + (x11 - x01) * progress,
                                                             y01 + (y11 - y01) * progress);
                }
              else
                {
                  cairo_pattern_get_radial_circles (pattern, &x00, &y00, &r00, &x01, &y01, &r01);
                  cairo_pattern_get_radial_circles (other_pattern, &x10, &y10, &r10, &x11, &y11, &r11);

                  new_pattern = cairo_pattern_create_radial (x00 + (x10 - x00) * progress,
                                                             y00 + (y10 - y00) * progress,
                                                             r00 + (r10 - r00) * progress,
                                                             x01 + (x11 - x01) * progress,
                                                             y01 + (y11 - y01) * progress,
                                                             r01 + (r11 - r01) * progress);
                }

              cairo_pattern_set_filter (new_pattern, CAIRO_FILTER_FAST);
              i = 0;

              /* Blend both gradients into one */
              while (i < n0 && i < n1)
                {
                  cairo_pattern_get_color_stop_rgba (pattern, i,
                                                     &offset0,
                                                     &red0, &green0, &blue0,
                                                     &alpha0);
                  cairo_pattern_get_color_stop_rgba (other_pattern, i,
                                                     &offset1,
                                                     &red1, &green1, &blue1,
                                                     &alpha1);

                  cairo_pattern_add_color_stop_rgba (new_pattern,
                                                     offset0 + ((offset1 - offset0) * progress),
                                                     red0 + ((red1 - red0) * progress),
                                                     green0 + ((green1 - green0) * progress),
                                                     blue0 + ((blue1 - blue0) * progress),
                                                     alpha0 + ((alpha1 - alpha0) * progress));
                  i++;
                }
            }
          else
            {
              /* Different pattern types, or different color
               * stop counts, alpha blend both patterns.
               */
              _cairo_round_rectangle_sides (cr, (gdouble) radius,
                                            0, 0, width, height,
                                            SIDE_ALL, junction);

              style_pattern_set_matrix (other_pattern, mat_w, mat_h);
              cairo_set_source (cr, other_pattern);
              cairo_fill_preserve (cr);

              cairo_pattern_set_matrix (other_pattern, &identity);

              /* Set alpha for posterior drawing
               * of the target pattern
               */
              alpha = 1 - progress;
            }
        }
      else if (pattern || other_pattern)
        {
          cairo_pattern_t *p;
          const GdkRGBA *c;
          gdouble x0, y0, x1, y1, r0, r1;
          gint n, i;

          /* Blend a pattern with a color */
          if (pattern)
            {
              p = pattern;
	      c = other_bg;
              progress = 1 - progress;
            }
          else
            {
              p = other_pattern;
	      c = bg_color;
            }

          if (cairo_pattern_get_type (p) == CAIRO_PATTERN_TYPE_LINEAR)
            {
              cairo_pattern_get_linear_points (p, &x0, &y0, &x1, &y1);
              new_pattern = cairo_pattern_create_linear (x0, y0, x1, y1);
            }
          else
            {
              cairo_pattern_get_radial_circles (p, &x0, &y0, &r0, &x1, &y1, &r1);
              new_pattern = cairo_pattern_create_radial (x0, y0, r0, x1, y1, r1);
            }

          cairo_pattern_get_color_stop_count (p, &n);

          for (i = 0; i < n; i++)
            {
              gdouble red1, green1, blue1, alpha1;
              gdouble offset;

              cairo_pattern_get_color_stop_rgba (p, i,
                                                 &offset,
                                                 &red1, &green1, &blue1,
                                                 &alpha1);
              cairo_pattern_add_color_stop_rgba (new_pattern, offset,
                                                 c->red + ((red1 - c->red) * progress),
                                                 c->green + ((green1 - c->green) * progress),
                                                 c->blue + ((blue1 - c->blue) * progress),
                                                 c->alpha + ((alpha1 - c->alpha) * progress));
            }
        }
      else
        {
          const GdkRGBA *color, *other_color;

          /* Merge just colors */
          color = bg_color;
          other_color = other_bg;

          new_pattern = cairo_pattern_create_rgba (CLAMP (color->red + ((other_color->red - color->red) * progress), 0, 1),
                                                   CLAMP (color->green + ((other_color->green - color->green) * progress), 0, 1),
                                                   CLAMP (color->blue + ((other_color->blue - color->blue) * progress), 0, 1),
                                                   CLAMP (color->alpha + ((other_color->alpha - color->alpha) * progress), 0, 1));
        }

      if (new_pattern)
        {
          /* Replace pattern to use */
          cairo_pattern_destroy (pattern);
          pattern = new_pattern;
        }

      if (other_pattern)
        cairo_pattern_destroy (other_pattern);

      if (other_bg)
        gdk_rgba_free (other_bg);
    }

  _cairo_round_rectangle_sides (cr, (gdouble) radius,
                                0, 0, width, height,
                                SIDE_ALL, junction);
  if (pattern)
    {
      style_pattern_set_matrix (pattern, mat_w, mat_h);
      cairo_set_source (cr, pattern);
    }
  else
    gdk_cairo_set_source_rgba (cr, bg_color);

  if (alpha == 1)
    {
      if (border_width > 1 &&
          border_style != GTK_BORDER_STYLE_NONE)
        {
          /* stroke with the same source, so the background
           * has exactly the shape than the frame, this
           * is important so gtk_render_background() and
           * gtk_render_frame() fit perfectly with round
           * borders.
           */
          cairo_fill_preserve (cr);
          cairo_stroke (cr);
        }
      else
        cairo_fill (cr);
    }
  else
    {
      cairo_save (cr);
      _cairo_round_rectangle_sides (cr, (gdouble) radius,
                                    0, 0, width, height,
                                    SIDE_ALL, junction);
      cairo_clip (cr);

      cairo_paint_with_alpha (cr, alpha);

      cairo_restore (cr);
    }

  if (pattern)
    {
      cairo_pattern_set_matrix (pattern, &identity);
      cairo_pattern_destroy (pattern);
    }

  cairo_restore (cr);

  gdk_rgba_free (bg_color);
  gtk_border_free (border);
}

static void
gtk_theming_engine_render_background (GtkThemingEngine *engine,
                                      cairo_t          *cr,
                                      gdouble           x,
                                      gdouble           y,
                                      gdouble           width,
                                      gdouble           height)
{
  GtkJunctionSides junction;

  junction = gtk_theming_engine_get_junction_sides (engine);

  render_background_internal (engine, cr,
                              x, y, width, height,
                              junction);
}

/* Renders the small triangle on corners so
 * frames with 0 radius have a 3D-like effect
 */
static void
_cairo_corner_triangle (cairo_t *cr,
                        gdouble  x,
                        gdouble  y,
                        gint     size)
{
  gint i;

  cairo_move_to (cr, x + 0.5, y + size - 0.5);
  cairo_line_to (cr, x + size - 0.5, y + size - 0.5);
  cairo_line_to (cr, x + size - 0.5, y + 0.5);

  for (i = 1; i < size - 1; i++)
    {
      cairo_move_to (cr, x + size - 0.5, y + i + 0.5);
      cairo_line_to (cr, x + (size - i) - 0.5, y + i + 0.5);
    }
}

static void
render_frame_internal (GtkThemingEngine *engine,
                       cairo_t          *cr,
                       gdouble           x,
                       gdouble           y,
                       gdouble           width,
                       gdouble           height,
                       guint             hidden_side,
                       GtkJunctionSides  junction)
{
  GtkStateFlags state;
  GdkRGBA lighter;
  GdkRGBA *border_color;
  GtkBorderStyle border_style;
  gint border_width, radius;
  gdouble progress, d1, d2, m;
  gboolean running;
  GtkBorder *border;

  state = gtk_theming_engine_get_state (engine);
  gtk_theming_engine_get (engine, state,
                          "border-color", &border_color,
                          "border-style", &border_style,
                          "border-width", &border,
                          "border-radius", &radius,
                          NULL);

  running = gtk_theming_engine_state_is_running (engine, GTK_STATE_PRELIGHT, &progress);
  border_width = MIN (MIN (border->top, border->bottom),
                      MIN (border->left, border->right));

  if (running)
    {
      GtkStateFlags other_state;
      GdkRGBA *other_color;

      if (state & GTK_STATE_FLAG_PRELIGHT)
        {
          other_state = state & ~(GTK_STATE_FLAG_PRELIGHT);
          progress = 1 - progress;
        }
      else
        other_state = state | GTK_STATE_FLAG_PRELIGHT;

      gtk_theming_engine_get (engine, other_state,
                              "border-color", &other_color,
                              NULL);

      border_color->red = CLAMP (border_color->red + ((other_color->red - border_color->red) * progress), 0, 1);
      border_color->green = CLAMP (border_color->green + ((other_color->green - border_color->green) * progress), 0, 1);
      border_color->blue = CLAMP (border_color->blue + ((other_color->blue - border_color->blue) * progress), 0, 1);
      border_color->alpha = CLAMP (border_color->alpha + ((other_color->alpha - border_color->alpha) * progress), 0, 1);

      gdk_rgba_free (other_color);
    }

  cairo_save (cr);

  color_shade (border_color, 1.8, &lighter);

  switch (border_style)
    {
    case GTK_BORDER_STYLE_NONE:
      break;
    case GTK_BORDER_STYLE_SOLID:
      cairo_set_line_width (cr, border_width);
      cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

      if (border_width > 1)
        {
          x += (gdouble) border_width / 2;
          y += (gdouble) border_width / 2;
          width -= border_width;
          height -= border_width;
        }
      else if (border_width == 1)
        {
          x += 0.5;
          y += 0.5;
          width -= 1;
          height -= 1;
        }

      _cairo_round_rectangle_sides (cr, (gdouble) radius,
                                    x, y, width, height,
                                    SIDE_ALL & ~(hidden_side),
                                    junction);
      gdk_cairo_set_source_rgba (cr, border_color);
      cairo_stroke (cr);

      break;
    case GTK_BORDER_STYLE_INSET:
    case GTK_BORDER_STYLE_OUTSET:
      cairo_set_line_width (cr, border_width);

      if (radius == 0)
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
      else
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);

      if (border_width > 1)
        {
          d1 = (gdouble) border_width / 2;
          d2 = border_width;
        }
      else
        {
          d1 = 0.5;
          d2 = 1;
        }

      cairo_save (cr);

      m = MIN (width, height);
      m /= 2;

      if (border_style == GTK_BORDER_STYLE_INSET)
        gdk_cairo_set_source_rgba (cr, &lighter);
      else
        gdk_cairo_set_source_rgba (cr, border_color);

      _cairo_round_rectangle_sides (cr, (gdouble) radius,
                                    x + d1, y + d1,
                                    width - d2, height - d2,
                                    (SIDE_BOTTOM | SIDE_RIGHT) & ~(hidden_side),
                                    junction);
      cairo_stroke (cr);

      if (border_style == GTK_BORDER_STYLE_INSET)
        gdk_cairo_set_source_rgba (cr, border_color);
      else
        gdk_cairo_set_source_rgba (cr, &lighter);

      _cairo_round_rectangle_sides (cr, (gdouble) radius,
                                    x + d1, y + d1,
                                    width - d2, height - d2,
                                    (SIDE_TOP | SIDE_LEFT) & ~(hidden_side),
                                    junction);
      cairo_stroke (cr);

      if (border_width > 1)
        {
          /* overprint top/right and bottom/left corner
           * triangles if there are square corners there,
           * to give the box a 3D-like appearance.
           */
          cairo_save (cr);

          if (border_style == GTK_BORDER_STYLE_INSET)
            gdk_cairo_set_source_rgba (cr, &lighter);
          else
            gdk_cairo_set_source_rgba (cr, border_color);

          cairo_set_line_width (cr, 1);

          if (radius == 0 ||
              (junction & GTK_JUNCTION_CORNER_TOPRIGHT) != 0)
            _cairo_corner_triangle (cr,
                                    x + width - border_width, y,
                                    border_width);

          if (radius == 0 ||
              (junction & GTK_JUNCTION_CORNER_BOTTOMLEFT) != 0)
            _cairo_corner_triangle (cr,
                                    x, y + height - border_width,
                                    border_width);
          cairo_stroke (cr);
          cairo_restore (cr);
        }

      cairo_restore (cr);
      break;
    }

  cairo_restore (cr);

  if (border_color)
    gdk_rgba_free (border_color);

  gtk_border_free (border);
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
  Gtk9Slice *slice;
  GtkBorderStyle border_style;
  GtkJunctionSides junction;

  flags = gtk_theming_engine_get_state (engine);
  junction = gtk_theming_engine_get_junction_sides (engine);

  gtk_theming_engine_get (engine, flags,
                          "border-image", &slice,
                          "border-style", &border_style,
                          NULL);

  if (slice)
    {
      _gtk_9slice_render (slice, cr, x, y, width, height);
      _gtk_9slice_unref (slice);
    }
  else if (border_style != GTK_BORDER_STYLE_NONE)
    render_frame_internal (engine, cr,
                           x, y, width, height,
                           0, junction);
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
  GdkRGBA *outline_color, *fg_color;
  double vertical_overshoot;
  int diameter;
  double radius;
  double interp;		/* interpolation factor for center position */
  double x_double_horz, y_double_horz;
  double x_double_vert, y_double_vert;
  double x_double, y_double;
  gdouble angle;
  gint line_width;
  gboolean running, is_rtl;
  gdouble progress;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  gtk_theming_engine_get (engine, flags,
                          "color", &fg_color,
                          NULL);
  gtk_theming_engine_get (engine, flags,
                          "border-color", &outline_color,
                          NULL);

  running = gtk_theming_engine_state_is_running (engine, GTK_STATE_ACTIVE, &progress);
  is_rtl = (gtk_theming_engine_get_direction (engine) == GTK_TEXT_DIR_RTL);
  line_width = 1;

  if (!running)
    progress = (flags & GTK_STATE_FLAG_ACTIVE) ? 1 : 0;

  if (!gtk_theming_engine_has_class (engine, GTK_STYLE_CLASS_HORIZONTAL))
    {
      if (is_rtl)
        angle = (G_PI) - ((G_PI / 2) * progress);
      else
        angle = (G_PI / 2) * progress;
    }
  else
    {
      if (is_rtl)
        angle = (G_PI / 2) + ((G_PI / 2) * progress);
      else
        angle = (G_PI / 2) - ((G_PI / 2) * progress);
    }

  interp = progress;

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

  gdk_cairo_set_source_rgba (cr, fg_color);

  cairo_fill_preserve (cr);

  gdk_cairo_set_source_rgba (cr, outline_color);
  cairo_stroke (cr);

  cairo_restore (cr);

  gdk_rgba_free (fg_color);
  gdk_rgba_free (outline_color);
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
  GdkRGBA *color;
  gint line_width;
  gint8 *dash_list;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  gtk_theming_engine_get (engine, flags,
                          "color", &color,
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

  gdk_cairo_set_source_rgba (cr, color);
  cairo_stroke (cr);

  cairo_restore (cr);

  gdk_rgba_free (color);
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
  GdkRGBA *bg_color, darker, lighter;
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
      gdk_cairo_set_source_rgba (cr, &lighter);
      add_path_line (cr, len - i - 1.5, y0, len - 0.5, y0);
      cairo_stroke (cr);

      gdk_cairo_set_source_rgba (cr, &darker);
      add_path_line (cr, 0.5, y0, len - i - 1.5, y0);
      cairo_stroke (cr);

      y0++;
    }

  for (i = 0; i < thickness_light; i++)
    {
      gdk_cairo_set_source_rgba (cr, &darker);
      add_path_line (cr, 0.5, y0, thickness_light - i + 0.5, y0);
      cairo_stroke (cr);

      gdk_cairo_set_source_rgba (cr, &lighter);
      add_path_line (cr, thickness_light - i + 0.5, y0, len - 0.5, y0);
      cairo_stroke (cr);

      y0++;
    }

  cairo_restore (cr);

  gdk_rgba_free (bg_color);
}

static void
gtk_theming_engine_render_layout (GtkThemingEngine *engine,
                                  cairo_t          *cr,
                                  gdouble           x,
                                  gdouble           y,
                                  PangoLayout      *layout)
{
  const PangoMatrix *matrix;
  GdkRGBA *fg_color;
  GtkStateFlags flags;
  gdouble progress;
  gboolean running;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  gtk_theming_engine_get (engine, flags,
                          "color", &fg_color,
                          NULL);

  matrix = pango_context_get_matrix (pango_layout_get_context (layout));

  running = gtk_theming_engine_state_is_running (engine, GTK_STATE_PRELIGHT, &progress);

  if (running)
    {
      GtkStateFlags other_flags;
      GdkRGBA *other_fg;

      if (flags & GTK_STATE_FLAG_PRELIGHT)
        {
          other_flags = flags & ~(GTK_STATE_FLAG_PRELIGHT);
          progress = 1 - progress;
        }
      else
        other_flags = flags | GTK_STATE_FLAG_PRELIGHT;

      gtk_theming_engine_get (engine, other_flags,
                              "color", &other_fg,
                              NULL);

      if (fg_color && other_fg)
        {
          fg_color->red = CLAMP (fg_color->red + ((other_fg->red - fg_color->red) * progress), 0, 1);
          fg_color->green = CLAMP (fg_color->green + ((other_fg->green - fg_color->green) * progress), 0, 1);
          fg_color->blue = CLAMP (fg_color->blue + ((other_fg->blue - fg_color->blue) * progress), 0, 1);
          fg_color->alpha = CLAMP (fg_color->alpha + ((other_fg->alpha - fg_color->alpha) * progress), 0, 1);
        }

      if (other_fg)
        gdk_rgba_free (other_fg);
    }

  if (matrix)
    {
      cairo_matrix_t cairo_matrix;
      PangoRectangle rect;

      cairo_matrix_init (&cairo_matrix,
                         matrix->xx, matrix->yx,
                         matrix->xy, matrix->yy,
                         matrix->x0, matrix->y0);

      pango_layout_get_extents (layout, NULL, &rect);
      pango_matrix_transform_rectangle (matrix, &rect);
      pango_extents_to_pixels (&rect, NULL);

      cairo_matrix.x0 += x - rect.x;
      cairo_matrix.y0 += y - rect.y;

      cairo_set_matrix (cr, &cairo_matrix);
    }
  else
    cairo_move_to (cr, x, y);

  if (flags & GTK_STATE_FLAG_INSENSITIVE)
    {
      cairo_save (cr);
      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_move_to (cr, x + 1, y + 1);
      _gtk_pango_fill_layout (cr, layout);
      cairo_restore (cr);
    }

  gdk_cairo_set_source_rgba (cr, fg_color);
  pango_cairo_show_layout (cr, layout);

  cairo_restore (cr);

  gdk_rgba_free (fg_color);
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
  GtkJunctionSides junction;
  GtkStateFlags state;
  gint border_width, radius;
  gdouble x0, y0, x1, y1, xc, yc, wc, hc;
  GtkBorder *border;

  xc = yc = wc = hc = 0;
  state = gtk_theming_engine_get_state (engine);
  junction = gtk_theming_engine_get_junction_sides (engine);
  gtk_theming_engine_get (engine, state,
                          "border-width", &border,
                          "border-radius", &radius,
                          NULL);

  border_width = MIN (MIN (border->top, border->bottom),
                      MIN (border->left, border->right));

  cairo_save (cr);

  switch (gap_side)
    {
    case GTK_POS_TOP:
      xc = x + xy0_gap + border_width;
      yc = y;
      wc = MAX (xy1_gap - xy0_gap - 2 * border_width, 0);
      hc = border_width;

      if (xy0_gap < radius)
        junction |= GTK_JUNCTION_CORNER_TOPLEFT;

      if (xy1_gap > width - radius)
        junction |= GTK_JUNCTION_CORNER_TOPRIGHT;
      break;
    case GTK_POS_BOTTOM:
      xc = x + xy0_gap + border_width;
      yc = y + height - border_width;
      wc = MAX (xy1_gap - xy0_gap - 2 * border_width, 0);
      hc = border_width;

      if (xy0_gap < radius)
        junction |= GTK_JUNCTION_CORNER_BOTTOMLEFT;

      if (xy1_gap > width - radius)
        junction |= GTK_JUNCTION_CORNER_BOTTOMRIGHT;

      break;
    case GTK_POS_LEFT:
      xc = x;
      yc = y + xy0_gap + border_width;
      wc = border_width;
      hc = MAX (xy1_gap - xy0_gap - 2 * border_width, 0);

      if (xy0_gap < radius)
        junction |= GTK_JUNCTION_CORNER_TOPLEFT;

      if (xy1_gap > height - radius)
        junction |= GTK_JUNCTION_CORNER_BOTTOMLEFT;

      break;
    case GTK_POS_RIGHT:
      xc = x + width - border_width;
      yc = y + xy0_gap + border_width;
      wc = border_width;
      hc = MAX (xy1_gap - xy0_gap - 2 * border_width, 0);

      if (xy0_gap < radius)
        junction |= GTK_JUNCTION_CORNER_TOPRIGHT;

      if (xy1_gap > height - radius)
        junction |= GTK_JUNCTION_CORNER_BOTTOMRIGHT;

      break;
    }

  cairo_clip_extents (cr, &x0, &y0, &x1, &y1);
  cairo_rectangle (cr, x0, y0, x1 - x0, yc - y0);
  cairo_rectangle (cr, x0, yc, xc - x0, hc);
  cairo_rectangle (cr, xc + wc, yc, x1 - (xc + wc), hc);
  cairo_rectangle (cr, x0, yc + hc, x1 - x0, y1 - (yc + hc));
  cairo_clip (cr);

  render_frame_internal (engine, cr,
                         x, y, width, height,
                         0, junction);

  cairo_restore (cr);

  gtk_border_free (border);
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
  GtkJunctionSides junction = 0;
  guint hidden_side = 0;

  cairo_save (cr);

  switch (gap_side)
    {
    case GTK_POS_LEFT:
      junction = GTK_JUNCTION_LEFT;
      hidden_side = SIDE_LEFT;

      cairo_translate (cr, x + width, y);
      cairo_rotate (cr, G_PI / 2);
      break;
    case GTK_POS_RIGHT:
      junction = GTK_JUNCTION_RIGHT;
      hidden_side = SIDE_RIGHT;

      cairo_translate (cr, x, y + height);
      cairo_rotate (cr, - G_PI / 2);
      break;
    case GTK_POS_TOP:
      junction = GTK_JUNCTION_TOP;
      hidden_side = SIDE_TOP;

      cairo_translate (cr, x + width, y + height);
      cairo_rotate (cr, G_PI);
      break;
    case GTK_POS_BOTTOM:
      junction = GTK_JUNCTION_BOTTOM;
      hidden_side = SIDE_BOTTOM;

      cairo_translate (cr, x, y);
      break;
    }

  if (gap_side == GTK_POS_TOP ||
      gap_side == GTK_POS_BOTTOM)
    render_background_internal (engine, cr,
                                0, 0, width, height,
                                GTK_JUNCTION_BOTTOM);
  else
    render_background_internal (engine, cr,
                                0, 0, height, width,
                                GTK_JUNCTION_BOTTOM);
  cairo_restore (cr);

  cairo_save (cr);

  render_frame_internal (engine, cr,
                         x, y, width, height,
                         hidden_side, junction);

  cairo_restore (cr);
}

static void
render_dot (cairo_t       *cr,
            const GdkRGBA *lighter,
            const GdkRGBA *darker,
            gdouble        x,
            gdouble        y,
            gdouble        size)
{
  size = CLAMP ((gint) size, 2, 3);

  if (size == 2)
    {
      gdk_cairo_set_source_rgba (cr, lighter);
      cairo_rectangle (cr, x, y, 1, 1);
      cairo_rectangle (cr, x + 1, y + 1, 1, 1);
      cairo_fill (cr);
    }
  else if (size == 3)
    {
      gdk_cairo_set_source_rgba (cr, lighter);
      cairo_rectangle (cr, x, y, 2, 1);
      cairo_rectangle (cr, x, y, 1, 2);
      cairo_fill (cr);

      gdk_cairo_set_source_rgba (cr, darker);
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
                                  gdouble           height)
{
  GtkStateFlags flags;
  GdkRGBA *bg_color;
  GdkRGBA lighter, darker;
  gint xx, yy;

  cairo_save (cr);
  flags = gtk_theming_engine_get_state (engine);

  cairo_set_line_width (cr, 1);

  gtk_theming_engine_get (engine, flags,
                          "background-color", &bg_color,
                          NULL);
  color_shade (bg_color, 0.7, &darker);
  color_shade (bg_color, 1.3, &lighter);

  gdk_cairo_set_source_rgba (cr, bg_color);
  cairo_rectangle (cr, x, y, width, height);
  cairo_fill (cr);

  if (gtk_theming_engine_has_class (engine, GTK_STYLE_CLASS_GRIP))
    {
      GtkJunctionSides sides;

      cairo_save (cr);

      cairo_set_line_width (cr, 1.0);
      sides = gtk_theming_engine_get_junction_sides (engine);

      /* reduce confusing values to a meaningful state */
      if ((sides & (GTK_JUNCTION_CORNER_TOPLEFT | GTK_JUNCTION_CORNER_BOTTOMRIGHT)) == (GTK_JUNCTION_CORNER_TOPLEFT | GTK_JUNCTION_CORNER_BOTTOMRIGHT))
        sides &= ~GTK_JUNCTION_CORNER_TOPLEFT;

      if ((sides & (GTK_JUNCTION_CORNER_TOPRIGHT | GTK_JUNCTION_CORNER_BOTTOMLEFT)) == (GTK_JUNCTION_CORNER_TOPRIGHT | GTK_JUNCTION_CORNER_BOTTOMLEFT))
        sides &= ~GTK_JUNCTION_CORNER_TOPRIGHT;

      if (sides == 0)
        sides = GTK_JUNCTION_CORNER_BOTTOMRIGHT;

      /* align drawing area to the connected side */
      if (sides == GTK_JUNCTION_LEFT)
        {
          if (height < width)
            width = height;
        }
      else if (sides == GTK_JUNCTION_CORNER_TOPLEFT)
        {
          if (width < height)
            height = width;
          else if (height < width)
            width = height;
        }
      else if (sides == GTK_JUNCTION_CORNER_BOTTOMLEFT)
        {
          /* make it square, aligning to bottom left */
          if (width < height)
            {
              y += (height - width);
              height = width;
            }
          else if (height < width)
            width = height;
        }
      else if (sides == GTK_JUNCTION_RIGHT)
        {
          /* aligning to right */
          if (height < width)
            {
              x += (width - height);
              width = height;
            }
        }
      else if (sides == GTK_JUNCTION_CORNER_TOPRIGHT)
        {
          if (width < height)
            height = width;
          else if (height < width)
            {
              x += (width - height);
              width = height;
            }
        }
      else if (sides == GTK_JUNCTION_CORNER_BOTTOMRIGHT)
        {
          /* make it square, aligning to bottom right */
          if (width < height)
            {
              y += (height - width);
              height = width;
            }
          else if (height < width)
            {
              x += (width - height);
              width = height;
            }
        }
      else if (sides == GTK_JUNCTION_TOP)
        {
          if (width < height)
            height = width;
        }
      else if (sides == GTK_JUNCTION_BOTTOM)
        {
          /* align to bottom */
          if (width < height)
            {
              y += (height - width);
              height = width;
            }
        }
      else
        g_assert_not_reached ();

      if (sides == GTK_JUNCTION_LEFT ||
          sides == GTK_JUNCTION_RIGHT)
        {
          gint xi;

          xi = x;

          while (xi < x + width)
            {
              gdk_cairo_set_source_rgba (cr, &lighter);
              add_path_line (cr, x, y, x, y + height);
              cairo_stroke (cr);
              xi++;

              gdk_cairo_set_source_rgba (cr, &darker);
              add_path_line (cr, xi, y, xi, y + height);
              cairo_stroke (cr);
              xi += 2;
            }
        }
      else if (sides == GTK_JUNCTION_TOP ||
               sides == GTK_JUNCTION_BOTTOM)
        {
          gint yi;

          yi = y;

          while (yi < y + height)
            {
              gdk_cairo_set_source_rgba (cr, &lighter);
              add_path_line (cr, x, yi, x + width, yi);
              cairo_stroke (cr);
              yi++;

              gdk_cairo_set_source_rgba (cr, &darker);
              add_path_line (cr, x, yi, x + width, yi);
              cairo_stroke (cr);
              yi += 2;
            }
        }
      else if (sides == GTK_JUNCTION_CORNER_TOPLEFT)
        {
          gint xi, yi;

          xi = x + width;
          yi = y + height;

          while (xi > x + 3)
            {
              gdk_cairo_set_source_rgba (cr, &darker);
              add_path_line (cr, xi, y, x, yi);
              cairo_stroke (cr);

              --xi;
              --yi;

              add_path_line (cr, xi, y, x, yi);
              cairo_stroke (cr);

              --xi;
              --yi;

              gdk_cairo_set_source_rgba (cr, &lighter);
              add_path_line (cr, xi, y, x, yi);
              cairo_stroke (cr);

              xi -= 3;
              yi -= 3;
            }
        }
      else if (sides == GTK_JUNCTION_CORNER_TOPRIGHT)
        {
          gint xi, yi;

          xi = x;
          yi = y + height;

          while (xi < (x + width - 3))
            {
              gdk_cairo_set_source_rgba (cr, &lighter);
              add_path_line (cr, xi, y, x + width, yi);
              cairo_stroke (cr);

              ++xi;
              --yi;

              gdk_cairo_set_source_rgba (cr, &darker);
              add_path_line (cr, xi, y, x + width, yi);
              cairo_stroke (cr);

              ++xi;
              --yi;

              add_path_line (cr, xi, y, x + width, yi);
              cairo_stroke (cr);

              xi += 3;
              yi -= 3;
            }
        }
      else if (sides == GTK_JUNCTION_CORNER_BOTTOMLEFT)
        {
          gint xi, yi;

          xi = x + width;
          yi = y;

          while (xi > x + 3)
            {
              gdk_cairo_set_source_rgba (cr, &darker);
              add_path_line (cr, x, yi, xi, y + height);
              cairo_stroke (cr);

              --xi;
              ++yi;

              add_path_line (cr, x, yi, xi, y + height);
              cairo_stroke (cr);

              --xi;
              ++yi;

              gdk_cairo_set_source_rgba (cr, &lighter);
              add_path_line (cr, x, yi, xi, y + height);
              cairo_stroke (cr);

              xi -= 3;
              yi += 3;
            }
        }
      else if (sides == GTK_JUNCTION_CORNER_BOTTOMRIGHT)
        {
          gint xi, yi;

          xi = x;
          yi = y;

          while (xi < (x + width - 3))
            {
              gdk_cairo_set_source_rgba (cr, &lighter);
              add_path_line (cr, xi, y + height, x + width, yi);
              cairo_stroke (cr);

              ++xi;
              ++yi;

              gdk_cairo_set_source_rgba (cr, &darker);
              add_path_line (cr, xi, y + height, x + width, yi);
              cairo_stroke (cr);

              ++xi;
              ++yi;

              add_path_line (cr, xi, y + height, x + width, yi);
              cairo_stroke (cr);

              xi += 3;
              yi += 3;
            }
        }

      cairo_restore (cr);
    }
  else if (gtk_theming_engine_has_class (engine, GTK_STYLE_CLASS_PANE_SEPARATOR))
    {
      if (width > height)
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

  gdk_rgba_free (bg_color);
}

static void
gtk_theming_engine_render_activity (GtkThemingEngine *engine,
                                    cairo_t          *cr,
                                    gdouble           x,
                                    gdouble           y,
                                    gdouble           width,
                                    gdouble           height)
{
  if (gtk_theming_engine_has_class (engine, GTK_STYLE_CLASS_SPINNER))
    {
      GtkStateFlags state;
      guint num_steps, step;
      GdkRGBA *color;
      gdouble dx, dy;
      gdouble progress;
      gdouble radius;
      gdouble half;
      gint i;

      num_steps = 12;

      state = gtk_theming_engine_get_state (engine);
      gtk_theming_engine_get (engine, state,
                              "color", &color,
                              NULL);

      if (gtk_theming_engine_state_is_running (engine, GTK_STATE_ACTIVE, &progress))
        step = (guint) (progress * num_steps);
      else
        step = 0;

      cairo_save (cr);

      cairo_translate (cr, x, y);

      /* draw clip region */
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

      dx = width / 2;
      dy = height / 2;
      radius = MIN (width / 2, height / 2);
      half = num_steps / 2;

      for (i = 0; i < num_steps; i++)
        {
          gint inset = 0.7 * radius;

          /* transparency is a function of time and intial value */
          gdouble t = (gdouble) ((i + num_steps - step)
                                 % num_steps) / num_steps;

          cairo_save (cr);

          cairo_set_source_rgba (cr,
                                 color->red,
                                 color->green,
                                 color->blue,
                                 color->alpha * t);

          cairo_set_line_width (cr, 2.0);
          cairo_move_to (cr,
                         dx + (radius - inset) * cos (i * G_PI / half),
                         dy + (radius - inset) * sin (i * G_PI / half));
          cairo_line_to (cr,
                         dx + radius * cos (i * G_PI / half),
                         dy + radius * sin (i * G_PI / half));
          cairo_stroke (cr);

          cairo_restore (cr);
        }

      cairo_restore (cr);

      gdk_rgba_free (color);
    }
  else
    {
      gtk_theming_engine_render_background (engine, cr, x, y, width, height);
      gtk_theming_engine_render_frame (engine, cr, x, y, width, height);
    }
}

static GdkPixbuf *
scale_or_ref (GdkPixbuf *src,
              gint       width,
              gint       height)
{
  if (width == gdk_pixbuf_get_width (src) &&
      height == gdk_pixbuf_get_height (src))
    return g_object_ref (src);
  else
    return gdk_pixbuf_scale_simple (src,
                                    width, height,
                                    GDK_INTERP_BILINEAR);
}

static gboolean
lookup_icon_size (GtkThemingEngine *engine,
		  GtkIconSize       size,
		  gint             *width,
		  gint             *height)
{
  GdkScreen *screen;
  GtkSettings *settings;

  screen = gtk_theming_engine_get_screen (engine);
  settings = gtk_settings_get_for_screen (screen);

  return gtk_icon_size_lookup_for_settings (settings, size, width, height);
}

/* Kudos to the gnome-panel guys. */
static void
colorshift_pixbuf (GdkPixbuf *src,
                   GdkPixbuf *dest,
                   gint       shift)
{
  gint i, j;
  gint width, height, has_alpha, src_rowstride, dest_rowstride;
  guchar *target_pixels;
  guchar *original_pixels;
  guchar *pix_src;
  guchar *pix_dest;
  int val;
  guchar r, g, b;

  has_alpha       = gdk_pixbuf_get_has_alpha (src);
  width           = gdk_pixbuf_get_width (src);
  height          = gdk_pixbuf_get_height (src);
  src_rowstride   = gdk_pixbuf_get_rowstride (src);
  dest_rowstride  = gdk_pixbuf_get_rowstride (dest);
  original_pixels = gdk_pixbuf_get_pixels (src);
  target_pixels   = gdk_pixbuf_get_pixels (dest);

  for (i = 0; i < height; i++)
    {
      pix_dest = target_pixels   + i * dest_rowstride;
      pix_src  = original_pixels + i * src_rowstride;

      for (j = 0; j < width; j++)
        {
          r = *(pix_src++);
          g = *(pix_src++);
          b = *(pix_src++);

          val = r + shift;
          *(pix_dest++) = CLAMP (val, 0, 255);

          val = g + shift;
          *(pix_dest++) = CLAMP (val, 0, 255);

          val = b + shift;
          *(pix_dest++) = CLAMP (val, 0, 255);

          if (has_alpha)
            *(pix_dest++) = *(pix_src++);
        }
    }
}

static GdkPixbuf *
gtk_theming_engine_render_icon_pixbuf (GtkThemingEngine    *engine,
                                       const GtkIconSource *source,
                                       GtkIconSize          size)
{
  GdkPixbuf *scaled;
  GdkPixbuf *stated;
  GdkPixbuf *base_pixbuf;
  GtkStateFlags state;
  gint width = 1;
  gint height = 1;

  base_pixbuf = gtk_icon_source_get_pixbuf (source);
  state = gtk_theming_engine_get_state (engine);

  g_return_val_if_fail (base_pixbuf != NULL, NULL);

  if (size != (GtkIconSize) -1 &&
      !lookup_icon_size (engine, size, &width, &height))
    {
      g_warning (G_STRLOC ": invalid icon size '%d'", size);
      return NULL;
    }

  /* If the size was wildcarded, and we're allowed to scale, then scale; otherwise,
   * leave it alone.
   */
  if (size != (GtkIconSize) -1 &&
      gtk_icon_source_get_size_wildcarded (source))
    scaled = scale_or_ref (base_pixbuf, width, height);
  else
    scaled = g_object_ref (base_pixbuf);

  /* If the state was wildcarded, then generate a state. */
  if (gtk_icon_source_get_state_wildcarded (source))
    {
      if (state & GTK_STATE_FLAG_INSENSITIVE)
        {
          stated = gdk_pixbuf_copy (scaled);
          gdk_pixbuf_saturate_and_pixelate (scaled, stated,
                                            0.8, TRUE);
          g_object_unref (scaled);
        }
      else if (state & GTK_STATE_FLAG_PRELIGHT)
        {
          stated = gdk_pixbuf_copy (scaled);
          colorshift_pixbuf (scaled, stated, 30);
          g_object_unref (scaled);
        }
      else
        stated = scaled;
    }
  else
    stated = scaled;

  return stated;
}
