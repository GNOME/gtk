/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkToolbar copyright (C) Federico Mena
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkbutton.h"
#include "gtktogglebutton.h"
#include "gtkradiobutton.h"
#include "gtklabel.h"
#include "gtkvbox.h"
#include "gtkhbox.h"
#include "gtktoolbar.h"
#include "gtkstock.h"
#include "gtkiconfactory.h"
#include "gtkimage.h"
#include "gtksettings.h"
#include "gtkintl.h"


#define DEFAULT_IPADDING 0
#define DEFAULT_SPACE_SIZE  5
#define DEFAULT_SPACE_STYLE GTK_TOOLBAR_SPACE_LINE

#define DEFAULT_ICON_SIZE GTK_ICON_SIZE_LARGE_TOOLBAR

#define SPACE_LINE_DIVISION 10
#define SPACE_LINE_START    3
#define SPACE_LINE_END      7

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_TOOLBAR_STYLE,
};

enum {
  ORIENTATION_CHANGED,
  STYLE_CHANGED,
  LAST_SIGNAL
};

typedef struct _GtkToolbarChildSpace GtkToolbarChildSpace;
struct _GtkToolbarChildSpace
{
  GtkToolbarChild child;

  gint alloc_x, alloc_y;
};

static void gtk_toolbar_class_init               (GtkToolbarClass *class);
static void gtk_toolbar_init                     (GtkToolbar      *toolbar);
static void gtk_toolbar_finalize                 (GObject         *object);
static void gtk_toolbar_set_property             (GObject         *object,
						  guint            prop_id,
						  const GValue    *value,
						  GParamSpec      *pspec);
static void gtk_toolbar_get_property             (GObject         *object,
						  guint            prop_id,
						  GValue          *value,
						  GParamSpec      *pspec);
static void gtk_toolbar_destroy                  (GtkObject       *object);
static gint gtk_toolbar_expose                   (GtkWidget       *widget,
						  GdkEventExpose  *event);
static void gtk_toolbar_size_request             (GtkWidget       *widget,
				                  GtkRequisition  *requisition);
static void gtk_toolbar_size_allocate            (GtkWidget       *widget,
				                  GtkAllocation   *allocation);
static void gtk_toolbar_style_set                (GtkWidget       *widget,
                                                  GtkStyle        *prev_style);
static gboolean gtk_toolbar_focus                (GtkWidget       *widget,
                                                  GtkDirectionType dir);
static void gtk_toolbar_show_all                 (GtkWidget       *widget);
static void gtk_toolbar_add                      (GtkContainer    *container,
				                  GtkWidget       *widget);
static void gtk_toolbar_remove                   (GtkContainer    *container,
						  GtkWidget       *widget);
static void gtk_toolbar_forall                   (GtkContainer    *container,
						  gboolean	   include_internals,
				                  GtkCallback      callback,
				                  gpointer         callback_data);

static void gtk_real_toolbar_orientation_changed (GtkToolbar      *toolbar,
						  GtkOrientation   orientation);
static void gtk_real_toolbar_style_changed       (GtkToolbar      *toolbar,
						  GtkToolbarStyle  style);

static GtkWidget * gtk_toolbar_internal_insert_element (GtkToolbar          *toolbar,
                                                        GtkToolbarChildType  type,
                                                        GtkWidget           *widget,
                                                        const char          *text,
                                                        const char          *tooltip_text,
                                                        const char          *tooltip_private_text,
                                                        GtkWidget           *icon,
                                                        GtkSignalFunc        callback,
                                                        gpointer             user_data,
                                                        gint                 position,
                                                        gboolean             has_mnemonic);

static GtkWidget * gtk_toolbar_internal_insert_item (GtkToolbar    *toolbar,
                                                     const char    *text,
                                                     const char    *tooltip_text,
                                                     const char    *tooltip_private_text,
                                                     GtkWidget     *icon,
                                                     GtkSignalFunc  callback,
                                                     gpointer       user_data,
                                                     gint           position,
                                                     gboolean       has_mnemonic);

static void        gtk_toolbar_update_button_relief (GtkToolbar *toolbar);

static GtkReliefStyle       get_button_relief (GtkToolbar *toolbar);
static gint                 get_space_size    (GtkToolbar *toolbar);
static GtkToolbarSpaceStyle get_space_style   (GtkToolbar *toolbar);


static GtkContainerClass *parent_class;

