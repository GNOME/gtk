/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <ctype.h>
#include <string.h>
#include "gdk/gdkkeysyms.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtktext.h"
#include "line-wrap.xbm"
#include "line-arrow.xbm"


#define INITIAL_BUFFER_SIZE      1024
#define INITIAL_LINE_CACHE_SIZE  256
#define MIN_GAP_SIZE             256
#define LINE_DELIM               '\n'
#define MIN_TEXT_WIDTH_LINES     20
#define MIN_TEXT_HEIGHT_LINES    10
#define TEXT_BORDER_ROOM         3
#define LINE_WRAP_ROOM           8           /* The bitmaps are 6 wide. */
#define DEFAULT_TAB_STOP_WIDTH   4
#define SCROLL_PIXELS            5
#define KEY_SCROLL_PIXELS        10

#define SET_PROPERTY_MARK(m, p, o)  do {                   \
                                      (m)->property = (p); \
			              (m)->offset = (o);   \
			            } while (0)
#define MARK_CURRENT_PROPERTY(mark) ((TextProperty*)(mark)->property->data)
#define MARK_NEXT_PROPERTY(mark)    ((TextProperty*)(mark)->property->next->data)
#define MARK_PREV_PROPERTY(mark)    ((TextProperty*)((mark)->property->prev ?     \
						     (mark)->property->prev->data \
						     : NULL))
#define MARK_PREV_LIST_PTR(mark)    ((mark)->property->prev)
#define MARK_LIST_PTR(mark)         ((mark)->property)
#define MARK_NEXT_LIST_PTR(mark)    ((mark)->property->next)
#define MARK_OFFSET(mark)           ((mark)->offset)
#define MARK_PROPERTY_LENGTH(mark)  (MARK_CURRENT_PROPERTY(mark)->length)
#define MARK_CURRENT_FONT(mark)     (((TextProperty*)(mark)->property->data)->font->gdk_font)
#define MARK_CURRENT_FORE(mark)     (&((TextProperty*)(mark)->property->data)->fore_color)
#define MARK_CURRENT_BACK(mark)     (&((TextProperty*)(mark)->property->data)->back_color)
#define MARK_CURRENT_TEXT_FONT(m)   (((TextProperty*)(m)->property->data)->font)
#define TEXT_INDEX(t, index)        ((index) < (t)->gap_position ? (t)->text[index] : \
				     (t)->text[(index) + (t)->gap_size])
#define TEXT_LENGTH(t)              ((t)->text_end - (t)->gap_size)
#define FONT_HEIGHT(f)              ((f)->ascent + (f)->descent)
#define LINE_HEIGHT(l)              ((l).font_ascent + (l).font_descent)
#define LINE_CONTAINS(l, i)         ((l).start.index <= (i) && (l).end.index >= (i))
#define LINE_STARTS_AT(l, i)        ((l).start.index == (i))
#define LINE_START_PIXEL(l)         ((l).tab_cont.pixel_offset)
#define LAST_INDEX(t, m)            ((m).index == TEXT_LENGTH(t))
#define CACHE_DATA(c)               (*(LineParams*)(c)->data)


typedef struct _TextFont              TextFont;
typedef struct _TextProperty          TextProperty;
typedef struct _TabStopMark           TabStopMark;
typedef struct _PrevTabCont           PrevTabCont;
typedef struct _FetchLinesData        FetchLinesData;
typedef struct _LineParams            LineParams;
typedef struct _SetVerticalScrollData SetVerticalScrollData;

typedef gint (*LineIteratorFunction) (GtkText* text, LineParams* lp, void* data);

typedef enum
{
  FetchLinesPixels,
  FetchLinesCount
} FLType;

struct _SetVerticalScrollData {
  gint pixel_height;
  gint last_didnt_wrap;
  gint last_line_start;
  GtkPropertyMark mark;
};

struct _TextFont
{
  /* The actual font. */
  GdkFont *gdk_font;

  gint16 char_widths[256];
};

struct _TextProperty
{
  /* Font. */
  TextFont* font;

  /* Background Color. */
  GdkColor back_color;

  /* Foreground Color. */
  GdkColor fore_color;

  /* Length of this property. */
  guint length;
};

struct _TabStopMark
{
  GList* tab_stops; /* Index into list containing the next tab position.  If
		     * NULL, using default widths. */
  gint to_next_tab;
};

struct _PrevTabCont
{
  guint pixel_offset;
  TabStopMark tab_start;
};

struct _FetchLinesData
{
  GList* new_lines;
  FLType fl_type;
  gint data;
  gint data_max;
};

struct _LineParams
{
  guint font_ascent;
  guint font_descent;
  guint pixel_width;
  guint displayable_chars;
  guint wraps : 1;

  PrevTabCont tab_cont;
  PrevTabCont tab_cont_next;

  GtkPropertyMark start;
  GtkPropertyMark end;
};


static void  gtk_text_class_init     (GtkTextClass   *klass);
static void  gtk_text_init           (GtkText        *text);
static void  gtk_text_finalize       (GtkObject      *object);
static void  gtk_text_realize        (GtkWidget      *widget);
static void  gtk_text_unrealize      (GtkWidget      *widget);
static void  gtk_text_draw_focus     (GtkWidget      *widget);
static void  gtk_text_size_request   (GtkWidget      *widget,
				      GtkRequisition *requisition);
static void  gtk_text_size_allocate  (GtkWidget      *widget,
				      GtkAllocation  *allocation);
static void  gtk_text_adjustment     (GtkAdjustment  *adjustment,
				      GtkText        *text);
static void  gtk_text_disconnect     (GtkAdjustment  *adjustment,
				      GtkText        *text);

/* Event handlers */
static void  gtk_text_draw              (GtkWidget         *widget,
					 GdkRectangle      *area);
static gint  gtk_text_expose            (GtkWidget         *widget,
					 GdkEventExpose    *event);
static gint  gtk_text_button_press      (GtkWidget         *widget,
					 GdkEventButton    *event);
static gint  gtk_text_button_release    (GtkWidget         *widget,
					 GdkEventButton    *event);
static gint  gtk_text_motion_notify     (GtkWidget         *widget,
					 GdkEventMotion    *event);
static gint  gtk_text_key_press         (GtkWidget         *widget,
					 GdkEventKey       *event);
static gint  gtk_text_focus_in          (GtkWidget         *widget,
					 GdkEventFocus     *event);
static gint  gtk_text_focus_out         (GtkWidget         *widget,
				         GdkEventFocus     *event);
static gint  gtk_text_selection_clear   (GtkWidget         *widget,
					 GdkEventSelection *event);
static gint  gtk_text_selection_request (GtkWidget         *widget,
					 GdkEventSelection *event);
static gint  gtk_text_selection_notify  (GtkWidget         *widget,
					 GdkEventSelection *event);

static void move_gap_to_point (GtkText* text);
static void make_forward_space (GtkText* text, guint len);
static void insert_text_property (GtkText* text, GdkFont* font,
				  GdkColor *fore, GdkColor* back, guint len);
static void delete_text_property (GtkText* text, guint len);
static void init_properties (GtkText *text);
static guint pixel_height_of (GtkText* text, GList* cache_line);

/* Property Movement and Size Computations */
static void advance_mark (GtkPropertyMark* mark);
static void decrement_mark (GtkPropertyMark* mark);
static void advance_mark_n (GtkPropertyMark* mark, gint n);
static void decrement_mark_n (GtkPropertyMark* mark, gint n);
static void move_mark_n (GtkPropertyMark* mark, gint n);
static GtkPropertyMark find_mark (GtkText* text, guint mark_position);
static GtkPropertyMark find_mark_near (GtkText* text, guint mark_position, const GtkPropertyMark* near);
static void find_line_containing_point (GtkText* text, guint point);
static TextProperty* new_text_property (GdkFont* font, GdkColor* fore, GdkColor* back, guint length);

/* Display */
static gint total_line_height (GtkText* text,
			       GList* line,
			       gint line_count);
static LineParams find_line_params (GtkText* text,
				    const GtkPropertyMark *mark,
				    const PrevTabCont *tab_cont,
				    PrevTabCont *next_cont);
static void recompute_geometry (GtkText* text);
static void insert_char_line_expose (GtkText* text, gchar key, guint old_pixels);
static void delete_char_line_expose (GtkText* text, gchar key, guint old_pixels);
static void clear_area (GtkText *text, GdkRectangle *area);
static void draw_line (GtkText* text,
		       gint pixel_height,
		       LineParams* lp);
static void draw_line_wrap (GtkText* text,
			    guint height);
static void draw_cursor (GtkText* text, gint absolute);
static void undraw_cursor (GtkText* text, gint absolute);
static gint drawn_cursor_min (GtkText* text);
static gint drawn_cursor_max (GtkText* text);
static void expose_text (GtkText* text, GdkRectangle *area, gboolean cursor);

/* Search and Placement. */
static void find_cursor (GtkText* text);
static void find_cursor_at_line (GtkText* text,
				 const LineParams* start_line,
				 gint pixel_height);
static void mouse_click_1 (GtkText* text, GdkEventButton *event);

/* Scrolling. */
static void adjust_adj  (GtkText* text, GtkAdjustment* adj);
static void scroll_up   (GtkText* text, gint diff);
static void scroll_down (GtkText* text, gint diff);
static void scroll_int  (GtkText* text, gint diff);

static void process_exposes (GtkText *text);

/* Cache Management. */
static GList* remove_cache_line (GtkText* text, GList* list);

/* Key Motion. */
static void move_cursor_buffer_ver (GtkText *text, int dir);
static void move_cursor_page_ver (GtkText *text, int dir);
static void move_cursor_ver (GtkText *text, int count);
static void move_cursor_hor (GtkText *text, int count);

/* #define DEBUG_GTK_TEXT */

#if defined(DEBUG_GTK_TEXT) && defined(__GNUC__)
/* Debugging utilities. */
static void gtk_text_assert_mark (GtkText         *text,
				  GtkPropertyMark *mark,
				  GtkPropertyMark *before,
				  GtkPropertyMark *after,
				  const gchar     *msg,
				  const gchar     *where,
				  gint             line);

static void gtk_text_assert (GtkText         *text,
			     const gchar     *msg,
			     gint             line);
static void gtk_text_show_cache_line (GtkText *text, GList *cache,
				      const char* what, const char* func, gint line);
static void gtk_text_show_cache (GtkText *text, const char* func, gint line);
static void gtk_text_show_adj (GtkText *text,
			       GtkAdjustment *adj,
			       const char* what,
			       const char* func,
			       gint line);
static void gtk_text_show_props (GtkText* test,
				 const char* func,
				 int line);

#define TDEBUG(args) g_print args
#define TEXT_ASSERT(text) gtk_text_assert (text,__PRETTY_FUNCTION__,__LINE__)
#define TEXT_ASSERT_MARK(text,mark,msg) gtk_text_assert_mark (text,mark, \
					   __PRETTY_FUNCTION__,msg,__LINE__)
#define TEXT_SHOW(text) gtk_text_show_cache (text, __PRETTY_FUNCTION__,__LINE__)
#define TEXT_SHOW_LINE(text,line,msg) gtk_text_show_cache_line (text,line,msg,\
					   __PRETTY_FUNCTION__,__LINE__)
#define TEXT_SHOW_ADJ(text,adj,msg) gtk_text_show_adj (text,adj,msg, \
					  __PRETTY_FUNCTION__,__LINE__)
#else
#define TDEBUG(args)
#define TEXT_ASSERT(text)
#define TEXT_ASSERT_MARK(text,mark,msg)
#define TEXT_SHOW(text)
#define TEXT_SHOW_LINE(text,line,msg)
#define TEXT_SHOW_ADJ(text,adj,msg)
#endif

/* Memory Management. */
static GMemChunk  *params_mem_chunk    = NULL;
static GMemChunk  *text_property_chunk = NULL;

static GtkWidgetClass *parent_class = NULL;


/**********************************************************************/
/*			        Widget Crap                           */
/**********************************************************************/

