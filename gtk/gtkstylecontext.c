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

#include <gdk/gdk.h>
#include <stdlib.h>
#include <gobject/gvaluecollector.h>

#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkthemingengine.h"
#include "gtkintl.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gtksymboliccolor.h"
#include "gtkanimationdescription.h"
#include "gtktimeline.h"
#include "gtkiconfactory.h"
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
 * combination of all provider's information in priority order.
 *
 * For GTK+ widgets, any #GtkStyleContext returned by
 * gtk_widget_get_style_context() will already have a #GtkWidgetPath, a
 * #GdkScreen and RTL/LTR information set, the style context will be also
 * updated automatically if any of these settings change on the widget.
 *
 * If you are using are the theming layer standalone, you will need to set a
 * widget path and a screen yourself to the created style context through
 * gtk_style_context_set_path() and gtk_style_context_set_screen(), as well
 * as updating the context yourself using gtk_style_context_invalidate()
 * whenever any of the conditions change, such as a change in the
 * #GtkSettings:gtk-theme-name setting or a hierarchy change in the rendered
 * widget.
 *
 * <refsect2 id="gtkstylecontext-animations">
 * <title>Transition animations</title>
 * <para>
 * #GtkStyleContext has built-in support for state change transitions.
 * Note that these animations respect the #GtkSettings:gtk-enable-animations
 * setting.
 * </para>
 * <para>
 * For simple widgets where state changes affect the whole widget area,
 * calling gtk_style_context_notify_state_change() with a %NULL region
 * is sufficient to trigger the transition animation. And GTK+ already
 * does that when gtk_widget_set_state() or gtk_widget_set_state_flags()
 * are called.
 * </para>
 * <para>
 * If a widget needs to declare several animatable regions (i.e. not
 * affecting the whole widget area), its #GtkWidget::draw signal handler
 * needs to wrap the render operations for the different regions with
 * calls to gtk_style_context_push_animatable_region() and
 * gtk_style_context_pop_animatable_region(). These functions take an
 * identifier for the region which must be unique within the style context.
 * For simple widgets with a fixed set of animatable regions, using an
 * enumeration works well:
 * </para>
 * <example>
 * <title>Using an enumeration to identify  animatable regions</title>
 * <programlisting>
 * enum {
 *   REGION_ENTRY,
 *   REGION_BUTTON_UP,
 *   REGION_BUTTON_DOWN
 * };
 *
 * ...
 *
 * gboolean
 * spin_button_draw (GtkWidget *widget,
 *                   cairo_t   *cr)
 * {
 *   GtkStyleContext *context;
 *
 *   context = gtk_widget_get_style_context (widget);
 *
 *   gtk_style_context_push_animatable_region (context,
 *                                             GUINT_TO_POINTER (REGION_ENTRY));
 *
 *   gtk_render_background (cr, 0, 0, 100, 30);
 *   gtk_render_frame (cr, 0, 0, 100, 30);
 *
 *   gtk_style_context_pop_animatable_region (context);
 *
 *   ...
 * }
 * </programlisting>
 * </example>
 * <para>
 * For complex widgets with an arbitrary number of animatable regions, it
 * is up to the implementation to come up with a way to uniquely identify
 * each animatable region. Using pointers to internal structs is one way
 * to achieve this:
 * </para>
 * <example>
 * <title>Using struct pointers to identify animatable regions</title>
 * <programlisting>
 * void
 * notebook_draw_tab (GtkWidget    *widget,
 *                    NotebookPage *page,
 *                    cairo_t      *cr)
 * {
 *   gtk_style_context_push_animatable_region (context, page);
 *   gtk_render_extension (cr, page->x, page->y, page->width, page->height);
 *   gtk_style_context_pop_animatable_region (context);
 * }
 * </programlisting>
 * </example>
 * <para>
 * The widget also needs to notify the style context about a state change
 * for a given animatable region so the animation is triggered.
 * </para>
 * <example>
 * <title>Triggering a state change animation on a region</title>
 * <programlisting>
 * gboolean
 * notebook_motion_notify (GtkWidget      *widget,
 *                         GdkEventMotion *event)
 * {
 *   GtkStyleContext *context;
 *   NotebookPage *page;
 *
 *   context = gtk_widget_get_style_context (widget);
 *   page = find_page_under_pointer (widget, event);
 *   gtk_style_context_notify_state_change (context,
 *                                          gtk_widget_get_window (widget),
 *                                          page,
 *                                          GTK_STATE_PRELIGHT,
 *                                          TRUE);
 *   ...
 * }
 * </programlisting>
 * </example>
 * <para>
 * gtk_style_context_notify_state_change() accepts %NULL region IDs as a
 * special value, in this case, the whole widget area will be updated
 * by the animation.
 * </para>
 * </refsect2>
 * <refsect2 id="gtkstylecontext-classes">
 * <title>Style classes and regions</title>
 * <para>
 * Widgets can add style classes to their context, which can be used
 * to associate different styles by class (see <xref linkend="gtkcssprovider-selectors"/>). Theme engines can also use style classes to vary their
 * rendering. GTK+ has a number of predefined style classes:
 * <informaltable>
 *   <tgroup cols="3">
 *     <thead>
 *       <row>
 *         <entry>Style class</entry>
 *         <entry>Macro</entry>
 *         <entry>Used by</entry>
 *       </row>
 *     </thead>
 *     <tbody>
 *       <row>
 *         <entry>button</entry>
 *         <entry>GTK_STYLE_CLASS_BUTTON</entry>
 *         <entry>#GtkButton, #GtkToggleButton, #GtkRadioButton, #GtkCheckButton</entry>
 *       </row>
 *       <row>
 *         <entry>default</entry>
 *         <entry>GTK_STYLE_CLASS_DEFAULT</entry>
 *         <entry>#GtkButton</entry>
 *       </row>
 *       <row>
 *         <entry>check</entry>
 *         <entry>GTK_STYLE_CLASS_CHECK</entry>
 *         <entry>#GtkCheckButton, #GtkCheckMenuItem, #GtkCellRendererToggle</entry>
 *       </row>
 *       <row>
 *         <entry>radio</entry>
 *         <entry>GTK_STYLE_CLASS_RADIO</entry>
 *         <entry>#GtkRadioButton, #GtkRadioMenuItem, #GtkCellRendererToggle</entry>
 *       </row>
 *       <row>
 *         <entry>arrow</entry>
 *         <entry>GTK_STYLE_CLASS_ARROW</entry>
 *         <entry>#GtkArrow</entry>
 *       </row>
 *       <row>
 *         <entry>calendar</entry>
 *         <entry>GTK_STYLE_CLASS_CALENDAR</entry>
 *         <entry>#GtkCalendar</entry>
 *       </row>
 *       <row>
 *         <entry>entry</entry>
 *         <entry>GTK_STYLE_CLASS_ENTRY</entry>
 *         <entry>#GtkEntry</entry>
 *       </row>
 *       <row>
 *         <entry>cell</entry>
 *         <entry>GTK_STYLE_CLASS_CELL</entry>
 *         <entry>#GtkCellRendererToggle</entry>
 *       </row>
 *       <row>
 *         <entry>menu</entry>
 *         <entry>GTK_STYLE_CLASS_MENU</entry>
 *         <entry>#GtkMenu, #GtkMenuItem, #GtkCheckMenuItem, #GtkRadioMenuItem</entry>
 *       </row>
 *       <row>
 *         <entry>expander</entry>
 *         <entry>GTK_STYLE_CLASS_EXPANDER</entry>
 *         <entry>#GtkExpander</entry>
 *       </row>
 *       <row>
 *         <entry>tooltip</entry>
 *         <entry>GTK_STYLE_CLASS_TOOLTIP</entry>
 *         <entry>#GtkTooltip</entry>
 *       </row>
 *       <row>
 *         <entry>frame</entry>
 *         <entry>GTK_STYLE_CLASS_FRAME</entry>
 *         <entry>#GtkFrame</entry>
 *       </row>
 *       <row>
 *         <entry>scrolled-window</entry>
 *         <entry></entry>
 *         <entry>#GtkScrolledWindow</entry>
 *       </row>
 *       <row>
 *         <entry>viewport</entry>
 *         <entry></entry>
 *         <entry>#GtkViewport</entry>
 *       </row>
 *       <row>
 *         <entry>trough</entry>
 *         <entry>GTK_STYLE_CLASS_TROUGH</entry>
 *         <entry>#GtkScrollbar, #GtkProgressBar, #GtkScale</entry>
 *       </row>
 *       <row>
 *         <entry>progressbar</entry>
 *         <entry>GTK_STYLE_CLASS_PROGRESSBAR</entry>
 *         <entry>#GtkProgressBar, #GtkCellRendererProgress</entry>
 *       </row>
 *       <row>
 *         <entry>slider</entry>
 *         <entry>GTK_STYLE_CLASS_SLIDER</entry>
 *         <entry>#GtkScrollbar, #GtkScale</entry>
 *       </row>
 *       <row>
 *         <entry>menuitem</entry>
 *         <entry>GTK_STYLE_CLASS_MENUITEM</entry>
 *         <entry>#GtkMenuItem</entry>
 *       </row>
 *       <row>
 *         <entry>popup</entry>
 *         <entry></entry>
 *         <entry>#GtkMenu</entry>
 *       </row>
 *       <row>
 *         <entry>accelerator</entry>
 *         <entry>GTK_STYLE_CLASS_ACCELERATOR</entry>
 *         <entry>#GtkAccelLabel</entry>
 *       </row>
 *       <row>
 *         <entry>menubar</entry>
 *         <entry>GTK_STYLE_CLASS_MENUBAR</entry>
 *         <entry>#GtkMenuBar</entry>
 *       </row>
 *       <row>
 *         <entry>toolbar</entry>
 *         <entry>GTK_STYLE_CLASS_TOOLBAR</entry>
 *         <entry>#GtkToolbar</entry>
 *       </row>
 *       <row>
 *         <entry>dock</entry>
 *         <entry>GTK_STYLE_CLASS_DOCK</entry>
 *         <entry>#GtkHandleBox</entry>
 *       </row>
 *       <row>
 *         <entry>notebook</entry>
 *         <entry></entry>
 *         <entry>#GtkNotebook</entry>
 *       </row>
 *       <row>
 *         <entry>background</entry>
 *         <entry>GTK_STYLE_CLASS_BACKGROUND</entry>
 *         <entry>#GtkWindow</entry>
 *       </row>
 *       <row>
 *         <entry>rubberband</entry>
 *         <entry>GTK_STYLE_CLASS_RUBBERBAND</entry>
 *         <entry></entry>
 *       </row>
 *       <row>
 *         <entry>header</entry>
 *         <entry>GTK_STYLE_CLASS_HEADER</entry>
 *         <entry></entry>
 *       </row>
 *       <row>
 *         <entry>grip</entry>
 *         <entry>GTK_STYLE_CLASS_GRIP</entry>
 *         <entry>#GtkWindow</entry>
 *       </row>
 *       <row>
 *         <entry>spinner</entry>
 *         <entry>GTK_STYLE_CLASS_SPINNER</entry>
 *         <entry>#GtkSpinner</entry>
 *       </row>
 *     </tbody>
 *   </tgroup>
 * </informaltable>
 * </para>
 * <para>
 * Widgets can also add regions with flags to their context.
 * The regions used by GTK+ widgets are:
 * <informaltable>
 *   <tgroup cols="4">
 *     <thead>
 *       <row>
 *         <entry>Region</entry>
 *         <entry>Flags</entry>
 *         <entry>Macro</entry>
 *         <entry>Used by</entry>
 *       </row>
 *     </thead>
 *     <tbody>
 *       <row>
 *         <entry>row</entry>
 *         <entry>even, odd</entry>
 *         <entry>GTK_STYLE_REGION_ROW</entry>
 *         <entry>#GtkTreeView</entry>
 *       </row>
 *       <row>
 *         <entry>column</entry>
 *         <entry>first, last, sorted</entry>
 *         <entry>GTK_STYLE_REGION_COLUMN</entry>
 *         <entry>#GtkTreeView</entry>
 *       </row>
 *       <row>
 *         <entry>column-header</entry>
 *         <entry></entry>
 *         <entry>GTK_STYLE_REGION_COLUMN_HEADER</entry>
 *         <entry></entry>
 *       </row>
 *       <row>
 *         <entry>tab</entry>
 *         <entry>even, odd, first, last</entry>
 *         <entry>GTK_STYLE_REGION_TAB</entry>
 *         <entry>#GtkNotebook</entry>
 *       </row>
 *     </tbody>
 *   </tgroup>
 * </informaltable>
 * </para>
 * </refsect2>
 * <refsect2 id="gtkstylecontext-custom-styling">
 * <title>Custom styling in UI libraries and applications</title>
 * <para>
 * If you are developing a library with custom #GtkWidget<!-- -->s that
 * render differently than standard components, you may need to add a
 * #GtkStyleProvider yourself with the %GTK_STYLE_PROVIDER_PRIORITY_FALLBACK
 * priority, either a #GtkCssProvider or a custom object implementing the
 * #GtkStyleProvider interface. This way theming engines may still attempt
 * to style your UI elements in a different way if needed so.
 * </para>
 * <para>
 * If you are using custom styling on an applications, you probably want then
 * to make your style information prevail to the theme's, so you must use
 * a #GtkStyleProvider with the %GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
 * priority, keep in mind that the user settings in
 * <filename><replaceable>XDG_CONFIG_HOME</replaceable>/gtk-3.0/gtk.css</filename> will
 * still take precedence over your changes, as it uses the
 * %GTK_STYLE_PROVIDER_PRIORITY_USER priority.
 * </para>
 * <para>
 * If a custom theming engine is needed, you probably want to implement a
 * #GtkStyleProvider yourself so it points to your #GtkThemingEngine
 * implementation, as #GtkCssProvider uses gtk_theming_engine_load()
 * which loads the theming engine module from the standard paths.
 * </para>
 * </refsect2>
 */

