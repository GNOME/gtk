/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#undef GDK_DISABLE_DEPRECATED
#undef GTK_DISABLE_DEPRECATED

#include "config.h"

#include "gtklabel.h"
#include "gtkeventbox.h"
#include "gtkpixmap.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtksignal.h"
#define GTK_ENABLE_BROKEN
#include "gtktree.h"
#include "gtktreeitem.h"
#include "gtkintl.h"

#include "gtkalias.h"

#include "tree_plus.xpm"
#include "tree_minus.xpm"

#define DEFAULT_DELTA 9

enum {
  COLLAPSE_TREE,
  EXPAND_TREE,
  LAST_SIGNAL
};

typedef struct _GtkTreePixmaps GtkTreePixmaps;

struct _GtkTreePixmaps {
  gint refcount;
  GdkColormap *colormap;
  
  GdkPixmap *pixmap_plus;
  GdkPixmap *pixmap_minus;
  GdkBitmap *mask_plus;
  GdkBitmap *mask_minus;
};

static GList *pixmaps = NULL;

static void gtk_tree_item_class_init (GtkTreeItemClass *klass);
static void gtk_tree_item_init       (GtkTreeItem      *tree_item);
static void gtk_tree_item_realize       (GtkWidget        *widget);
static void gtk_tree_item_size_request  (GtkWidget        *widget,
					 GtkRequisition   *requisition);
static void gtk_tree_item_size_allocate (GtkWidget        *widget,
					 GtkAllocation    *allocation);
static void gtk_tree_item_paint         (GtkWidget        *widget,
					 GdkRectangle     *area);
static gint gtk_tree_item_button_press  (GtkWidget        *widget,
					 GdkEventButton   *event);
static gint gtk_tree_item_expose        (GtkWidget        *widget,
					 GdkEventExpose   *event);
static void gtk_tree_item_forall        (GtkContainer    *container,
					 gboolean         include_internals,
					 GtkCallback      callback,
					 gpointer         callback_data);

static void gtk_real_tree_item_select   (GtkItem          *item);
static void gtk_real_tree_item_deselect (GtkItem          *item);
static void gtk_real_tree_item_toggle   (GtkItem          *item);
static void gtk_real_tree_item_expand   (GtkTreeItem      *item);
static void gtk_real_tree_item_collapse (GtkTreeItem      *item);
static void gtk_tree_item_destroy        (GtkObject *object);
static gint gtk_tree_item_subtree_button_click (GtkWidget *widget);
static void gtk_tree_item_subtree_button_changed_state (GtkWidget *widget);

static void gtk_tree_item_map(GtkWidget*);
static void gtk_tree_item_unmap(GtkWidget*);

static void gtk_tree_item_add_pixmaps    (GtkTreeItem       *tree_item);
static void gtk_tree_item_remove_pixmaps (GtkTreeItem       *tree_item);

