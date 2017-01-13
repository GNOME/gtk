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

#include "gtkstylecontextprivate.h"

#include <gdk/gdk.h>
#include <math.h>
#include <stdlib.h>
#include <gobject/gvaluecollector.h>

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssnodedeclarationprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsspathnodeprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransientnodeprivate.h"
#include "gtkcsswidgetnodeprivate.h"
#include "gtkdebug.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrendericonprivate.h"
#include "gtksettings.h"
#include "gtksettingsprivate.h"
#include "gtkstylecascadeprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwindow.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"


/**
 * SECTION:gtkstylecontext
 * @Short_description: Rendering UI elements
 * @Title: GtkStyleContext
 *
 * #GtkStyleContext is an object that stores styling information affecting
 * a widget defined by #GtkWidgetPath.
 *
 * In order to construct the final style information, #GtkStyleContext
 * queries information from all attached #GtkStyleProviders. Style providers
 * can be either attached explicitly to the context through
 * gtk_style_context_add_provider(), or to the screen through
 * gtk_style_context_add_provider_for_screen(). The resulting style is a
 * combination of all providers’ information in priority order.
 *
 * For GTK+ widgets, any #GtkStyleContext returned by
 * gtk_widget_get_style_context() will already have a #GtkWidgetPath, a
 * #GdkScreen and RTL/LTR information set. The style context will also be
 * updated automatically if any of these settings change on the widget.
 *
 * If you are using the theming layer standalone, you will need to set a
 * widget path and a screen yourself to the created style context through
 * gtk_style_context_set_path() and gtk_style_context_set_screen(), as well
 * as updating the context yourself using gtk_style_context_invalidate()
 * whenever any of the conditions change, such as a change in the
 * #GtkSettings:gtk-theme-name setting or a hierarchy change in the rendered
 * widget. See the “Foreign drawing“ example in gtk3-demo.
 *
 * # Style Classes # {#gtkstylecontext-classes}
 *
 * Widgets can add style classes to their context, which can be used to associate
 * different styles by class. The documentation for individual widgets lists
 * which style classes it uses itself, and which style classes may be added by
 * applications to affect their appearance.
 *
 * GTK+ defines macros for a number of style classes.
 *
 * # Custom styling in UI libraries and applications
 *
 * If you are developing a library with custom #GtkWidgets that
 * render differently than standard components, you may need to add a
 * #GtkStyleProvider yourself with the %GTK_STYLE_PROVIDER_PRIORITY_FALLBACK
 * priority, either a #GtkCssProvider or a custom object implementing the
 * #GtkStyleProvider interface. This way themes may still attempt
 * to style your UI elements in a different way if needed so.
 *
 * If you are using custom styling on an applications, you probably want then
 * to make your style information prevail to the theme’s, so you must use
 * a #GtkStyleProvider with the %GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
 * priority, keep in mind that the user settings in
 * `XDG_CONFIG_HOME/gtk-4.0/gtk.css` will
 * still take precedence over your changes, as it uses the
 * %GTK_STYLE_PROVIDER_PRIORITY_USER priority.
 */

#define CURSOR_ASPECT_RATIO (0.04)
typedef struct PropertyValue PropertyValue;

struct PropertyValue
{
  GType       widget_type;
  GParamSpec *pspec;
  GValue      value;
};

struct _GtkStyleContextPrivate
{
  GdkScreen *screen;

  guint cascade_changed_id;
  GtkStyleCascade *cascade;
  GtkStyleContext *parent;
  GtkCssNode *cssnode;
  GSList *saved_nodes;
  GArray *property_cache;

  GdkFrameClock *frame_clock;

  GtkCssStyleChange *invalidating_context;
};

enum {
  PROP_0,
  PROP_SCREEN,
  PROP_FRAME_CLOCK,
  PROP_PARENT,
  LAST_PROP
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static GParamSpec *properties[LAST_PROP] = { NULL, };

static guint signals[LAST_SIGNAL] = { 0 };

static void gtk_style_context_finalize (GObject *object);

static void gtk_style_context_impl_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec);
static void gtk_style_context_impl_get_property (GObject      *object,
                                                 guint         prop_id,
                                                 GValue       *value,
                                                 GParamSpec   *pspec);

static GtkCssNode * gtk_style_context_get_root (GtkStyleContext *context);

G_DEFINE_TYPE_WITH_PRIVATE (GtkStyleContext, gtk_style_context, G_TYPE_OBJECT)

static void
gtk_style_context_real_changed (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  if (GTK_IS_CSS_WIDGET_NODE (priv->cssnode))
    _gtk_widget_style_context_invalidated (gtk_css_widget_node_get_widget (GTK_CSS_WIDGET_NODE (priv->cssnode)));
}

