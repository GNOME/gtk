/* gtkcellrenderertext.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

#include <stdlib.h>
#include "gtkcellrenderertext.h"
#include "gtkintl.h"

static void gtk_cell_renderer_text_init       (GtkCellRendererText      *celltext);
static void gtk_cell_renderer_text_class_init (GtkCellRendererTextClass *class);

static void gtk_cell_renderer_text_get_property  (GObject                  *object,
						  guint                     param_id,
						  GValue                   *value,
						  GParamSpec               *pspec,
						  const gchar              *trailer);
static void gtk_cell_renderer_text_set_property  (GObject                  *object,
						  guint                     param_id,
						  const GValue             *value,
						  GParamSpec               *pspec,
						  const gchar              *trailer);
static void gtk_cell_renderer_text_get_size   (GtkCellRenderer          *cell,
					       GtkWidget                *widget,
					       gint                     *width,
					       gint                     *height);
static void gtk_cell_renderer_text_render     (GtkCellRenderer          *cell,
					       GdkWindow                *window,
					       GtkWidget                *widget,
					       GdkRectangle             *background_area,
					       GdkRectangle             *cell_area,
					       GdkRectangle             *expose_area,
					       guint                     flags);


enum {
  PROP_ZERO,
  PROP_TEXT,
  PROP_FONT,
  PROP_BACKGROUND,
  PROP_BACKGROUND_GDK,
  PROP_FOREGROUND,
  PROP_FOREGROUND_GDK,
  PROP_STRIKETHROUGH,
  PROP_UNDERLINE,
  PROP_EDITABLE,
  PROP_ITALIC,
  PROP_BOLD
};

GtkType
gtk_cell_renderer_text_get_type (void)
{
  static GtkType cell_text_type = 0;

  if (!cell_text_type)
    {
      static const GTypeInfo cell_text_info =
      {
        sizeof (GtkCellRendererTextClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_cell_renderer_text_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkCellRendererText),
	0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_cell_renderer_text_init,
      };

      cell_text_type = g_type_register_static (GTK_TYPE_CELL_RENDERER, "GtkCellRendererText", &cell_text_info, 0);
    }

  return cell_text_type;
}

static void
gtk_cell_renderer_text_init (GtkCellRendererText *celltext)
{
  celltext->attr_list = pango_attr_list_new ();
  GTK_CELL_RENDERER (celltext)->xalign = 0.0;
  GTK_CELL_RENDERER (celltext)->yalign = 0.5;
  GTK_CELL_RENDERER (celltext)->xpad = 2;
  GTK_CELL_RENDERER (celltext)->ypad = 2;
  celltext->underline = FALSE;
}

static void
gtk_cell_renderer_text_class_init (GtkCellRendererTextClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  object_class->get_property = gtk_cell_renderer_text_get_property;
  object_class->set_property = gtk_cell_renderer_text_set_property;

  cell_class->get_size = gtk_cell_renderer_text_get_size;
  cell_class->render = gtk_cell_renderer_text_render;
  
  g_object_class_install_property (object_class,
				   PROP_TEXT,
				   g_param_spec_string ("text",
							_("Text String"),
							_("The text of the renderer."),
							"",
							G_PARAM_READABLE |
							G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_FONT,
				   g_param_spec_string ("font",
							_("Font String"),
							_("The string of the font."),
							"",
							G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_BACKGROUND,
				   g_param_spec_string ("background",
							_("Background Color string"),
							_("The color for the background of the text."),
							"white",
							G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_FOREGROUND,
				   g_param_spec_string ("foreground",
							_("Foreground Color string"),
							_("The color for the background of the text."),
							"black",
							G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_STRIKETHROUGH,
				   g_param_spec_boolean ("strikethrough",
							 _("Strikethrough"),
							 _("Draw a line through the text."),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_UNDERLINE,
				   g_param_spec_boolean ("underline",
							 _("Underline"),
							 _("Underline the text."),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_EDITABLE,
				   g_param_spec_boolean ("editable",
							 _("Editable"),
							 _("Make the text editable."),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_ITALIC,
				   g_param_spec_boolean ("italic",
							 _("Italic"),
							 _("Make the text italic."),
							 FALSE,
							 G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_BOLD,
				   g_param_spec_boolean ("bold",
							 _("Bold"),
							 _("Make the text bold."),
							 FALSE,
							 G_PARAM_WRITABLE));
}

static void
gtk_cell_renderer_text_get_property (GObject        *object,
				     guint           param_id,
				     GValue         *value,
				     GParamSpec     *pspec,
				     const gchar    *trailer)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (object);
  PangoAttrIterator *attr_iter;
  PangoAttribute *attr;

  switch (param_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, celltext->text);
      break;
    case PROP_STRIKETHROUGH:
      attr_iter = pango_attr_list_get_iterator (celltext->attr_list);
      attr = pango_attr_iterator_get (attr_iter,
				      PANGO_ATTR_STRIKETHROUGH);
      g_value_set_boolean (value, ((PangoAttrInt*) attr)->value);
      break;
    case PROP_UNDERLINE:
      g_value_set_boolean (value, celltext->underline);
      break;
    case PROP_EDITABLE:
      g_value_set_boolean (value, celltext->editable);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
gtk_cell_renderer_text_set_property (GObject      *object,
				     guint         param_id,
				     const GValue *value,
				     GParamSpec   *pspec,
				     const gchar  *trailer)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (object);

  GdkColor color;
  PangoFontDescription *font_desc;
  gchar *string;
  PangoAttribute *attribute;
  gchar *font;

  switch (param_id)
    {
    case PROP_TEXT:
      g_free (celltext->text);
      celltext->text = g_value_dup_string (value);
      break;
    case PROP_FONT:
      font = g_value_get_string (value);

      if (font)
	{
	  font_desc = pango_font_description_from_string (font);
	  attribute = pango_attr_font_desc_new (font_desc);
	  attribute->start_index = 0;
	  attribute->end_index = G_MAXINT;
	  pango_font_description_free (font_desc);
	  pango_attr_list_change (celltext->attr_list,
				  attribute);
	}
      break;
    case PROP_BACKGROUND:
      string = g_value_get_string (value);
      if (string && gdk_color_parse (string, &color))
	{
	  attribute = pango_attr_background_new (color.red,
						 color.green,
						 color.blue);
	  attribute->start_index = 0;
	  attribute->end_index = G_MAXINT;
	  pango_attr_list_change (celltext->attr_list,
				  attribute);
	}
      break;
    case PROP_BACKGROUND_GDK:
      break;
    case PROP_FOREGROUND:
      string = g_value_get_string (value);
      if (string && gdk_color_parse (string, &color))
	{
	  attribute = pango_attr_foreground_new (color.red,
						 color.green,
						 color.blue);
	  attribute->start_index = 0;
	  attribute->end_index = G_MAXINT;
	  pango_attr_list_change (celltext->attr_list,
				  attribute);
	}
      break;
    case PROP_FOREGROUND_GDK:
      break;
    case PROP_STRIKETHROUGH:
      attribute = pango_attr_strikethrough_new (g_value_get_boolean (value));
      attribute->start_index = 0;
      attribute->end_index = G_MAXINT;
      pango_attr_list_change (celltext->attr_list,
			      attribute);
      break;
    case PROP_UNDERLINE:
      celltext->underline = g_value_get_boolean (value);
      attribute = pango_attr_underline_new (celltext->underline ?
					    PANGO_UNDERLINE_SINGLE :
					    PANGO_UNDERLINE_NONE);
      attribute->start_index = 0;
      attribute->end_index = G_MAXINT;
      pango_attr_list_change (celltext->attr_list,
			      attribute);
      break;
    case PROP_EDITABLE:
      break;
    case PROP_ITALIC:
      attribute = pango_attr_style_new (g_value_get_boolean (value) ?
					PANGO_STYLE_ITALIC :
					PANGO_STYLE_NORMAL);
      attribute->start_index = 0;
      attribute->end_index = G_MAXINT;
      pango_attr_list_change (celltext->attr_list,
			      attribute);
      break;
    case PROP_BOLD:
      attribute = pango_attr_weight_new (g_value_get_boolean (value) ?
					 PANGO_WEIGHT_BOLD :
					 PANGO_WEIGHT_NORMAL);
      attribute->start_index = 0;
      attribute->end_index = G_MAXINT;
      pango_attr_list_change (celltext->attr_list,
			      attribute);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

GtkCellRenderer *
gtk_cell_renderer_text_new (void)
{
  return GTK_CELL_RENDERER (gtk_type_new (gtk_cell_renderer_text_get_type ()));
}

static void
gtk_cell_renderer_text_get_size (GtkCellRenderer *cell,
				 GtkWidget       *widget,
				 gint            *width,
				 gint            *height)
{
  GtkCellRendererText *celltext = (GtkCellRendererText *)cell;
  PangoRectangle rect;
  PangoLayout *layout;
  PangoAttribute *attr;
  PangoUnderline underline;

  layout = gtk_widget_create_pango_layout (widget, celltext->text);
  underline = celltext->underline ?
    PANGO_UNDERLINE_DOUBLE :
    PANGO_UNDERLINE_NONE;

  attr = pango_attr_underline_new (underline);
  attr->start_index = 0;
  attr->end_index = G_MAXINT;

  pango_attr_list_change (celltext->attr_list, attr);
  pango_layout_set_attributes (layout, celltext->attr_list);
  pango_layout_set_width (layout, -1);
  pango_layout_get_pixel_extents (layout, NULL, &rect);

  if (width)
    *width = GTK_CELL_RENDERER (celltext)->xpad * 2 + rect.width;

  if (height)
    *height = GTK_CELL_RENDERER (celltext)->ypad * 2 + rect.height;

  g_object_unref (G_OBJECT (layout));
}

static void
gtk_cell_renderer_text_render (GtkCellRenderer    *cell,
			       GdkWindow          *window,
			       GtkWidget          *widget,
			       GdkRectangle       *background_area,
			       GdkRectangle       *cell_area,
			       GdkRectangle       *expose_area,
			       guint               flags)

{
  GtkCellRendererText *celltext = (GtkCellRendererText *) cell;
  PangoRectangle rect;
  PangoLayout *layout;
  PangoAttribute *attr;
  PangoUnderline underline;

  gint real_xoffset;
  gint real_yoffset;

  GdkGC *font_gc = NULL;

  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
    font_gc = widget->style->fg_gc [GTK_STATE_SELECTED];
  else
    font_gc = widget->style->fg_gc [GTK_STATE_NORMAL];

  layout = gtk_widget_create_pango_layout (widget, celltext->text);

  if (celltext->underline)
    underline = PANGO_UNDERLINE_SINGLE;
  else
    underline = PANGO_UNDERLINE_NONE;

  attr = pango_attr_underline_new (underline);
  attr->start_index = 0;
  attr->end_index = G_MAXINT;
  pango_attr_list_change (celltext->attr_list, attr);
  pango_layout_set_attributes (layout, celltext->attr_list);

  pango_layout_set_width (layout, -1);
  pango_layout_get_pixel_extents (layout, NULL, &rect);

  if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT)
    underline = (celltext->underline) ? PANGO_UNDERLINE_DOUBLE:PANGO_UNDERLINE_SINGLE;
  else
    underline = (celltext->underline) ? PANGO_UNDERLINE_SINGLE:PANGO_UNDERLINE_NONE;

  attr = pango_attr_underline_new (underline);
  attr->start_index = 0;
  attr->end_index = G_MAXINT;
  pango_attr_list_change (celltext->attr_list, attr);
  pango_layout_set_attributes (layout, celltext->attr_list);

  gdk_gc_set_clip_rectangle (font_gc, cell_area);

  real_xoffset = cell->xalign * (cell_area->width - rect.width - (2 * cell->xpad));
  real_xoffset = MAX (real_xoffset, 0) + cell->xpad;
  real_yoffset = cell->yalign * (cell_area->height - rect.height - (2 * cell->ypad));
  real_yoffset = MAX (real_yoffset, 0) + cell->ypad;

  gdk_draw_layout (window,
		   font_gc,
		   cell_area->x + real_xoffset,
		   cell_area->y + real_yoffset,
		   layout);

  g_object_unref (G_OBJECT (layout));

  gdk_gc_set_clip_rectangle (font_gc, NULL);
}
