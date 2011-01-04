/* gtkellview.c
 * Copyright (C) 2002, 2003  Kristian Rietveld <kris@gtk.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <string.h>
#include "gtkcellview.h"
#include "gtkcelllayout.h"
#include "gtkintl.h"
#include "gtkcellrenderertext.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include <gobject/gmarshal.h>
#include "gtkbuildable.h"


/**
 * SECTION:gtkcellview
 * @Short_description: A widget displaying a single row of a GtkTreeModel
 * @Title: GtkCellView
 *
 * A #GtkCellView displays a single row of a #GtkTreeModel, using
 * cell renderers just like #GtkTreeView. #GtkCellView doesn't support
 * some of the more complex features of #GtkTreeView, like cell editing
 * and drag and drop.
 */



typedef struct _GtkCellViewCellInfo GtkCellViewCellInfo;
struct _GtkCellViewCellInfo
{
  GtkCellRenderer *cell;

  gint requested_width;
  gint natural_width;
  gint real_width;
  guint expand : 1;
  guint pack : 1;

  GSList *attributes;

  GtkCellLayoutDataFunc func;
  gpointer func_data;
  GDestroyNotify destroy;
};

struct _GtkCellViewPrivate
{
  GtkTreeModel *model;
  GtkTreeRowReference *displayed_row;
  GList *cell_list;
  gint spacing;

  GdkRGBA background;
  gboolean background_set;
};


static void        gtk_cell_view_cell_layout_init         (GtkCellLayoutIface *iface);
static void        gtk_cell_view_get_property             (GObject           *object,
                                                           guint             param_id,
                                                           GValue           *value,
                                                           GParamSpec       *pspec);
static void        gtk_cell_view_set_property             (GObject          *object,
                                                           guint             param_id,
                                                           const GValue     *value,
                                                           GParamSpec       *pspec);
static void        gtk_cell_view_finalize                 (GObject          *object);
static void        gtk_cell_view_size_allocate            (GtkWidget        *widget,
                                                           GtkAllocation    *allocation);
static gboolean    gtk_cell_view_draw                     (GtkWidget        *widget,
                                                           cairo_t          *cr);
static void        gtk_cell_view_set_value                (GtkCellView     *cell_view,
                                                           GtkCellRenderer *renderer,
                                                           gchar           *property,
                                                           GValue          *value);
static GtkCellViewCellInfo *gtk_cell_view_get_cell_info   (GtkCellView      *cellview,
                                                           GtkCellRenderer  *renderer);
static void        gtk_cell_view_set_cell_data            (GtkCellView      *cell_view);


static void        gtk_cell_view_cell_layout_pack_start        (GtkCellLayout         *layout,
                                                                GtkCellRenderer       *renderer,
                                                                gboolean               expand);
static void        gtk_cell_view_cell_layout_pack_end          (GtkCellLayout         *layout,
                                                                GtkCellRenderer       *renderer,
                                                                gboolean               expand);
static void        gtk_cell_view_cell_layout_add_attribute     (GtkCellLayout         *layout,
                                                                GtkCellRenderer       *renderer,
                                                                const gchar           *attribute,
                                                                gint                   column);
static void       gtk_cell_view_cell_layout_clear              (GtkCellLayout         *layout);
static void       gtk_cell_view_cell_layout_clear_attributes   (GtkCellLayout         *layout,
                                                                GtkCellRenderer       *renderer);
static void       gtk_cell_view_cell_layout_set_cell_data_func (GtkCellLayout         *layout,
                                                                GtkCellRenderer       *cell,
                                                                GtkCellLayoutDataFunc  func,
                                                                gpointer               func_data,
                                                                GDestroyNotify         destroy);
static void       gtk_cell_view_cell_layout_reorder            (GtkCellLayout         *layout,
                                                                GtkCellRenderer       *cell,
                                                                gint                   position);
static GList *    gtk_cell_view_cell_layout_get_cells          (GtkCellLayout         *layout);

/* buildable */
static void       gtk_cell_view_buildable_init                 (GtkBuildableIface     *iface);
static gboolean   gtk_cell_view_buildable_custom_tag_start     (GtkBuildable  	      *buildable,
								GtkBuilder    	      *builder,
								GObject       	      *child,
								const gchar   	      *tagname,
								GMarkupParser 	      *parser,
								gpointer      	      *data);
static void       gtk_cell_view_buildable_custom_tag_end       (GtkBuildable  	      *buildable,
								GtkBuilder    	      *builder,
								GObject       	      *child,
								const gchar   	      *tagname,
								gpointer      	      *data);

static void       gtk_cell_view_get_preferred_width            (GtkWidget             *widget,
								gint                  *minimum_size,
								gint                  *natural_size);
static void       gtk_cell_view_get_preferred_height           (GtkWidget             *widget,
								gint                  *minimum_size,
								gint                  *natural_size);
static void       gtk_cell_view_get_preferred_width_for_height (GtkWidget             *widget,
								gint                   avail_size,
								gint                  *minimum_size,
								gint                  *natural_size);
static void       gtk_cell_view_get_preferred_height_for_width (GtkWidget             *widget,
								gint                   avail_size,
								gint                  *minimum_size,
								gint                  *natural_size);

static GtkBuildableIface *parent_buildable_iface;