static void
gtk_style_context_class_init (GtkStyleContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_style_context_finalize;
  object_class->set_property = gtk_style_context_impl_set_property;
  object_class->get_property = gtk_style_context_impl_get_property;

  klass->changed = gtk_style_context_real_changed;

  /**
   * GtkStyleContext::changed:
   *
   * The ::changed signal is emitted when there is a change in the
   * #GtkStyleContext.
   *
   * For a #GtkStyleContext returned by gtk_widget_get_style_context(), the
   * #GtkWidget::style-updated signal/vfunc might be more convenient to use.
   *
   * This signal is useful when using the theming layer standalone.
   *
   * Since: 3.0
   */
  signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkStyleContextClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  properties[PROP_SCREEN] =
      g_param_spec_object ("screen",
                           P_("Screen"),
                           P_("The associated GdkScreen"),
                           GDK_TYPE_SCREEN,
                           GTK_PARAM_READWRITE);

  properties[PROP_FRAME_CLOCK] =
      g_param_spec_object ("paint-clock",
                           P_("FrameClock"),
                           P_("The associated GdkFrameClock"),
                           GDK_TYPE_FRAME_CLOCK,
                           GTK_PARAM_READWRITE);

  /**
   * GtkStyleContext:parent:
   *
   * Sets or gets the style context’s parent. See gtk_style_context_set_parent()
   * for details.
   *
   * Since: 3.4
   */
  properties[PROP_PARENT] =
      g_param_spec_object ("parent",
                           P_("Parent"),
                           P_("The parent style context"),
                           GTK_TYPE_STYLE_CONTEXT,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

void
gtk_style_context_clear_property_cache (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;
  guint i;

  for (i = 0; i < priv->property_cache->len; i++)
    {
      PropertyValue *node = &g_array_index (priv->property_cache, PropertyValue, i);

      g_param_spec_unref (node->pspec);
      g_value_unset (&node->value);
    }

  g_array_set_size (priv->property_cache, 0);
}

static void
gtk_style_context_pop_style_node (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  g_return_if_fail (priv->saved_nodes != NULL);

  if (GTK_IS_CSS_TRANSIENT_NODE (priv->cssnode))
    gtk_css_node_set_parent (priv->cssnode, NULL);
  g_object_unref (priv->cssnode);
  priv->cssnode = priv->saved_nodes->data;
  priv->saved_nodes = g_slist_remove (priv->saved_nodes, priv->cssnode);
}

static void
gtk_style_context_cascade_changed (GtkStyleCascade *cascade,
                                   GtkStyleContext *context)
{
  gtk_css_node_invalidate_style_provider (gtk_style_context_get_root (context));
}

static void
gtk_style_context_set_cascade (GtkStyleContext *context,
                               GtkStyleCascade *cascade)
{
  GtkStyleContextPrivate *priv;

  priv = context->priv;

  if (priv->cascade == cascade)
    return;

  if (priv->cascade)
    {
      g_signal_handler_disconnect (priv->cascade, priv->cascade_changed_id);
      priv->cascade_changed_id = 0;
      g_object_unref (priv->cascade);
    }

  if (cascade)
    {
      g_object_ref (cascade);
      priv->cascade_changed_id = g_signal_connect (cascade,
                                                   "-gtk-private-changed",
                                                   G_CALLBACK (gtk_style_context_cascade_changed),
                                                   context);
    }

  priv->cascade = cascade;

  if (cascade && priv->cssnode != NULL)
    gtk_style_context_cascade_changed (cascade, context);
}

static void
gtk_style_context_init (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  priv = context->priv = gtk_style_context_get_instance_private (context);

  priv->screen = gdk_screen_get_default ();

  if (priv->screen == NULL)
    g_error ("Can't create a GtkStyleContext without a display connection");

  priv->property_cache = g_array_new (FALSE, FALSE, sizeof (PropertyValue));

  gtk_style_context_set_cascade (context,
                                 _gtk_settings_get_style_cascade (gtk_settings_get_for_screen (priv->screen), 1));

  /* Create default info store */
  priv->cssnode = gtk_css_path_node_new (context);
  gtk_css_node_set_state (priv->cssnode, GTK_STATE_FLAG_DIR_LTR);
}

static void
gtk_style_context_clear_parent (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  if (priv->parent)
    g_object_unref (priv->parent);
}

static void
gtk_style_context_finalize (GObject *object)
{
  GtkStyleContext *context = GTK_STYLE_CONTEXT (object);
  GtkStyleContextPrivate *priv = context->priv;

  while (priv->saved_nodes)
    gtk_style_context_pop_style_node (context);

  if (GTK_IS_CSS_PATH_NODE (priv->cssnode))
    gtk_css_path_node_unset_context (GTK_CSS_PATH_NODE (priv->cssnode));

  gtk_style_context_clear_parent (context);
  gtk_style_context_set_cascade (context, NULL);

  g_object_unref (priv->cssnode);

  gtk_style_context_clear_property_cache (context);
  g_array_free (priv->property_cache, TRUE);

  G_OBJECT_CLASS (gtk_style_context_parent_class)->finalize (object);
}

static void
gtk_style_context_impl_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkStyleContext *context = GTK_STYLE_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      gtk_style_context_set_screen (context, g_value_get_object (value));
      break;
    case PROP_FRAME_CLOCK:
      gtk_style_context_set_frame_clock (context, g_value_get_object (value));
      break;
    case PROP_PARENT:
      gtk_style_context_set_parent (context, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_style_context_impl_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkStyleContext *context = GTK_STYLE_CONTEXT (object);
  GtkStyleContextPrivate *priv = context->priv;

  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, priv->screen);
      break;
    case PROP_FRAME_CLOCK:
      g_value_set_object (value, priv->frame_clock);
      break;
    case PROP_PARENT:
      g_value_set_object (value, priv->parent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* returns TRUE if someone called gtk_style_context_save() but hasn’t
 * called gtk_style_context_restore() yet.
 * In those situations we don’t invalidate the context when somebody
 * changes state/classes.
 */
static gboolean
gtk_style_context_is_saved (GtkStyleContext *context)
{
  return context->priv->saved_nodes != NULL;
}

static GtkCssNode *
gtk_style_context_get_root (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  if (priv->saved_nodes != NULL)
    return g_slist_last (priv->saved_nodes)->data;
  else
    return priv->cssnode;
}

GtkStyleProviderPrivate *
gtk_style_context_get_style_provider (GtkStyleContext *context)
{
  return GTK_STYLE_PROVIDER_PRIVATE (context->priv->cascade);
}

static gboolean
gtk_style_context_has_custom_cascade (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;
  GtkSettings *settings = gtk_settings_get_for_screen (priv->screen);

  return priv->cascade != _gtk_settings_get_style_cascade (settings, _gtk_style_cascade_get_scale (priv->cascade));
}

GtkCssStyle *
gtk_style_context_lookup_style (GtkStyleContext *context)
{
  /* Code will recreate style if it was changed */
  return gtk_css_node_get_style (context->priv->cssnode);
}

GtkCssNode*
gtk_style_context_get_node (GtkStyleContext *context)
{
  return context->priv->cssnode;
}

/**
 * gtk_style_context_new:
 *
 * Creates a standalone #GtkStyleContext, this style context
 * won’t be attached to any widget, so you may want
 * to call gtk_style_context_set_path() yourself.
 *
 * This function is only useful when using the theming layer
 * separated from GTK+, if you are using #GtkStyleContext to
 * theme #GtkWidgets, use gtk_widget_get_style_context()
 * in order to get a style context ready to theme the widget.
 *
 * Returns: A newly created #GtkStyleContext.
 **/
GtkStyleContext *
gtk_style_context_new (void)
{
  return g_object_new (GTK_TYPE_STYLE_CONTEXT, NULL);
}

GtkStyleContext *
gtk_style_context_new_for_node (GtkCssNode *node)
{
  GtkStyleContext *context;

  g_return_val_if_fail (GTK_IS_CSS_NODE (node), NULL);

  context = gtk_style_context_new ();
  g_set_object (&context->priv->cssnode, node);

  return context;
}

/**
 * gtk_style_context_add_provider:
 * @context: a #GtkStyleContext
 * @provider: a #GtkStyleProvider
 * @priority: the priority of the style provider. The lower
 *            it is, the earlier it will be used in the style
 *            construction. Typically this will be in the range
 *            between %GTK_STYLE_PROVIDER_PRIORITY_FALLBACK and
 *            %GTK_STYLE_PROVIDER_PRIORITY_USER
 *
 * Adds a style provider to @context, to be used in style construction.
 * Note that a style provider added by this function only affects
 * the style of the widget to which @context belongs. If you want
 * to affect the style of all widgets, use
 * gtk_style_context_add_provider_for_screen().
 *
 * Note: If both priorities are the same, a #GtkStyleProvider
 * added through this function takes precedence over another added
 * through gtk_style_context_add_provider_for_screen().
 *
 * Since: 3.0
 **/
void
gtk_style_context_add_provider (GtkStyleContext  *context,
                                GtkStyleProvider *provider,
                                guint             priority)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  priv = context->priv;

  if (!gtk_style_context_has_custom_cascade (context))
    {
      GtkStyleCascade *new_cascade;

      new_cascade = _gtk_style_cascade_new ();
      _gtk_style_cascade_set_scale (new_cascade, _gtk_style_cascade_get_scale (priv->cascade));
      _gtk_style_cascade_set_parent (new_cascade,
                                     _gtk_settings_get_style_cascade (gtk_settings_get_for_screen (priv->screen), 1));
      _gtk_style_cascade_add_provider (new_cascade, provider, priority);
      gtk_style_context_set_cascade (context, new_cascade);
      g_object_unref (new_cascade);
    }
  else
    {
      _gtk_style_cascade_add_provider (priv->cascade, provider, priority);
    }
}

/**
 * gtk_style_context_remove_provider:
 * @context: a #GtkStyleContext
 * @provider: a #GtkStyleProvider
 *
 * Removes @provider from the style providers list in @context.
 *
 * Since: 3.0
 **/
void
gtk_style_context_remove_provider (GtkStyleContext  *context,
                                   GtkStyleProvider *provider)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  if (!gtk_style_context_has_custom_cascade (context))
    return;

  _gtk_style_cascade_remove_provider (context->priv->cascade, provider);
}

/**
 * gtk_style_context_reset_widgets:
 * @screen: a #GdkScreen
 *
 * This function recomputes the styles for all widgets under a particular
 * #GdkScreen. This is useful when some global parameter has changed that
 * affects the appearance of all widgets, because when a widget gets a new
 * style, it will both redraw and recompute any cached information about
 * its appearance. As an example, it is used when the color scheme changes
 * in the related #GtkSettings object.
 *
 * Since: 3.0
 **/
void
gtk_style_context_reset_widgets (GdkScreen *screen)
{
  GList *list, *toplevels;

  toplevels = gtk_window_list_toplevels ();
  g_list_foreach (toplevels, (GFunc) g_object_ref, NULL);

  for (list = toplevels; list; list = list->next)
    {
      if (gtk_widget_get_screen (list->data) == screen)
        gtk_widget_reset_style (list->data);

      g_object_unref (list->data);
    }

  g_list_free (toplevels);
}

/**
 * gtk_style_context_add_provider_for_screen:
 * @screen: a #GdkScreen
 * @provider: a #GtkStyleProvider
 * @priority: the priority of the style provider. The lower
 *            it is, the earlier it will be used in the style
 *            construction. Typically this will be in the range
 *            between %GTK_STYLE_PROVIDER_PRIORITY_FALLBACK and
 *            %GTK_STYLE_PROVIDER_PRIORITY_USER
 *
 * Adds a global style provider to @screen, which will be used
 * in style construction for all #GtkStyleContexts under @screen.
 *
 * GTK+ uses this to make styling information from #GtkSettings
 * available.
 *
 * Note: If both priorities are the same, A #GtkStyleProvider
 * added through gtk_style_context_add_provider() takes precedence
 * over another added through this function.
 *
 * Since: 3.0
 **/
void
gtk_style_context_add_provider_for_screen (GdkScreen        *screen,
                                           GtkStyleProvider *provider,
                                           guint             priority)
{
  GtkStyleCascade *cascade;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));
  g_return_if_fail (!GTK_IS_SETTINGS (provider) || _gtk_settings_get_screen (GTK_SETTINGS (provider)) == screen);

  cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_screen (screen), 1);
  _gtk_style_cascade_add_provider (cascade, provider, priority);
}

