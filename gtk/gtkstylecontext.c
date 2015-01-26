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

#include <gdk/gdk.h>
#include <math.h>
#include <stdlib.h>
#include <gobject/gvaluecollector.h>

#include "gtkstylecontextprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkcssanimatedstyleprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcsscornervalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssimagebuiltinprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssnodedeclarationprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsspathnodeprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcssstaticstyleprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransformvalueprivate.h"
#include "gtkcsstransientnodeprivate.h"
#include "gtkcsswidgetnodeprivate.h"
#include "gtkdebug.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"
#include "gtkstylecascadeprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtksettings.h"
#include "gtksettingsprivate.h"

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
 * #GdkScreen and RTL/LTR information set. The style context will be also
 * updated automatically if any of these settings change on the widget.
 *
 * If you are using the theming layer standalone, you will need to set a
 * widget path and a screen yourself to the created style context through
 * gtk_style_context_set_path() and gtk_style_context_set_screen(), as well
 * as updating the context yourself using gtk_style_context_invalidate()
 * whenever any of the conditions change, such as a change in the
 * #GtkSettings:gtk-theme-name setting or a hierarchy change in the rendered
 * widget.
 *
 * # Style Classes # {#gtkstylecontext-classes}
 *
 * Widgets can add style classes to their context, which can be used
 * to associate different styles by class
 * (see [Selectors][gtkcssprovider-selectors]).
 *
 * # Style Regions
 *
 * Widgets can also add regions with flags to their context. This feature is
 * deprecated and will be removed in a future GTK+ update. Please use style
 * classes instead.
 *
 * The regions used by GTK+ widgets are:
 *
 * ## row
 * Used by #GtkTreeView. Can be used with the flags: `even`, `odd`.
 *
 * ## column
 * Used by #GtkTreeView. Can be used with the flags: `first`, `last`, `sorted`.
 *
 * ## column-header
 * Used by #GtkTreeView.
 *
 * ## tab
 * Used by #GtkNotebook. Can be used with the flags: `even`, `odd`, `first`, `last`.
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

/* When these change we do a full restyling. Otherwise we try to figure out
 * if we need to change things. */
#define GTK_STYLE_CONTEXT_RADICAL_CHANGE (GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_CLASS | GTK_CSS_CHANGE_SOURCE)
/* When these change we don’t clear the cache. This takes more memory but makes
 * things go faster. */
#define GTK_STYLE_CONTEXT_CACHED_CHANGE (GTK_CSS_CHANGE_STATE)

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
  GSList *children;
  GtkCssNode *cssnode;
  GSList *saved_nodes;
  GArray *property_cache;

  guint frame_clock_update_id;
  GdkFrameClock *frame_clock;

  const GtkBitmask *invalidating_context;
  guint animating : 1;
  guint invalid : 1;
};

enum {
  PROP_0,
  PROP_SCREEN,
  PROP_DIRECTION,
  PROP_FRAME_CLOCK,
  PROP_PARENT
};

enum {
  CHANGED,
  LAST_SIGNAL
};

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


static void gtk_style_context_disconnect_update (GtkStyleContext *context);
static void gtk_style_context_connect_update    (GtkStyleContext *context);

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

  signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkStyleContextClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_SCREEN,
                                   g_param_spec_object ("screen",
                                                        P_("Screen"),
                                                        P_("The associated GdkScreen"),
                                                        GDK_TYPE_SCREEN,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_FRAME_CLOCK,
                                   g_param_spec_object ("paint-clock",
                                                        P_("FrameClock"),
                                                        P_("The associated GdkFrameClock"),
                                                        GDK_TYPE_FRAME_CLOCK,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_DIRECTION,
                                   g_param_spec_enum ("direction",
                                                      P_("Direction"),
                                                      P_("Text direction"),
                                                      GTK_TYPE_TEXT_DIRECTION,
                                                      GTK_TEXT_DIR_LTR,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_DEPRECATED));

  /**
   * GtkStyleContext:parent:
   *
   * Sets or gets the style context’s parent. See gtk_style_context_set_parent()
   * for details.
   *
   * Since: 3.4
   */
  g_object_class_install_property (object_class,
                                   PROP_PARENT,
                                   g_param_spec_object ("parent",
                                                        P_("Parent"),
                                                        P_("The parent style context"),
                                                        GTK_TYPE_STYLE_CONTEXT,
                                                        GTK_PARAM_READWRITE));
}

static void
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

static GtkCssStyle *
gtk_css_node_get_parent_style (GtkStyleContext *context,
                               GtkCssNode      *cssnode)
{
  GtkCssNode *parent;

  parent = gtk_css_node_get_parent (cssnode);

  if (parent == NULL)
    return NULL;
  
  return gtk_css_node_get_style (parent);
}

static void
gtk_style_context_pop_style_node (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  g_return_if_fail (priv->saved_nodes != NULL);

  g_object_unref (priv->cssnode);
  priv->cssnode = priv->saved_nodes->data;
  priv->saved_nodes = g_slist_remove (priv->saved_nodes, priv->cssnode);
}

