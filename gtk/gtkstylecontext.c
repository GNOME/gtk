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

#include "gtkcontainerprivate.h"
#include "gtkcssanimatedstyleprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssnodedeclarationprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsspathnodeprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcssstaticstyleprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransformvalueprivate.h"
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

#include "deprecated/gtkgradientprivate.h"
#include "deprecated/gtksymboliccolorprivate.h"

#include "fallback-c89.c"

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
 * gtk_style_context_set_path() and possibly gtk_style_context_set_screen(). See
 * the “Foreign drawing“ example in gtk3-demo.
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
 * # Style Regions
 *
 * Widgets can also add regions with flags to their context. This feature is
 * deprecated and will be removed in a future GTK+ update. Please use style
 * classes instead.
 *
 * GTK+ defines macros for a number of style regions.
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
 * `XDG_CONFIG_HOME/gtk-3.0/gtk.css` will
 * still take precedence over your changes, as it uses the
 * %GTK_STYLE_PROVIDER_PRIORITY_USER priority.
 */

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
  PROP_DIRECTION,
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
    {
      GtkWidget *widget = gtk_css_widget_node_get_widget (GTK_CSS_WIDGET_NODE (priv->cssnode));
      if (widget != NULL)
        _gtk_widget_style_context_invalidated (widget);
    }
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
                  NULL,
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

  properties[PROP_DIRECTION] =
      g_param_spec_enum ("direction",
                         P_("Direction"),
                         P_("Text direction"),
                         GTK_TYPE_TEXT_DIRECTION,
                         GTK_TEXT_DIR_LTR,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_DEPRECATED);

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
    case PROP_DIRECTION:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_style_context_set_direction (context, g_value_get_enum (value));
      G_GNUC_END_IGNORE_DEPRECATIONS;
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
    case PROP_DIRECTION:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_value_set_enum (value, gtk_style_context_get_direction (context));
      G_GNUC_END_IGNORE_DEPRECATIONS;
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
 * changes state/regions/classes.
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

static GtkStateFlags
gtk_style_context_push_state (GtkStyleContext *context,
                              GtkStateFlags    state)
{
  GtkStyleContextPrivate *priv = context->priv;
  GtkStateFlags current_state;
  GtkCssNode *root;

  current_state = gtk_css_node_get_state (priv->cssnode);

  if (current_state == state)
    return state;

  root = gtk_style_context_get_root (context);

  if (GTK_IS_CSS_TRANSIENT_NODE (priv->cssnode))
    {
      /* don't emit a warning, changing state here is fine */
    }
  else if (GTK_IS_CSS_WIDGET_NODE (root))
    {
      GtkWidget *widget = gtk_css_widget_node_get_widget (GTK_CSS_WIDGET_NODE (root));
      g_debug ("State %u for %s %p doesn't match state %u set via gtk_style_context_set_state ()",
               state, (widget == NULL) ? "(null)" : gtk_widget_get_name (widget),
               widget, gtk_css_node_get_state (priv->cssnode));
    }
  else
    {
      g_debug ("State %u for context %p doesn't match state %u set via gtk_style_context_set_state ()",
               state, context, gtk_css_node_get_state (priv->cssnode));
    }

  gtk_css_node_set_state (priv->cssnode, state);

  return current_state;
}

