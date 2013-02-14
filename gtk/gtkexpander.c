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
 * Normally you use an expander as you would use any other descendant
 * of #GtkBin; you create the child widget and use gtk_container_add()
 * to add it to the expander. When the expander is toggled, it will take
 * care of showing and hiding the child automatically.
 *
 * <refsect2 id="expander-special-usage">
 * <title>Special Usage</title>
 * <para>
 * There are situations in which you may prefer to show and hide the
 * expanded widget yourself, such as when you want to actually create
 * the widget at expansion time. In this case, create a #GtkExpander
 * but do not add a child to it. The expander widget has an
 * #GtkExpander:expanded property which can be used to monitor
 * its expansion state. You should watch this property with a signal
 * connection as follows:
 * </para>
 * <informalexample>
 * <programlisting id="expander-callback-example">
 * expander = gtk_expander_new_with_mnemonic ("_More Options");
 * g_signal_connect (expander, "notify::expanded",
 *                   G_CALLBACK (expander_callback), NULL);
 *
 * ...
 *
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
 *       /&ast; Show or create widgets &ast;/
 *     }
 *   else
 *     {
 *       /&ast; Hide or destroy widgets &ast;/
 *     }
 * }
 * </programlisting>
 * </informalexample>
 * </refsect2>
 * <refsect2 id="GtkExpander-BUILDER-UI">
 * <title>GtkExpander as GtkBuildable</title>
 * <para>
 * The GtkExpander implementation of the GtkBuildable interface
 * supports placing a child in the label position by specifying
 * "label" as the "type" attribute of a &lt;child&gt; element.
 * A normal content child can be specified without specifying
 * a &lt;child&gt; type attribute.
 * </para>
 * <example>
 * <title>A UI definition fragment with GtkExpander</title>
 * <programlisting><![CDATA[
 * <object class="GtkExpander">
 *   <child type="label">
 *     <object class="GtkLabel" id="expander-label"/>
 *   </child>
 *   <child>
 *     <object class="GtkEntry" id="expander-content"/>
 *   </child>
 * </object>
 * ]]></programlisting>
 * </example>
 * </refsect2>
 *
 */

#include "config.h"

#include <string.h>

#include "gtkexpander.h"

#include "gtklabel.h"
#include "gtkbuildable.h"
#include "gtkcontainer.h"
#include "gtkmarshalers.h"
#include "gtkmain.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkdnd.h"
#include "a11y/gtkexpanderaccessible.h"


#define DEFAULT_EXPANDER_SIZE 10
#define DEFAULT_EXPANDER_SPACING 2

enum
{
  PROP_0,
  PROP_EXPANDED,
  PROP_LABEL,
  PROP_USE_UNDERLINE,
  PROP_USE_MARKUP,
  PROP_SPACING,
  PROP_LABEL_WIDGET,
  PROP_LABEL_FILL,
  PROP_RESIZE_TOPLEVEL
};

struct _GtkExpanderPrivate
{
  GtkWidget        *label_widget;
  GdkWindow        *event_window;
  gint              spacing;

  guint             expand_timer;

