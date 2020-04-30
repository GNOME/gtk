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
#include "gtkcsscolorvalueprivate.h"
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
#include "gtkwidgetprivate.h"


/**
 * SECTION:gtkstylecontext
 * @Short_description: Rendering UI elements
 * @Title: GtkStyleContext
 *
 * #GtkStyleContext is an object that stores styling information affecting
 * a widget.
 *
 * In order to construct the final style information, #GtkStyleContext
 * queries information from all attached #GtkStyleProviders. Style providers
 * can be either attached explicitly to the context through
 * gtk_style_context_add_provider(), or to the display through
 * gtk_style_context_add_provider_for_display(). The resulting style is a
 * combination of all providers’ information in priority order.
 *
 * For GTK+ widgets, any #GtkStyleContext returned by
 * gtk_widget_get_style_context() will already have a #GdkDisplay and
 * RTL/LTR information set. The style context will also be updated
 * automatically if any of these settings change on the widget.
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

struct _GtkStyleContextPrivate
{
  GdkDisplay *display;

  guint cascade_changed_id;
  GtkStyleCascade *cascade;
  GtkCssNode *cssnode;
  GSList *saved_nodes;
};
typedef struct _GtkStyleContextPrivate GtkStyleContextPrivate;

enum {
  PROP_0,
  PROP_DISPLAY,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL, };

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
gtk_style_context_class_init (GtkStyleContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_style_context_finalize;
  object_class->set_property = gtk_style_context_impl_set_property;
  object_class->get_property = gtk_style_context_impl_get_property;

  properties[PROP_DISPLAY] =
      g_param_spec_object ("display",
                           P_("Display"),
                           P_("The associated GdkDisplay"),
                           GDK_TYPE_DISPLAY,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gtk_style_context_pop_style_node (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

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
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

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
                                                   "gtk-private-changed",
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
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  priv->display = gdk_display_get_default ();

  if (priv->display == NULL)
    g_error ("Can't create a GtkStyleContext without a display connection");

  gtk_style_context_set_cascade (context,
                                 _gtk_settings_get_style_cascade (gtk_settings_get_for_display (priv->display), 1));
}

static void
gtk_style_context_finalize (GObject *object)
{
  GtkStyleContext *context = GTK_STYLE_CONTEXT (object);
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  while (priv->saved_nodes)
    gtk_style_context_pop_style_node (context);

  gtk_style_context_set_cascade (context, NULL);

  if (priv->cssnode)
    g_object_unref (priv->cssnode);

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
    case PROP_DISPLAY:
      gtk_style_context_set_display (context, g_value_get_object (value));
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
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
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
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  return priv->saved_nodes != NULL;
}

static GtkCssNode *
gtk_style_context_get_root (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  if (priv->saved_nodes != NULL)
    return g_slist_last (priv->saved_nodes)->data;
  else
    return priv->cssnode;
}

GtkStyleProvider *
gtk_style_context_get_style_provider (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  return GTK_STYLE_PROVIDER (priv->cascade);
}

static gboolean
gtk_style_context_has_custom_cascade (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GtkSettings *settings = gtk_settings_get_for_display (priv->display);

  return priv->cascade != _gtk_settings_get_style_cascade (settings, _gtk_style_cascade_get_scale (priv->cascade));
}

GtkCssStyle *
gtk_style_context_lookup_style (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  /* Code will recreate style if it was changed */
  return gtk_css_node_get_style (priv->cssnode);
}

GtkCssNode*
gtk_style_context_get_node (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  return priv->cssnode;
}

GtkStyleContext *
gtk_style_context_new_for_node (GtkCssNode *node)
{
  GtkStyleContext *context;
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_NODE (node), NULL);

  context = g_object_new (GTK_TYPE_STYLE_CONTEXT, NULL);
  priv = gtk_style_context_get_instance_private (context);
  priv->cssnode = g_object_ref (node);

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
 * gtk_style_context_add_provider_for_display().
 *
 * Note: If both priorities are the same, a #GtkStyleProvider
 * added through this function takes precedence over another added
 * through gtk_style_context_add_provider_for_display().
 **/