static void
gtk_style_context_pop_state (GtkStyleContext *context,
                             GtkStateFlags    saved_state)
{
  gtk_css_node_set_state (context->priv->cssnode, saved_state);
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
 * @state: state to retrieve the property value for
 * @value: (out) (transfer full):  return location for the style property value
 *
 * Gets a style property from @context for the given state.
 *
 * Note that not all CSS properties that are supported by GTK+ can be
 * retrieved in this way, since they may not be representable as #GValue.
 * GTK+ defines macros for a number of properties that can be used
 * with this function.
 *
 * Note that passing a state other than the current state of @context
 * is not recommended unless the style context has been saved with
 * gtk_style_context_save().
 *
 * When @value is no longer needed, g_value_unset() must be called
 * to free any allocated memory.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_property (GtkStyleContext *context,
                                const gchar     *property,
                                GtkStateFlags    state,
                                GValue          *value)
{
  GtkStateFlags saved_state;
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

  saved_state = gtk_style_context_push_state (context, state);
  _gtk_style_property_query (prop,
                             value,
                             gtk_style_context_query_func,
                             gtk_css_node_get_style (context->priv->cssnode));
  gtk_style_context_pop_state (context, saved_state);
}

/**
 * gtk_style_context_get_valist:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the property values for
 * @args: va_list of property name/return location pairs, followed by %NULL
 *
 * Retrieves several style property values from @context for a given state.
 *
 * See gtk_style_context_get_property() for details.
 *
 * Since: 3.0
 */
void
gtk_style_context_get_valist (GtkStyleContext *context,
                              GtkStateFlags    state,
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
                                      state,
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
 * @state: state to retrieve the property values for
 * @...: property name / return value pairs, followed by %NULL
 *
 * Retrieves several style property values from @context for a
 * given state.
 *
 * See gtk_style_context_get_property() for details.
 *
 * For the property name / return value pairs, it works similarly as
 * g_object_get(). Example:
 *
 * |[<!-- language="C" -->
 * GdkRGBA *background_color = NULL;
 * PangoFontDescription *font_desc = NULL;
 * gint border_radius = 0;
 *
 * gtk_style_context_get (style_context,
 *                        gtk_style_context_get_state (style_context),
 *                        GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &background_color,
 *                        GTK_STYLE_PROPERTY_FONT, &font_desc,
 *                        GTK_STYLE_PROPERTY_BORDER_RADIUS, &border_radius,
 *                        NULL);
 *
 * // Do something with the property values.
 *
 * if (background_color != NULL)
 *   gdk_rgba_free (background_color);
 * if (font_desc != NULL)
 *   pango_font_description_free (font_desc);
 * ]|
 *
 * Since: 3.0
 */
void
gtk_style_context_get (GtkStyleContext *context,
                       GtkStateFlags    state,
                       ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  va_start (args, state);
  gtk_style_context_get_valist (context, state, args);
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
  GtkStateFlags old_flags;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  old_flags = gtk_css_node_get_state (context->priv->cssnode);

  gtk_css_node_set_state (context->priv->cssnode, flags);

  if (((old_flags ^ flags) & (GTK_STATE_FLAG_DIR_LTR | GTK_STATE_FLAG_DIR_RTL)) &&
      !gtk_style_context_is_saved (context))
    g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_DIRECTION]);
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
 * gtk_style_context_state_is_running:
 * @context: a #GtkStyleContext
 * @state: a widget state
 * @progress: (out): return location for the transition progress
 *
 * Returns %TRUE if there is a transition animation running for the
 * current region (see gtk_style_context_push_animatable_region()).
 *
 * If @progress is not %NULL, the animation progress will be returned
 * there, 0.0 means the state is closest to being unset, while 1.0 means
 * it’s closest to being set. This means transition animation will
 * run from 0 to 1 when @state is being set and from 1 to 0 when
 * it’s being unset.
 *
 * Returns: %TRUE if there is a running transition animation for @state.
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: This function always returns %FALSE
 **/
gboolean
gtk_style_context_state_is_running (GtkStyleContext *context,
                                    GtkStateType     state,
                                    gdouble         *progress)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);

  return FALSE;
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
 * |[ <!-- language="CSS" -->
 * entry.search { ... }
 * ]|
 *
 * While any widget defining a “search” class would be
 * matched by:
 * |[ <!-- language="CSS" -->
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

/**
 * gtk_style_context_list_regions:
 * @context: a #GtkStyleContext
 *
 * Returns the list of regions currently defined in @context.
 *
 * Returns: (transfer container) (element-type utf8): a #GList of
 *          strings with the currently defined regions. The contents
 *          of the list are owned by GTK+, but you must free the list
 *          itself with g_list_free() when you are done with it.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
GList *
gtk_style_context_list_regions (GtkStyleContext *context)
{
  GList *regions, *l;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  regions = gtk_css_node_list_regions (context->priv->cssnode);
  for (l = regions; l; l = l->next)
    l->data = (char *) g_quark_to_string (GPOINTER_TO_UINT (l->data));

  return regions;
}

gboolean
_gtk_style_context_check_region_name (const gchar *str)
{
  g_return_val_if_fail (str != NULL, FALSE);

  if (!g_ascii_islower (str[0]))
    return FALSE;

  while (*str)
    {
      if (*str != '-' &&
          !g_ascii_islower (*str))
        return FALSE;

      str++;
    }

  return TRUE;
}

/**
 * gtk_style_context_add_region:
 * @context: a #GtkStyleContext
 * @region_name: region name to use in styling
 * @flags: flags that apply to the region
 *
 * Adds a region to @context, so posterior calls to
 * gtk_style_context_get() or any of the gtk_render_*()
 * functions will make use of this new region for styling.
 *
 * In the CSS file format, a #GtkTreeView defining a “row”
 * region, would be matched by:
 *
 * |[ <!-- language="CSS" -->
 * treeview row { ... }
 * ]|
 *
 * Pseudo-classes are used for matching @flags, so the two
 * following rules:
 * |[ <!-- language="CSS" -->
 * treeview row:nth-child(even) { ... }
 * treeview row:nth-child(odd) { ... }
 * ]|
 *
 * would apply to even and odd rows, respectively.
 *
 * Region names must only contain lowercase letters
 * and “-”, starting always with a lowercase letter.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
void
gtk_style_context_add_region (GtkStyleContext *context,
                              const gchar     *region_name,
                              GtkRegionFlags   flags)
{
  GQuark region_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (region_name != NULL);
  g_return_if_fail (_gtk_style_context_check_region_name (region_name));

  region_quark = g_quark_from_string (region_name);

  gtk_css_node_add_region (context->priv->cssnode, region_quark, flags);
}

/**
 * gtk_style_context_remove_region:
 * @context: a #GtkStyleContext
 * @region_name: region name to unset
 *
 * Removes a region from @context.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
void
gtk_style_context_remove_region (GtkStyleContext *context,
                                 const gchar     *region_name)
{
  GQuark region_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (region_name != NULL);

  region_quark = g_quark_try_string (region_name);
  if (!region_quark)
    return;

  gtk_css_node_remove_region (context->priv->cssnode, region_quark);
}

/**
 * gtk_style_context_has_region:
 * @context: a #GtkStyleContext
 * @region_name: a region name
 * @flags_return: (out) (allow-none): return location for region flags
 *
 * Returns %TRUE if @context has the region defined.
 * If @flags_return is not %NULL, it is set to the flags
 * affecting the region.
 *
 * Returns: %TRUE if region is defined
 *
 * Since: 3.0
 *
 * Deprecated: 3.14
 **/
gboolean
gtk_style_context_has_region (GtkStyleContext *context,
                              const gchar     *region_name,
                              GtkRegionFlags  *flags_return)
{
  GQuark region_quark;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (region_name != NULL, FALSE);

  if (flags_return)
    *flags_return = 0;

  region_quark = g_quark_try_string (region_name);
  if (!region_quark)
    return FALSE;

  return gtk_css_node_has_region (context->priv->cssnode, region_quark, flags_return);
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
          G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

          /* Resolve symbolic colors to GdkColor/GdkRGBA */
          if (G_VALUE_TYPE (&pcache->value) == GTK_TYPE_SYMBOLIC_COLOR)
            {
              GtkSymbolicColor *color;
              GdkRGBA rgba;

              color = g_value_dup_boxed (&pcache->value);

              g_value_unset (&pcache->value);

              if (G_PARAM_SPEC_VALUE_TYPE (pspec) == GDK_TYPE_RGBA)
                g_value_init (&pcache->value, GDK_TYPE_RGBA);
              else
                g_value_init (&pcache->value, GDK_TYPE_COLOR);

              if (_gtk_style_context_resolve_color (context, _gtk_symbolic_color_get_css_value (color), &rgba))
                {
                  if (G_PARAM_SPEC_VALUE_TYPE (pspec) == GDK_TYPE_RGBA)
                    g_value_set_boxed (&pcache->value, &rgba);
                  else
                    {
                      GdkColor rgb;

                      rgb.red = rgba.red * 65535. + 0.5;
                      rgb.green = rgba.green * 65535. + 0.5;
                      rgb.blue = rgba.blue * 65535. + 0.5;

                      g_value_set_boxed (&pcache->value, &rgb);
                    }
                }
              else
                g_param_value_set_default (pspec, &pcache->value);

              gtk_symbolic_color_unref (color);
            }

          G_GNUC_END_IGNORE_DEPRECATIONS;

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
 * gtk_style_context_lookup_icon_set:
 * @context: a #GtkStyleContext
 * @stock_id: an icon name
 *
 * Looks up @stock_id in the icon factories associated to @context and
 * the default icon factory, returning an icon set if found, otherwise
 * %NULL.
 *
 * Returns: (nullable) (transfer none): The looked up %GtkIconSet, or %NULL
 *
 * Deprecated: 3.10: Use gtk_icon_theme_lookup_icon() instead.
 **/
GtkIconSet *
gtk_style_context_lookup_icon_set (GtkStyleContext *context,
                                   const gchar     *stock_id)
{
  GtkIconSet *icon_set;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  icon_set = gtk_icon_factory_lookup_default (stock_id);

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return icon_set;
}

/**
 * gtk_style_context_set_screen:
 * @context: a #GtkStyleContext
 * @screen: a #GdkScreen
 *
 * Attaches @context to the given screen.
 *
 * The screen is used to add style information from “global” style
 * providers, such as the screen’s #GtkSettings instance.
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

/**
 * gtk_style_context_set_direction:
 * @context: a #GtkStyleContext
 * @direction: the new direction.
 *
 * Sets the reading direction for rendering purposes.
 *
 * If you are using a #GtkStyleContext returned from
 * gtk_widget_get_style_context(), you do not need to
 * call this yourself.
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: Use gtk_style_context_set_state() with
 *   #GTK_STATE_FLAG_DIR_LTR and #GTK_STATE_FLAG_DIR_RTL
 *   instead.
 **/
void
gtk_style_context_set_direction (GtkStyleContext  *context,
                                 GtkTextDirection  direction)
{
  GtkStateFlags state;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  state = gtk_style_context_get_state (context);
  state &= ~(GTK_STATE_FLAG_DIR_LTR | GTK_STATE_FLAG_DIR_RTL);

  switch (direction)
    {
    case GTK_TEXT_DIR_LTR:
      state |= GTK_STATE_FLAG_DIR_LTR;
      break;

    case GTK_TEXT_DIR_RTL:
      state |= GTK_STATE_FLAG_DIR_RTL;
      break;

    case GTK_TEXT_DIR_NONE:
    default:
      break;
    }

  gtk_style_context_set_state (context, state);
}

/**
 * gtk_style_context_get_direction:
 * @context: a #GtkStyleContext
 *
 * Returns the widget direction used for rendering.
 *
 * Returns: the widget direction
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: Use gtk_style_context_get_state() and
 *   check for #GTK_STATE_FLAG_DIR_LTR and
 *   #GTK_STATE_FLAG_DIR_RTL instead.
 **/
GtkTextDirection
gtk_style_context_get_direction (GtkStyleContext *context)
{
  GtkStateFlags state;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), GTK_TEXT_DIR_LTR);

  state = gtk_style_context_get_state (context);

  if (state & GTK_STATE_FLAG_DIR_LTR)
    return GTK_TEXT_DIR_LTR;
  else if (state & GTK_STATE_FLAG_DIR_RTL)
    return GTK_TEXT_DIR_RTL;
  else
    return GTK_TEXT_DIR_NONE;
}