static void
gtk_style_context_cascade_changed (GtkStyleCascade *cascade,
                                   GtkStyleContext *context)
{
  _gtk_style_context_queue_invalidate (context, GTK_CSS_CHANGE_SOURCE);
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
gtk_style_context_init (GtkStyleContext *style_context)
{
  GtkStyleContextPrivate *priv;

  priv = style_context->priv =
    gtk_style_context_get_instance_private (style_context);

  priv->screen = gdk_screen_get_default ();

  priv->property_cache = g_array_new (FALSE, FALSE, sizeof (PropertyValue));

  gtk_style_context_set_cascade (style_context,
                                 _gtk_settings_get_style_cascade (gtk_settings_get_for_screen (priv->screen), 1));

  /* Create default info store */
  priv->cssnode = gtk_css_path_node_new (style_context);
  gtk_css_node_set_state (priv->cssnode, GTK_STATE_FLAG_DIR_LTR);
}

static void
gtk_style_context_update (GdkFrameClock  *clock,
                          GtkStyleContext *context)
{
  _gtk_style_context_queue_invalidate (context, GTK_CSS_CHANGE_ANIMATE);
}

static gboolean
gtk_style_context_is_animating (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  return priv->animating;
}

static void
gtk_style_context_disconnect_update (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  if (priv->frame_clock && priv->frame_clock_update_id)
    {
      g_signal_handler_disconnect (priv->frame_clock,
                                   priv->frame_clock_update_id);
      priv->frame_clock_update_id = 0;
      gdk_frame_clock_end_updating (priv->frame_clock);
    }
}

static void
gtk_style_context_connect_update (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  if (priv->frame_clock && priv->frame_clock_update_id == 0)
    {
      priv->frame_clock_update_id = g_signal_connect (priv->frame_clock,
                                                      "update",
                                                      G_CALLBACK (gtk_style_context_update),
                                                      context);
      gdk_frame_clock_begin_updating (priv->frame_clock);
    }
}

static void
gtk_style_context_stop_animating (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  if (!gtk_style_context_is_animating (context))
    return;

  priv->animating = FALSE;

  gtk_style_context_disconnect_update (context);
}

static void
gtk_style_context_start_animating (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  if (gtk_style_context_is_animating (context))
    return;

  priv->animating = TRUE;

  gtk_style_context_connect_update (context);
}

static gboolean
gtk_style_context_should_animate (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkWidget *widget;
  GtkCssStyle *values;
  gboolean animate;

  priv = context->priv;

  if (!GTK_IS_CSS_WIDGET_NODE (priv->cssnode))
    return FALSE;

  widget = gtk_css_widget_node_get_widget (GTK_CSS_WIDGET_NODE (priv->cssnode));
  if (widget == NULL)
    return FALSE;

  if (!gtk_widget_get_mapped (widget))
    return FALSE;

  values = gtk_style_context_lookup_style (context);
  if (!GTK_IS_CSS_ANIMATED_STYLE (values) ||
      gtk_css_animated_style_is_static (GTK_CSS_ANIMATED_STYLE (values)))
    return FALSE;

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-enable-animations", &animate,
                NULL);

  return animate;
}

void
_gtk_style_context_update_animating (GtkStyleContext *context)
{
  if (gtk_style_context_should_animate (context))
    gtk_style_context_start_animating (context);
  else
    gtk_style_context_stop_animating (context);
}

static void
gtk_style_context_clear_parent (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  if (priv->parent)
    {
      priv->parent->priv->children = g_slist_remove (priv->parent->priv->children, context);
      g_object_unref (priv->parent);
    }
}

static void
gtk_style_context_finalize (GObject *object)
{
  GtkStyleContextPrivate *priv;
  GtkStyleContext *style_context;

  style_context = GTK_STYLE_CONTEXT (object);
  priv = style_context->priv;

  gtk_style_context_stop_animating (style_context);

  /* children hold a reference to us */
  g_assert (priv->children == NULL);

  gtk_style_context_clear_parent (style_context);

  gtk_style_context_set_cascade (style_context, NULL);

  while (priv->saved_nodes)
    gtk_style_context_pop_style_node (style_context);
  g_object_unref (priv->cssnode);

  gtk_style_context_clear_property_cache (style_context);
  g_array_free (priv->property_cache, TRUE);

  G_OBJECT_CLASS (gtk_style_context_parent_class)->finalize (object);
}

static void
gtk_style_context_impl_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkStyleContext *style_context;

  style_context = GTK_STYLE_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      gtk_style_context_set_screen (style_context,
                                    g_value_get_object (value));
      break;
    case PROP_DIRECTION:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_style_context_set_direction (style_context,
                                       g_value_get_enum (value));
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_FRAME_CLOCK:
      gtk_style_context_set_frame_clock (style_context,
                                         g_value_get_object (value));
      break;
    case PROP_PARENT:
      gtk_style_context_set_parent (style_context,
                                    g_value_get_object (value));
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
  GtkStyleContext *style_context;
  GtkStyleContextPrivate *priv;

  style_context = GTK_STYLE_CONTEXT (object);
  priv = style_context->priv;

  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, priv->screen);
      break;
    case PROP_DIRECTION:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_value_set_enum (value, gtk_style_context_get_direction (style_context));
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
  GtkStyleContextPrivate *priv;

  priv = context->priv;

  if (priv->saved_nodes != NULL)
    return g_slist_last (priv->saved_nodes)->data;
  else
    return priv->cssnode;
}

static gboolean
gtk_style_context_has_custom_cascade (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;
  GtkSettings *settings = gtk_settings_get_for_screen (context->priv->screen);

  return priv->cascade != _gtk_settings_get_style_cascade (settings, _gtk_style_cascade_get_scale (priv->cascade));
}

static gboolean
may_use_global_parent_cache (GtkStyleContext *context)
{
  if (gtk_style_context_has_custom_cascade (context))
    return FALSE;

  return TRUE;
}

static GtkCssStyle *
lookup_in_global_parent_cache (GtkStyleContext             *context,
                               GtkCssStyle                 *parent,
                               const GtkCssNodeDeclaration *decl)
{
  GHashTable *cache;
  GtkCssStyle *style;

  if (parent == NULL ||
      !may_use_global_parent_cache (context))
    return NULL;

  cache = g_object_get_data (G_OBJECT (parent), "gtk-global-cache");
  if (cache == NULL)
    return NULL;

  style = g_hash_table_lookup (cache, decl);

  return style;
}