enum
{
  PROP_0,
  PROP_BACKGROUND,
  PROP_BACKGROUND_GDK,
  PROP_BACKGROUND_RGBA,
  PROP_BACKGROUND_SET,
  PROP_MODEL
};

G_DEFINE_TYPE_WITH_CODE (GtkCellView, gtk_cell_view, GTK_TYPE_WIDGET, 
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
						gtk_cell_view_cell_layout_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_cell_view_buildable_init))


static void
gtk_cell_view_class_init (GtkCellViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->get_property = gtk_cell_view_get_property;
  gobject_class->set_property = gtk_cell_view_set_property;
  gobject_class->finalize = gtk_cell_view_finalize;

  widget_class->draw                           = gtk_cell_view_draw;
  widget_class->size_allocate                  = gtk_cell_view_size_allocate;
  widget_class->get_preferred_width            = gtk_cell_view_get_preferred_width;
  widget_class->get_preferred_height           = gtk_cell_view_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_cell_view_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_cell_view_get_preferred_height_for_width;

  /* properties */
  g_object_class_install_property (gobject_class,
                                   PROP_BACKGROUND,
                                   g_param_spec_string ("background",
                                                        P_("Background color name"),
                                                        P_("Background color as a string"),
                                                        NULL,
                                                        GTK_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_BACKGROUND_GDK,
                                   g_param_spec_boxed ("background-gdk",
                                                      P_("Background color"),
                                                      P_("Background color as a GdkColor"),
                                                      GDK_TYPE_COLOR,
                                                      GTK_PARAM_READWRITE));
  /**
   * GtkCellView:background-rgba
   *
   * The background color as a #GdkRGBA
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_BACKGROUND_RGBA,
                                   g_param_spec_boxed ("background-rgba",
                                                      P_("Background RGBA color"),
                                                      P_("Background color as a GdkRGBA"),
                                                      GDK_TYPE_RGBA,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkCellView:model
   *
   * The model for cell view
   *
   * since 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_MODEL,
				   g_param_spec_object  ("model",
							 P_("CellView model"),
							 P_("The model for cell view"),
							 GTK_TYPE_TREE_MODEL,
							 GTK_PARAM_READWRITE));
  
#define ADD_SET_PROP(propname, propval, nick, blurb) g_object_class_install_property (gobject_class, propval, g_param_spec_boolean (propname, nick, blurb, FALSE, GTK_PARAM_READWRITE))

  ADD_SET_PROP ("background-set", PROP_BACKGROUND_SET,
                P_("Background set"),
                P_("Whether this tag affects the background color"));

  g_type_class_add_private (gobject_class, sizeof (GtkCellViewPrivate));
}

static void
gtk_cell_view_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = _gtk_cell_layout_buildable_add_child;
  iface->custom_tag_start = gtk_cell_view_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_cell_view_buildable_custom_tag_end;
}

static void
gtk_cell_view_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->pack_start = gtk_cell_view_cell_layout_pack_start;
  iface->pack_end = gtk_cell_view_cell_layout_pack_end;
  iface->clear = gtk_cell_view_cell_layout_clear;
  iface->add_attribute = gtk_cell_view_cell_layout_add_attribute;
  iface->set_cell_data_func = gtk_cell_view_cell_layout_set_cell_data_func;
  iface->clear_attributes = gtk_cell_view_cell_layout_clear_attributes;
  iface->reorder = gtk_cell_view_cell_layout_reorder;
  iface->get_cells = gtk_cell_view_cell_layout_get_cells;
}

static void
gtk_cell_view_get_property (GObject    *object,
                            guint       param_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkCellView *view = GTK_CELL_VIEW (object);

  switch (param_id)
    {
      case PROP_BACKGROUND_GDK:
        {
          GdkColor color;

          color.red = (guint) (view->priv->background.red * 65535);
          color.green = (guint) (view->priv->background.green * 65535);
          color.blue = (guint) (view->priv->background.blue * 65535);
          color.pixel = 0;

          g_value_set_boxed (value, &color);
        }
        break;
      case PROP_BACKGROUND_RGBA:
        g_value_set_boxed (value, &view->priv->background);
        break;
      case PROP_BACKGROUND_SET:
        g_value_set_boolean (value, view->priv->background_set);
        break;
      case PROP_MODEL:
	g_value_set_object (value, view->priv->model);
	break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
gtk_cell_view_set_property (GObject      *object,
                            guint         param_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkCellView *view = GTK_CELL_VIEW (object);

  switch (param_id)
    {
      case PROP_BACKGROUND:
        {
          GdkColor color;

          if (!g_value_get_string (value))
            gtk_cell_view_set_background_color (view, NULL);
          else if (gdk_color_parse (g_value_get_string (value), &color))
            gtk_cell_view_set_background_color (view, &color);
          else
            g_warning ("Don't know color `%s'", g_value_get_string (value));

          g_object_notify (object, "background-gdk");
        }
        break;
      case PROP_BACKGROUND_GDK:
        gtk_cell_view_set_background_color (view, g_value_get_boxed (value));
        break;
      case PROP_BACKGROUND_RGBA:
        gtk_cell_view_set_background_rgba (view, g_value_get_boxed (value));
        break;
      case PROP_BACKGROUND_SET:
        view->priv->background_set = g_value_get_boolean (value);
        break;
      case PROP_MODEL:
	gtk_cell_view_set_model (view, g_value_get_object (value));
	break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
gtk_cell_view_init (GtkCellView *cellview)
{
  GtkCellViewPrivate *priv;

  cellview->priv = G_TYPE_INSTANCE_GET_PRIVATE (cellview,
                                                GTK_TYPE_CELL_VIEW,
                                                GtkCellViewPrivate);
  priv = cellview->priv;

  gtk_widget_set_has_window (GTK_WIDGET (cellview), FALSE);
}

static void
gtk_cell_view_finalize (GObject *object)
{
  GtkCellView *cellview = GTK_CELL_VIEW (object);

  gtk_cell_view_cell_layout_clear (GTK_CELL_LAYOUT (cellview));

  if (cellview->priv->model)
     g_object_unref (cellview->priv->model);

  if (cellview->priv->displayed_row)
     gtk_tree_row_reference_free (cellview->priv->displayed_row);

  G_OBJECT_CLASS (gtk_cell_view_parent_class)->finalize (object);
}

static void
gtk_cell_view_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkCellView      *cellview;
  GtkRequestedSize *sizes;
  GList            *list;
  gint              n_visible_cells, n_expand_cells;
  gint              avail_width = 0;
  gint              extra_per_cell, extra_extra, i;
  gboolean          first_cell = TRUE;

  gtk_widget_set_allocation (widget, allocation);

  cellview = GTK_CELL_VIEW (widget);

  avail_width = allocation->width;

  /* Count visible/expand children */
  for (n_visible_cells = 0, n_expand_cells = 0, list = cellview->priv->cell_list; 
       list; list = list->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)list->data;

      n_visible_cells++;

      if (info->expand)
        n_expand_cells++;
    }

  sizes = g_new0 (GtkRequestedSize, n_visible_cells);

  /* checking how much extra space we have */
  for (i = 0, list = cellview->priv->cell_list; list; list = list->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)list->data;

      if (!gtk_cell_renderer_get_visible (info->cell))
        continue;

      sizes[i].data = info;
      sizes[i].minimum_size = info->requested_width;
      sizes[i].natural_size = info->natural_width;

      if (!first_cell)
	avail_width -= cellview->priv->spacing;

      avail_width -= sizes[i].minimum_size;

      first_cell = FALSE;

      i++;
    }

  avail_width = gtk_distribute_natural_allocation (MAX (0, avail_width), n_visible_cells, sizes);

  /* Deal with any expand space... */
  if (n_expand_cells > 0)
    {
      extra_per_cell = avail_width / n_expand_cells;
      extra_extra    = avail_width % n_expand_cells;
    }
  else
    /* Everything just left-aligned if no cells expand */
    extra_per_cell = extra_extra = 0;

  for (i = 0, list = cellview->priv->cell_list; list; list = list->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)list->data;

      if (!gtk_cell_renderer_get_visible (info->cell))
        continue;

      info->real_width = sizes[i].minimum_size;

      if (info->expand)
        {
	  info->real_width += extra_per_cell;

	  if (extra_extra)
	    {
	      info->real_width++;
	      extra_extra--;
	    }
	}
      
      /* increment index into sizes for visible children */
      i++;
    }

  g_free (sizes);
}

