/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 */

/**
 * SECTION:gtkexpander
 * @Short_description: A container which can hide its child
 * @Title: GtkExpander
 *
 * A #GtkExpander allows the user to hide or show its child by clicking
 * on an expander triangle similar to the triangles used in a #GtkTreeView.
 *
 * Normally you use an expander as you would use a descendant
 * of #GtkBin; you create the child widget and use gtk_container_add()
 * to add it to the expander. When the expander is toggled, it will take
 * care of showing and hiding the child automatically.
 *
 * # Special Usage
 *
 * There are situations in which you may prefer to show and hide the
 * expanded widget yourself, such as when you want to actually create
 * the widget at expansion time. In this case, create a #GtkExpander
 * but do not add a child to it. The expander widget has an
 * #GtkExpander:expanded property which can be used to monitor
 * its expansion state. You should watch this property with a signal
 * connection as follows:
 *
 * |[<!-- language="C" -->
 * static void
 * expander_callback (GObject    *object,
 *                    GParamSpec *param_spec,
 *                    gpointer    user_data)
 * {
 *   GtkExpander *expander;
 *
 *   expander = GTK_EXPANDER (object);
 *
 *   if (gtk_expander_get_expanded (expander))
 *     {
 *       // Show or create widgets
 *     }
 *   else
 *     {
 *       // Hide or destroy widgets
 *     }
 * }
 *
 * static void
 * create_expander (void)
 * {
 *   GtkWidget *expander = gtk_expander_new_with_mnemonic ("_More Options");
 *   g_signal_connect (expander, "notify::expanded",
 *                     G_CALLBACK (expander_callback), NULL);
 *
 *   // ...
 * }
 * ]|
 *
 * # GtkExpander as GtkBuildable
 *
 * The GtkExpander implementation of the GtkBuildable interface supports
 * placing a child in the label position by specifying “label” as the
 * “type” attribute of a <child> element. A normal content child can be
 * specified without specifying a <child> type attribute.
 *
 * An example of a UI definition fragment with GtkExpander:
 * |[
 * <object class="GtkExpander">
 *   <child type="label">
 *     <object class="GtkLabel" id="expander-label"/>
 *   </child>
 *   <child>
 *     <object class="GtkEntry" id="expander-content"/>
 *   </child>
 * </object>
 * ]|
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * expander
 * ╰── box
 *     ├── title
 *     │   ├── arrow
 *     │   ╰── <label widget>
 *     ╰── <child>
 * ]|
 *
 * GtkExpander has three CSS nodes, the main node with the name expander,
 * a subnode with name title and node below it with name arrow. The arrow of an
 * expander that is showing its child gets the :checked pseudoclass added to it.
 */

#include "config.h"

#include "gtkexpander.h"

#include "gtkbox.h"
#include "gtkbuildable.h"
#include "gtkcontainerprivate.h"
#include "gtkdnd.h"
#include "gtkdragdest.h"
#include "gtkiconprivate.h"
#include "gtkgesturemultipress.h"
#include "gtkgesturesingle.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtkexpanderaccessible.h"

#include <string.h>

#define TIMEOUT_EXPAND 500

enum
{
  PROP_0,
  PROP_EXPANDED,
  PROP_LABEL,
  PROP_USE_UNDERLINE,
  PROP_USE_MARKUP,
  PROP_LABEL_WIDGET,
  PROP_RESIZE_TOPLEVEL
};

typedef struct _GtkExpanderClass   GtkExpanderClass;

struct _GtkExpander
{
  GtkContainer parent_instance;
};

struct _GtkExpanderClass
{
  GtkContainerClass parent_class;

  void (* activate) (GtkExpander *expander);
};

typedef struct _GtkExpanderPrivate GtkExpanderPrivate;
struct _GtkExpanderPrivate
{
  GtkWidget        *label_widget;

  GtkWidget        *box;
  GtkWidget        *title_widget;
  GtkWidget        *arrow_widget;
  GtkWidget        *child;