guint
gtk_text_get_type ()
{
  static guint text_type = 0;

  if (!text_type)
    {
      GtkTypeInfo text_info =
      {
	"GtkText",
	sizeof (GtkText),
	sizeof (GtkTextClass),
	(GtkClassInitFunc) gtk_text_class_init,
	(GtkObjectInitFunc) gtk_text_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      text_type = gtk_type_unique (gtk_widget_get_type (), &text_info);
    }

  return text_type;
}

static void
gtk_text_class_init (GtkTextClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class (gtk_widget_get_type ());

  object_class->finalize = gtk_text_finalize;

  widget_class->realize = gtk_text_realize;
  widget_class->unrealize = gtk_text_unrealize;
  widget_class->draw_focus = gtk_text_draw_focus;
  widget_class->size_request = gtk_text_size_request;
  widget_class->size_allocate = gtk_text_size_allocate;
  widget_class->draw = gtk_text_draw;
  widget_class->expose_event = gtk_text_expose;
  widget_class->button_press_event = gtk_text_button_press;
  widget_class->button_release_event = gtk_text_button_release;
  widget_class->motion_notify_event = gtk_text_motion_notify;
  widget_class->key_press_event = gtk_text_key_press;
  widget_class->focus_in_event = gtk_text_focus_in;
  widget_class->focus_out_event = gtk_text_focus_out;
  widget_class->selection_clear_event = gtk_text_selection_clear;
  widget_class->selection_request_event = gtk_text_selection_request;
  widget_class->selection_notify_event = gtk_text_selection_notify;
}

static void
gtk_text_init (GtkText *text)
{
  GTK_WIDGET_SET_FLAGS (text, GTK_CAN_FOCUS);

  text->text = g_new (guchar, INITIAL_BUFFER_SIZE);
  text->text_len = INITIAL_BUFFER_SIZE;

  if (!params_mem_chunk)
    params_mem_chunk = g_mem_chunk_new ("LineParams",
					sizeof (LineParams),
					256 * sizeof (LineParams),
					G_ALLOC_AND_FREE);

  text->default_tab_width = 4;
  text->tab_stops = NULL;

  text->tab_stops = g_list_prepend (text->tab_stops, (void*)8);
  text->tab_stops = g_list_prepend (text->tab_stops, (void*)8);

  text->line_wrap = TRUE;
  text->is_editable = FALSE;
}

GtkWidget*
gtk_text_new (GtkAdjustment *hadj,
	      GtkAdjustment *vadj)
{
  GtkText *text;

  text = gtk_type_new (gtk_text_get_type ());

  gtk_text_set_adjustments (text, hadj, vadj);

  return GTK_WIDGET (text);
}

void
gtk_text_set_editable (GtkText *text,
		       gint     editable)
{
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));

  text->is_editable = (editable != FALSE);
}

void
gtk_text_set_adjustments (GtkText       *text,
			  GtkAdjustment *hadj,
			  GtkAdjustment *vadj)
{
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));

  if (text->hadj && (text->hadj != hadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (text->hadj), text);
      gtk_object_unref (GTK_OBJECT (text->hadj));
    }

  if (text->vadj && (text->vadj != vadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (text->vadj), text);
      gtk_object_unref (GTK_OBJECT (text->vadj));
    }

  if (!hadj)
    hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

  if (!vadj)
    vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

  if (text->hadj != hadj)
    {
      text->hadj = hadj;
      gtk_object_ref (GTK_OBJECT (text->hadj));
      gtk_object_sink (GTK_OBJECT (text->hadj));
      
      gtk_signal_connect (GTK_OBJECT (text->hadj), "changed",
			  (GtkSignalFunc) gtk_text_adjustment,
			  text);
      gtk_signal_connect (GTK_OBJECT (text->hadj), "value_changed",
			  (GtkSignalFunc) gtk_text_adjustment,
			  text);
      gtk_signal_connect (GTK_OBJECT (text->hadj), "disconnect",
			  (GtkSignalFunc) gtk_text_disconnect,
			  text);
    }

  if (text->vadj != vadj)
    {
      text->vadj = vadj;
      gtk_object_ref (GTK_OBJECT (text->vadj));
      gtk_object_sink (GTK_OBJECT (text->vadj));
      
      gtk_signal_connect (GTK_OBJECT (text->vadj), "changed",
			  (GtkSignalFunc) gtk_text_adjustment,
			  text);
      gtk_signal_connect (GTK_OBJECT (text->vadj), "value_changed",
			  (GtkSignalFunc) gtk_text_adjustment,
			  text);
      gtk_signal_connect (GTK_OBJECT (text->vadj), "disconnect",
			  (GtkSignalFunc) gtk_text_disconnect,
			  text);
    }
}

void
gtk_text_set_point (GtkText *text,
		    guint    index)
{
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));
  g_return_if_fail (index >= 0 && index <= TEXT_LENGTH (text));

  text->point = find_mark (text, index);
}

guint
gtk_text_get_point (GtkText *text)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (GTK_IS_TEXT (text), 0);

  return text->point.index;
}

guint
gtk_text_get_length (GtkText *text)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (GTK_IS_TEXT (text), 0);

  return TEXT_LENGTH (text);
}

void
gtk_text_freeze (GtkText *text)
{
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));

  text->freeze = TRUE;
}

void
gtk_text_thaw (GtkText *text)
{
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));

  text->freeze = FALSE;

  if (GTK_WIDGET_DRAWABLE (text))
    {
      recompute_geometry (text);
      gtk_widget_queue_draw (GTK_WIDGET (text));
    }
}

void
gtk_text_insert (GtkText    *text,
		 GdkFont    *font,
		 GdkColor   *fore,
		 GdkColor   *back,
		 const char *chars,
		 gint        length)
{
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));

  g_assert (GTK_WIDGET_REALIZED (text));

  if (fore == NULL)
    fore = &text->widget.style->fg[GTK_STATE_NORMAL];
  if (back == NULL)
    back = &text->widget.style->bg[GTK_STATE_NORMAL];
  
  /* This must be because we need to have the style set up. */
  g_assert (GTK_WIDGET_REALIZED(text));

  if (length < 0)
    length = strlen (chars);

  if (length == 0)
    return;

  move_gap_to_point (text);

  if (font == NULL)
    font = GTK_WIDGET (text)->style->font;

  make_forward_space (text, length);

  memcpy (text->text + text->gap_position, chars, length);

  insert_text_property (text, font, fore, back, length);

  text->gap_size -= length;
  text->gap_position += length;

  advance_mark_n (&text->point, length);
}

gint
gtk_text_backward_delete (GtkText *text,
			  guint    nchars)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (GTK_IS_TEXT (text), 0);

  if (nchars > text->point.index || nchars <= 0)
    return FALSE;

  gtk_text_set_point (text, text->point.index - nchars);

  return gtk_text_forward_delete (text, nchars);
}

gint
gtk_text_forward_delete (GtkText *text,
			  guint    nchars)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (GTK_IS_TEXT (text), 0);

  if (text->point.index + nchars > TEXT_LENGTH (text) || nchars <= 0)
    return FALSE;

  move_gap_to_point (text);

  text->gap_size += nchars;

  delete_text_property (text, nchars);

  return TRUE;
}

static void
gtk_text_finalize (GtkObject *object)
{
  GtkText *text;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_TEXT (object));

  text = (GtkText *)object;
  if (text->hadj)
    gtk_object_unref (GTK_OBJECT (text->hadj));
  if (text->vadj)
    gtk_object_unref (GTK_OBJECT (text->vadj));

  GTK_OBJECT_CLASS(parent_class)->finalize (object);
}

static void
gtk_text_realize (GtkWidget *widget)
{
  GtkText *text;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));

  text = (GtkText*) widget;
  GTK_WIDGET_SET_FLAGS (text, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_BUTTON_MOTION_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
			    GDK_KEY_PRESS_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, text);

  attributes.x = (widget->style->klass->xthickness + TEXT_BORDER_ROOM);
  attributes.y = (widget->style->klass->ythickness + TEXT_BORDER_ROOM);
  attributes.width = widget->allocation.width - attributes.x * 2;
  attributes.height = widget->allocation.height - attributes.y * 2;

  text->text_area = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (text->text_area, text);

  widget->style = gtk_style_attach (widget->style, widget->window);

  /* Can't call gtk_style_set_background here because its handled specially */
  if (!text->widget.style->bg_pixmap[GTK_STATE_NORMAL])
    gdk_window_set_background (text->widget.window, &text->widget.style->bg[GTK_STATE_NORMAL]);

  if (!text->widget.style->bg_pixmap[GTK_STATE_NORMAL])
    gdk_window_set_background (text->text_area, &text->widget.style->bg[GTK_STATE_NORMAL]);

  text->line_wrap_bitmap = gdk_bitmap_create_from_data (text->text_area,
							(gchar*) line_wrap_bits,
							line_wrap_width,
							line_wrap_height);

  text->line_arrow_bitmap = gdk_bitmap_create_from_data (text->text_area,
							 (gchar*) line_arrow_bits,
							 line_arrow_width,
							 line_arrow_height);

  text->gc = gdk_gc_new (text->text_area);
  gdk_gc_set_exposures (text->gc, TRUE);
  gdk_gc_set_foreground (text->gc, &widget->style->fg[GTK_STATE_NORMAL]);

  init_properties (text);

  gdk_window_show (text->text_area);
}

static void
gtk_text_unrealize (GtkWidget *widget)
{
  GtkText *text;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));

  text = GTK_TEXT (widget);

  gdk_window_set_user_data (text->text_area, NULL);
  gdk_window_destroy (text->text_area);
  text->text_area = NULL;

  gdk_gc_destroy (text->gc);
  text->gc = NULL;

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
clear_focus_area (GtkText *text, gint area_x, gint area_y, gint area_width, gint area_height)
{
  gint ythick = TEXT_BORDER_ROOM + text->widget.style->klass->ythickness;
  gint xthick = TEXT_BORDER_ROOM + text->widget.style->klass->xthickness;

  gint width, height;
  gint xorig, yorig;
  gint x, y;

  gdk_window_get_size (text->widget.style->bg_pixmap[GTK_STATE_NORMAL], &width, &height);

  yorig = - text->first_onscreen_ver_pixel + ythick;
  xorig = - text->first_onscreen_hor_pixel + xthick;

  while (yorig > 0)
    yorig -= height;

  while (xorig > 0)
    xorig -= width;

  for (y = area_y; y < area_y + area_height; )
    {
      gint yoff = (y - yorig) % height;
      gint yw = MIN(height - yoff, (area_y + area_height) - y);

      for (x = area_x; x < area_x + area_width; )
	{
	  gint xoff = (x - xorig) % width;
	  gint xw = MIN(width - xoff, (area_x + area_width) - x);

	  gdk_draw_pixmap (text->widget.window,
			   text->gc,
			   text->widget.style->bg_pixmap[GTK_STATE_NORMAL],
			   xoff,
			   yoff,
			   x,
			   y,
			   xw,
			   yw);

	  x += width - xoff;
	}
      y += height - yoff;
    }
}

static void
gtk_text_draw_focus (GtkWidget *widget)
{
  GtkText *text;
  gint width, height;
  gint x, y;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));

  text = GTK_TEXT (widget);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      TDEBUG (("in gtk_text_draw_focus\n"));

      x = 0;
      y = 0;
      width = widget->allocation.width;
      height = widget->allocation.height;

      if (widget->style->bg_pixmap[GTK_STATE_NORMAL])
	{
	  gint ythick = TEXT_BORDER_ROOM + widget->style->klass->ythickness;
	  gint xthick = TEXT_BORDER_ROOM + widget->style->klass->xthickness;

	  /* top rect */
	  clear_focus_area (text, 0, 0, width, ythick);
	  /* right rect */
	  clear_focus_area (text, 0, ythick, xthick, height - 2 * ythick);
	  /* left rect */
	  clear_focus_area (text, width - xthick, ythick, xthick, height - 2 * ythick);
	  /* bottom rect */
	  clear_focus_area (text, 0, height - ythick, width, ythick);
	}

      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  x += 1;
	  y += 1;
	  width -=  2;
	  height -= 2;

	  gdk_draw_rectangle (widget->window,
			      widget->style->fg_gc[GTK_STATE_NORMAL],
			      FALSE, 0, 0,
			      widget->allocation.width - 1,
			      widget->allocation.height - 1);
	}
      else
	{
	  gdk_draw_rectangle (widget->window,
			      widget->style->white_gc, FALSE,
			      x + 2,
			      y + 2,
			      width - 1 - 2,
			      height - 1 - 2);
	}

      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, GTK_SHADOW_IN,
		       x, y, width, height);
    }
  else
    {
      TDEBUG (("in gtk_text_draw_focus (undrawable !!!)\n"));
    }
}