/**
 * gtk_style_context_remove_provider_for_screen:
 * @screen: a #GdkScreen
 * @provider: a #GtkStyleProvider
 *
 * Removes @provider from the global style providers list in @screen.
 *
 * Since: 3.0
 **/
void
gtk_style_context_remove_provider_for_screen (GdkScreen        *screen,
                                              GtkStyleProvider *provider)
{
  GtkStyleCascade *cascade;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));
  g_return_if_fail (!GTK_IS_SETTINGS (provider));

  cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_screen (screen), 1);
  _gtk_style_cascade_remove_provider (cascade, provider);
}

/**
 * gtk_style_context_get_section:
 * @context: a #GtkStyleContext
 * @property: style property name
 *
 * Queries the location in the CSS where @property was defined for the
 * current @context. Note that the state to be queried is taken from
 * gtk_style_context_get_state().
 *
 * If the location is not available, %NULL will be returned. The
 * location might not be available for various reasons, such as the
 * property being overridden, @property not naming a supported CSS
 * property or tracking of definitions being disabled for performance
 * reasons.
 *
 * Shorthand CSS properties cannot be queried for a location and will
 * always return %NULL.
 *
 * Returns: (nullable) (transfer none): %NULL or the section where a value
 * for @property was defined
 **/
GtkCssSection *
gtk_style_context_get_section (GtkStyleContext *context,
                               const gchar     *property)
{
  GtkCssStyle *values;
  GtkStyleProperty *prop;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);
  g_return_val_if_fail (property != NULL, NULL);

  prop = _gtk_style_property_lookup (property);
  if (!GTK_IS_CSS_STYLE_PROPERTY (prop))
    return NULL;

  values = gtk_style_context_lookup_style (context);
  return gtk_css_style_get_section (values, _gtk_css_style_property_get_id (GTK_CSS_STYLE_PROPERTY (prop)));
}

static GtkCssValue *
gtk_style_context_query_func (guint    id,
                              gpointer values)
{
  return gtk_css_style_get_value (values, id);
}

/**
 * gtk_style_context_get_property:
 * @context: a #GtkStyleContext
 * @property: style property name
 * @value: (out) (transfer full):  return location for the style property value
 *
 * Gets a style property from @context for the current state.
 *
 * Note that not all CSS properties that are supported by GTK+ can be
 * retrieved in this way, since they may not be representable as #GValue.
 * GTK+ defines macros for a number of properties that can be used
 * with this function.
 *
 * When @value is no longer needed, g_value_unset() must be called
 * to free any allocated memory.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_property (GtkStyleContext *context,
                                const gchar     *property,
                                GValue          *value)
{
  GtkStyleProperty *prop;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (property != NULL);
  g_return_if_fail (value != NULL);

  prop = _gtk_style_property_lookup (property);
  if (prop == NULL)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return;
    }
  if (_gtk_style_property_get_value_type (prop) == G_TYPE_NONE)
    {
      g_warning ("Style property \"%s\" is not gettable", property);
      return;
    }

  _gtk_style_property_query (prop,
                             value,
                             gtk_style_context_query_func,
                             gtk_css_node_get_style (context->priv->cssnode));
}

/**
 * gtk_style_context_get_valist:
 * @context: a #GtkStyleContext
 * @args: va_list of property name/return location pairs, followed by %NULL
 *
 * Retrieves several style property values from @context for a given state.
 *
 * See gtk_style_context_get_property() for details.
 *
 * As with g_object_get(), a copy is made of the property contents for
 * pointer-valued properties, and the caller is responsible for freeing the
 * memory in the appropriate manner for the type. For example, by calling
 * g_free() or g_object_unref(). Non-pointer-valued properties, such as
 * integers, are returned by value and do not need to be freed.
 *
 * Since: 3.0
 */
void
gtk_style_context_get_valist (GtkStyleContext *context,
                              va_list          args)
{
  const gchar *property_name;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  property_name = va_arg (args, const gchar *);

  while (property_name)
    {
      gchar *error = NULL;
      GValue value = G_VALUE_INIT;

      gtk_style_context_get_property (context,
                                      property_name,
                                      &value);

      G_VALUE_LCOPY (&value, args, 0, &error);
      g_value_unset (&value);

      if (error)
        {
          g_warning ("Could not get style property \"%s\": %s", property_name, error);
          g_free (error);
          break;
        }

      property_name = va_arg (args, const gchar *);
    }
}

/**
 * gtk_style_context_get:
 * @context: a #GtkStyleContext
 * @...: property name /return value pairs, followed by %NULL
 *
 * Retrieves several style property values from @context for a
 * given state.
 *
 * See gtk_style_context_get_property() for details.
 *
 * As with g_object_get(), a copy is made of the property contents for
 * pointer-valued properties, and the caller is responsible for freeing the
 * memory in the appropriate manner for the type. For example, by calling
 * g_free() or g_object_unref(). Non-pointer-valued properties, such as
 * integers, are returned by value and do not need to be freed.
 *
 * Since: 3.0
 */
void
gtk_style_context_get (GtkStyleContext *context,
                       ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  va_start (args, context);
  gtk_style_context_get_valist (context, args);
  va_end (args);
}

/*
 * gtk_style_context_set_id:
 * @context: a #GtkStyleContext
 * @id: (allow-none): the id to use or %NULL for none.
 *
 * Sets the CSS ID to be used when obtaining style information.
 **/
void
gtk_style_context_set_id (GtkStyleContext *context,
                          const char      *id)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_css_node_set_id (context->priv->cssnode, id);
}

/*
 * gtk_style_context_get_id:
 * @context: a #GtkStyleContext
 *
 * Returns the CSS ID used when obtaining style information.
 *
 * Returns: (nullable): the ID or %NULL if no ID is set.
 **/
const char *
gtk_style_context_get_id (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return gtk_css_node_get_id (context->priv->cssnode);
}

/**
 * gtk_style_context_set_state:
 * @context: a #GtkStyleContext
 * @flags: state to represent
 *
 * Sets the state to be used for style matching.
 *
 * Since: 3.0
 **/
void
gtk_style_context_set_state (GtkStyleContext *context,
                             GtkStateFlags    flags)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_css_node_set_state (context->priv->cssnode, flags);
}

/**
 * gtk_style_context_get_state:
 * @context: a #GtkStyleContext
 *
 * Returns the state used for style matching.
 *
 * This method should only be used to retrieve the #GtkStateFlags
 * to pass to #GtkStyleContext methods, like gtk_style_context_get_padding().
 * If you need to retrieve the current state of a #GtkWidget, use
 * gtk_widget_get_state_flags().
 *
 * Returns: the state flags
 *
 * Since: 3.0
 **/
GtkStateFlags
gtk_style_context_get_state (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  return gtk_css_node_get_state (context->priv->cssnode);
}

/**
 * gtk_style_context_set_scale:
 * @context: a #GtkStyleContext
 * @scale: scale
 *
 * Sets the scale to use when getting image assets for the style.
 *
 * Since: 3.10
 **/