  guint             expand_timer;

  guint             expanded        : 1;
  guint             use_underline   : 1;
  guint             use_markup      : 1;
  guint             resize_toplevel : 1;
};

static void gtk_expander_set_property (GObject          *object,
                                       guint             prop_id,
                                       const GValue     *value,
                                       GParamSpec       *pspec);
static void gtk_expander_get_property (GObject          *object,
                                       guint             prop_id,
                                       GValue           *value,
                                       GParamSpec       *pspec);

static void     gtk_expander_destroy        (GtkWidget        *widget);
static void     gtk_expander_size_allocate  (GtkWidget        *widget,
                                             int               width,
                                             int               height,
                                             int               baseline);
static gboolean gtk_expander_focus          (GtkWidget        *widget,
                                             GtkDirectionType  direction);
static gboolean gtk_expander_drag_motion    (GtkWidget        *widget,
                                             GdkDrop          *drop,
                                             gint              x,
                                             gint              y);
static void     gtk_expander_drag_leave     (GtkWidget        *widget,
                                             GdkDrop          *drop);

static void gtk_expander_add    (GtkContainer *container,
                                 GtkWidget    *widget);
static void gtk_expander_remove (GtkContainer *container,
                                 GtkWidget    *widget);

static void gtk_expander_activate (GtkExpander *expander);


/* GtkBuildable */
static void gtk_expander_buildable_init           (GtkBuildableIface *iface);
static void gtk_expander_buildable_add_child      (GtkBuildable *buildable,
                                                   GtkBuilder   *builder,
                                                   GObject      *child,
                                                   const gchar  *type);


/* GtkWidget      */
static void gtk_expander_measure (GtkWidget      *widget,
                                  GtkOrientation  orientation,
                                  int             for_size,
                                  int            *minimum,
                                  int            *natural,
                                  int            *minimum_baseline,
                                  int            *natural_baseline);

/* Gestures */
static void     gesture_multipress_released_cb (GtkGestureMultiPress *gesture,
                                                gint                  n_press,
                                                gdouble               x,
                                                gdouble               y,
                                                GtkExpander          *expander);

G_DEFINE_TYPE_WITH_CODE (GtkExpander, gtk_expander, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkExpander)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_expander_buildable_init))

static void
gtk_expander_forall (GtkContainer *container,
                     GtkCallback   callback,
                     gpointer      user_data)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (GTK_EXPANDER (container));

  if (priv->child)
    (*callback) (priv->child, user_data);

  if (priv->label_widget)
    (*callback) (priv->label_widget, user_data);
}