static GtkItemClass *parent_class = NULL;
static guint tree_item_signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_tree_item_get_type (void)
{
  static GtkType tree_item_type = 0;

  if (!tree_item_type)
    {
      static const GtkTypeInfo tree_item_info =
      {
	"GtkTreeItem",
	sizeof (GtkTreeItem),
	sizeof (GtkTreeItemClass),
	(GtkClassInitFunc) gtk_tree_item_class_init,
	(GtkObjectInitFunc) gtk_tree_item_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      I_("GtkTreeItem");
      tree_item_type = gtk_type_unique (gtk_item_get_type (), &tree_item_info);
    }

  return tree_item_type;
}

static void
gtk_tree_item_class_init (GtkTreeItemClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkItemClass *item_class;

  parent_class = gtk_type_class (GTK_TYPE_ITEM);
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  item_class = (GtkItemClass*) class;
  container_class = (GtkContainerClass*) class;

  object_class->destroy = gtk_tree_item_destroy;

  widget_class->realize = gtk_tree_item_realize;
  widget_class->size_request = gtk_tree_item_size_request;
  widget_class->size_allocate = gtk_tree_item_size_allocate;
  widget_class->button_press_event = gtk_tree_item_button_press;
  widget_class->expose_event = gtk_tree_item_expose;
  widget_class->map = gtk_tree_item_map;
  widget_class->unmap = gtk_tree_item_unmap;

  container_class->forall = gtk_tree_item_forall;

  item_class->select = gtk_real_tree_item_select;
  item_class->deselect = gtk_real_tree_item_deselect;
  item_class->toggle = gtk_real_tree_item_toggle;

  class->expand = gtk_real_tree_item_expand;
  class->collapse = gtk_real_tree_item_collapse;

  tree_item_signals[EXPAND_TREE] =
    gtk_signal_new (I_("expand"),
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeItemClass, expand),
		    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  tree_item_signals[COLLAPSE_TREE] =
    gtk_signal_new (I_("collapse"),
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeItemClass, collapse),
		    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
}

/* callback for event box mouse event */
static gint
gtk_tree_item_subtree_button_click (GtkWidget *widget)
{
  GtkTreeItem* item;
  
  g_return_val_if_fail (GTK_IS_EVENT_BOX (widget), FALSE);
  
  item = (GtkTreeItem*) gtk_object_get_user_data (GTK_OBJECT (widget));
  if (!gtk_widget_is_sensitive (GTK_WIDGET (item)))
    return FALSE;
  
  if (item->expanded)
    gtk_tree_item_collapse (item);
  else
    gtk_tree_item_expand (item);

  return TRUE;
}

/* callback for event box state changed */
static void
gtk_tree_item_subtree_button_changed_state (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_EVENT_BOX (widget));
  
  if (gtk_widget_get_visible (widget))
    {
      
      if (widget->state == GTK_STATE_NORMAL)
	gdk_window_set_background (widget->window, &widget->style->base[widget->state]);
      else
	gdk_window_set_background (widget->window, &widget->style->bg[widget->state]);
      
      if (gtk_widget_is_drawable (widget))
	gdk_window_clear_area (widget->window, 0, 0, 
			       widget->allocation.width, widget->allocation.height);
    }
}

static void
gtk_tree_item_init (GtkTreeItem *tree_item)
{
  GtkWidget *eventbox, *pixmapwid;

  tree_item->expanded = FALSE;
  tree_item->subtree = NULL;
  gtk_widget_set_can_focus (GTK_WIDGET (tree_item), TRUE);
  
  /* create an event box containing one pixmaps */
  eventbox = gtk_event_box_new();
  gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK);
  gtk_signal_connect(GTK_OBJECT(eventbox), "state-changed",
		     G_CALLBACK (gtk_tree_item_subtree_button_changed_state),
		     (gpointer)NULL);
  gtk_signal_connect(GTK_OBJECT(eventbox), "realize",
		     G_CALLBACK (gtk_tree_item_subtree_button_changed_state),
		     (gpointer)NULL);
  gtk_signal_connect(GTK_OBJECT(eventbox), "button-press-event",
		     G_CALLBACK (gtk_tree_item_subtree_button_click),
		     (gpointer)NULL);
  gtk_object_set_user_data(GTK_OBJECT(eventbox), tree_item);
  tree_item->pixmaps_box = eventbox;

  /* create pixmap for button '+' */
  pixmapwid = gtk_type_new (gtk_pixmap_get_type ());
  if (!tree_item->expanded) 
    gtk_container_add (GTK_CONTAINER (eventbox), pixmapwid);
  gtk_widget_show (pixmapwid);
  tree_item->plus_pix_widget = pixmapwid;
  g_object_ref_sink (tree_item->plus_pix_widget);
  
  /* create pixmap for button '-' */
  pixmapwid = gtk_type_new (gtk_pixmap_get_type ());
  if (tree_item->expanded) 
    gtk_container_add (GTK_CONTAINER (eventbox), pixmapwid);
  gtk_widget_show (pixmapwid);
  tree_item->minus_pix_widget = pixmapwid;
  g_object_ref_sink (tree_item->minus_pix_widget);
  
  gtk_widget_set_parent (eventbox, GTK_WIDGET (tree_item));
}


GtkWidget*
gtk_tree_item_new (void)
{
  GtkWidget *tree_item;

  tree_item = GTK_WIDGET (gtk_type_new (gtk_tree_item_get_type ()));

  return tree_item;
}

GtkWidget*
gtk_tree_item_new_with_label (const gchar *label)
{
  GtkWidget *tree_item;
  GtkWidget *label_widget;

  tree_item = gtk_tree_item_new ();
  label_widget = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (tree_item), label_widget);
  gtk_widget_show (label_widget);


  return tree_item;
}