/**
 * gtk_style_context_set_junction_sides:
 * @context: a #GtkStyleContext
 * @sides: sides where rendered elements are visually connected to
 *     other elements
 *
 * Sets the sides where rendered elements (mostly through
 * gtk_render_frame()) will visually connect with other visual elements.
 *
 * This is merely a hint that may or may not be honored
 * by themes.
 *
 * Container widgets are expected to set junction hints as appropriate
 * for their children, so it should not normally be necessary to call
 * this function manually.
 *
 * Since: 3.0
 **/
void
gtk_style_context_set_junction_sides (GtkStyleContext  *context,
                                      GtkJunctionSides  sides)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_css_node_set_junction_sides (context->priv->cssnode, sides);
}

/**
 * gtk_style_context_get_junction_sides:
 * @context: a #GtkStyleContext
 *
 * Returns the sides where rendered elements connect visually with others.
 *
 * Returns: the junction sides
 *
 * Since: 3.0
 **/
GtkJunctionSides
gtk_style_context_get_junction_sides (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  return gtk_css_node_get_junction_sides (context->priv->cssnode);
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

/**
 * gtk_style_context_notify_state_change:
 * @context: a #GtkStyleContext
 * @window: a #GdkWindow
 * @region_id: (allow-none): animatable region to notify on, or %NULL.
 *     See gtk_style_context_push_animatable_region()
 * @state: state to trigger transition for
 * @state_value: %TRUE if @state is the state we are changing to,
 *     %FALSE if we are changing away from it
 *
 * Notifies a state change on @context, so if the current style makes use
 * of transition animations, one will be started so all rendered elements
 * under @region_id are animated for state @state being set to value
 * @state_value.
 *
 * The @window parameter is used in order to invalidate the rendered area
 * as the animation runs, so make sure it is the same window that is being
 * rendered on by the gtk_render_*() functions.
 *
 * If @region_id is %NULL, all rendered elements using @context will be
 * affected by this state transition.
 *
 * As a practical example, a #GtkButton notifying a state transition on
 * the prelight state:
 * |[ <!-- language="C" -->
 * gtk_style_context_notify_state_change (context,
 *                                        gtk_widget_get_window (widget),
 *                                        NULL,
 *                                        GTK_STATE_PRELIGHT,
 *                                        button->in_button);
 * ]|
 *
 * Can be handled in the CSS file like this:
 * |[ <!-- language="CSS" -->
 * button {
 *     background-color: #f00
 * }
 *
 * button:hover {
 *     background-color: #fff;
 *     transition: 200ms linear
 * }
 * ]|
 *
 * This combination will animate the button background from red to white
 * if a pointer enters the button, and back to red if the pointer leaves
 * the button.
 *
 * Note that @state is used when finding the transition parameters, which
 * is why the style places the transition under the :hover pseudo-class.
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: This function does nothing.
 **/
void
gtk_style_context_notify_state_change (GtkStyleContext *context,
                                       GdkWindow       *window,
                                       gpointer         region_id,
                                       GtkStateType     state,
                                       gboolean         state_value)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (state > GTK_STATE_NORMAL && state <= GTK_STATE_FOCUSED);
}