void
gtk_style_context_set_scale (GtkStyleContext *context,
                             gint             scale)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  if (scale == _gtk_style_cascade_get_scale (priv->cascade))
    return;

  if (gtk_style_context_has_custom_cascade (context))
    {
      _gtk_style_cascade_set_scale (priv->cascade, scale);
    }
  else
    {
      GtkStyleCascade *new_cascade;

      new_cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_screen (priv->screen),
                                                     scale);
      gtk_style_context_set_cascade (context, new_cascade);
    }
}

/**
 * gtk_style_context_get_scale:
 * @context: a #GtkStyleContext
 *
 * Returns the scale used for assets.
 *
 * Returns: the scale
 *
 * Since: 3.10
 **/
gint
gtk_style_context_get_scale (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  return _gtk_style_cascade_get_scale (context->priv->cascade);
}

/**
 * gtk_style_context_set_path:
 * @context: a #GtkStyleContext
 * @path: a #GtkWidgetPath
 *
 * Sets the #GtkWidgetPath used for style matching. As a
 * consequence, the style will be regenerated to match
 * the new given path.
 *
 * If you are using a #GtkStyleContext returned from
 * gtk_widget_get_style_context(), you do not need to call
 * this yourself.
 *
 * Since: 3.0
 **/
void
gtk_style_context_set_path (GtkStyleContext *context,
                            GtkWidgetPath   *path)
{
  GtkCssNode *root;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (path != NULL);

  root = gtk_style_context_get_root (context);
  g_return_if_fail (GTK_IS_CSS_PATH_NODE (root));

  if (path && gtk_widget_path_length (path) > 0)
    {
      GtkWidgetPath *copy = gtk_widget_path_copy (path);
      gtk_css_path_node_set_widget_path (GTK_CSS_PATH_NODE (root), copy);
      gtk_css_node_set_widget_type (root,
                                    gtk_widget_path_iter_get_object_type (copy, -1));
      gtk_css_node_set_name (root, gtk_widget_path_iter_get_object_name (copy, -1));
      gtk_widget_path_unref (copy);
    }
  else
    {
      gtk_css_path_node_set_widget_path (GTK_CSS_PATH_NODE (root), NULL);
      gtk_css_node_set_widget_type (root, G_TYPE_NONE);
      gtk_css_node_set_name (root, NULL);
    }
}

/**
 * gtk_style_context_get_path:
 * @context: a #GtkStyleContext
 *
 * Returns the widget path used for style matching.
 *
 * Returns: (transfer none): A #GtkWidgetPath
 *
 * Since: 3.0
 **/
const GtkWidgetPath *
gtk_style_context_get_path (GtkStyleContext *context)
{
  return gtk_css_node_get_widget_path (gtk_style_context_get_root (context));
}

/**
 * gtk_style_context_set_parent:
 * @context: a #GtkStyleContext
 * @parent: (allow-none): the new parent or %NULL
 *
 * Sets the parent style context for @context. The parent style
 * context is used to implement
 * [inheritance](http://www.w3.org/TR/css3-cascade/#inheritance)
 * of properties.
 *
 * If you are using a #GtkStyleContext returned from
 * gtk_widget_get_style_context(), the parent will be set for you.
 *
 * Since: 3.4
 **/
void
gtk_style_context_set_parent (GtkStyleContext *context,
                              GtkStyleContext *parent)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (parent == NULL || GTK_IS_STYLE_CONTEXT (parent));

  priv = context->priv;

  if (priv->parent == parent)
    return;

  if (parent)
    {
      GtkCssNode *root = gtk_style_context_get_root (context);
      g_object_ref (parent);

      if (gtk_css_node_get_parent (root) == NULL)
        gtk_css_node_set_parent (root, gtk_style_context_get_root (parent));
    }
  else
    {
      gtk_css_node_set_parent (gtk_style_context_get_root (context), NULL);
    }

  gtk_style_context_clear_parent (context);

  priv->parent = parent;

  g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_PARENT]);
  gtk_css_node_invalidate (gtk_style_context_get_root (context), GTK_CSS_CHANGE_ANY_PARENT | GTK_CSS_CHANGE_ANY_SIBLING);
}

/**
 * gtk_style_context_get_parent:
 * @context: a #GtkStyleContext
 *
 * Gets the parent context set via gtk_style_context_set_parent().
 * See that function for details.
 *
 * Returns: (nullable) (transfer none): the parent context or %NULL
 *
 * Since: 3.4
 **/
GtkStyleContext *
gtk_style_context_get_parent (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return context->priv->parent;
}

/*
 * gtk_style_context_save_to_node:
 * @context: a #GtkStyleContext
 * @node: the node to save to
 *
 * Saves the @context state, so temporary modifications done through
 * gtk_style_context_add_class(), gtk_style_context_remove_class(),
 * gtk_style_context_set_state(), etc. and rendering using
 * gtk_render_background() or similar functions are done using the
 * given @node.
 *
 * To undo, call gtk_style_context_restore().
 *
 * The matching call to gtk_style_context_restore() must be done
 * before GTK returns to the main loop.
 **/
void
gtk_style_context_save_to_node (GtkStyleContext *context,
                                GtkCssNode      *node)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_CSS_NODE (node));

  priv = context->priv;

  priv->saved_nodes = g_slist_prepend (priv->saved_nodes, priv->cssnode);
  priv->cssnode = g_object_ref (node);
}

void
gtk_style_context_save_named (GtkStyleContext *context,
                              const char      *name)
{
  GtkStyleContextPrivate *priv;
  GtkCssNode *cssnode;

  priv = context->priv;

  /* Make sure we have the style existing. It is the
   * parent of the new saved node after all.
   */
  if (!gtk_style_context_is_saved (context))
    gtk_style_context_lookup_style (context);

  cssnode = gtk_css_transient_node_new (priv->cssnode);
  gtk_css_node_set_parent (cssnode, gtk_style_context_get_root (context));
  if (name)
    gtk_css_node_set_name (cssnode, g_intern_string (name));

  gtk_style_context_save_to_node (context, cssnode);

  g_object_unref (cssnode);
}

/**
 * gtk_style_context_save:
 * @context: a #GtkStyleContext
 *
 * Saves the @context state, so temporary modifications done through
 * gtk_style_context_add_class(), gtk_style_context_remove_class(),
 * gtk_style_context_set_state(), etc. can quickly be reverted
 * in one go through gtk_style_context_restore().
 *
 * The matching call to gtk_style_context_restore() must be done
 * before GTK returns to the main loop.
 *
 * Since: 3.0
 **/
void
gtk_style_context_save (GtkStyleContext *context)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_save_named (context, NULL);
}

/**
 * gtk_style_context_restore:
 * @context: a #GtkStyleContext
 *
 * Restores @context state to a previous stage.
 * See gtk_style_context_save().
 *
 * Since: 3.0
 **/
void
gtk_style_context_restore (GtkStyleContext *context)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  if (context->priv->saved_nodes == NULL)
    {
      g_warning ("Unpaired gtk_style_context_restore() call");
      return;
    }

  gtk_style_context_pop_style_node (context);
}

/**
 * gtk_style_context_add_class:
 * @context: a #GtkStyleContext
 * @class_name: class name to use in styling
 *
 * Adds a style class to @context, so posterior calls to
 * gtk_style_context_get() or any of the gtk_render_*()
 * functions will make use of this new class for styling.
 *
 * In the CSS file format, a #GtkEntry defining a “search”
 * class, would be matched by:
 *
 * |[
 * entry.search { ... }
 * ]|
 *
 * While any widget defining a “search” class would be
 * matched by:
 * |[
 * .search { ... }
 * ]|
 *
 * Since: 3.0
 **/
void
gtk_style_context_add_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GQuark class_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_from_string (class_name);

  gtk_css_node_add_class (context->priv->cssnode, class_quark);
}

/**
 * gtk_style_context_remove_class:
 * @context: a #GtkStyleContext
 * @class_name: class name to remove
 *
 * Removes @class_name from @context.
 *
 * Since: 3.0
 **/