static guint toolbar_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_toolbar_get_type (void)
{
  static GtkType toolbar_type = 0;

  if (!toolbar_type)
    {
      static const GtkTypeInfo toolbar_info =
      {
	"GtkToolbar",
	sizeof (GtkToolbar),
	sizeof (GtkToolbarClass),
	(GtkClassInitFunc) gtk_toolbar_class_init,
	(GtkObjectInitFunc) gtk_toolbar_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      toolbar_type = gtk_type_unique (gtk_container_get_type (), &toolbar_info);
    }

  return toolbar_type;
}

static void
gtk_toolbar_class_init (GtkToolbarClass *class)
{
  GObjectClass   *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = G_OBJECT_CLASS (class);
  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  parent_class = gtk_type_class (gtk_container_get_type ());

  gobject_class->finalize = gtk_toolbar_finalize;
  
  object_class->destroy = gtk_toolbar_destroy;
  gobject_class->set_property = gtk_toolbar_set_property;
  gobject_class->get_property = gtk_toolbar_get_property;

  widget_class->expose_event = gtk_toolbar_expose;
  widget_class->size_request = gtk_toolbar_size_request;
  widget_class->size_allocate = gtk_toolbar_size_allocate;
  widget_class->style_set = gtk_toolbar_style_set;
  widget_class->show_all = gtk_toolbar_show_all;
  widget_class->focus = gtk_toolbar_focus;
  
  container_class->add = gtk_toolbar_add;
  container_class->remove = gtk_toolbar_remove;
  container_class->forall = gtk_toolbar_forall;
  
  class->orientation_changed = gtk_real_toolbar_orientation_changed;
  class->style_changed = gtk_real_toolbar_style_changed;

  toolbar_signals[ORIENTATION_CHANGED] =
    gtk_signal_new ("orientation_changed",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkToolbarClass, orientation_changed),
		    gtk_marshal_VOID__ENUM,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_ORIENTATION);
  toolbar_signals[STYLE_CHANGED] =
    gtk_signal_new ("style_changed",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkToolbarClass, style_changed),
		    gtk_marshal_VOID__ENUM,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_TOOLBAR_STYLE);
  
  g_object_class_install_property (gobject_class,
				   PROP_ORIENTATION,
				   g_param_spec_enum ("orientation",
 						      _("Orientation"),
 						      _("The orientation of the toolbar"),
 						      GTK_TYPE_ORIENTATION,
 						      GTK_ORIENTATION_HORIZONTAL,
 						      G_PARAM_READWRITE));
 
   g_object_class_install_property (gobject_class,
                                    PROP_TOOLBAR_STYLE,
                                    g_param_spec_enum ("toolbar_style",
 						      _("Toolbar Style"),
 						      _("How to draw the toolbar"),
 						      GTK_TYPE_TOOLBAR_STYLE,
 						      GTK_TOOLBAR_ICONS,
 						      G_PARAM_READWRITE));


  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("space_size",
							     _("Spacer size"),
							     _("Size of spacers"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_SPACE_SIZE,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("internal_padding",
							     _("Internal padding"),
							     _("Amount of border space between the toolbar shadow and the buttons"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_IPADDING,
                                                             G_PARAM_READABLE));
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_enum ("space_style",
							     _("Space style"),
							     _("Whether spacers are vertical lines or just blank"),
                                                              GTK_TYPE_TOOLBAR_SPACE_STYLE,
                                                              DEFAULT_SPACE_STYLE,
                                                              
                                                              G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_enum ("button_relief",
							     _("Button relief"),
							     _("Type of bevel around toolbar buttons"),
                                                              GTK_TYPE_RELIEF_STYLE,
                                                              GTK_RELIEF_NONE,
                                                              G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("shadow_type",
                                                              _("Shadow type"),
                                                              _("Style of bevel around the toolbar"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_OUT,
                                                              G_PARAM_READABLE));

  gtk_settings_install_property (g_param_spec_enum ("gtk-toolbar-style",
                                                    _("Toolbar style"),
                                                    _("Whether default toolbars have text only, text and icons, icons only, etc."),
                                                    GTK_TYPE_TOOLBAR_STYLE,
                                                    GTK_TOOLBAR_BOTH,
                                                    G_PARAM_READWRITE));

  gtk_settings_install_property (g_param_spec_enum ("gtk-toolbar-icon-size",
                                                    _("Toolbar icon size"),
                                                    _("Size of icons in default toolbars"),
                                                    GTK_TYPE_ICON_SIZE,
                                                    GTK_ICON_SIZE_LARGE_TOOLBAR,
                                                    G_PARAM_READWRITE));  
}

static void
style_change_notify (GObject    *object,
                     GParamSpec *pspec,
                     gpointer    data)
{
  GtkToolbar *toolbar;

  toolbar = GTK_TOOLBAR (data);

  if (!toolbar->style_set)
    {
      /* pretend it was set, then unset, thus reverting to new default */
      toolbar->style_set = TRUE; 
      gtk_toolbar_unset_style (toolbar);
    }
}

static void
icon_size_change_notify (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    data)
{
  GtkToolbar *toolbar;

  toolbar = GTK_TOOLBAR (data);

  if (!toolbar->icon_size_set)
    {
      /* pretend it was set, then unset, thus reverting to new default */
      toolbar->icon_size_set = TRUE; 
      gtk_toolbar_unset_icon_size (toolbar);
    }
}

static void
gtk_toolbar_init (GtkToolbar *toolbar)
{
  GTK_WIDGET_SET_FLAGS (toolbar, GTK_NO_WINDOW);
  GTK_WIDGET_UNSET_FLAGS (toolbar, GTK_CAN_FOCUS);

  toolbar->num_children = 0;
  toolbar->children     = NULL;
  toolbar->orientation  = GTK_ORIENTATION_HORIZONTAL;
  toolbar->icon_size    = DEFAULT_ICON_SIZE;
  toolbar->tooltips     = gtk_tooltips_new ();
  toolbar->button_maxw  = 0;
  toolbar->button_maxh  = 0;

  toolbar->style_set = FALSE;
  toolbar->icon_size_set = FALSE;
  g_object_get (gtk_settings_get_default (),
                "gtk-toolbar-icon-size",
                &toolbar->icon_size,
                "gtk-toolbar-style",
                &toolbar->style,
                NULL);
  
  toolbar->style_set_connection =
    g_signal_connect (G_OBJECT (gtk_settings_get_default ()),
		      "notify::gtk-toolbar-style",
		      G_CALLBACK (style_change_notify),
		      toolbar);

  toolbar->icon_size_connection =
    g_signal_connect (G_OBJECT (gtk_settings_get_default ()),
		      "notify::gtk-toolbar-icon-size",
		      G_CALLBACK (icon_size_change_notify),
		      toolbar);
}

static void
gtk_toolbar_finalize (GObject *object)
{
  GtkToolbar *toolbar;

  toolbar = GTK_TOOLBAR (object);

  g_signal_handler_disconnect (G_OBJECT (gtk_settings_get_default ()),
                               toolbar->style_set_connection);
  g_signal_handler_disconnect (G_OBJECT (gtk_settings_get_default ()),
                               toolbar->icon_size_connection);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_toolbar_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (object);
  
  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_toolbar_set_orientation (toolbar, g_value_get_enum (value));
      break;
    case PROP_TOOLBAR_STYLE:
      gtk_toolbar_set_style (toolbar, g_value_get_enum (value));
      break;
    }
}

static void
gtk_toolbar_get_property (GObject      *object,
			  guint         prop_id,
			  GValue       *value,
			  GParamSpec   *pspec)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, toolbar->orientation);
      break;
    case PROP_TOOLBAR_STYLE:
      g_value_set_enum (value, toolbar->style);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

GtkWidget*
gtk_toolbar_new (void)
{
  GtkToolbar *toolbar;

  toolbar = gtk_type_new (gtk_toolbar_get_type ());

  return GTK_WIDGET (toolbar);
}

static void
gtk_toolbar_destroy (GtkObject *object)
{
  GtkToolbar *toolbar;
  GList *children;

  g_return_if_fail (GTK_IS_TOOLBAR (object));

  toolbar = GTK_TOOLBAR (object);

  if (toolbar->tooltips)
    {
      gtk_object_unref (GTK_OBJECT (toolbar->tooltips));
      toolbar->tooltips = NULL;
    }

  for (children = toolbar->children; children; children = children->next)
    {
      GtkToolbarChild *child;

      child = children->data;

      if (child->type != GTK_TOOLBAR_CHILD_SPACE)
	{
	  gtk_widget_ref (child->widget);
	  gtk_widget_unparent (child->widget);
	  gtk_widget_destroy (child->widget);
	  gtk_widget_unref (child->widget);
	}

      g_free (child);
    }
  g_list_free (toolbar->children);
  toolbar->children = NULL;
  
  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_toolbar_paint_space_line (GtkWidget       *widget,
			      GdkRectangle    *area,
			      GtkToolbarChild *child)
{
  GtkToolbar *toolbar;
  GtkToolbarChildSpace *child_space;
  gint space_size;
  
  g_return_if_fail (GTK_IS_TOOLBAR (widget));
  g_return_if_fail (child != NULL);
  g_return_if_fail (child->type == GTK_TOOLBAR_CHILD_SPACE);

  toolbar = GTK_TOOLBAR (widget);

  child_space = (GtkToolbarChildSpace *) child;
  space_size = get_space_size (toolbar);
  
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_paint_vline (widget->style, widget->window,
		     GTK_WIDGET_STATE (widget), area, widget,
		     "toolbar",
		     child_space->alloc_y + toolbar->button_maxh *
		     SPACE_LINE_START / SPACE_LINE_DIVISION,
		     child_space->alloc_y + toolbar->button_maxh *
		     SPACE_LINE_END / SPACE_LINE_DIVISION,
		     child_space->alloc_x +
		     (space_size -
		      widget->style->xthickness) / 2);
  else
    gtk_paint_hline (widget->style, widget->window,
		     GTK_WIDGET_STATE (widget), area, widget,
		     "toolbar",
		     child_space->alloc_x + toolbar->button_maxw *
		     SPACE_LINE_START / SPACE_LINE_DIVISION,
		     child_space->alloc_x + toolbar->button_maxw *
		     SPACE_LINE_END / SPACE_LINE_DIVISION,
		     child_space->alloc_y +
		     (space_size -
		      widget->style->ythickness) / 2);
}

static gint
gtk_toolbar_expose (GtkWidget      *widget,
		    GdkEventExpose *event)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;
  gint border_width;
  
  g_return_val_if_fail (GTK_IS_TOOLBAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  border_width = GTK_CONTAINER (widget)->border_width;
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      GtkShadowType shadow_type;

      toolbar = GTK_TOOLBAR (widget);

      gtk_widget_style_get (widget, "shadow_type", &shadow_type, NULL);
      
      gtk_paint_box (widget->style,
		     widget->window,
                     GTK_WIDGET_STATE (widget),
                     shadow_type,
		     &event->area, widget, "toolbar",
		     widget->allocation.x + border_width,
                     widget->allocation.y + border_width,
		     widget->allocation.width - border_width,
                     widget->allocation.height - border_width);
      
      for (children = toolbar->children; children; children = children->next)
	{
	  child = children->data;

	  if (child->type == GTK_TOOLBAR_CHILD_SPACE)
	    {
	      if (get_space_style (toolbar) == GTK_TOOLBAR_SPACE_LINE)
		gtk_toolbar_paint_space_line (widget, &event->area, child);
	    }
	  else
	    gtk_container_propagate_expose (GTK_CONTAINER (widget),
					    child->widget,
					    event);
	}
    }

  return FALSE;
}

static void
gtk_toolbar_size_request (GtkWidget      *widget,
			  GtkRequisition *requisition)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;
  gint nbuttons;
  gint button_maxw, button_maxh;
  gint widget_maxw, widget_maxh;
  GtkRequisition child_requisition;
  gint space_size;
  gint ipadding;
  
  g_return_if_fail (GTK_IS_TOOLBAR (widget));
  g_return_if_fail (requisition != NULL);

  toolbar = GTK_TOOLBAR (widget);

  requisition->width = GTK_CONTAINER (toolbar)->border_width * 2;
  requisition->height = GTK_CONTAINER (toolbar)->border_width * 2;
  nbuttons = 0;
  button_maxw = 0;
  button_maxh = 0;
  widget_maxw = 0;
  widget_maxh = 0;

  space_size = get_space_size (toolbar);
  
  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      switch (child->type)
	{
	case GTK_TOOLBAR_CHILD_SPACE:
	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    requisition->width += space_size;
	  else
	    requisition->height += space_size;

	  break;

	case GTK_TOOLBAR_CHILD_BUTTON:
	case GTK_TOOLBAR_CHILD_RADIOBUTTON:
	case GTK_TOOLBAR_CHILD_TOGGLEBUTTON:
	  if (GTK_WIDGET_VISIBLE (child->widget))
	    {              
	      gtk_widget_size_request (child->widget, &child_requisition);

	      nbuttons++;
	      button_maxw = MAX (button_maxw, child_requisition.width);
	      button_maxh = MAX (button_maxh, child_requisition.height);
	    }

	  break;

	case GTK_TOOLBAR_CHILD_WIDGET:
	  if (GTK_WIDGET_VISIBLE (child->widget))
	    {
	      gtk_widget_size_request (child->widget, &child_requisition);

	      widget_maxw = MAX (widget_maxw, child_requisition.width);
	      widget_maxh = MAX (widget_maxh, child_requisition.height);

	      if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
		requisition->width += child_requisition.width;
	      else
		requisition->height += child_requisition.height;
	    }

	  break;

	default:
	  g_assert_not_reached ();
	}
    }

  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      requisition->width += nbuttons * button_maxw;
      requisition->height += MAX (button_maxh, widget_maxh);
    }
  else
    {
      requisition->width += MAX (button_maxw, widget_maxw);
      requisition->height += nbuttons * button_maxh;
    }

  /* Extra spacing */
  gtk_widget_style_get (widget, "internal_padding", &ipadding, NULL);
  
  requisition->width += 2 * (widget->style->xthickness + ipadding);
  requisition->height += 2 * (widget->style->ythickness + ipadding);
  
  toolbar->button_maxw = button_maxw;
  toolbar->button_maxh = button_maxh;
}