/**
 * gtk_style_context_cancel_animations:
 * @context: a #GtkStyleContext
 * @region_id: (allow-none): animatable region to stop, or %NULL.
 *     See gtk_style_context_push_animatable_region()
 *
 * Stops all running animations for @region_id and all animatable
 * regions underneath.
 *
 * A %NULL @region_id will stop all ongoing animations in @context,
 * when dealing with a #GtkStyleContext obtained through
 * gtk_widget_get_style_context(), this is normally done for you
 * in all circumstances you would expect all widget to be stopped,
 * so this should be only used in complex widgets with different
 * animatable regions.
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: This function does nothing.
 **/
void
gtk_style_context_cancel_animations (GtkStyleContext *context,
                                     gpointer         region_id)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
}

/**
 * gtk_style_context_scroll_animations:
 * @context: a #GtkStyleContext
 * @window: a #GdkWindow used previously in
 *          gtk_style_context_notify_state_change()
 * @dx: Amount to scroll in the X axis
 * @dy: Amount to scroll in the Y axis
 *
 * This function is analogous to gdk_window_scroll(), and
 * should be called together with it so the invalidation
 * areas for any ongoing animation are scrolled together
 * with it.
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: This function does nothing.
 **/
void
gtk_style_context_scroll_animations (GtkStyleContext *context,
                                     GdkWindow       *window,
                                     gint             dx,
                                     gint             dy)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_WINDOW (window));
}