static void
gtk_text_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  gint xthickness;
  gint ythickness;
  gint char_height;
  gint char_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));
  g_return_if_fail (requisition != NULL);

  xthickness = widget->style->klass->xthickness + TEXT_BORDER_ROOM;
  ythickness = widget->style->klass->ythickness + TEXT_BORDER_ROOM;

  char_height = MIN_TEXT_HEIGHT_LINES * (widget->style->font->ascent +
					 widget->style->font->descent);

  char_width = MIN_TEXT_WIDTH_LINES * (gdk_text_width (widget->style->font,
						       "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
						       26)
				       / 26);

  requisition->width  = char_width  + xthickness * 2;
  requisition->height = char_height + ythickness * 2;
}

static void
gtk_text_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GtkText *text;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));
  g_return_if_fail (allocation != NULL);

  text = GTK_TEXT (widget);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gdk_window_move_resize (text->text_area,
			      widget->style->klass->xthickness + TEXT_BORDER_ROOM,
			      widget->style->klass->ythickness + TEXT_BORDER_ROOM,
			      widget->allocation.width - (widget->style->klass->xthickness +
							  TEXT_BORDER_ROOM) * 2,
			      widget->allocation.height - (widget->style->klass->ythickness +
							   TEXT_BORDER_ROOM) * 2);

      recompute_geometry (text);
    }
}

static void
gtk_text_draw (GtkWidget    *widget,
	       GdkRectangle *area)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      expose_text (GTK_TEXT (widget), area, TRUE);
      gtk_widget_draw_focus (widget);
    }
}

static gint
gtk_text_expose (GtkWidget      *widget,
		 GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->window == GTK_TEXT (widget)->text_area)
    {
      TDEBUG (("in gtk_text_expose (expose)\n"));
      expose_text (GTK_TEXT (widget), &event->area, TRUE);
    }
  else if (event->count == 0)
    {
      TDEBUG (("in gtk_text_expose (focus)\n"));
      gtk_widget_draw_focus (widget);
    }

  return FALSE;
}

static gint
gtk_text_button_press (GtkWidget      *widget,
		       GdkEventButton *event)
{
  GtkText *text;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  text = GTK_TEXT(widget);
  if (!GTK_WIDGET_HAS_FOCUS (widget))
    gtk_widget_grab_focus (widget);

  if (event->type == GDK_BUTTON_PRESS && event->button != 2)
    gtk_grab_add (widget);

  if (event->button == 1)
    {
      switch (event->type)
	{
	case GDK_BUTTON_PRESS:
	  undraw_cursor (GTK_TEXT (widget), FALSE);
	  mouse_click_1 (GTK_TEXT (widget), event);
	  draw_cursor (GTK_TEXT (widget), FALSE);
	  /* start selection */
	  break;

	case GDK_2BUTTON_PRESS:
	  /* select word */
	  break;

	case GDK_3BUTTON_PRESS:
	  /* select line */
	  break;

	default:
	  break;
	}
    }
  else if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 2)
	{
	  /* insert selection. */
	}
      else
	{
	  /* start selection */
	}
    }

  return FALSE;
}

static gint
gtk_text_button_release (GtkWidget      *widget,
			 GdkEventButton *event)
{
  GtkText *text;
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->button != 2)
    {
      gtk_grab_remove (widget);

      text = GTK_TEXT (widget);

      /* stop selecting. */
    }

  return FALSE;
}

static gint
gtk_text_motion_notify (GtkWidget      *widget,
			 GdkEventMotion *event)
{
  GtkText *text;
  gint x;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  text = GTK_TEXT (widget);

  x = event->x;
  if (event->is_hint || (text->text_area != event->window))
    gdk_window_get_pointer (text->text_area, &x, NULL, NULL);

  /* update selection */

  return FALSE;
}

static void
gtk_text_insert_1_at_point (GtkText* text, gchar key)
{
  guint old_height= total_line_height (text, text->current_line, 1);
  gtk_text_insert (text,
		   MARK_CURRENT_FONT (&text->point),
		   MARK_CURRENT_FORE (&text->point),
		   MARK_CURRENT_BACK (&text->point),
		   &key, 1);


  insert_char_line_expose (text, key, old_height );

}

static void
gtk_text_backward_delete_1_at_point (GtkText* text)
{
  guint old_height;
  gchar key= TEXT_INDEX(text,text->cursor_mark.index-1);

  if (1 > text->point.index )
    return;

  gtk_text_set_point (text, text->point.index - 1);
  find_line_containing_point (text, text->cursor_mark.index-1);

  old_height= total_line_height (text, text->current_line, 1 + (key == LINE_DELIM));

  gtk_text_forward_delete (text, 1);
  
  delete_char_line_expose (text, key, old_height);
}

static void
gtk_text_forward_delete_1_at_point (GtkText* text)
{
  guint old_height;
  gchar key= TEXT_INDEX(text,text->cursor_mark.index);

  old_height= total_line_height (text, text->current_line, 1 + (key == LINE_DELIM));
  gtk_text_forward_delete (text, 1);

  delete_char_line_expose (text, key, old_height);
}

static gint
gtk_text_key_press (GtkWidget   *widget,
		    GdkEventKey *event)
{
  GtkText *text;
  gchar key;
  gint return_val;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  return_val = FALSE;

  text = GTK_TEXT (widget);

  if (!return_val)
    {
      key = event->keyval;
      return_val = TRUE;

      if (text->is_editable == FALSE)
	{
	  switch (event->keyval)
	    {
	    case GDK_Home:      scroll_int (text, -text->vadj->value); break;
	    case GDK_End:       scroll_int (text, +text->vadj->upper); break;
	    case GDK_Page_Up:   scroll_int (text, -text->vadj->page_increment); break;
	    case GDK_Page_Down: scroll_int (text, +text->vadj->page_increment); break;
	    case GDK_Up:        scroll_int (text, -KEY_SCROLL_PIXELS); break;
	    case GDK_Down:      scroll_int (text, +KEY_SCROLL_PIXELS); break;
	    default: break;
	    }
	}
      else
	{
	  text->point = find_mark (text, text->cursor_mark.index);

	  switch (event->keyval)
	    {
	    case GDK_Home:      move_cursor_buffer_ver (text, -1); break;
	    case GDK_End:       move_cursor_buffer_ver (text, +1); break;
	    case GDK_Page_Up:   move_cursor_page_ver (text, -1); break;
	    case GDK_Page_Down: move_cursor_page_ver (text, +1); break;
	    case GDK_Up:        move_cursor_ver (text, -1); break;
	    case GDK_Down:      move_cursor_ver (text, +1); break;
	    case GDK_Left:      move_cursor_hor (text, -1); break;
	    case GDK_Right:     move_cursor_hor (text, +1); break;

	    case GDK_BackSpace:
	      if (!text->has_cursor || text->cursor_mark.index == 0)
		break;

	      gtk_text_backward_delete_1_at_point (text);
	      break;
	    case GDK_Delete:
	      if (!text->has_cursor || LAST_INDEX (text, text->cursor_mark))
		break;

	      gtk_text_forward_delete_1_at_point (text);
	      break;
	    case GDK_Tab:
	      if (!text->has_cursor)
		break;

	      gtk_text_insert_1_at_point (text, '\t');
	      break;
	    case GDK_Return:
	      if (!text->has_cursor)
		break;

	      gtk_text_insert_1_at_point (text, '\n');
	      break;
	    default:
	      if (!text->has_cursor)
		break;

	      if ((event->keyval >= 0x20) && (event->keyval <= 0x7e))
		{
		  return_val = TRUE;

		  if (event->state & GDK_CONTROL_MASK)
		    {
		      if ((key >= 'A') && (key <= 'Z'))
			key -= 'A' - 'a';

		      if ((key >= 'a') && (key <= 'z') && text->control_keys[(int) (key - 'a')])
			(* text->control_keys[(int) (key - 'a')]) (text);
		    }
		  else if (event->state & GDK_MOD1_MASK)
		    {
		      g_message ("alt key");

		      if ((key >= 'A') && (key <= 'Z'))
			key -= 'A' - 'a';

		      if ((key >= 'a') && (key <= 'z') && text->alt_keys[(int) (key - 'a')])
			(* text->alt_keys[(int) (key - 'a')]) (text);
		    }
		  else
		    {
		      gtk_text_insert_1_at_point (text, key);
		    }
		}
	      else
		{
		  return_val = FALSE;
		}
	      break;
	    }
	}
    }

  return return_val;
}

static gint
gtk_text_focus_in (GtkWidget     *widget,
		   GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  TDEBUG (("in gtk_text_focus_in\n"));

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

  draw_cursor (GTK_TEXT(widget), TRUE);

  return FALSE;
}

static gint
gtk_text_focus_out (GtkWidget     *widget,
		    GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  TDEBUG (("in gtk_text_focus_out\n"));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

  undraw_cursor (GTK_TEXT(widget), TRUE);

  return FALSE;
}

static void
gtk_text_adjustment (GtkAdjustment *adjustment,
		     GtkText       *text)
{
  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));

  if (adjustment == text->hadj)
    {
      g_warning ("horizontal scrolling not implemented");
    }
  else
    {
      gint diff = ((gint)adjustment->value) - text->last_ver_value;

      if (diff != 0)
	{
	  undraw_cursor (text, FALSE);

	  if (diff > 0)
	    scroll_down (text, diff);
	  else /* if (diff < 0) */
	    scroll_up (text, diff);

	  draw_cursor (text, FALSE);

	  text->last_ver_value = adjustment->value;
	}
    }
}

static void
gtk_text_disconnect (GtkAdjustment *adjustment,
		     GtkText       *text)
{
  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));

  if (adjustment == text->hadj)
    text->hadj = NULL;
  if (adjustment == text->vadj)
    text->vadj = NULL;
}


static GtkPropertyMark
find_this_line_start_mark (GtkText* text, guint point_position, const GtkPropertyMark* near)
{
  GtkPropertyMark mark;

  mark = find_mark_near (text, point_position, near);

  while (mark.index > 0 &&
	 TEXT_INDEX (text, mark.index - 1) != LINE_DELIM)
    decrement_mark (&mark);

  return mark;
}

static void
init_tab_cont (GtkText* text, PrevTabCont* tab_cont)
{
  tab_cont->pixel_offset          = 0;
  tab_cont->tab_start.tab_stops   = text->tab_stops;
  tab_cont->tab_start.to_next_tab = (gulong) text->tab_stops->data;

  if (!tab_cont->tab_start.to_next_tab)
    tab_cont->tab_start.to_next_tab = text->default_tab_width;
}

static void
line_params_iterate (GtkText* text,
		     const GtkPropertyMark* mark0,
		     const PrevTabCont* tab_mark0,
		     gint8 alloc,
		     void* data,
		     LineIteratorFunction iter)
  /* mark0 MUST be a real line start.  if ALLOC, allocate line params
   * from a mem chunk.  DATA is passed to ITER_CALL, which is called
   * for each line following MARK, iteration continues unless ITER_CALL
   * returns TRUE. */
{
  GtkPropertyMark mark = *mark0;
  PrevTabCont  tab_conts[2];
  LineParams   *lp, lpbuf;
  gint         tab_cont_index = 0;

  if (tab_mark0)
    tab_conts[0] = *tab_mark0;
  else
    init_tab_cont (text, tab_conts);

  for (;;)
    {
      if (alloc)
	lp = g_chunk_new (LineParams, params_mem_chunk);
      else
	lp = &lpbuf;

      *lp = find_line_params (text, &mark, tab_conts + tab_cont_index,
			      tab_conts + (tab_cont_index + 1) % 2);

      if ((*iter) (text, lp, data))
	return;

      if (LAST_INDEX (text, lp->end))
	break;

      mark = lp->end;
      advance_mark (&mark);
      tab_cont_index = (tab_cont_index + 1) % 2;
    }
}