static void
gtk_toolbar_size_allocate (GtkWidget     *widget,
			   GtkAllocation *allocation)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;
  GtkToolbarChildSpace *child_space;
  GtkAllocation alloc;
  GtkRequisition child_requisition;
  gint x_border_width, y_border_width;
  gint space_size;
  gint ipadding;
  
  g_return_if_fail (GTK_IS_TOOLBAR (widget));
  g_return_if_fail (allocation != NULL);

  toolbar = GTK_TOOLBAR (widget);
  widget->allocation = *allocation;
  
  x_border_width = GTK_CONTAINER (toolbar)->border_width;
  y_border_width = GTK_CONTAINER (toolbar)->border_width;

  gtk_widget_style_get (widget, "internal_padding", &ipadding, NULL);
  
  x_border_width += 2 * (widget->style->xthickness + ipadding);
  y_border_width += 2 * (widget->style->ythickness + ipadding);
  
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    alloc.x = allocation->x + x_border_width;
  else
    alloc.y = allocation->y + y_border_width;

  space_size = get_space_size (toolbar);
  
  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      switch (child->type)
	{
	case GTK_TOOLBAR_CHILD_SPACE:

	  child_space = (GtkToolbarChildSpace *) child;

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      child_space->alloc_x = alloc.x;
	      child_space->alloc_y = allocation->y + (allocation->height - toolbar->button_maxh) / 2;
	      alloc.x += space_size;
	    }
	  else
	    {
	      child_space->alloc_x = allocation->x + (allocation->width - toolbar->button_maxw) / 2;
	      child_space->alloc_y = alloc.y;
	      alloc.y += space_size;
	    }

	  break;

	case GTK_TOOLBAR_CHILD_BUTTON:
	case GTK_TOOLBAR_CHILD_RADIOBUTTON:
	case GTK_TOOLBAR_CHILD_TOGGLEBUTTON:
	  if (!GTK_WIDGET_VISIBLE (child->widget))
	    break;

	  alloc.width = toolbar->button_maxw;
	  alloc.height = toolbar->button_maxh;

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.y = allocation->y + (allocation->height - toolbar->button_maxh) / 2;
	  else
	    alloc.x = allocation->x + (allocation->width - toolbar->button_maxw) / 2;

	  gtk_widget_size_allocate (child->widget, &alloc);

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.x += toolbar->button_maxw;
	  else
	    alloc.y += toolbar->button_maxh;

	  break;

	case GTK_TOOLBAR_CHILD_WIDGET:
	  if (!GTK_WIDGET_VISIBLE (child->widget))
	    break;

	  gtk_widget_get_child_requisition (child->widget, &child_requisition);
	  
	  alloc.width = child_requisition.width;
	  alloc.height = child_requisition.height;

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.y = allocation->y + (allocation->height - child_requisition.height) / 2;
	  else
	    alloc.x = allocation->x + (allocation->width - child_requisition.width) / 2;

	  gtk_widget_size_allocate (child->widget, &alloc);

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.x += child_requisition.width;
	  else
	    alloc.y += child_requisition.height;

	  break;

	default:
	  g_assert_not_reached ();
	}
    }
}