typedef struct GtkStyleContextPrivate GtkStyleContextPrivate;
typedef struct GtkStyleProviderData GtkStyleProviderData;
typedef struct GtkStyleInfo GtkStyleInfo;
typedef struct GtkRegion GtkRegion;
typedef struct PropertyValue PropertyValue;
typedef struct AnimationInfo AnimationInfo;
typedef struct StyleData StyleData;

struct GtkRegion
{
  GQuark class_quark;
  GtkRegionFlags flags;
};

struct GtkStyleProviderData
{
  GtkStyleProvider *provider;
  guint priority;
};

struct PropertyValue
{
  GType       widget_type;
  GParamSpec *pspec;
  GtkStateFlags state;
  GValue      value;
};

struct GtkStyleInfo
{
  GArray *style_classes;
  GArray *regions;
  GtkJunctionSides junction_sides;
  GtkStateFlags state_flags;
};

struct StyleData
{
  GtkStyleProperties *store;
  GSList *icon_factories;
  GArray *property_cache;
};

struct AnimationInfo
{
  GtkTimeline *timeline;

  gpointer region_id;

  /* Region stack (until region_id) at the time of
   * rendering, this is used for nested cancellation.
   */
  GSList *parent_regions;

  GdkWindow *window;
  GtkStateType state;
  gboolean target_value;

  cairo_region_t *invalidation_region;
  GArray *rectangles;
};

struct GtkStyleContextPrivate
{
  GdkScreen *screen;

  GList *providers;
  GList *providers_last;

  GtkWidgetPath *widget_path;
  GHashTable *style_data;
  GSList *info_stack;
  StyleData *current_data;

  GSList *animation_regions;
  GSList *animations;

  guint animations_invalidated : 1;
  guint invalidating_context : 1;

  GtkThemingEngine *theming_engine;

  GtkTextDirection direction;
};

enum {
  PROP_0,
  PROP_SCREEN,
  PROP_DIRECTION
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GQuark provider_list_quark = 0;
static GdkRGBA fallback_color = { 1.0, 0.75, 0.75, 1.0 };
static GtkBorder fallback_border = { 0 };

static void gtk_style_context_finalize (GObject *object);

static void gtk_style_context_impl_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec);
static void gtk_style_context_impl_get_property (GObject      *object,
                                                 guint         prop_id,
                                                 GValue       *value,
                                                 GParamSpec   *pspec);


G_DEFINE_TYPE (GtkStyleContext, gtk_style_context, G_TYPE_OBJECT)

static void
gtk_style_context_class_init (GtkStyleContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_style_context_finalize;
  object_class->set_property = gtk_style_context_impl_set_property;
  object_class->get_property = gtk_style_context_impl_get_property;

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
                                   PROP_DIRECTION,
                                   g_param_spec_enum ("direction",
                                                      P_("Direction"),
                                                      P_("Text direction"),
                                                      GTK_TYPE_TEXT_DIRECTION,
                                                      GTK_TEXT_DIR_LTR,
                                                      GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkStyleContextPrivate));
}

static GtkStyleInfo *
style_info_new (void)
{
  GtkStyleInfo *info;

  info = g_slice_new0 (GtkStyleInfo);
  info->style_classes = g_array_new (FALSE, FALSE, sizeof (GQuark));
  info->regions = g_array_new (FALSE, FALSE, sizeof (GtkRegion));

  return info;
}

static void
style_info_free (GtkStyleInfo *info)
{
  g_array_free (info->style_classes, TRUE);
  g_array_free (info->regions, TRUE);
  g_slice_free (GtkStyleInfo, info);
}

static GtkStyleInfo *
style_info_copy (const GtkStyleInfo *info)
{
  GtkStyleInfo *copy;

  copy = style_info_new ();
  g_array_insert_vals (copy->style_classes, 0,
                       info->style_classes->data,
                       info->style_classes->len);

  g_array_insert_vals (copy->regions, 0,
                       info->regions->data,
                       info->regions->len);

  copy->junction_sides = info->junction_sides;
  copy->state_flags = info->state_flags;

  return copy;
}

static guint
style_info_hash (gconstpointer elem)
{
  const GtkStyleInfo *info;
  guint i, hash = 0;

  info = elem;

  for (i = 0; i < info->style_classes->len; i++)
    {
      hash += g_array_index (info->style_classes, GQuark, i);
      hash <<= 5;
    }

  for (i = 0; i < info->regions->len; i++)
    {
      GtkRegion *region;

      region = &g_array_index (info->regions, GtkRegion, i);
      hash += region->class_quark;
      hash += region->flags;
      hash <<= 5;
    }

  return hash;
}