static gint
fetch_lines_iterator (GtkText* text, LineParams* lp, void* data)
{
  FetchLinesData *fldata = (FetchLinesData*) data;

  fldata->new_lines = g_list_prepend (fldata->new_lines, lp);

  switch (fldata->fl_type)
    {
    case FetchLinesCount:
      if (!text->line_wrap || !lp->wraps)
	fldata->data += 1;

      if (fldata->data >= fldata->data_max)
	return TRUE;

      break;
    case FetchLinesPixels:

      fldata->data += LINE_HEIGHT(*lp);

      if (fldata->data >= fldata->data_max)
	return TRUE;

      break;
    }

  return FALSE;
}

static GList*
fetch_lines (GtkText* text,
	     const GtkPropertyMark* mark0,
	     const PrevTabCont* tab_cont0,
	     FLType fl_type,
	     gint data)
{
  FetchLinesData fl_data;

  fl_data.new_lines = NULL;
  fl_data.data      = 0;
  fl_data.data_max  = data;
  fl_data.fl_type   = fl_type;

  line_params_iterate (text, mark0, tab_cont0, TRUE, &fl_data, fetch_lines_iterator);

  return g_list_reverse (fl_data.new_lines);
}

static void
fetch_lines_backward (GtkText* text)
{
  GList* new_lines = NULL, *new_line_start;

  GtkPropertyMark mark = find_this_line_start_mark (text,
						 CACHE_DATA(text->line_start_cache).start.index - 1,
						 &CACHE_DATA(text->line_start_cache).start);

  new_line_start = new_lines = fetch_lines (text, &mark, NULL, FetchLinesCount, 1);

  while (new_line_start->next)
    new_line_start = new_line_start->next;

  new_line_start->next = text->line_start_cache;
  text->line_start_cache->prev = new_line_start;
}

static void
fetch_lines_forward (GtkText* text, gint line_count)
{
  GtkPropertyMark mark;
  GList* line = text->line_start_cache;

  while(line->next)
    line = line->next;

  mark = CACHE_DATA(line).end;

  if (LAST_INDEX (text, mark))
    return;

  advance_mark(&mark);

  line->next = fetch_lines (text, &mark, &CACHE_DATA(line).tab_cont_next, FetchLinesCount, line_count);

  if (line->next)
    line->next->prev = line;
}

static gint
total_line_height (GtkText* text, GList* line, gint line_count)
{
  gint height = 0;

  for (; line && line_count > 0; line = line->next)
    {
      height += LINE_HEIGHT(CACHE_DATA(line));

      if (!text->line_wrap || !CACHE_DATA(line).wraps)
	line_count -= 1;

      if (!line->next)
	fetch_lines_forward (text, line_count);
    }

  return height;
}

static void
swap_lines (GtkText* text, GList* old, GList* new, gint old_line_count)
{
  if (old == text->line_start_cache)
    {
      GList* last;

      for (; old_line_count > 0; old_line_count -= 1)
	{
	  while (text->line_start_cache &&
		 text->line_wrap &&
		 CACHE_DATA(text->line_start_cache).wraps)
	    remove_cache_line(text, text->line_start_cache);

	  remove_cache_line(text, text->line_start_cache);
	}

      last = g_list_last (new);

      last->next = text->line_start_cache;

      if (text->line_start_cache)
	text->line_start_cache->prev = last;

      text->line_start_cache = new;
    }
  else
    {
      GList *last;

      g_assert (old->prev);

      last = old->prev;

      for (; old_line_count > 0; old_line_count -= 1)
	{
	  while (old && text->line_wrap && CACHE_DATA(old).wraps)
	    old = remove_cache_line (text, old);

	  old = remove_cache_line (text, old);
	}

      last->next = new;
      new->prev = last;

      last = g_list_last (new);

      last->next = old;

      if (old)
	old->prev = last;
    }
}

static void
correct_cache_delete (GtkText* text, gint lines)
{
  GList* cache = text->current_line;
  gint i;

  for (i = 0; cache && i < lines; i += 1, cache = cache->next)
    /* nothing */;

  for (; cache; cache = cache->next)
    {
      GtkPropertyMark *start = &CACHE_DATA(cache).start;
      GtkPropertyMark *end = &CACHE_DATA(cache).end;

      start->index -= 1;
      end->index -= 1;

      if (start->property == text->point.property)
	start->offset = start->index - (text->point.index - text->point.offset);

      if (end->property == text->point.property)
	end->offset = end->index - (text->point.index - text->point.offset);

      /*TEXT_ASSERT_MARK(text, start, "start");*/
      /*TEXT_ASSERT_MARK(text, end, "end");*/
    }
}

static void
delete_char_line_expose (GtkText* text, gchar key, guint old_pixels)
{
  gint pixel_height;
  guint new_pixels = 0;
  gint old_line_count = 1 + (key == LINE_DELIM);
  GdkRectangle rect;
  GList* new_line = NULL;
  gint width, height;

  text->cursor_virtual_x = 0;

  undraw_cursor (text, FALSE);

  correct_cache_delete (text, old_line_count);

  pixel_height = pixel_height_of(text, text->current_line) -
                 LINE_HEIGHT(CACHE_DATA(text->current_line));

  if (CACHE_DATA(text->current_line).start.index == text->point.index)
    CACHE_DATA(text->current_line).start = text->point;

  new_line = fetch_lines (text,
			  &CACHE_DATA(text->current_line).start,
			  &CACHE_DATA(text->current_line).tab_cont,
			  FetchLinesCount,
			  1);

  swap_lines (text, text->current_line, new_line, old_line_count);

  text->current_line = new_line;

  new_pixels = total_line_height (text, new_line, 1);

  gdk_window_get_size (text->text_area, &width, &height);

  if (old_pixels != new_pixels)
    {
      gdk_draw_pixmap (text->text_area,
		       text->gc,
		       text->text_area,
		       0,
		       pixel_height + old_pixels,
		       0,
		       pixel_height + new_pixels,
		       width,
		       height);

      text->vadj->upper += new_pixels;
      text->vadj->upper -= old_pixels;
      adjust_adj (text, text->vadj);
    }

  rect.x = 0;
  rect.y = pixel_height;
  rect.width = width;
  rect.height = new_pixels;

  expose_text (text, &rect, FALSE);
  gtk_text_draw_focus ( (GtkWidget *) text);

  text->cursor_mark = text->point;

  find_cursor (text);

  draw_cursor (text, FALSE);

  if (old_pixels != new_pixels)
    process_exposes (text);
  
  TEXT_ASSERT (text);
  TEXT_SHOW(text);
}

static void
correct_cache_insert (GtkText* text)
{
  GList* cache = text->current_line;

  for (; cache; cache = cache->next)
    {
      GtkPropertyMark *start = &CACHE_DATA(cache).start;
      GtkPropertyMark *end = &CACHE_DATA(cache).end;

      if (start->index >= text->point.index)
	{
	  if (start->property == text->point.property)
	    move_mark_n(start, 1);
	  else
	    start->index += 1;
	}

      if (end->index >= text->point.index)
	{
	  if (end->property == text->point.property)
	    move_mark_n(end, 1);
	  else
	    end->index += 1;
	}

      /*TEXT_ASSERT_MARK(text, start, "start");*/
      /*TEXT_ASSERT_MARK(text, end, "end");*/
    }
}


static void
insert_char_line_expose (GtkText* text, gchar key, guint old_pixels)
{
  gint pixel_height;
  guint new_pixels = 0;
  guint new_line_count = 1 + (key == LINE_DELIM);
  GdkRectangle rect;
  GList* new_line = NULL;
  gint width, height;

  text->cursor_virtual_x = 0;

  undraw_cursor (text, FALSE);

  correct_cache_insert (text);

  TEXT_SHOW_ADJ (text, text->vadj, "vadj");

  pixel_height = pixel_height_of(text, text->current_line) -
                 LINE_HEIGHT(CACHE_DATA(text->current_line));

  new_line = fetch_lines (text,
			  &CACHE_DATA(text->current_line).start,
			  &CACHE_DATA(text->current_line).tab_cont,
			  FetchLinesCount,
			  new_line_count);

  swap_lines (text, text->current_line, new_line, 1);

  text->current_line = new_line;

  new_pixels = total_line_height (text, new_line, new_line_count);

  gdk_window_get_size (text->text_area, &width, &height);

  if (old_pixels != new_pixels)
    {
      gdk_draw_pixmap (text->text_area,
		       text->gc,
		       text->text_area,
		       0,
		       pixel_height + old_pixels,
		       0,
		       pixel_height + new_pixels,
		       width,
		       height + (old_pixels - new_pixels) - pixel_height);

      text->vadj->upper += new_pixels;
      text->vadj->upper -= old_pixels;
      adjust_adj (text, text->vadj);
    }

  rect.x = 0;
  rect.y = pixel_height;
  rect.width = width;
  rect.height = new_pixels;

  expose_text (text, &rect, FALSE);
  gtk_text_draw_focus ( (GtkWidget *) text);

  text->cursor_mark = text->point;

  find_cursor (text);

  draw_cursor (text, FALSE);

  if (old_pixels != new_pixels)
    process_exposes (text);
    
  TEXT_SHOW_ADJ (text, text->vadj, "vadj");
  TEXT_ASSERT (text);
  TEXT_SHOW(text);
}

static guint
font_hash (gpointer font)
{
  return gdk_font_id ((GdkFont*) font);
}

static TextFont*
get_text_font (GdkFont* gfont)
{
  static GHashTable *font_cache_table = NULL;
  TextFont* tf;
  gpointer lu;
  gint i;

  if (!font_cache_table)
    font_cache_table = g_hash_table_new (font_hash, (GCompareFunc) gdk_font_equal);

  lu = g_hash_table_lookup (font_cache_table, gfont);

  if (lu)
    return (TextFont*)lu;

  tf = g_new (TextFont, 1);

  tf->gdk_font = gfont;

  for(i = 0; i < 256; i += 1)
    tf->char_widths[i] = gdk_char_width (gfont, (char)i);

  g_hash_table_insert (font_cache_table, gfont, tf);

  return tf;
}

static gint
text_properties_equal (TextProperty* prop, GdkFont* font, GdkColor *fore, GdkColor *back)
{
  return prop->font == get_text_font(font) &&
    gdk_color_equal(&prop->fore_color, fore) &&
    gdk_color_equal(&prop->back_color, back);
}

static TextProperty*
new_text_property (GdkFont* font, GdkColor* fore, GdkColor* back, guint length)
{
  TextProperty *prop;

  if (text_property_chunk == NULL)
    {
      text_property_chunk = g_mem_chunk_new ("text property mem chunk",
					     sizeof(TextProperty),
					     1024*sizeof(TextProperty),
					     G_ALLOC_ONLY);
    }

  prop = g_chunk_new(TextProperty, text_property_chunk);

  prop->font = get_text_font (font);
  prop->fore_color = *fore;
  if (back)
    prop->back_color = *back;
  prop->length = length;

  return prop;
}

/* Flop the memory between the point and the gap around like a
 * dead fish. */
static void
move_gap_to_point (GtkText* text)
{
  if (text->gap_position < text->point.index)
    {
      gint diff = text->point.index - text->gap_position;

      g_memmove (text->text + text->gap_position,
		 text->text + text->gap_position + text->gap_size,
		 diff);

      text->gap_position = text->point.index;
    }
  else if (text->gap_position > text->point.index)
    {
      gint diff = text->gap_position - text->point.index;

      g_memmove (text->text + text->point.index + text->gap_size,
		 text->text + text->point.index,
		 diff);

      text->gap_position = text->point.index;
    }
}