  guint             expanded : 1;
  guint             use_underline : 1;
  guint             use_markup : 1; 
  guint             button_down : 1;
  guint             prelight : 1;
  guint             label_fill : 1;
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
static void     gtk_expander_realize        (GtkWidget        *widget);
static void     gtk_expander_unrealize      (GtkWidget        *widget);
static void     gtk_expander_size_allocate  (GtkWidget        *widget,
                                             GtkAllocation    *allocation);
static void     gtk_expander_map            (GtkWidget        *widget);
static void     gtk_expander_unmap          (GtkWidget        *widget);
static gboolean gtk_expander_draw           (GtkWidget        *widget,
                                             cairo_t          *cr);
static gboolean gtk_expander_button_press   (GtkWidget        *widget,
                                             GdkEventButton   *event);
static gboolean gtk_expander_button_release (GtkWidget        *widget,
                                             GdkEventButton   *event);
static gboolean gtk_expander_enter_notify   (GtkWidget        *widget,
                                             GdkEventCrossing *event);
static gboolean gtk_expander_leave_notify   (GtkWidget        *widget,
                                             GdkEventCrossing *event);
static gboolean gtk_expander_focus          (GtkWidget        *widget,
                                             GtkDirectionType  direction);
static void     gtk_expander_grab_notify    (GtkWidget        *widget,
                                             gboolean          was_grabbed);
static void     gtk_expander_state_flags_changed  (GtkWidget     *widget,
                                                   GtkStateFlags  previous_state);
static gboolean gtk_expander_drag_motion    (GtkWidget        *widget,
                                             GdkDragContext   *context,
                                             gint              x,
                                             gint              y,
                                             guint             time);
static void     gtk_expander_drag_leave     (GtkWidget        *widget,
                                             GdkDragContext   *context,
                                             guint             time);

static void gtk_expander_add    (GtkContainer *container,
                                 GtkWidget    *widget);
static void gtk_expander_remove (GtkContainer *container,
                                 GtkWidget    *widget);
static void gtk_expander_forall (GtkContainer *container,
                                 gboolean        include_internals,
                                 GtkCallback     callback,
                                 gpointer        callback_data);

static void gtk_expander_activate (GtkExpander *expander);

static void get_expander_bounds (GtkExpander  *expander,
                                 GdkRectangle *rect);

/* GtkBuildable */
static void gtk_expander_buildable_init           (GtkBuildableIface *iface);
static void gtk_expander_buildable_add_child      (GtkBuildable *buildable,
                                                   GtkBuilder   *builder,
                                                   GObject      *child,
                                                   const gchar  *type);


/* GtkWidget      */
static void  gtk_expander_get_preferred_width             (GtkWidget           *widget,
                                                           gint                *minimum_size,
                                                           gint                *natural_size);
static void  gtk_expander_get_preferred_height            (GtkWidget           *widget,
                                                           gint                *minimum_size,
                                                           gint                *natural_size);
static void  gtk_expander_get_preferred_height_for_width  (GtkWidget           *layout,
                                                           gint                 width,
                                                           gint                *minimum_height,
                                                           gint                *natural_height);
static void  gtk_expander_get_preferred_width_for_height  (GtkWidget           *layout,
                                                           gint                 width,
                                                           gint                *minimum_height,
                                                           gint                *natural_height);

G_DEFINE_TYPE_WITH_CODE (GtkExpander, gtk_expander, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_expander_buildable_init))

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
  widget_class->realize              = gtk_expander_realize;
  widget_class->unrealize            = gtk_expander_unrealize;
  widget_class->size_allocate        = gtk_expander_size_allocate;
  widget_class->map                  = gtk_expander_map;
  widget_class->unmap                = gtk_expander_unmap;
  widget_class->draw                 = gtk_expander_draw;
  widget_class->button_press_event   = gtk_expander_button_press;
  widget_class->button_release_event = gtk_expander_button_release;
  widget_class->enter_notify_event   = gtk_expander_enter_notify;
  widget_class->leave_notify_event   = gtk_expander_leave_notify;
  widget_class->focus                = gtk_expander_focus;
  widget_class->grab_notify          = gtk_expander_grab_notify;
  widget_class->state_flags_changed  = gtk_expander_state_flags_changed;
  widget_class->drag_motion          = gtk_expander_drag_motion;
  widget_class->drag_leave           = gtk_expander_drag_leave;
  widget_class->get_preferred_width            = gtk_expander_get_preferred_width;
  widget_class->get_preferred_height           = gtk_expander_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_expander_get_preferred_height_for_width;
  widget_class->get_preferred_width_for_height = gtk_expander_get_preferred_width_for_height;

  container_class->add    = gtk_expander_add;
  container_class->remove = gtk_expander_remove;
  container_class->forall = gtk_expander_forall;

  klass->activate = gtk_expander_activate;

  g_type_class_add_private (klass, sizeof (GtkExpanderPrivate));

  g_object_class_install_property (gobject_class,
                                   PROP_EXPANDED,
                                   g_param_spec_boolean ("expanded",
                                                         P_("Expanded"),
                                                         P_("Whether the expander has been opened to reveal the child widget"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        P_("Label"),
                                                        P_("Text of the expander's label"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_USE_UNDERLINE,
                                   g_param_spec_boolean ("use-underline",
                                                         P_("Use underline"),
                                                         P_("If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_USE_MARKUP,
                                   g_param_spec_boolean ("use-markup",
                                                         P_("Use markup"),
                                                         P_("The text of the label includes XML markup. See pango_parse_markup()"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_SPACING,
                                   g_param_spec_int ("spacing",
                                                     P_("Spacing"),
                                                     P_("Space to put between the label and the child"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL_WIDGET,
                                   g_param_spec_object ("label-widget",
                                                        P_("Label widget"),
                                                        P_("A widget to display in place of the usual expander label"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL_FILL,
                                   g_param_spec_boolean ("label-fill",
                                                         P_("Label fill"),
                                                         P_("Whether the label widget should fill all available horizontal space"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * GtkExpander:resize-toplevel:
   *
   * When this property is %TRUE, the expander will resize the toplevel
   * widget containing the expander upon expanding and collapsing.
   *
   * Since: 3.2
   */
  g_object_class_install_property (gobject_class,
                                   PROP_RESIZE_TOPLEVEL,
                                   g_param_spec_boolean ("resize-toplevel",
                                                         P_("Resize toplevel"),
                                                         P_("Whether the expander will resize the toplevel window upon expanding and collapsing"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("expander-size",
                                                             P_("Expander Size"),
                                                             P_("Size of the expander arrow"),
                                                             0,
                                                             G_MAXINT,
                                                             DEFAULT_EXPANDER_SIZE,
                                                             GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("expander-spacing",
                                                             P_("Indicator Spacing"),
                                                             P_("Spacing around expander arrow"),
                                                             0,
                                                             G_MAXINT,
                                                             DEFAULT_EXPANDER_SPACING,
                                                             GTK_PARAM_READABLE));

  widget_class->activate_signal =
    g_signal_new (I_("activate"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkExpanderClass, activate),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_EXPANDER_ACCESSIBLE);
}

static void
gtk_expander_init (GtkExpander *expander)
{
  GtkExpanderPrivate *priv;

  expander->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (expander,
                                                       GTK_TYPE_EXPANDER,
                                                       GtkExpanderPrivate);

  gtk_widget_set_can_focus (GTK_WIDGET (expander), TRUE);
  gtk_widget_set_has_window (GTK_WIDGET (expander), FALSE);

  priv->label_widget = NULL;
  priv->event_window = NULL;
  priv->spacing = 0;

  priv->expanded = FALSE;
  priv->use_underline = FALSE;
  priv->use_markup = FALSE;
  priv->button_down = FALSE;
  priv->prelight = FALSE;
  priv->label_fill = FALSE;
  priv->expand_timer = 0;
  priv->resize_toplevel = 0;

  gtk_drag_dest_set (GTK_WIDGET (expander), 0, NULL, 0, 0);
  gtk_drag_dest_set_track_motion (GTK_WIDGET (expander), TRUE);
}

static void
gtk_expander_buildable_add_child (GtkBuildable  *buildable,
                                  GtkBuilder    *builder,
                                  GObject       *child,
                                  const gchar   *type)
{
  if (!type)
    gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
  else if (strcmp (type, "label") == 0)
    gtk_expander_set_label_widget (GTK_EXPANDER (buildable), GTK_WIDGET (child));
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (GTK_EXPANDER (buildable), type);
}

static void
gtk_expander_buildable_init (GtkBuildableIface *iface)
{
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
    case PROP_SPACING:
      gtk_expander_set_spacing (expander, g_value_get_int (value));
      break;
    case PROP_LABEL_WIDGET:
      gtk_expander_set_label_widget (expander, g_value_get_object (value));
      break;
    case PROP_LABEL_FILL:
      gtk_expander_set_label_fill (expander, g_value_get_boolean (value));
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
  GtkExpanderPrivate *priv = expander->priv;

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
    case PROP_SPACING:
      g_value_set_int (value, priv->spacing);
      break;
    case PROP_LABEL_WIDGET:
      g_value_set_object (value,
                          priv->label_widget ?
                          G_OBJECT (priv->label_widget) : NULL);
      break;
    case PROP_LABEL_FILL:
      g_value_set_boolean (value, priv->label_fill);
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
  GtkExpanderPrivate *priv = GTK_EXPANDER (widget)->priv;

  if (priv->expand_timer)
    {
      g_source_remove (priv->expand_timer);
      priv->expand_timer = 0;
    }

  GTK_WIDGET_CLASS (gtk_expander_parent_class)->destroy (widget);
}

static void
gtk_expander_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GtkExpanderPrivate *priv;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;
  GdkRectangle expander_rect;
  gint label_height;

  priv = GTK_EXPANDER (widget)->priv;

  gtk_widget_set_realized (widget, TRUE);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  get_expander_bounds (GTK_EXPANDER (widget), &expander_rect);

  if (priv->label_widget && gtk_widget_get_visible (priv->label_widget))
    {
      GtkRequisition label_requisition;

      gtk_widget_get_preferred_size (priv->label_widget,
                                     &label_requisition, NULL);
      label_height = label_requisition.height;
    }
  else
    label_height = 0;

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x + border_width;
  attributes.y = allocation.y + border_width;
  attributes.width = MAX (allocation.width - 2 * border_width, 1);
  attributes.height = MAX (expander_rect.height, label_height - 2 * border_width);
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget)
                          | GDK_BUTTON_PRESS_MASK
                          | GDK_BUTTON_RELEASE_MASK
                          | GDK_ENTER_NOTIFY_MASK
                          | GDK_LEAVE_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  priv->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                       &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);
}

static void
gtk_expander_unrealize (GtkWidget *widget)
{
  GtkExpanderPrivate *priv = GTK_EXPANDER (widget)->priv;

  if (priv->event_window)
    {
      gtk_widget_unregister_window (widget, priv->event_window);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gtk_expander_parent_class)->unrealize (widget);
}

static void
get_expander_bounds (GtkExpander  *expander,
                     GdkRectangle *rect)
{
  GtkAllocation allocation;
  GtkWidget *widget;
  GtkExpanderPrivate *priv;
  gint border_width;
  gint expander_size;
  gint expander_spacing;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;
  gboolean ltr;

  widget = GTK_WIDGET (expander);
  priv = expander->priv;

  gtk_widget_get_allocation (widget, &allocation);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_widget_style_get (widget,
                        "interior-focus", &interior_focus,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        "expander-size", &expander_size,
                        "expander-spacing", &expander_spacing,
                        NULL);

  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  rect->x = allocation.x + border_width;
  rect->y = allocation.y + border_width;

  if (ltr)
    rect->x += expander_spacing;
  else
    rect->x += allocation.width - 2 * border_width -
               expander_spacing - expander_size;

  if (priv->label_widget && gtk_widget_get_visible (priv->label_widget))
    {
      GtkAllocation label_allocation;

      gtk_widget_get_allocation (priv->label_widget, &label_allocation);

      if (expander_size < label_allocation.height)
        rect->y += focus_width + focus_pad + (label_allocation.height - expander_size) / 2;
      else
        rect->y += expander_spacing;
    }
  else
    {
      rect->y += expander_spacing;
    }

  if (!interior_focus)
    {
      if (ltr)
        rect->x += focus_width + focus_pad;
      else
        rect->x -= focus_width + focus_pad;
      rect->y += focus_width + focus_pad;
    }

  rect->width = rect->height = expander_size;
}

static void
gtk_expander_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  GtkExpander *expander;
  GtkWidget *child;
  GtkExpanderPrivate *priv;
  gboolean child_visible = FALSE;
  guint border_width;
  gint expander_size;
  gint expander_spacing;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;
  gint label_height, top_min_height;
  gint label_xpad, label_xoffset;
  gint child_ypad, child_yoffset;

  expander = GTK_EXPANDER (widget);
  child    = gtk_bin_get_child (GTK_BIN (widget));
  priv     = expander->priv;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_widget_set_allocation (widget, allocation);

  gtk_widget_style_get (widget,
                        "interior-focus", &interior_focus,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        "expander-size", &expander_size,
                        "expander-spacing", &expander_spacing,
                        NULL);


  /* Calculate some offsets/padding first */
  label_xoffset = border_width + expander_size + focus_width + 2 * expander_spacing + focus_pad;
  label_xpad = 2 * border_width + expander_size + 2 * focus_width + 2 * expander_spacing + 2 * focus_pad;

  child_yoffset  = border_width + priv->spacing + (interior_focus ? 0 : 2 * focus_width + 2 * focus_pad);
  child_ypad     = 2 * border_width + priv->spacing + (interior_focus ? 0 : 2 * focus_width + 2 * focus_pad);
  top_min_height = 2 * expander_spacing + expander_size;

  child_visible = (child && gtk_widget_get_child_visible (child));

  if (priv->label_widget && gtk_widget_get_visible (priv->label_widget))
    {
      GtkAllocation label_allocation;
      gint          natural_label_width;
      gboolean ltr;

      gtk_widget_get_preferred_width (priv->label_widget, NULL, &natural_label_width);

      if (priv->label_fill)
        label_allocation.width = allocation->width - label_xpad;
      else
        label_allocation.width = MIN (natural_label_width, allocation->width - label_xpad);
      label_allocation.width = MAX (label_allocation.width, 1);

      /* We distribute the minimum height to the label widget and prioritize
       * the child widget giving it the remaining height
       */
      gtk_widget_get_preferred_height_for_width (priv->label_widget,
                                                 label_allocation.width, &label_height, NULL);

      ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

      if (priv->label_fill)
        label_allocation.x = allocation->x + label_xoffset;
      else if (ltr)
        label_allocation.x = allocation->x + label_xoffset;
      else
        label_allocation.x = allocation->x + allocation->width -
                             (label_allocation.width + label_xoffset);

      label_allocation.y = allocation->y + border_width + focus_width + focus_pad;
      label_allocation.height = MIN (label_height,
                                     allocation->height - 2 * border_width -
                                     2 * focus_width - 2 * focus_pad -
                                     (child_visible ? priv->spacing : 0));
      label_allocation.height = MAX (label_allocation.height, 1);

      gtk_widget_size_allocate (priv->label_widget, &label_allocation);

      label_height = label_allocation.height;
    }
  else
    {
      label_height = 0;
    }

  if (gtk_widget_get_realized (widget))
    {
      GdkRectangle rect;

      get_expander_bounds (expander, &rect);

      gdk_window_move_resize (priv->event_window,
                              allocation->x + border_width,
                              allocation->y + border_width,
                              MAX (allocation->width - 2 * border_width, 1),
                              MAX (rect.height, label_height - 2 * border_width));
    }

  if (child_visible)
    {
      GtkAllocation child_allocation;
      gint top_height;

      top_height = MAX (top_min_height,
                        label_height + (interior_focus ? 2 * focus_width + 2 * focus_pad : 0));

      child_allocation.x = allocation->x + border_width;
      child_allocation.y = allocation->y + top_height + child_yoffset;

      child_allocation.width = MAX (allocation->width - 2 * border_width, 1);
      child_allocation.height = allocation->height - top_height - child_ypad;
      child_allocation.height = MAX (child_allocation.height, 1);

      gtk_widget_size_allocate (child, &child_allocation);
    }
}

static void
gtk_expander_map (GtkWidget *widget)
{
  GtkExpanderPrivate *priv = GTK_EXPANDER (widget)->priv;

  if (priv->label_widget)
    gtk_widget_map (priv->label_widget);

  GTK_WIDGET_CLASS (gtk_expander_parent_class)->map (widget);

  if (priv->event_window)
    gdk_window_show (priv->event_window);
}

static void
gtk_expander_unmap (GtkWidget *widget)
{
  GtkExpanderPrivate *priv = GTK_EXPANDER (widget)->priv;

  if (priv->event_window)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gtk_expander_parent_class)->unmap (widget);

  if (priv->label_widget)
    gtk_widget_unmap (priv->label_widget);
}

static void
gtk_expander_paint_prelight (GtkExpander *expander,
                             cairo_t     *cr)
{
  GtkAllocation allocation;
  GtkWidget *widget;
  GtkContainer *container;
  GtkExpanderPrivate *priv;
  GdkRectangle area;
  GtkStyleContext *context;
  gboolean interior_focus;
  int focus_width;
  int focus_pad;
  int expander_size;
  int expander_spacing;
  guint border_width;

  priv = expander->priv;
  widget = GTK_WIDGET (expander);
  container = GTK_CONTAINER (expander);

  gtk_widget_style_get (widget,
                        "interior-focus", &interior_focus,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        "expander-size", &expander_size,
                        "expander-spacing", &expander_spacing,
                        NULL);

  gtk_widget_get_allocation (widget, &allocation);

  border_width = gtk_container_get_border_width (container);
  area.x = border_width;
  area.y = border_width;
  area.width = allocation.width - (2 * border_width);

  if (priv->label_widget && gtk_widget_get_visible (priv->label_widget))
    {
      GtkAllocation label_widget_allocation;

      gtk_widget_get_allocation (priv->label_widget, &label_widget_allocation);
      area.height = label_widget_allocation.height;
    }
  else
    area.height = 0;

  area.height += interior_focus ? (focus_width + focus_pad) * 2 : 0;
  area.height = MAX (area.height, expander_size + 2 * expander_spacing);
  area.height += !interior_focus ? (focus_width + focus_pad) * 2 : 0;

  context = gtk_widget_get_style_context (widget);
  gtk_render_background (context, cr,
                         area.x, area.y,
                         area.width, area.height);
}

static void
gtk_expander_paint (GtkExpander *expander,
                    cairo_t     *cr)
{
  GtkExpanderPrivate *priv = expander->priv;
  GtkWidget *widget;
  GdkRectangle clip;
  GtkAllocation allocation;
  GtkStyleContext *context;
  GtkStateFlags state = 0;
  gint size;

  widget = GTK_WIDGET (expander);
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  get_expander_bounds (expander, &clip);
  gtk_widget_get_allocation (widget, &allocation);

  gtk_style_context_save (context);

  state &= ~(GTK_STATE_FLAG_PRELIGHT);
  if (expander->priv->prelight)
    {
      state |= GTK_STATE_FLAG_PRELIGHT;
      gtk_style_context_set_state (context, state);
      gtk_expander_paint_prelight (expander, cr);
    }

  gtk_widget_style_get (widget, "expander-size", &size, NULL);

  /* Set active flag as per the expanded state */
  if (priv->expanded)
    state |= GTK_STATE_FLAG_ACTIVE;
  else
    state &= ~(GTK_STATE_FLAG_ACTIVE);

  gtk_style_context_set_state (context, state);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_EXPANDER);

  gtk_render_expander (context, cr,
                       clip.x - allocation.x,
                       clip.y - allocation.y,
                       size, size);

  gtk_style_context_restore (context);
}

static void
gtk_expander_paint_focus (GtkExpander *expander,
                          cairo_t     *cr)
{
  GtkWidget *widget;
  GtkExpanderPrivate *priv;
  GdkRectangle rect;
  GtkStyleContext *context;
  gint x, y, width, height;
  gboolean interior_focus;
  gint border_width;
  gint focus_width;
  gint focus_pad;
  gint expander_size;
  gint expander_spacing;
  gboolean ltr;
  GtkAllocation allocation;

  widget = GTK_WIDGET (expander);
  priv = expander->priv;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  gtk_widget_get_allocation (widget, &allocation);

  gtk_widget_style_get (widget,
                        "interior-focus", &interior_focus,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        "expander-size", &expander_size,
                        "expander-spacing", &expander_spacing,
                        NULL);

  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  width = height = 0;

  if (priv->label_widget)
    {
      if (gtk_widget_get_visible (priv->label_widget))
        {
          GtkAllocation label_allocation;

          gtk_widget_get_allocation (priv->label_widget, &label_allocation);
          width  = label_allocation.width;
          height = label_allocation.height;
        }

      width  += 2 * focus_pad + 2 * focus_width;
      height += 2 * focus_pad + 2 * focus_width;

      x = border_width;
      y = border_width;

      if (ltr)
        {
          if (interior_focus)
            x += expander_spacing * 2 + expander_size;
        }
      else
        {
          x += allocation.width - 2 * border_width
            - expander_spacing * 2 - expander_size - width;
        }

      if (!interior_focus)
        {
          width += expander_size + 2 * expander_spacing;
          height = MAX (height, expander_size + 2 * expander_spacing);
        }
    }
  else
    {
      get_expander_bounds (expander, &rect);

      x = rect.x - allocation.x - focus_pad;
      y = rect.y - allocation.y - focus_pad;
      width = rect.width + 2 * focus_pad;
      height = rect.height + 2 * focus_pad;
    }

  context = gtk_widget_get_style_context (widget);
  gtk_render_focus (context, cr,
                    x, y, width, height);
}

static gboolean
gtk_expander_draw (GtkWidget *widget,
                   cairo_t   *cr)
{
  GtkExpander *expander = GTK_EXPANDER (widget);

  gtk_expander_paint (expander, cr);

  if (gtk_widget_has_visible_focus (widget))
    gtk_expander_paint_focus (expander, cr);

  GTK_WIDGET_CLASS (gtk_expander_parent_class)->draw (widget, cr);

  return FALSE;
}

static gboolean
gtk_expander_button_press (GtkWidget      *widget,
                           GdkEventButton *event)
{
  GtkExpander *expander = GTK_EXPANDER (widget);

  if (event->button == GDK_BUTTON_PRIMARY && event->window == expander->priv->event_window)
    {
      expander->priv->button_down = TRUE;
      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_expander_button_release (GtkWidget      *widget,
                             GdkEventButton *event)
{
  GtkExpander *expander = GTK_EXPANDER (widget);

  if (event->button == GDK_BUTTON_PRIMARY && expander->priv->button_down)
    {
      gtk_widget_activate (widget);
      expander->priv->button_down = FALSE;
      return TRUE;
    }

  return FALSE;
}

static void
gtk_expander_grab_notify (GtkWidget *widget,
                          gboolean   was_grabbed)
{
  if (!was_grabbed)
    GTK_EXPANDER (widget)->priv->button_down = FALSE;
}

static void
gtk_expander_state_flags_changed (GtkWidget    *widget,
                                  GtkStateFlags  previous_state)
{
  if (!gtk_widget_is_sensitive (widget))
    GTK_EXPANDER (widget)->priv->button_down = FALSE;
}

static void
gtk_expander_redraw_expander (GtkExpander *expander)
{
  GtkAllocation allocation;
  GtkWidget *widget = GTK_WIDGET (expander);

  if (gtk_widget_get_realized (widget))
    {
      gtk_widget_get_allocation (widget, &allocation);
      gdk_window_invalidate_rect (gtk_widget_get_window (widget), &allocation, FALSE);
    }
}

static gboolean
gtk_expander_enter_notify (GtkWidget        *widget,
                           GdkEventCrossing *event)
{
  GtkExpander *expander = GTK_EXPANDER (widget);

  if (event->window == expander->priv->event_window &&
      event->detail != GDK_NOTIFY_INFERIOR)
    {
      expander->priv->prelight = TRUE;

      if (expander->priv->label_widget)
        gtk_widget_set_state_flags (expander->priv->label_widget,
                                    GTK_STATE_FLAG_PRELIGHT,
                                    FALSE);

      gtk_expander_redraw_expander (expander);
    }

  return FALSE;
}

static gboolean
gtk_expander_leave_notify (GtkWidget        *widget,
                           GdkEventCrossing *event)
{
  GtkExpander *expander = GTK_EXPANDER (widget);

  if (event->window == expander->priv->event_window &&
      event->detail != GDK_NOTIFY_INFERIOR)
    {
      expander->priv->prelight = FALSE;

      if (expander->priv->label_widget)
        gtk_widget_unset_state_flags (expander->priv->label_widget,
                                      GTK_STATE_FLAG_PRELIGHT);

      gtk_expander_redraw_expander (expander);
    }

  return FALSE;
}

static gboolean
expand_timeout (gpointer data)
{
  GtkExpander *expander = GTK_EXPANDER (data);
  GtkExpanderPrivate *priv = expander->priv;

  priv->expand_timer = 0;
  gtk_expander_set_expanded (expander, TRUE);

  return FALSE;
}

static gboolean
gtk_expander_drag_motion (GtkWidget        *widget,
                          GdkDragContext   *context,
                          gint              x,
                          gint              y,
                          guint             time)
{
  GtkExpander *expander = GTK_EXPANDER (widget);
  GtkExpanderPrivate *priv = expander->priv;

  if (!priv->expanded && !priv->expand_timer)
    {
      GtkSettings *settings;
      guint timeout;

      settings = gtk_widget_get_settings (widget);
      g_object_get (settings, "gtk-timeout-expand", &timeout, NULL);

      priv->expand_timer = gdk_threads_add_timeout (timeout, (GSourceFunc) expand_timeout, expander);
    }

  return TRUE;
}

static void
gtk_expander_drag_leave (GtkWidget      *widget,
                         GdkDragContext *context,
                         guint           time)
{
  GtkExpander *expander = GTK_EXPANDER (widget);
  GtkExpanderPrivate *priv = expander->priv;

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

  current_focus = gtk_container_get_focus_child (GTK_CONTAINER (expander));

  if (!current_focus)
    return FALSE;

  return gtk_widget_child_focus (current_focus, direction);
}

static gboolean
focus_in_site (GtkExpander      *expander,
               FocusSite         site,
               GtkDirectionType  direction)
{
  switch (site)
    {
    case FOCUS_WIDGET:
      gtk_widget_grab_focus (GTK_WIDGET (expander));
      return TRUE;
    case FOCUS_LABEL:
      if (expander->priv->label_widget)
        return gtk_widget_child_focus (expander->priv->label_widget, direction);
      else
        return FALSE;
    case FOCUS_CHILD:
      {
        GtkWidget *child = gtk_bin_get_child (GTK_BIN (expander));

        if (child && gtk_widget_get_child_visible (child))
          return gtk_widget_child_focus (child, direction);
        else
          return FALSE;
      }
    case FOCUS_NONE:
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
          return FOCUS_NONE;
        }
      break;
    }

  g_assert_not_reached ();
  return FOCUS_NONE;
}

static void
gtk_expander_resize_toplevel (GtkExpander *expander)
{
  GtkExpanderPrivate *priv = expander->priv;
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (expander));

  if (child && priv->resize_toplevel &&
      gtk_widget_get_realized (GTK_WIDGET (expander)))
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (expander));

      if (toplevel && gtk_widget_get_realized (toplevel))
        {
          GtkAllocation toplevel_allocation;

          gtk_widget_get_allocation (toplevel, &toplevel_allocation);

          if (priv->expanded)
            {
              GtkRequisition child_requisition;

              gtk_widget_get_preferred_size (child, &child_requisition, NULL);

              toplevel_allocation.height += child_requisition.height;
            }
          else
            {
              GtkAllocation child_allocation;

              gtk_widget_get_allocation (child, &child_allocation);

              toplevel_allocation.height -= child_allocation.height;
            }

          gtk_window_resize (GTK_WINDOW (toplevel),
                             toplevel_allocation.width,
                             toplevel_allocation.height);
        }
    }
}

static gboolean
gtk_expander_focus (GtkWidget        *widget,
                    GtkDirectionType  direction)
{
  GtkExpander *expander = GTK_EXPANDER (widget);

  if (!focus_current_site (expander, direction))
    {
      GtkWidget *old_focus_child;
      gboolean widget_is_focus;
      FocusSite site = FOCUS_NONE;

      widget_is_focus = gtk_widget_is_focus (widget);
      old_focus_child = gtk_container_get_focus_child (GTK_CONTAINER (widget));

      if (old_focus_child && old_focus_child == expander->priv->label_widget)
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
  GTK_CONTAINER_CLASS (gtk_expander_parent_class)->add (container, widget);

  gtk_widget_set_child_visible (widget, GTK_EXPANDER (container)->priv->expanded);
  gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gtk_expander_remove (GtkContainer *container,
                     GtkWidget    *widget)
{
  GtkExpander *expander = GTK_EXPANDER (container);

  if (GTK_EXPANDER (expander)->priv->label_widget == widget)
    gtk_expander_set_label_widget (expander, NULL);
  else
    GTK_CONTAINER_CLASS (gtk_expander_parent_class)->remove (container, widget);
}

static void
gtk_expander_forall (GtkContainer *container,
                     gboolean      include_internals,
                     GtkCallback   callback,
                     gpointer      callback_data)
{
  GtkBin *bin = GTK_BIN (container);
  GtkExpanderPrivate *priv = GTK_EXPANDER (container)->priv;
  GtkWidget *child;

  child = gtk_bin_get_child (bin);
  if (child)
    (* callback) (child, callback_data);

  if (priv->label_widget)
    (* callback) (priv->label_widget, callback_data);
}

static void
gtk_expander_activate (GtkExpander *expander)
{
  gtk_expander_set_expanded (expander, !expander->priv->expanded);
}


static void
gtk_expander_get_preferred_width (GtkWidget *widget,
                                  gint      *minimum_size,
                                  gint      *natural_size)
{
  GtkExpander *expander;
  GtkWidget *child;
  GtkExpanderPrivate *priv;
  gint border_width;
  gint expander_size;
  gint expander_spacing;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;

  child = gtk_bin_get_child (GTK_BIN (widget));
  expander = GTK_EXPANDER (widget);
  priv = expander->priv;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_widget_style_get (GTK_WIDGET (widget),
                        "interior-focus", &interior_focus,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        "expander-size", &expander_size,
                        "expander-spacing", &expander_spacing,
                        NULL);

  *minimum_size = *natural_size =
    expander_size + 2 * expander_spacing +
    2 * focus_width + 2 * focus_pad;

  if (priv->label_widget && gtk_widget_get_visible (priv->label_widget))
    {
      gint label_min, label_nat;

      gtk_widget_get_preferred_width (priv->label_widget,
                                      &label_min, &label_nat);

      *minimum_size += label_min;
      *natural_size += label_nat;
    }

  if (child && gtk_widget_get_child_visible (child))
    {
      gint child_min, child_nat;

      gtk_widget_get_preferred_width (child,
                                      &child_min, &child_nat);

      *minimum_size = MAX (*minimum_size, child_min);
      *natural_size = MAX (*natural_size, child_nat);

    }

  *minimum_size += 2 * border_width;
  *natural_size += 2 * border_width;
}

static void
gtk_expander_get_preferred_height (GtkWidget *widget,
                                   gint      *minimum_size,
                                   gint      *natural_size)
{
  GtkExpander *expander;
  GtkWidget *child;
  GtkExpanderPrivate *priv;
  gint border_width;
  gint expander_size;
  gint expander_spacing;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;

  child = gtk_bin_get_child (GTK_BIN (widget));
  expander = GTK_EXPANDER (widget);
  priv = expander->priv;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_widget_style_get (GTK_WIDGET (widget),
                        "interior-focus", &interior_focus,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        "expander-size", &expander_size,
                        "expander-spacing", &expander_spacing,
                        NULL);

  *minimum_size = *natural_size =
    interior_focus ? (2 * focus_width + 2 * focus_pad) : 0;


  if (priv->label_widget && gtk_widget_get_visible (priv->label_widget))
    {
      gint label_min, label_nat;

      gtk_widget_get_preferred_height (priv->label_widget,
                                       &label_min, &label_nat);

      *minimum_size += label_min;
      *natural_size += label_nat;
    }

  *minimum_size = MAX (*minimum_size, expander_size + 2 * expander_spacing);
  *natural_size = MAX (*natural_size, *minimum_size);

  if (!interior_focus)
    {
      gint extra = 2 * focus_width + 2 * focus_pad;
      *minimum_size += extra;
      *natural_size += extra;
    }

  if (child && gtk_widget_get_child_visible (child))
    {
      gint child_min, child_nat;

      gtk_widget_get_preferred_height (child,
                                       &child_min, &child_nat);

      *minimum_size += child_min + priv->spacing;
      *natural_size += child_nat + priv->spacing;

    }

  *minimum_size += 2 * border_width;
  *natural_size += 2 * border_width;
}

static void
gtk_expander_get_preferred_height_for_width (GtkWidget *widget,
                                             gint       width,
                                             gint      *minimum_height,
                                             gint      *natural_height)
{
  GtkExpander *expander;
  GtkWidget *child;
  GtkExpanderPrivate *priv;
  gint border_width;
  gint expander_size;
  gint expander_spacing;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;
  gint label_xpad;

  child = gtk_bin_get_child (GTK_BIN (widget));
  expander = GTK_EXPANDER (widget);
  priv = expander->priv;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_widget_style_get (GTK_WIDGET (widget),
                        "interior-focus", &interior_focus,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        "expander-size", &expander_size,
                        "expander-spacing", &expander_spacing,
                        NULL);

  label_xpad = 2 * border_width + expander_size + 2 * expander_spacing - 2 * focus_width + 2 * focus_pad;

  *minimum_height = *natural_height =
    interior_focus ? (2 * focus_width + 2 * focus_pad) : 0;


  if (priv->label_widget && gtk_widget_get_visible (priv->label_widget))
    {
      gint label_min, label_nat;

      gtk_widget_get_preferred_height_for_width (priv->label_widget,
                                                 MAX (width - label_xpad, 1),
                                                 &label_min, &label_nat);

      *minimum_height += label_min;
      *natural_height += label_nat;
    }

  *minimum_height = MAX (*minimum_height, expander_size + 2 * expander_spacing);
  *natural_height = MAX (*natural_height, *minimum_height);

  if (!interior_focus)
    {
      gint extra = 2 * focus_width + 2 * focus_pad;
      *minimum_height += extra;
      *natural_height += extra;
    }

  if (child && gtk_widget_get_child_visible (child))
    {
      gint child_min, child_nat;

      gtk_widget_get_preferred_height_for_width (child,
                                                 MAX (width - 2 * border_width, 1),
                                                 &child_min, &child_nat);

      *minimum_height += child_min + priv->spacing;
      *natural_height += child_nat + priv->spacing;
    }

  *minimum_height += 2 * border_width;
  *natural_height += 2 * border_width;
}

static void
gtk_expander_get_preferred_width_for_height (GtkWidget *widget,
                                             gint       height,
                                             gint      *minimum_width,
                                             gint      *natural_width)
{
  GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, minimum_width, natural_width);
}



/**
 * gtk_expander_new:
 * @label: the text of the label
 *
 * Creates a new expander using @label as the text of the label.
 *
 * Return value: a new #GtkExpander widget.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_expander_new (const gchar *label)
{
  return g_object_new (GTK_TYPE_EXPANDER, "label", label, NULL);
}

/**
 * gtk_expander_new_with_mnemonic:
 * @label: (allow-none): the text of the label with an underscore
 *     in front of the mnemonic character
 *
 * Creates a new expander using @label as the text of the label.
 * If characters in @label are preceded by an underscore, they are underlined.
 * If you need a literal underscore character in a label, use '__' (two
 * underscores). The first underlined character represents a keyboard
 * accelerator called a mnemonic.
 * Pressing Alt and that key activates the button.
 *
 * Return value: a new #GtkExpander widget.
 *
 * Since: 2.4
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
 *
 * Since: 2.4
 */
void
gtk_expander_set_expanded (GtkExpander *expander,
                           gboolean     expanded)
{
  GtkExpanderPrivate *priv;
  GtkWidget *child;

  g_return_if_fail (GTK_IS_EXPANDER (expander));

  priv = expander->priv;

  expanded = expanded != FALSE;

  if (priv->expanded != expanded)
    {
      GtkWidget *widget = GTK_WIDGET (expander);

      priv->expanded = expanded;

      child = gtk_bin_get_child (GTK_BIN (expander));

      if (child)
        {
          gtk_widget_set_child_visible (child, priv->expanded);
          gtk_widget_queue_resize (widget);
          gtk_expander_resize_toplevel (expander);
        }

      g_object_notify (G_OBJECT (expander), "expanded");
    }
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
 * Return value: the current state of the expander
 *
 * Since: 2.4
 */
gboolean
gtk_expander_get_expanded (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return expander->priv->expanded;
}

/**
 * gtk_expander_set_spacing:
 * @expander: a #GtkExpander
 * @spacing: distance between the expander and child in pixels
 *
 * Sets the spacing field of @expander, which is the number of
 * pixels to place between expander and the child.
 *
 * Since: 2.4
 */
void
gtk_expander_set_spacing (GtkExpander *expander,
                          gint         spacing)
{
  g_return_if_fail (GTK_IS_EXPANDER (expander));
  g_return_if_fail (spacing >= 0);

  if (expander->priv->spacing != spacing)
    {
      expander->priv->spacing = spacing;

      gtk_widget_queue_resize (GTK_WIDGET (expander));

      g_object_notify (G_OBJECT (expander), "spacing");
    }
}

/**
 * gtk_expander_get_spacing:
 * @expander: a #GtkExpander
 *
 * Gets the value set by gtk_expander_set_spacing().
 *
 * Return value: spacing between the expander and child
 *
 * Since: 2.4
 */
gint
gtk_expander_get_spacing (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), 0);

  return expander->priv->spacing;
}

/**
 * gtk_expander_set_label:
 * @expander: a #GtkExpander
 * @label: (allow-none): a string
 *
 * Sets the text of the label of the expander to @label.
 *
 * This will also clear any previously set labels.
 *
 * Since: 2.4
 */
void
gtk_expander_set_label (GtkExpander *expander,
                        const gchar *label)
{
  g_return_if_fail (GTK_IS_EXPANDER (expander));

  if (!label)
    {
      gtk_expander_set_label_widget (expander, NULL);
    }
  else
    {
      GtkWidget *child;

      child = gtk_label_new (label);
      gtk_label_set_use_underline (GTK_LABEL (child), expander->priv->use_underline);
      gtk_label_set_use_markup (GTK_LABEL (child), expander->priv->use_markup);
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
 * Return value: The text of the label widget. This string is owned
 *     by the widget and must not be modified or freed.
 *
 * Since: 2.4
 */
const char *
gtk_expander_get_label (GtkExpander *expander)
{
  GtkExpanderPrivate *priv;

  g_return_val_if_fail (GTK_IS_EXPANDER (expander), NULL);

  priv = expander->priv;

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
 *
 * Since: 2.4
 */
void
gtk_expander_set_use_underline (GtkExpander *expander,
                                gboolean     use_underline)
{
  GtkExpanderPrivate *priv;

  g_return_if_fail (GTK_IS_EXPANDER (expander));

  priv = expander->priv;

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
 * Return value: %TRUE if an embedded underline in the expander
 *     label indicates the mnemonic accelerator keys
 *
 * Since: 2.4
 */
gboolean
gtk_expander_get_use_underline (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return expander->priv->use_underline;
}

/**
 * gtk_expander_set_use_markup:
 * @expander: a #GtkExpander
 * @use_markup: %TRUE if the label's text should be parsed for markup
 *
 * Sets whether the text of the label contains markup in <link
 * linkend="PangoMarkupFormat">Pango's text markup
 * language</link>. See gtk_label_set_markup().
 *
 * Since: 2.4
 */
void
gtk_expander_set_use_markup (GtkExpander *expander,
                             gboolean     use_markup)
{
  GtkExpanderPrivate *priv;

  g_return_if_fail (GTK_IS_EXPANDER (expander));

  priv = expander->priv;

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
 * Returns whether the label's text is interpreted as marked up with
 * the <link linkend="PangoMarkupFormat">Pango text markup
 * language</link>. See gtk_expander_set_use_markup().
 *
 * Return value: %TRUE if the label's text will be parsed for markup
 *
 * Since: 2.4
 */
gboolean
gtk_expander_get_use_markup (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return expander->priv->use_markup;
}

/**
 * gtk_expander_set_label_widget:
 * @expander: a #GtkExpander
 * @label_widget: (allow-none): the new label widget
 *
 * Set the label widget for the expander. This is the widget
 * that will appear embedded alongside the expander arrow.
 *
 * Since: 2.4
 */
void
gtk_expander_set_label_widget (GtkExpander *expander,
                               GtkWidget   *label_widget)
{
  GtkExpanderPrivate *priv;
  GtkWidget          *widget;

  g_return_if_fail (GTK_IS_EXPANDER (expander));
  g_return_if_fail (label_widget == NULL || GTK_IS_WIDGET (label_widget));
  g_return_if_fail (label_widget == NULL || gtk_widget_get_parent (label_widget) == NULL);

  priv = expander->priv;

  if (priv->label_widget == label_widget)
    return;

  if (priv->label_widget)
    {
      gtk_widget_set_state_flags (priv->label_widget, 0, TRUE);
      gtk_widget_unparent (priv->label_widget);
    }

  priv->label_widget = label_widget;
  widget = GTK_WIDGET (expander);

  if (label_widget)
    {
      priv->label_widget = label_widget;

      gtk_widget_set_parent (label_widget, widget);

      if (priv->prelight)
        gtk_widget_set_state_flags (label_widget,
                                    GTK_STATE_FLAG_PRELIGHT,
                                    FALSE);
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
 * Return value: (transfer none): the label widget,
 *     or %NULL if there is none
 *
 * Since: 2.4
 */
GtkWidget *
gtk_expander_get_label_widget (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), NULL);

  return expander->priv->label_widget;
}

/**
 * gtk_expander_set_label_fill:
 * @expander: a #GtkExpander
 * @label_fill: %TRUE if the label should should fill
 *     all available horizontal space
 *
 * Sets whether the label widget should fill all available
 * horizontal space allocated to @expander.
 *
 * Since: 2.22
 */
void
gtk_expander_set_label_fill (GtkExpander *expander,
                             gboolean     label_fill)
{
  GtkExpanderPrivate *priv;

  g_return_if_fail (GTK_IS_EXPANDER (expander));

  priv = expander->priv;

  label_fill = label_fill != FALSE;

  if (priv->label_fill != label_fill)
    {
      priv->label_fill = label_fill;

      if (priv->label_widget != NULL)
        gtk_widget_queue_resize (GTK_WIDGET (expander));

      g_object_notify (G_OBJECT (expander), "label-fill");
    }
}

/**
 * gtk_expander_get_label_fill:
 * @expander: a #GtkExpander
 *
 * Returns whether the label widget will fill all available
 * horizontal space allocated to @expander.
 *
 * Return value: %TRUE if the label widget will fill all
 *     available horizontal space
 *
 * Since: 2.22
 */
gboolean
gtk_expander_get_label_fill (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return expander->priv->label_fill;
}

/**
 * gtk_expander_set_resize_toplevel:
 * @expander: a #GtkExpander
 * @resize_toplevel: whether to resize the toplevel
 *
 * Sets whether the expander will resize the toplevel widget
 * containing the expander upon resizing and collpasing.
 *
 * Since: 3.2
 */
void
gtk_expander_set_resize_toplevel (GtkExpander *expander,
                                  gboolean     resize_toplevel)
{
  g_return_if_fail (GTK_IS_EXPANDER (expander));

  if (expander->priv->resize_toplevel != resize_toplevel)
    {
      expander->priv->resize_toplevel = resize_toplevel ? TRUE : FALSE;
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
 * Return value: the "resize toplevel" setting.
 *
 * Since: 3.2
 */
gboolean
gtk_expander_get_resize_toplevel (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return expander->priv->resize_toplevel;
}