static void
gtk_expander_class_init (GtkExpanderClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class   = (GObjectClass *) klass;
  widget_class    = (GtkWidgetClass *) klass;
  container_class = (GtkContainerClass *) klass;

  gobject_class->set_property = gtk_expander_set_property;
  gobject_class->get_property = gtk_expander_get_property;

  widget_class->destroy              = gtk_expander_destroy;
  widget_class->size_allocate        = gtk_expander_size_allocate;
  widget_class->focus                = gtk_expander_focus;
  widget_class->drag_motion          = gtk_expander_drag_motion;
  widget_class->drag_leave           = gtk_expander_drag_leave;
  widget_class->measure              = gtk_expander_measure;

  container_class->add    = gtk_expander_add;
  container_class->remove = gtk_expander_remove;
  container_class->forall = gtk_expander_forall;

  klass->activate = gtk_expander_activate;

  g_object_class_install_property (gobject_class,
                                   PROP_EXPANDED,
                                   g_param_spec_boolean ("expanded",
                                                         P_("Expanded"),
                                                         P_("Whether the expander has been opened to reveal the child widget"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        P_("Label"),
                                                        P_("Text of the expander’s label"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_USE_UNDERLINE,
                                   g_param_spec_boolean ("use-underline",
                                                         P_("Use underline"),
                                                         P_("If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_USE_MARKUP,
                                   g_param_spec_boolean ("use-markup",
                                                         P_("Use markup"),
                                                         P_("The text of the label includes XML markup. See pango_parse_markup()"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL_WIDGET,
                                   g_param_spec_object ("label-widget",
                                                        P_("Label widget"),
                                                        P_("A widget to display in place of the usual expander label"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkExpander:resize-toplevel:
   *
   * When this property is %TRUE, the expander will resize the toplevel
   * widget containing the expander upon expanding and collapsing.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_RESIZE_TOPLEVEL,
                                   g_param_spec_boolean ("resize-toplevel",
                                                         P_("Resize toplevel"),
                                                         P_("Whether the expander will resize the toplevel window upon expanding and collapsing"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  widget_class->activate_signal =
    g_signal_new (I_("activate"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkExpanderClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_EXPANDER_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("expander"));
}

static void
gtk_expander_init (GtkExpander *expander)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);
  GtkGesture *gesture;

  gtk_widget_set_can_focus (GTK_WIDGET (expander), TRUE);

  priv->label_widget = NULL;
  priv->child = NULL;

  priv->expanded = FALSE;
  priv->use_underline = FALSE;
  priv->use_markup = FALSE;
  priv->expand_timer = 0;
  priv->resize_toplevel = 0;

  priv->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_parent (priv->box, GTK_WIDGET (expander));

  priv->title_widget = g_object_new (GTK_TYPE_BOX,
                                     "css-name", "title",
                                     NULL);
  gtk_container_add (GTK_CONTAINER (priv->box), priv->title_widget);

  priv->arrow_widget = gtk_icon_new ("arrow");
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->arrow_widget),
                               GTK_STYLE_CLASS_HORIZONTAL);
  gtk_container_add (GTK_CONTAINER (priv->title_widget), priv->arrow_widget);

  gtk_drag_dest_set (GTK_WIDGET (expander), 0, NULL, 0);
  gtk_drag_dest_set_track_motion (GTK_WIDGET (expander), TRUE);

  gesture = gtk_gesture_multi_press_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture),
                                 GDK_BUTTON_PRIMARY);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture),
                                     FALSE);
  g_signal_connect (gesture, "released",
                    G_CALLBACK (gesture_multipress_released_cb), expander);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_BUBBLE);
  gtk_widget_add_controller (GTK_WIDGET (priv->title_widget), GTK_EVENT_CONTROLLER (gesture));
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_expander_buildable_add_child (GtkBuildable  *buildable,
                                  GtkBuilder    *builder,
                                  GObject       *child,
                                  const gchar   *type)
{
  if (g_strcmp0 (type, "label") == 0)
    gtk_expander_set_label_widget (GTK_EXPANDER (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_expander_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_expander_buildable_add_child;
}

static void
gtk_expander_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkExpander *expander = GTK_EXPANDER (object);

  switch (prop_id)
    {
    case PROP_EXPANDED:
      gtk_expander_set_expanded (expander, g_value_get_boolean (value));
      break;
    case PROP_LABEL:
      gtk_expander_set_label (expander, g_value_get_string (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_expander_set_use_underline (expander, g_value_get_boolean (value));
      break;
    case PROP_USE_MARKUP:
      gtk_expander_set_use_markup (expander, g_value_get_boolean (value));
      break;
    case PROP_LABEL_WIDGET:
      gtk_expander_set_label_widget (expander, g_value_get_object (value));
      break;
    case PROP_RESIZE_TOPLEVEL:
      gtk_expander_set_resize_toplevel (expander, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_expander_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkExpander *expander = GTK_EXPANDER (object);
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  switch (prop_id)
    {
    case PROP_EXPANDED:
      g_value_set_boolean (value, priv->expanded);
      break;
    case PROP_LABEL:
      g_value_set_string (value, gtk_expander_get_label (expander));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, priv->use_underline);
      break;
    case PROP_USE_MARKUP:
      g_value_set_boolean (value, priv->use_markup);
      break;
    case PROP_LABEL_WIDGET:
      g_value_set_object (value,
                          priv->label_widget ?
                          G_OBJECT (priv->label_widget) : NULL);
      break;
    case PROP_RESIZE_TOPLEVEL:
      g_value_set_boolean (value, gtk_expander_get_resize_toplevel (expander));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_expander_destroy (GtkWidget *widget)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (GTK_EXPANDER (widget));

  if (priv->expand_timer)
    {
      g_source_remove (priv->expand_timer);
      priv->expand_timer = 0;
    }

  if (priv->box)
    {
      gtk_widget_unparent (priv->box);
      priv->box = NULL;
      priv->child = NULL;
      priv->label_widget = NULL;
      priv->arrow_widget = NULL;
    }

  GTK_WIDGET_CLASS (gtk_expander_parent_class)->destroy (widget);
}

static void
gtk_expander_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (GTK_EXPANDER (widget));

  gtk_widget_size_allocate (priv->box,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            }, baseline);
}

static void
gesture_multipress_released_cb (GtkGestureMultiPress *gesture,
                                gint                  n_press,
                                gdouble               x,
                                gdouble               y,
                                GtkExpander          *expander)
{
  gtk_widget_activate (GTK_WIDGET (expander));
}

static gboolean
expand_timeout (gpointer data)
{
  GtkExpander *expander = GTK_EXPANDER (data);
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  priv->expand_timer = 0;
  gtk_expander_set_expanded (expander, TRUE);

  return FALSE;
}

static gboolean
gtk_expander_drag_motion (GtkWidget *widget,
                          GdkDrop   *drop,
                          gint       x,
                          gint       y)
{
  GtkExpander *expander = GTK_EXPANDER (widget);
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  if (!priv->expanded && !priv->expand_timer)
    {
      priv->expand_timer = g_timeout_add (TIMEOUT_EXPAND, (GSourceFunc) expand_timeout, expander);
      g_source_set_name_by_id (priv->expand_timer, "[gtk] expand_timeout");
    }

  return TRUE;
}

static void
gtk_expander_drag_leave (GtkWidget *widget,
                         GdkDrop   *drop)
{
  GtkExpander *expander = GTK_EXPANDER (widget);
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  if (priv->expand_timer)
    {
      g_source_remove (priv->expand_timer);
      priv->expand_timer = 0;
    }
}

typedef enum
{
  FOCUS_NONE,
  FOCUS_WIDGET,
  FOCUS_LABEL,
  FOCUS_CHILD
} FocusSite;

static gboolean
focus_current_site (GtkExpander      *expander,
                    GtkDirectionType  direction)
{
  GtkWidget *current_focus;

  current_focus = gtk_widget_get_focus_child (GTK_WIDGET (expander));

  if (!current_focus)
    return FALSE;

  return gtk_widget_child_focus (current_focus, direction);
}

static gboolean
focus_in_site (GtkExpander      *expander,
               FocusSite         site,
               GtkDirectionType  direction)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  switch (site)
    {
    case FOCUS_WIDGET:
      gtk_widget_grab_focus (GTK_WIDGET (expander));
      return TRUE;
    case FOCUS_LABEL:
      if (priv->label_widget)
        return gtk_widget_child_focus (priv->label_widget, direction);
      else
        return FALSE;
    case FOCUS_CHILD:
      {
        GtkWidget *child = priv->child;

        if (child && gtk_widget_get_child_visible (child))
          return gtk_widget_child_focus (child, direction);
        else
          return FALSE;
      }
    case FOCUS_NONE:
    default:
      break;
    }

  g_assert_not_reached ();
  return FALSE;
}

static FocusSite
get_next_site (GtkExpander      *expander,
               FocusSite         site,
               GtkDirectionType  direction)
{
  gboolean ltr;

  ltr = gtk_widget_get_direction (GTK_WIDGET (expander)) != GTK_TEXT_DIR_RTL;

  switch (site)
    {
    case FOCUS_NONE:
      switch (direction)
        {
        case GTK_DIR_TAB_BACKWARD:
        case GTK_DIR_LEFT:
        case GTK_DIR_UP:
          return FOCUS_CHILD;
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_DOWN:
        case GTK_DIR_RIGHT:
        default:
          return FOCUS_WIDGET;
        }
      break;
    case FOCUS_WIDGET:
      switch (direction)
        {
        case GTK_DIR_TAB_BACKWARD:
        case GTK_DIR_UP:
          return FOCUS_NONE;
        case GTK_DIR_LEFT:
          return ltr ? FOCUS_NONE : FOCUS_LABEL;
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_DOWN:
        default:
          return FOCUS_LABEL;
        case GTK_DIR_RIGHT:
          return ltr ? FOCUS_LABEL : FOCUS_NONE;
        }
      break;
    case FOCUS_LABEL:
      switch (direction)
        {
        case GTK_DIR_TAB_BACKWARD:
        case GTK_DIR_UP:
          return FOCUS_WIDGET;
        case GTK_DIR_LEFT:
          return ltr ? FOCUS_WIDGET : FOCUS_CHILD;
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_DOWN:
        default:
          return FOCUS_CHILD;
        case GTK_DIR_RIGHT:
          return ltr ? FOCUS_CHILD : FOCUS_WIDGET;
        }
      break;
    case FOCUS_CHILD:
      switch (direction)
        {
        case GTK_DIR_TAB_BACKWARD:
        case GTK_DIR_LEFT:
        case GTK_DIR_UP:
          return FOCUS_LABEL;
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_DOWN:
        case GTK_DIR_RIGHT:
        default:
          return FOCUS_NONE;
        }
      break;
    default:
      g_assert_not_reached ();
      break;
    }
  
  return FOCUS_NONE;
}

static void
gtk_expander_resize_toplevel (GtkExpander *expander)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);
  GtkWidget *child = priv->child;

  if (child && priv->resize_toplevel &&
      gtk_widget_get_realized (GTK_WIDGET (expander)))
    {
      GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (expander)));

      if (GTK_IS_WINDOW (toplevel) &&
          gtk_widget_get_realized (toplevel))
        {
          int toplevel_width, toplevel_height;
          int child_height;

          gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, -1,
                              &child_height, NULL, NULL, NULL);
          gtk_window_get_size (GTK_WINDOW (toplevel), &toplevel_width, &toplevel_height);

          if (priv->expanded)
            toplevel_height += child_height;
          else
            toplevel_height -= child_height;

          gtk_window_resize (GTK_WINDOW (toplevel),
                             toplevel_width,
                             toplevel_height);
        }
    }
}

static gboolean
gtk_expander_focus (GtkWidget        *widget,
                    GtkDirectionType  direction)
{
  GtkExpander *expander = GTK_EXPANDER (widget);
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  if (!focus_current_site (expander, direction))
    {
      GtkWidget *old_focus_child;
      gboolean widget_is_focus;
      FocusSite site = FOCUS_NONE;

      widget_is_focus = gtk_widget_is_focus (widget);
      old_focus_child = gtk_widget_get_focus_child (GTK_WIDGET (widget));

      if (old_focus_child && old_focus_child == priv->label_widget)
        site = FOCUS_LABEL;
      else if (old_focus_child)
        site = FOCUS_CHILD;
      else if (widget_is_focus)
        site = FOCUS_WIDGET;

      while ((site = get_next_site (expander, site, direction)) != FOCUS_NONE)
        {
          if (focus_in_site (expander, site, direction))
            return TRUE;
        }

      return FALSE;
    }

  return TRUE;
}

static void
gtk_expander_add (GtkContainer *container,
                  GtkWidget    *widget)
{
  GtkExpander *expander = GTK_EXPANDER (container);
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  if (priv->child != NULL)
    {
      g_warning ("Attempting to add a widget with type %s to a %s, "
                 "but a %s can only contain one widget at a time; "
                 "it already contains a widget of type %s",
                 g_type_name (G_OBJECT_TYPE (widget)),
                 g_type_name (G_OBJECT_TYPE (container)),
                 g_type_name (G_OBJECT_TYPE (container)),
                 g_type_name (G_OBJECT_TYPE (priv->child)));
      return;
    }

  if (priv->expanded)
    {
      gtk_container_add (GTK_CONTAINER (priv->box), widget);
    }
  else
    {
      if (g_object_is_floating (widget))
        g_object_ref_sink (widget);

      g_object_ref (widget);
    }

  priv->child = widget;
}

static void
gtk_expander_remove (GtkContainer *container,
                     GtkWidget    *widget)
{
  GtkExpander *expander = GTK_EXPANDER (container);
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  if (priv->label_widget == widget)
    gtk_expander_set_label_widget (expander, NULL);
  else
    {
      gtk_container_remove (GTK_CONTAINER (priv->box), widget);
      if (!priv->expanded)
        {
          /* We hold an extra ref */
          g_object_unref (widget);
        }
      GTK_CONTAINER_CLASS (gtk_expander_parent_class)->remove (container, widget);
    }
}

static void
gtk_expander_activate (GtkExpander *expander)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  gtk_expander_set_expanded (expander, !priv->expanded);
}

static void
gtk_expander_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  GtkExpander *expander = GTK_EXPANDER (widget);
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  gtk_widget_measure (priv->box,
                       orientation,
                       for_size,
                       minimum, natural,
                       minimum_baseline, natural_baseline);
}