static gboolean
gtk_cell_view_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  GList *list;
  GtkCellView *cellview;
  GdkRectangle area;
  GtkCellRendererState state;
  gboolean rtl = (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL);
  GtkPackType packing;
  int width;

  cellview = GTK_CELL_VIEW (widget);

  /* render cells */
  area.x = 0;
  area.y = 0;
  area.width = width = gtk_widget_get_allocated_width (widget);
  area.height = gtk_widget_get_allocated_height (widget);

  /* "blank" background */
  if (cellview->priv->background_set)
    {
      gdk_cairo_rectangle (cr, &area);
      gdk_cairo_set_source_rgba (cr, &cellview->priv->background);
      cairo_fill (cr);
    }

  /* set cell data (if available) */
  if (cellview->priv->displayed_row)
    gtk_cell_view_set_cell_data (cellview);
  else if (cellview->priv->model)
    return FALSE;

  if (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_PRELIGHT)
    state = GTK_CELL_RENDERER_PRELIT;
  else if (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_INSENSITIVE)
    state = GTK_CELL_RENDERER_INSENSITIVE;
  else
    state = 0;
      
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; ++packing)
    {
      if (packing == GTK_PACK_START)
	area.x = rtl ? width : 0;
      else
	area.x = rtl ? 0 : width;

      for (list = cellview->priv->cell_list; list; list = list->next)
	{
	  GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)list->data;

	  if (info->pack != packing)
	    continue;

	  if (!gtk_cell_renderer_get_visible (info->cell))
	    continue;

	  area.width = info->real_width;

	  if ((packing == GTK_PACK_START && rtl) ||
	      (packing == GTK_PACK_END && !rtl))
	    area.x -= area.width;

	  gtk_cell_renderer_render (info->cell,
                                    cr,
                                    widget,
                                    /* FIXME! */
                                    &area, &area, state);

	  if ((packing == GTK_PACK_START && !rtl) ||
	      (packing == GTK_PACK_END && rtl))
	    {
	      area.x += area.width;
	      area.x += cellview->priv->spacing;
	    }
	  else
	    area.x -= cellview->priv->spacing;
	}
    }

  return FALSE;
}

