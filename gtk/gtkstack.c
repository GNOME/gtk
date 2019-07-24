/*
 * Copyright (c) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtkstack.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkcontainerprivate.h"
#include "gtkprogresstrackerprivate.h"
#include "gtksettingsprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"
#include "gtksingleselection.h"
#include "gtklistlistmodelprivate.h"
#include "a11y/gtkstackaccessible.h"
#include "a11y/gtkstackaccessibleprivate.h"
#include <math.h>
#include <string.h>

/**
 * SECTION:gtkstack
 * @Short_description: A stacking container
 * @Title: GtkStack
 * @See_also: #GtkNotebook, #GtkStackSwitcher
 *
 * The GtkStack widget is a container which only shows
 * one of its children at a time. In contrast to GtkNotebook,
 * GtkStack does not provide a means for users to change the
 * visible child. Instead, the #GtkStackSwitcher widget can be
 * used with GtkStack to provide this functionality.
 *
 * Transitions between pages can be animated as slides or
 * fades. This can be controlled with gtk_stack_set_transition_type().
 * These animations respect the #GtkSettings:gtk-enable-animations
 * setting.
 *
 * GtkStack maintains a #GtkStackPage object for each added
 * child, which holds additional per-child properties. You
 * obtain the #GtkStackPage for a child with gtk_stack_get_page().
 *
 * # GtkStack as GtkBuildable
 *
 * To set child-specific properties in a .ui file, create GtkStackPage
 * objects explictly, and set the child widget as a property on it:
 * |[
 *   <object class="GtkStack" id="stack">
 *     <child>
 *       <object class="GtkStackPage">
 *         <property name="name">page1</property>
 *         <property name="title">In the beginning…</property>
 *         <property name="child">
 *           <object class="GtkLabel">
 *             <property name="label">It was dark</property>
 *           </object>
 *         </property>
 *       </object>
 *     </child>
 * ]|
 * 
 * # CSS nodes
 *
 * GtkStack has a single CSS node named stack.
 */

/**
 * GtkStackTransitionType:
 * @GTK_STACK_TRANSITION_TYPE_NONE: No transition
 * @GTK_STACK_TRANSITION_TYPE_CROSSFADE: A cross-fade
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT: Slide from left to right
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT: Slide from right to left
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_UP: Slide from bottom up
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN: Slide from top down
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT: Slide from left or right according to the children order
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN: Slide from top down or bottom up according to the order
 * @GTK_STACK_TRANSITION_TYPE_OVER_UP: Cover the old page by sliding up
 * @GTK_STACK_TRANSITION_TYPE_OVER_DOWN: Cover the old page by sliding down
 * @GTK_STACK_TRANSITION_TYPE_OVER_LEFT: Cover the old page by sliding to the left
 * @GTK_STACK_TRANSITION_TYPE_OVER_RIGHT: Cover the old page by sliding to the right
 * @GTK_STACK_TRANSITION_TYPE_UNDER_UP: Uncover the new page by sliding up
 * @GTK_STACK_TRANSITION_TYPE_UNDER_DOWN: Uncover the new page by sliding down
 * @GTK_STACK_TRANSITION_TYPE_UNDER_LEFT: Uncover the new page by sliding to the left
 * @GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT: Uncover the new page by sliding to the right
 * @GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN: Cover the old page sliding up or uncover the new page sliding down, according to order
 * @GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP: Cover the old page sliding down or uncover the new page sliding up, according to order
 * @GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT: Cover the old page sliding left or uncover the new page sliding right, according to order
 * @GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT: Cover the old page sliding right or uncover the new page sliding left, according to order
 * @GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT: Pretend the pages are sides of a cube and rotate that cube to the left
 * @GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT: Pretend the pages are sides of a cube and rotate that cube to the right
 * @GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT_RIGHT: Pretend the pages are sides of a cube and rotate that cube to the left or right according to the children order
 *
 * These enumeration values describe the possible transitions
 * between pages in a #GtkStack widget.
 *
 * New values may be added to this enumeration over time.
 */

/* TODO:
 *  filter events out events to the last_child widget during transitions
 */

struct _GtkStack {
  GtkContainer parent_instance;
};

typedef struct _GtkStackClass GtkStackClass;
struct _GtkStackClass {
  GtkContainerClass parent_class;
};

typedef struct {
  GList *children;

  GtkStackPage *visible_child;

  gboolean hhomogeneous;
  gboolean vhomogeneous;

  GtkStackTransitionType transition_type;
  guint transition_duration;

  GtkStackPage *last_visible_child;
  GskRenderNode *last_visible_node;
  GtkAllocation last_visible_surface_allocation;
  guint tick_id;
  GtkProgressTracker tracker;
  gboolean first_frame_skipped;

  gint last_visible_widget_width;
  gint last_visible_widget_height;

  gboolean interpolate_size;

  GtkStackTransitionType active_transition_type;

  GtkSelectionModel *pages;

} GtkStackPrivate;

static void gtk_stack_buildable_interface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkStack, gtk_stack, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkStack)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_stack_buildable_interface_init))
enum  {
  PROP_0,
  PROP_HOMOGENEOUS,
  PROP_HHOMOGENEOUS,
  PROP_VHOMOGENEOUS,
  PROP_VISIBLE_CHILD,
  PROP_VISIBLE_CHILD_NAME,
  PROP_TRANSITION_DURATION,
  PROP_TRANSITION_TYPE,
  PROP_TRANSITION_RUNNING,
  PROP_INTERPOLATE_SIZE,
  PROP_PAGES,
  LAST_PROP
};

enum
{
  CHILD_PROP_0,
  CHILD_PROP_CHILD,
  CHILD_PROP_NAME,
  CHILD_PROP_TITLE,
  CHILD_PROP_ICON_NAME,
  CHILD_PROP_NEEDS_ATTENTION,
  CHILD_PROP_VISIBLE,
  LAST_CHILD_PROP
};

struct _GtkStackPage {
  GObject instance;
  GtkWidget *widget;
  gchar *name;
  gchar *title;
  gchar *icon_name;
  gboolean needs_attention;
  gboolean visible;
  GtkWidget *last_focus;
};

typedef struct _GtkStackPageClass GtkStackPageClass;
struct _GtkStackPageClass {
  GObjectClass parent_class;
};

static GParamSpec *stack_props[LAST_PROP] = { NULL, };
static GParamSpec *stack_child_props[LAST_CHILD_PROP] = { NULL, };

G_DEFINE_TYPE (GtkStackPage, gtk_stack_page, G_TYPE_OBJECT)

static void
gtk_stack_page_init (GtkStackPage *page)
{
  page->visible = TRUE;
}

static void
gtk_stack_page_finalize (GObject *object)
{
  GtkStackPage *page = GTK_STACK_PAGE (object);

  g_clear_object (&page->widget);
  g_free (page->name);
  g_free (page->title);
  g_free (page->icon_name);

  if (page->last_focus)
    g_object_remove_weak_pointer (G_OBJECT (page->last_focus),
                                  (gpointer *)&page->last_focus);

  G_OBJECT_CLASS (gtk_stack_page_parent_class)->finalize (object);
}