/**
 * gtk_expander_new:
 * @label: (nullable): the text of the label
 *
 * Creates a new expander using @label as the text of the label.
 *
 * Returns: a new #GtkExpander widget.
 */
GtkWidget *
gtk_expander_new (const gchar *label)
{
  return g_object_new (GTK_TYPE_EXPANDER, "label", label, NULL);
}

/**
 * gtk_expander_new_with_mnemonic:
 * @label: (nullable): the text of the label with an underscore
 *     in front of the mnemonic character
 *
 * Creates a new expander using @label as the text of the label.
 * If characters in @label are preceded by an underscore, they are underlined.
 * If you need a literal underscore character in a label, use “__” (two
 * underscores). The first underlined character represents a keyboard
 * accelerator called a mnemonic.
 * Pressing Alt and that key activates the button.
 *
 * Returns: a new #GtkExpander widget.
 */
GtkWidget *
gtk_expander_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_EXPANDER,
                       "label", label,
                       "use-underline", TRUE,
                       NULL);
}

/**
 * gtk_expander_set_expanded:
 * @expander: a #GtkExpander
 * @expanded: whether the child widget is revealed
 *
 * Sets the state of the expander. Set to %TRUE, if you want
 * the child widget to be revealed, and %FALSE if you want the
 * child widget to be hidden.
 */