void
gtk_style_context_remove_class (GtkStyleContext *context,
                                const gchar     *class_name)
{
  GQuark class_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_try_string (class_name);
  if (!class_quark)
    return;

  gtk_css_node_remove_class (context->priv->cssnode, class_quark);
}

/**
 * gtk_style_context_has_class:
 * @context: a #GtkStyleContext
 * @class_name: a class name
 *
 * Returns %TRUE if @context currently has defined the
 * given class name.
 *
 * Returns: %TRUE if @context has @class_name defined
 *
 * Since: 3.0
 **/
gboolean
gtk_style_context_has_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GQuark class_quark;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (class_name != NULL, FALSE);

  class_quark = g_quark_try_string (class_name);
  if (!class_quark)
    return FALSE;

  return gtk_css_node_has_class (context->priv->cssnode, class_quark);
}

/**
 * gtk_style_context_list_classes:
 * @context: a #GtkStyleContext
 *
 * Returns the list of classes currently defined in @context.
 *
 * Returns: (transfer container) (element-type utf8): a #GList of
 *          strings with the currently defined classes. The contents
 *          of the list are owned by GTK+, but you must free the list
 *          itself with g_list_free() when you are done with it.
 *
 * Since: 3.0
 **/
GList *
gtk_style_context_list_classes (GtkStyleContext *context)
{
  GList *classes_list = NULL;
  const GQuark *classes;
  guint n_classes, i;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  classes = gtk_css_node_list_classes (context->priv->cssnode, &n_classes);
  for (i = n_classes; i > 0; i--)
    classes_list = g_list_prepend (classes_list, (gchar *)g_quark_to_string (classes[i - 1]));

  return classes_list;
}

static gint
style_property_values_cmp (gconstpointer bsearch_node1,
                           gconstpointer bsearch_node2)
{
  const PropertyValue *val1 = bsearch_node1;
  const PropertyValue *val2 = bsearch_node2;

  if (val1->widget_type != val2->widget_type)
    return val1->widget_type < val2->widget_type ? -1 : 1;

  if (val1->pspec != val2->pspec)
    return val1->pspec < val2->pspec ? -1 : 1;

  return 0;
}

GtkCssValue *
_gtk_style_context_peek_property (GtkStyleContext *context,
                                  guint            property_id)
{
  GtkCssStyle *values = gtk_style_context_lookup_style (context);

  return gtk_css_style_get_value (values, property_id);
}

const GValue *
_gtk_style_context_peek_style_property (GtkStyleContext *context,
                                        GType            widget_type,
                                        GParamSpec      *pspec)
{
  GtkStyleContextPrivate *priv;
  GtkWidgetPath *path;
  PropertyValue *pcache, key = { 0 };
  guint i;

  priv = context->priv;

  /* ensure the style cache is valid by forcing a validation */
  gtk_style_context_lookup_style (context);

  key.widget_type = widget_type;
  key.pspec = pspec;

  /* need value cache array */
  pcache = bsearch (&key,
                    priv->property_cache->data, priv->property_cache->len,
                    sizeof (PropertyValue), style_property_values_cmp);
  if (pcache)
    return &pcache->value;

  i = 0;
  while (i < priv->property_cache->len &&
         style_property_values_cmp (&key, &g_array_index (priv->property_cache, PropertyValue, i)) >= 0)
    i++;

  g_array_insert_val (priv->property_cache, i, key);
  pcache = &g_array_index (priv->property_cache, PropertyValue, i);

  /* cache miss, initialize value type, then set contents */
  g_param_spec_ref (pcache->pspec);
  g_value_init (&pcache->value, G_PARAM_SPEC_VALUE_TYPE (pspec));

  path = gtk_css_node_create_widget_path (gtk_style_context_get_root (context));
  if (path && gtk_widget_path_length (path) > 0)
    {
      if (gtk_style_provider_get_style_property (GTK_STYLE_PROVIDER (priv->cascade),
                                                 path,
                                                 gtk_widget_path_iter_get_state (path, -1),
                                                 pspec, &pcache->value))
        {
          gtk_widget_path_unref (path);

          return &pcache->value;
        }
    }

  gtk_widget_path_unref (path);

  /* not supplied by any provider, revert to default */
  g_param_value_set_default (pspec, &pcache->value);

  return &pcache->value;
}

/**
 * gtk_style_context_get_style_property:
 * @context: a #GtkStyleContext
 * @property_name: the name of the widget style property
 * @value: Return location for the property value
 *
 * Gets the value for a widget style property.
 *
 * When @value is no longer needed, g_value_unset() must be called
 * to free any allocated memory.
 **/
void
gtk_style_context_get_style_property (GtkStyleContext *context,
                                      const gchar     *property_name,
                                      GValue          *value)
{
  GtkCssNode *root;
  GtkWidgetClass *widget_class;
  GParamSpec *pspec;
  const GValue *peek_value;
  GType widget_type;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (value != NULL);

  root = gtk_style_context_get_root (context);

  if (GTK_IS_CSS_WIDGET_NODE (root))
    {
      GtkWidget *widget;

      widget = gtk_css_widget_node_get_widget (GTK_CSS_WIDGET_NODE (root));
      if (widget == NULL)
        return;

      widget_type = G_OBJECT_TYPE (widget);
    }
  else if (GTK_IS_CSS_PATH_NODE (root))
    {
      GtkWidgetPath *path;

      path = gtk_css_path_node_get_widget_path (GTK_CSS_PATH_NODE (root));
      if (path == NULL)
        return;

      widget_type = gtk_widget_path_get_object_type (path);

      if (!g_type_is_a (widget_type, GTK_TYPE_WIDGET))
        {
          g_warning ("%s: can't get style properties for non-widget class '%s'",
                     G_STRLOC,
                     g_type_name (widget_type));
          return;
        }
    }
  else
    {
      return;
    }

  widget_class = g_type_class_ref (widget_type);
  pspec = gtk_widget_class_find_style_property (widget_class, property_name);
  g_type_class_unref (widget_class);

  if (!pspec)
    {
      g_warning ("%s: widget class '%s' has no style property named '%s'",
                 G_STRLOC,
                 g_type_name (widget_type),
                 property_name);
      return;
    }

  peek_value = _gtk_style_context_peek_style_property (context, widget_type, pspec);

  if (G_VALUE_TYPE (value) == G_VALUE_TYPE (peek_value))
    g_value_copy (peek_value, value);
  else if (g_value_type_transformable (G_VALUE_TYPE (peek_value), G_VALUE_TYPE (value)))
    g_value_transform (peek_value, value);
  else
    g_warning ("can't retrieve style property '%s' of type '%s' as value of type '%s'",
               pspec->name,
               G_VALUE_TYPE_NAME (peek_value),
               G_VALUE_TYPE_NAME (value));
}