static void
gtk_stack_page_get_property (GObject      *object,
                             guint         property_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  GtkStackPage *info = GTK_STACK_PAGE (object);

  switch (property_id)
    {
    case CHILD_PROP_CHILD:
      g_value_set_object (value, info->widget);
      break;

    case CHILD_PROP_NAME:
      g_value_set_string (value, info->name);
      break;

    case CHILD_PROP_TITLE:
      g_value_set_string (value, info->title);
      break;

    case CHILD_PROP_ICON_NAME:
      g_value_set_string (value, info->icon_name);
      break;

    case CHILD_PROP_NEEDS_ATTENTION:
      g_value_set_boolean (value, info->needs_attention);
      break;

    case CHILD_PROP_VISIBLE:
      g_value_set_boolean (value, info->visible);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_stack_page_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkStackPage *info = GTK_STACK_PAGE (object);
  GtkWidget *stack = NULL;
  GtkStackPrivate *priv = NULL;
  gchar *name;
  GList *l;

  if (info->widget)
    {
      stack = gtk_widget_get_parent (info->widget);
      if (stack)
        priv = gtk_stack_get_instance_private (GTK_STACK (stack));
    }

  switch (property_id)
    {
    case CHILD_PROP_CHILD:
      g_set_object (&info->widget, g_value_get_object (value));
      break;

    case CHILD_PROP_NAME:
      name = g_value_dup_string (value);
      for (l = priv ? priv->children : NULL; l != NULL; l = l->next)
        {
          GtkStackPage *info2 = l->data;
          if (info == info2)
            continue;
          if (g_strcmp0 (info2->name, name) == 0)
            {
              g_warning ("Duplicate child name in GtkStack: %s", name);
              break;
            }
        }

      g_free (info->name);
      info->name = name;

      g_object_notify_by_pspec (object, pspec);

      if (priv && priv->visible_child == info)
        g_object_notify_by_pspec (G_OBJECT (stack),
                                  stack_props[PROP_VISIBLE_CHILD_NAME]);

      break;

    case CHILD_PROP_TITLE:
      g_free (info->title);
      info->title = g_value_dup_string (value);
      g_object_notify_by_pspec (object, pspec);
      break;

    case CHILD_PROP_ICON_NAME:
      g_free (info->icon_name);
      info->icon_name = g_value_dup_string (value);
      g_object_notify_by_pspec (object, pspec);
      break;

    case CHILD_PROP_NEEDS_ATTENTION:
      if (info->needs_attention != g_value_get_boolean (value))
        {
          info->needs_attention = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case CHILD_PROP_VISIBLE:
      if (info->visible != g_value_get_boolean (value))
        {
          info->visible = g_value_get_boolean (value);
          if (info->widget)
            gtk_widget_set_visible (info->widget, info->visible);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}
static void
gtk_stack_page_class_init (GtkStackPageClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_stack_page_finalize;
  object_class->get_property = gtk_stack_page_get_property;
  object_class->set_property = gtk_stack_page_set_property;

  stack_child_props[CHILD_PROP_CHILD] =
    g_param_spec_object ("child",
                         P_("Child"),
                         P_("The child of the page"),
                         GTK_TYPE_WIDGET,
                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  stack_child_props[CHILD_PROP_NAME] =
    g_param_spec_string ("name",
                         P_("Name"),
                         P_("The name of the child page"),
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  stack_child_props[CHILD_PROP_TITLE] =
    g_param_spec_string ("title",
                         P_("Title"),
                         P_("The title of the child page"),
                         NULL,
                         GTK_PARAM_READWRITE);

  stack_child_props[CHILD_PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         P_("Icon name"),
                         P_("The icon name of the child page"),
                         NULL,
                         GTK_PARAM_READWRITE);

  /**
   * GtkStack:needs-attention:
   *
   * Sets a flag specifying whether the child requires the user attention.
   * This is used by the #GtkStackSwitcher to change the appearance of the
   * corresponding button when a page needs attention and it is not the
   * current one.
   */
  stack_child_props[CHILD_PROP_NEEDS_ATTENTION] =
    g_param_spec_boolean ("needs-attention",
                         P_("Needs Attention"),
                         P_("Whether this page needs attention"),
                         FALSE,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  stack_child_props[CHILD_PROP_VISIBLE] =
    g_param_spec_boolean ("visible",
                         P_("Visible"),
                         P_("Whether this page is visible"),
                         TRUE,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_CHILD_PROP, stack_child_props);
}

#define GTK_TYPE_STACK_PAGES (gtk_stack_pages_get_type ())
G_DECLARE_FINAL_TYPE (GtkStackPages, gtk_stack_pages, GTK, STACK_PAGES, GObject)

struct _GtkStackPages
{
  GObject parent_instance;
  GtkStack *stack;
};

struct _GtkStackPagesClass
{
  GObjectClass parent_class;
};

static GType
gtk_stack_pages_get_item_type (GListModel *model)
{
  return GTK_TYPE_STACK_PAGE;
}

static guint
gtk_stack_pages_get_n_items (GListModel *model)
{
  GtkStackPages *pages = GTK_STACK_PAGES (model);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (pages->stack);

  return g_list_length (priv->children);
}

static gpointer
gtk_stack_pages_get_item (GListModel *model,
                          guint       position)
{
  GtkStackPages *pages = GTK_STACK_PAGES (model);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (pages->stack);
  GtkStackPage *page;

  page = g_list_nth_data (priv->children, position);

  return g_object_ref (page);
}

static void
gtk_stack_pages_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_stack_pages_get_item_type;
  iface->get_n_items = gtk_stack_pages_get_n_items;
  iface->get_item = gtk_stack_pages_get_item;
}

static gboolean
gtk_stack_pages_is_selected (GtkSelectionModel *model,
                             guint              position)
{
  GtkStackPages *pages = GTK_STACK_PAGES (model);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (pages->stack);
  GtkStackPage *page;

  page = g_list_nth_data (priv->children, position);

  return page == priv->visible_child;
}

static void set_visible_child (GtkStack               *stack,
                               GtkStackPage           *child_info,
                               GtkStackTransitionType  transition_type,
                               guint                   transition_duration);

static gboolean
gtk_stack_pages_select_item (GtkSelectionModel *model,
                             guint              position,
                             gboolean           exclusive)
{
  GtkStackPages *pages = GTK_STACK_PAGES (model);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (pages->stack);
  GtkStackPage *page;

  page = g_list_nth_data (priv->children, position);

  set_visible_child (pages->stack, page, priv->transition_type, priv->transition_duration);

  return TRUE;
}

static void
gtk_stack_pages_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_stack_pages_is_selected;
  iface->select_item = gtk_stack_pages_select_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkStackPages, gtk_stack_pages, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_stack_pages_list_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL, gtk_stack_pages_selection_model_init))

static void
gtk_stack_pages_init (GtkStackPages *pages)
{
}

static void
gtk_stack_pages_class_init (GtkStackPagesClass *class)
{
}

static GtkStackPages *
gtk_stack_pages_new (GtkStack *stack)
{
  GtkStackPages *pages;

  pages = g_object_new (GTK_TYPE_STACK_PAGES, NULL);
  pages->stack = stack;

  return pages;
}

static void     gtk_stack_add                            (GtkContainer  *widget,
                                                          GtkWidget     *child);
static void     gtk_stack_remove                         (GtkContainer  *widget,
                                                          GtkWidget     *child);
static void     gtk_stack_forall                         (GtkContainer  *container,
                                                          GtkCallback    callback,
                                                          gpointer       callback_data);
static void     gtk_stack_compute_expand                 (GtkWidget     *widget,
                                                          gboolean      *hexpand,
                                                          gboolean      *vexpand);
static void     gtk_stack_size_allocate                  (GtkWidget     *widget,
                                                          int            width,
                                                          int            height,
                                                          int            baseline);
static void     gtk_stack_snapshot                       (GtkWidget     *widget,
                                                          GtkSnapshot   *snapshot);
static void     gtk_stack_measure                        (GtkWidget      *widget,
                                                          GtkOrientation  orientation,
                                                          int             for_size,
                                                          int            *minimum,
                                                          int            *natural,
                                                          int            *minimum_baseline,
                                                          int            *natural_baseline);
static void     gtk_stack_dispose                        (GObject       *obj);
static void     gtk_stack_finalize                       (GObject       *obj);
static void     gtk_stack_get_property                   (GObject       *object,
                                                          guint          property_id,
                                                          GValue        *value,
                                                          GParamSpec    *pspec);
static void     gtk_stack_set_property                   (GObject       *object,
                                                          guint          property_id,
                                                          const GValue  *value,
                                                          GParamSpec    *pspec);
static void     gtk_stack_unschedule_ticks               (GtkStack      *stack);


static void     gtk_stack_add_page                       (GtkStack     *stack,
                                                          GtkStackPage *page);

static void
gtk_stack_buildable_add_child (GtkBuildable *buildable,
                               GtkBuilder   *builder,
                               GObject      *child,
                               const char   *type)
{
  if (GTK_IS_STACK_PAGE (child))
    gtk_stack_add_page (GTK_STACK (buildable), GTK_STACK_PAGE (child));
  else if (GTK_IS_WIDGET (child))
    gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
  else
    g_warning ("Can't add a child of type '%s' to '%s'", G_OBJECT_TYPE_NAME (child), G_OBJECT_TYPE_NAME (buildable));
}

static void
gtk_stack_buildable_interface_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_stack_buildable_add_child;
}

static void stack_remove (GtkStack  *stack,
                          GtkWidget *child,
                          gboolean   in_dispose);

static void
remove_child (GtkWidget *child, gpointer user_data)
{
  stack_remove (GTK_STACK (user_data), child, TRUE);
}

static void
gtk_stack_dispose (GObject *obj)
{
  GtkStack *stack = GTK_STACK (obj);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->pages)
    g_list_model_items_changed (G_LIST_MODEL (priv->pages), 0, g_list_length (priv->children), 0);

  gtk_container_foreach (GTK_CONTAINER (obj), remove_child, obj);

  G_OBJECT_CLASS (gtk_stack_parent_class)->dispose (obj);
}

static void
gtk_stack_finalize (GObject *obj)
{
  GtkStack *stack = GTK_STACK (obj);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->pages)
    g_object_remove_weak_pointer (G_OBJECT (priv->pages), (gpointer *)&priv->pages);

  gtk_stack_unschedule_ticks (stack);

  g_clear_pointer (&priv->last_visible_node, gsk_render_node_unref);

  G_OBJECT_CLASS (gtk_stack_parent_class)->finalize (obj);
}

static void
gtk_stack_get_property (GObject   *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GtkStack *stack = GTK_STACK (object);

  switch (property_id)
    {
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, gtk_stack_get_homogeneous (stack));
      break;
    case PROP_HHOMOGENEOUS:
      g_value_set_boolean (value, gtk_stack_get_hhomogeneous (stack));
      break;
    case PROP_VHOMOGENEOUS:
      g_value_set_boolean (value, gtk_stack_get_vhomogeneous (stack));
      break;
    case PROP_VISIBLE_CHILD:
      g_value_set_object (value, gtk_stack_get_visible_child (stack));
      break;
    case PROP_VISIBLE_CHILD_NAME:
      g_value_set_string (value, gtk_stack_get_visible_child_name (stack));
      break;
    case PROP_TRANSITION_DURATION:
      g_value_set_uint (value, gtk_stack_get_transition_duration (stack));
      break;
    case PROP_TRANSITION_TYPE:
      g_value_set_enum (value, gtk_stack_get_transition_type (stack));
      break;
    case PROP_TRANSITION_RUNNING:
      g_value_set_boolean (value, gtk_stack_get_transition_running (stack));
      break;
    case PROP_INTERPOLATE_SIZE:
      g_value_set_boolean (value, gtk_stack_get_interpolate_size (stack));
      break;
    case PROP_PAGES:
      g_value_set_object (value, gtk_stack_get_pages (stack));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_stack_set_property (GObject     *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GtkStack *stack = GTK_STACK (object);

  switch (property_id)
    {
    case PROP_HOMOGENEOUS:
      gtk_stack_set_homogeneous (stack, g_value_get_boolean (value));
      break;
    case PROP_HHOMOGENEOUS:
      gtk_stack_set_hhomogeneous (stack, g_value_get_boolean (value));
      break;
    case PROP_VHOMOGENEOUS:
      gtk_stack_set_vhomogeneous (stack, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE_CHILD:
      gtk_stack_set_visible_child (stack, g_value_get_object (value));
      break;
    case PROP_VISIBLE_CHILD_NAME:
      gtk_stack_set_visible_child_name (stack, g_value_get_string (value));
      break;
    case PROP_TRANSITION_DURATION:
      gtk_stack_set_transition_duration (stack, g_value_get_uint (value));
      break;
    case PROP_TRANSITION_TYPE:
      gtk_stack_set_transition_type (stack, g_value_get_enum (value));
      break;
    case PROP_INTERPOLATE_SIZE:
      gtk_stack_set_interpolate_size (stack, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_stack_class_init (GtkStackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = gtk_stack_get_property;
  object_class->set_property = gtk_stack_set_property;
  object_class->dispose = gtk_stack_dispose;
  object_class->finalize = gtk_stack_finalize;

  widget_class->size_allocate = gtk_stack_size_allocate;
  widget_class->snapshot = gtk_stack_snapshot;
  widget_class->measure = gtk_stack_measure;
  widget_class->compute_expand = gtk_stack_compute_expand;

  container_class->add = gtk_stack_add;
  container_class->remove = gtk_stack_remove;
  container_class->forall = gtk_stack_forall;

  stack_props[PROP_HOMOGENEOUS] =
      g_param_spec_boolean ("homogeneous", P_("Homogeneous"), P_("Homogeneous sizing"),
                            TRUE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStack:hhomogeneous:
   *
   * %TRUE if the stack allocates the same width for all children.
   */
  stack_props[PROP_HHOMOGENEOUS] =
      g_param_spec_boolean ("hhomogeneous", P_("Horizontally homogeneous"), P_("Horizontally homogeneous sizing"),
                            TRUE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStack:vhomogeneous:
   *
   * %TRUE if the stack allocates the same height for all children.
   */
  stack_props[PROP_VHOMOGENEOUS] =
      g_param_spec_boolean ("vhomogeneous", P_("Vertically homogeneous"), P_("Vertically homogeneous sizing"),
                            TRUE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  stack_props[PROP_VISIBLE_CHILD] =
      g_param_spec_object ("visible-child", P_("Visible child"), P_("The widget currently visible in the stack"),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  stack_props[PROP_VISIBLE_CHILD_NAME] =
      g_param_spec_string ("visible-child-name", P_("Name of visible child"), P_("The name of the widget currently visible in the stack"),
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  stack_props[PROP_TRANSITION_DURATION] =
      g_param_spec_uint ("transition-duration", P_("Transition duration"), P_("The animation duration, in milliseconds"),
                         0, G_MAXUINT, 200,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  stack_props[PROP_TRANSITION_TYPE] =
      g_param_spec_enum ("transition-type", P_("Transition type"), P_("The type of animation used to transition"),
                         GTK_TYPE_STACK_TRANSITION_TYPE, GTK_STACK_TRANSITION_TYPE_NONE,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  stack_props[PROP_TRANSITION_RUNNING] =
      g_param_spec_boolean ("transition-running", P_("Transition running"), P_("Whether or not the transition is currently running"),
                            FALSE,
                            GTK_PARAM_READABLE);
  stack_props[PROP_INTERPOLATE_SIZE] =
      g_param_spec_boolean ("interpolate-size", P_("Interpolate size"), P_("Whether or not the size should smoothly change when changing between differently sized children"),
                            FALSE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  stack_props[PROP_PAGES] =
      g_param_spec_object ("pages", P_("Pages"), P_("A selection model with the stacks pages"),
                           GTK_TYPE_SELECTION_MODEL,
                           GTK_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, stack_props);


  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_STACK_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("stack"));
}

/**
 * gtk_stack_new:
 *
 * Creates a new #GtkStack container.
 *
 * Returns: a new #GtkStack
 */
GtkWidget *
gtk_stack_new (void)
{
  return g_object_new (GTK_TYPE_STACK, NULL);
}

static GtkStackPage *
find_child_info_for_widget (GtkStack  *stack,
                            GtkWidget *child)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *info;
  GList *l;

  for (l = priv->children; l != NULL; l = l->next)
    {
      info = l->data;
      if (info->widget == child)
        return info;
    }

  return NULL;
}

static inline gboolean
is_left_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_LEFT);
}

static inline gboolean
is_right_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_RIGHT);
}

static inline gboolean
is_up_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_UP ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_UP);
}

static inline gboolean
is_down_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_DOWN);
}

/* Transitions that cause the bin window to move */
static inline gboolean
is_window_moving_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_UP ||
          transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_UP ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_LEFT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_RIGHT);
}

/* Transitions that change direction depending on the relative order of the
old and new child */
static inline gboolean
is_direction_dependent_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT_RIGHT);
}