static GtkCellViewCellInfo *
gtk_cell_view_get_cell_info (GtkCellView     *cellview,
                             GtkCellRenderer *renderer)
{
  GList *i;

  for (i = cellview->priv->cell_list; i; i = i->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)i->data;

      if (info->cell == renderer)
        return info;
    }

  return NULL;
}

static void
gtk_cell_view_set_cell_data (GtkCellView *cell_view)
{
  GList *i;
  GtkTreeIter iter;
  GtkTreePath *path;

  g_return_if_fail (cell_view->priv->displayed_row != NULL);

  path = gtk_tree_row_reference_get_path (cell_view->priv->displayed_row);
  if (!path)
    return;

  gtk_tree_model_get_iter (cell_view->priv->model, &iter, path);
  gtk_tree_path_free (path);

  for (i = cell_view->priv->cell_list; i; i = i->next)
    {
      GSList *j;
      GtkCellViewCellInfo *info = i->data;

      g_object_freeze_notify (G_OBJECT (info->cell));

      for (j = info->attributes; j && j->next; j = j->next->next)
        {
          gchar *property = j->data;
          gint column = GPOINTER_TO_INT (j->next->data);
          GValue value = {0, };

          gtk_tree_model_get_value (cell_view->priv->model, &iter,
                                    column, &value);
          g_object_set_property (G_OBJECT (info->cell),
                                 property, &value);
          g_value_unset (&value);
        }

      if (info->func)
	(* info->func) (GTK_CELL_LAYOUT (cell_view),
			info->cell,
			cell_view->priv->model,
			&iter,
			info->func_data);

      g_object_thaw_notify (G_OBJECT (info->cell));
    }
}

/* GtkCellLayout implementation */
static void
gtk_cell_view_cell_layout_pack_start (GtkCellLayout   *layout,
                                      GtkCellRenderer *renderer,
                                      gboolean         expand)
{
  GtkCellViewCellInfo *info;
  GtkCellView *cellview = GTK_CELL_VIEW (layout);

  g_return_if_fail (!gtk_cell_view_get_cell_info (cellview, renderer));

  g_object_ref_sink (renderer);

  info = g_slice_new0 (GtkCellViewCellInfo);
  info->cell = renderer;
  info->expand = expand ? TRUE : FALSE;
  info->pack = GTK_PACK_START;

  cellview->priv->cell_list = g_list_append (cellview->priv->cell_list, info);

  gtk_widget_queue_resize (GTK_WIDGET (cellview));
}

static void
gtk_cell_view_cell_layout_pack_end (GtkCellLayout   *layout,
                                    GtkCellRenderer *renderer,
                                    gboolean         expand)
{
  GtkCellViewCellInfo *info;
  GtkCellView *cellview = GTK_CELL_VIEW (layout);

  g_return_if_fail (!gtk_cell_view_get_cell_info (cellview, renderer));

  g_object_ref_sink (renderer);

  info = g_slice_new0 (GtkCellViewCellInfo);
  info->cell = renderer;
  info->expand = expand ? TRUE : FALSE;
  info->pack = GTK_PACK_END;

  cellview->priv->cell_list = g_list_append (cellview->priv->cell_list, info);

  gtk_widget_queue_resize (GTK_WIDGET (cellview));
}

static void
gtk_cell_view_cell_layout_add_attribute (GtkCellLayout   *layout,
                                         GtkCellRenderer *renderer,
                                         const gchar     *attribute,
                                         gint             column)
{
  GtkCellViewCellInfo *info;
  GtkCellView *cellview = GTK_CELL_VIEW (layout);

  info = gtk_cell_view_get_cell_info (cellview, renderer);
  g_return_if_fail (info != NULL);

  info->attributes = g_slist_prepend (info->attributes,
                                      GINT_TO_POINTER (column));
  info->attributes = g_slist_prepend (info->attributes,
                                      g_strdup (attribute));
}

static void
gtk_cell_view_cell_layout_clear (GtkCellLayout *layout)
{
  GtkCellView *cellview = GTK_CELL_VIEW (layout);

  while (cellview->priv->cell_list)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)cellview->priv->cell_list->data;

      gtk_cell_view_cell_layout_clear_attributes (layout, info->cell);
      g_object_unref (info->cell);
      g_slice_free (GtkCellViewCellInfo, info);
      cellview->priv->cell_list = g_list_delete_link (cellview->priv->cell_list, 
						      cellview->priv->cell_list);
    }
}

static void
gtk_cell_view_cell_layout_set_cell_data_func (GtkCellLayout         *layout,
                                              GtkCellRenderer       *cell,
                                              GtkCellLayoutDataFunc  func,
                                              gpointer               func_data,
                                              GDestroyNotify         destroy)
{
  GtkCellView *cellview = GTK_CELL_VIEW (layout);
  GtkCellViewCellInfo *info;

  info = gtk_cell_view_get_cell_info (cellview, cell);
  g_return_if_fail (info != NULL);

  if (info->destroy)
    {
      GDestroyNotify d = info->destroy;

      info->destroy = NULL;
      d (info->func_data);
    }

  info->func = func;
  info->func_data = func_data;
  info->destroy = destroy;
}