void
gtk_tree_item_set_subtree (GtkTreeItem *tree_item,
			   GtkWidget   *subtree)
{
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));
  g_return_if_fail (GTK_IS_TREE (subtree));

  if (tree_item->subtree)
    {
      g_warning("there is already a subtree for this tree item\n");
      return;
    }

  tree_item->subtree = subtree; 
  GTK_TREE (subtree)->tree_owner = GTK_WIDGET (tree_item);

  /* show subtree button */
  if (tree_item->pixmaps_box)
    gtk_widget_show (tree_item->pixmaps_box);

  if (tree_item->expanded)
    gtk_widget_show (subtree);
  else
    gtk_widget_hide (subtree);

  gtk_widget_set_parent (subtree, GTK_WIDGET (tree_item)->parent);
}

void
gtk_tree_item_select (GtkTreeItem *tree_item)
{
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));

  gtk_item_select (GTK_ITEM (tree_item));
}

void
gtk_tree_item_deselect (GtkTreeItem *tree_item)
{
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));

  gtk_item_deselect (GTK_ITEM (tree_item));
}

void
gtk_tree_item_expand (GtkTreeItem *tree_item)
{
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));

  gtk_signal_emit (GTK_OBJECT (tree_item), tree_item_signals[EXPAND_TREE], NULL);
}

void
gtk_tree_item_collapse (GtkTreeItem *tree_item)
{
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));

  gtk_signal_emit (GTK_OBJECT (tree_item), tree_item_signals[COLLAPSE_TREE], NULL);
}

static void
gtk_tree_item_add_pixmaps (GtkTreeItem *tree_item)
{
  GList *tmp_list;
  GdkColormap *colormap;
  GtkTreePixmaps *pixmap_node = NULL;

  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));

  if (tree_item->pixmaps)
    return;

  colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_item));

  tmp_list = pixmaps;
  while (tmp_list)
    {
      pixmap_node = (GtkTreePixmaps *)tmp_list->data;

      if (pixmap_node->colormap == colormap)
	break;
      
      tmp_list = tmp_list->next;
    }

  if (tmp_list)
    {
      pixmap_node->refcount++;
      tree_item->pixmaps = tmp_list;
    }
  else
    {
      pixmap_node = g_new (GtkTreePixmaps, 1);

      pixmap_node->colormap = colormap;
      g_object_ref (colormap);

      pixmap_node->refcount = 1;

      /* create pixmaps for plus icon */
      pixmap_node->pixmap_plus = 
	gdk_pixmap_create_from_xpm_d (GTK_WIDGET (tree_item)->window,
				      &pixmap_node->mask_plus,
				      NULL,
				      (gchar **)tree_plus);
      
      /* create pixmaps for minus icon */
      pixmap_node->pixmap_minus = 
	gdk_pixmap_create_from_xpm_d (GTK_WIDGET (tree_item)->window,
				      &pixmap_node->mask_minus,
				      NULL,
				      (gchar **)tree_minus);

      tree_item->pixmaps = pixmaps = g_list_prepend (pixmaps, pixmap_node);
    }
  
  gtk_pixmap_set (GTK_PIXMAP (tree_item->plus_pix_widget), 
		  pixmap_node->pixmap_plus, pixmap_node->mask_plus);
  gtk_pixmap_set (GTK_PIXMAP (tree_item->minus_pix_widget), 
		  pixmap_node->pixmap_minus, pixmap_node->mask_minus);
}

static void
gtk_tree_item_remove_pixmaps (GtkTreeItem *tree_item)
{
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));

  if (tree_item->pixmaps)
    {
      GtkTreePixmaps *pixmap_node = (GtkTreePixmaps *)tree_item->pixmaps->data;
      
      g_assert (pixmap_node->refcount > 0);
      
      if (--pixmap_node->refcount == 0)
	{
	  g_object_unref (pixmap_node->colormap);
	  g_object_unref (pixmap_node->pixmap_plus);
	  g_object_unref (pixmap_node->mask_plus);
	  g_object_unref (pixmap_node->pixmap_minus);
	  g_object_unref (pixmap_node->mask_minus);
	  
	  pixmaps = g_list_remove_link (pixmaps, tree_item->pixmaps);
	  g_list_free_1 (tree_item->pixmaps);
	  g_free (pixmap_node);
	}

      tree_item->pixmaps = NULL;
    }
}