void
gtk_style_context_add_provider (GtkStyleContext  *context,
                                GtkStyleProvider *provider,
                                guint             priority)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  if (!gtk_style_context_has_custom_cascade (context))
    {
      GtkStyleCascade *new_cascade;

      new_cascade = _gtk_style_cascade_new ();
      _gtk_style_cascade_set_scale (new_cascade, _gtk_style_cascade_get_scale (priv->cascade));
      _gtk_style_cascade_set_parent (new_cascade,
                                     _gtk_settings_get_style_cascade (gtk_settings_get_for_display (priv->display), 1));
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
 **/
void
gtk_style_context_remove_provider (GtkStyleContext  *context,
                                   GtkStyleProvider *provider)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  if (!gtk_style_context_has_custom_cascade (context))
    return;

  _gtk_style_cascade_remove_provider (priv->cascade, provider);
}

/**
 * gtk_style_context_add_provider_for_display:
 * @display: a #GdkDisplay
 * @provider: a #GtkStyleProvider
 * @priority: the priority of the style provider. The lower
 *            it is, the earlier it will be used in the style
 *            construction. Typically this will be in the range
 *            between %GTK_STYLE_PROVIDER_PRIORITY_FALLBACK and
 *            %GTK_STYLE_PROVIDER_PRIORITY_USER
 *
 * Adds a global style provider to @display, which will be used
 * in style construction for all #GtkStyleContexts under @display.
 *
 * GTK+ uses this to make styling information from #GtkSettings
 * available.
 *
 * Note: If both priorities are the same, A #GtkStyleProvider
 * added through gtk_style_context_add_provider() takes precedence
 * over another added through this function.
 **/
void
gtk_style_context_add_provider_for_display (GdkDisplay       *display,
                                            GtkStyleProvider *provider,
                                            guint             priority)
{
  GtkStyleCascade *cascade;

  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));
  g_return_if_fail (!GTK_IS_SETTINGS (provider) || _gtk_settings_get_display (GTK_SETTINGS (provider)) == display);

  cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_display (display), 1);
  _gtk_style_cascade_add_provider (cascade, provider, priority);
}

/**
 * gtk_style_context_remove_provider_for_display:
 * @display: a #GdkDisplay
 * @provider: a #GtkStyleProvider
 *
 * Removes @provider from the global style providers list in @display.
 **/
void
gtk_style_context_remove_provider_for_display (GdkDisplay       *display,
                                               GtkStyleProvider *provider)
{
  GtkStyleCascade *cascade;

  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));
  g_return_if_fail (!GTK_IS_SETTINGS (provider));

  cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_display (display), 1);
  _gtk_style_cascade_remove_provider (cascade, provider);
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
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_css_node_set_id (priv->cssnode, g_quark_from_string (id));
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
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return g_quark_to_string (gtk_css_node_get_id (priv->cssnode));
}

/**
 * gtk_style_context_set_state:
 * @context: a #GtkStyleContext
 * @flags: state to represent
 *
 * Sets the state to be used for style matching.
 **/
void
gtk_style_context_set_state (GtkStyleContext *context,
                             GtkStateFlags    flags)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_css_node_set_state (priv->cssnode, flags);
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
 **/
GtkStateFlags
gtk_style_context_get_state (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  return gtk_css_node_get_state (priv->cssnode);
}

/**
 * gtk_style_context_set_scale:
 * @context: a #GtkStyleContext
 * @scale: scale
 *
 * Sets the scale to use when getting image assets for the style.
 **/
void
gtk_style_context_set_scale (GtkStyleContext *context,
                             gint             scale)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  if (scale == _gtk_style_cascade_get_scale (priv->cascade))
    return;

  if (gtk_style_context_has_custom_cascade (context))
    {
      _gtk_style_cascade_set_scale (priv->cascade, scale);
    }
  else
    {
      GtkStyleCascade *new_cascade;

      new_cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_display (priv->display),
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
 **/
gint
gtk_style_context_get_scale (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  return _gtk_style_cascade_get_scale (priv->cascade);
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
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_CSS_NODE (node));

  priv->saved_nodes = g_slist_prepend (priv->saved_nodes, priv->cssnode);
  priv->cssnode = g_object_ref (node);
}