static void
gtk_cell_view_cell_layout_clear_attributes (GtkCellLayout   *layout,
                                            GtkCellRenderer *renderer)
{
  GtkCellView *cellview = GTK_CELL_VIEW (layout);
  GtkCellViewCellInfo *info;
  GSList *list;

  info = gtk_cell_view_get_cell_info (cellview, renderer);
  if (info != NULL)
    {
      list = info->attributes;
      while (list && list->next)
	{
	  g_free (list->data);
	  list = list->next->next;
	}
      
      g_slist_free (info->attributes);
      info->attributes = NULL;
    }
}

static void
gtk_cell_view_cell_layout_reorder (GtkCellLayout   *layout,
                                   GtkCellRenderer *cell,
                                   gint             position)
{
  GtkCellView *cellview = GTK_CELL_VIEW (layout);
  GtkCellViewCellInfo *info;
  GList *link;

  info = gtk_cell_view_get_cell_info (cellview, cell);

  g_return_if_fail (info != NULL);
  g_return_if_fail (position >= 0);

  link = g_list_find (cellview->priv->cell_list, info);

  g_return_if_fail (link != NULL);

  cellview->priv->cell_list = g_list_delete_link (cellview->priv->cell_list,
                                                  link);
  cellview->priv->cell_list = g_list_insert (cellview->priv->cell_list,
                                             info, position);

  gtk_widget_queue_draw (GTK_WIDGET (cellview));
}

/**
 * gtk_cell_view_new:
 *
 * Creates a new #GtkCellView widget.
 *
 * Return value: A newly created #GtkCellView widget.
 *
 * Since: 2.6
 */
GtkWidget *
gtk_cell_view_new (void)
{
  GtkCellView *cellview;

  cellview = g_object_new (gtk_cell_view_get_type (), NULL);

  return GTK_WIDGET (cellview);
}

/**
 * gtk_cell_view_new_with_text:
 * @text: the text to display in the cell view
 *
 * Creates a new #GtkCellView widget, adds a #GtkCellRendererText 
 * to it, and makes its show @text.
 *
 * Return value: A newly created #GtkCellView widget.
 *
 * Since: 2.6
 */
GtkWidget *
gtk_cell_view_new_with_text (const gchar *text)
{
  GtkCellView *cellview;
  GtkCellRenderer *renderer;
  GValue value = {0, };

  cellview = GTK_CELL_VIEW (gtk_cell_view_new ());

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_view_cell_layout_pack_start (GTK_CELL_LAYOUT (cellview),
                                        renderer, TRUE);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, text);
  gtk_cell_view_set_value (cellview, renderer, "text", &value);
  g_value_unset (&value);

  return GTK_WIDGET (cellview);
}

/**
 * gtk_cell_view_new_with_markup:
 * @markup: the text to display in the cell view
 *
 * Creates a new #GtkCellView widget, adds a #GtkCellRendererText 
 * to it, and makes it show @markup. The text can be
 * marked up with the <link linkend="PangoMarkupFormat">Pango text 
 * markup language</link>.
 *
 * Return value: A newly created #GtkCellView widget.
 *
 * Since: 2.6
 */
GtkWidget *
gtk_cell_view_new_with_markup (const gchar *markup)
{
  GtkCellView *cellview;
  GtkCellRenderer *renderer;
  GValue value = {0, };

  cellview = GTK_CELL_VIEW (gtk_cell_view_new ());

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_view_cell_layout_pack_start (GTK_CELL_LAYOUT (cellview),
                                        renderer, TRUE);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, markup);
  gtk_cell_view_set_value (cellview, renderer, "markup", &value);
  g_value_unset (&value);

  return GTK_WIDGET (cellview);
}

/**
 * gtk_cell_view_new_with_pixbuf:
 * @pixbuf: the image to display in the cell view
 *
 * Creates a new #GtkCellView widget, adds a #GtkCellRendererPixbuf 
 * to it, and makes its show @pixbuf. 
 *
 * Return value: A newly created #GtkCellView widget.
 *
 * Since: 2.6
 */
GtkWidget *
gtk_cell_view_new_with_pixbuf (GdkPixbuf *pixbuf)
{
  GtkCellView *cellview;
  GtkCellRenderer *renderer;
  GValue value = {0, };

  cellview = GTK_CELL_VIEW (gtk_cell_view_new ());

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_view_cell_layout_pack_start (GTK_CELL_LAYOUT (cellview),
                                        renderer, TRUE);

  g_value_init (&value, GDK_TYPE_PIXBUF);
  g_value_set_object (&value, pixbuf);
  gtk_cell_view_set_value (cellview, renderer, "pixbuf", &value);
  g_value_unset (&value);

  return GTK_WIDGET (cellview);
}

/**
 * gtk_cell_view_set_value:
 * @cell_view: a #GtkCellView widget
 * @renderer: one of the renderers of @cell_view
 * @property: the name of the property of @renderer to set
 * @value: the new value to set the property to
 * 
 * Sets a property of a cell renderer of @cell_view, and
 * makes sure the display of @cell_view is updated.
 *
 * Since: 2.6
 */
static void
gtk_cell_view_set_value (GtkCellView     *cell_view,
                         GtkCellRenderer *renderer,
                         gchar           *property,
                         GValue          *value)
{
  g_object_set_property (G_OBJECT (renderer), property, value);

  /* force resize and redraw */
  gtk_widget_queue_resize (GTK_WIDGET (cell_view));
  gtk_widget_queue_draw (GTK_WIDGET (cell_view));
}