/**
 * gtk_style_context_get_style_valist:
 * @context: a #GtkStyleContext
 * @args: va_list of property name/return location pairs, followed by %NULL
 *
 * Retrieves several widget style properties from @context according to the
 * current style.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_style_valist (GtkStyleContext *context,
                                    va_list          args)
{
  GtkCssNode *root;
  const gchar *prop_name;
  GType widget_type;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  prop_name = va_arg (args, const gchar *);
  root = gtk_style_context_get_root (context);

  if (GTK_IS_CSS_WIDGET_NODE (root))
    {
      GtkWidget *widget;

      widget = gtk_css_widget_node_get_widget (GTK_CSS_WIDGET_NODE (root));
      if (widget == NULL)
        return;

      widget_type = G_OBJECT_TYPE (widget);
    }
  else if (GTK_IS_CSS_PATH_NODE (root))
    {
      GtkWidgetPath *path;

      path = gtk_css_path_node_get_widget_path (GTK_CSS_PATH_NODE (root));
      if (path == NULL)
        return;

      widget_type = gtk_widget_path_get_object_type (path);

      if (!g_type_is_a (widget_type, GTK_TYPE_WIDGET))
        {
          g_warning ("%s: can't get style properties for non-widget class '%s'",
                     G_STRLOC,
                     g_type_name (widget_type));
          return;
        }
    }
  else
    {
      return;
    }

  while (prop_name)
    {
      GtkWidgetClass *widget_class;
      GParamSpec *pspec;
      const GValue *peek_value;
      gchar *error;

      widget_class = g_type_class_ref (widget_type);
      pspec = gtk_widget_class_find_style_property (widget_class, prop_name);
      g_type_class_unref (widget_class);

      if (!pspec)
        {
          g_warning ("%s: widget class '%s' has no style property named '%s'",
                     G_STRLOC,
                     g_type_name (widget_type),
                     prop_name);
          break;
        }

      peek_value = _gtk_style_context_peek_style_property (context, widget_type, pspec);

      G_VALUE_LCOPY (peek_value, args, 0, &error);

      if (error)
        {
          g_warning ("can't retrieve style property '%s' of type '%s': %s",
                     pspec->name,
                     G_VALUE_TYPE_NAME (peek_value),
                     error);
          g_free (error);
          break;
        }

      prop_name = va_arg (args, const gchar *);
    }
}

/**
 * gtk_style_context_get_style:
 * @context: a #GtkStyleContext
 * @...: property name /return value pairs, followed by %NULL
 *
 * Retrieves several widget style properties from @context according to the
 * current style.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_style (GtkStyleContext *context,
                             ...)
{
  va_list args;

  va_start (args, context);
  gtk_style_context_get_style_valist (context, args);
  va_end (args);
}

/**
 * gtk_style_context_set_screen:
 * @context: a #GtkStyleContext
 * @screen: a #GdkScreen
 *
 * Attaches @context to the given screen.
 *
 * The screen is used to add style information from “global” style
 * providers, such as the screens #GtkSettings instance.
 *
 * If you are using a #GtkStyleContext returned from
 * gtk_widget_get_style_context(), you do not need to
 * call this yourself.
 *
 * Since: 3.0
 **/
void
gtk_style_context_set_screen (GtkStyleContext *context,
                              GdkScreen       *screen)
{
  GtkStyleContextPrivate *priv;
  GtkStyleCascade *screen_cascade;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_SCREEN (screen));

  priv = context->priv;
  if (priv->screen == screen)
    return;

  if (gtk_style_context_has_custom_cascade (context))
    {
      screen_cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_screen (screen), 1);
      _gtk_style_cascade_set_parent (priv->cascade, screen_cascade);
    }
  else
    {
      screen_cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_screen (screen),
                                                        _gtk_style_cascade_get_scale (priv->cascade));
      gtk_style_context_set_cascade (context, screen_cascade);
    }

  priv->screen = screen;

  g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_SCREEN]);
}

/**
 * gtk_style_context_get_screen:
 * @context: a #GtkStyleContext
 *
 * Returns the #GdkScreen to which @context is attached.
 *
 * Returns: (transfer none): a #GdkScreen.
 **/
GdkScreen *
gtk_style_context_get_screen (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return context->priv->screen;
}

/**
 * gtk_style_context_set_frame_clock:
 * @context: a #GdkFrameClock
 * @frame_clock: a #GdkFrameClock
 *
 * Attaches @context to the given frame clock.
 *
 * The frame clock is used for the timing of animations.
 *
 * If you are using a #GtkStyleContext returned from
 * gtk_widget_get_style_context(), you do not need to
 * call this yourself.
 *
 * Since: 3.8
 **/
void
gtk_style_context_set_frame_clock (GtkStyleContext *context,
                                   GdkFrameClock   *frame_clock)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (frame_clock == NULL || GDK_IS_FRAME_CLOCK (frame_clock));

  if (g_set_object (&context->priv->frame_clock, frame_clock))
    g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_FRAME_CLOCK]);
}

/**
 * gtk_style_context_get_frame_clock:
 * @context: a #GtkStyleContext
 *
 * Returns the #GdkFrameClock to which @context is attached.
 *
 * Returns: (nullable) (transfer none): a #GdkFrameClock, or %NULL
 *  if @context does not have an attached frame clock.
 *
 * Since: 3.8
 **/
GdkFrameClock *
gtk_style_context_get_frame_clock (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return context->priv->frame_clock;
}

gboolean
_gtk_style_context_resolve_color (GtkStyleContext    *context,
                                  GtkCssValue        *color,
                                  GdkRGBA            *result)
{
  GtkCssValue *val;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  val = _gtk_css_color_value_resolve (color,
                                      GTK_STYLE_PROVIDER_PRIVATE (context->priv->cascade),
                                      _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR),
                                      NULL);
  if (val == NULL)
    return FALSE;

  *result = *_gtk_css_rgba_value_get_rgba (val);
  _gtk_css_value_unref (val);
  return TRUE;
}

/**
 * gtk_style_context_lookup_color:
 * @context: a #GtkStyleContext
 * @color_name: color name to lookup
 * @color: (out): Return location for the looked up color
 *
 * Looks up and resolves a color name in the @context color map.
 *
 * Returns: %TRUE if @color_name was found and resolved, %FALSE otherwise
 **/
gboolean
gtk_style_context_lookup_color (GtkStyleContext *context,
                                const gchar     *color_name,
                                GdkRGBA         *color)
{
  GtkCssValue *value;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (color_name != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  value = _gtk_style_provider_private_get_color (GTK_STYLE_PROVIDER_PRIVATE (context->priv->cascade), color_name);
  if (value == NULL)
    return FALSE;

  return _gtk_style_context_resolve_color (context, value, color);
}

static GtkCssStyleChange magic_number;

void
gtk_style_context_validate (GtkStyleContext  *context,
                            GtkCssStyleChange *change)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  /* Avoid reentrancy */
  if (priv->invalidating_context)
    return;

  if (change)
    priv->invalidating_context = change;
  else
    priv->invalidating_context = &magic_number;

  g_signal_emit (context, signals[CHANGED], 0);

  priv->invalidating_context = NULL;
}

/**
 * gtk_style_context_get_color:
 * @context: a #GtkStyleContext
 * @color: (out): return value for the foreground color
 *
 * Gets the foreground color for a given state.
 *
 * See gtk_style_context_get_property() and
 * #GTK_STYLE_PROPERTY_COLOR for details.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_color (GtkStyleContext *context,
                             GdkRGBA         *color)
{
  GdkRGBA *c;

  g_return_if_fail (color != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context,
                         "color", &c,
                         NULL);

  *color = *c;
  gdk_rgba_free (c);
}

/**
 * gtk_style_context_get_background_color:
 * @context: a #GtkStyleContext
 * @color: (out): return value for the background color
 *
 * Gets the background color for a given state.
 *
 * This function is far less useful than it seems, and it should not be used in
 * newly written code. CSS has no concept of "background color", as a background
 * can be an image, or a gradient, or any other pattern including solid colors.
 *
 * The only reason why you would call gtk_style_context_get_background_color() is
 * to use the returned value to draw the background with it; the correct way to
 * achieve this result is to use gtk_render_background() instead, along with CSS
 * style classes to modify the color to be rendered.
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: Use gtk_render_background() instead.
 **/
void
gtk_style_context_get_background_color (GtkStyleContext *context,
                                        GdkRGBA         *color)
{
  GdkRGBA *c;

  g_return_if_fail (color != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context,
                         "background-color", &c,
                         NULL);

  *color = *c;
  gdk_rgba_free (c);
}

/**
 * gtk_style_context_get_border_color:
 * @context: a #GtkStyleContext
 * @color: (out): return value for the border color
 *
 * Gets the border color for a given state.
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: Use gtk_render_frame() instead.
 **/
void
gtk_style_context_get_border_color (GtkStyleContext *context,
                                    GdkRGBA         *color)
{
  GdkRGBA *c;

  g_return_if_fail (color != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context,
                         "border-color", &c,
                         NULL);

  *color = *c;
  gdk_rgba_free (c);
}