/* Returns simple transition type for a direction dependent transition, given
whether the new child (the one being switched to) is first in the stacking order
(added earlier). */
static inline GtkStackTransitionType
get_simple_transition_type (gboolean               new_child_first,
                            GtkStackTransitionType transition_type)
{
  switch (transition_type)
    {
    case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT : GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT;
    case GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT_RIGHT:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT : GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT;
    case GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN : GTK_STACK_TRANSITION_TYPE_SLIDE_UP;
    case GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_UNDER_DOWN : GTK_STACK_TRANSITION_TYPE_OVER_UP;
    case GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_UNDER_UP : GTK_STACK_TRANSITION_TYPE_OVER_DOWN;
    case GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT : GTK_STACK_TRANSITION_TYPE_OVER_LEFT;
    case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_UNDER_LEFT : GTK_STACK_TRANSITION_TYPE_OVER_RIGHT;
    case GTK_STACK_TRANSITION_TYPE_SLIDE_UP:
    case GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN:
    case GTK_STACK_TRANSITION_TYPE_OVER_UP:
    case GTK_STACK_TRANSITION_TYPE_OVER_DOWN:
    case GTK_STACK_TRANSITION_TYPE_UNDER_UP:
    case GTK_STACK_TRANSITION_TYPE_UNDER_DOWN:
    case GTK_STACK_TRANSITION_TYPE_NONE:
    case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT:
    case GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT:
    case GTK_STACK_TRANSITION_TYPE_OVER_LEFT:
    case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT:
    case GTK_STACK_TRANSITION_TYPE_UNDER_LEFT:
    case GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT:
    case GTK_STACK_TRANSITION_TYPE_CROSSFADE:
    case GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT:
    case GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT:
    default:
      return transition_type;
    }
}