static void
gtk_tree_item_realize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  gdk_window_set_background (widget->window, 
			     &widget->style->base[GTK_STATE_NORMAL]);

  gtk_tree_item_add_pixmaps (GTK_TREE_ITEM (widget));
}

static void
gtk_tree_item_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkTreeItem *item = GTK_TREE_ITEM (widget);
  GtkRequisition child_requisition;

  requisition->width = (GTK_CONTAINER (widget)->border_width +
			widget->style->xthickness) * 2;
  requisition->height = GTK_CONTAINER (widget)->border_width * 2;

  if (bin->child && gtk_widget_get_visible (bin->child))
    {
      GtkRequisition pix_requisition;
      
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += child_requisition.width;

      gtk_widget_size_request (item->pixmaps_box, 
			       &pix_requisition);
      requisition->width += pix_requisition.width + DEFAULT_DELTA + 
	GTK_TREE (widget->parent)->current_indent;

      requisition->height += MAX (child_requisition.height,
				  pix_requisition.height);
    }
}

static void
gtk_tree_item_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkTreeItem *item = GTK_TREE_ITEM (widget);
  GtkAllocation child_allocation;
  gint border_width;
  int temp;

  widget->allocation = *allocation;
  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  if (bin->child)
    {
      border_width = (GTK_CONTAINER (widget)->border_width +
		      widget->style->xthickness);

      child_allocation.x = border_width + GTK_TREE(widget->parent)->current_indent;
      child_allocation.y = GTK_CONTAINER (widget)->border_width;

      child_allocation.width = item->pixmaps_box->requisition.width;
      child_allocation.height = item->pixmaps_box->requisition.height;
      
      temp = allocation->height - child_allocation.height;
      child_allocation.y += ( temp / 2 ) + ( temp % 2 );

      gtk_widget_size_allocate (item->pixmaps_box, &child_allocation);

      child_allocation.y = GTK_CONTAINER (widget)->border_width;
      child_allocation.height = MAX (1, (gint)allocation->height - child_allocation.y * 2);
      child_allocation.x += item->pixmaps_box->requisition.width+DEFAULT_DELTA;

      child_allocation.width = 
	MAX (1, (gint)allocation->width - ((gint)child_allocation.x + border_width));

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void 
gtk_tree_item_draw_lines (GtkWidget *widget) 
{
  GtkTreeItem* item;
  GtkTree* tree;
  guint lx1, ly1, lx2, ly2;
  GdkGC* gc;

  g_return_if_fail (GTK_IS_TREE_ITEM (widget));

  item = GTK_TREE_ITEM(widget);
  tree = GTK_TREE(widget->parent);

  if (!tree->view_line)
    return;

  gc = widget->style->text_gc[GTK_STATE_NORMAL];

  /* draw vertical line */
  lx1 = item->pixmaps_box->allocation.width;
  lx1 = lx2 = ((lx1 / 2) + (lx1 % 2) + 
	       GTK_CONTAINER (widget)->border_width + 1 + tree->current_indent);
  ly1 = 0;
  ly2 = widget->allocation.height;

  if (g_list_last (tree->children)->data == widget)
    ly2 = (ly2 / 2) + (ly2 % 2);

  if (tree != tree->root_tree)
    gdk_draw_line (widget->window, gc, lx1, ly1, lx2, ly2);

  /* draw vertical line for subtree connecting */
  if(g_list_last(tree->children)->data != (gpointer)widget)
    ly2 = (ly2 / 2) + (ly2 % 2);
  
  lx2 += DEFAULT_DELTA;

  if (item->subtree && item->expanded)
    gdk_draw_line (widget->window, gc,
		   lx2, ly2, lx2, widget->allocation.height);

  /* draw horizontal line */
  ly1 = ly2;
  lx2 += 2;

  gdk_draw_line (widget->window, gc, lx1, ly1, lx2, ly2);

  lx2 -= DEFAULT_DELTA+2;
  ly1 = 0;
  ly2 = widget->allocation.height;

  if (tree != tree->root_tree)
    {
      item = GTK_TREE_ITEM (tree->tree_owner);
      tree = GTK_TREE (GTK_WIDGET (tree)->parent);
      while (tree != tree->root_tree)
	{
	  lx1 = lx2 -= tree->indent_value;
	  
	  if (g_list_last (tree->children)->data != item)
	    gdk_draw_line (widget->window, gc, lx1, ly1, lx2, ly2);
	  item = GTK_TREE_ITEM (tree->tree_owner);
	  tree = GTK_TREE (GTK_WIDGET (tree)->parent);
	} 
    }
}

static void
gtk_tree_item_paint (GtkWidget    *widget,
		     GdkRectangle *area)
{
  GdkRectangle child_area, item_area;
  GtkTreeItem* tree_item;

  /* FIXME: We should honor tree->view_mode, here - I think
   * the desired effect is that when the mode is VIEW_ITEM,
   * only the subitem is drawn as selected, not the entire
   * line. (Like the way that the tree in Windows Explorer
   * works).
   */
  if (gtk_widget_is_drawable (widget))
    {
      tree_item = GTK_TREE_ITEM(widget);

      if (widget->state == GTK_STATE_NORMAL)
	{
	  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
	  gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);
	}
      else 
	{
	  if (!gtk_widget_is_sensitive (widget))
	    gtk_paint_flat_box(widget->style, widget->window,
			       widget->state, GTK_SHADOW_NONE,
			       area, widget, "treeitem",
			       0, 0, -1, -1);
	  else
	    gtk_paint_flat_box(widget->style, widget->window,
			       widget->state, GTK_SHADOW_ETCHED_OUT,
			       area, widget, "treeitem",
			       0, 0, -1, -1);
	}

      /* draw left size of tree item */
      item_area.x = 0;
      item_area.y = 0;
      item_area.width = (tree_item->pixmaps_box->allocation.width + DEFAULT_DELTA +
			 GTK_TREE (widget->parent)->current_indent + 2);
      item_area.height = widget->allocation.height;


      if (gdk_rectangle_intersect(&item_area, area, &child_area)) 
	{
	  
	  gtk_tree_item_draw_lines(widget);

	  if (tree_item->pixmaps_box && 
	      gtk_widget_get_visible(tree_item->pixmaps_box) &&
	      gtk_widget_intersect (tree_item->pixmaps_box, area, &child_area))
            {
              gtk_widget_queue_draw_area (tree_item->pixmaps_box,
                                          child_area.x, child_area.y,
                                          child_area.width, child_area.height);
              gdk_window_process_updates (tree_item->pixmaps_box->window, TRUE);
            }
	}

      if (gtk_widget_has_focus (widget))
	gtk_paint_focus (widget->style, widget->window, gtk_widget_get_state (widget),
			 NULL, widget, "treeitem",
			 0, 0,
			 widget->allocation.width,
			 widget->allocation.height);
      
    }
}