static void
gtk_toolbar_style_set (GtkWidget  *widget,
                       GtkStyle   *prev_style)
{
  if (prev_style)
    gtk_toolbar_update_button_relief (GTK_TOOLBAR (widget));
}

static gboolean
gtk_toolbar_focus (GtkWidget       *widget,
                   GtkDirectionType dir)
{
  /* Focus can't go in toolbars */
  
  return FALSE;
}

static void
child_show_all (GtkWidget *widget)
{
  /* Don't show our own children, since that would
   * show labels we may intend to hide in icons-only mode
   */
  if (!g_object_get_data (G_OBJECT (widget),
                          "gtk-toolbar-is-child"))
    gtk_widget_show_all (widget);
}

static void
gtk_toolbar_show_all (GtkWidget *widget)
{
  gtk_container_foreach (GTK_CONTAINER (widget),
			 (GtkCallback) child_show_all,
			 NULL);
  gtk_widget_show (widget);
}

static void
gtk_toolbar_add (GtkContainer *container,
		 GtkWidget    *widget)
{
  g_return_if_fail (GTK_IS_TOOLBAR (container));
  g_return_if_fail (widget != NULL);

  gtk_toolbar_append_widget (GTK_TOOLBAR (container), widget, NULL, NULL);
}

static void
gtk_toolbar_remove (GtkContainer *container,
		    GtkWidget    *widget)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;

  g_return_if_fail (GTK_IS_TOOLBAR (container));
  g_return_if_fail (widget != NULL);

  toolbar = GTK_TOOLBAR (container);

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      if ((child->type != GTK_TOOLBAR_CHILD_SPACE) && (child->widget == widget))
	{
	  gboolean was_visible;

	  was_visible = GTK_WIDGET_VISIBLE (widget);
	  gtk_widget_unparent (widget);

	  toolbar->children = g_list_remove_link (toolbar->children, children);
	  g_free (child);
	  g_list_free (children);
	  toolbar->num_children--;

	  if (was_visible && GTK_WIDGET_VISIBLE (container))
	    gtk_widget_queue_resize (GTK_WIDGET (container));

	  break;
	}
    }
}