static gboolean
may_be_stored_in_parent_cache (GtkCssStyle *style)
{
  GtkCssChange change;

  change = gtk_css_static_style_get_change (GTK_CSS_STATIC_STYLE (style));

  /* The cache is shared between all children of the parent, so if a
   * style depends on a sibling it is not independant of the child.
   */
  if (change & GTK_CSS_CHANGE_ANY_SIBLING)
    return FALSE;

  /* Again, the cache is shared between all children of the parent.
   * If the position is relevant, no child has the same style.
   */
  if (change & GTK_CSS_CHANGE_POSITION)
    return FALSE;

  return TRUE;
}

static void
store_in_global_parent_cache (GtkStyleContext             *context,
                              GtkCssStyle                 *parent,
                              const GtkCssNodeDeclaration *decl,
                              GtkCssStyle                 *style)
{
  GHashTable *cache;

  g_assert (GTK_IS_CSS_STATIC_STYLE (style));

  if (parent == NULL ||
      !may_use_global_parent_cache (context))
    return;

  if (!may_be_stored_in_parent_cache (style))
    return;

  cache = g_object_get_data (G_OBJECT (parent), "gtk-global-cache");
  if (cache == NULL)
    {
      cache = g_hash_table_new_full (gtk_css_node_declaration_hash,
                                     gtk_css_node_declaration_equal,
                                     (GDestroyNotify) gtk_css_node_declaration_unref,
                                     g_object_unref);
      g_object_set_data_full (G_OBJECT (parent), "gtk-global-cache", cache, (GDestroyNotify) g_hash_table_destroy);
    }

  g_hash_table_insert (cache,
                       gtk_css_node_declaration_ref ((GtkCssNodeDeclaration *) decl),
                       g_object_ref (style));
}

static GtkCssStyle *
update_properties (GtkStyleContext             *context,
                   GtkCssNode                  *cssnode,
                   GtkCssStyle                 *style,
                   const GtkBitmask            *parent_changes)
{
  GtkStyleContextPrivate *priv;
  const GtkCssNodeDeclaration *decl;
  GtkCssMatcher matcher;
  GtkWidgetPath *path;
  GtkCssStyle *parent;
  GtkCssStyle *result;

  priv = context->priv;
  parent = gtk_css_node_get_parent_style (context, cssnode);
  decl = gtk_css_node_get_declaration (cssnode);

  result = lookup_in_global_parent_cache (context, parent, decl);
  if (result)
    return g_object_ref (result);

  path = gtk_css_node_create_widget_path (cssnode);

  if (!_gtk_css_matcher_init (&matcher, path))
    {
      g_assert_not_reached ();
    }

  result = gtk_css_static_style_new_update (GTK_CSS_STATIC_STYLE (style),
                                            parent_changes,
                                            GTK_STYLE_PROVIDER_PRIVATE (priv->cascade),
                                            &matcher,
                                            parent);

  gtk_widget_path_free (path);

  store_in_global_parent_cache (context, parent, decl, result);

  return result;
}

static GtkCssStyle *
build_properties (GtkStyleContext             *context,
                  GtkCssNode                  *cssnode,
                  gboolean                     override_state,
                  GtkStateFlags                state)
{
  GtkStyleContextPrivate *priv;
  const GtkCssNodeDeclaration *decl;
  GtkCssMatcher matcher;
  GtkWidgetPath *path;
  GtkCssStyle *parent;
  GtkCssStyle *style;

  priv = context->priv;
  decl = gtk_css_node_get_declaration (cssnode);
  parent = gtk_css_node_get_parent_style (context, cssnode);

  style = lookup_in_global_parent_cache (context, parent, decl);
  if (style)
    return g_object_ref (style);

  path = gtk_css_node_create_widget_path (cssnode);
  if (override_state)
    gtk_widget_path_iter_set_state (path, -1, state);

  if (_gtk_css_matcher_init (&matcher, path))
    style = gtk_css_static_style_new_compute (GTK_STYLE_PROVIDER_PRIVATE (priv->cascade),
                                              &matcher,
                                              parent);
  else
    style = gtk_css_static_style_new_compute (GTK_STYLE_PROVIDER_PRIVATE (priv->cascade),
                                              NULL,
                                              parent);

  gtk_widget_path_free (path);

  store_in_global_parent_cache (context, parent, decl, style);

  return style;
}

GtkCssStyle *
gtk_style_context_lookup_style (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkCssStyle *values;
  GtkCssNode *cssnode;

  priv = context->priv;
  cssnode = priv->cssnode;

  /* Current data in use is cached, just return it */
  values = gtk_css_node_get_style (cssnode);
  if (values)
    return values;

  values = build_properties (context, cssnode, FALSE, 0);
  
  gtk_css_node_set_style (cssnode, values);
  g_object_unref (values);

  return values;
}

static GtkCssStyle *
gtk_style_context_lookup_style_for_state (GtkStyleContext *context,
                                          GtkStateFlags    state)
{
  GtkCssNodeDeclaration *decl;
  GtkCssStyle *values;

  if (gtk_css_node_get_state (context->priv->cssnode) == state)
    return g_object_ref (gtk_style_context_lookup_style (context));

  if (g_getenv ("GTK_STYLE_CONTEXT_WARNING"))
    g_warning ("State does not match current state");

  decl = gtk_css_node_dup_declaration (context->priv->cssnode);
  gtk_css_node_declaration_set_state (&decl, state);
  values = build_properties (context,
                             context->priv->cssnode,
                             TRUE, state);
  gtk_css_node_declaration_unref (decl);

  return values;
}