/**
 * gtk_style_context_push_animatable_region:
 * @context: a #GtkStyleContext
 * @region_id: unique identifier for the animatable region
 *
 * Pushes an animatable region, so all further gtk_render_*() calls between
 * this call and the following gtk_style_context_pop_animatable_region()
 * will potentially show transition animations for this region if
 * gtk_style_context_notify_state_change() is called for a given state,
 * and the current theme/style defines transition animations for state
 * changes.
 *
 * The @region_id used must be unique in @context so the themes
 * can uniquely identify rendered elements subject to a state transition.
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: This function does nothing.
 **/
void
gtk_style_context_push_animatable_region (GtkStyleContext *context,
                                          gpointer         region_id)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (region_id != NULL);
}

/**
 * gtk_style_context_pop_animatable_region:
 * @context: a #GtkStyleContext
 *
 * Pops an animatable region from @context.
 * See gtk_style_context_push_animatable_region().
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: This function does nothing.
 **/
void
gtk_style_context_pop_animatable_region (GtkStyleContext *context)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
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

  g_object_set_data (G_OBJECT (context), "font-cache-for-get_font", NULL);

  priv->invalidating_context = NULL;
}

/**
 * gtk_style_context_invalidate:
 * @context: a #GtkStyleContext.
 *
 * Invalidates @context style information, so it will be reconstructed
 * again. It is useful if you modify the @context and need the new
 * information immediately.
 *
 * Since: 3.0
 *
 * Deprecated: 3.12: Style contexts are invalidated automatically.
 **/