static void
gtk_toolbar_forall (GtkContainer *container,
		    gboolean	  include_internals,
		    GtkCallback   callback,
		    gpointer      callback_data)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;

  g_return_if_fail (GTK_IS_TOOLBAR (container));
  g_return_if_fail (callback != NULL);

  toolbar = GTK_TOOLBAR (container);

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      if (child->type != GTK_TOOLBAR_CHILD_SPACE)
	(*callback) (child->widget, callback_data);
    }
}

GtkWidget *
gtk_toolbar_append_item (GtkToolbar    *toolbar,
			 const char    *text,
			 const char    *tooltip_text,
			 const char    *tooltip_private_text,
			 GtkWidget     *icon,
			 GtkSignalFunc  callback,
			 gpointer       user_data)
{
  return gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     toolbar->num_children);
}

GtkWidget *
gtk_toolbar_prepend_item (GtkToolbar    *toolbar,
			  const char    *text,
			  const char    *tooltip_text,
			  const char    *tooltip_private_text,
			  GtkWidget     *icon,
			  GtkSignalFunc  callback,
			  gpointer       user_data)
{
  return gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     0);
}

static GtkWidget *
gtk_toolbar_internal_insert_item (GtkToolbar    *toolbar,
                                  const char    *text,
                                  const char    *tooltip_text,
                                  const char    *tooltip_private_text,
                                  GtkWidget     *icon,
                                  GtkSignalFunc  callback,
                                  gpointer       user_data,
                                  gint           position,
                                  gboolean       has_mnemonic)
{
  return gtk_toolbar_internal_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
                                              NULL, text,
                                              tooltip_text, tooltip_private_text,
                                              icon, callback, user_data,
                                              position, has_mnemonic);
}
     
GtkWidget *
gtk_toolbar_insert_item (GtkToolbar    *toolbar,
			 const char    *text,
			 const char    *tooltip_text,
			 const char    *tooltip_private_text,
			 GtkWidget     *icon,
			 GtkSignalFunc  callback,
			 gpointer       user_data,
			 gint           position)
{
  return gtk_toolbar_internal_insert_item (toolbar, 
                                           text, tooltip_text, tooltip_private_text,
                                           icon, callback, user_data,
                                           position, FALSE);
}

/**
 * gtk_toolbar_set_icon_size:
 * @toolbar: A #GtkToolbar
 * @icon_size: The #GtkIconSize that stock icons in the toolbar shall have.
 *
 * This function sets the size of stock icons in the toolbar. You
 * can call it both before you add the icons and after they've been
 * added. The size you set will override user preferences for the default
 * icon size.
 **/
void
gtk_toolbar_set_icon_size (GtkToolbar  *toolbar,
			   GtkIconSize  icon_size)
{
  GList *children;
  GtkToolbarChild *child;
  GtkImage *image;
  gchar *stock_id;

  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  toolbar->icon_size_set = TRUE;
  
  if (toolbar->icon_size == icon_size)
    return;
  
  toolbar->icon_size = icon_size;

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;
      if (child->type == GTK_TOOLBAR_CHILD_BUTTON &&
	  GTK_IS_IMAGE (child->icon))
	{
	  image = GTK_IMAGE (child->icon);
	  if (gtk_image_get_storage_type (image) == GTK_IMAGE_STOCK)
	    {
	      gtk_image_get_stock (image, &stock_id, NULL);
	      stock_id = g_strdup (stock_id);
	      gtk_image_set_from_stock (image,
					stock_id,
					icon_size);
	      g_free (stock_id);
	    }
	}
    }
  
  gtk_widget_queue_resize (GTK_WIDGET (toolbar));
}

/**
 * gtk_toolbar_get_icon_size:
 * @toolbar: a #GtkToolbar
 *
 * Retrieves the icon size fo the toolbar. See gtk_toolbar_set_icon_size().
 *
 * Return value: the current icon size for the icons on the toolbar.
 **/
GtkIconSize
gtk_toolbar_get_icon_size (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), GTK_ICON_SIZE_LARGE_TOOLBAR);

  return toolbar->icon_size;
}

/**
 * gtk_toolbar_unset_icon_size:
 * @toolbar: a #GtkToolbar
 * 
 * Unsets toolbar icon size set with gtk_toolbar_set_icon_size(), so that
 * user preferences will be used to determine the icon size.
 **/
void
gtk_toolbar_unset_icon_size (GtkToolbar  *toolbar)
{
  GtkIconSize size;

  if (toolbar->icon_size_set)
    {
      g_object_get (gtk_settings_get_default (),
                    "gtk-toolbar-icon-size",
                    &size, NULL);

      if (size != toolbar->icon_size)
        gtk_toolbar_set_icon_size (toolbar, size);

      toolbar->icon_size_set = FALSE;
    }
}

/**
 * gtk_toolbar_insert_stock:
 * @toolbar: A #GtkToolbar
 * @stock_id: The id of the stock item you want to insert
 * @tooltip_text: The text in the tooltip of the toolbar button
 * @tooltip_private_text: The private text of the tooltip
 * @callback: The callback called when the toolbar button is clicked.
 * @user_data: user data passed to callback
 * @position: The position the button shall be inserted at.
 *            -1 means at the end.
 *
 * Inserts a stock item at the specified position of the toolbar.  If
 * @stock_id is not a known stock item ID, it's inserted verbatim,
 * except that underscores are used to mark mnemonics (see
 * gtk_label_new_with_mnemonic()).
 *
 * Returns: the inserted widget
 */