static gint
gtk_tree_item_button_press (GtkWidget      *widget,
			    GdkEventButton *event)
{
  if (event->type == GDK_BUTTON_PRESS
      && gtk_widget_is_sensitive(widget)
      && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  return (event->type == GDK_BUTTON_PRESS && gtk_widget_is_sensitive(widget));
}

static void
gtk_tree_item_expose_child (GtkWidget *child,
                            gpointer   client_data)
{
  struct {
    GtkWidget *container;
    GdkEventExpose *event;
  } *data = client_data;

  if (gtk_widget_is_drawable (child) &&
      !gtk_widget_get_has_window (child) &&
      (child->window == data->event->window))
    {
      GdkEvent *child_event = gdk_event_new (GDK_EXPOSE);
      child_event->expose = *data->event;
      g_object_ref (child_event->expose.window);

      child_event->expose.region = gtk_widget_region_intersect (child,
								data->event->region);
      if (!gdk_region_empty (child_event->expose.region))
        {
          gdk_region_get_clipbox (child_event->expose.region, &child_event->expose.area);
          gtk_widget_send_expose (child, child_event);
	}
      gdk_event_free (child_event);
    }
}

static gint
gtk_tree_item_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  struct {
    GtkWidget *container;
    GdkEventExpose *event;
  } data;

  if (gtk_widget_is_drawable (widget))
    {
      gtk_tree_item_paint (widget, &event->area);

      data.container = widget;
      data.event = event;

      gtk_container_forall (GTK_CONTAINER (widget),
                            gtk_tree_item_expose_child,
                            &data);
   }

  return FALSE;
}