void
gtk_style_context_invalidate (GtkStyleContext *context)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_clear_property_cache (context);

  gtk_style_context_validate (context, NULL);
}

/**
 * gtk_style_context_set_background:
 * @context: a #GtkStyleContext
 * @window: a #GdkWindow
 *
 * Sets the background of @window to the background pattern or
 * color specified in @context for its current state.
 *
 * Since: 3.0
 *
 * Deprecated: 3.18: Use gtk_render_background() instead.
 *   Note that clients still using this function are now responsible
 *   for calling this function again whenever @context is invalidated.
 **/
void
gtk_style_context_set_background (GtkStyleContext *context,
                                  GdkWindow       *window)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* This is a sophisticated optimization.
   * If we know the GDK window's background will be opaque, we mark
   * it as opaque. This is so GDK can do all the optimizations it does
   * for opaque windows and be fast.
   * This is mainly used when scrolling.
   *
   * We could indeed just set black instead of the color we have.
   */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (gtk_css_style_render_background_is_opaque (gtk_style_context_lookup_style (context)))
    {
      const GdkRGBA *color;

      color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BACKGROUND_COLOR));

      gdk_window_set_background_rgba (window, color);
    }
  else
    {
      GdkRGBA transparent = { 0.0, 0.0, 0.0, 0.0 };
      gdk_window_set_background_rgba (window, &transparent);
    }
G_GNUC_END_IGNORE_DEPRECATIONS
}