void
gtk_style_context_set_invalid (GtkStyleContext *context,
                               gboolean         invalid)
{
  GtkStyleContextPrivate *priv;
  
  priv = context->priv;

  if (priv->invalid == invalid)
    return;

  priv->invalid = invalid;

  if (invalid)
    {
      GtkWidget *widget;

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      if (GTK_IS_CSS_WIDGET_NODE (priv->cssnode) &&
          GTK_IS_RESIZE_CONTAINER (widget = gtk_css_widget_node_get_widget (GTK_CSS_WIDGET_NODE (priv->cssnode))))
        _gtk_container_queue_restyle (GTK_CONTAINER (widget));
      else if (priv->parent)
        gtk_style_context_set_invalid (priv->parent, TRUE);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
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

void
_gtk_style_context_set_widget (GtkStyleContext *context,
                               GtkWidget       *widget)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));
  g_return_if_fail (!gtk_style_context_is_saved (context));

  priv = context->priv;

  if (!GTK_IS_CSS_WIDGET_NODE (priv->cssnode))
    {
      g_object_unref (priv->cssnode);
      priv->cssnode = gtk_css_widget_node_new (widget);
      gtk_css_node_set_state (priv->cssnode, GTK_STATE_FLAG_DIR_LTR);
      gtk_css_node_set_style (priv->cssnode, gtk_css_static_style_get_default ());
    }

  if (widget)
    {
      gtk_css_node_set_widget_type (priv->cssnode, G_OBJECT_TYPE (widget));
    }
  else
    {
      gtk_css_node_set_widget_type (priv->cssnode, G_TYPE_NONE);
      gtk_css_widget_node_widget_destroyed (GTK_CSS_WIDGET_NODE (priv->cssnode));
    }

  _gtk_style_context_update_animating (context);

  _gtk_style_context_queue_invalidate (context, GTK_CSS_CHANGE_ANY_SELF);
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
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  priv = context->priv;

  if (!gtk_style_context_has_custom_cascade (context))
    return;

  _gtk_style_cascade_remove_provider (priv->cascade, provider);
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

  _gtk_icon_set_invalidate_caches ();

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
 * Returns: %NULL or the section where value was defined
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
  GtkCssStyle *values;
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

  values = gtk_style_context_lookup_style_for_state (context, state);
  _gtk_style_property_query (prop, value, gtk_style_context_query_func, values);
  g_object_unref (values);
}

/**
 * gtk_style_context_get_valist:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the property values for
 * @args: va_list of property name/return location pairs, followed by %NULL
 *
 * Retrieves several style property values from @context for a given state.
 *
 * Since: 3.0
 **/
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
 * @...: property name /return value pairs, followed by %NULL
 *
 * Retrieves several style property values from @context for a
 * given state.
 *
 * Since: 3.0
 **/
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
 * Sets the CSS ID to be used when rendering with any
 * of the gtk_render_*() functions.
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
 * Returns the CSS ID used when rendering.
 *
 * Returns: the ID or %NULL if no ID is set.
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
 * Sets the state to be used when rendering with any
 * of the gtk_render_*() functions.
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
    g_object_notify (G_OBJECT (context), "direction");
}

/**
 * gtk_style_context_get_state:
 * @context: a #GtkStyleContext
 *
 * Returns the state used when rendering.
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
 * Sets the scale to use when getting image assets for the style .
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

  if (path)
    {
      GtkWidgetPath *copy = gtk_widget_path_copy (path);
      gtk_css_path_node_set_widget_path (GTK_CSS_PATH_NODE (root), copy);
      if (gtk_widget_path_length (copy))
        gtk_css_node_set_widget_type (root,
                                      gtk_widget_path_iter_get_object_type (copy, -1));
      gtk_widget_path_unref (copy);
    }
  else
    {
      gtk_css_path_node_set_widget_path (GTK_CSS_PATH_NODE (root), NULL);
      gtk_css_node_set_widget_type (root, G_TYPE_NONE);
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
      parent->priv->children = g_slist_prepend (parent->priv->children, context);
      g_object_ref (parent);
      if (priv->invalid)
        gtk_style_context_set_invalid (parent, TRUE);
      gtk_css_node_set_parent (gtk_style_context_get_root (context),
                               gtk_style_context_get_root (parent));
    }
  else
    {
      gtk_css_node_set_parent (gtk_style_context_get_root (context), NULL);
    }

  gtk_style_context_clear_parent (context);

  priv->parent = parent;

  g_object_notify (G_OBJECT (context), "parent");
  _gtk_style_context_queue_invalidate (context, GTK_CSS_CHANGE_ANY_PARENT | GTK_CSS_CHANGE_ANY_SIBLING);
}

/**
 * gtk_style_context_get_parent:
 * @context: a #GtkStyleContext
 *
 * Gets the parent context set via gtk_style_context_set_parent().
 * See that function for details.
 *
 * Returns: (transfer none): the parent context or %NULL
 *
 * Since: 3.4
 **/
GtkStyleContext *
gtk_style_context_get_parent (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return context->priv->parent;
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
  GtkStyleContextPrivate *priv;
  GtkCssNode *cssnode;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  /* Make sure we have the style existing. It is the
   * parent of the new saved node after all. */
  if (!gtk_style_context_is_saved (context))
    gtk_style_context_lookup_style (context);

  cssnode = gtk_css_transient_node_new (priv->cssnode);
  gtk_css_node_set_parent (cssnode, gtk_style_context_get_root (context));
  gtk_css_node_set_widget_type (cssnode, gtk_css_node_get_widget_type (priv->cssnode));

  priv->saved_nodes = g_slist_prepend (priv->saved_nodes, priv->cssnode);
  priv->cssnode = cssnode;
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
 * In the CSS file format, a #GtkEntry defining an “entry”
 * class, would be matched by:
 *
 * |[
 * GtkEntry.entry { ... }
 * ]|
 *
 * While any widget defining an “entry” class would be
 * matched by:
 * |[
 * .entry { ... }
 * ]|
 *
 * Since: 3.0
 **/
void
gtk_style_context_add_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GQuark class_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  priv = context->priv;
  class_quark = g_quark_from_string (class_name);

  gtk_css_node_add_class (priv->cssnode, class_quark);
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
  GtkStyleContextPrivate *priv;
  GQuark class_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return;

  priv = context->priv;

  gtk_css_node_remove_class (priv->cssnode, class_quark);
}