static gboolean
style_info_equal (gconstpointer elem1,
                  gconstpointer elem2)
{
  const GtkStyleInfo *info1, *info2;

  info1 = elem1;
  info2 = elem2;

  if (info1->junction_sides != info2->junction_sides)
    return FALSE;

  if (info1->style_classes->len != info2->style_classes->len)
    return FALSE;

  if (memcmp (info1->style_classes->data,
              info2->style_classes->data,
              info1->style_classes->len * sizeof (GQuark)) != 0)
    return FALSE;

  if (info1->regions->len != info2->regions->len)
    return FALSE;

  if (memcmp (info1->regions->data,
              info2->regions->data,
              info1->regions->len * sizeof (GtkRegion)) != 0)
    return FALSE;

  return TRUE;
}

static StyleData *
style_data_new (void)
{
  StyleData *data;

  data = g_slice_new0 (StyleData);
  data->store = gtk_style_properties_new ();

  return data;
}

static void
clear_property_cache (StyleData *data)
{
  guint i;

  if (!data->property_cache)
    return;

  for (i = 0; i < data->property_cache->len; i++)
    {
      PropertyValue *node = &g_array_index (data->property_cache, PropertyValue, i);

      g_param_spec_unref (node->pspec);
      g_value_unset (&node->value);
    }

  g_array_free (data->property_cache, TRUE);
  data->property_cache = NULL;
}

static void
style_data_free (StyleData *data)
{
  g_object_unref (data->store);
  clear_property_cache (data);

  g_slist_foreach (data->icon_factories, (GFunc) g_object_unref, NULL);
  g_slist_free (data->icon_factories);

  g_slice_free (StyleData, data);
}

static void
gtk_style_context_init (GtkStyleContext *style_context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;

  priv = style_context->priv = G_TYPE_INSTANCE_GET_PRIVATE (style_context,
                                                            GTK_TYPE_STYLE_CONTEXT,
                                                            GtkStyleContextPrivate);

  priv->style_data = g_hash_table_new_full (style_info_hash,
                                            style_info_equal,
                                            (GDestroyNotify) style_info_free,
                                            (GDestroyNotify) style_data_free);
  priv->theming_engine = g_object_ref ((gpointer) gtk_theming_engine_load (NULL));

  priv->direction = GTK_TEXT_DIR_LTR;

  priv->screen = gdk_screen_get_default ();

  /* Create default info store */
  info = style_info_new ();
  priv->info_stack = g_slist_prepend (priv->info_stack, info);
}

static GtkStyleProviderData *
style_provider_data_new (GtkStyleProvider *provider,
                         guint             priority)
{
  GtkStyleProviderData *data;

  data = g_slice_new (GtkStyleProviderData);
  data->provider = g_object_ref (provider);
  data->priority = priority;

  return data;
}

static void
style_provider_data_free (GtkStyleProviderData *data)
{
  g_object_unref (data->provider);
  g_slice_free (GtkStyleProviderData, data);
}

static void
animation_info_free (AnimationInfo *info)
{
  g_object_unref (info->timeline);
  g_object_unref (info->window);

  if (info->invalidation_region)
    cairo_region_destroy (info->invalidation_region);

  g_array_free (info->rectangles, TRUE);
  g_slist_free (info->parent_regions);
  g_slice_free (AnimationInfo, info);
}

static AnimationInfo *
animation_info_lookup_by_timeline (GtkStyleContext *context,
                                   GtkTimeline     *timeline)
{
  GtkStyleContextPrivate *priv;
  AnimationInfo *info;
  GSList *l;

  priv = context->priv;

  for (l = priv->animations; l; l = l->next)
    {
      info = l->data;

      if (info->timeline == timeline)
        return info;
    }

  return NULL;
}

static void
timeline_frame_cb (GtkTimeline *timeline,
                   gdouble      progress,
                   gpointer     user_data)
{
  GtkStyleContextPrivate *priv;
  GtkStyleContext *context;
  AnimationInfo *info;

  context = user_data;
  priv = context->priv;
  info = animation_info_lookup_by_timeline (context, timeline);

  g_assert (info != NULL);

  /* Cancel transition if window is gone */
  if (gdk_window_is_destroyed (info->window) ||
      !gdk_window_is_visible (info->window))
    {
      priv->animations = g_slist_remove (priv->animations, info);
      animation_info_free (info);
      return;
    }

  if (info->invalidation_region &&
      !cairo_region_is_empty (info->invalidation_region))
    gdk_window_invalidate_region (info->window, info->invalidation_region, TRUE);
  else
    gdk_window_invalidate_rect (info->window, NULL, TRUE);
}

static void
timeline_finished_cb (GtkTimeline *timeline,
                      gpointer     user_data)
{
  GtkStyleContextPrivate *priv;
  GtkStyleContext *context;
  AnimationInfo *info;

  context = user_data;
  priv = context->priv;
  info = animation_info_lookup_by_timeline (context, timeline);

  g_assert (info != NULL);

  priv->animations = g_slist_remove (priv->animations, info);

  /* Invalidate one last time the area, so the final content is painted */
  if (info->invalidation_region &&
      !cairo_region_is_empty (info->invalidation_region))
    gdk_window_invalidate_region (info->window, info->invalidation_region, TRUE);
  else
    gdk_window_invalidate_rect (info->window, NULL, TRUE);

  animation_info_free (info);
}

static AnimationInfo *
animation_info_new (GtkStyleContext         *context,
                    gpointer                 region_id,
                    guint                    duration,
                    GtkTimelineProgressType  progress_type,
                    gboolean                 loop,
                    GtkStateType             state,
                    gboolean                 target_value,
                    GdkWindow               *window)
{
  AnimationInfo *info;

  info = g_slice_new0 (AnimationInfo);

  info->rectangles = g_array_new (FALSE, FALSE, sizeof (cairo_rectangle_int_t));
  info->timeline = _gtk_timeline_new (duration);
  info->window = g_object_ref (window);
  info->state = state;
  info->target_value = target_value;
  info->region_id = region_id;

  _gtk_timeline_set_progress_type (info->timeline, progress_type);
  _gtk_timeline_set_loop (info->timeline, loop);

  if (!loop && !target_value)
    {
      _gtk_timeline_set_direction (info->timeline, GTK_TIMELINE_DIRECTION_BACKWARD);
      _gtk_timeline_rewind (info->timeline);
    }

  g_signal_connect (info->timeline, "frame",
                    G_CALLBACK (timeline_frame_cb), context);
  g_signal_connect (info->timeline, "finished",
                    G_CALLBACK (timeline_finished_cb), context);

  _gtk_timeline_start (info->timeline);

  return info;
}

static AnimationInfo *
animation_info_lookup (GtkStyleContext *context,
                       gpointer         region_id,
                       GtkStateType     state)
{
  GtkStyleContextPrivate *priv;
  GSList *l;

  priv = context->priv;

  for (l = priv->animations; l; l = l->next)
    {
      AnimationInfo *info;

      info = l->data;

      if (info->state == state &&
          info->region_id == region_id)
        return info;
    }

  return NULL;
}