/**
 * gtk_cell_view_set_model:
 * @cell_view: a #GtkCellView
 * @model: (allow-none): a #GtkTreeModel
 *
 * Sets the model for @cell_view.  If @cell_view already has a model
 * set, it will remove it before setting the new model.  If @model is
 * %NULL, then it will unset the old model.
 *
 * Since: 2.6
 */
void
gtk_cell_view_set_model (GtkCellView  *cell_view,
                         GtkTreeModel *model)
{
  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  if (cell_view->priv->model)
    {
      if (cell_view->priv->displayed_row)
        gtk_tree_row_reference_free (cell_view->priv->displayed_row);
      cell_view->priv->displayed_row = NULL;

      g_object_unref (cell_view->priv->model);
      cell_view->priv->model = NULL;
    }

  cell_view->priv->model = model;

  if (cell_view->priv->model)
    g_object_ref (cell_view->priv->model);
}

/**
 * gtk_cell_view_get_model:
 * @cell_view: a #GtkCellView
 *
 * Returns the model for @cell_view. If no model is used %NULL is
 * returned.
 *
 * Returns: (transfer none): a #GtkTreeModel used or %NULL
 *
 * Since: 2.16
 **/
GtkTreeModel *
gtk_cell_view_get_model (GtkCellView *cell_view)
{
  g_return_val_if_fail (GTK_IS_CELL_VIEW (cell_view), NULL);

  return cell_view->priv->model;
}

/**
 * gtk_cell_view_set_displayed_row:
 * @cell_view: a #GtkCellView
 * @path: (allow-none): a #GtkTreePath or %NULL to unset.
 *
 * Sets the row of the model that is currently displayed
 * by the #GtkCellView. If the path is unset, then the
 * contents of the cellview "stick" at their last value;
 * this is not normally a desired result, but may be
 * a needed intermediate state if say, the model for
 * the #GtkCellView becomes temporarily empty.
 *
 * Since: 2.6
 **/
void
gtk_cell_view_set_displayed_row (GtkCellView *cell_view,
                                 GtkTreePath *path)
{
  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));
  g_return_if_fail (GTK_IS_TREE_MODEL (cell_view->priv->model));

  if (cell_view->priv->displayed_row)
    gtk_tree_row_reference_free (cell_view->priv->displayed_row);

  if (path)
    {
      cell_view->priv->displayed_row =
	gtk_tree_row_reference_new (cell_view->priv->model, path);
    }
  else
    cell_view->priv->displayed_row = NULL;

  /* force resize and redraw */
  gtk_widget_queue_resize (GTK_WIDGET (cell_view));
  gtk_widget_queue_draw (GTK_WIDGET (cell_view));
}

/**
 * gtk_cell_view_get_displayed_row:
 * @cell_view: a #GtkCellView
 *
 * Returns a #GtkTreePath referring to the currently 
 * displayed row. If no row is currently displayed, 
 * %NULL is returned.
 *
 * Returns: the currently displayed row or %NULL
 *
 * Since: 2.6
 */
GtkTreePath *
gtk_cell_view_get_displayed_row (GtkCellView *cell_view)
{
  g_return_val_if_fail (GTK_IS_CELL_VIEW (cell_view), NULL);

  if (!cell_view->priv->displayed_row)
    return NULL;

  return gtk_tree_row_reference_get_path (cell_view->priv->displayed_row);
}

/**
 * gtk_cell_view_get_size_of_row:
 * @cell_view: a #GtkCellView
 * @path: a #GtkTreePath 
 * @requisition: return location for the size 
 *
 * Sets @requisition to the size needed by @cell_view to display 
 * the model row pointed to by @path.
 * 
 * Return value: %TRUE
 *
 * Since: 2.6
 * 
 * Deprecated: 3.0: Use gtk_cell_view_get_desired_width_of_row() and
 * gtk_cell_view_get_desired_height_for_width_of_row() instead.
 */
gboolean
gtk_cell_view_get_size_of_row (GtkCellView    *cell_view,
                               GtkTreePath    *path,
                               GtkRequisition *requisition)
{
  GtkRequisition req;

  g_return_val_if_fail (GTK_IS_CELL_VIEW (cell_view), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  /* Return the minimum height for the minimum width */
  gtk_cell_view_get_desired_width_of_row (cell_view, path, &req.width, NULL);
  gtk_cell_view_get_desired_height_for_width_of_row (cell_view, path, req.width, &req.height, NULL);

  if (requisition)
    *requisition = req;

  return TRUE;
}


/**
 * gtk_cell_view_get_desired_width_of_row:
 * @cell_view: a #GtkCellView
 * @path: a #GtkTreePath 
 * @minimum_size: location to store the minimum size 
 * @natural_size: location to store the natural size 
 *
 * Sets @minimum_size and @natural_size to the width desired by @cell_view 
 * to display the model row pointed to by @path.
 * 
 * Since: 3.0
 */
void
gtk_cell_view_get_desired_width_of_row (GtkCellView     *cell_view,
					GtkTreePath     *path,
					gint            *minimum_size,
					gint            *natural_size)
{
  GtkTreeRowReference *tmp;

  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));
  g_return_if_fail (path != NULL);
  g_return_if_fail (minimum_size != NULL || natural_size != NULL);

  tmp = cell_view->priv->displayed_row;
  cell_view->priv->displayed_row =
    gtk_tree_row_reference_new (cell_view->priv->model, path);

  gtk_cell_view_get_preferred_width (GTK_WIDGET (cell_view), minimum_size, natural_size);

  gtk_tree_row_reference_free (cell_view->priv->displayed_row);
  cell_view->priv->displayed_row = tmp;

  /* Restore active size (this will restore the cellrenderer info->width/requested_width's) */
  gtk_cell_view_get_preferred_width (GTK_WIDGET (cell_view), NULL, NULL);
}