static gint
get_bin_window_x (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  int width;
  int x = 0;

  width = gtk_widget_get_width (GTK_WIDGET (stack));

  if (gtk_progress_tracker_get_state (&priv->tracker) != GTK_PROGRESS_STATE_AFTER)
    {
      if (is_left_transition (priv->active_transition_type))
        x = width * (1 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
      if (is_right_transition (priv->active_transition_type))
        x = -width * (1 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
    }

  return x;
}

static gint
get_bin_window_y (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  int height;
  int y = 0;

  height = gtk_widget_get_height (GTK_WIDGET (stack));

  if (gtk_progress_tracker_get_state (&priv->tracker) != GTK_PROGRESS_STATE_AFTER)
    {
      if (is_up_transition (priv->active_transition_type))
        y = height * (1 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
      if (is_down_transition(priv->active_transition_type))
        y = -height * (1 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
    }

  return y;
}

static void
gtk_stack_progress_updated (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (!priv->vhomogeneous || !priv->hhomogeneous)
    {
      GdkSurface *surface;

      surface = gtk_native_get_surface (gtk_widget_get_native (GTK_WIDGET (stack)));
      if (!surface || gdk_surface_can_resize_now (surface))
        gtk_widget_queue_resize (GTK_WIDGET (stack));
    }
  else if (is_window_moving_transition (priv->active_transition_type))
    gtk_widget_queue_allocate (GTK_WIDGET (stack));
  else
    gtk_widget_queue_draw (GTK_WIDGET (stack));

  if (gtk_progress_tracker_get_state (&priv->tracker) == GTK_PROGRESS_STATE_AFTER)
    {
      g_clear_pointer (&priv->last_visible_node, gsk_render_node_unref);

      if (priv->last_visible_child != NULL)
        {
          gtk_widget_set_child_visible (priv->last_visible_child->widget, FALSE);
          priv->last_visible_child = NULL;
        }
    }
}

static gboolean
gtk_stack_transition_cb (GtkWidget     *widget,
                         GdkFrameClock *frame_clock,
                         gpointer       user_data)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->first_frame_skipped)
    gtk_progress_tracker_advance_frame (&priv->tracker,
                                        gdk_frame_clock_get_frame_time (frame_clock));
  else
    priv->first_frame_skipped = TRUE;

  /* Finish animation early if not mapped anymore */
  if (!gtk_widget_get_mapped (widget))
    gtk_progress_tracker_finish (&priv->tracker);

  gtk_stack_progress_updated (GTK_STACK (widget));

  if (gtk_progress_tracker_get_state (&priv->tracker) == GTK_PROGRESS_STATE_AFTER)
    {
      priv->tick_id = 0;
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_TRANSITION_RUNNING]);

      return FALSE;
    }

  return TRUE;
}

static void
gtk_stack_schedule_ticks (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->tick_id == 0)
    {
      priv->tick_id =
        gtk_widget_add_tick_callback (GTK_WIDGET (stack), gtk_stack_transition_cb, stack, NULL);
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_TRANSITION_RUNNING]);
    }
}

static void
gtk_stack_unschedule_ticks (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->tick_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (stack), priv->tick_id);
      priv->tick_id = 0;
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_TRANSITION_RUNNING]);
    }
}

static GtkStackTransitionType
effective_transition_type (GtkStack               *stack,
                           GtkStackTransitionType  transition_type)
{
  if (_gtk_widget_get_direction (GTK_WIDGET (stack)) == GTK_TEXT_DIR_RTL)
    {
      switch (transition_type)
        {
        case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT:
          return GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT:
          return GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT;
        case GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT:
          return GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT;
        case GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT:
          return GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT;
        case GTK_STACK_TRANSITION_TYPE_OVER_LEFT:
          return GTK_STACK_TRANSITION_TYPE_OVER_RIGHT;
        case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT:
          return GTK_STACK_TRANSITION_TYPE_OVER_LEFT;
        case GTK_STACK_TRANSITION_TYPE_UNDER_LEFT:
          return GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT;
        case GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT:
          return GTK_STACK_TRANSITION_TYPE_UNDER_LEFT;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_UP:
        case GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN:
        case GTK_STACK_TRANSITION_TYPE_OVER_UP:
        case GTK_STACK_TRANSITION_TYPE_OVER_DOWN:
        case GTK_STACK_TRANSITION_TYPE_UNDER_UP:
        case GTK_STACK_TRANSITION_TYPE_UNDER_DOWN:
        case GTK_STACK_TRANSITION_TYPE_NONE:
        case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT:
        case GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN:
        case GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN:
        case GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP:
        case GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT:
        case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT:
        case GTK_STACK_TRANSITION_TYPE_CROSSFADE:
        case GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT_RIGHT:
        default:
          return transition_type;
        }
    }
  else
    {
      return transition_type;
    }
}

static void
gtk_stack_start_transition (GtkStack               *stack,
                            GtkStackTransitionType  transition_type,
                            guint                   transition_duration)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkWidget *widget = GTK_WIDGET (stack);

  if (gtk_widget_get_mapped (widget) &&
      gtk_settings_get_enable_animations (gtk_widget_get_settings (widget)) &&
      transition_type != GTK_STACK_TRANSITION_TYPE_NONE &&
      transition_duration != 0 &&
      priv->last_visible_child != NULL)
    {
      priv->active_transition_type = effective_transition_type (stack, transition_type);
      priv->first_frame_skipped = FALSE;
      gtk_stack_schedule_ticks (stack);
      gtk_progress_tracker_start (&priv->tracker,
                                  priv->transition_duration * 1000,
                                  0,
                                  1.0);
    }
  else
    {
      gtk_stack_unschedule_ticks (stack);
      priv->active_transition_type = GTK_STACK_TRANSITION_TYPE_NONE;
      gtk_progress_tracker_finish (&priv->tracker);
    }

  gtk_stack_progress_updated (GTK_STACK (widget));
}

static void
set_visible_child (GtkStack               *stack,
                   GtkStackPage      *child_info,
                   GtkStackTransitionType  transition_type,
                   guint                   transition_duration)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *info;
  GtkWidget *widget = GTK_WIDGET (stack);
  GList *l;
  GtkWidget *focus;
  gboolean contains_focus = FALSE;
  guint old_pos = GTK_INVALID_LIST_POSITION;
  guint new_pos = GTK_INVALID_LIST_POSITION;

  /* if we are being destroyed, do not bother with transitions
   * and notifications
   */
  if (gtk_widget_in_destruction (widget))
    return;

  /* If none, pick first visible */
  if (child_info == NULL)
    {
      for (l = priv->children; l != NULL; l = l->next)
        {
          info = l->data;
          if (gtk_widget_get_visible (info->widget))
            {
              child_info = info;
              break;
            }
        }
    }

  if (child_info == priv->visible_child)
    return;

  if (priv->pages)
    {
      guint position;
      for (l = priv->children, position = 0; l != NULL; l = l->next, position++)
        {
          info = l->data;
          if (info == priv->visible_child)
            old_pos = position;
          else if (info == child_info)
            new_pos = position;
        }
    }

  if (gtk_widget_get_root (widget))
    focus = gtk_root_get_focus (gtk_widget_get_root (widget));
  else
    focus = NULL;
  if (focus &&
      priv->visible_child &&
      priv->visible_child->widget &&
      gtk_widget_is_ancestor (focus, priv->visible_child->widget))
    {
      contains_focus = TRUE;

      if (priv->visible_child->last_focus)
        g_object_remove_weak_pointer (G_OBJECT (priv->visible_child->last_focus),
                                      (gpointer *)&priv->visible_child->last_focus);
      priv->visible_child->last_focus = focus;
      g_object_add_weak_pointer (G_OBJECT (priv->visible_child->last_focus),
                                 (gpointer *)&priv->visible_child->last_focus);
    }

  if (priv->last_visible_child)
    gtk_widget_set_child_visible (priv->last_visible_child->widget, FALSE);
  priv->last_visible_child = NULL;

  g_clear_pointer (&priv->last_visible_node, gsk_render_node_unref);

  if (priv->visible_child && priv->visible_child->widget)
    {
      if (gtk_widget_is_visible (widget))
        {
          priv->last_visible_child = priv->visible_child;
          priv->last_visible_widget_width = gtk_widget_get_allocated_width (priv->last_visible_child->widget);
          priv->last_visible_widget_height = gtk_widget_get_allocated_height (priv->last_visible_child->widget);
        }
      else
        {
          gtk_widget_set_child_visible (priv->visible_child->widget, FALSE);
        }
    }

  gtk_stack_accessible_update_visible_child (stack,
                                             priv->visible_child ? priv->visible_child->widget : NULL,
                                             child_info ? child_info->widget : NULL);

  priv->visible_child = child_info;

  if (child_info)
    {
      gtk_widget_set_child_visible (child_info->widget, TRUE);

      if (contains_focus)
        {
          if (child_info->last_focus)
            gtk_widget_grab_focus (child_info->last_focus);
          else
            gtk_widget_child_focus (child_info->widget, GTK_DIR_TAB_FORWARD);
        }
    }

  if ((child_info == NULL || priv->last_visible_child == NULL) &&
      is_direction_dependent_transition (transition_type))
    {
      transition_type = GTK_STACK_TRANSITION_TYPE_NONE;
    }
  else if (is_direction_dependent_transition (transition_type))
    {
      gboolean i_first = FALSE;
      for (l = priv->children; l != NULL; l = l->next)
        {
	  if (child_info == l->data)
	    {
	      i_first = TRUE;
	      break;
	    }
	  if (priv->last_visible_child == l->data)
	    break;
        }

      transition_type = get_simple_transition_type (i_first, transition_type);
    }

  if (priv->hhomogeneous && priv->vhomogeneous)
    gtk_widget_queue_allocate (widget);
  else
    gtk_widget_queue_resize (widget);

  g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_VISIBLE_CHILD]);
  g_object_notify_by_pspec (G_OBJECT (stack),
                            stack_props[PROP_VISIBLE_CHILD_NAME]);

  if (priv->pages)
    {
      if (old_pos == GTK_INVALID_LIST_POSITION && new_pos == GTK_INVALID_LIST_POSITION)
        ; /* nothing to do */
      else if (old_pos == GTK_INVALID_LIST_POSITION)
        gtk_selection_model_selection_changed (priv->pages, new_pos, 1);
      else if (new_pos == GTK_INVALID_LIST_POSITION)
        gtk_selection_model_selection_changed (priv->pages, old_pos, 1);
      else
        gtk_selection_model_selection_changed (priv->pages,
                                               MIN (old_pos, new_pos),
                                               MAX (old_pos, new_pos) - MIN (old_pos, new_pos) + 1);
    }

  gtk_stack_start_transition (stack, transition_type, transition_duration);
}