/**
 * gtk_style_context_get_border:
 * @context: a #GtkStyleContext
 * @border: (out): return value for the border settings
 *
 * Gets the border for a given state as a #GtkBorder.
 *
 * See gtk_style_context_get_property() and
 * #GTK_STYLE_PROPERTY_BORDER_WIDTH for details.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_border (GtkStyleContext *context,
                              GtkBorder       *border)
{
  GtkCssStyle *style;
  double top, left, bottom, right;

  g_return_if_fail (border != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  style = gtk_style_context_lookup_style (context);

  top = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH), 100));
  right = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH), 100));
  bottom = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH), 100));
  left = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH), 100));

  border->top = top;
  border->left = left;
  border->bottom = bottom;
  border->right = right;
}

/**
 * gtk_style_context_get_padding:
 * @context: a #GtkStyleContext
 * @padding: (out): return value for the padding settings
 *
 * Gets the padding for a given state as a #GtkBorder.
 * See gtk_style_context_get() and #GTK_STYLE_PROPERTY_PADDING
 * for details.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_padding (GtkStyleContext *context,
                               GtkBorder       *padding)
{
  GtkCssStyle *style;
  double top, left, bottom, right;

  g_return_if_fail (padding != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  style = gtk_style_context_lookup_style (context);

  top = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_PADDING_TOP), 100));
  right = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_PADDING_RIGHT), 100));
  bottom = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_PADDING_BOTTOM), 100));
  left = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_PADDING_LEFT), 100));

  padding->top = top;
  padding->left = left;
  padding->bottom = bottom;
  padding->right = right;
}

/**
 * gtk_style_context_get_margin:
 * @context: a #GtkStyleContext
 * @margin: (out): return value for the margin settings
 *
 * Gets the margin for a given state as a #GtkBorder.
 * See gtk_style_property_get() and #GTK_STYLE_PROPERTY_MARGIN
 * for details.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_margin (GtkStyleContext *context,
                              GtkBorder       *margin)
{
  GtkCssStyle *style;
  double top, left, bottom, right;

  g_return_if_fail (margin != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  style = gtk_style_context_lookup_style (context);

  top = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MARGIN_TOP), 100));
  right = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MARGIN_RIGHT), 100));
  bottom = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MARGIN_BOTTOM), 100));
  left = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MARGIN_LEFT), 100));

  margin->top = top;
  margin->left = left;
  margin->bottom = bottom;
  margin->right = right;
}

void
_gtk_style_context_get_cursor_color (GtkStyleContext *context,
                                     GdkRGBA         *primary_color,
                                     GdkRGBA         *secondary_color)
{
  GdkRGBA *pc, *sc;

  gtk_style_context_get (context,
                         "caret-color", &pc,
                         "-gtk-secondary-caret-color", &sc,
                         NULL);
  if (primary_color)
    *primary_color = *pc;

  if (secondary_color)
    *secondary_color = *sc;

  gdk_rgba_free (pc);
  gdk_rgba_free (sc);
}

static void
draw_insertion_cursor (GtkStyleContext *context,
                       cairo_t         *cr,
                       gdouble          x,
                       gdouble          y,
                       gdouble          height,
                       gboolean         is_primary,
                       PangoDirection   direction,
                       gboolean         draw_arrow)
{
  GdkRGBA primary_color;
  GdkRGBA secondary_color;
  gint stem_width;
  gint offset;

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_style_context_get_cursor_color (context, &primary_color, &secondary_color);
  gdk_cairo_set_source_rgba (cr, is_primary ? &primary_color : &secondary_color);

  /* When changing the shape or size of the cursor here,
   * propagate the changes to gtktextview.c:text_window_invalidate_cursors().
   */

  stem_width = height * CURSOR_ASPECT_RATIO + 1;

  /* put (stem_width % 2) on the proper side of the cursor */
  if (direction == PANGO_DIRECTION_LTR)
    offset = stem_width / 2;
  else
    offset = stem_width - stem_width / 2;

  cairo_rectangle (cr, x - offset, y, stem_width, height);
  cairo_fill (cr);

  if (draw_arrow)
    {
      gint arrow_width;
      gint ax, ay;

      arrow_width = stem_width + 1;

      if (direction == PANGO_DIRECTION_RTL)
        {
          ax = x - offset - 1;
          ay = y + height - arrow_width * 2 - arrow_width + 1;

          cairo_move_to (cr, ax, ay + 1);
          cairo_line_to (cr, ax - arrow_width, ay + arrow_width);
          cairo_line_to (cr, ax, ay + 2 * arrow_width);
          cairo_fill (cr);
        }
      else if (direction == PANGO_DIRECTION_LTR)
        {
          ax = x + stem_width - offset;
          ay = y + height - arrow_width * 2 - arrow_width + 1;

          cairo_move_to (cr, ax, ay + 1);
          cairo_line_to (cr, ax + arrow_width, ay + arrow_width);
          cairo_line_to (cr, ax, ay + 2 * arrow_width);
          cairo_fill (cr);
        }
      else
        g_assert_not_reached();
    }

  cairo_restore (cr);
}

static void
get_insertion_cursor_bounds (gdouble          height,
                             PangoDirection   direction,
                             gboolean         draw_arrow,
                             graphene_rect_t *bounds)
{
  gint stem_width;
  gint offset;

  stem_width = height * CURSOR_ASPECT_RATIO + 1;
  if (direction == PANGO_DIRECTION_LTR)
    offset = stem_width / 2;
  else
    offset = stem_width - stem_width / 2;

  if (draw_arrow)
    {
      if (direction == PANGO_DIRECTION_LTR)
        {
          graphene_rect_init (bounds,
                              - offset, 0,
                              2 * stem_width + 1, height);
        }
      else
        {
          graphene_rect_init (bounds,
                              - offset - stem_width - 2, 0,
                              2 * stem_width + 2, height);
        }
    }
  else
    {
      graphene_rect_init (bounds,
                          - offset, 0,
                          stem_width, height);
    }
}

static void
snapshot_insertion_cursor (GtkSnapshot     *snapshot,
                           GtkStyleContext *context,
                           gdouble          height,
                           gboolean         is_primary,
                           PangoDirection   direction,
                           gboolean         draw_arrow)
{
  graphene_rect_t bounds;
  cairo_t *cr;
  
  get_insertion_cursor_bounds (height, direction, draw_arrow, &bounds);
  cr = gtk_snapshot_append_cairo (snapshot,
                                  &bounds,
                                  "%s Cursor", is_primary ? "Primary" : "Secondary");

  draw_insertion_cursor (context, cr, 0, 0, height, is_primary, direction, draw_arrow);

  cairo_destroy (cr);
}

/**
 * gtk_render_insertion_cursor:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin
 * @y: Y origin
 * @layout: the #PangoLayout of the text
 * @index: the index in the #PangoLayout
 * @direction: the #PangoDirection of the text
 *
 * Draws a text caret on @cr at the specified index of @layout.
 *
 * Since: 3.4
 **/
void
gtk_render_insertion_cursor (GtkStyleContext *context,
                             cairo_t         *cr,
                             gdouble          x,
                             gdouble          y,
                             PangoLayout     *layout,
                             int              index,
                             PangoDirection   direction)
{
  GtkStyleContextPrivate *priv;
  gboolean split_cursor;
  PangoRectangle strong_pos, weak_pos;
  PangoRectangle *cursor1, *cursor2;
  PangoDirection keymap_direction;
  PangoDirection direction2;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (index >= 0);

  priv = context->priv;

  g_object_get (gtk_settings_get_for_screen (priv->screen),
                "gtk-split-cursor", &split_cursor,
                NULL);

  keymap_direction = gdk_keymap_get_direction (gdk_keymap_get_for_display (gdk_screen_get_display (priv->screen)));

  pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);

  direction2 = PANGO_DIRECTION_NEUTRAL;

  if (split_cursor)
    {
      cursor1 = &strong_pos;

      if (strong_pos.x != weak_pos.x || strong_pos.y != weak_pos.y)
        {
          direction2 = (direction == PANGO_DIRECTION_LTR) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
          cursor2 = &weak_pos;
        }
    }
  else
    {
      if (keymap_direction == direction)
        cursor1 = &strong_pos;
      else
        cursor1 = &weak_pos;
    }

  draw_insertion_cursor (context,
                         cr,
                         x + PANGO_PIXELS (cursor1->x),
                         y + PANGO_PIXELS (cursor1->y),
                         PANGO_PIXELS (cursor1->height),
                         TRUE,
                         direction,
                         direction2 != PANGO_DIRECTION_NEUTRAL);

  if (direction2 != PANGO_DIRECTION_NEUTRAL)
    {
      draw_insertion_cursor (context,
                             cr,
                             x + PANGO_PIXELS (cursor2->x),
                             y + PANGO_PIXELS (cursor2->y),
                             PANGO_PIXELS (cursor2->height),
                             FALSE,
                             direction2,
                             TRUE);
    }
}