void
gtk_expander_set_expanded (GtkExpander *expander,
                           gboolean     expanded)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);
  GtkWidget *child;

  g_return_if_fail (GTK_IS_EXPANDER (expander));

  expanded = expanded != FALSE;

  if (priv->expanded == expanded)
    return;

  priv->expanded = expanded;

  if (priv->expanded)
    gtk_widget_set_state_flags (priv->arrow_widget, GTK_STATE_FLAG_CHECKED, FALSE);
  else
    gtk_widget_unset_state_flags (priv->arrow_widget, GTK_STATE_FLAG_CHECKED);

  child = priv->child;

  if (child)
    {
      if (priv->expanded)
        {
          gtk_container_add (GTK_CONTAINER (priv->box), child);
          g_object_unref (priv->child);
        }
      else
        {
          g_object_ref (priv->child);
          gtk_container_remove (GTK_CONTAINER (priv->box), child);
        }
      gtk_expander_resize_toplevel (expander);
    }

  g_object_notify (G_OBJECT (expander), "expanded");
}

/**
 * gtk_expander_get_expanded:
 * @expander:a #GtkExpander
 *
 * Queries a #GtkExpander and returns its current state. Returns %TRUE
 * if the child widget is revealed.
 *
 * See gtk_expander_set_expanded().
 *
 * Returns: the current state of the expander
 */