/**
 * gtk_cell_view_get_desired_height_for_width_of_row:
 * @cell_view: a #GtkCellView
 * @path: a #GtkTreePath 
 * @avail_size: available width
 * @minimum_size: location to store the minimum height 
 * @natural_size: location to store the natural height
 *
 * Sets @minimum_size and @natural_size to the height desired by @cell_view 
 * if it were allocated a width of @avail_size to display the model row 
 * pointed to by @path.
 * 
 * Since: 3.0
 */
void
gtk_cell_view_get_desired_height_for_width_of_row (GtkCellView     *cell_view,
						   GtkTreePath     *path,
						   gint             avail_size,
						   gint            *minimum_size,
						   gint            *natural_size)
{
  GtkTreeRowReference *tmp;

  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));
  g_return_if_fail (path != NULL);
  g_return_if_fail (minimum_size != NULL || natural_size != NULL);

  tmp = cell_view->priv->displayed_row;
  cell_view->priv->displayed_row =
    gtk_tree_row_reference_new (cell_view->priv->model, path);

  /* Then get the collective height_for_width based on the cached values */
  gtk_cell_view_get_preferred_height_for_width (GTK_WIDGET (cell_view), avail_size, minimum_size, natural_size);

  gtk_tree_row_reference_free (cell_view->priv->displayed_row);
  cell_view->priv->displayed_row = tmp;

  /* Restore active size (this will restore the cellrenderer info->width/requested_width's) */
  gtk_cell_view_get_preferred_width (GTK_WIDGET (cell_view), NULL, NULL);
}

/**
 * gtk_cell_view_set_background_color:
 * @cell_view: a #GtkCellView
 * @color: the new background color
 *
 * Sets the background color of @view.
 *
 * Since: 2.6
 */
void
gtk_cell_view_set_background_color (GtkCellView    *cell_view,
                                    const GdkColor *color)
{
  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));

  if (color)
    {
      if (!cell_view->priv->background_set)
        {
          cell_view->priv->background_set = TRUE;
          g_object_notify (G_OBJECT (cell_view), "background-set");
        }

      cell_view->priv->background.red = color->red / 65535.;
      cell_view->priv->background.green = color->green / 65535.;
      cell_view->priv->background.blue = color->blue / 65535.;
      cell_view->priv->background.alpha = 1;
    }
  else
    {
      if (cell_view->priv->background_set)
        {
          cell_view->priv->background_set = FALSE;
          g_object_notify (G_OBJECT (cell_view), "background-set");
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (cell_view));
}

/**
 * gtk_cell_view_set_background_rgba:
 * @cell_view: a #GtkCellView
 * @rgba: the new background color
 *
 * Sets the background color of @cell_view.
 *
 * Since: 3.0
 */
void
gtk_cell_view_set_background_rgba (GtkCellView   *cell_view,
                                   const GdkRGBA *rgba)
{
  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));

  if (rgba)
    {
      if (!cell_view->priv->background_set)
        {
          cell_view->priv->background_set = TRUE;
          g_object_notify (G_OBJECT (cell_view), "background-set");
        }

      cell_view->priv->background = *rgba;
    }
  else
    {
      if (cell_view->priv->background_set)
        {
          cell_view->priv->background_set = FALSE;
          g_object_notify (G_OBJECT (cell_view), "background-set");
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (cell_view));
}

static GList *
gtk_cell_view_cell_layout_get_cells (GtkCellLayout *layout)
{
  GtkCellView *cell_view = GTK_CELL_VIEW (layout);
  GList *retval = NULL, *list;

  g_return_val_if_fail (cell_view != NULL, NULL);

  gtk_cell_view_set_cell_data (cell_view);

  for (list = cell_view->priv->cell_list; list; list = list->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)list->data;

      retval = g_list_prepend (retval, info->cell);
    }

  return g_list_reverse (retval);
}

static gboolean
gtk_cell_view_buildable_custom_tag_start (GtkBuildable  *buildable,
					  GtkBuilder    *builder,
					  GObject       *child,
					  const gchar   *tagname,
					  GMarkupParser *parser,
					  gpointer      *data)
{
  if (parent_buildable_iface->custom_tag_start &&
      parent_buildable_iface->custom_tag_start (buildable, builder, child,
						tagname, parser, data))
    return TRUE;

  return _gtk_cell_layout_buildable_custom_tag_start (buildable, builder, child,
						      tagname, parser, data);
}

static void
gtk_cell_view_buildable_custom_tag_end (GtkBuildable *buildable,
					GtkBuilder   *builder,
					GObject      *child,
					const gchar  *tagname,
					gpointer     *data)
{
  if (strcmp (tagname, "attributes") == 0)
    _gtk_cell_layout_buildable_custom_tag_end (buildable, builder, child, tagname,
					       data);
  else if (parent_buildable_iface->custom_tag_end)
    parent_buildable_iface->custom_tag_end (buildable, builder, child, tagname,
					    data);
}