/**
 * gtk_snapshot_render_insertion_cursor:
 * @snapshot: snapshot to render to
 * @context: a #GtkStyleContext
 * @x: X origin
 * @y: Y origin
 * @layout: the #PangoLayout of the text
 * @index: the index in the #PangoLayout
 * @direction: the #PangoDirection of the text
 *
 * Draws a text caret on @cr at the specified index of @layout.
 *
 * Since: 3.90
 **/
void
gtk_snapshot_render_insertion_cursor (GtkSnapshot     *snapshot,
                                      GtkStyleContext *context,
                                      gdouble          x,
                                      gdouble          y,
                                      PangoLayout     *layout,
                                      int              index,
                                      PangoDirection   direction)
{
  GtkStyleContextPrivate *priv;
  gboolean split_cursor;
  PangoRectangle strong_pos, weak_pos;
  PangoRectangle *cursor1, *cursor2;
  PangoDirection keymap_direction;
  PangoDirection direction2;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (index >= 0);

  priv = context->priv;

  g_object_get (gtk_settings_get_for_screen (priv->screen),
                "gtk-split-cursor", &split_cursor,
                NULL);

  keymap_direction = gdk_keymap_get_direction (gdk_keymap_get_for_display (gdk_screen_get_display (priv->screen)));

  pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);

  direction2 = PANGO_DIRECTION_NEUTRAL;

  if (split_cursor)
    {
      cursor1 = &strong_pos;

      if (strong_pos.x != weak_pos.x || strong_pos.y != weak_pos.y)
        {
          direction2 = (direction == PANGO_DIRECTION_LTR) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
          cursor2 = &weak_pos;
        }
    }
  else
    {
      if (keymap_direction == direction)
        cursor1 = &strong_pos;
      else
        cursor1 = &weak_pos;
    }

  gtk_snapshot_offset (snapshot, x + PANGO_PIXELS (cursor1->x), y + PANGO_PIXELS (cursor1->y));
  snapshot_insertion_cursor (snapshot,
                             context,
                             PANGO_PIXELS (cursor1->height),
                             TRUE,
                             direction,
                             direction2 != PANGO_DIRECTION_NEUTRAL);
  gtk_snapshot_offset (snapshot, - x - PANGO_PIXELS (cursor1->x), - y - PANGO_PIXELS (cursor1->y));

  if (direction2 != PANGO_DIRECTION_NEUTRAL)
    {
      gtk_snapshot_offset (snapshot, x + PANGO_PIXELS (cursor2->x), y + PANGO_PIXELS (cursor2->y));
      snapshot_insertion_cursor (snapshot,
                                 context,
                                 PANGO_PIXELS (cursor2->height),
                                 FALSE,
                                 direction2,
                                 TRUE);
      gtk_snapshot_offset (snapshot, - x - PANGO_PIXELS (cursor2->x), - y - PANGO_PIXELS (cursor2->y));
    }
}

/**
 * gtk_style_context_get_change:
 * @context: the context to query
 *
 * Queries the context for the changes for the currently executing
 * GtkStyleContext::invalidate signal. If no signal is currently
 * emitted or the signal has not been triggered by a CssNode
 * invalidation, this function returns %NULL.
 *
 * FIXME 4.0: Make this part of the signal.
 *
 * Returns: %NULL or the currently invalidating changes
 **/
GtkCssStyleChange *
gtk_style_context_get_change (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  if (context->priv->invalidating_context == &magic_number)
    return NULL;

  return context->priv->invalidating_context;
}

void
_gtk_style_context_get_icon_extents (GtkStyleContext *context,
                                     GdkRectangle    *extents,
                                     gint             x,
                                     gint             y,
                                     gint             width,
                                     gint             height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (extents != NULL);

  if (_gtk_css_image_value_get_image (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_SOURCE)) == NULL)
    {
      extents->x = extents->y = extents->width = extents->height = 0;
      return;
    }

  gtk_css_style_render_icon_get_extents (gtk_style_context_lookup_style (context),
                                         extents,
                                         x, y, width, height);
}

PangoAttrList *
_gtk_style_context_get_pango_attributes (GtkStyleContext *context)
{
  return gtk_css_style_get_pango_attributes (gtk_style_context_lookup_style (context));
}

static AtkAttributeSet *
add_attribute (AtkAttributeSet  *attributes,
               AtkTextAttribute  attr,
               const gchar      *value)
{
  AtkAttribute *at;

  at = g_new (AtkAttribute, 1);
  at->name = g_strdup (atk_text_attribute_get_name (attr));
  at->value = g_strdup (value);

  return g_slist_prepend (attributes, at);
}

/*
 * _gtk_style_context_get_attributes:
 * @attributes: a #AtkAttributeSet to add attributes to
 * @context: the #GtkStyleContext to get attributes from
 * @flags: the state to use with @context
 *
 * Adds the foreground and background color from @context to
 * @attributes, after translating them to ATK attributes.
 *
 * This is a convenience function that can be used in
 * implementing the #AtkText interface in widgets.
 *
 * Returns: the modified #AtkAttributeSet
 */
AtkAttributeSet *
_gtk_style_context_get_attributes (AtkAttributeSet *attributes,
                                   GtkStyleContext *context)
{
  GdkRGBA color;
  gchar *value;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_style_context_get_background_color (context, &color);
G_GNUC_END_IGNORE_DEPRECATIONS
  value = g_strdup_printf ("%u,%u,%u",
                           (guint) ceil (color.red * 65536 - color.red),
                           (guint) ceil (color.green * 65536 - color.green),
                           (guint) ceil (color.blue * 65536 - color.blue));
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_BG_COLOR, value);
  g_free (value);

  gtk_style_context_get_color (context, &color);
  value = g_strdup_printf ("%u,%u,%u",
                           (guint) ceil (color.red * 65536 - color.red),
                           (guint) ceil (color.green * 65536 - color.green),
                           (guint) ceil (color.blue * 65536 - color.blue));
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_FG_COLOR, value);
  g_free (value);

  return attributes;
}

/**
 * GtkStyleContextPrintFlags:
 * @GTK_STYLE_CONTEXT_PRINT_RECURSE: Print the entire tree of
 *     CSS nodes starting at the style context's node
 * @GTK_STYLE_CONTEXT_PRINT_SHOW_STYLE: Show the values of the
 *     CSS properties for each node
 *
 * Flags that modify the behavior of gtk_style_context_to_string().
 * New values may be added to this enumeration.
 */

/**
 * gtk_style_context_to_string:
 * @context: a #GtkStyleContext
 * @flags: Flags that determine what to print
 *
 * Converts the style context into a string representation.
 *
 * The string representation always includes information about
 * the name, state, id, visibility and style classes of the CSS
 * node that is backing @context. Depending on the flags, more
 * information may be included.
 *
 * This function is intended for testing and debugging of the
 * CSS implementation in GTK+. There are no guarantees about
 * the format of the returned string, it may change.
 *
 * Returns: a newly allocated string representing @context
 *
 * Since: 3.20
 */
char *
gtk_style_context_to_string (GtkStyleContext           *context,
                             GtkStyleContextPrintFlags  flags)
{
  GString *string;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  string = g_string_new ("");

  gtk_css_node_print (context->priv->cssnode, flags, string, 0);

  return g_string_free (string, FALSE);
}