/**
 * gtk_style_context_get_color:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the color for
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
                             GtkStateFlags    state,
                             GdkRGBA         *color)
{
  GdkRGBA *c;

  g_return_if_fail (color != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context,
                         state,
                         "color", &c,
                         NULL);

  *color = *c;
  gdk_rgba_free (c);
}

/**
 * gtk_style_context_get_background_color:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the color for
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
                                        GtkStateFlags    state,
                                        GdkRGBA         *color)
{
  GdkRGBA *c;

  g_return_if_fail (color != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context,
                         state,
                         "background-color", &c,
                         NULL);

  *color = *c;
  gdk_rgba_free (c);
}

/**
 * gtk_style_context_get_border_color:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the color for
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
                                    GtkStateFlags    state,
                                    GdkRGBA         *color)
{
  GdkRGBA *c;

  g_return_if_fail (color != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context,
                         state,
                         "border-color", &c,
                         NULL);

  *color = *c;
  gdk_rgba_free (c);
}

/**
 * gtk_style_context_get_border:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the border for
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
                              GtkStateFlags    state,
                              GtkBorder       *border)
{
  GtkCssStyle *style;
  GtkStateFlags saved_state;
  double top, left, bottom, right;

  g_return_if_fail (border != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  saved_state = gtk_style_context_push_state (context, state);
  style = gtk_style_context_lookup_style (context);

  top = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH), 100));
  right = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH), 100));
  bottom = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH), 100));
  left = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH), 100));

  border->top = top;
  border->left = left;
  border->bottom = bottom;
  border->right = right;

  gtk_style_context_pop_state (context, saved_state);
}

/**
 * gtk_style_context_get_padding:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the padding for
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
                               GtkStateFlags    state,
                               GtkBorder       *padding)
{
  GtkCssStyle *style;
  GtkStateFlags saved_state;
  double top, left, bottom, right;

  g_return_if_fail (padding != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  saved_state = gtk_style_context_push_state (context, state);
  style = gtk_style_context_lookup_style (context);

  top = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_PADDING_TOP), 100));
  right = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_PADDING_RIGHT), 100));
  bottom = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_PADDING_BOTTOM), 100));
  left = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_PADDING_LEFT), 100));

  padding->top = top;
  padding->left = left;
  padding->bottom = bottom;
  padding->right = right;

  gtk_style_context_pop_state (context, saved_state);
}

/**
 * gtk_style_context_get_margin:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the border for
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
                              GtkStateFlags    state,
                              GtkBorder       *margin)
{
  GtkCssStyle *style;
  GtkStateFlags saved_state;
  double top, left, bottom, right;

  g_return_if_fail (margin != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  saved_state = gtk_style_context_push_state (context, state);
  style = gtk_style_context_lookup_style (context);

  top = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MARGIN_TOP), 100));
  right = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MARGIN_RIGHT), 100));
  bottom = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MARGIN_BOTTOM), 100));
  left = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MARGIN_LEFT), 100));

  margin->top = top;
  margin->left = left;
  margin->bottom = bottom;
  margin->right = right;

  gtk_style_context_pop_state (context, saved_state);
}

/**
 * gtk_style_context_get_font:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the font for
 *
 * Returns the font description for a given state. The returned
 * object is const and will remain valid until the
 * #GtkStyleContext::changed signal happens.
 *
 * Returns: (transfer none): the #PangoFontDescription for the given
 *          state.  This object is owned by GTK+ and should not be
 *          freed.
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: Use gtk_style_context_get() for "font" or
 *     subproperties instead.
 **/
const PangoFontDescription *
gtk_style_context_get_font (GtkStyleContext *context,
                            GtkStateFlags    state)
{
  GHashTable *hash;
  PangoFontDescription *description, *previous;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  /* Yuck, fonts are created on-demand but we don't return a ref.
   * Do bad things to achieve this requirement */
  gtk_style_context_get (context, state, "font", &description, NULL);
  
  hash = g_object_get_data (G_OBJECT (context), "font-cache-for-get_font");

  if (hash == NULL)
    {
      hash = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                    NULL,
                                    (GDestroyNotify) pango_font_description_free);
      g_object_set_data_full (G_OBJECT (context),
                              "font-cache-for-get_font",
                              hash,
                              (GDestroyNotify) g_hash_table_unref);
    }

  previous = g_hash_table_lookup (hash, GUINT_TO_POINTER (state));
  if (previous)
    {
      pango_font_description_merge (previous, description, TRUE);
      pango_font_description_free (description);
      description = previous;
    }
  else
    {
      g_hash_table_insert (hash, GUINT_TO_POINTER (state), description);
    }

  return description;
}