void
gtk_style_context_save_named (GtkStyleContext *context,
                              const char      *name)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GtkCssNode *cssnode;

  /* Make sure we have the style existing. It is the
   * parent of the new saved node after all.
   */
  if (!gtk_style_context_is_saved (context))
    gtk_style_context_lookup_style (context);

  cssnode = gtk_css_transient_node_new (priv->cssnode);
  gtk_css_node_set_parent (cssnode, gtk_style_context_get_root (context));
  if (name)
    gtk_css_node_set_name (cssnode, g_quark_from_string (name));

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
 **/
void
gtk_style_context_restore (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  if (priv->saved_nodes == NULL)
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
 **/
void
gtk_style_context_add_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GQuark class_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_from_string (class_name);

  gtk_css_node_add_class (priv->cssnode, class_quark);
}

/**
 * gtk_style_context_remove_class:
 * @context: a #GtkStyleContext
 * @class_name: class name to remove
 *
 * Removes @class_name from @context.
 **/
void
gtk_style_context_remove_class (GtkStyleContext *context,
                                const gchar     *class_name)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GQuark class_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_try_string (class_name);
  if (!class_quark)
    return;

  gtk_css_node_remove_class (priv->cssnode, class_quark);
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
 **/
gboolean
gtk_style_context_has_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GQuark class_quark;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (class_name != NULL, FALSE);

  class_quark = g_quark_try_string (class_name);
  if (!class_quark)
    return FALSE;

  return gtk_css_node_has_class (priv->cssnode, class_quark);
}

GtkCssValue *
_gtk_style_context_peek_property (GtkStyleContext *context,
                                  guint            property_id)
{
  GtkCssStyle *values = gtk_style_context_lookup_style (context);

  return gtk_css_style_get_value (values, property_id);
}

/**
 * gtk_style_context_set_display:
 * @context: a #GtkStyleContext
 * @display: a #GdkDisplay
 *
 * Attaches @context to the given display.
 *
 * The display is used to add style information from “global” style
 * providers, such as the display's #GtkSettings instance.
 *
 * If you are using a #GtkStyleContext returned from
 * gtk_widget_get_style_context(), you do not need to
 * call this yourself.
 **/
void
gtk_style_context_set_display (GtkStyleContext *context,
                               GdkDisplay      *display)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GtkStyleCascade *display_cascade;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (priv->display == display)
    return;

  if (gtk_style_context_has_custom_cascade (context))
    {
      display_cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_display (display), 1);
      _gtk_style_cascade_set_parent (priv->cascade, display_cascade);
    }
  else
    {
      display_cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_display (display),
                                                        _gtk_style_cascade_get_scale (priv->cascade));
      gtk_style_context_set_cascade (context, display_cascade);
    }

  priv->display = display;

  g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_DISPLAY]);
}

/**
 * gtk_style_context_get_display:
 * @context: a #GtkStyleContext
 *
 * Returns the #GdkDisplay to which @context is attached.
 *
 * Returns: (transfer none): a #GdkDisplay.
 **/
GdkDisplay *
gtk_style_context_get_display (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return priv->display;
}