GtkWidget*
gtk_toolbar_insert_stock (GtkToolbar      *toolbar,
			  const gchar     *stock_id,
			  const char      *tooltip_text,
			  const char      *tooltip_private_text,
			  GtkSignalFunc    callback,
			  gpointer         user_data,
			  gint             position)
{
  GtkStockItem item;
  GtkWidget *image;

  if (gtk_stock_lookup (stock_id, &item))
    {
      image = gtk_image_new_from_stock (stock_id, toolbar->icon_size);

      return gtk_toolbar_internal_insert_item (toolbar,
                                               item.label,
                                               tooltip_text,
                                               tooltip_private_text,
                                               image,
                                               callback,
                                               user_data,
                                               position,
                                               TRUE);
    }
  else
    return gtk_toolbar_internal_insert_item (toolbar,
                                             stock_id,
                                             tooltip_text,
                                             tooltip_private_text,
                                             NULL,
                                             callback,
                                             user_data,
                                             position,
                                             TRUE);
}



void
gtk_toolbar_append_space (GtkToolbar *toolbar)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      toolbar->num_children);
}

void
gtk_toolbar_prepend_space (GtkToolbar *toolbar)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      0);
}

void
gtk_toolbar_insert_space (GtkToolbar *toolbar,
			  gint        position)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      position);
}

void
gtk_toolbar_remove_space (GtkToolbar *toolbar,
                          gint        position)
{
  GList *children;
  GtkToolbarChild *child;
  gint i;
  
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));
  
  i = 0;
  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      if (i == position)
        {
          if (child->type == GTK_TOOLBAR_CHILD_SPACE)
            {
              toolbar->children = g_list_remove_link (toolbar->children, children);
              g_free (child);
              g_list_free (children);
              toolbar->num_children--;
              
              gtk_widget_queue_resize (GTK_WIDGET (toolbar));
            }
          else
            {
              g_warning ("Toolbar position %d is not a space", position);
            }

          return;
        }

      ++i;
    }

  g_warning ("Toolbar position %d doesn't exist", position);
}

void
gtk_toolbar_append_widget (GtkToolbar  *toolbar,
			   GtkWidget   *widget,
			   const gchar *tooltip_text,
			   const gchar *tooltip_private_text)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      toolbar->num_children);
}

void
gtk_toolbar_prepend_widget (GtkToolbar  *toolbar,
			    GtkWidget   *widget,
			    const gchar *tooltip_text,
			    const gchar *tooltip_private_text)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      0);
}

void
gtk_toolbar_insert_widget (GtkToolbar *toolbar,
			   GtkWidget  *widget,
			   const char *tooltip_text,
			   const char *tooltip_private_text,
			   gint        position)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      position);
}

GtkWidget*
gtk_toolbar_append_element (GtkToolbar          *toolbar,
			    GtkToolbarChildType  type,
			    GtkWidget           *widget,
			    const char          *text,
			    const char          *tooltip_text,
			    const char          *tooltip_private_text,
			    GtkWidget           *icon,
			    GtkSignalFunc        callback,
			    gpointer             user_data)
{
  return gtk_toolbar_insert_element (toolbar, type, widget, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     toolbar->num_children);
}

GtkWidget *
gtk_toolbar_prepend_element (GtkToolbar          *toolbar,
			     GtkToolbarChildType  type,
			     GtkWidget           *widget,
			     const char          *text,
			     const char          *tooltip_text,
			     const char          *tooltip_private_text,
			     GtkWidget           *icon,
			     GtkSignalFunc        callback,
			     gpointer             user_data)
{
  return gtk_toolbar_insert_element (toolbar, type, widget, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data, 0);
}

GtkWidget *
gtk_toolbar_insert_element (GtkToolbar          *toolbar,
			    GtkToolbarChildType  type,
			    GtkWidget           *widget,
			    const char          *text,
			    const char          *tooltip_text,
			    const char          *tooltip_private_text,
			    GtkWidget           *icon,
			    GtkSignalFunc        callback,
			    gpointer             user_data,
			    gint                 position)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), NULL);
  if (type == GTK_TOOLBAR_CHILD_WIDGET)
    {
      g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
    }
  else if (type != GTK_TOOLBAR_CHILD_RADIOBUTTON)
    g_return_val_if_fail (widget == NULL, NULL);
  
  return gtk_toolbar_internal_insert_element (toolbar, type, widget, text,
                                              tooltip_text, tooltip_private_text,
                                              icon, callback, user_data,
                                              position, FALSE);
}