void
_gtk_style_context_get_cursor_color (GtkStyleContext *context,
                                     GdkRGBA         *primary_color,
                                     GdkRGBA         *secondary_color)
{
  GdkRGBA *pc, *sc;

  gtk_style_context_get (context,
                         gtk_style_context_get_state (context),
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
                       float            aspect_ratio,
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

  stem_width = height * aspect_ratio + 1;

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
  float aspect_ratio;
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
                "gtk-cursor-aspect-ratio", &aspect_ratio,
                NULL);

  /* Fall back to style property if the GtkSetting property is unchanged */
  if (aspect_ratio == 0.04f)
    {
      gtk_style_context_get_style (context,
                                   "cursor-aspect-ratio", &aspect_ratio,
                                   NULL);
    }

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
                         aspect_ratio,
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
                             aspect_ratio,
                             FALSE,
                             direction2,
                             TRUE);
    }
}

/**
 * gtk_draw_insertion_cursor:
 * @widget:  a #GtkWidget
 * @cr: cairo context to draw to
 * @location: location where to draw the cursor (@location->width is ignored)
 * @is_primary: if the cursor should be the primary cursor color.
 * @direction: whether the cursor is left-to-right or
 *             right-to-left. Should never be #GTK_TEXT_DIR_NONE
 * @draw_arrow: %TRUE to draw a directional arrow on the
 *        cursor. Should be %FALSE unless the cursor is split.
 *
 * Draws a text caret on @cr at @location. This is not a style function
 * but merely a convenience function for drawing the standard cursor shape.
 *
 * Since: 3.0
 * Deprecated: 3.4: Use gtk_render_insertion_cursor() instead.
 */
void
gtk_draw_insertion_cursor (GtkWidget          *widget,
                           cairo_t            *cr,
                           const GdkRectangle *location,
                           gboolean            is_primary,
                           GtkTextDirection    direction,
                           gboolean            draw_arrow)
{
  GtkStyleContext *context;
  float aspect_ratio;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (location != NULL);
  g_return_if_fail (direction != GTK_TEXT_DIR_NONE);

  context = gtk_widget_get_style_context (widget);

  g_object_get (gtk_settings_get_for_screen (context->priv->screen),
                "gtk-cursor-aspect-ratio", &aspect_ratio,
                NULL);

  /* Fall back to style property if the GtkSetting property is unchanged */
  if (aspect_ratio == 0.04f)
    {
      gtk_style_context_get_style (context,
                                   "cursor-aspect-ratio", &aspect_ratio,
                                   NULL);
    }

  draw_insertion_cursor (context, cr,
                         location->x, location->y, location->height,
                         aspect_ratio,
                         is_primary,
                         (direction == GTK_TEXT_DIR_RTL) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR,
                         draw_arrow);
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
                                   GtkStyleContext *context,
                                   GtkStateFlags    flags)
{
  GdkRGBA color;
  gchar *value;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_style_context_get_background_color (context, flags, &color);
G_GNUC_END_IGNORE_DEPRECATIONS
  value = g_strdup_printf ("%u,%u,%u",
                           (guint) ceil (color.red * 65536 - color.red),
                           (guint) ceil (color.green * 65536 - color.green),
                           (guint) ceil (color.blue * 65536 - color.blue));
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_BG_COLOR, value);
  g_free (value);

  gtk_style_context_get_color (context, flags, &color);
  value = g_strdup_printf ("%u,%u,%u",
                           (guint) ceil (color.red * 65536 - color.red),
                           (guint) ceil (color.green * 65536 - color.green),
                           (guint) ceil (color.blue * 65536 - color.blue));
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_FG_COLOR, value);
  g_free (value);

  return attributes;
}

cairo_pattern_t *
gtk_gradient_resolve_for_context (GtkGradient     *gradient,
                                  GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  g_return_val_if_fail (gradient != NULL, NULL);
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return _gtk_gradient_resolve_full (gradient,
                                     GTK_STYLE_PROVIDER_PRIVATE (priv->cascade),
                                     gtk_style_context_lookup_style (context),
                                     priv->parent ? gtk_style_context_lookup_style (priv->parent) : NULL);
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