/* Increase the gap size. */
static void
make_forward_space (GtkText* text, guint len)
{
  if (text->gap_size < len)
    {
      guint sum = MAX(2*len, MIN_GAP_SIZE) + text->text_end;

      if (sum >= text->text_len)
	{
	  guint i = 1;

	  while (i <= sum) i <<= 1;

	  text->text = (guchar*)g_realloc(text->text, i);
	}

      g_memmove (text->text + text->gap_position + text->gap_size + 2*len,
		 text->text + text->gap_position + text->gap_size,
		 text->text_end - (text->gap_position + text->gap_size));

      text->text_end += len*2;
      text->gap_size += len*2;
    }
}

/* Inserts into the text property list a list element that guarantees
 * that for len characters following the point, text has the correct
 * property.  does not move point.  adjusts text_properties_point and
 * text_properties_point_offset relative to the current value of
 * point. */
static void
insert_text_property (GtkText* text, GdkFont* font,
		      GdkColor *fore, GdkColor* back, guint len)
{
  GtkPropertyMark *mark = &text->point;
  TextProperty* forward_prop = MARK_CURRENT_PROPERTY(mark);
  TextProperty* backward_prop = MARK_PREV_PROPERTY(mark);

  if (MARK_OFFSET(mark) == 0)
    {
      /* Point is on the boundary of two properties.
       * If it is the same as either, grow, else insert
       * a new one. */

      if (text_properties_equal(forward_prop, font, fore, back))
	{
	  /* Grow the property in front of us. */

	  MARK_PROPERTY_LENGTH(mark) += len;
	}
      else if (backward_prop &&
	       text_properties_equal(backward_prop, font, fore, back))
	{
	  /* Grow property behind us, point property and offset
	   * change. */

	  SET_PROPERTY_MARK (&text->point,
			     MARK_PREV_LIST_PTR (mark),
			     backward_prop->length);

	  backward_prop->length += len;
	}
      else
	{
	  /* Splice a new property into the list. */

	  GList* new_prop = g_list_alloc();

	  new_prop->next = MARK_LIST_PTR(mark);
	  new_prop->prev = MARK_PREV_LIST_PTR(mark);
	  new_prop->next->prev = new_prop;

	  if (new_prop->prev)
	    new_prop->prev->next = new_prop;

	  new_prop->data = new_text_property (font, fore, back, len);

	  SET_PROPERTY_MARK (mark, new_prop, 0);
	}
    }
  else
    {
      /* In the middle of forward_prop, if properties are equal,
       * just add to its length, else split it into two and splice
       * in a new one. */
      if (text_properties_equal (forward_prop, font, fore, back))
	{
	  forward_prop->length += len;
	}
      else
	{
	  GList* new_prop = g_list_alloc();
	  GList* new_prop_forward = g_list_alloc();
	  gint old_length = forward_prop->length;
	  GList* next = MARK_NEXT_LIST_PTR(mark);

	  /* Set the new lengths according to where they are split.  Construct
	   * two new properties. */
	  forward_prop->length = MARK_OFFSET(mark);

	  new_prop_forward->data = new_text_property(forward_prop->font->gdk_font,
						     fore,
						     back,
						     old_length - forward_prop->length);

	  new_prop->data = new_text_property(font, fore, back, len);

	  /* Now splice things in. */
	  MARK_NEXT_LIST_PTR(mark) = new_prop;
	  new_prop->prev = MARK_LIST_PTR(mark);

	  new_prop->next = new_prop_forward;
	  new_prop_forward->prev = new_prop;

	  new_prop_forward->next = next;

	  if (next)
	    next->prev = new_prop_forward;

	  SET_PROPERTY_MARK (mark, new_prop, 0);
	}
    }

  while (text->text_properties_end->next)
    text->text_properties_end = text->text_properties_end->next;

  while (text->text_properties->prev)
    text->text_properties = text->text_properties->prev;
}

static void
delete_text_property (GtkText* text, guint nchars)
{
  /* Delete nchars forward from point. */
  TextProperty *prop;
  GList        *tmp;
  gint          is_first;

  for(; nchars; nchars -= 1)
    {
      prop = MARK_CURRENT_PROPERTY(&text->point);

      prop->length -= 1;

      if (prop->length == 0)
	{
	  tmp = MARK_LIST_PTR (&text->point);

	  is_first = tmp == text->text_properties;

	  MARK_LIST_PTR (&text->point) = g_list_remove_link (tmp, tmp);
	  text->point.offset = 0;

	  g_mem_chunk_free (text_property_chunk, prop);

	  prop = MARK_CURRENT_PROPERTY (&text->point);

	  if (is_first)
	    text->text_properties = MARK_LIST_PTR (&text->point);

	  g_assert (prop->length != 0);
	}
      else if (prop->length == text->point.offset)
	{
	  MARK_LIST_PTR (&text->point) = MARK_NEXT_LIST_PTR (&text->point);
	  text->point.offset = 0;
	}
    }
}

static void
init_properties (GtkText *text)
{
  if (!text->text_properties)
    {
      text->text_properties = g_list_alloc();
      text->text_properties->next = NULL;
      text->text_properties->prev = NULL;
      text->text_properties->data = new_text_property (text->widget.style->font,
						       &text->widget.style->fg[GTK_STATE_NORMAL],
						       &text->widget.style->bg[GTK_STATE_NORMAL],
						       1);
      text->text_properties_end = text->text_properties;

      SET_PROPERTY_MARK (&text->point, text->text_properties, 0);

      text->point.index = 0;
    }
}


/**********************************************************************/
/*			   Property Movement                          */
/**********************************************************************/

static void
move_mark_n (GtkPropertyMark* mark, gint n)
{
  if (n > 0)
    advance_mark_n(mark, n);
  else if (n < 0)
    decrement_mark_n(mark, -n);
}

static void
advance_mark_n (GtkPropertyMark* mark, gint n)
{
  gint i;

  g_assert (n > 0);

  for (i = 0; i < n; i += 1)
    advance_mark (mark);
}

static void
advance_mark (GtkPropertyMark* mark)
{
  TextProperty* prop = MARK_CURRENT_PROPERTY (mark);

  mark->index += 1;

  if (prop->length > mark->offset + 1)
    mark->offset += 1;
  else
    {
      mark->property = MARK_NEXT_LIST_PTR (mark);
      mark->offset   = 0;
    }
}

static void
decrement_mark (GtkPropertyMark* mark)
{
  mark->index -= 1;

  if (mark->offset > 0)
    mark->offset -= 1;
  else
    {
      mark->property = MARK_PREV_LIST_PTR (mark);
      mark->offset   = MARK_CURRENT_PROPERTY (mark)->length - 1;
    }
}

static void
decrement_mark_n (GtkPropertyMark* mark, gint n)
{
  gint i;

  g_assert (n > 0);

  for (i = 0; i < n; i += 1)
    decrement_mark (mark);
}

static GtkPropertyMark
find_mark (GtkText* text, guint mark_position)
{
  return find_mark_near (text, mark_position, &text->point);
}

/* This can be optimized in two ways.
 * First, advances can be made in units of the current TextProperty
 * length, when possible.  This will reduce computation and function
 * call overhead.
 *
 * You can also start from the end, what a drag.
 */
static GtkPropertyMark
find_mark_near (GtkText* text, guint mark_position, const GtkPropertyMark* near)
{
  gint diffa;
  gint diffb;

  GtkPropertyMark mark;

  if (!near)
    diffa = mark_position + 1;
  else
    diffa = mark_position - near->index;

  diffb = mark_position;

  if (diffa < 0)
    diffa = -diffa;

  if (diffa <= diffb)
    {
      mark = *near;
    }
  else
    {
      mark.index = 0;
      mark.property = text->text_properties;
      mark.offset = 0;
    }

  if (mark.index > mark_position)
    {
      while (mark.index > mark_position)
	decrement_mark (&mark);
    }
  else
    {
      while (mark_position > mark.index)
	advance_mark (&mark);
    }

  return mark;
}

static void
find_line_containing_point (GtkText* text, guint point)
{
  GList* cache;
  gint height;

  text->current_line = NULL;

  if (!text->line_start_cache->next)
    {
      /* @@@ Its visible, right? */
      text->current_line = text->line_start_cache;
      return;
    }

  while ( ( (text->first_cut_pixels != 0) &&
	    (CACHE_DATA(text->line_start_cache->next).start.index > point) ) ||
	  ( (text->first_cut_pixels == 0) &&
	    (CACHE_DATA(text->line_start_cache).start.index > point) ) )
    {
      scroll_int (text, - SCROLL_PIXELS);
      g_assert (text->line_start_cache->next);
    }

  TEXT_SHOW (text);
  gdk_window_get_size (text->text_area, NULL, &height);

  for (cache = text->line_start_cache; cache; cache = cache->next)
    {
      guint lph;

      if (CACHE_DATA(cache).end.index >= point ||
	  LAST_INDEX(text, CACHE_DATA(cache).end))
	{
	  text->current_line = cache; /* LOOK HERE, this proc has an
				       * important side effect. */
	  return;
	}

      TEXT_SHOW_LINE (text, cache, "cache");

      lph = pixel_height_of (text, cache->next);

      while (lph > height || lph == 0)
	{
	  TEXT_SHOW_LINE (text, cache, "cache");
	  TEXT_SHOW_LINE (text, cache->next, "cache->next");
	  scroll_int (text, LINE_HEIGHT(CACHE_DATA(cache->next)));
	  lph = pixel_height_of (text, cache->next);
	}
    }

  g_assert (FALSE); /* Must set text->current_line here */
}

static guint
pixel_height_of (GtkText* text, GList* cache_line)
{
  gint pixels = - text->first_cut_pixels;
  GList *cache = text->line_start_cache;

  while (TRUE) {
    pixels += LINE_HEIGHT (CACHE_DATA(cache));

    if (cache->data == cache_line->data)
      break;

    cache = cache->next;
  }

  return pixels;
}

/**********************************************************************/
/*			Search and Placement                          */
/**********************************************************************/

static gint
find_char_width (GtkText* text, const GtkPropertyMark *mark, const TabStopMark *tab_mark)
{
  gchar ch = TEXT_INDEX (text, mark->index);
  gint16* char_widths = MARK_CURRENT_TEXT_FONT (mark)->char_widths;

  if (ch == '\t')
    {
      return tab_mark->to_next_tab * char_widths[' '];
    }
  else
    {
      return char_widths[ch & 0xff];
    }
}

static void
advance_tab_mark (GtkText* text, TabStopMark* tab_mark, gchar ch)
{
  if (tab_mark->to_next_tab == 1 || ch == '\t')
    {
      if (tab_mark->tab_stops->next)
	{
	  tab_mark->tab_stops = tab_mark->tab_stops->next;
	  tab_mark->to_next_tab = (gulong) tab_mark->tab_stops->data;
	}
      else
	{
	  tab_mark->to_next_tab = text->default_tab_width;
	}
    }
  else
    {
      tab_mark->to_next_tab -= 1;
    }
}

static void
advance_tab_mark_n (GtkText* text, TabStopMark* tab_mark, gint n)
  /* No tabs! */
{
  while (n--)
    advance_tab_mark (text, tab_mark, 0);
}

static void
find_cursor_at_line (GtkText* text, const LineParams* start_line, gint pixel_height)
{
  gchar ch;

  GtkPropertyMark mark        = start_line->start;
  TabStopMark  tab_mark    = start_line->tab_cont.tab_start;
  gint         pixel_width = LINE_START_PIXEL (*start_line);

  while (mark.index < text->cursor_mark.index)
    {
      pixel_width += find_char_width (text, &mark, &tab_mark);

      advance_tab_mark (text, &tab_mark, TEXT_INDEX(text, mark.index));
      advance_mark (&mark);
    }

  text->cursor_pos_x       = pixel_width;
  text->cursor_pos_y       = pixel_height;
  text->cursor_char_offset = start_line->font_descent;
  text->cursor_mark        = mark;

  ch = TEXT_INDEX (text, mark.index);

  if (!isspace(ch))
    text->cursor_char = ch;
  else
    text->cursor_char = 0;
}