static void
gtk_style_context_finalize (GObject *object)
{
  GtkStyleContextPrivate *priv;
  GtkStyleContext *style_context;
  GSList *l;

  style_context = GTK_STYLE_CONTEXT (object);
  priv = style_context->priv;

  if (priv->widget_path)
    gtk_widget_path_free (priv->widget_path);

  g_hash_table_destroy (priv->style_data);

  g_list_foreach (priv->providers, (GFunc) style_provider_data_free, NULL);
  g_list_free (priv->providers);

  g_slist_foreach (priv->info_stack, (GFunc) style_info_free, NULL);
  g_slist_free (priv->info_stack);

  g_slist_free (priv->animation_regions);

  for (l = priv->animations; l; l = l->next)
    animation_info_free ((AnimationInfo *) l->data);

  g_slist_free (priv->animations);

  if (priv->theming_engine)
    g_object_unref (priv->theming_engine);

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
      gtk_style_context_set_direction (style_context,
                                       g_value_get_enum (value));
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
      g_value_set_enum (value, priv->direction);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GList *
find_next_candidate (GList    *local,
                     GList    *global,
                     gboolean  ascending)
{
  if (local && global)
    {
      GtkStyleProviderData *local_data, *global_data;

      local_data = local->data;
      global_data = global->data;

      if (local_data->priority < global_data->priority)
        return (ascending) ? local : global;
      else
        return (ascending) ? global : local;
    }
  else if (local)
    return local;
  else if (global)
    return global;

  return NULL;
}

static void
build_properties (GtkStyleContext *context,
                  StyleData       *style_data,
                  GtkWidgetPath   *path)
{
  GtkStyleContextPrivate *priv;
  GList *elem, *list, *global_list = NULL;

  priv = context->priv;
  list = priv->providers;

  if (priv->screen)
    global_list = g_object_get_qdata (G_OBJECT (priv->screen), provider_list_quark);

  while ((elem = find_next_candidate (list, global_list, TRUE)) != NULL)
    {
      GtkStyleProviderData *data;
      GtkStyleProperties *provider_style;

      data = elem->data;

      if (elem == list)
        list = list->next;
      else
        global_list = global_list->next;

      provider_style = gtk_style_provider_get_style (data->provider, path);

      if (provider_style)
        {
          gtk_style_properties_merge (style_data->store, provider_style, TRUE);
          g_object_unref (provider_style);
        }
    }
}

static void
build_icon_factories (GtkStyleContext *context,
                      StyleData       *style_data,
                      GtkWidgetPath   *path)
{
  GtkStyleContextPrivate *priv;
  GList *elem, *list, *global_list = NULL;

  priv = context->priv;
  list = priv->providers_last;

  if (priv->screen)
    {
      global_list = g_object_get_qdata (G_OBJECT (priv->screen), provider_list_quark);
      global_list = g_list_last (global_list);
    }

  while ((elem = find_next_candidate (list, global_list, FALSE)) != NULL)
    {
      GtkIconFactory *factory;
      GtkStyleProviderData *data;

      data = elem->data;

      if (elem == list)
        list = list->prev;
      else
        global_list = global_list->prev;

      factory = gtk_style_provider_get_icon_factory (data->provider, path);

      if (factory)
        style_data->icon_factories = g_slist_prepend (style_data->icon_factories, factory);
    }
}

static GtkWidgetPath *
create_query_path (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkWidgetPath *path;
  GtkStyleInfo *info;
  guint i, pos;

  priv = context->priv;
  path = gtk_widget_path_copy (priv->widget_path);
  pos = gtk_widget_path_length (path) - 1;

  info = priv->info_stack->data;

  /* Set widget regions */
  for (i = 0; i < info->regions->len; i++)
    {
      GtkRegion *region;

      region = &g_array_index (info->regions, GtkRegion, i);
      gtk_widget_path_iter_add_region (path, pos,
                                       g_quark_to_string (region->class_quark),
                                       region->flags);
    }

  /* Set widget classes */
  for (i = 0; i < info->style_classes->len; i++)
    {
      GQuark quark;

      quark = g_array_index (info->style_classes, GQuark, i);
      gtk_widget_path_iter_add_class (path, pos,
                                      g_quark_to_string (quark));
    }

  return path;
}

static StyleData *
style_data_lookup (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  StyleData *data;

  priv = context->priv;

  /* Current data in use is cached, just return it */
  if (priv->current_data)
    return priv->current_data;

  g_assert (priv->widget_path != NULL);

  data = g_hash_table_lookup (priv->style_data, priv->info_stack->data);

  if (!data)
    {
      GtkWidgetPath *path;

      data = style_data_new ();
      path = create_query_path (context);

      build_properties (context, data, path);
      build_icon_factories (context, data, path);

      g_hash_table_insert (priv->style_data,
                           style_info_copy (priv->info_stack->data),
                           data);

      gtk_widget_path_free (path);
    }

  priv->current_data = data;

  if (priv->theming_engine)
    g_object_unref (priv->theming_engine);

  gtk_style_properties_get (data->store, 0,
                            "engine", &priv->theming_engine,
                            NULL);

  if (!priv->theming_engine)
    priv->theming_engine = g_object_ref (gtk_theming_engine_load (NULL));

  return data;
}

static void
style_provider_add (GList            **list,
                    GtkStyleProvider  *provider,
                    guint              priority)
{
  GtkStyleProviderData *new_data;
  gboolean added = FALSE;
  GList *l = *list;

  new_data = style_provider_data_new (provider, priority);

  while (l)
    {
      GtkStyleProviderData *data;

      data = l->data;

      /* Provider was already attached to the style
       * context, remove in order to add the new data
       */
      if (data->provider == provider)
        {
          GList *link;

          link = l;
          l = l->next;

          /* Remove and free link */
          *list = g_list_remove_link (*list, link);
          style_provider_data_free (link->data);
          g_list_free_1 (link);

          continue;
        }

      if (!added &&
          data->priority > priority)
        {
          *list = g_list_insert_before (*list, l, new_data);
          added = TRUE;
        }

      l = l->next;
    }

  if (!added)
    *list = g_list_append (*list, new_data);
}

static gboolean
style_provider_remove (GList            **list,
                       GtkStyleProvider  *provider)
{
  GList *l = *list;

  while (l)
    {
      GtkStyleProviderData *data;

      data = l->data;

      if (data->provider == provider)
        {
          *list = g_list_remove_link (*list, l);
          style_provider_data_free (l->data);
          g_list_free_1 (l);

          return TRUE;
        }

      l = l->next;
    }

  return FALSE;
}

/**
 * gtk_style_context_new:
 *
 * Creates a standalone #GtkStyleContext, this style context
 * won't be attached to any widget, so you may want
 * to call gtk_style_context_set_path() yourself.
 *
 * <note>
 * This function is only useful when using the theming layer
 * separated from GTK+, if you are using #GtkStyleContext to
 * theme #GtkWidget<!-- -->s, use gtk_widget_get_style_context()
 * in order to get a style context ready to theme the widget.
 * </note>
 *
 * Returns: A newly created #GtkStyleContext.
 **/
GtkStyleContext *
gtk_style_context_new (void)
{
  return g_object_new (GTK_TYPE_STYLE_CONTEXT, NULL);
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
 *
 * <note><para>If both priorities are the same, A #GtkStyleProvider
 * added through this function takes precedence over another added
 * through gtk_style_context_add_provider_for_screen().</para></note>
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
  style_provider_add (&priv->providers, provider, priority);
  priv->providers_last = g_list_last (priv->providers);

  gtk_style_context_invalidate (context);
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

  if (style_provider_remove (&priv->providers, provider))
    {
      priv->providers_last = g_list_last (priv->providers);

      gtk_style_context_invalidate (context);
    }
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
 * in style construction for all #GtkStyleContext<!-- -->s under
 * @screen.
 *
 * GTK+ uses this to make styling information from #GtkSettings
 * available.
 *
 * <note><para>If both priorities are the same, A #GtkStyleProvider
 * added through gtk_style_context_add_provider() takes precedence
 * over another added through this function.</para></note>
 *
 * Since: 3.0
 **/
void
gtk_style_context_add_provider_for_screen (GdkScreen        *screen,
                                           GtkStyleProvider *provider,
                                           guint             priority)
{
  GList *providers, *list;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  if (G_UNLIKELY (!provider_list_quark))
    provider_list_quark = g_quark_from_static_string ("gtk-provider-list-quark");

  list = providers = g_object_get_qdata (G_OBJECT (screen), provider_list_quark);
  style_provider_add (&list, provider, priority);

  if (list != providers)
    g_object_set_qdata (G_OBJECT (screen), provider_list_quark, list);

  gtk_style_context_reset_widgets (screen);
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
  GList *providers, *list;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  if (G_UNLIKELY (!provider_list_quark))
    return;

  list = providers = g_object_get_qdata (G_OBJECT (screen), provider_list_quark);

  if (style_provider_remove (&list, provider))
    {
      if (list != providers)
        g_object_set_qdata (G_OBJECT (screen), provider_list_quark, list);

      gtk_style_context_reset_widgets (screen);
    }
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
  GtkStyleContextPrivate *priv;
  StyleData *data;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (property != NULL);
  g_return_if_fail (value != NULL);

  priv = context->priv;

  g_return_if_fail (priv->widget_path != NULL);

  data = style_data_lookup (context);
  gtk_style_properties_get_property (data->store, property, state, value);
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
  GtkStyleContextPrivate *priv;
  StyleData *data;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  g_return_if_fail (priv->widget_path != NULL);

  data = style_data_lookup (context);
  gtk_style_properties_get_valist (data->store, state, args);
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
  GtkStyleContextPrivate *priv;
  StyleData *data;
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  g_return_if_fail (priv->widget_path != NULL);

  data = style_data_lookup (context);

  va_start (args, state);
  gtk_style_properties_get_valist (data->store, state, args);
  va_end (args);
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
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  info = priv->info_stack->data;
  info->state_flags = flags;
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
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  priv = context->priv;
  info = priv->info_stack->data;

  return info->state_flags;
}

static gboolean
context_has_animatable_region (GtkStyleContext *context,
                               gpointer         region_id)
{
  GtkStyleContextPrivate *priv;

  /* NULL region_id means everything
   * rendered through the style context
   */
  if (!region_id)
    return TRUE;

  priv = context->priv;
  return g_slist_find (priv->animation_regions, region_id) != NULL;
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
 * it's closest to being set. This means transition animation will
 * run from 0 to 1 when @state is being set and from 1 to 0 when
 * it's being unset.
 *
 * Returns: %TRUE if there is a running transition animation for @state.
 *
 * Since: 3.0
 **/
gboolean
gtk_style_context_state_is_running (GtkStyleContext *context,
                                    GtkStateType     state,
                                    gdouble         *progress)
{
  GtkStyleContextPrivate *priv;
  AnimationInfo *info;
  GSList *l;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);

  priv = context->priv;

  for (l = priv->animations; l; l = l->next)
    {
      info = l->data;

      if (info->state == state &&
          context_has_animatable_region (context, info->region_id))
        {
          if (progress)
            *progress = _gtk_timeline_get_progress (info->timeline);

          return TRUE;
        }
    }

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
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (path != NULL);

  priv = context->priv;

  if (priv->widget_path)
    {
      gtk_widget_path_free (priv->widget_path);
      priv->widget_path = NULL;
    }

  if (path)
    priv->widget_path = gtk_widget_path_copy (path);

  gtk_style_context_invalidate (context);
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
G_CONST_RETURN GtkWidgetPath *
gtk_style_context_get_path (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  priv = context->priv;
  return priv->widget_path;
}

/**
 * gtk_style_context_save:
 * @context: a #GtkStyleContext
 *
 * Saves the @context state, so all modifications done through
 * gtk_style_context_add_class(), gtk_style_context_remove_class(),
 * gtk_style_context_add_region(), gtk_style_context_remove_region()
 * or gtk_style_context_set_junction_sides() can be reverted in one
 * go through gtk_style_context_restore().
 *
 * Since: 3.0
 **/
void
gtk_style_context_save (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  g_assert (priv->info_stack != NULL);

  info = style_info_copy (priv->info_stack->data);
  priv->info_stack = g_slist_prepend (priv->info_stack, info);
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
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  if (priv->info_stack)
    {
      info = priv->info_stack->data;
      priv->info_stack = g_slist_remove (priv->info_stack, info);
      style_info_free (info);
    }

  if (!priv->info_stack)
    {
      g_warning ("Unpaired gtk_style_context_restore() call");

      /* Create default region */
      info = style_info_new ();
      priv->info_stack = g_slist_prepend (priv->info_stack, info);
    }

  priv->current_data = NULL;
}

static gboolean
style_class_find (GArray *array,
                  GQuark  class_quark,
                  guint  *position)
{
  gint min, max, mid;
  gboolean found = FALSE;
  guint pos;

  if (position)
    *position = 0;

  if (!array || array->len == 0)
    return FALSE;

  min = 0;
  max = array->len - 1;

  do
    {
      GQuark item;

      mid = (min + max) / 2;
      item = g_array_index (array, GQuark, mid);

      if (class_quark == item)
        {
          found = TRUE;
          pos = mid;
        }
      else if (class_quark > item)
        min = pos = mid + 1;
      else
        {
          max = mid - 1;
          pos = mid;
        }
    }
  while (!found && min <= max);

  if (position)
    *position = pos;

  return found;
}

static gboolean
region_find (GArray *array,
             GQuark  class_quark,
             guint  *position)
{
  gint min, max, mid;
  gboolean found = FALSE;
  guint pos;

  if (position)
    *position = 0;

  if (!array || array->len == 0)
    return FALSE;

  min = 0;
  max = array->len - 1;

  do
    {
      GtkRegion *region;

      mid = (min + max) / 2;
      region = &g_array_index (array, GtkRegion, mid);

      if (region->class_quark == class_quark)
        {
          found = TRUE;
          pos = mid;
        }
      else if (region->class_quark > class_quark)
        min = pos = mid + 1;
      else
        {
          max = mid - 1;
          pos = mid;
        }
    }
  while (!found && min <= max);

  if (position)
    *position = pos;

  return found;
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
 * In the CSS file format, a #GtkEntry defining an "entry"
 * class, would be matched by:
 *
 * <programlisting>
 * GtkEntry.entry { ... }
 * </programlisting>
 *
 * While any widget defining an "entry" class would be
 * matched by:
 * <programlisting>
 * .entry { ... }
 * </programlisting>
 *
 * Since: 3.0
 **/
void
gtk_style_context_add_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark class_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  priv = context->priv;
  class_quark = g_quark_from_string (class_name);

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  if (!style_class_find (info->style_classes, class_quark, &position))
    {
      g_array_insert_val (info->style_classes, position, class_quark);

      /* Unset current data, as it likely changed due to the class change */
      priv->current_data = NULL;
    }
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
  GtkStyleInfo *info;
  GQuark class_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return;

  priv = context->priv;

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  if (style_class_find (info->style_classes, class_quark, &position))
    {
      g_array_remove_index (info->style_classes, position);

      /* Unset current data, as it likely changed due to the class change */
      priv->current_data = NULL;
    }
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
  GtkStyleInfo *info;
  GQuark class_quark;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (class_name != NULL, FALSE);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return FALSE;

  priv = context->priv;

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  if (style_class_find (info->style_classes, class_quark, NULL))
    return TRUE;

  return FALSE;
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
  GtkStyleInfo *info;
  GList *classes = NULL;
  guint i;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  for (i = 0; i < info->style_classes->len; i++)
    {
      GQuark quark;

      quark = g_array_index (info->style_classes, GQuark, i);
      classes = g_list_prepend (classes, (gchar *) g_quark_to_string (quark));
    }

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
 **/
GList *
gtk_style_context_list_regions (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GList *classes = NULL;
  guint i;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  for (i = 0; i < info->regions->len; i++)
    {
      GtkRegion *region;
      const gchar *class_name;

      region = &g_array_index (info->regions, GtkRegion, i);

      class_name = g_quark_to_string (region->class_quark);
      classes = g_list_prepend (classes, (gchar *) class_name);
    }

  return classes;
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
 * In the CSS file format, a #GtkTreeView defining a "row"
 * region, would be matched by:
 *
 * <programlisting>
 * GtkTreeView row { ... }
 * </programlisting>
 *
 * Pseudo-classes are used for matching @flags, so the two
 * following rules:
 * <programlisting>
 * GtkTreeView row:nth-child (even) { ... }
 * GtkTreeView row:nth-child (odd) { ... }
 * </programlisting>
 *
 * would apply to even and odd rows, respectively.
 *
 * <note><para>Region names must only contain lowercase letters
 * and '-', starting always with a lowercase letter.</para></note>
 *
 * Since: 3.0
 **/
void
gtk_style_context_add_region (GtkStyleContext *context,
                              const gchar     *region_name,
                              GtkRegionFlags   flags)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark region_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (region_name != NULL);
  g_return_if_fail (_gtk_style_context_check_region_name (region_name));

  priv = context->priv;
  region_quark = g_quark_from_string (region_name);

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  if (!region_find (info->regions, region_quark, &position))
    {
      GtkRegion region;

      region.class_quark = region_quark;
      region.flags = flags;

      g_array_insert_val (info->regions, position, region);

      /* Unset current data, as it likely changed due to the region change */
      priv->current_data = NULL;
    }
}

/**
 * gtk_style_context_remove_region:
 * @context: a #GtkStyleContext
 * @region_name: region name to unset
 *
 * Removes a region from @context.
 *
 * Since: 3.0
 **/
void
gtk_style_context_remove_region (GtkStyleContext *context,
                                 const gchar     *region_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark region_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (region_name != NULL);

  region_quark = g_quark_try_string (region_name);

  if (!region_quark)
    return;

  priv = context->priv;

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  if (region_find (info->regions, region_quark, &position))
    {
      g_array_remove_index (info->regions, position);

      /* Unset current data, as it likely changed due to the region change */
      priv->current_data = NULL;
    }
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
 **/
gboolean
gtk_style_context_has_region (GtkStyleContext *context,
                              const gchar     *region_name,
                              GtkRegionFlags  *flags_return)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark region_quark;
  guint position;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (region_name != NULL, FALSE);

  if (flags_return)
    *flags_return = 0;

  region_quark = g_quark_try_string (region_name);

  if (!region_quark)
    return FALSE;

  priv = context->priv;

  g_assert (priv->info_stack != NULL);
  info = priv->info_stack->data;

  if (region_find (info->regions, region_quark, &position))
    {
      if (flags_return)
        {
          GtkRegion *region;

          region = &g_array_index (info->regions, GtkRegion, position);
          *flags_return = region->flags;
        }
      return TRUE;
    }

  return FALSE;
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

  if (val1->state != val2->state)
    return val1->state < val2->state ? -1 : 1;

  return 0;
}

const GValue *
_gtk_style_context_peek_style_property (GtkStyleContext *context,
                                        GType            widget_type,
                                        GtkStateFlags    state,
                                        GParamSpec      *pspec)
{
  GtkStyleContextPrivate *priv;
  PropertyValue *pcache, key = { 0 };
  GList *global_list = NULL;
  StyleData *data;
  guint i;

  priv = context->priv;
  data = style_data_lookup (context);

  key.widget_type = widget_type;
  key.state = state;
  key.pspec = pspec;

  /* need value cache array */
  if (!data->property_cache)
    data->property_cache = g_array_new (FALSE, FALSE, sizeof (PropertyValue));
  else
    {
      pcache = bsearch (&key,
                        data->property_cache->data, data->property_cache->len,
                        sizeof (PropertyValue), style_property_values_cmp);
      if (pcache)
        return &pcache->value;
    }

  i = 0;
  while (i < data->property_cache->len &&
         style_property_values_cmp (&key, &g_array_index (data->property_cache, PropertyValue, i)) >= 0)
    i++;

  g_array_insert_val (data->property_cache, i, key);
  pcache = &g_array_index (data->property_cache, PropertyValue, i);

  /* cache miss, initialize value type, then set contents */
  g_param_spec_ref (pcache->pspec);
  g_value_init (&pcache->value, G_PARAM_SPEC_VALUE_TYPE (pspec));

  if (priv->screen)
    {
      global_list = g_object_get_qdata (G_OBJECT (priv->screen), provider_list_quark);
      global_list = g_list_last (global_list);
    }

  if (priv->widget_path)
    {
      GList *list, *global, *elem;

      list = priv->providers_last;
      global = global_list;

      while ((elem = find_next_candidate (list, global, FALSE)) != NULL)
        {
          GtkStyleProviderData *provider_data;

          provider_data = elem->data;

          if (elem == list)
            list = list->prev;
          else
            global = global->prev;

          if (gtk_style_provider_get_style_property (provider_data->provider,
                                                     priv->widget_path, state,
                                                     pspec, &pcache->value))
            {
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

                  if (gtk_symbolic_color_resolve (color, data->store, &rgba))
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

              return &pcache->value;
            }
        }
    }

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
  GtkStyleContextPrivate *priv;
  GtkWidgetClass *widget_class;
  GtkStateFlags state;
  GParamSpec *pspec;
  const GValue *peek_value;
  GType widget_type;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (value != NULL);

  priv = context->priv;

  if (!priv->widget_path)
    return;

  widget_type = gtk_widget_path_get_object_type (priv->widget_path);

  if (!g_type_is_a (widget_type, GTK_TYPE_WIDGET))
    {
      g_warning ("%s: can't get style properties for non-widget class `%s'",
                 G_STRLOC,
                 g_type_name (widget_type));
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

  state = gtk_style_context_get_state (context);
  peek_value = _gtk_style_context_peek_style_property (context, widget_type,
                                                       state, pspec);

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
  GtkStyleContextPrivate *priv;
  const gchar *prop_name;
  GtkStateFlags state;
  GType widget_type;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  prop_name = va_arg (args, const gchar *);
  priv = context->priv;

  if (!priv->widget_path)
    return;

  widget_type = gtk_widget_path_get_object_type (priv->widget_path);

  if (!g_type_is_a (widget_type, GTK_TYPE_WIDGET))
    {
      g_warning ("%s: can't get style properties for non-widget class `%s'",
                 G_STRLOC,
                 g_type_name (widget_type));
      return;
    }

  state = gtk_style_context_get_state (context);

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
          continue;
        }

      peek_value = _gtk_style_context_peek_style_property (context, widget_type,
                                                           state, pspec);

      G_VALUE_LCOPY (peek_value, args, 0, &error);

      if (error)
        {
          g_warning ("can't retrieve style property `%s' of type `%s': %s",
                     pspec->name,
                     G_VALUE_TYPE_NAME (peek_value),
                     error);
          g_free (error);
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
 **/
GtkIconSet *
gtk_style_context_lookup_icon_set (GtkStyleContext *context,
                                   const gchar     *stock_id)
{
  GtkStyleContextPrivate *priv;
  StyleData *data;
  GSList *list;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);

  priv = context->priv;
  g_return_val_if_fail (priv->widget_path != NULL, NULL);

  data = style_data_lookup (context);

  for (list = data->icon_factories; list; list = list->next)
    {
      GtkIconFactory *factory;
      GtkIconSet *icon_set;

      factory = list->data;
      icon_set = gtk_icon_factory_lookup (factory, stock_id);

      if (icon_set)
        return icon_set;
    }

  return gtk_icon_factory_lookup_default (stock_id);
}

/**
 * gtk_style_context_set_screen:
 * @context: a #GtkStyleContext
 * @screen: a #GdkScreen
 *
 * Attaches @context to the given screen.
 *
 * The screen is used to add style information from 'global' style
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

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_SCREEN (screen));

  priv = context->priv;
  if (priv->screen == screen)
    return;

  priv->screen = screen;

  g_object_notify (G_OBJECT (context), "screen");

  gtk_style_context_invalidate (context);
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
 **/
void
gtk_style_context_set_direction (GtkStyleContext  *context,
                                 GtkTextDirection  direction)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  priv->direction = direction;

  g_object_notify (G_OBJECT (context), "direction");
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
 **/
GtkTextDirection
gtk_style_context_get_direction (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), GTK_TEXT_DIR_LTR);

  priv = context->priv;
  return priv->direction;
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
 * by theming engines.
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
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  info = priv->info_stack->data;
  info->junction_sides = sides;
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
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  priv = context->priv;
  info = priv->info_stack->data;
  return info->junction_sides;
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
  GtkStyleContextPrivate *priv;
  GtkSymbolicColor *sym_color;
  StyleData *data;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (color_name != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  priv = context->priv;
  g_return_val_if_fail (priv->widget_path != NULL, FALSE);

  data = style_data_lookup (context);
  sym_color = gtk_style_properties_lookup_color (data->store, color_name);

  if (!sym_color)
    return FALSE;

  return gtk_symbolic_color_resolve (sym_color, data->store, color);
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
 * <programlisting>
 * gtk_style_context_notify_state_change (context,
 *                                        gtk_widget_get_window (widget),
 *                                        NULL,
 *                                        GTK_STATE_PRELIGHT,
 *                                        button->in_button);
 * </programlisting>
 *
 * Can be handled in the CSS file like this:
 * <programlisting>
 * GtkButton {
 *     background-color: &num;f00
 * }
 *
 * GtkButton:hover {
 *     background-color: &num;fff;
 *     transition: 200ms linear
 * }
 * </programlisting>
 *
 * This combination will animate the button background from red to white
 * if a pointer enters the button, and back to red if the pointer leaves
 * the button.
 *
 * Note that @state is used when finding the transition parameters, which
 * is why the style places the transition under the :hover pseudo-class.
 *
 * Since: 3.0
 **/
void
gtk_style_context_notify_state_change (GtkStyleContext *context,
                                       GdkWindow       *window,
                                       gpointer         region_id,
                                       GtkStateType     state,
                                       gboolean         state_value)
{
  GtkStyleContextPrivate *priv;
  GtkAnimationDescription *desc;
  AnimationInfo *info;
  GtkStateFlags flags;
  StyleData *data;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (state > GTK_STATE_NORMAL && state <= GTK_STATE_FOCUSED);

  priv = context->priv;
  g_return_if_fail (priv->widget_path != NULL);

  state_value = (state_value == TRUE);

  switch (state)
    {
    case GTK_STATE_ACTIVE:
      flags = GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags = GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags = GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags = GTK_STATE_FLAG_INSENSITIVE;
      break;
    case GTK_STATE_INCONSISTENT:
      flags = GTK_STATE_FLAG_INCONSISTENT;
      break;
    case GTK_STATE_FOCUSED:
      flags = GTK_STATE_FLAG_FOCUSED;
      break;
    case GTK_STATE_NORMAL:
    default:
      flags = 0;
      break;
    }

  /* Find out if there is any animation description for the given
   * state, it will fallback to the normal state as well if necessary.
   */
  data = style_data_lookup (context);
  gtk_style_properties_get (data->store, flags,
                            "transition", &desc,
                            NULL);

  if (!desc)
    return;

  if (_gtk_animation_description_get_duration (desc) == 0)
    {
      _gtk_animation_description_unref (desc);
      return;
    }

  info = animation_info_lookup (context, region_id, state);

  if (info &&
      info->target_value != state_value)
    {
      /* Target values are the opposite */
      if (!_gtk_timeline_get_loop (info->timeline))
        {
          /* Reverse the animation */
          if (_gtk_timeline_get_direction (info->timeline) == GTK_TIMELINE_DIRECTION_FORWARD)
            _gtk_timeline_set_direction (info->timeline, GTK_TIMELINE_DIRECTION_BACKWARD);
          else
            _gtk_timeline_set_direction (info->timeline, GTK_TIMELINE_DIRECTION_FORWARD);

          info->target_value = state_value;
        }
      else
        {
          /* Take it out of its looping state */
          _gtk_timeline_set_loop (info->timeline, FALSE);
        }
    }
  else if (!info &&
           (!_gtk_animation_description_get_loop (desc) ||
            state_value))
    {
      info = animation_info_new (context, region_id,
                                 _gtk_animation_description_get_duration (desc),
                                 _gtk_animation_description_get_progress_type (desc),
                                 _gtk_animation_description_get_loop (desc),
                                 state, state_value, window);

      priv->animations = g_slist_prepend (priv->animations, info);
      priv->animations_invalidated = TRUE;
    }

  _gtk_animation_description_unref (desc);
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
 **/
void
gtk_style_context_cancel_animations (GtkStyleContext *context,
                                     gpointer         region_id)
{
  GtkStyleContextPrivate *priv;
  AnimationInfo *info;
  GSList *l;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  l = priv->animations;

  while (l)
    {
      info = l->data;
      l = l->next;

      if (!region_id ||
          info->region_id == region_id ||
          g_slist_find (info->parent_regions, region_id))
        {
          priv->animations = g_slist_remove (priv->animations, info);
          animation_info_free (info);
        }
    }
}

static gboolean
is_parent_of (GdkWindow *parent,
              GdkWindow *child)
{
  GtkWidget *child_widget, *parent_widget;
  GdkWindow *window;

  gdk_window_get_user_data (child, (gpointer *) &child_widget);
  gdk_window_get_user_data (parent, (gpointer *) &parent_widget);

  if (child_widget != parent_widget &&
      !gtk_widget_is_ancestor (child_widget, parent_widget))
    return FALSE;

  window = child;

  while (window)
    {
      if (window == parent)
        return TRUE;

      window = gdk_window_get_parent (window);
    }

  return FALSE;
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
 **/
void
gtk_style_context_scroll_animations (GtkStyleContext *context,
                                     GdkWindow       *window,
                                     gint             dx,
                                     gint             dy)
{
  GtkStyleContextPrivate *priv;
  AnimationInfo *info;
  GSList *l;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_WINDOW (window));

  priv = context->priv;
  l = priv->animations;

  while (l)
    {
      info = l->data;
      l = l->next;

      if (info->invalidation_region &&
          (window == info->window ||
           is_parent_of (window, info->window)))
        cairo_region_translate (info->invalidation_region, dx, dy);
    }
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
 * The @region_id used must be unique in @context so the theming engine
 * can uniquely identify rendered elements subject to a state transition.
 *
 * Since: 3.0
 **/
void
gtk_style_context_push_animatable_region (GtkStyleContext *context,
                                          gpointer         region_id)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (region_id != NULL);

  priv = context->priv;
  priv->animation_regions = g_slist_prepend (priv->animation_regions, region_id);
}

/**
 * gtk_style_context_pop_animatable_region:
 * @context: a #GtkStyleContext
 *
 * Pops an animatable region from @context.
 * See gtk_style_context_push_animatable_region().
 *
 * Since: 3.0
 **/
void
gtk_style_context_pop_animatable_region (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  priv->animation_regions = g_slist_delete_link (priv->animation_regions,
                                                 priv->animation_regions);
}

void
_gtk_style_context_invalidate_animation_areas (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GSList *l;

  priv = context->priv;

  for (l = priv->animations; l; l = l->next)
    {
      AnimationInfo *info;

      info = l->data;

      /* A NULL invalidation region means it has to be recreated on
       * the next expose event, this happens usually after a widget
       * allocation change, so the next expose after it will update
       * the invalidation region.
       */
      if (info->invalidation_region)
        {
          cairo_region_destroy (info->invalidation_region);
          info->invalidation_region = NULL;
        }
    }

  priv->animations_invalidated = TRUE;
}

void
_gtk_style_context_coalesce_animation_areas (GtkStyleContext *context,
					     GtkWidget       *widget)
{
  GtkStyleContextPrivate *priv;
  GSList *l;

  priv = context->priv;

  if (!priv->animations_invalidated)
    return;

  l = priv->animations;

  while (l)
    {
      AnimationInfo *info;
      gint rel_x, rel_y;
      GSList *cur;
      guint i;

      cur = l;
      info = cur->data;
      l = l->next;

      if (info->invalidation_region)
        continue;

      if (info->rectangles->len == 0)
        continue;

      info->invalidation_region = cairo_region_create ();
      _gtk_widget_get_translation_to_window (widget, info->window, &rel_x, &rel_y);

      for (i = 0; i < info->rectangles->len; i++)
        {
          cairo_rectangle_int_t *rect;

          rect = &g_array_index (info->rectangles, cairo_rectangle_int_t, i);

	  /* These are widget relative coordinates,
	   * so have them inverted to be window relative
	   */
          rect->x -= rel_x;
          rect->y -= rel_y;

          cairo_region_union_rectangle (info->invalidation_region, rect);
        }

      g_array_remove_range (info->rectangles, 0, info->rectangles->len);
    }

  priv->animations_invalidated = FALSE;
}

static void
store_animation_region (GtkStyleContext *context,
                        gdouble          x,
                        gdouble          y,
                        gdouble          width,
                        gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GSList *l;

  priv = context->priv;

  if (!priv->animations_invalidated)
    return;

  for (l = priv->animations; l; l = l->next)
    {
      AnimationInfo *info;

      info = l->data;

      /* The animation doesn't need updating
       * the invalidation area, bail out.
       */
      if (info->invalidation_region)
        continue;

      if (context_has_animatable_region (context, info->region_id))
        {
          cairo_rectangle_int_t rect;

          rect.x = (gint) x;
          rect.y = (gint) y;
          rect.width = (gint) width;
          rect.height = (gint) height;

          g_array_append_val (info->rectangles, rect);

          if (!info->parent_regions)
            {
              GSList *parent_regions;

              parent_regions = g_slist_find (priv->animation_regions, info->region_id);
              info->parent_regions = g_slist_copy (parent_regions);
            }
        }
    }
}

/**
 * gtk_style_context_invalidate:
 * @context: a #GtkStyleContext.
 *
 * Invalidates @context style information, so it will be reconstructed
 * again.
 *
 * If you're using a #GtkStyleContext returned from
 * gtk_widget_get_style_context(), you do not need to
 * call this yourself.
 *
 * Since: 3.0
 **/
void
gtk_style_context_invalidate (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  /* Avoid reentrancy */
  if (priv->invalidating_context)
    return;

  priv->invalidating_context = TRUE;

  g_hash_table_remove_all (priv->style_data);
  priv->current_data = NULL;

  g_signal_emit (context, signals[CHANGED], 0);

  priv->invalidating_context = FALSE;
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
  GtkStateFlags state;
  cairo_pattern_t *pattern;
  GdkRGBA *color;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_WINDOW (window));

  state = gtk_style_context_get_state (context);
  gtk_style_context_get (context, state,
                         "background-image", &pattern,
                         NULL);
  if (pattern)
    {
      gdk_window_set_background_pattern (window, pattern);
      cairo_pattern_destroy (pattern);
      return;
    }

  gtk_style_context_get (context, state,
                         "background-color", &color,
                         NULL);
  if (color)
    {
      gdk_window_set_background_rgba (window, color);
      gdk_rgba_free (color);
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
  GtkStyleContextPrivate *priv;
  StyleData *data;
  const GValue *value;
  GdkRGBA *c;

  g_return_if_fail (color != NULL);
  *color = fallback_color;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  g_return_if_fail (priv->widget_path != NULL);

  data = style_data_lookup (context);
  value = _gtk_style_properties_peek_property (data->store,
                                               "color", state);

  if (value)
    {
      c = g_value_get_boxed (value);
      *color = *c;
    }
}

/**
 * gtk_style_context_get_background_color:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the color for
 * @color: (out): return value for the background color
 *
 * Gets the background color for a given state.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_background_color (GtkStyleContext *context,
                                        GtkStateFlags    state,
                                        GdkRGBA         *color)
{
  GtkStyleContextPrivate *priv;
  StyleData *data;
  const GValue *value;
  GdkRGBA *c;

  g_return_if_fail (color != NULL);
  *color = fallback_color;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  g_return_if_fail (priv->widget_path != NULL);

  data = style_data_lookup (context);
  value = _gtk_style_properties_peek_property (data->store,
                                               "background-color", state);

  if (value)
    {
      c = g_value_get_boxed (value);
      *color = *c;
    }
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
 **/
void
gtk_style_context_get_border_color (GtkStyleContext *context,
                                    GtkStateFlags    state,
                                    GdkRGBA         *color)
{
  GtkStyleContextPrivate *priv;
  StyleData *data;
  const GValue *value;
  GdkRGBA *c;

  g_return_if_fail (color != NULL);
  *color = fallback_color;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  g_return_if_fail (priv->widget_path != NULL);

  data = style_data_lookup (context);
  value = _gtk_style_properties_peek_property (data->store,
                                               "border-color", state);

  if (value)
    {
      c = g_value_get_boxed (value);
      *color = *c;
    }
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
  GtkStyleContextPrivate *priv;
  StyleData *data;
  const GValue *value;
  GtkBorder *b;

  g_return_if_fail (border != NULL);
  *border = fallback_border;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  g_return_if_fail (priv->widget_path != NULL);

  data = style_data_lookup (context);
  value = _gtk_style_properties_peek_property (data->store,
                                               "border-width", state);

  if (value)
    {
      b = g_value_get_boxed (value);
      *border = *b;
    }
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
  GtkStyleContextPrivate *priv;
  StyleData *data;
  const GValue *value;
  GtkBorder *b;

  g_return_if_fail (padding != NULL);
  *padding = fallback_border;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  g_return_if_fail (priv->widget_path != NULL);

  data = style_data_lookup (context);
  value = _gtk_style_properties_peek_property (data->store,
                                               "padding", state);

  if (value)
    {
      b = g_value_get_boxed (value);
      *padding = *b;
    }
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
  GtkStyleContextPrivate *priv;
  StyleData *data;
  const GValue *value;
  GtkBorder *b;

  g_return_if_fail (margin != NULL);
  *margin = fallback_border;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  g_return_if_fail (priv->widget_path != NULL);

  data = style_data_lookup (context);
  value = _gtk_style_properties_peek_property (data->store,
                                               "margin", state);

  if (value)
    {
      b = g_value_get_boxed (value);
      *margin = *b;
    }
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
 **/
const PangoFontDescription *
gtk_style_context_get_font (GtkStyleContext *context,
                            GtkStateFlags    state)
{
  GtkStyleContextPrivate *priv;
  StyleData *data;
  const GValue *value;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;
  g_return_val_if_fail (priv->widget_path != NULL, NULL);

  data = style_data_lookup (context);
  value = _gtk_style_properties_peek_property (data->store, "font", state);

  if (value)
    return g_value_get_boxed (value);

  return NULL;
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
      color->red = style_color->red / 65535;
      color->green = style_color->green / 65535;
      color->blue = style_color->blue / 65535;
      color->alpha = 1;

      gdk_color_free (style_color);
    }
  else
    {
      gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, color);

      if (!primary)
      {
        GdkRGBA bg;

        gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg);

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

/* Paint methods */

/**
 * gtk_render_check:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a checkmark (as in a #GtkCheckButton).
 *
 * The %GTK_STATE_FLAG_ACTIVE state determines whether the check is
 * on or off, and %GTK_STATE_FLAG_INCONSISTENT determines whether it
 * should be marked as undefined.
 *
 * <example>
 * <title>Typical checkmark rendering</title>
 * <inlinegraphic fileref="checks.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_check (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  store_animation_region (context, x, y, width, height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_check (priv->theming_engine, cr,
                              x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_option:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an option mark (as in a #GtkRadioButton), the %GTK_STATE_FLAG_ACTIVE
 * state will determine whether the option is on or off, and
 * %GTK_STATE_FLAG_INCONSISTENT whether it should be marked as undefined.
 *
 * <example>
 * <title>Typical option mark rendering</title>
 * <inlinegraphic fileref="options.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_option (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  store_animation_region (context, x, y, width, height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_option (priv->theming_engine, cr,
                               x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_arrow:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @angle: arrow angle from 0 to 2 * %G_PI, being 0 the arrow pointing to the north
 * @x: Center X for the render area
 * @y: Center Y for the render area
 * @size: square side for render area
 *
 * Renders an arrow pointing to @angle.
 *
 * <example>
 * <title>Typical arrow rendering at 0, 1&solidus;2 &pi;, &pi; and 3&solidus;2 &pi;</title>
 * <inlinegraphic fileref="arrows.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_arrow (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          angle,
                  gdouble          x,
                  gdouble          y,
                  gdouble          size)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (size > 0);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  store_animation_region (context, x, y, size, size);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_arrow (priv->theming_engine, cr,
                              angle, x, y, size);

  cairo_restore (cr);
}

/**
 * gtk_render_background:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders the background of an element.
 *
 * <example>
 * <title>Typical background rendering, showing the effect of
 * <parameter>background-image</parameter>,
 * <parameter>border-width</parameter> and
 * <parameter>border-radius</parameter></title>
 * <inlinegraphic fileref="background.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0.
 **/
void
gtk_render_background (GtkStyleContext *context,
                       cairo_t         *cr,
                       gdouble          x,
                       gdouble          y,
                       gdouble          width,
                       gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  store_animation_region (context, x, y, width, height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_background (priv->theming_engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_frame:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a frame around the rectangle defined by @x, @y, @width, @height.
 *
 * <example>
 * <title>Examples of frame rendering, showing the effect of
 * <parameter>border-image</parameter>,
 * <parameter>border-color</parameter>,
 * <parameter>border-width</parameter>,
 * <parameter>border-radius</parameter> and
 * junctions</title>
 * <inlinegraphic fileref="frames.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_frame (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  store_animation_region (context, x, y, width, height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_frame (priv->theming_engine, cr, x, y, width, height);
  
  cairo_restore (cr);
}

/**
 * gtk_render_expander:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an expander (as used in #GtkTreeView and #GtkExpander) in the area
 * defined by @x, @y, @width, @height. The state %GTK_STATE_FLAG_ACTIVE
 * determines whether the expander is collapsed or expanded.
 *
 * <example>
 * <title>Typical expander rendering</title>
 * <inlinegraphic fileref="expanders.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_expander (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          x,
                     gdouble          y,
                     gdouble          width,
                     gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  store_animation_region (context, x, y, width, height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_expander (priv->theming_engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_focus:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a focus indicator on the rectangle determined by @x, @y, @width, @height.
 * <example>
 * <title>Typical focus rendering</title>
 * <inlinegraphic fileref="focus.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_focus (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  store_animation_region (context, x, y, width, height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_focus (priv->theming_engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_layout:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin
 * @y: Y origin
 * @layout: the #PangoLayout to render
 *
 * Renders @layout on the coordinates @x, @y
 *
 * Since: 3.0
 **/
void
gtk_render_layout (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   PangoLayout     *layout)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;
  PangoRectangle extents;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  pango_layout_get_extents (layout, &extents, NULL);

  store_animation_region (context,
                          x + extents.x,
                          y + extents.y,
                          extents.width,
                          extents.height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_layout (priv->theming_engine, cr, x, y, layout);

  cairo_restore (cr);
}

/**
 * gtk_render_line:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x0: X coordinate for the origin of the line
 * @y0: Y coordinate for the origin of the line
 * @x1: X coordinate for the end of the line
 * @y1: Y coordinate for the end of the line
 *
 * Renders a line from (x0, y0) to (x1, y1).
 *
 * Since: 3.0
 **/
void
gtk_render_line (GtkStyleContext *context,
                 cairo_t         *cr,
                 gdouble          x0,
                 gdouble          y0,
                 gdouble          x1,
                 gdouble          y1)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_line (priv->theming_engine, cr, x0, y0, x1, y1);

  cairo_restore (cr);
}

/**
 * gtk_render_slider:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @orientation: orientation of the slider
 *
 * Renders a slider (as in #GtkScale) in the rectangle defined by @x, @y,
 * @width, @height. @orientation defines whether the slider is vertical
 * or horizontal.
 *
 * <example>
 * <title>Typical slider rendering</title>
 * <inlinegraphic fileref="sliders.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_slider (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height,
                   GtkOrientation   orientation)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  store_animation_region (context, x, y, width, height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_slider (priv->theming_engine, cr, x, y, width, height, orientation);

  cairo_restore (cr);
}

/**
 * gtk_render_frame_gap:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @gap_side: side where the gap is
 * @xy0_gap: initial coordinate (X or Y depending on @gap_side) for the gap
 * @xy1_gap: end coordinate (X or Y depending on @gap_side) for the gap
 *
 * Renders a frame around the rectangle defined by (@x, @y, @width, @height),
 * leaving a gap on one side. @xy0_gap and @xy1_gap will mean X coordinates
 * for %GTK_POS_TOP and %GTK_POS_BOTTOM gap sides, and Y coordinates for
 * %GTK_POS_LEFT and %GTK_POS_RIGHT.
 *
 * <example>
 * <title>Typical rendering of a frame with a gap</title>
 * <inlinegraphic fileref="frame-gap.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_frame_gap (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height,
                      GtkPositionType  gap_side,
                      gdouble          xy0_gap,
                      gdouble          xy1_gap)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);
  g_return_if_fail (xy0_gap <= xy1_gap);
  g_return_if_fail (xy0_gap >= 0);

  if (gap_side == GTK_POS_LEFT ||
      gap_side == GTK_POS_RIGHT)
    g_return_if_fail (xy1_gap <= height);
  else
    g_return_if_fail (xy1_gap <= width);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  store_animation_region (context, x, y, width, height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_frame_gap (priv->theming_engine, cr,
                                  x, y, width, height, gap_side,
                                  xy0_gap, xy1_gap);

  cairo_restore (cr);
}

/**
 * gtk_render_extension:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @gap_side: side where the gap is
 *
 * Renders a extension (as in a #GtkNotebook tab) in the rectangle
 * defined by @x, @y, @width, @height. The side where the extension
 * connects to is defined by @gap_side.
 *
 * <example>
 * <title>Typical extension rendering</title>
 * <inlinegraphic fileref="extensions.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_extension (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height,
                      GtkPositionType  gap_side)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  store_animation_region (context, x, y, width, height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_extension (priv->theming_engine, cr, x, y, width, height, gap_side);

  cairo_restore (cr);
}

/**
 * gtk_render_handle:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a handle (as in #GtkHandleBox, #GtkPaned and
 * #GtkWindow<!-- -->'s resize grip), in the rectangle
 * determined by @x, @y, @width, @height.
 *
 * <example>
 * <title>Handles rendered for the paned and grip classes</title>
 * <inlinegraphic fileref="handles.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_handle (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  store_animation_region (context, x, y, width, height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_handle (priv->theming_engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_activity:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an activity area (Such as in #GtkSpinner or the
 * fill line in #GtkRange), the state %GTK_STATE_FLAG_ACTIVE
 * determines whether there is activity going on.
 *
 * Since: 3.0
 **/
void
gtk_render_activity (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          x,
                     gdouble          y,
                     gdouble          width,
                     gdouble          height)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  cairo_save (cr);

  store_animation_region (context, x, y, width, height);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  engine_class->render_activity (priv->theming_engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_icon_pixbuf:
 * @context: a #GtkStyleContext
 * @source: the #GtkIconSource specifying the icon to render
 * @size: (type int): the size to render the icon at. A size of (GtkIconSize) -1
 *        means render at the size of the source and don't scale.
 *
 * Renders the icon specified by @source at the given @size, returning the result
 * in a pixbuf.
 *
 * Returns: (transfer full): a newly-created #GdkPixbuf containing the rendered icon
 *
 * Since: 3.0
 **/
GdkPixbuf *
gtk_render_icon_pixbuf (GtkStyleContext     *context,
                        const GtkIconSource *source,
                        GtkIconSize          size)
{
  GtkStyleContextPrivate *priv;
  GtkThemingEngineClass *engine_class;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);
  g_return_val_if_fail (size == -1 || size <= GTK_ICON_SIZE_DIALOG, NULL);
  g_return_val_if_fail (source != NULL, NULL);

  priv = context->priv;
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (priv->theming_engine);

  _gtk_theming_engine_set_context (priv->theming_engine, context);
  return engine_class->render_icon_pixbuf (priv->theming_engine, source, size);
}