static gboolean
gtk_style_context_resolve_color (GtkStyleContext    *context,
                                 GtkCssValue        *color,
                                 GdkRGBA            *result)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GtkCssValue *val;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  val = _gtk_css_color_value_resolve (color,
                                      GTK_STYLE_PROVIDER (priv->cascade),
                                      _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR),
                                      NULL);
  if (val == NULL)
    return FALSE;

  *result = *gtk_css_color_value_get_rgba (val);
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
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GtkCssValue *value;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (color_name != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  value = gtk_style_provider_get_color (GTK_STYLE_PROVIDER (priv->cascade), color_name);
  if (value == NULL)
    return FALSE;

  return gtk_style_context_resolve_color (context, value, color);
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
 **/
void
gtk_style_context_get_color (GtkStyleContext *context,
                             GdkRGBA         *color)
{
  g_return_if_fail (color != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  *color = *gtk_css_color_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));
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
 **/
void
gtk_style_context_get_border (GtkStyleContext *context,
                              GtkBorder       *border)
{
  GtkCssStyle *style;

  g_return_if_fail (border != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  style = gtk_style_context_lookup_style (context);

  border->top = round (_gtk_css_number_value_get (style->border->border_top_width, 100));
  border->right = round (_gtk_css_number_value_get (style->border->border_right_width, 100));
  border->bottom = round (_gtk_css_number_value_get (style->border->border_bottom_width, 100));
  border->left = round (_gtk_css_number_value_get (style->border->border_left_width, 100));
}

/**
 * gtk_style_context_get_padding:
 * @context: a #GtkStyleContext
 * @padding: (out): return value for the padding settings
 *
 * Gets the padding for a given state as a #GtkBorder.
 * See gtk_style_context_get() and #GTK_STYLE_PROPERTY_PADDING
 * for details.
 **/
void
gtk_style_context_get_padding (GtkStyleContext *context,
                               GtkBorder       *padding)
{
  GtkCssStyle *style;

  g_return_if_fail (padding != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  style = gtk_style_context_lookup_style (context);

  padding->top = round (_gtk_css_number_value_get (style->size->padding_top, 100));
  padding->right = round (_gtk_css_number_value_get (style->size->padding_right, 100));
  padding->bottom = round (_gtk_css_number_value_get (style->size->padding_bottom, 100));
  padding->left = round (_gtk_css_number_value_get (style->size->padding_left, 100));
}

/**
 * gtk_style_context_get_margin:
 * @context: a #GtkStyleContext
 * @margin: (out): return value for the margin settings
 *
 * Gets the margin for a given state as a #GtkBorder.
 * See gtk_style_property_get() and #GTK_STYLE_PROPERTY_MARGIN
 * for details.
 **/
void
gtk_style_context_get_margin (GtkStyleContext *context,
                              GtkBorder       *margin)
{
  GtkCssStyle *style;

  g_return_if_fail (margin != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  style = gtk_style_context_lookup_style (context);

  margin->top = round (_gtk_css_number_value_get (style->size->margin_top, 100));
  margin->right = round (_gtk_css_number_value_get (style->size->margin_right, 100));
  margin->bottom = round (_gtk_css_number_value_get (style->size->margin_bottom, 100));
  margin->left = round (_gtk_css_number_value_get (style->size->margin_left, 100));
}

void
_gtk_style_context_get_cursor_color (GtkStyleContext *context,
                                     GdkRGBA         *primary_color,
                                     GdkRGBA         *secondary_color)
{
  GtkCssStyle *style;

  style = gtk_style_context_lookup_style (context);

  if (primary_color)
    *primary_color = *gtk_css_color_value_get_rgba (style->font->caret_color ? style->font->caret_color : style->core->color);

  if (secondary_color)
    *secondary_color = *gtk_css_color_value_get_rgba (style->font->secondary_caret_color ? style->font->secondary_caret_color : style->core->color);
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

static void
get_insertion_cursor_bounds (gdouble          height,
                             float            aspect_ratio,
                             PangoDirection   direction,
                             gboolean         draw_arrow,
                             graphene_rect_t *bounds)
{
  gint stem_width;
  gint offset;

  stem_width = height * aspect_ratio + 1;
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
                           float            aspect_ratio,
                           gboolean         is_primary,
                           PangoDirection   direction,
                           gboolean         draw_arrow)
{
  if (draw_arrow)
    {
      cairo_t *cr;
      graphene_rect_t bounds;

      get_insertion_cursor_bounds (height, aspect_ratio, direction, draw_arrow, &bounds);
      cr = gtk_snapshot_append_cairo (snapshot, &bounds);

      draw_insertion_cursor (context, cr, 0, 0, height, aspect_ratio, is_primary, direction, draw_arrow);

      cairo_destroy (cr);
    }
  else
    {
      GdkRGBA primary_color;
      GdkRGBA secondary_color;
      int stem_width;
      int offset;

      _gtk_style_context_get_cursor_color (context, &primary_color, &secondary_color);

      stem_width = height * aspect_ratio + 1;

      /* put (stem_width % 2) on the proper side of the cursor */
      if (direction == PANGO_DIRECTION_LTR)
        offset = stem_width / 2;
      else
        offset = stem_width - stem_width / 2;

      gtk_snapshot_append_color (snapshot,
                                 is_primary ? &primary_color : &secondary_color,
                                 &GRAPHENE_RECT_INIT (- offset, 0, stem_width, height));
    }
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
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  gboolean split_cursor;
  float aspect_ratio;
  PangoRectangle strong_pos, weak_pos;
  PangoRectangle *cursor1, *cursor2;
  GdkDevice *keyboard;
  PangoDirection keyboard_direction;
  PangoDirection direction2;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (index >= 0);

  g_object_get (gtk_settings_get_for_display (priv->display),
                "gtk-split-cursor", &split_cursor,
                "gtk-cursor-aspect-ratio", &aspect_ratio,
                NULL);

  keyboard = gdk_seat_get_keyboard (gdk_display_get_default_seat (priv->display));
  keyboard_direction = gdk_device_get_direction (keyboard);

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
      if (keyboard_direction == direction)
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
 * gtk_snapshot_render_insertion_cursor:
 * @snapshot: snapshot to render to
 * @context: a #GtkStyleContext
 * @x: X origin
 * @y: Y origin
 * @layout: the #PangoLayout of the text
 * @index: the index in the #PangoLayout
 * @direction: the #PangoDirection of the text
 *
 * Draws a text caret using @snapshot at the specified index of @layout.
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
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  gboolean split_cursor;
  float aspect_ratio;
  PangoRectangle strong_pos, weak_pos;
  PangoRectangle *cursor1, *cursor2;
  GdkDevice *keyboard;
  PangoDirection keyboard_direction;
  PangoDirection direction2;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (index >= 0);

  g_object_get (gtk_settings_get_for_display (priv->display),
                "gtk-split-cursor", &split_cursor,
                "gtk-cursor-aspect-ratio", &aspect_ratio,
                NULL);

  keyboard = gdk_seat_get_keyboard (gdk_display_get_default_seat (priv->display));
  keyboard_direction = gdk_device_get_direction (keyboard);

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
      if (keyboard_direction == direction)
        cursor1 = &strong_pos;
      else
        cursor1 = &weak_pos;
    }

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x + PANGO_PIXELS (cursor1->x), y + PANGO_PIXELS (cursor1->y)));
  snapshot_insertion_cursor (snapshot,
                             context,
                             PANGO_PIXELS (cursor1->height),
                             aspect_ratio,
                             TRUE,
                             direction,
                             direction2 != PANGO_DIRECTION_NEUTRAL);
  gtk_snapshot_restore (snapshot);

  if (direction2 != PANGO_DIRECTION_NEUTRAL)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x + PANGO_PIXELS (cursor2->x), y + PANGO_PIXELS (cursor2->y)));
      snapshot_insertion_cursor (snapshot,
                                 context,
                                 PANGO_PIXELS (cursor2->height),
                                 aspect_ratio,
                                 FALSE,
                                 direction2,
                                 TRUE);
      gtk_snapshot_restore (snapshot);
    }
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
  const GdkRGBA *color; 
  gchar *value;

  color = gtk_css_color_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BACKGROUND_COLOR));
  value = g_strdup_printf ("%u,%u,%u",
                           (guint) ceil (color->red * 65536 - color->red),
                           (guint) ceil (color->green * 65536 - color->green),
                           (guint) ceil (color->blue * 65536 - color->blue));
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_BG_COLOR, value);

  color = gtk_css_color_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));
  value = g_strdup_printf ("%u,%u,%u",
                           (guint) ceil (color->red * 65536 - color->red),
                           (guint) ceil (color->green * 65536 - color->green),
                           (guint) ceil (color->blue * 65536 - color->blue));
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_FG_COLOR, value);

  return attributes;
}

/**
 * GtkStyleContextPrintFlags:
 * @GTK_STYLE_CONTEXT_PRINT_NONE: Default value.
 * @GTK_STYLE_CONTEXT_PRINT_RECURSE: Print the entire tree of
 *     CSS nodes starting at the style context's node
 * @GTK_STYLE_CONTEXT_PRINT_SHOW_STYLE: Show the values of the
 *     CSS properties for each node
 * @GTK_STYLE_CONTEXT_PRINT_SHOW_CHANGE: Show information about 
 *     what changes affect the styles
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
 */
char *
gtk_style_context_to_string (GtkStyleContext           *context,
                             GtkStyleContextPrintFlags  flags)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GString *string;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  string = g_string_new ("");

  gtk_css_node_print (priv->cssnode, flags, string, 0);

  return g_string_free (string, FALSE);
}