static void
stack_child_visibility_notify_cb (GObject    *obj,
                                  GParamSpec *pspec,
                                  gpointer    user_data)
{
  GtkStack *stack = GTK_STACK (user_data);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkWidget *child = GTK_WIDGET (obj);
  GtkStackPage *child_info;

  child_info = find_child_info_for_widget (stack, child);

  if (priv->visible_child == NULL &&
      gtk_widget_get_visible (child))
    set_visible_child (stack, child_info, priv->transition_type, priv->transition_duration);
  else if (priv->visible_child == child_info &&
           !gtk_widget_get_visible (child))
    set_visible_child (stack, NULL, priv->transition_type, priv->transition_duration);

  if (child_info == priv->last_visible_child)
    {
      gtk_widget_set_child_visible (priv->last_visible_child->widget, FALSE);
      priv->last_visible_child = NULL;
    }
}

static void
gtk_stack_add_internal (GtkStack *stack,
                        GtkWidget  *child,
                        const char *name,
                        const char *title);

/**
 * gtk_stack_add_titled:
 * @stack: a #GtkStack
 * @child: the widget to add
 * @name: the name for @child
 * @title: a human-readable title for @child
 *
 * Adds a child to @stack.
 * The child is identified by the @name. The @title
 * will be used by #GtkStackSwitcher to represent
 * @child in a tab bar, so it should be short.
 */
void
gtk_stack_add_titled (GtkStack   *stack,
                     GtkWidget   *child,
                     const gchar *name,
                     const gchar *title)
{
  g_return_if_fail (GTK_IS_STACK (stack));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_stack_add_internal (stack, child, name, title);
}

/**
 * gtk_stack_add_named:
 * @stack: a #GtkStack
 * @child: the widget to add
 * @name: the name for @child
 *
 * Adds a child to @stack.
 * The child is identified by the @name.
 */
void
gtk_stack_add_named (GtkStack   *stack,
                    GtkWidget   *child,
                    const gchar *name)
{
  g_return_if_fail (GTK_IS_STACK (stack));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_stack_add_internal (stack, child, name, NULL);
}

static void
gtk_stack_add (GtkContainer *container,
               GtkWidget    *child)
{
  GtkStack *stack = GTK_STACK (container);

  gtk_stack_add_internal (stack, child, NULL, NULL);
}

static void
gtk_stack_add_internal (GtkStack   *stack,
                        GtkWidget  *child,
                        const char *name,
                        const char *title)
{
  GtkStackPage *child_info;

  g_return_if_fail (child != NULL);

  child_info = g_object_new (GTK_TYPE_STACK_PAGE, NULL);
  child_info->widget = g_object_ref (child);
  child_info->name = g_strdup (name);
  child_info->title = g_strdup (title);
  child_info->icon_name = NULL;
  child_info->needs_attention = FALSE;
  child_info->last_focus = NULL;

  gtk_stack_add_page (stack, child_info);

  g_object_unref (child_info);
}

static void
gtk_stack_add_page (GtkStack     *stack,
                    GtkStackPage *child_info)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GList *l;

  g_return_if_fail (child_info->widget != NULL);

  for (l = priv->children; l != NULL; l = l->next)
    {
      GtkStackPage *info = l->data;
      if (info->name &&
          g_strcmp0 (info->name, child_info->name) == 0)
        {
          g_warning ("While adding page: duplicate child name in GtkStack: %s", child_info->name);
          break;
        }
    }

  priv->children = g_list_append (priv->children, g_object_ref (child_info));

  gtk_widget_set_child_visible (child_info->widget, FALSE);
  gtk_widget_set_parent (child_info->widget, GTK_WIDGET (stack));

  if (priv->pages)
    g_list_model_items_changed (G_LIST_MODEL (priv->pages), g_list_length (priv->children) - 1, 0, 1);

  g_signal_connect (child_info->widget, "notify::visible",
                    G_CALLBACK (stack_child_visibility_notify_cb), stack);

  if (priv->visible_child == NULL &&
      gtk_widget_get_visible (child_info->widget))
    set_visible_child (stack, child_info, priv->transition_type, priv->transition_duration);

  if (priv->hhomogeneous || priv->vhomogeneous || priv->visible_child == child_info)
    gtk_widget_queue_resize (GTK_WIDGET (stack));
}

static void
stack_remove (GtkStack  *stack,
              GtkWidget *child,
              gboolean   in_dispose)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *child_info;
  gboolean was_visible;

  child_info = find_child_info_for_widget (stack, child);
  if (child_info == NULL)
    return;

  priv->children = g_list_remove (priv->children, child_info);
  
  g_signal_handlers_disconnect_by_func (child,
                                        stack_child_visibility_notify_cb,
                                        stack);

  was_visible = gtk_widget_get_visible (child);

  g_clear_object (&child_info->widget);

  if (priv->visible_child == child_info)
    {
      if (in_dispose)
        priv->visible_child = NULL;
      else
        set_visible_child (stack, NULL, priv->transition_type, priv->transition_duration);
    }

  if (priv->last_visible_child == child_info)
    priv->last_visible_child = NULL;

  gtk_widget_unparent (child);

  g_object_unref (child_info);

  if (!in_dispose &&
      (priv->hhomogeneous || priv->vhomogeneous) &&
      was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (stack));
}

static void
gtk_stack_remove (GtkContainer *container,
                  GtkWidget    *child)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (GTK_STACK (container));
  GList *l;
  guint position;

  for (l = priv->children, position = 0; l; l = l->next, position++)
    {
      GtkStackPage *page = l->data;
      if (page->widget == child)
        break;
    }

  stack_remove (GTK_STACK (container), child, FALSE);

  if (priv->pages)
    g_list_model_items_changed (G_LIST_MODEL (priv->pages), position, 1, 0);
}

/**
 * gtk_stack_get_page:
 * @stack: a #GtkStack
 * @child: a child of @stack
 *
 * Returns the #GtkStackPage object for @child.
 * 
 * Returns: (transfer none): the #GtkStackPage for @child
 */
GtkStackPage *
gtk_stack_get_page (GtkStack  *stack,
                    GtkWidget *child)
{
  return find_child_info_for_widget (stack, child);
}

/**
 * gtk_stack_get_child_by_name:
 * @stack: a #GtkStack
 * @name: the name of the child to find
 *
 * Finds the child of the #GtkStack with the name given as
 * the argument. Returns %NULL if there is no child with this
 * name.
 *
 * Returns: (transfer none) (nullable): the requested child of the #GtkStack
 */
GtkWidget *
gtk_stack_get_child_by_name (GtkStack    *stack,
                             const gchar *name)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *info;
  GList *l;

  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (l = priv->children; l != NULL; l = l->next)
    {
      info = l->data;
      if (info->name && strcmp (info->name, name) == 0)
        return info->widget;
    }

  return NULL;
}

/**
 * gtk_stack_page_get_child:
 * @page: a #GtkStackPage
 *
 * Returns the stack child to which @page belongs.
 *
 * Returns: (transfer none): the child to which @page belongs
 */
GtkWidget *
gtk_stack_page_get_child (GtkStackPage *page)
{
  return page->widget;
}