static void
gtk_real_tree_item_select (GtkItem *item)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_TREE_ITEM (item));

  widget = GTK_WIDGET (item);

  gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_SELECTED);

  if (!widget->parent || GTK_TREE (widget->parent)->view_mode == GTK_TREE_VIEW_LINE)
    gtk_widget_set_state (GTK_TREE_ITEM (item)->pixmaps_box, GTK_STATE_SELECTED);
}

static void
gtk_real_tree_item_deselect (GtkItem *item)
{
  GtkTreeItem *tree_item;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_TREE_ITEM (item));

  tree_item = GTK_TREE_ITEM (item);
  widget = GTK_WIDGET (item);

  gtk_widget_set_state (widget, GTK_STATE_NORMAL);

  if (!widget->parent || GTK_TREE (widget->parent)->view_mode == GTK_TREE_VIEW_LINE)
    gtk_widget_set_state (tree_item->pixmaps_box, GTK_STATE_NORMAL);
}

static void
gtk_real_tree_item_toggle (GtkItem *item)
{
  g_return_if_fail (GTK_IS_TREE_ITEM (item));

  if(!gtk_widget_is_sensitive(GTK_WIDGET (item)))
    return;

  if (GTK_IS_TREE (GTK_WIDGET (item)->parent))
    gtk_tree_select_child (GTK_TREE (GTK_WIDGET (item)->parent),
			   GTK_WIDGET (item));
  else
    {
      /* Should we really bother with this bit? A listitem not in a list?
       * -Johannes Keukelaar
       * yes, always be on the safe side!
       * -timj
       */
      if (GTK_WIDGET (item)->state == GTK_STATE_SELECTED)
	gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_NORMAL);
      else
	gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_SELECTED);
    }
}

static void
gtk_real_tree_item_expand (GtkTreeItem *tree_item)
{
  GtkTree* tree;
  
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));
  
  if (tree_item->subtree && !tree_item->expanded)
    {
      tree = GTK_TREE (GTK_WIDGET (tree_item)->parent); 
      
      /* hide subtree widget */
      gtk_widget_show (tree_item->subtree);
      
      /* hide button '+' and show button '-' */
      if (tree_item->pixmaps_box)
	{
	  gtk_container_remove (GTK_CONTAINER (tree_item->pixmaps_box), 
				tree_item->plus_pix_widget);
	  gtk_container_add (GTK_CONTAINER (tree_item->pixmaps_box), 
			     tree_item->minus_pix_widget);
	}
      if (tree->root_tree)
	gtk_widget_queue_resize (GTK_WIDGET (tree->root_tree));
      tree_item->expanded = TRUE;
    }
}

static void
gtk_real_tree_item_collapse (GtkTreeItem *tree_item)
{
  GtkTree* tree;
  
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));
  
  if (tree_item->subtree && tree_item->expanded) 
    {
      tree = GTK_TREE (GTK_WIDGET (tree_item)->parent);
      
      /* hide subtree widget */
      gtk_widget_hide (tree_item->subtree);
      
      /* hide button '-' and show button '+' */
      if (tree_item->pixmaps_box)
	{
	  gtk_container_remove (GTK_CONTAINER (tree_item->pixmaps_box), 
				tree_item->minus_pix_widget);
	  gtk_container_add (GTK_CONTAINER (tree_item->pixmaps_box), 
			     tree_item->plus_pix_widget);
	}
      if (tree->root_tree)
	gtk_widget_queue_resize (GTK_WIDGET (tree->root_tree));
      tree_item->expanded = FALSE;
    }
}