static void
find_cursor (GtkText* text)
{
  if (!text->has_cursor)
    return;

  find_line_containing_point (text, text->cursor_mark.index);

  g_assert (text->cursor_mark.index >= text->first_line_start_index);

  if (text->current_line)
    find_cursor_at_line (text,
			 &CACHE_DATA(text->current_line),
			 pixel_height_of(text, text->current_line));
}

static void
mouse_click_1_at_line (GtkText *text, const LineParams* lp,
		       guint line_pixel_height,
		       gint button_x)
{
  GtkPropertyMark mark     = lp->start;
  TabStopMark  tab_mark = lp->tab_cont.tab_start;

  guint char_width = find_char_width(text, &mark, &tab_mark);
  guint pixel_width = LINE_START_PIXEL (*lp) + (char_width+1)/2;

  text->cursor_pos_y = line_pixel_height;

  for (;;)
    {
      gchar ch = TEXT_INDEX (text, mark.index);

      if (button_x < pixel_width || mark.index == lp->end.index)
	{
	  text->cursor_pos_x       = pixel_width - (char_width+1)/2;
	  text->cursor_mark        = mark;
	  text->cursor_char_offset = lp->font_descent;

	  if (!isspace(ch))
	    text->cursor_char = ch;
	  else
	    text->cursor_char = 0;

	  break;
	}

      advance_tab_mark (text, &tab_mark, ch);
      advance_mark (&mark);

      pixel_width += char_width/2;

      char_width = find_char_width (text, &mark, &tab_mark);

      pixel_width += (char_width+1)/2;
    }
}

static void
mouse_click_1 (GtkText* text, GdkEventButton *event)
{
  if (text->is_editable)
    {
      gint pixel_height;
      GList* cache = text->line_start_cache;

      g_assert (cache);

      pixel_height = - text->first_cut_pixels;

      text->has_cursor = 1;

      for (; cache; cache = cache->next)
	{
	  pixel_height += LINE_HEIGHT(CACHE_DATA(cache));

	  if (event->y < pixel_height || !cache->next)
	    {
	      mouse_click_1_at_line (text, &CACHE_DATA(cache), pixel_height, event->x);

	      find_cursor (text);

	      return;
	    }
	}
    }
}

/**********************************************************************/
/*			    Cache Manager                             */
/**********************************************************************/

static void
free_cache (GtkText* text)
{
  GList* cache = text->line_start_cache;

  for (; cache; cache = cache->next)
    g_mem_chunk_free (params_mem_chunk, cache->data);

  g_list_free (text->line_start_cache);

  text->line_start_cache = NULL;
}

static GList*
remove_cache_line (GtkText* text, GList* member)
{
  if (member == text->line_start_cache)
    {
      if (text->line_start_cache)
	text->line_start_cache = text->line_start_cache->next;
      return text->line_start_cache;
    }
  else
    {
      GList *list = member->prev;

      list->next = list->next->next;
      if (list->next)
	list->next->prev = list;

      member->next = NULL;
      g_mem_chunk_free (params_mem_chunk, member->data);
      g_list_free (member);

      return list->next;
    }
}

/**********************************************************************/
/*			     Key Motion                               */
/**********************************************************************/

static void
move_cursor_buffer_ver (GtkText *text, int dir)
{
  if (dir > 0)
    scroll_int (text, text->vadj->upper);
  else
    scroll_int (text, - text->vadj->value);
}

static void
move_cursor_page_ver (GtkText *text, int dir)
{
  scroll_int (text, dir * text->vadj->page_increment);
}

static void
move_cursor_ver (GtkText *text, int count)
{
  gint i;
  GtkPropertyMark mark;
  gint offset;

  if (!text->has_cursor)
    {
      scroll_int (text, count * KEY_SCROLL_PIXELS);
      return;
    }

  mark = find_this_line_start_mark (text, text->cursor_mark.index, &text->cursor_mark);
  offset = text->cursor_mark.index - mark.index;

  if (offset > text->cursor_virtual_x)
    text->cursor_virtual_x = offset;

  if (count < 0)
    {
      if (mark.index == 0)
	return;

      decrement_mark (&mark);
      mark = find_this_line_start_mark (text, mark.index, &mark);
    }
  else
    {
      mark = text->cursor_mark;

      while (!LAST_INDEX(text, mark) && TEXT_INDEX(text, mark.index) != LINE_DELIM)
	advance_mark (&mark);

      if (LAST_INDEX(text, mark))
	  return;

      advance_mark (&mark);
    }

  for (i=0; i < text->cursor_virtual_x; i += 1, advance_mark(&mark))
    if (LAST_INDEX(text, mark) || TEXT_INDEX(text, mark.index) == LINE_DELIM)
      break;

  undraw_cursor (text, FALSE);

  text->cursor_mark = mark;

  find_cursor (text);

  draw_cursor (text, FALSE);
}

static void
move_cursor_hor (GtkText *text, int count)
{
  /* count should be +-1. */
  if (!text->has_cursor)
    return;

  if ( (count > 0 && text->cursor_mark.index + count > TEXT_LENGTH(text)) ||
       (count < 0 && text->cursor_mark.index < (- count)) ||
       (count == 0) )
    return;

  text->cursor_virtual_x = 0;

  undraw_cursor (text, FALSE);

  move_mark_n (&text->cursor_mark, count);

  find_cursor (text);

  draw_cursor (text, FALSE);
}

/**********************************************************************/
/*			      Scrolling                               */
/**********************************************************************/

static void
adjust_adj (GtkText* text, GtkAdjustment* adj)
{
  gint height;

  gdk_window_get_size (text->text_area, NULL, &height);

  adj->step_increment = MIN (adj->upper, (float) SCROLL_PIXELS);
  adj->page_increment = MIN (adj->upper, height - (float) KEY_SCROLL_PIXELS);
  adj->page_size      = MIN (adj->upper, height);
  adj->value          = MIN (adj->value, adj->upper - adj->page_size);
  adj->value          = MAX (adj->value, 0.0);

  gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");
}

static gint
set_vertical_scroll_iterator (GtkText* text, LineParams* lp, void* data)
{
  gint *pixel_count = (gint*) data;

  if (text->first_line_start_index == lp->start.index)
    text->vadj->value = (float) *pixel_count;

  *pixel_count += LINE_HEIGHT (*lp);

  return FALSE;
}

static gint
set_vertical_scroll_find_iterator (GtkText* text, LineParams* lp, void* data)
{
  SetVerticalScrollData *svdata = (SetVerticalScrollData *) data;
  gint return_val;

  if (svdata->last_didnt_wrap)
    svdata->last_line_start = lp->start.index;

  if (svdata->pixel_height <= (gint) text->vadj->value &&
      svdata->pixel_height + LINE_HEIGHT(*lp) > (gint) text->vadj->value)
    {
      svdata->mark = lp->start;

      text->first_cut_pixels = (gint)text->vadj->value - svdata->pixel_height;
      text->first_onscreen_ver_pixel = svdata->pixel_height;
      text->first_line_start_index = svdata->last_line_start;

      return_val = TRUE;
    }
  else
    {
      svdata->pixel_height += LINE_HEIGHT (*lp);

      return_val = FALSE;
    }

  if (!lp->wraps)
    svdata->last_didnt_wrap = TRUE;
  else
    svdata->last_didnt_wrap = FALSE;

  return return_val;
}

static GtkPropertyMark
set_vertical_scroll (GtkText* text)
{
  GtkPropertyMark mark = find_mark (text, 0);
  SetVerticalScrollData data;
  gint height;
  gint pixel_count = 0;
  gint orig_value;

  line_params_iterate (text, &mark, NULL, FALSE, &pixel_count, set_vertical_scroll_iterator);

  text->vadj->upper = (float) pixel_count;
  orig_value = (gint) text->vadj->value;

  gdk_window_get_size (text->text_area, NULL, &height);

  text->vadj->step_increment = MIN (text->vadj->upper, (float) SCROLL_PIXELS);
  text->vadj->page_increment = MIN (text->vadj->upper, height - (float) KEY_SCROLL_PIXELS);
  text->vadj->page_size      = MIN (text->vadj->upper, height);
  text->vadj->value          = MIN (text->vadj->value, text->vadj->upper - text->vadj->page_size);
  text->vadj->value          = MAX (text->vadj->value, 0.0);

  text->last_ver_value = (gint)text->vadj->value;
  text->first_cut_pixels = 0;

  gtk_signal_emit_by_name (GTK_OBJECT (text->vadj), "changed");

  if (text->vadj->value != orig_value)
    {
      /* We got clipped, and don't really know which line to put first. */
      data.pixel_height = 0;
      data.last_didnt_wrap = TRUE;

      line_params_iterate (text, &mark, NULL,
			   FALSE, &data,
			   set_vertical_scroll_find_iterator);

      return data.mark;
    }
  else
    {
      return find_mark (text, text->first_line_start_index);
    }
}

static void
scroll_int (GtkText* text, gint diff)
{
  gfloat upper;

  text->vadj->value += diff;

  upper = text->vadj->upper - text->vadj->page_size;
  text->vadj->value = MIN (text->vadj->value, upper);
  text->vadj->value = MAX (text->vadj->value, 0.0);

  gtk_signal_emit_by_name (GTK_OBJECT (text->vadj), "value_changed");
}

static void 
process_exposes (GtkText *text)
{
  GdkEvent *event;

  /* Make sure graphics expose events are processed before scrolling
   * again */

  while ((event = gdk_event_get_graphics_expose (text->text_area)) != NULL)
    {
      gtk_widget_event (GTK_WIDGET (text), event);
      if (event->expose.count == 0)
	{
	  gdk_event_free (event);
	  break;
	}
      gdk_event_free (event);
    }
}

static gint last_visible_line_height (GtkText* text)
{
  GList *cache = text->line_start_cache;
  gint height;

  gdk_window_get_size (text->text_area, NULL, &height);

  for (; cache->next; cache = cache->next)
    if (pixel_height_of(text, cache->next) > height)
      break;

  if (cache)
    return pixel_height_of(text, cache) - 1;
  else
    return 0;
}

static gint first_visible_line_height (GtkText* text)
{
  if (text->first_cut_pixels)
    return pixel_height_of(text, text->line_start_cache) + 1;
  else
    return 1;
}

static void
scroll_down (GtkText* text, gint diff0)
{
  GdkRectangle rect;
  gint real_diff = 0;
  gint width, height;

  text->first_onscreen_ver_pixel += diff0;

  while (diff0-- > 0)
    {
      g_assert (text->line_start_cache &&
		text->line_start_cache->next);

      if (text->first_cut_pixels < LINE_HEIGHT(CACHE_DATA(text->line_start_cache)) - 1)
	{
	  text->first_cut_pixels += 1;
	}
      else
	{
	  text->first_cut_pixels = 0;

	  text->line_start_cache = text->line_start_cache->next;

	  text->first_line_start_index =
	    CACHE_DATA(text->line_start_cache).start.index;

	  if (!text->line_start_cache->next)
	    fetch_lines_forward (text, 1);
	}

      real_diff += 1;
    }

  gdk_window_get_size (text->text_area, &width, &height);
  if (height > real_diff)
    gdk_draw_pixmap (text->text_area,
		     text->gc,
		     text->text_area,
		     0,
		     real_diff,
		     0,
		     0,
		     width,
		     height - real_diff);

  rect.x      = 0;
  rect.y      = MAX (0, height - real_diff);
  rect.width  = width;
  rect.height = MIN (height, real_diff);

  expose_text (text, &rect, FALSE);
  gtk_text_draw_focus ( (GtkWidget *) text);

  if (text->current_line)
    {
      gint cursor_min;

      text->cursor_pos_y -= real_diff;
      cursor_min = drawn_cursor_min(text);

      if (cursor_min < 0)
	{
	  GdkEventButton button;

	  button.x = text->cursor_pos_x;
	  button.y = first_visible_line_height (text);

	  mouse_click_1 (text, &button);
	}
    }

  if (height > real_diff)
    process_exposes (text);
}