/**
 * gtk_stack_set_homogeneous:
 * @stack: a #GtkStack
 * @homogeneous: %TRUE to make @stack homogeneous
 *
 * Sets the #GtkStack to be homogeneous or not. If it
 * is homogeneous, the #GtkStack will request the same
 * size for all its children. If it isn't, the stack
 * may change size when a different child becomes visible.
 *
 * Homogeneity can be controlled separately
 * for horizontal and vertical size, with the
 * #GtkStack:hhomogeneous and #GtkStack:vhomogeneous.
 */
void
gtk_stack_set_homogeneous (GtkStack *stack,
                           gboolean  homogeneous)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  homogeneous = !!homogeneous;

  if ((priv->hhomogeneous && priv->vhomogeneous) == homogeneous)
    return;

  g_object_freeze_notify (G_OBJECT (stack));

  if (priv->hhomogeneous != homogeneous)
    {
      priv->hhomogeneous = homogeneous;
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_HHOMOGENEOUS]);
    }

  if (priv->vhomogeneous != homogeneous)
    {
      priv->vhomogeneous = homogeneous;
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_VHOMOGENEOUS]);
    }

  if (gtk_widget_get_visible (GTK_WIDGET(stack)))
    gtk_widget_queue_resize (GTK_WIDGET (stack));

  g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_HOMOGENEOUS]);
  g_object_thaw_notify (G_OBJECT (stack));
}

/**
 * gtk_stack_get_homogeneous:
 * @stack: a #GtkStack
 *
 * Gets whether @stack is homogeneous.
 * See gtk_stack_set_homogeneous().
 *
 * Returns: whether @stack is homogeneous.
 */
gboolean
gtk_stack_get_homogeneous (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return priv->hhomogeneous && priv->vhomogeneous;
}

/**
 * gtk_stack_set_hhomogeneous:
 * @stack: a #GtkStack
 * @hhomogeneous: %TRUE to make @stack horizontally homogeneous
 *
 * Sets the #GtkStack to be horizontally homogeneous or not.
 * If it is homogeneous, the #GtkStack will request the same
 * width for all its children. If it isn't, the stack
 * may change width when a different child becomes visible.
 */
void
gtk_stack_set_hhomogeneous (GtkStack *stack,
                            gboolean  hhomogeneous)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  hhomogeneous = !!hhomogeneous;

  if (priv->hhomogeneous == hhomogeneous)
    return;

  priv->hhomogeneous = hhomogeneous;

  if (gtk_widget_get_visible (GTK_WIDGET(stack)))
    gtk_widget_queue_resize (GTK_WIDGET (stack));

  g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_HHOMOGENEOUS]);
}

/**
 * gtk_stack_get_hhomogeneous:
 * @stack: a #GtkStack
 *
 * Gets whether @stack is horizontally homogeneous.
 * See gtk_stack_set_hhomogeneous().
 *
 * Returns: whether @stack is horizontally homogeneous.
 */
gboolean
gtk_stack_get_hhomogeneous (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return priv->hhomogeneous;
}

/**
 * gtk_stack_set_vhomogeneous:
 * @stack: a #GtkStack
 * @vhomogeneous: %TRUE to make @stack vertically homogeneous
 *
 * Sets the #GtkStack to be vertically homogeneous or not.
 * If it is homogeneous, the #GtkStack will request the same
 * height for all its children. If it isn't, the stack
 * may change height when a different child becomes visible.
 */
void
gtk_stack_set_vhomogeneous (GtkStack *stack,
                            gboolean  vhomogeneous)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  vhomogeneous = !!vhomogeneous;

  if (priv->vhomogeneous == vhomogeneous)
    return;

  priv->vhomogeneous = vhomogeneous;

  if (gtk_widget_get_visible (GTK_WIDGET(stack)))
    gtk_widget_queue_resize (GTK_WIDGET (stack));

  g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_VHOMOGENEOUS]);
}

/**
 * gtk_stack_get_vhomogeneous:
 * @stack: a #GtkStack
 *
 * Gets whether @stack is vertically homogeneous.
 * See gtk_stack_set_vhomogeneous().
 *
 * Returns: whether @stack is vertically homogeneous.
 */
gboolean
gtk_stack_get_vhomogeneous (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return priv->vhomogeneous;
}

/**
 * gtk_stack_get_transition_duration:
 * @stack: a #GtkStack
 *
 * Returns the amount of time (in milliseconds) that
 * transitions between pages in @stack will take.
 *
 * Returns: the transition duration
 */
guint
gtk_stack_get_transition_duration (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), 0);

  return priv->transition_duration;
}

/**
 * gtk_stack_set_transition_duration:
 * @stack: a #GtkStack
 * @duration: the new duration, in milliseconds
 *
 * Sets the duration that transitions between pages in @stack
 * will take.
 */
void
gtk_stack_set_transition_duration (GtkStack *stack,
                                   guint     duration)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  if (priv->transition_duration == duration)
    return;

  priv->transition_duration = duration;
  g_object_notify_by_pspec (G_OBJECT (stack),
                            stack_props[PROP_TRANSITION_DURATION]);
}

/**
 * gtk_stack_get_transition_type:
 * @stack: a #GtkStack
 *
 * Gets the type of animation that will be used
 * for transitions between pages in @stack.
 *
 * Returns: the current transition type of @stack
 */
GtkStackTransitionType
gtk_stack_get_transition_type (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), GTK_STACK_TRANSITION_TYPE_NONE);

  return priv->transition_type;
}

/**
 * gtk_stack_set_transition_type:
 * @stack: a #GtkStack
 * @transition: the new transition type
 *
 * Sets the type of animation that will be used for
 * transitions between pages in @stack. Available
 * types include various kinds of fades and slides.
 *
 * The transition type can be changed without problems
 * at runtime, so it is possible to change the animation
 * based on the page that is about to become current.
 */
void
gtk_stack_set_transition_type (GtkStack              *stack,
                              GtkStackTransitionType  transition)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  if (priv->transition_type == transition)
    return;

  priv->transition_type = transition;
  g_object_notify_by_pspec (G_OBJECT (stack),
                            stack_props[PROP_TRANSITION_TYPE]);
}

/**
 * gtk_stack_get_transition_running:
 * @stack: a #GtkStack
 *
 * Returns whether the @stack is currently in a transition from one page to
 * another.
 *
 * Returns: %TRUE if the transition is currently running, %FALSE otherwise.
 */
gboolean
gtk_stack_get_transition_running (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return (priv->tick_id != 0);
}

/**
 * gtk_stack_set_interpolate_size:
 * @stack: A #GtkStack
 * @interpolate_size: the new value
 *
 * Sets whether or not @stack will interpolate its size when
 * changing the visible child. If the #GtkStack:interpolate-size
 * property is set to %TRUE, @stack will interpolate its size between
 * the current one and the one it'll take after changing the
 * visible child, according to the set transition duration.
 */
void
gtk_stack_set_interpolate_size (GtkStack *stack,
                                gboolean  interpolate_size)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  g_return_if_fail (GTK_IS_STACK (stack));

  interpolate_size = !!interpolate_size;

  if (priv->interpolate_size == interpolate_size)
    return;

  priv->interpolate_size = interpolate_size;
  g_object_notify_by_pspec (G_OBJECT (stack),
                            stack_props[PROP_INTERPOLATE_SIZE]);
}

/**
 * gtk_stack_get_interpolate_size:
 * @stack: A #GtkStack
 *
 * Returns wether the #GtkStack is set up to interpolate between
 * the sizes of children on page switch.
 *
 * Returns: %TRUE if child sizes are interpolated
 */
gboolean
gtk_stack_get_interpolate_size (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return priv->interpolate_size;
}



/**
 * gtk_stack_get_visible_child:
 * @stack: a #GtkStack
 *
 * Gets the currently visible child of @stack, or %NULL if
 * there are no visible children.
 *
 * Returns: (transfer none) (nullable): the visible child of the #GtkStack
 */
GtkWidget *
gtk_stack_get_visible_child (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);

  return priv->visible_child ? priv->visible_child->widget : NULL;
}

/**
 * gtk_stack_get_visible_child_name:
 * @stack: a #GtkStack
 *
 * Returns the name of the currently visible child of @stack, or
 * %NULL if there is no visible child.
 *
 * Returns: (transfer none) (nullable): the name of the visible child of the #GtkStack
 */
const gchar *
gtk_stack_get_visible_child_name (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);

  if (priv->visible_child)
    return priv->visible_child->name;

  return NULL;
}

/**
 * gtk_stack_set_visible_child:
 * @stack: a #GtkStack
 * @child: a child of @stack
 *
 * Makes @child the visible child of @stack.
 *
 * If @child is different from the currently
 * visible child, the transition between the
 * two will be animated with the current
 * transition type of @stack.
 *
 * Note that the @child widget has to be visible itself
 * (see gtk_widget_show()) in order to become the visible
 * child of @stack.
 */