static GtkWidget *
gtk_toolbar_internal_insert_element (GtkToolbar          *toolbar,
                                     GtkToolbarChildType  type,
                                     GtkWidget           *widget,
                                     const char          *text,
                                     const char          *tooltip_text,
                                     const char          *tooltip_private_text,
                                     GtkWidget           *icon,
                                     GtkSignalFunc        callback,
                                     gpointer             user_data,
                                     gint                 position,
                                     gboolean             has_mnemonic)
{
  GtkToolbarChild *child;
  GtkWidget *box;

  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), NULL);
  if (type == GTK_TOOLBAR_CHILD_WIDGET)
    {
      g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
    }
  else if (type != GTK_TOOLBAR_CHILD_RADIOBUTTON)
    g_return_val_if_fail (widget == NULL, NULL);

  if (type == GTK_TOOLBAR_CHILD_SPACE)
    child = (GtkToolbarChild *) g_new (GtkToolbarChildSpace, 1);
  else
    child = g_new (GtkToolbarChild, 1);

  child->type = type;
  child->icon = NULL;
  child->label = NULL;

  switch (type)
    {
    case GTK_TOOLBAR_CHILD_SPACE:
      child->widget = NULL;
      ((GtkToolbarChildSpace *) child)->alloc_x =
	((GtkToolbarChildSpace *) child)->alloc_y = 0;
      break;

    case GTK_TOOLBAR_CHILD_WIDGET:
      child->widget = widget;
      break;

    case GTK_TOOLBAR_CHILD_BUTTON:
    case GTK_TOOLBAR_CHILD_TOGGLEBUTTON:
    case GTK_TOOLBAR_CHILD_RADIOBUTTON:
      if (type == GTK_TOOLBAR_CHILD_BUTTON)
	{
	  child->widget = gtk_button_new ();
	  gtk_button_set_relief (GTK_BUTTON (child->widget), get_button_relief (toolbar));
	}
      else if (type == GTK_TOOLBAR_CHILD_TOGGLEBUTTON)
	{
	  child->widget = gtk_toggle_button_new ();
	  gtk_button_set_relief (GTK_BUTTON (child->widget), get_button_relief (toolbar));
	  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (child->widget),
				      FALSE);
	}
      else
	{
	  child->widget = gtk_radio_button_new (widget
						? gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget))
						: NULL);
	  gtk_button_set_relief (GTK_BUTTON (child->widget), get_button_relief (toolbar));
	  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (child->widget), FALSE);
	}

      GTK_WIDGET_UNSET_FLAGS (child->widget, GTK_CAN_FOCUS);

      if (callback)
	gtk_signal_connect (GTK_OBJECT (child->widget), "clicked",
			    callback, user_data);

      if (toolbar->style == GTK_TOOLBAR_BOTH_HORIZ)
	  box = gtk_hbox_new (FALSE, 0);
      else
	  box = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (child->widget), box);
      gtk_widget_show (box);

      if (text)
	{
          if (has_mnemonic)
            child->label = gtk_label_new_with_mnemonic (text);
          else
            child->label = gtk_label_new (text);
	  gtk_box_pack_end (GTK_BOX (box), child->label, FALSE, FALSE, 0);
	  if (toolbar->style != GTK_TOOLBAR_ICONS)
	    gtk_widget_show (child->label);
	}

      if (icon)
	{
	  child->icon = GTK_WIDGET (icon);
	  gtk_box_pack_end (GTK_BOX (box), child->icon, FALSE, FALSE, 0);
	  if (toolbar->style != GTK_TOOLBAR_TEXT)
	    gtk_widget_show (child->icon);
	}

      if (type != GTK_TOOLBAR_CHILD_WIDGET)
        {
          /* Mark child as ours */
          g_object_set_data (G_OBJECT (child->widget),
                             "gtk-toolbar-is-child",
                             GINT_TO_POINTER (TRUE));
        }
      gtk_widget_show (child->widget);
      break;

    default:
      g_assert_not_reached ();
    }

  if ((type != GTK_TOOLBAR_CHILD_SPACE) && tooltip_text)
    gtk_tooltips_set_tip (toolbar->tooltips, child->widget,
			  tooltip_text, tooltip_private_text);

  toolbar->children = g_list_insert (toolbar->children, child, position);
  toolbar->num_children++;

  if (type != GTK_TOOLBAR_CHILD_SPACE)
    gtk_widget_set_parent (child->widget, GTK_WIDGET (toolbar));
  else
    gtk_widget_queue_resize (GTK_WIDGET (toolbar));

  return child->widget;
}

void
gtk_toolbar_set_orientation (GtkToolbar     *toolbar,
			     GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  gtk_signal_emit (GTK_OBJECT (toolbar), toolbar_signals[ORIENTATION_CHANGED], orientation);
}

/**
 * gtk_toolbar_get_orientation:
 * @toolbar: a #GtkToolbar
 * 
 * Retrieves the current orientation of the toolbar. See
 * gtk_toolbar_set_orientation().
 *
 * Return value: the orientation
 **/
GtkOrientation
gtk_toolbar_get_orientation (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), GTK_ORIENTATION_HORIZONTAL);

  return toolbar->orientation;
}

void
gtk_toolbar_set_style (GtkToolbar      *toolbar,
		       GtkToolbarStyle  style)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  toolbar->style_set = TRUE;
  gtk_signal_emit (GTK_OBJECT (toolbar), toolbar_signals[STYLE_CHANGED], style);
}

/**
 * gtk_toolbar_get_style:
 * @toolbar: a #GtkToolbar
 *
 * Retrieves whether the toolbar has text, icons, or both . See
 * gtk_toolbar_set_style().
 
 * Return value: the current style of @toolbar
 **/
GtkToolbarStyle
gtk_toolbar_get_style (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH);

  return toolbar->style;
}

/**
 * gtk_toolbar_unset_style:
 * @toolbar: a #GtkToolbar
 * 
 * Unsets a toolbar style set with gtk_toolbar_set_style(), so that
 * user preferences will be used to determine the toolbar style.
 **/
void
gtk_toolbar_unset_style (GtkToolbar *toolbar)
{
  GtkToolbarStyle style;
  
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (toolbar->style_set)
    {
      g_object_get (gtk_settings_get_default (),
                    "gtk-toolbar-style",
                    &style,
                    NULL);

      if (style != toolbar->style)
        gtk_signal_emit (GTK_OBJECT (toolbar), toolbar_signals[STYLE_CHANGED], style);
      
      toolbar->style_set = FALSE;
    }
}

void
gtk_toolbar_set_tooltips (GtkToolbar *toolbar,
			  gboolean    enable)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (enable)
    gtk_tooltips_enable (toolbar->tooltips);
  else
    gtk_tooltips_disable (toolbar->tooltips);
}

/**
 * gtk_toolbar_get_tooltips:
 * @toolbar: a #GtkToolbar
 *
 * Retrieves whether tooltips are enabled. See
 * gtk_toolbar_set_tooltips().
 *
 * Return value: %TRUE if tooltips are enabled
 **/
gboolean
gtk_toolbar_get_tooltips (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), FALSE);

  return toolbar->tooltips->enabled;
}

static void
gtk_toolbar_update_button_relief (GtkToolbar *toolbar)
{
  GList *children;
  GtkToolbarChild *child;
  GtkReliefStyle relief;
  
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  relief = get_button_relief (toolbar);
  
  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;
      if (child->type == GTK_TOOLBAR_CHILD_BUTTON ||
          child->type == GTK_TOOLBAR_CHILD_RADIOBUTTON ||
          child->type == GTK_TOOLBAR_CHILD_TOGGLEBUTTON)
        gtk_button_set_relief (GTK_BUTTON (child->widget), relief);
    }
}