static void
scroll_up (GtkText* text, gint diff0)
{
  gint real_diff = 0;
  GdkRectangle rect;
  gint width, height;

  text->first_onscreen_ver_pixel += diff0;

  while (diff0++ < 0)
    {
      g_assert (text->line_start_cache);

      if (text->first_cut_pixels > 0)
	{
	  text->first_cut_pixels -= 1;
	}
      else
	{
	  if (!text->line_start_cache->prev)
	    fetch_lines_backward (text);

	  text->line_start_cache = text->line_start_cache->prev;

	  text->first_line_start_index =
	    CACHE_DATA(text->line_start_cache).start.index;

	  text->first_cut_pixels = LINE_HEIGHT(CACHE_DATA(text->line_start_cache)) - 1;
	}

      real_diff += 1;
    }

  gdk_window_get_size (text->text_area, &width, &height);
  if (height > real_diff)
    gdk_draw_pixmap (text->text_area,
		     text->gc,
		     text->text_area,
		     0,
		     0,
		     0,
		     real_diff,
		     width,
		     height - real_diff);

  rect.x      = 0;
  rect.y      = 0;
  rect.width  = width;
  rect.height = MIN (height, real_diff);

  expose_text (text, &rect, FALSE);
  gtk_text_draw_focus ( (GtkWidget *) text);

  if (text->current_line)
    {
      gint cursor_max;
      gint height;

      text->cursor_pos_y += real_diff;
      cursor_max = drawn_cursor_max(text);
      gdk_window_get_size (text->text_area, NULL, &height);

      if (cursor_max >= height)
	{
	  GdkEventButton button;

	  button.x = text->cursor_pos_x;
	  button.y = last_visible_line_height(text);

	  mouse_click_1 (text, &button);
	}
    }

  if (height > real_diff)
    process_exposes (text);
}

/**********************************************************************/
/*			      Display Code                            */
/**********************************************************************/

/* Assumes mark starts a line.  Calculates the height, width, and
 * displayable character count of a single DISPLAYABLE line.  That
 * means that in line-wrap mode, this does may not compute the
 * properties of an entire line. */
static LineParams
find_line_params (GtkText* text,
		  const GtkPropertyMark* mark,
		  const PrevTabCont *tab_cont,
		  PrevTabCont *next_cont)
{
  LineParams lp;
  TabStopMark tab_mark = tab_cont->tab_start;
  guint max_display_pixels;
  gchar ch;
  gint ch_width;
  GdkFont *font;

  gdk_window_get_size (text->text_area, (gint*) &max_display_pixels, NULL);
  max_display_pixels -= LINE_WRAP_ROOM;

  lp.wraps             = 0;
  lp.tab_cont          = *tab_cont;
  lp.start             = *mark;
  lp.end               = *mark;
  lp.pixel_width       = tab_cont->pixel_offset;
  lp.displayable_chars = 0;
  lp.font_ascent       = 0;
  lp.font_descent      = 0;

  init_tab_cont (text, next_cont);

  while (!LAST_INDEX(text, lp.end))
    {
      g_assert (lp.end.property);

      ch   = TEXT_INDEX (text, lp.end.index);
      font = MARK_CURRENT_FONT (&lp.end);

      if (ch == LINE_DELIM)
	{
	  /* Newline doesn't count in computation of line height, even
	   * if its in a bigger font than the rest of the line.  Unless,
	   * of course, there are no other characters. */

	  if (!lp.font_ascent && !lp.font_descent)
	    {
	      lp.font_ascent = font->ascent;
	      lp.font_descent = font->descent;
	    }

	  lp.tab_cont_next = *next_cont;

	  return lp;
	}

      ch_width = find_char_width (text, &lp.end, &tab_mark);

      if (ch_width + lp.pixel_width > max_display_pixels)
	{
	  lp.wraps = 1;

	  if (text->line_wrap)
	    {
	      next_cont->tab_start    = tab_mark;
	      next_cont->pixel_offset = 0;

	      if (ch == '\t')
		{
		  /* Here's the tough case, a tab is wrapping. */
		  gint pixels_avail = max_display_pixels - lp.pixel_width;
		  gint space_width  = MARK_CURRENT_TEXT_FONT(&lp.end)->char_widths[' '];
		  gint spaces_avail = pixels_avail / space_width;

		  if (spaces_avail == 0)
		    {
		      decrement_mark (&lp.end);
		    }
		  else
		    {
		      advance_tab_mark (text, &next_cont->tab_start, '\t');
		      next_cont->pixel_offset = space_width * (tab_mark.to_next_tab -
							       spaces_avail);
		      lp.displayable_chars += 1;
		    }
		}
	      else
		{
		  /* Don't include this character, it will wrap. */
		  decrement_mark (&lp.end);
		}

	      lp.tab_cont_next = *next_cont;

	      return lp;
	    }
	}
      else
	{
	  lp.displayable_chars += 1;
	}

      lp.font_ascent = MAX (font->ascent, lp.font_ascent);
      lp.font_descent = MAX (font->descent, lp.font_descent);
      lp.pixel_width  += ch_width;

      advance_mark(&lp.end);
      advance_tab_mark (text, &tab_mark, ch);
    }

  if (LAST_INDEX(text, lp.start))
    {
      /* Special case, empty last line. */
      font = MARK_CURRENT_FONT (&lp.end);

      lp.font_ascent = font->ascent;
      lp.font_descent = font->descent;
    }

  lp.tab_cont_next = *next_cont;

  return lp;
}

static void
expand_scratch_buffer (GtkText* text, guint len)
{
  if (len >= text->scratch_buffer_len)
    {
      guint i = 1;

      while (i <= len && i < MIN_GAP_SIZE) i <<= 1;

      if (text->scratch_buffer)
	text->scratch_buffer = g_new (guchar, i);
      else
	text->scratch_buffer = g_realloc (text->scratch_buffer, i);

      text->scratch_buffer_len = i;
    }
}

static void
draw_line (GtkText* text,
	   gint pixel_start_height,
	   LineParams* lp)
{
  GdkGCValues gc_values;
  gint i;
  gint len = 0;
  guint running_offset = lp->tab_cont.pixel_offset;
  guchar* buffer;

  GtkPropertyMark mark = lp->start;
  TabStopMark tab_mark = lp->tab_cont.tab_start;
  gint pixel_height = pixel_start_height + lp->font_ascent;
  guint chars = lp->displayable_chars;

  /* First provide a contiguous segment of memory.  This makes reading
   * the code below *much* easier, and only incurs the cost of copying
   * when the line being displayed spans the gap. */
  if (mark.index <= text->gap_position &&
      mark.index + chars > text->gap_position)
    {
      expand_scratch_buffer (text, chars);

      for (i = 0; i < chars; i += 1)
	text->scratch_buffer[i] = TEXT_INDEX(text, mark.index + i);

      buffer = text->scratch_buffer;
    }
  else
    {
      if (mark.index >= text->gap_position)
	buffer = text->text + mark.index + text->gap_size;
      else
	buffer = text->text + mark.index;
    }

  if (running_offset > 0 && MARK_CURRENT_BACK (&mark))
    {
      gdk_gc_set_foreground (text->gc, MARK_CURRENT_BACK (&mark));

      gdk_draw_rectangle (text->text_area,
			  text->gc,
			  TRUE,
			  0,
			  pixel_start_height,
			  running_offset,
			  LINE_HEIGHT (*lp));
    }

  for (; chars > 0; chars -= len, buffer += len, len = 0)
    {
      if (buffer[0] != '\t')
	{
	  guchar* next_tab = memchr (buffer, '\t', chars);
	  gint pixel_width;
	  GdkFont *font;

	  len = MIN (MARK_CURRENT_PROPERTY (&mark)->length - mark.offset, chars);

	  if (next_tab)
	    len = MIN (len, next_tab - buffer);

	  font = MARK_CURRENT_PROPERTY (&mark)->font->gdk_font;
	  if (font->type == GDK_FONT_FONT)
	    {
	      gdk_gc_set_font (text->gc, font);
	      gdk_gc_get_values (text->gc, &gc_values);
	      pixel_width = gdk_text_width (gc_values.font,
					    (gchar*) buffer, len);
	    }
	  else
	    pixel_width = gdk_text_width (font, (gchar*) buffer, len);

	  if (MARK_CURRENT_BACK (&mark))
	    {
	      gdk_gc_set_foreground (text->gc, MARK_CURRENT_BACK (&mark));

	      gdk_draw_rectangle (text->text_area,
				  text->gc,
				  TRUE,
				  running_offset,
				  pixel_start_height,
				  pixel_width,
				  LINE_HEIGHT(*lp));
	    }

	  gdk_gc_set_foreground (text->gc, MARK_CURRENT_FORE (&mark));

	  gdk_draw_text (text->text_area, MARK_CURRENT_FONT (&mark),
			 text->gc,
			 running_offset,
			 pixel_height,
			 (gchar*) buffer,
			 len);

	  running_offset += pixel_width;

	  advance_tab_mark_n (text, &tab_mark, len);
	}
      else
	{
	  len = 1;

	  if (MARK_CURRENT_BACK (&mark))
	    {
	      gint pixels_remaining;
	      gint space_width;
	      gint spaces_avail;

	      gdk_window_get_size (text->text_area, &pixels_remaining, NULL);
	      pixels_remaining -= (LINE_WRAP_ROOM + running_offset);

	      space_width = MARK_CURRENT_TEXT_FONT(&mark)->char_widths[' '];

	      spaces_avail = pixels_remaining / space_width;
	      spaces_avail = MIN (spaces_avail, tab_mark.to_next_tab);

	      gdk_gc_set_foreground (text->gc, MARK_CURRENT_BACK (&mark));

	      gdk_draw_rectangle (text->text_area,
				  text->gc,
				  TRUE,
				  running_offset,
				  pixel_start_height,
				  spaces_avail * space_width,
				  LINE_HEIGHT (*lp));
	    }

	  running_offset += tab_mark.to_next_tab *
	    MARK_CURRENT_TEXT_FONT(&mark)->char_widths[' '];

	  advance_tab_mark (text, &tab_mark, '\t');
	}

      advance_mark_n (&mark, len);
    }
}

static void
draw_line_wrap (GtkText* text, guint height /* baseline height */)
{
  gint width;
  GdkPixmap *bitmap;
  gint bitmap_width;
  gint bitmap_height;

  if (text->line_wrap)
    {
      bitmap = text->line_wrap_bitmap;
      bitmap_width = line_wrap_width;
      bitmap_height = line_wrap_height;
    }
  else
    {
      bitmap = text->line_arrow_bitmap;
      bitmap_width = line_arrow_width;
      bitmap_height = line_arrow_height;
    }

  gdk_window_get_size (text->text_area, &width, NULL);
  width -= LINE_WRAP_ROOM;

  gdk_gc_set_stipple (text->gc,
		      bitmap);

  gdk_gc_set_fill (text->gc, GDK_STIPPLED);

  gdk_gc_set_foreground (text->gc, &text->widget.style->fg[GTK_STATE_NORMAL]);

  gdk_gc_set_ts_origin (text->gc,
			width + 1,
			height - bitmap_height - 1);

  gdk_draw_rectangle (text->text_area,
		      text->gc,
		      TRUE,
		      width + 1,
		      height - bitmap_height - 1 /* one pixel above the baseline. */,
		      bitmap_width,
		      bitmap_height);

  gdk_gc_set_ts_origin (text->gc, 0, 0);

  gdk_gc_set_fill (text->gc, GDK_SOLID);
}