void
gtk_stack_set_visible_child (GtkStack  *stack,
                             GtkWidget *child)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *child_info;

  g_return_if_fail (GTK_IS_STACK (stack));
  g_return_if_fail (GTK_IS_WIDGET (child));

  child_info = find_child_info_for_widget (stack, child);
  if (child_info == NULL)
    {
      g_warning ("Given child of type '%s' not found in GtkStack",
                 G_OBJECT_TYPE_NAME (child));
      return;
    }

  if (gtk_widget_get_visible (child_info->widget))
    set_visible_child (stack, child_info,
                       priv->transition_type,
                       priv->transition_duration);
}

/**
 * gtk_stack_set_visible_child_name:
 * @stack: a #GtkStack
 * @name: the name of the child to make visible
 *
 * Makes the child with the given name visible.
 *
 * If @child is different from the currently
 * visible child, the transition between the
 * two will be animated with the current
 * transition type of @stack.
 *
 * Note that the child widget has to be visible itself
 * (see gtk_widget_show()) in order to become the visible
 * child of @stack.
 */
void
gtk_stack_set_visible_child_name (GtkStack   *stack,
                                 const gchar *name)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  gtk_stack_set_visible_child_full (stack, name, priv->transition_type);
}

/**
 * gtk_stack_set_visible_child_full:
 * @stack: a #GtkStack
 * @name: the name of the child to make visible
 * @transition: the transition type to use
 *
 * Makes the child with the given name visible.
 *
 * Note that the child widget has to be visible itself
 * (see gtk_widget_show()) in order to become the visible
 * child of @stack.
 */
void
gtk_stack_set_visible_child_full (GtkStack               *stack,
                                  const gchar            *name,
                                  GtkStackTransitionType  transition)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *child_info, *info;
  GList *l;

  g_return_if_fail (GTK_IS_STACK (stack));

  if (name == NULL)
    return;

  child_info = NULL;
  for (l = priv->children; l != NULL; l = l->next)
    {
      info = l->data;
      if (info->name != NULL &&
          strcmp (info->name, name) == 0)
        {
          child_info = info;
          break;
        }
    }

  if (child_info == NULL)
    {
      g_warning ("Child name '%s' not found in GtkStack", name);
      return;
    }

  if (gtk_widget_get_visible (child_info->widget))
    set_visible_child (stack, child_info, transition, priv->transition_duration);
}

static void
gtk_stack_forall (GtkContainer *container,
                  GtkCallback   callback,
                  gpointer      callback_data)
{
  GtkStack *stack = GTK_STACK (container);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *child_info;
  GList *l;

  l = priv->children;
  while (l)
    {
      child_info = l->data;
      l = l->next;

      (* callback) (child_info->widget, callback_data);
    }
}

static void
gtk_stack_compute_expand (GtkWidget *widget,
                          gboolean  *hexpand_p,
                          gboolean  *vexpand_p)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  gboolean hexpand, vexpand;
  GtkStackPage *child_info;
  GtkWidget *child;
  GList *l;

  hexpand = FALSE;
  vexpand = FALSE;
  for (l = priv->children; l != NULL; l = l->next)
    {
      child_info = l->data;
      child = child_info->widget;

      if (!hexpand &&
          gtk_widget_compute_expand (child, GTK_ORIENTATION_HORIZONTAL))
        hexpand = TRUE;

      if (!vexpand &&
          gtk_widget_compute_expand (child, GTK_ORIENTATION_VERTICAL))
        vexpand = TRUE;

      if (hexpand && vexpand)
        break;
    }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}

static void
gtk_stack_snapshot_crossfade (GtkWidget   *widget,
                              GtkSnapshot *snapshot)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  gdouble progress = gtk_progress_tracker_get_progress (&priv->tracker, FALSE);

  gtk_snapshot_push_cross_fade (snapshot, progress);

  if (priv->last_visible_node)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (
                              priv->last_visible_surface_allocation.x,
                              priv->last_visible_surface_allocation.y));
      gtk_snapshot_append_node (snapshot, priv->last_visible_node);
      gtk_snapshot_restore (snapshot);
    }
  gtk_snapshot_pop (snapshot);

  gtk_widget_snapshot_child (widget,
                             priv->visible_child->widget,
                             snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
gtk_stack_snapshot_under (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  int widget_width, widget_height;
  gint x, y, width, height, pos_x, pos_y;


  x = y = 0;
  width = widget_width = gtk_widget_get_width (widget);
  height = widget_height = gtk_widget_get_height (widget);

  pos_x = pos_y = 0;

  switch ((guint) priv->active_transition_type)
    {
    case GTK_STACK_TRANSITION_TYPE_UNDER_DOWN:
      y = 0;
      height = widget_height * (gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
      pos_y = height;
      break;
    case GTK_STACK_TRANSITION_TYPE_UNDER_UP:
      y = widget_height * (1 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
      height = widget_height - y;
      pos_y = y - widget_height;
      break;
    case GTK_STACK_TRANSITION_TYPE_UNDER_LEFT:
      x = widget_width * (1 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
      width = widget_width - x;
      pos_x = x - widget_width;
      break;
    case GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT:
      x = 0;
      width = widget_width * (gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
      pos_x = width;
      break;
    default:
      g_assert_not_reached ();
    }

  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT(x, y, width, height));

  gtk_widget_snapshot_child (widget,
                             priv->visible_child->widget,
                             snapshot);

  gtk_snapshot_pop (snapshot);

  if (priv->last_visible_node)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (pos_x, pos_y));
      gtk_snapshot_append_node (snapshot, priv->last_visible_node);
      gtk_snapshot_restore (snapshot);
    }
}

static void
gtk_stack_snapshot_cube (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  double progress = gtk_progress_tracker_get_progress (&priv->tracker, FALSE);

  if (priv->active_transition_type == GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT)
    progress = 1 - progress;

  if (priv->last_visible_node && progress > 0.5)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                                 gtk_widget_get_width (widget) / 2.f,
                                 gtk_widget_get_height (widget) / 2.f,
                                 0));
      gtk_snapshot_perspective (snapshot, 2 * gtk_widget_get_width (widget) / 1.f);
      gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                                 0, 0,
                                 - gtk_widget_get_width (widget) / 2.f));
      gtk_snapshot_rotate_3d (snapshot, -90 * progress, graphene_vec3_y_axis());
      gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                                 - gtk_widget_get_width (widget) / 2.f,
                                 - gtk_widget_get_height (widget) / 2.f,
                                 gtk_widget_get_width (widget) / 2.f));
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (
                              priv->last_visible_surface_allocation.x,
                              priv->last_visible_surface_allocation.y));
      if (priv->active_transition_type == GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT)
        gtk_snapshot_append_node (snapshot, priv->last_visible_node);
      else
        gtk_widget_snapshot_child (widget, priv->visible_child->widget, snapshot);
      gtk_snapshot_restore (snapshot);
    }

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                             gtk_widget_get_width (widget) / 2.f,
                             gtk_widget_get_height (widget) / 2.f,
                             0));
  gtk_snapshot_perspective (snapshot, 2 * gtk_widget_get_width (widget) / 1.f);
  gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                             0, 0,
                             - gtk_widget_get_width (widget) / 2.f));
  gtk_snapshot_rotate_3d (snapshot, 90 * (1.0 - progress), graphene_vec3_y_axis());
  gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                             - gtk_widget_get_width (widget) / 2.f,
                             - gtk_widget_get_height (widget) / 2.f,
                             gtk_widget_get_width (widget) / 2.f));
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (
                          priv->last_visible_surface_allocation.x,
                          priv->last_visible_surface_allocation.y));

  if (priv->active_transition_type == GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT)
    gtk_widget_snapshot_child (widget, priv->visible_child->widget, snapshot);
  else
    gtk_snapshot_append_node (snapshot, priv->last_visible_node);
  gtk_snapshot_restore (snapshot);

  if (priv->last_visible_node && progress <= 0.5)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                                 gtk_widget_get_width (widget) / 2.f,
                                 gtk_widget_get_height (widget) / 2.f,
                                 0));
      gtk_snapshot_perspective (snapshot, 2 * gtk_widget_get_width (widget) / 1.f);
      gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                                 0, 0,
                                 - gtk_widget_get_width (widget) / 2.f));
      gtk_snapshot_rotate_3d (snapshot, -90 * progress, graphene_vec3_y_axis());
      gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                                 - gtk_widget_get_width (widget) / 2.f,
                                 - gtk_widget_get_height (widget) / 2.f,
                                 gtk_widget_get_width (widget) / 2.f));
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (
                              priv->last_visible_surface_allocation.x,
                              priv->last_visible_surface_allocation.y));
      if (priv->active_transition_type == GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT)
        gtk_snapshot_append_node (snapshot, priv->last_visible_node);
      else
        gtk_widget_snapshot_child (widget, priv->visible_child->widget, snapshot);
      gtk_snapshot_restore (snapshot);
    }
}