gboolean
gtk_expander_get_expanded (GtkExpander *expander)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return priv->expanded;
}

/**
 * gtk_expander_set_label:
 * @expander: a #GtkExpander
 * @label: (nullable): a string
 *
 * Sets the text of the label of the expander to @label.
 *
 * This will also clear any previously set labels.
 */
void
gtk_expander_set_label (GtkExpander *expander,
                        const gchar *label)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  g_return_if_fail (GTK_IS_EXPANDER (expander));

  if (!label)
    {
      gtk_expander_set_label_widget (expander, NULL);
    }
  else
    {
      GtkWidget *child;

      child = gtk_label_new (label);
      gtk_label_set_use_underline (GTK_LABEL (child), priv->use_underline);
      gtk_label_set_use_markup (GTK_LABEL (child), priv->use_markup);
      gtk_widget_show (child);

      gtk_expander_set_label_widget (expander, child);
    }

  g_object_notify (G_OBJECT (expander), "label");
}

/**
 * gtk_expander_get_label:
 * @expander: a #GtkExpander
 *
 * Fetches the text from a label widget including any embedded
 * underlines indicating mnemonics and Pango markup, as set by
 * gtk_expander_set_label(). If the label text has not been set the
 * return value will be %NULL. This will be the case if you create an
 * empty button with gtk_button_new() to use as a container.
 *
 * Note that this function behaved differently in versions prior to
 * 2.14 and used to return the label text stripped of embedded
 * underlines indicating mnemonics and Pango markup. This problem can
 * be avoided by fetching the label text directly from the label
 * widget.
 *
 * Returns: (nullable): The text of the label widget. This string is owned
 *     by the widget and must not be modified or freed.
 */