static void
gtk_cell_view_get_preferred_width  (GtkWidget *widget,
                                    gint      *minimum_size,
                                    gint      *natural_size)
{
  GList *list;
  gint cell_min, cell_nat;
  gboolean first_cell = TRUE;
  GtkCellView *cellview = GTK_CELL_VIEW (widget);
  gint minimum, natural;

  minimum = natural = 0;

  if (cellview->priv->displayed_row)
    gtk_cell_view_set_cell_data (cellview);

  for (list = cellview->priv->cell_list; list; list = list->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)list->data;

      if (gtk_cell_renderer_get_visible (info->cell))
        {
	  
	  if (!first_cell)
	    {
	      minimum += cellview->priv->spacing;
	      natural += cellview->priv->spacing;
	    }

	  gtk_cell_renderer_get_preferred_width (info->cell,
                                                 GTK_WIDGET (cellview), &cell_min, &cell_nat);
	  
	  info->requested_width = cell_min;
	  info->natural_width   = cell_nat;
	  
	  minimum += info->requested_width;
	  natural += info->natural_width;

	  first_cell = FALSE;
        }
    }

  if (minimum_size)
    *minimum_size = minimum;

  if (natural_size)
    *natural_size = natural;
}

static void       
gtk_cell_view_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum_size,
                                    gint      *natural_size)
{
  gint minimum_width;

  /* CellViews only need to respond to height-for-width mode (cellview is pretty much
   * an implementation detail of GtkComboBox) */
  gtk_cell_view_get_preferred_width (widget, &minimum_width, NULL);
  gtk_cell_view_get_preferred_height_for_width (widget, minimum_width, minimum_size, natural_size);
}

static void       
gtk_cell_view_get_preferred_width_for_height (GtkWidget *widget,
                                              gint       for_size,
                                              gint      *minimum_size,
                                              gint      *natural_size)
{
  /* CellViews only need to respond to height-for-width mode (cellview is pretty much
   * an implementation detail of GtkComboBox) */
  gtk_cell_view_get_preferred_width (widget, minimum_size, natural_size);
}

static void       
gtk_cell_view_get_preferred_height_for_width (GtkWidget *widget,
                                              gint       for_size,
                                              gint      *minimum_size,
                                              gint      *natural_size)
{
  GtkCellView      *cellview = GTK_CELL_VIEW (widget);
  GList            *list;
  GtkRequestedSize *sizes;
  GArray           *array;
  gint              minimum, natural, avail_size;
  gboolean          first_cell = TRUE;
  gint              n_expand_cells = 0;
  gint              extra_per_cell, extra_extra, i;

  minimum = natural = 0;
  avail_size = for_size;

  array = g_array_new (0, TRUE, sizeof (GtkRequestedSize));

  if (cellview->priv->displayed_row)
    gtk_cell_view_set_cell_data (cellview);

  /* First allocate the right width to all cells */
  for (list = cellview->priv->cell_list; list; list = list->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)list->data;

      if (gtk_cell_renderer_get_visible (info->cell))
        {
	  GtkRequestedSize requested;

	  gtk_cell_renderer_get_preferred_width (GTK_CELL_RENDERER (info->cell),
                                                 GTK_WIDGET (cellview), 
                                                 &requested.minimum_size, 
                                                 &requested.natural_size);

	  requested.data = info;
	  g_array_append_val (array, requested);

	  avail_size -= requested.minimum_size;

	  if (!first_cell)
	    avail_size -= cellview->priv->spacing;

	  first_cell = FALSE;

	  if (info->expand)
	    n_expand_cells++;
        }
    }

  sizes      = (GtkRequestedSize *)array->data;
  avail_size = gtk_distribute_natural_allocation (MAX (0, avail_size), array->len, sizes);

  /* Deal with any expand space... */
  if (n_expand_cells > 0)
    {
      extra_per_cell = avail_size / n_expand_cells;
      extra_extra    = avail_size % n_expand_cells;
    }
  else
    /* Everything just left-aligned if no cells expand */
    extra_per_cell = extra_extra = 0;

  /* Now get the height for the real width of each cell */
  for (i = 0, list = cellview->priv->cell_list; list; list = list->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)list->data;
      gint cell_minimum, cell_natural;

      if (gtk_cell_renderer_get_visible (info->cell))
        {
	  gint cell_width = sizes[i].minimum_size;

	  g_assert (sizes[i].data == info);

	  if (info->expand)
	    {
	      cell_width += extra_per_cell;
	      if (extra_extra)
		{
		  cell_width++;
		  extra_extra--;
		}
	    }

	  /* Get the height for the real width of this cell */
	  gtk_cell_renderer_get_preferred_height_for_width (GTK_CELL_RENDERER (info->cell),
                                                            GTK_WIDGET (widget),
                                                            cell_width, &cell_minimum, &cell_natural);

	  minimum = MAX (minimum, cell_minimum);
	  natural = MAX (natural, cell_natural);

	  /* increment sizes[] index for visible cells */
	  i++;
        }
    }

  g_array_free (array, TRUE);

  if (minimum_size)
    *minimum_size = minimum;
  if (natural_size)
    *natural_size = natural;
}