static void
gtk_stack_snapshot_slide (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->last_visible_node)
    {
      int x, y;
      int width, height;

      width = gtk_widget_get_width (widget);
      height = gtk_widget_get_height (widget);

      x = get_bin_window_x (stack);
      y = get_bin_window_y (stack);

      switch ((guint) priv->active_transition_type)
        {
        case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT:
          x -= width;
          break;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT:
          x += width;
          break;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_UP:
          y -= height;
          break;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN:
          y += height;
          break;
        case GTK_STACK_TRANSITION_TYPE_OVER_UP:
        case GTK_STACK_TRANSITION_TYPE_OVER_DOWN:
          y = 0;
          break;
        case GTK_STACK_TRANSITION_TYPE_OVER_LEFT:
        case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT:
          x = 0;
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      if (priv->last_visible_child != NULL)
        {
          if (gtk_widget_get_valign (priv->last_visible_child->widget) == GTK_ALIGN_END &&
              priv->last_visible_widget_height > height)
            y -= priv->last_visible_widget_height - height;
          else if (gtk_widget_get_valign (priv->last_visible_child->widget) == GTK_ALIGN_CENTER)
            y -= (priv->last_visible_widget_height - height) / 2;
        }

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
      gtk_snapshot_append_node (snapshot, priv->last_visible_node);
      gtk_snapshot_restore (snapshot);
     }

  gtk_widget_snapshot_child (widget,
                             priv->visible_child->widget,
                             snapshot);
}

static void
gtk_stack_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->visible_child)
    {
      if (gtk_progress_tracker_get_state (&priv->tracker) != GTK_PROGRESS_STATE_AFTER)
        {
          if (priv->last_visible_node == NULL &&
              priv->last_visible_child != NULL)
            {
              GtkSnapshot *last_visible_snapshot;

              gtk_widget_get_allocation (priv->last_visible_child->widget,
                                         &priv->last_visible_surface_allocation);
              last_visible_snapshot = gtk_snapshot_new ();
              gtk_widget_snapshot (priv->last_visible_child->widget, last_visible_snapshot);
              priv->last_visible_node = gtk_snapshot_free_to_node (last_visible_snapshot);
            }

          gtk_snapshot_push_clip (snapshot,
                                  &GRAPHENE_RECT_INIT(
                                      0, 0,
                                      gtk_widget_get_width (widget),
                                      gtk_widget_get_height (widget)
                                  ));

          switch (priv->active_transition_type)
            {
            case GTK_STACK_TRANSITION_TYPE_CROSSFADE:
	      gtk_stack_snapshot_crossfade (widget, snapshot);
              break;
            case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_UP:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN:
            case GTK_STACK_TRANSITION_TYPE_OVER_UP:
            case GTK_STACK_TRANSITION_TYPE_OVER_DOWN:
            case GTK_STACK_TRANSITION_TYPE_OVER_LEFT:
            case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT:
              gtk_stack_snapshot_slide (widget, snapshot);
              break;
            case GTK_STACK_TRANSITION_TYPE_UNDER_UP:
            case GTK_STACK_TRANSITION_TYPE_UNDER_DOWN:
            case GTK_STACK_TRANSITION_TYPE_UNDER_LEFT:
            case GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT:
	      gtk_stack_snapshot_under (widget, snapshot);
              break;
            case GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT:
            case GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT:
	      gtk_stack_snapshot_cube (widget, snapshot);
              break;
            case GTK_STACK_TRANSITION_TYPE_NONE:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN:
            case GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN:
            case GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP:
            case GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT:
            case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT:
            case GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT_RIGHT:
            default:
              g_assert_not_reached ();
            }

          gtk_snapshot_pop (snapshot);
        }
      else
        gtk_widget_snapshot_child (widget,
                                   priv->visible_child->widget,
                                   snapshot);
    }
}

static void
gtk_stack_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkAllocation child_allocation;

  child_allocation.x = get_bin_window_x (stack);
  child_allocation.y = get_bin_window_y (stack);

  if (priv->last_visible_child)
    {
      int min, nat;

      gtk_widget_measure (priv->last_visible_child->widget, GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          &min, &nat, NULL, NULL);
      child_allocation.width = MAX (min, width);
      gtk_widget_measure (priv->last_visible_child->widget, GTK_ORIENTATION_VERTICAL,
                          child_allocation.width,
                          &min, &nat, NULL, NULL);
      child_allocation.height = MAX (min, height);

      gtk_widget_size_allocate (priv->last_visible_child->widget, &child_allocation, -1);

      if (!gdk_rectangle_equal (&priv->last_visible_surface_allocation,
                                &child_allocation))
        {
          g_clear_pointer (&priv->last_visible_node, gsk_render_node_unref);
        }
    }

  child_allocation.width = width;
  child_allocation.height = height;

  if (priv->visible_child)
    {
      int min_width;
      int min_height;

      gtk_widget_measure (priv->visible_child->widget, GTK_ORIENTATION_HORIZONTAL,
                          height, &min_width, NULL, NULL, NULL);
      child_allocation.width = MAX (child_allocation.width, min_width);

      gtk_widget_measure (priv->visible_child->widget, GTK_ORIENTATION_VERTICAL,
                          child_allocation.width, &min_height, NULL, NULL, NULL);
      child_allocation.height = MAX (child_allocation.height, min_height);

      if (child_allocation.width > width)
        {
          GtkAlign halign = gtk_widget_get_halign (priv->visible_child->widget);

          if (halign == GTK_ALIGN_CENTER || halign == GTK_ALIGN_FILL)
            child_allocation.x = (width - child_allocation.width) / 2;
          else if (halign == GTK_ALIGN_END)
            child_allocation.x = (width - child_allocation.width);
        }

      if (child_allocation.height > height)
        {
          GtkAlign valign = gtk_widget_get_valign (priv->visible_child->widget);

          if (valign == GTK_ALIGN_CENTER || valign == GTK_ALIGN_FILL)
            child_allocation.y = (height - child_allocation.height) / 2;
          else if (valign == GTK_ALIGN_END)
            child_allocation.y = (height - child_allocation.height);
        }

      gtk_widget_size_allocate (priv->visible_child->widget, &child_allocation, -1);
    }
}

#define LERP(a, b, t) ((a) + (((b) - (a)) * (1.0 - (t))))
static void
gtk_stack_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *child_info;
  GtkWidget *child;
  gint child_min, child_nat;
  GList *l;

  *minimum = 0;
  *natural = 0;

  for (l = priv->children; l != NULL; l = l->next)
    {
      child_info = l->data;
      child = child_info->widget;

      if (((orientation == GTK_ORIENTATION_VERTICAL && !priv->vhomogeneous) ||
           (orientation == GTK_ORIENTATION_HORIZONTAL && !priv->hhomogeneous)) &&
           priv->visible_child != child_info)
        continue;

      if (gtk_widget_get_visible (child))
        {
          gtk_widget_measure (child, orientation, for_size, &child_min, &child_nat, NULL, NULL);

          *minimum = MAX (*minimum, child_min);
          *natural = MAX (*natural, child_nat);
        }
    }

  if (priv->last_visible_child != NULL)
    {
      if (orientation == GTK_ORIENTATION_VERTICAL && !priv->vhomogeneous)
        {
          gdouble t = priv->interpolate_size ? gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE) : 1.0;
          *minimum = LERP (*minimum, priv->last_visible_widget_height, t);
          *natural = LERP (*natural, priv->last_visible_widget_height, t);
        }
      if (orientation == GTK_ORIENTATION_HORIZONTAL && !priv->hhomogeneous)
        {
          gdouble t = priv->interpolate_size ? gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE) : 1.0;
          *minimum = LERP (*minimum, priv->last_visible_widget_width, t);
          *natural = LERP (*natural, priv->last_visible_widget_width, t);
        }
    }
}

static void
gtk_stack_init (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  priv->vhomogeneous = TRUE;
  priv->hhomogeneous = TRUE;
  priv->transition_duration = 200;
  priv->transition_type = GTK_STACK_TRANSITION_TYPE_NONE;
}

/**
 * gtk_stack_get_pages:
 * @stack: a #GtkStack
 *
 * Returns a #GListModel that contains the pages of the stack,
 * and can be used to keep and up-to-date view. The model also
 * implements #GtkSelectionModel and can be used to track and
 * modify the visible page..
 *
 * Returns: (transfer full): a #GtkSelectionModel for the stack's children
 */
GtkSelectionModel *
gtk_stack_get_pages (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);

  if (priv->pages)
    return g_object_ref (priv->pages);

  priv->pages = GTK_SELECTION_MODEL (gtk_stack_pages_new (stack));
  g_object_add_weak_pointer (G_OBJECT (priv->pages), (gpointer *)&priv->pages);

  return priv->pages;
}