static void
gtk_real_toolbar_orientation_changed (GtkToolbar     *toolbar,
				      GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (toolbar->orientation != orientation)
    {
      toolbar->orientation = orientation;
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
      g_object_notify (G_OBJECT (toolbar), "orientation");
    }
}

static void
gtk_real_toolbar_style_changed (GtkToolbar      *toolbar,
				GtkToolbarStyle  style)
{
  GList *children;
  GtkToolbarChild *child;
  GtkWidget* box = NULL;
  
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (toolbar->style != style)
    {
      toolbar->style = style;

      for (children = toolbar->children; children; children = children->next)
	{
	  child = children->data;

	  if (child->type == GTK_TOOLBAR_CHILD_BUTTON ||
	      child->type == GTK_TOOLBAR_CHILD_RADIOBUTTON ||
	      child->type == GTK_TOOLBAR_CHILD_TOGGLEBUTTON)
	    switch (style)
	      {
	      case GTK_TOOLBAR_ICONS:
		if (child->icon && !GTK_WIDGET_VISIBLE (child->icon))
		  gtk_widget_show (child->icon);

		if (child->label && GTK_WIDGET_VISIBLE (child->label))
		  gtk_widget_hide (child->label);

		break;

	      case GTK_TOOLBAR_TEXT:
		if (child->icon && GTK_WIDGET_VISIBLE (child->icon))
		  gtk_widget_hide (child->icon);
		
		if (child->label && !GTK_WIDGET_VISIBLE (child->label))
		  gtk_widget_show (child->label);

		break;

	      case GTK_TOOLBAR_BOTH:
		if (child->icon && !GTK_WIDGET_VISIBLE (child->icon))
		  gtk_widget_show (child->icon);

		if (child->label && !GTK_WIDGET_VISIBLE (child->label))
		  gtk_widget_show (child->label);

		box = (GtkWidget*)gtk_container_get_children (GTK_CONTAINER (child->widget))->data;

		if (GTK_IS_HBOX (box))
		{
		    if (child->icon)
		    {
			gtk_object_ref (GTK_OBJECT (child->icon));
			gtk_container_remove (GTK_CONTAINER (box),
					      child->icon);
		    }
		    if (child->label)
		    {
			gtk_object_ref (GTK_OBJECT (child->label));
			gtk_container_remove (GTK_CONTAINER (box),
					      child->label);
		    }
		    gtk_container_remove (GTK_CONTAINER (child->widget),
					  box);
		    
		    box = gtk_vbox_new (FALSE, 0);
		    gtk_widget_show (box);
		    
		    if (child->label)
		    {
			gtk_box_pack_end (GTK_BOX (box), child->label, FALSE, FALSE, 0);
			gtk_object_unref (GTK_OBJECT (child->label));
		    }
		    if (child->icon)
		    {
			gtk_box_pack_end (GTK_BOX (box), child->icon, FALSE, FALSE, 0);
			gtk_object_unref (GTK_OBJECT (child->icon));
		    }
		    gtk_container_add (GTK_CONTAINER (child->widget),
				       box);
		}
		
		break;
		
	      case GTK_TOOLBAR_BOTH_HORIZ:
		if (child->icon && !GTK_WIDGET_VISIBLE (child->icon))
		  gtk_widget_show (child->icon);
		if (child->label && !GTK_WIDGET_VISIBLE (child->label))
		  gtk_widget_show (child->label);

		box = (GtkWidget*)gtk_container_get_children (GTK_CONTAINER (child->widget))->data;
		
		if (GTK_IS_VBOX (box))
		{
		    if (child->icon)
		    {
			gtk_object_ref (GTK_OBJECT (child->icon));
			gtk_container_remove (GTK_CONTAINER (box),
					      child->icon);
		    }
		    if (child->label)
		    {
			gtk_object_ref (GTK_OBJECT (child->label));
			gtk_container_remove (GTK_CONTAINER (box),
					      child->label);
		    }
		    gtk_container_remove (GTK_CONTAINER (child->widget),
					  box);

		    box = gtk_hbox_new (FALSE, 0);
		    gtk_widget_show (box);
		    
		    if (child->label)
		    {
			gtk_box_pack_end (GTK_BOX (box), child->label, TRUE, TRUE, 0);
			gtk_object_unref (GTK_OBJECT (child->label));
		    }
		    if (child->icon)
		    {
			gtk_box_pack_end (GTK_BOX (box), child->icon, FALSE, FALSE, 0);
			gtk_object_unref (GTK_OBJECT (child->icon));
		    }
		    gtk_container_add (GTK_CONTAINER (child->widget), box);
		    
		}
		
		break;

	      default:
		g_assert_not_reached ();
	      }
	}

      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
      g_object_notify (G_OBJECT (toolbar), "toolbar_style");
    }
}


static GtkReliefStyle
get_button_relief (GtkToolbar *toolbar)
{
  GtkReliefStyle button_relief = GTK_RELIEF_NORMAL;
  
  gtk_widget_style_get (GTK_WIDGET (toolbar),
                        "button_relief", &button_relief,
                        NULL);

  return button_relief;
}

static gint
get_space_size (GtkToolbar *toolbar)
{
  gint space_size = DEFAULT_SPACE_SIZE;

  gtk_widget_style_get (GTK_WIDGET (toolbar),
                        "space_size", &space_size,
                        NULL);

  return space_size;
}

static GtkToolbarSpaceStyle
get_space_style (GtkToolbar *toolbar)
{
  GtkToolbarSpaceStyle space_style = DEFAULT_SPACE_STYLE;

  gtk_widget_style_get (GTK_WIDGET (toolbar),
                        "space_style", &space_style,
                        NULL);


  return space_style;  
}