const gchar *
gtk_expander_get_label (GtkExpander *expander)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  g_return_val_if_fail (GTK_IS_EXPANDER (expander), NULL);

  if (GTK_IS_LABEL (priv->label_widget))
    return gtk_label_get_label (GTK_LABEL (priv->label_widget));
  else
    return NULL;
}

/**
 * gtk_expander_set_use_underline:
 * @expander: a #GtkExpander
 * @use_underline: %TRUE if underlines in the text indicate mnemonics
 *
 * If true, an underline in the text of the expander label indicates
 * the next character should be used for the mnemonic accelerator key.
 */
void
gtk_expander_set_use_underline (GtkExpander *expander,
                                gboolean     use_underline)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  g_return_if_fail (GTK_IS_EXPANDER (expander));

  use_underline = use_underline != FALSE;

  if (priv->use_underline != use_underline)
    {
      priv->use_underline = use_underline;

      if (GTK_IS_LABEL (priv->label_widget))
        gtk_label_set_use_underline (GTK_LABEL (priv->label_widget), use_underline);

      g_object_notify (G_OBJECT (expander), "use-underline");
    }
}

/**
 * gtk_expander_get_use_underline:
 * @expander: a #GtkExpander
 *
 * Returns whether an embedded underline in the expander label
 * indicates a mnemonic. See gtk_expander_set_use_underline().
 *
 * Returns: %TRUE if an embedded underline in the expander
 *     label indicates the mnemonic accelerator keys
 */
gboolean
gtk_expander_get_use_underline (GtkExpander *expander)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return priv->use_underline;
}

/**
 * gtk_expander_set_use_markup:
 * @expander: a #GtkExpander
 * @use_markup: %TRUE if the label’s text should be parsed for markup
 *
 * Sets whether the text of the label contains markup in
 * [Pango’s text markup language][PangoMarkupFormat].
 * See gtk_label_set_markup().
 */