static void
gtk_tree_item_destroy (GtkObject *object)
{
  GtkTreeItem* item = GTK_TREE_ITEM(object);
  GtkWidget* child;

#ifdef TREE_DEBUG
  g_message("+ gtk_tree_item_destroy [object %#x]\n", (int)object);
#endif /* TREE_DEBUG */

  /* free sub tree if it exist */
  child = item->subtree;
  if (child)
    {
      g_object_ref (child);
      gtk_widget_unparent (child);
      gtk_widget_destroy (child);
      g_object_unref (child);
      item->subtree = NULL;
    }
  
  /* free pixmaps box */
  child = item->pixmaps_box;
  if (child)
    {
      g_object_ref (child);
      gtk_widget_unparent (child);
      gtk_widget_destroy (child);
      g_object_unref (child);
      item->pixmaps_box = NULL;
    }
  
  
  /* destroy plus pixmap */
  if (item->plus_pix_widget)
    {
      gtk_widget_destroy (item->plus_pix_widget);
      g_object_unref (item->plus_pix_widget);
      item->plus_pix_widget = NULL;
    }
  
  /* destroy minus pixmap */
  if (item->minus_pix_widget)
    {
      gtk_widget_destroy (item->minus_pix_widget);
      g_object_unref (item->minus_pix_widget);
      item->minus_pix_widget = NULL;
    }
  
  /* By removing the pixmaps here, and not in unrealize, we depend on
   * the fact that a widget can never change colormap or visual.
   */
  gtk_tree_item_remove_pixmaps (item);
  
  GTK_OBJECT_CLASS (parent_class)->destroy (object);
  
#ifdef TREE_DEBUG
  g_message("- gtk_tree_item_destroy\n");
#endif /* TREE_DEBUG */
}

void
gtk_tree_item_remove_subtree (GtkTreeItem* item) 
{
  g_return_if_fail (GTK_IS_TREE_ITEM(item));
  g_return_if_fail (item->subtree != NULL);
  
  if (GTK_TREE (item->subtree)->children)
    {
      /* The following call will remove the children and call
       * gtk_tree_item_remove_subtree() again. So we are done.
       */
      gtk_tree_remove_items (GTK_TREE (item->subtree), 
			     GTK_TREE (item->subtree)->children);
      return;
    }

  if (gtk_widget_get_mapped (item->subtree))
    gtk_widget_unmap (item->subtree);
      
  gtk_widget_unparent (item->subtree);
  
  if (item->pixmaps_box)
    gtk_widget_hide (item->pixmaps_box);
  
  item->subtree = NULL;

  if (item->expanded)
    {
      item->expanded = FALSE;
      if (item->pixmaps_box)
	{
	  gtk_container_remove (GTK_CONTAINER (item->pixmaps_box), 
				item->minus_pix_widget);
	  gtk_container_add (GTK_CONTAINER (item->pixmaps_box), 
			     item->plus_pix_widget);
	}
    }
}

static void
gtk_tree_item_map (GtkWidget *widget)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkTreeItem* item = GTK_TREE_ITEM(widget);

  gtk_widget_set_mapped (widget, TRUE);

  if(item->pixmaps_box &&
     gtk_widget_get_visible (item->pixmaps_box) &&
     !gtk_widget_get_mapped (item->pixmaps_box))
    gtk_widget_map (item->pixmaps_box);

  if (bin->child &&
      gtk_widget_get_visible (bin->child) &&
      !gtk_widget_get_mapped (bin->child))
    gtk_widget_map (bin->child);

  gdk_window_show (widget->window);
}

static void
gtk_tree_item_unmap (GtkWidget *widget)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkTreeItem* item = GTK_TREE_ITEM(widget);

  gtk_widget_set_mapped (widget, FALSE);

  gdk_window_hide (widget->window);

  if(item->pixmaps_box &&
     gtk_widget_get_visible (item->pixmaps_box) &&
     gtk_widget_get_mapped (item->pixmaps_box))
    gtk_widget_unmap (bin->child);

  if (bin->child &&
      gtk_widget_get_visible (bin->child) &&
      gtk_widget_get_mapped (bin->child))
    gtk_widget_unmap (bin->child);
}

static void
gtk_tree_item_forall (GtkContainer *container,
		      gboolean      include_internals,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkBin *bin = GTK_BIN (container);
  GtkTreeItem *tree_item = GTK_TREE_ITEM (container);

  if (bin->child)
    (* callback) (bin->child, callback_data);
  if (include_internals && tree_item->subtree)
    (* callback) (tree_item->subtree, callback_data);
  if (include_internals && tree_item->pixmaps_box)
    (* callback) (tree_item->pixmaps_box, callback_data);
}

#define __GTK_TREE_ITEM_C__
#include "gtkaliasdef.c"