/**
 * gtk_style_context_has_class:
 * @context: a #GtkStyleContext
 * @class_name: a class name
 *
 * Returns %TRUE if @context currently has defined the
 * given class name
 *
 * Returns: %TRUE if @context has @class_name defined
 *
 * Since: 3.0
 **/
gboolean
gtk_style_context_has_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GQuark class_quark;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (class_name != NULL, FALSE);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return FALSE;

  priv = context->priv;

  return gtk_css_node_has_class (priv->cssnode, class_quark);
}

static void
quarks_to_strings (GList *list)
{
  GList *l;

  for (l = list; l; l = l->next)
    {
      l->data = (char *) g_quark_to_string (GPOINTER_TO_UINT (l->data));
    }
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
  GtkStyleContextPrivate *priv;
  GList *classes;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;
  
  classes = gtk_css_node_list_classes (priv->cssnode);
  quarks_to_strings (classes);

  return classes;
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
  GtkStyleContextPrivate *priv;
  GList *regions;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;

  regions = gtk_css_node_list_regions (priv->cssnode);
  quarks_to_strings (regions);

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
 * |[
 * GtkTreeView row { ... }
 * ]|
 *
 * Pseudo-classes are used for matching @flags, so the two
 * following rules:
 * |[
 * GtkTreeView row:nth-child(even) { ... }
 * GtkTreeView row:nth-child(odd) { ... }
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
  GtkStyleContextPrivate *priv;
  GQuark region_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (region_name != NULL);
  g_return_if_fail (_gtk_style_context_check_region_name (region_name));

  priv = context->priv;
  region_quark = g_quark_from_string (region_name);

  gtk_css_node_add_region (priv->cssnode, region_quark, flags);
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
  GtkStyleContextPrivate *priv;
  GQuark region_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (region_name != NULL);

  region_quark = g_quark_try_string (region_name);

  if (!region_quark)
    return;

  priv = context->priv;

  gtk_css_node_remove_region (priv->cssnode, region_quark);
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
  GtkStyleContextPrivate *priv;
  GQuark region_quark;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (region_name != NULL, FALSE);

  if (flags_return)
    *flags_return = 0;

  region_quark = g_quark_try_string (region_name);

  if (!region_quark)
    return FALSE;

  priv = context->priv;

  return gtk_css_node_has_region (priv->cssnode, region_quark, flags_return);
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

              if (_gtk_style_context_resolve_color (context, _gtk_symbolic_color_get_css_value (color), &rgba, NULL))
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
          g_warning ("%s: can't get style properties for non-widget class `%s'",
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
      g_warning ("%s: widget class `%s' has no style property named `%s'",
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
    g_warning ("can't retrieve style property `%s' of type `%s' as value of type `%s'",
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
          g_warning ("%s: can't get style properties for non-widget class `%s'",
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
          g_warning ("%s: widget class `%s' has no style property named `%s'",
                     G_STRLOC,
                     g_type_name (widget_type),
                     prop_name);
          break;
        }

      peek_value = _gtk_style_context_peek_style_property (context, widget_type, pspec);

      G_VALUE_LCOPY (peek_value, args, 0, &error);

      if (error)
        {
          g_warning ("can't retrieve style property `%s' of type `%s': %s",
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

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

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
 * Returns: (transfer none): The looked  up %GtkIconSet, or %NULL
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

  g_object_notify (G_OBJECT (context), "screen");
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
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;
  return priv->screen;
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
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (frame_clock == NULL || GDK_IS_FRAME_CLOCK (frame_clock));

  priv = context->priv;
  if (priv->frame_clock == frame_clock)
    return;

  if (priv->animating)
    gtk_style_context_disconnect_update (context);

  if (priv->frame_clock)
    g_object_unref (priv->frame_clock);
  priv->frame_clock = frame_clock;
  if (priv->frame_clock)
    g_object_ref (priv->frame_clock);

  if (priv->animating)
    gtk_style_context_connect_update (context);

  g_object_notify (G_OBJECT (context), "paint-clock");
}

/**
 * gtk_style_context_get_frame_clock:
 * @context: a #GtkStyleContext
 *
 * Returns the #GdkFrameClock to which @context is attached.
 *
 * Returns: (transfer none): a #GdkFrameClock, or %NULL
 *  if @context does not have an attached frame clock.
 *
 * Since: 3.8
 **/
GdkFrameClock *
gtk_style_context_get_frame_clock (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;
  return priv->frame_clock;
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
                                  GdkRGBA            *result,
                                  GtkCssDependencies *dependencies)
{
  GtkCssValue *val;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  val = _gtk_css_color_value_resolve (color,
                                      GTK_STYLE_PROVIDER_PRIVATE (context->priv->cascade),
                                      _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR),
                                      GTK_CSS_DEPENDS_ON_COLOR,
                                      dependencies,
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

  return _gtk_style_context_resolve_color (context, value, color, NULL);
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
 * |[<!-- language="C" -->
 * gtk_style_context_notify_state_change (context,
 *                                        gtk_widget_get_window (widget),
 *                                        NULL,
 *                                        GTK_STATE_PRELIGHT,
 *                                        button->in_button);
 * ]|
 *
 * Can be handled in the CSS file like this:
 * |[
 * GtkButton {
 *     background-color: #f00
 * }
 *
 * GtkButton:hover {
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

static void
gtk_style_context_clear_cache (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GSList *l;

  priv = context->priv;

  for (l = priv->saved_nodes; l; l = l->next)
    {
      gtk_css_node_set_style (l->data, NULL);
    }

  gtk_style_context_clear_property_cache (context);
}

static void
gtk_style_context_do_invalidate (GtkStyleContext  *context,
                                 const GtkBitmask *changes)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  /* Avoid reentrancy */
  if (priv->invalidating_context)
    return;

  priv->invalidating_context = changes;

  g_signal_emit (context, signals[CHANGED], 0);

  g_object_set_data (G_OBJECT (context), "font-cache-for-get_font", NULL);

  priv->invalidating_context = NULL;
}

static gboolean
gtk_style_context_style_needs_full_revalidate (GtkCssStyle  *style,
                                               GtkCssChange  change)
{
  /* Try to avoid invalidating if we can */
  if (change & GTK_STYLE_CONTEXT_RADICAL_CHANGE)
    return TRUE;

  if (GTK_IS_CSS_ANIMATED_STYLE (style))
    style = GTK_CSS_ANIMATED_STYLE (style)->style;

  if (gtk_css_static_style_get_change (GTK_CSS_STATIC_STYLE (style)) & change)
    return TRUE;
  else
    return FALSE;
}

static gboolean
gtk_style_context_should_create_transitions (GtkStyleContext *context,
                                             GtkCssStyle     *previous_style)
{
  GtkStyleContextPrivate *priv;
  GtkWidget *widget;
  gboolean animate;

  priv = context->priv;

  if (GTK_IS_CSS_WIDGET_NODE (priv->cssnode))
    return FALSE;

  widget = gtk_css_widget_node_get_widget (GTK_CSS_WIDGET_NODE (priv->cssnode));
  if (widget == NULL)
    return FALSE;

  if (!gtk_widget_get_mapped (widget))
    return FALSE;

  if (previous_style == gtk_css_static_style_get_default ())
    return FALSE;

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-enable-animations", &animate,
                NULL);

  return animate;
}

void
_gtk_style_context_validate (GtkStyleContext  *context,
                             gint64            timestamp,
                             GtkCssChange      change,
                             const GtkBitmask *parent_changes)
{
  GtkStyleContextPrivate *priv;
  GtkCssNode *cssnode;
  GtkCssStyle *current;
  GtkBitmask *changes;
  GSList *list;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  if (G_UNLIKELY (gtk_style_context_is_saved (context)))
    {
      cssnode = gtk_style_context_get_root (context);
      if (GTK_IS_CSS_WIDGET_NODE (cssnode))
        {
          GtkWidget *widget = gtk_css_widget_node_get_widget (GTK_CSS_WIDGET_NODE (cssnode));
          g_warning ("unmatched gtk_style_context_save/restore() detected while validating context for %s %p",
                     gtk_widget_get_name (widget), widget);
        }
      else
        {
          g_warning ("unmatched gtk_style_context_save/restore() detected while validating context");
        }
    }
  else
    cssnode = priv->cssnode;

  if (GTK_IS_CSS_WIDGET_NODE (cssnode))
    change |= gtk_css_widget_node_reset_change (GTK_CSS_WIDGET_NODE (cssnode));
  
  /* If you run your application with
   *   GTK_DEBUG=no-css-cache
   * every invalidation will purge the cache and completely query
   * everything anew form the cache. This is slow (in particular
   * when animating), but useful for figuring out bugs.
   *
   * We achieve that by pretending that everything that could have
   * changed has and so we of course totally need to redo everything.
   *
   * Note that this also completely revalidates child widgets all
   * the time.
   */
  if (G_UNLIKELY (gtk_get_debug_flags () & GTK_DEBUG_NO_CSS_CACHE))
    change = GTK_CSS_CHANGE_ANY;

  if (!priv->invalid && change == 0 && _gtk_bitmask_is_empty (parent_changes))
    return;

  gtk_style_context_set_invalid (context, FALSE);

  current = gtk_css_node_get_style (cssnode);
  if (current == NULL)
    current = gtk_css_static_style_get_default ();
  g_object_ref (current);

  /* Try to avoid invalidating if we can */
  if (gtk_style_context_style_needs_full_revalidate (current, change))
    {
      GtkCssStyle *style, *static_style;

      static_style = build_properties (context, cssnode, FALSE, 0);
      style = gtk_css_animated_style_new (static_style,
                                          priv->parent ? gtk_style_context_lookup_style (priv->parent) : NULL,
                                          timestamp,
                                          GTK_STYLE_PROVIDER_PRIVATE (priv->cascade),
                                          gtk_style_context_should_create_transitions (context, current) ? current : NULL);
  
      gtk_style_context_clear_cache (context);

      gtk_css_node_set_style (cssnode, style);

      g_object_unref (static_style);
      g_object_unref (style);
    }
  else
    {
      if (!_gtk_bitmask_is_empty (parent_changes))
        {
          GtkCssStyle *new_values;

          if (GTK_IS_CSS_ANIMATED_STYLE (current))
            {
	      GtkCssStyle *new_base;
              
              new_base = update_properties (context,
                                            cssnode,
                                            GTK_CSS_ANIMATED_STYLE (current)->style,
                                            parent_changes);
              new_values = gtk_css_animated_style_new_advance (GTK_CSS_ANIMATED_STYLE (current),
                                                               new_base,
                                                               timestamp);
              g_object_unref (new_base);
            }
          else
            {
	      new_values = update_properties (context,
                                              cssnode,
                                              current,
                                              parent_changes);
            }

          gtk_css_node_set_style (cssnode, new_values);
          g_object_unref (new_values);
        }
      else if (change & GTK_CSS_CHANGE_ANIMATE &&
          gtk_style_context_is_animating (context))
        {
          GtkCssStyle *new_values;

          new_values = gtk_css_animated_style_new_advance (GTK_CSS_ANIMATED_STYLE (current),
                                                           GTK_CSS_ANIMATED_STYLE (current)->style,
                                                           timestamp);
          gtk_css_node_set_style (cssnode, new_values);
          g_object_unref (new_values);
        }
    }

  _gtk_style_context_update_animating (context);

  changes = gtk_css_style_get_difference (gtk_css_node_get_style (cssnode), current);
  g_object_unref (current);

  if (!_gtk_bitmask_is_empty (changes))
    gtk_style_context_do_invalidate (context, changes);

  change = _gtk_css_change_for_child (change);
  
  gtk_style_context_clear_property_cache (context);

  for (list = priv->children; list; list = list->next)
    {
      _gtk_style_context_validate (list->data, timestamp, change, changes);
    }

  _gtk_bitmask_free (changes);
}

void
_gtk_style_context_queue_invalidate (GtkStyleContext *context,
                                     GtkCssChange     change)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (change != 0);

  gtk_css_node_invalidate (gtk_style_context_get_root (context), change);
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
  GtkBitmask *changes;
  GtkCssStyle *style;
  GtkCssNode *root;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_clear_cache (context);
  gtk_css_node_set_style (context->priv->cssnode, NULL);

  root = gtk_style_context_get_root (context);
  style = build_properties (context,
                            root,
                            FALSE,
                            0);
  gtk_css_node_set_style (root, style);
  g_object_unref (style);

  if (!gtk_style_context_is_saved (context))
    _gtk_style_context_update_animating (context);

  changes = _gtk_bitmask_new ();
  changes = _gtk_bitmask_invert_range (changes,
                                       0,
                                       _gtk_css_style_property_get_n_properties ());
  gtk_style_context_do_invalidate (context, changes);
  _gtk_bitmask_free (changes);
}

static gboolean
corner_value_is_right_angle (GtkCssValue *value)
{
  return _gtk_css_corner_value_get_x (value, 100) <= 0.0 &&
         _gtk_css_corner_value_get_y (value, 100) <= 0.0;
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
 **/
void
gtk_style_context_set_background (GtkStyleContext *context,
                                  GdkWindow       *window)
{
  const GdkRGBA *color;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* This is a sophisitcated optimization.
   * If we know the GDK window's background will be opaque, we mark
   * it as opaque. This is so GDK can do all the optimizations it does
   * for opaque windows and be fast.
   * This is mainly used when scrolling.
   *
   * We could indeed just set black instead of the color we have.
   */
  color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BACKGROUND_COLOR));

  if (color->alpha >= 1.0 &&
      corner_value_is_right_angle (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS)) &&
      corner_value_is_right_angle (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS)) &&
      corner_value_is_right_angle (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS)) &&
      corner_value_is_right_angle (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS)))
    {
      gdk_window_set_background_rgba (window, color);
    }
  else
    {
      GdkRGBA transparent = { 0.0, 0.0, 0.0, 0.0 };
      gdk_window_set_background_rgba (window, &transparent);
    }
}

/**
 * gtk_style_context_get_color:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the color for
 * @color: (out): return value for the foreground color
 *
 * Gets the foreground color for a given state.
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
 * Deprecated: 3.16: Use gtk_render_border() instead.
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
 * See %GTK_STYLE_PROPERTY_BORDER_WIDTH.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_border (GtkStyleContext *context,
                              GtkStateFlags    state,
                              GtkBorder       *border)
{
  GtkCssStyle *style;
  double top, left, bottom, right;

  g_return_if_fail (border != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  style = gtk_style_context_lookup_style_for_state (context, state);

  top = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH), 100));
  right = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH), 100));
  bottom = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH), 100));
  left = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH), 100));

  border->top = top;
  border->left = left;
  border->bottom = bottom;
  border->right = right;

  g_object_unref (style);
}

/**
 * gtk_style_context_get_padding:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the padding for
 * @padding: (out): return value for the padding settings
 *
 * Gets the padding for a given state as a #GtkBorder.
 * See %GTK_STYLE_PROPERTY_PADDING.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_padding (GtkStyleContext *context,
                               GtkStateFlags    state,
                               GtkBorder       *padding)
{
  GtkCssStyle *style;
  double top, left, bottom, right;

  g_return_if_fail (padding != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  style = gtk_style_context_lookup_style_for_state (context, state);

  top = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_PADDING_TOP), 100));
  right = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_PADDING_RIGHT), 100));
  bottom = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_PADDING_BOTTOM), 100));
  left = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_PADDING_LEFT), 100));

  padding->top = top;
  padding->left = left;
  padding->bottom = bottom;
  padding->right = right;

  g_object_unref (style);
}

/**
 * gtk_style_context_get_margin:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the border for
 * @margin: (out): return value for the margin settings
 *
 * Gets the margin for a given state as a #GtkBorder.
 * See %GTK_STYLE_PROPERTY_MARGIN.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_margin (GtkStyleContext *context,
                              GtkStateFlags    state,
                              GtkBorder       *margin)
{
  GtkCssStyle *style;
  double top, left, bottom, right;

  g_return_if_fail (margin != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  style = gtk_style_context_lookup_style_for_state (context, state);

  top = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MARGIN_TOP), 100));
  right = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MARGIN_RIGHT), 100));
  bottom = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MARGIN_BOTTOM), 100));
  left = round (_gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MARGIN_LEFT), 100));

  margin->top = top;
  margin->left = left;
  margin->bottom = bottom;
  margin->right = right;

  g_object_unref (style);
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

static void
get_cursor_color (GtkStyleContext *context,
                  gboolean         primary,
                  GdkRGBA         *color)
{
  GdkColor *style_color;

  gtk_style_context_get_style (context,
                               primary ? "cursor-color" : "secondary-cursor-color",
                               &style_color,
                               NULL);

  if (style_color)
    {
      color->red = style_color->red / 65535.0;
      color->green = style_color->green / 65535.0;
      color->blue = style_color->blue / 65535.0;
      color->alpha = 1;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gdk_color_free (style_color);
G_GNUC_END_IGNORE_DEPRECATIONS
    }
  else
    {
      GtkStateFlags state;

      state = gtk_style_context_get_state (context);

      gtk_style_context_get_color (context, state, color);

      if (!primary)
      {
        GdkRGBA bg;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_style_context_get_background_color (context, state, &bg);
G_GNUC_END_IGNORE_DEPRECATIONS

        color->red = (color->red + bg.red) * 0.5;
        color->green = (color->green + bg.green) * 0.5;
        color->blue = (color->blue + bg.blue) * 0.5;
      }
    }
}

void
_gtk_style_context_get_cursor_color (GtkStyleContext *context,
                                     GdkRGBA         *primary_color,
                                     GdkRGBA         *secondary_color)
{
  if (primary_color)
    get_cursor_color (context, TRUE, primary_color);

  if (secondary_color)
    get_cursor_color (context, FALSE, secondary_color);
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
  gfloat cursor_aspect_ratio;
  gint stem_width;
  gint offset;

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_style_context_get_cursor_color (context, &primary_color, &secondary_color);
  gdk_cairo_set_source_rgba (cr, is_primary ? &primary_color : &secondary_color);

  /* When changing the shape or size of the cursor here,
   * propagate the changes to gtktextview.c:text_window_invalidate_cursors().
   */

  gtk_style_context_get_style (context,
                               "cursor-aspect-ratio", &cursor_aspect_ratio,
                               NULL);

  stem_width = height * cursor_aspect_ratio + 1;

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

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (location != NULL);
  g_return_if_fail (direction != GTK_TEXT_DIR_NONE);

  context = gtk_widget_get_style_context (widget);

  draw_insertion_cursor (context, cr,
                         location->x, location->y, location->height,
                         is_primary,
                         (direction == GTK_TEXT_DIR_RTL) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR,
                         draw_arrow);
}