void
gtk_expander_set_use_markup (GtkExpander *expander,
                             gboolean     use_markup)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  g_return_if_fail (GTK_IS_EXPANDER (expander));

  use_markup = use_markup != FALSE;

  if (priv->use_markup != use_markup)
    {
      priv->use_markup = use_markup;

      if (GTK_IS_LABEL (priv->label_widget))
        gtk_label_set_use_markup (GTK_LABEL (priv->label_widget), use_markup);

      g_object_notify (G_OBJECT (expander), "use-markup");
    }
}

/**
 * gtk_expander_get_use_markup:
 * @expander: a #GtkExpander
 *
 * Returns whether the label’s text is interpreted as marked up with
 * the [Pango text markup language][PangoMarkupFormat].
 * See gtk_expander_set_use_markup().
 *
 * Returns: %TRUE if the label’s text will be parsed for markup
 */
gboolean
gtk_expander_get_use_markup (GtkExpander *expander)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return priv->use_markup;
}

/**
 * gtk_expander_set_label_widget:
 * @expander: a #GtkExpander
 * @label_widget: (nullable): the new label widget
 *
 * Set the label widget for the expander. This is the widget
 * that will appear embedded alongside the expander arrow.
 */
void
gtk_expander_set_label_widget (GtkExpander *expander,
                               GtkWidget   *label_widget)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_EXPANDER (expander));
  g_return_if_fail (label_widget == NULL || GTK_IS_WIDGET (label_widget));
  g_return_if_fail (label_widget == NULL || gtk_widget_get_parent (label_widget) == NULL);

  if (priv->label_widget == label_widget)
    return;

  if (priv->label_widget)
    {
      gtk_container_remove (GTK_CONTAINER (priv->title_widget), priv->label_widget);
    }

  priv->label_widget = label_widget;
  widget = GTK_WIDGET (expander);

  if (label_widget)
    {
      priv->label_widget = label_widget;

      gtk_container_add (GTK_CONTAINER (priv->title_widget), label_widget);
    }

  if (gtk_widget_get_visible (widget))
    gtk_widget_queue_resize (widget);

  g_object_freeze_notify (G_OBJECT (expander));
  g_object_notify (G_OBJECT (expander), "label-widget");
  g_object_notify (G_OBJECT (expander), "label");
  g_object_thaw_notify (G_OBJECT (expander));
}

/**
 * gtk_expander_get_label_widget:
 * @expander: a #GtkExpander
 *
 * Retrieves the label widget for the frame. See
 * gtk_expander_set_label_widget().
 *
 * Returns: (nullable) (transfer none): the label widget,
 *     or %NULL if there is none
 */
GtkWidget *
gtk_expander_get_label_widget (GtkExpander *expander)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  g_return_val_if_fail (GTK_IS_EXPANDER (expander), NULL);

  return priv->label_widget;
}

/**
 * gtk_expander_set_resize_toplevel:
 * @expander: a #GtkExpander
 * @resize_toplevel: whether to resize the toplevel
 *
 * Sets whether the expander will resize the toplevel widget
 * containing the expander upon resizing and collpasing.
 */
void
gtk_expander_set_resize_toplevel (GtkExpander *expander,
                                  gboolean     resize_toplevel)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  g_return_if_fail (GTK_IS_EXPANDER (expander));

  if (priv->resize_toplevel != resize_toplevel)
    {
      priv->resize_toplevel = resize_toplevel ? TRUE : FALSE;
      g_object_notify (G_OBJECT (expander), "resize-toplevel");
    }
}

/**
 * gtk_expander_get_resize_toplevel:
 * @expander: a #GtkExpander
 *
 * Returns whether the expander will resize the toplevel widget
 * containing the expander upon resizing and collpasing.
 *
 * Returns: the “resize toplevel” setting.
 */
gboolean
gtk_expander_get_resize_toplevel (GtkExpander *expander)
{
  GtkExpanderPrivate *priv = gtk_expander_get_instance_private (expander);

  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return priv->resize_toplevel;
}