static void
undraw_cursor (GtkText* text, gint absolute)
{
  TDEBUG (("in undraw_cursor\n"));

  if (absolute)
    text->cursor_drawn_level = 0;

  if (text->has_cursor && (text->cursor_drawn_level ++ == 0))
    {
      GdkFont* font;

      g_assert(text->cursor_mark.property);

      font = MARK_CURRENT_FONT(&text->cursor_mark);

      if (text->widget.style->bg_pixmap[GTK_STATE_NORMAL])
	{
	  GdkRectangle rect;

	  rect.x = text->cursor_pos_x;
	  rect.y = text->cursor_pos_y - text->cursor_char_offset - font->ascent;
	  rect.width = 1;
	  rect.height = font->ascent + 1; /* @@@ I add one here because draw_line is inclusive, right? */

	  clear_area (text, &rect);
	}
      else
	{
	  if (MARK_CURRENT_BACK (&text->cursor_mark))
	    gdk_gc_set_foreground (text->gc, MARK_CURRENT_BACK (&text->cursor_mark));
	  else
	    gdk_gc_set_foreground (text->gc, &text->widget.style->bg[GTK_STATE_NORMAL]);

	  gdk_draw_line (text->text_area, text->gc, text->cursor_pos_x,
			 text->cursor_pos_y - text->cursor_char_offset, text->cursor_pos_x,
			 text->cursor_pos_y - text->cursor_char_offset - font->ascent);
	}

      if (text->cursor_char)
	{
	  if (font->type == GDK_FONT_FONT)
	    gdk_gc_set_font (text->gc, font);

	  gdk_gc_set_foreground (text->gc, MARK_CURRENT_FORE (&text->cursor_mark));

	  gdk_draw_text (text->text_area, font,
			 text->gc,
			 text->cursor_pos_x,
			 text->cursor_pos_y - text->cursor_char_offset,
			 &text->cursor_char,
			 1);
	}
    }
}

static gint
drawn_cursor_min (GtkText* text)
{
  if (text->has_cursor)
    {
      GdkFont* font;

      g_assert(text->cursor_mark.property);

      font = MARK_CURRENT_FONT(&text->cursor_mark);

      return text->cursor_pos_y - text->cursor_char_offset - font->ascent;
    }
  else
    return 0;
}

static gint
drawn_cursor_max (GtkText* text)
{
  if (text->has_cursor)
    {
      GdkFont* font;

      g_assert(text->cursor_mark.property);

      font = MARK_CURRENT_FONT(&text->cursor_mark);

      return text->cursor_pos_y - text->cursor_char_offset;
    }
  else
    return 0;
}

static void
draw_cursor (GtkText* text, gint absolute)
{
  TDEBUG (("in draw_cursor\n"));

  if (absolute)
    text->cursor_drawn_level = 1;

  if (text->has_cursor && (--text->cursor_drawn_level == 0))
    {
      GdkFont* font;

      g_assert (text->cursor_mark.property);

      font = MARK_CURRENT_FONT (&text->cursor_mark);

      gdk_gc_set_foreground (text->gc, &text->widget.style->fg[GTK_STATE_NORMAL]);

      gdk_draw_line (text->text_area, text->gc, text->cursor_pos_x,
		     text->cursor_pos_y - text->cursor_char_offset,
		     text->cursor_pos_x,
		     text->cursor_pos_y - text->cursor_char_offset - font->ascent);
    }
}

static void
clear_area (GtkText *text, GdkRectangle *area)
{
  if (text->widget.style->bg_pixmap[GTK_STATE_NORMAL])
    {
      gint width, height;
      gint x = area->x, y = area->y;
      gint xorig, yorig;

      gdk_window_get_size (text->widget.style->bg_pixmap[GTK_STATE_NORMAL], &width, &height);

      yorig = - text->first_onscreen_ver_pixel;
      xorig = - text->first_onscreen_hor_pixel;

      for (y = area->y; y < area->y + area->height; )
	{
	  gint yoff = (y - yorig) % height;
	  gint yw = MIN(height - yoff, (area->y + area->height) - y);

	  for (x = area->x; x < area->x + area->width; )
	    {
	      gint xoff = (x - xorig) % width;
	      gint xw = MIN(width - xoff, (area->x + area->width) - x);

	      gdk_draw_pixmap (text->text_area,
			       text->gc,
			       text->widget.style->bg_pixmap[GTK_STATE_NORMAL],
			       xoff,
			       yoff,
			       x,
			       y,
			       xw,
			       yw);

	      x += width - xoff;
	    }
	  y += height - yoff;
	}
    }
  else
    gdk_window_clear_area (text->text_area, area->x, area->y, area->width, area->height);
}

static void
expose_text (GtkText* text, GdkRectangle *area, gboolean cursor)
{
  GList *cache = text->line_start_cache;
  gint pixels = - text->first_cut_pixels;
  gint min_y = area->y;
  gint max_y = area->y + area->height;
  gint height;

  gdk_window_get_size (text->text_area, NULL, &height);
  max_y = MIN (max_y, height);

  TDEBUG (("in expose x=%d y=%d w=%d h=%d\n", area->x, area->y, area->width, area->height));

  clear_area (text, area);

  for (; pixels < height; cache = cache->next)
    {
      if (pixels < max_y && (pixels + LINE_HEIGHT(CACHE_DATA(cache))) >= min_y)
	{
	  draw_line (text, pixels, &CACHE_DATA(cache));

	  if (CACHE_DATA(cache).wraps)
	    draw_line_wrap (text, pixels + CACHE_DATA(cache).font_ascent);
	}

      if (cursor && text->has_cursor && GTK_WIDGET_HAS_FOCUS (&text->widget))
	{
	  if (CACHE_DATA(cache).start.index <= text->cursor_mark.index &&
	      CACHE_DATA(cache).end.index >= text->cursor_mark.index)
	    draw_cursor (text, TRUE);
	}

      pixels += LINE_HEIGHT(CACHE_DATA(cache));

      if (!cache->next)
	{
	  fetch_lines_forward (text, 1);

	  if (!cache->next)
	    break;
	}
    }
}

static void
recompute_geometry (GtkText* text)
{
  GtkPropertyMark start_mark;
  gint height;
  gint width;

  free_cache (text);

  start_mark = set_vertical_scroll (text);

  gdk_window_get_size (text->text_area, &width, &height);

  text->line_start_cache = fetch_lines (text,
					&start_mark,
					NULL,
					FetchLinesPixels,
					height + text->first_cut_pixels);

  find_cursor (text);
}

/**********************************************************************/
/*                            Selection                               */
/**********************************************************************/

static gint
gtk_text_selection_clear (GtkWidget         *widget,
			  GdkEventSelection *event)
{
#if 0
  GtkEntry *entry;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ENTRY (widget);

  if (entry->have_selection)
    {
      entry->have_selection = FALSE;
      gtk_entry_queue_draw (entry);
    }
#endif
  return FALSE;
}

static gint
gtk_text_selection_request (GtkWidget         *widget,
			    GdkEventSelection *event)
{
#if 0

  GtkEntry *entry;
  gint selection_start_pos;
  gint selection_end_pos;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ENTRY (widget);
  if (entry->selection_start_pos != entry->selection_end_pos)
    {
      selection_start_pos = MIN (entry->selection_start_pos, entry->selection_end_pos);
      selection_end_pos = MAX (entry->selection_start_pos, entry->selection_end_pos);

      gdk_selection_set (event->requestor, event->selection,
			 event->property, event->time,
			 (guchar*) &entry->text[selection_start_pos],
			 selection_end_pos - selection_start_pos);
    }
#endif
  return FALSE;
}

static gint
gtk_text_selection_notify (GtkWidget         *widget,
			   GdkEventSelection *event)
{
#if 0
  GtkEntry *entry;
  gchar *data;
  gint tmp_pos;
  gint reselect;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ENTRY (widget);

  gdk_selection_get (widget->window, (guchar**) &data);

  reselect = FALSE;
  if (entry->selection_start_pos != entry->selection_end_pos)
    {
      reselect = TRUE;
      gtk_delete_selection (entry);
    }

  tmp_pos = entry->current_pos;
  gtk_entry_insert_text (entry, data, strlen (data), &tmp_pos);

  if (reselect)
    {
      reselect = entry->have_selection;
      gtk_select_region (entry, entry->current_pos, tmp_pos);
      entry->have_selection = reselect;
    }

  entry->current_pos = tmp_pos;

  gtk_entry_queue_draw (entry);
#endif
  return FALSE;
}

/**********************************************************************/
/*                              Debug                                 */
/**********************************************************************/

#ifdef DEBUG_GTK_TEXT
static void
gtk_text_show_cache_line (GtkText *text, GList *cache,
			  const char* what, const char* func, gint line)
{
  LineParams *lp = &CACHE_DATA(cache);
  gint i;

  if (cache == text->line_start_cache)
    g_print ("Line Start Cache: ");

  if (cache == text->current_line)
    g_print("Current Line: ");

  g_print ("%s:%d: cache line %s s=%d,e=%d,lh=%d (",
	   func,
	   line,
	   what,
	   lp->start.index,
	   lp->end.index,
	   LINE_HEIGHT(*lp));

  for (i = lp->start.index; i < (lp->end.index + lp->wraps); i += 1)
    g_print ("%c", TEXT_INDEX (text, i));

  g_print (")\n");
}

static void
gtk_text_show_cache (GtkText *text, const char* func, gint line)
{
  GList *l = text->line_start_cache;

  if (!l) {
    return;
  }

  /* back up to the absolute beginning of the line cache */
  while (l->prev)
    l = l->prev;

  g_print ("*** line cache ***\n");
  for (; l; l = l->next)
    gtk_text_show_cache_line (text, l, "all", func, line);
}

static void
gtk_text_assert_mark (GtkText         *text,
		      GtkPropertyMark *mark,
		      GtkPropertyMark *before,
		      GtkPropertyMark *after,
		      const gchar     *msg,
		      const gchar     *where,
		      gint             line)
{
  GtkPropertyMark correct_mark = find_mark (text, mark->index);

  if (mark->offset != correct_mark.offset ||
      mark->property != correct_mark.property)
    g_warning ("incorrect %s text property marker in %s:%d, index %d -- bad!", where, msg, line, mark->index);
}

static void
gtk_text_assert (GtkText         *text,
		 const gchar     *msg,
		 gint             line)
{
  GList* cache = text->line_start_cache;
  GtkPropertyMark* before_mark = NULL;
  GtkPropertyMark* after_mark = NULL;

  gtk_text_show_props (text, msg, line);

  for (; cache->prev; cache = cache->prev)
    /* nothing */;

  g_print ("*** line markers ***\n");

  for (; cache; cache = cache->next)
    {
      after_mark = &CACHE_DATA(cache).end;
      gtk_text_assert_mark (text, &CACHE_DATA(cache).start, before_mark, after_mark, msg, "start", line);
      before_mark = &CACHE_DATA(cache).start;

      if (cache->next)
	after_mark = &CACHE_DATA(cache->next).start;
      else
	after_mark = NULL;

      gtk_text_assert_mark (text, &CACHE_DATA(cache).end, before_mark, after_mark, msg, "end", line);
      before_mark = &CACHE_DATA(cache).end;
    }
}

static void
gtk_text_show_adj (GtkText *text,
		   GtkAdjustment *adj,
		   const char* what,
		   const char* func,
		   gint line)
{
  g_print ("*** adjustment ***\n");

  g_print ("%s:%d: %s adjustment l=%.1f u=%.1f v=%.1f si=%.1f pi=%.1f ps=%.1f\n",
	   func,
	   line,
	   what,
	   adj->lower,
	   adj->upper,
	   adj->value,
	   adj->step_increment,
	   adj->page_increment,
	   adj->page_size);
}

static void
gtk_text_show_props (GtkText *text,
		     const char* msg,
		     int line)
{
  GList* props = text->text_properties;
  int proplen = 0;

  g_print ("%s:%d: ", msg, line);

  for (; props; props = props->next)
    {
      TextProperty *p = (TextProperty*)props->data;

      proplen += p->length;

      g_print ("[%d,%p,%p,%p,%p] ", p->length, p, p->font, p->fore_color, p->back_color);
    }

  g_print ("\n");

  if (proplen - 1 != TEXT_LENGTH(text))
    g_warning ("incorrect property list length in %s:%d -- bad!", msg, line);
}
#endif