/**
 * _gtk_style_context_get_changes:
 * @context: the context to query
 *
 * Queries the context for the changes for the currently executing
 * GtkStyleContext::invalidate signal. If no signal is currently
 * emitted, this function returns %NULL.
 *
 * FIXME 4.0: Make this part of the signal.
 *
 * Returns: %NULL or the currently invalidating changes
 **/
const GtkBitmask *
_gtk_style_context_get_changes (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return context->priv->invalidating_context;
}

static void
gtk_cairo_rectangle_transform (cairo_rectangle_int_t       *dest,
                               const cairo_rectangle_int_t *src,
                               const cairo_matrix_t        *matrix)
{
  double x1, x2, x3, x4;
  double y1, y2, y3, y4;

  g_return_if_fail (dest != NULL);
  g_return_if_fail (src != NULL);
  g_return_if_fail (matrix != NULL);

  x1 = src->x;
  y1 = src->y;
  x2 = src->x + src->width;
  y2 = src->y;
  x3 = src->x + src->width;
  y3 = src->y + src->height;
  x4 = src->x;
  y4 = src->y + src->height;

  cairo_matrix_transform_point (matrix, &x1, &y1);
  cairo_matrix_transform_point (matrix, &x2, &y2);
  cairo_matrix_transform_point (matrix, &x3, &y3);
  cairo_matrix_transform_point (matrix, &x4, &y4);

  dest->x = floor (MIN (MIN (x1, x2), MIN (x3, x4)));
  dest->y = floor (MIN (MIN (y1, y2), MIN (y3, y4)));
  dest->width = ceil (MAX (MAX (x1, x2), MAX (x3, x4))) - dest->x;
  dest->height = ceil (MAX (MAX (y1, y2), MAX (y3, y4))) - dest->y;
}
void
_gtk_style_context_get_icon_extents (GtkStyleContext *context,
                                     GdkRectangle    *extents,
                                     gint             x,
                                     gint             y,
                                     gint             width,
                                     gint             height)
{
  cairo_matrix_t transform_matrix, matrix;
  GtkBorder border;
  GdkRectangle rect;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (extents != NULL);

  if (_gtk_css_image_value_get_image (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_SOURCE)) == NULL)
    {
      extents->x = extents->y = extents->width = extents->height = 0;
      return;
    }

  extents->x = x;
  extents->y = y;
  extents->width = width;
  extents->height = height;

  /* builtin images can't be transformed */
  if (GTK_IS_CSS_IMAGE_BUILTIN (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_SOURCE)))
    return;

  if (!_gtk_css_transform_value_get_matrix (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_TRANSFORM), &transform_matrix))
    return;
  
  cairo_matrix_init_translate (&matrix, x + width / 2.0, y + height / 2.0);
  cairo_matrix_multiply (&matrix, &transform_matrix, &matrix);
  /* need to round to full pixels */
  rect.x = - (width + 1) / 2;
  rect.y = - (height + 1) / 2;
  rect.width = (width + 1) & ~1;
  rect.height = (height + 1) & ~1;
  gtk_cairo_rectangle_transform (extents, &rect, &matrix);

  _gtk_css_shadows_value_get_extents (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_SHADOW), &border);

  extents->x -= border.left;
  extents->y -= border.top;
  extents->width += border.left + border.right;
  extents->height += border.top + border.bottom;
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
  GtkCssDependencies ignored = 0;

  g_return_val_if_fail (gradient != NULL, NULL);
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return _gtk_gradient_resolve_full (gradient,
                                     GTK_STYLE_PROVIDER_PRIVATE (priv->cascade),
                                     gtk_style_context_lookup_style (context),
                                     priv->parent ? gtk_style_context_lookup_style (priv->parent) : NULL,
                                     &ignored);
}

