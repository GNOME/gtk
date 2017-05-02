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

#include "config.h"

#include <ctype.h>
#include <string.h>

#undef GDK_DISABLE_DEPRECATED

#include "gdk/gdkkeysyms.h"
#include "gdk/gdki18n.h"

#undef GTK_DISABLE_DEPRECATED

#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtkstyle.h"
#define GTK_ENABLE_BROKEN
#include "gtktext.h"
#include "line-wrap.xbm"
#include "line-arrow.xbm"
#include "gtkprivate.h"
#include "gtkintl.h"

#include "gtkalias.h"


#define INITIAL_BUFFER_SIZE      1024
#define INITIAL_LINE_CACHE_SIZE  256
#define MIN_GAP_SIZE             256
#define LINE_DELIM               '\n'
#define MIN_TEXT_WIDTH_LINES     20
#define MIN_TEXT_HEIGHT_LINES    10
#define TEXT_BORDER_ROOM         1
#define LINE_WRAP_ROOM           8           /* The bitmaps are 6 wide. */
#define DEFAULT_TAB_STOP_WIDTH   4
#define SCROLL_PIXELS            5
#define KEY_SCROLL_PIXELS        10
#define SCROLL_TIME              100
#define FREEZE_LENGTH            1024        
/* Freeze text when inserting or deleting more than this many characters */

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


#define MARK_CURRENT_FONT(text, mark) \
  ((MARK_CURRENT_PROPERTY(mark)->flags & PROPERTY_FONT) ? \
         MARK_CURRENT_PROPERTY(mark)->font->gdk_font : \
         gtk_style_get_font (GTK_WIDGET (text)->style))
#define MARK_CURRENT_FORE(text, mark) \
  ((MARK_CURRENT_PROPERTY(mark)->flags & PROPERTY_FOREGROUND) ? \
         &MARK_CURRENT_PROPERTY(mark)->fore_color : \
         &((GtkWidget *)text)->style->text[((GtkWidget *)text)->state])
#define MARK_CURRENT_BACK(text, mark) \
  ((MARK_CURRENT_PROPERTY(mark)->flags & PROPERTY_BACKGROUND) ? \
         &MARK_CURRENT_PROPERTY(mark)->back_color : \
         &((GtkWidget *)text)->style->base[((GtkWidget *)text)->state])
#define MARK_CURRENT_TEXT_FONT(text, mark) \
  ((MARK_CURRENT_PROPERTY(mark)->flags & PROPERTY_FONT) ? \
         MARK_CURRENT_PROPERTY(mark)->font : \
         text->current_font)

#define TEXT_LENGTH(t)              ((t)->text_end - (t)->gap_size)
#define FONT_HEIGHT(f)              ((f)->ascent + (f)->descent)
#define LINE_HEIGHT(l)              ((l).font_ascent + (l).font_descent)
#define LINE_CONTAINS(l, i)         ((l).start.index <= (i) && (l).end.index >= (i))
#define LINE_STARTS_AT(l, i)        ((l).start.index == (i))
#define LINE_START_PIXEL(l)         ((l).tab_cont.pixel_offset)
#define LAST_INDEX(t, m)            ((m).index == TEXT_LENGTH(t))
#define CACHE_DATA(c)               (*(LineParams*)(c)->data)

enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_LINE_WRAP,
  PROP_WORD_WRAP
};

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

struct _GtkTextFont
{
  /* The actual font. */
  GdkFont *gdk_font;
  guint ref_count;

  gint16 char_widths[256];
};

typedef enum {
  PROPERTY_FONT =       1 << 0,
  PROPERTY_FOREGROUND = 1 << 1,
  PROPERTY_BACKGROUND = 1 << 2
} TextPropertyFlags;

struct _TextProperty
{
  /* Font. */
  GtkTextFont* font;

  /* Background Color. */
  GdkColor back_color;
  
  /* Foreground Color. */
  GdkColor fore_color;

  /* Show which properties are set */
  TextPropertyFlags flags;

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
static void  gtk_text_set_property   (GObject         *object,
				      guint            prop_id,
				      const GValue    *value,
				      GParamSpec      *pspec);
static void  gtk_text_get_property   (GObject         *object,
				      guint            prop_id,
				      GValue          *value,
				      GParamSpec      *pspec);
static void  gtk_text_editable_init  (GtkEditableClass *iface);
static void  gtk_text_init           (GtkText        *text);
static void  gtk_text_destroy        (GtkObject      *object);
static void  gtk_text_finalize       (GObject        *object);
static void  gtk_text_realize        (GtkWidget      *widget);
static void  gtk_text_unrealize      (GtkWidget      *widget);
static void  gtk_text_style_set	     (GtkWidget      *widget,
				      GtkStyle       *previous_style);
static void  gtk_text_state_changed  (GtkWidget      *widget,
				      GtkStateType    previous_state);
static void  gtk_text_draw_focus     (GtkWidget      *widget);
static void  gtk_text_size_request   (GtkWidget      *widget,
				      GtkRequisition *requisition);
static void  gtk_text_size_allocate  (GtkWidget      *widget,
				      GtkAllocation  *allocation);
static void  gtk_text_adjustment     (GtkAdjustment  *adjustment,
				      GtkText        *text);
static void   gtk_text_insert_text       (GtkEditable    *editable,
					  const gchar    *new_text,
					  gint            new_text_length,
					  gint           *position);
static void   gtk_text_delete_text       (GtkEditable    *editable,
					  gint            start_pos,
					  gint            end_pos);
static void   gtk_text_update_text       (GtkOldEditable *old_editable,
					  gint            start_pos,
					  gint            end_pos);
static gchar *gtk_text_get_chars         (GtkOldEditable *old_editable,
					  gint            start,
					  gint            end);
static void   gtk_text_set_selection     (GtkOldEditable *old_editable,
					  gint            start,
					  gint            end);
static void   gtk_text_real_set_editable (GtkOldEditable *old_editable,
					  gboolean        is_editable);

static void  gtk_text_adjustment_destroyed (GtkAdjustment  *adjustment,
                                            GtkText        *text);

/* Event handlers */
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

static void move_gap (GtkText* text, guint index);
static void make_forward_space (GtkText* text, guint len);

/* Property management */
static GtkTextFont* get_text_font (GdkFont* gfont);
static void         text_font_unref (GtkTextFont *text_font);

static void insert_text_property (GtkText* text, GdkFont* font,
				  const GdkColor *fore, const GdkColor* back, guint len);
static TextProperty* new_text_property (GtkText *text, GdkFont* font, 
					const GdkColor* fore, const GdkColor* back, guint length);
static void destroy_text_property (TextProperty *prop);
static void init_properties      (GtkText *text);
static void realize_property     (GtkText *text, TextProperty *prop);
static void realize_properties   (GtkText *text);
static void unrealize_property   (GtkText *text, TextProperty *prop);
static void unrealize_properties (GtkText *text);

static void delete_text_property (GtkText* text, guint len);

static guint pixel_height_of (GtkText* text, GList* cache_line);

/* Property Movement and Size Computations */
static void advance_mark (GtkPropertyMark* mark);
static void decrement_mark (GtkPropertyMark* mark);
static void advance_mark_n (GtkPropertyMark* mark, gint n);
static void decrement_mark_n (GtkPropertyMark* mark, gint n);
static void move_mark_n (GtkPropertyMark* mark, gint n);
static GtkPropertyMark find_mark (GtkText* text, guint mark_position);
static GtkPropertyMark find_mark_near (GtkText* text, guint mark_position, const GtkPropertyMark* near);
static void find_line_containing_point (GtkText* text, guint point,
					gboolean scroll);

/* Display */
static void compute_lines_pixels (GtkText* text, guint char_count,
				  guint *lines, guint *pixels);

static gint total_line_height (GtkText* text,
			       GList* line,
			       gint line_count);
static LineParams find_line_params (GtkText* text,
				    const GtkPropertyMark *mark,
				    const PrevTabCont *tab_cont,
				    PrevTabCont *next_cont);
static void recompute_geometry (GtkText* text);
static void insert_expose (GtkText* text, guint old_pixels, gint nchars, guint new_line_count);
static void delete_expose (GtkText* text,
			   guint nchars,
			   guint old_lines, 
			   guint old_pixels);
static GdkGC *create_bg_gc (GtkText *text);
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
static void find_cursor (GtkText* text,
			 gboolean scroll);
static void find_cursor_at_line (GtkText* text,
				 const LineParams* start_line,
				 gint pixel_height);
static void find_mouse_cursor (GtkText* text, gint x, gint y);

/* Scrolling. */
static void adjust_adj  (GtkText* text, GtkAdjustment* adj);
static void scroll_up   (GtkText* text, gint diff);
static void scroll_down (GtkText* text, gint diff);
static void scroll_int  (GtkText* text, gint diff);

static void process_exposes (GtkText *text);

/* Cache Management. */
static void   free_cache        (GtkText* text);
static GList* remove_cache_line (GtkText* text, GList* list);

/* Key Motion. */
static void move_cursor_buffer_ver (GtkText *text, int dir);
static void move_cursor_page_ver (GtkText *text, int dir);
static void move_cursor_ver (GtkText *text, int count);
static void move_cursor_hor (GtkText *text, int count);

/* Binding actions */
static void gtk_text_move_cursor    (GtkOldEditable *old_editable,
				     gint            x,
				     gint            y);
static void gtk_text_move_word      (GtkOldEditable *old_editable,
				     gint            n);
static void gtk_text_move_page      (GtkOldEditable *old_editable,
				     gint            x,
				     gint            y);
static void gtk_text_move_to_row    (GtkOldEditable *old_editable,
				     gint            row);
static void gtk_text_move_to_column (GtkOldEditable *old_editable,
				     gint            row);
static void gtk_text_kill_char      (GtkOldEditable *old_editable,
				     gint            direction);
static void gtk_text_kill_word      (GtkOldEditable *old_editable,
				     gint            direction);
static void gtk_text_kill_line      (GtkOldEditable *old_editable,
				     gint            direction);

/* To be removed */
static void gtk_text_move_forward_character    (GtkText          *text);
static void gtk_text_move_backward_character   (GtkText          *text);
static void gtk_text_move_forward_word         (GtkText          *text);
static void gtk_text_move_backward_word        (GtkText          *text);
static void gtk_text_move_beginning_of_line    (GtkText          *text);
static void gtk_text_move_end_of_line          (GtkText          *text);
static void gtk_text_move_next_line            (GtkText          *text);
static void gtk_text_move_previous_line        (GtkText          *text);

static void gtk_text_delete_forward_character  (GtkText          *text);
static void gtk_text_delete_backward_character (GtkText          *text);
static void gtk_text_delete_forward_word       (GtkText          *text);
static void gtk_text_delete_backward_word      (GtkText          *text);
static void gtk_text_delete_line               (GtkText          *text);
static void gtk_text_delete_to_line_end        (GtkText          *text);
static void gtk_text_select_word               (GtkText          *text,
						guint32           time);
static void gtk_text_select_line               (GtkText          *text,
						guint32           time);

static void gtk_text_set_position (GtkOldEditable *old_editable,
				   gint            position);

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

#define TDEBUG(args) g_message args
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

static GtkWidgetClass *parent_class = NULL;

/**********************************************************************/
/*			        Widget Crap                           */
/**********************************************************************/

GtkType
gtk_text_get_type (void)
{
  static GtkType text_type = 0;
  
  if (!text_type)
    {
      static const GtkTypeInfo text_info =
      {
	"GtkText",
	sizeof (GtkText),
	sizeof (GtkTextClass),
	(GtkClassInitFunc) gtk_text_class_init,
	(GtkObjectInitFunc) gtk_text_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      const GInterfaceInfo editable_info =
      {
	(GInterfaceInitFunc) gtk_text_editable_init, /* interface_init */
	NULL, /* interface_finalize */
	NULL  /* interface_data */
      };
      
      I_("GtkText");
      text_type = gtk_type_unique (GTK_TYPE_OLD_EDITABLE, &text_info);
      g_type_add_interface_static (text_type,
				   GTK_TYPE_EDITABLE,
				   &editable_info);
    }
  
  return text_type;
}

static void
gtk_text_class_init (GtkTextClass *class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkOldEditableClass *old_editable_class;

  gobject_class = G_OBJECT_CLASS (class);
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  old_editable_class = (GtkOldEditableClass*) class;
  parent_class = gtk_type_class (GTK_TYPE_OLD_EDITABLE);

  gobject_class->finalize = gtk_text_finalize;
  gobject_class->set_property = gtk_text_set_property;
  gobject_class->get_property = gtk_text_get_property;
  
  object_class->destroy = gtk_text_destroy;
  
  widget_class->realize = gtk_text_realize;
  widget_class->unrealize = gtk_text_unrealize;
  widget_class->style_set = gtk_text_style_set;
  widget_class->state_changed = gtk_text_state_changed;
  widget_class->size_request = gtk_text_size_request;
  widget_class->size_allocate = gtk_text_size_allocate;
  widget_class->expose_event = gtk_text_expose;
  widget_class->button_press_event = gtk_text_button_press;
  widget_class->button_release_event = gtk_text_button_release;
  widget_class->motion_notify_event = gtk_text_motion_notify;
  widget_class->key_press_event = gtk_text_key_press;

  old_editable_class->set_editable = gtk_text_real_set_editable;
  
  old_editable_class->move_cursor = gtk_text_move_cursor;
  old_editable_class->move_word = gtk_text_move_word;
  old_editable_class->move_page = gtk_text_move_page;
  old_editable_class->move_to_row = gtk_text_move_to_row;
  old_editable_class->move_to_column = gtk_text_move_to_column;
  
  old_editable_class->kill_char = gtk_text_kill_char;
  old_editable_class->kill_word = gtk_text_kill_word;
  old_editable_class->kill_line = gtk_text_kill_line;
  
  old_editable_class->update_text = gtk_text_update_text;
  old_editable_class->get_chars   = gtk_text_get_chars;
  old_editable_class->set_selection = gtk_text_set_selection;
  old_editable_class->set_position = gtk_text_set_position;

  class->set_scroll_adjustments = gtk_text_set_adjustments;

  g_object_class_install_property (gobject_class,
                                   PROP_HADJUSTMENT,
                                   g_param_spec_object ("hadjustment",
                                                        P_("Horizontal Adjustment"),
                                                        P_("Horizontal adjustment for the text widget"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_VADJUSTMENT,
                                   g_param_spec_object ("vadjustment",
                                                        P_("Vertical Adjustment"),
                                                        P_("Vertical adjustment for the text widget"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_LINE_WRAP,
                                   g_param_spec_boolean ("line-wrap",
							 P_("Line Wrap"),
							 P_("Whether lines are wrapped at widget edges"),
							 TRUE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_WORD_WRAP,
                                   g_param_spec_boolean ("word-wrap",
							 P_("Word Wrap"),
							 P_("Whether words are wrapped at widget edges"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  widget_class->set_scroll_adjustments_signal =
    gtk_signal_new (I_("set-scroll-adjustments"),
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTextClass, set_scroll_adjustments),
		    _gtk_marshal_VOID__OBJECT_OBJECT,
		    GTK_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);
}

static void
gtk_text_set_property (GObject         *object,
		       guint            prop_id,
		       const GValue    *value,
		       GParamSpec      *pspec)
{
  GtkText *text;
  
  text = GTK_TEXT (object);
  
  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      gtk_text_set_adjustments (text,
				g_value_get_object (value),
				text->vadj);
      break;
    case PROP_VADJUSTMENT:
      gtk_text_set_adjustments (text,
				text->hadj,
				g_value_get_object (value));
      break;
    case PROP_LINE_WRAP:
      gtk_text_set_line_wrap (text, g_value_get_boolean (value));
      break;
    case PROP_WORD_WRAP:
      gtk_text_set_word_wrap (text, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_text_get_property (GObject         *object,
		       guint            prop_id,
		       GValue          *value,
		       GParamSpec      *pspec)
{
  GtkText *text;
  
  text = GTK_TEXT (object);
  
  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, text->hadj);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, text->vadj);
      break;
    case PROP_LINE_WRAP:
      g_value_set_boolean (value, text->line_wrap);
      break;
    case PROP_WORD_WRAP:
      g_value_set_boolean (value, text->word_wrap);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_text_editable_init (GtkEditableClass *iface)
{
  iface->insert_text = gtk_text_insert_text;
  iface->delete_text = gtk_text_delete_text;
}

static void
gtk_text_init (GtkText *text)
{
  gtk_widget_set_can_focus (GTK_WIDGET (text), TRUE);

  text->text_area = NULL;
  text->hadj = NULL;
  text->vadj = NULL;
  text->gc = NULL;
  text->bg_gc = NULL;
  text->line_wrap_bitmap = NULL;
  text->line_arrow_bitmap = NULL;
  
  text->use_wchar = FALSE;
  text->text.ch = g_new (guchar, INITIAL_BUFFER_SIZE);
  text->text_len = INITIAL_BUFFER_SIZE;
 
  text->scratch_buffer.ch = NULL;
  text->scratch_buffer_len = 0;
 
  text->freeze_count = 0;
  
  text->default_tab_width = 4;
  text->tab_stops = NULL;
  
  text->tab_stops = g_list_prepend (text->tab_stops, (void*)8);
  text->tab_stops = g_list_prepend (text->tab_stops, (void*)8);
  
  text->line_start_cache = NULL;
  text->first_cut_pixels = 0;
  
  text->line_wrap = TRUE;
  text->word_wrap = FALSE;
  
  text->timer = 0;
  text->button = 0;
  
  text->current_font = NULL;
  
  init_properties (text);
  
  GTK_OLD_EDITABLE (text)->editable = FALSE;
  
  gtk_text_set_adjustments (text, NULL, NULL);
  gtk_editable_set_position (GTK_EDITABLE (text), 0);
}

GtkWidget*
gtk_text_new (GtkAdjustment *hadj,
	      GtkAdjustment *vadj)
{
  GtkWidget *text;

  if (hadj)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (hadj), NULL);
  if (vadj)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (vadj), NULL);

  text = g_object_new (GTK_TYPE_TEXT,
			 "hadjustment", hadj,
			 "vadjustment", vadj,
			 NULL);

  return text;
}

void
gtk_text_set_word_wrap (GtkText *text,
			gboolean word_wrap)
{
  g_return_if_fail (GTK_IS_TEXT (text));
  
  text->word_wrap = (word_wrap != FALSE);
  
  if (gtk_widget_get_realized (GTK_WIDGET (text)))
    {
      recompute_geometry (text);
      gtk_widget_queue_draw (GTK_WIDGET (text));
    }
  
  g_object_notify (G_OBJECT (text), "word-wrap");
}

void
gtk_text_set_line_wrap (GtkText *text,
			gboolean line_wrap)
{
  g_return_if_fail (GTK_IS_TEXT (text));
  
  text->line_wrap = (line_wrap != FALSE);
  
  if (gtk_widget_get_realized (GTK_WIDGET (text)))
    {
      recompute_geometry (text);
      gtk_widget_queue_draw (GTK_WIDGET (text));
    }
  
  g_object_notify (G_OBJECT (text), "line-wrap");
}

void
gtk_text_set_editable (GtkText *text,
		       gboolean is_editable)
{
  g_return_if_fail (GTK_IS_TEXT (text));
  
  gtk_editable_set_editable (GTK_EDITABLE (text), is_editable);
}

static void
gtk_text_real_set_editable (GtkOldEditable *old_editable,
			    gboolean        is_editable)
{
  GtkText *text;
  
  g_return_if_fail (GTK_IS_TEXT (old_editable));
  
  text = GTK_TEXT (old_editable);
  
  old_editable->editable = (is_editable != FALSE);
  
  if (is_editable)
    draw_cursor (text, TRUE);
  else
    undraw_cursor (text, TRUE);
}

void
gtk_text_set_adjustments (GtkText       *text,
			  GtkAdjustment *hadj,
			  GtkAdjustment *vadj)
{
  g_return_if_fail (GTK_IS_TEXT (text));
  if (hadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
  else
    hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  if (vadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
  else
    vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  
  if (text->hadj && (text->hadj != hadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (text->hadj), text);
      g_object_unref (text->hadj);
    }
  
  if (text->vadj && (text->vadj != vadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (text->vadj), text);
      g_object_unref (text->vadj);
    }
  
  g_object_freeze_notify (G_OBJECT (text));
  if (text->hadj != hadj)
    {
      text->hadj = hadj;
      g_object_ref_sink (text->hadj);
      
      gtk_signal_connect (GTK_OBJECT (text->hadj), "changed",
			  G_CALLBACK (gtk_text_adjustment),
			  text);
      gtk_signal_connect (GTK_OBJECT (text->hadj), "value-changed",
			  G_CALLBACK (gtk_text_adjustment),
			  text);
      gtk_signal_connect (GTK_OBJECT (text->hadj), "destroy",
			  G_CALLBACK (gtk_text_adjustment_destroyed),
			  text);
      gtk_text_adjustment (hadj, text);

      g_object_notify (G_OBJECT (text), "hadjustment");
    }
  
  if (text->vadj != vadj)
    {
      text->vadj = vadj;
      g_object_ref_sink (text->vadj);
      
      gtk_signal_connect (GTK_OBJECT (text->vadj), "changed",
			  G_CALLBACK (gtk_text_adjustment),
			  text);
      gtk_signal_connect (GTK_OBJECT (text->vadj), "value-changed",
			  G_CALLBACK (gtk_text_adjustment),
			  text);
      gtk_signal_connect (GTK_OBJECT (text->vadj), "destroy",
			  G_CALLBACK (gtk_text_adjustment_destroyed),
			  text);
      gtk_text_adjustment (vadj, text);

      g_object_notify (G_OBJECT (text), "vadjustment");
    }
  g_object_thaw_notify (G_OBJECT (text));
}

void
gtk_text_set_point (GtkText *text,
		    guint    index)
{
  g_return_if_fail (GTK_IS_TEXT (text));
  g_return_if_fail (index <= TEXT_LENGTH (text));
  
  text->point = find_mark (text, index);
}

guint
gtk_text_get_point (GtkText *text)
{
  g_return_val_if_fail (GTK_IS_TEXT (text), 0);
  
  return text->point.index;
}

guint
gtk_text_get_length (GtkText *text)
{
  g_return_val_if_fail (GTK_IS_TEXT (text), 0);
  
  return TEXT_LENGTH (text);
}

void
gtk_text_freeze (GtkText *text)
{
  g_return_if_fail (GTK_IS_TEXT (text));
  
  text->freeze_count++;
}

void
gtk_text_thaw (GtkText *text)
{
  g_return_if_fail (GTK_IS_TEXT (text));
  
  if (text->freeze_count)
    if (!(--text->freeze_count) && gtk_widget_get_realized (GTK_WIDGET (text)))
      {
	recompute_geometry (text);
	gtk_widget_queue_draw (GTK_WIDGET (text));
      }
}

void
gtk_text_insert (GtkText        *text,
		 GdkFont        *font,
		 const GdkColor *fore,
		 const GdkColor *back,
		 const char     *chars,
		 gint            nchars)
{
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (text);
  gboolean frozen = FALSE;
  
  gint new_line_count = 1;
  guint old_height = 0;
  guint length;
  guint i;
  gint numwcs;
  
  g_return_if_fail (GTK_IS_TEXT (text));

  if (nchars < 0)
    length = strlen (chars);
  else
    length = nchars;
  
  if (length == 0)
    return;
  
  if (!text->freeze_count && (length > FREEZE_LENGTH))
    {
      gtk_text_freeze (text);
      frozen = TRUE;
    }
  
  if (!text->freeze_count && (text->line_start_cache != NULL))
    {
      find_line_containing_point (text, text->point.index, TRUE);
      old_height = total_line_height (text, text->current_line, 1);
    }
  
  if ((TEXT_LENGTH (text) == 0) && (text->use_wchar == FALSE))
    {
      GtkWidget *widget = GTK_WIDGET (text);
      
      gtk_widget_ensure_style (widget);
      if ((widget->style) &&
	  (gtk_style_get_font (widget->style)->type == GDK_FONT_FONTSET))
 	{
 	  text->use_wchar = TRUE;
 	  g_free (text->text.ch);
 	  text->text.wc = g_new (GdkWChar, INITIAL_BUFFER_SIZE);
 	  text->text_len = INITIAL_BUFFER_SIZE;
 	  g_free (text->scratch_buffer.ch);
 	  text->scratch_buffer.wc = NULL;
 	  text->scratch_buffer_len = 0;
 	}
    }
 
  move_gap (text, text->point.index);
  make_forward_space (text, length);
 
  if (text->use_wchar)
    {
      char *chars_nt = (char *)chars;
      if (nchars > 0)
	{
	  chars_nt = g_new (char, length+1);
	  memcpy (chars_nt, chars, length);
	  chars_nt[length] = 0;
	}
      numwcs = gdk_mbstowcs (text->text.wc + text->gap_position, chars_nt,
 			     length);
      if (chars_nt != chars)
	g_free(chars_nt);
      if (numwcs < 0)
	numwcs = 0;
    }
  else
    {
      numwcs = length;
      memcpy(text->text.ch + text->gap_position, chars, length);
    }
 
  if (!text->freeze_count && (text->line_start_cache != NULL))
    {
      if (text->use_wchar)
 	{
 	  for (i=0; i<numwcs; i++)
 	    if (text->text.wc[text->gap_position + i] == '\n')
 	      new_line_count++;
	}
      else
 	{
 	  for (i=0; i<numwcs; i++)
 	    if (text->text.ch[text->gap_position + i] == '\n')
 	      new_line_count++;
 	}
    }
 
  if (numwcs > 0)
    {
      insert_text_property (text, font, fore, back, numwcs);
   
      text->gap_size -= numwcs;
      text->gap_position += numwcs;
   
      if (text->point.index < text->first_line_start_index)
 	text->first_line_start_index += numwcs;
      if (text->point.index < old_editable->selection_start_pos)
 	old_editable->selection_start_pos += numwcs;
      if (text->point.index < old_editable->selection_end_pos)
 	old_editable->selection_end_pos += numwcs;
      /* We'll reset the cursor later anyways if we aren't frozen */
      if (text->point.index < text->cursor_mark.index)
 	text->cursor_mark.index += numwcs;
  
      advance_mark_n (&text->point, numwcs);
  
      if (!text->freeze_count && (text->line_start_cache != NULL))
	insert_expose (text, old_height, numwcs, new_line_count);
    }

  if (frozen)
    gtk_text_thaw (text);
}

gboolean
gtk_text_backward_delete (GtkText *text,
			  guint    nchars)
{
  g_return_val_if_fail (GTK_IS_TEXT (text), FALSE);
  
  if (nchars > text->point.index || nchars <= 0)
    return FALSE;
  
  gtk_text_set_point (text, text->point.index - nchars);
  
  return gtk_text_forward_delete (text, nchars);
}

gboolean
gtk_text_forward_delete (GtkText *text,
			 guint    nchars)
{
  guint old_lines = 0, old_height = 0;
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (text);
  gboolean frozen = FALSE;
  
  g_return_val_if_fail (GTK_IS_TEXT (text), FALSE);
  
  if (text->point.index + nchars > TEXT_LENGTH (text) || nchars <= 0)
    return FALSE;
  
  if (!text->freeze_count && nchars > FREEZE_LENGTH)
    {
      gtk_text_freeze (text);
      frozen = TRUE;
    }
  
  if (!text->freeze_count && text->line_start_cache != NULL)
    {
      /* We need to undraw the cursor here, since we may later
       * delete the cursor's property
       */
      undraw_cursor (text, FALSE);
      find_line_containing_point (text, text->point.index, TRUE);
      compute_lines_pixels (text, nchars, &old_lines, &old_height);
    }
  
  /* FIXME, or resizing after deleting will be odd */
  if (text->point.index < text->first_line_start_index)
    {
      if (text->point.index + nchars >= text->first_line_start_index)
	{
	  text->first_line_start_index = text->point.index;
	  while ((text->first_line_start_index > 0) &&
		 (GTK_TEXT_INDEX (text, text->first_line_start_index - 1)
		  != LINE_DELIM))
	    text->first_line_start_index -= 1;
	  
	}
      else
	text->first_line_start_index -= nchars;
    }
  
  if (text->point.index < old_editable->selection_start_pos)
    old_editable->selection_start_pos -= 
      MIN(nchars, old_editable->selection_start_pos - text->point.index);
  if (text->point.index < old_editable->selection_end_pos)
    old_editable->selection_end_pos -= 
      MIN(nchars, old_editable->selection_end_pos - text->point.index);
  /* We'll reset the cursor later anyways if we aren't frozen */
  if (text->point.index < text->cursor_mark.index)
    move_mark_n (&text->cursor_mark, 
		 -MIN(nchars, text->cursor_mark.index - text->point.index));
  
  move_gap (text, text->point.index);
  
  text->gap_size += nchars;
  
  delete_text_property (text, nchars);
  
  if (!text->freeze_count && (text->line_start_cache != NULL))
    {
      delete_expose (text, nchars, old_lines, old_height);
      draw_cursor (text, FALSE);
    }
  
  if (frozen)
    gtk_text_thaw (text);
  
  return TRUE;
}

static void
gtk_text_set_position (GtkOldEditable *old_editable,
		       gint            position)
{
  GtkText *text = (GtkText *) old_editable;

  if (position < 0)
    position = gtk_text_get_length (text);                                    
  
  undraw_cursor (text, FALSE);
  text->cursor_mark = find_mark (text, position);
  find_cursor (text, TRUE);
  draw_cursor (text, FALSE);
  gtk_editable_select_region (GTK_EDITABLE (old_editable), 0, 0);
}

static gchar *    
gtk_text_get_chars (GtkOldEditable *old_editable,
		    gint            start_pos,
		    gint            end_pos)
{
  GtkText *text;

  gchar *retval;
  
  g_return_val_if_fail (GTK_IS_TEXT (old_editable), NULL);
  text = GTK_TEXT (old_editable);
  
  if (end_pos < 0)
    end_pos = TEXT_LENGTH (text);
  
  if ((start_pos < 0) || 
      (end_pos > TEXT_LENGTH (text)) || 
      (end_pos < start_pos))
    return NULL;
  
  move_gap (text, TEXT_LENGTH (text));
  make_forward_space (text, 1);

  if (text->use_wchar)
    {
      GdkWChar ch;
      ch = text->text.wc[end_pos];
      text->text.wc[end_pos] = 0;
      retval = gdk_wcstombs (text->text.wc + start_pos);
      text->text.wc[end_pos] = ch;
    }
  else
    {
      guchar ch;
      ch = text->text.ch[end_pos];
      text->text.ch[end_pos] = 0;
      retval = g_strdup ((gchar *)(text->text.ch + start_pos));
      text->text.ch[end_pos] = ch;
    }

  return retval;
}


static void
gtk_text_destroy (GtkObject *object)
{
  GtkText *text = GTK_TEXT (object);

  if (text->hadj)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (text->hadj), text);
      g_object_unref (text->hadj);
      text->hadj = NULL;
    }
  if (text->vadj)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (text->vadj), text);
      g_object_unref (text->vadj);
      text->vadj = NULL;
    }

  if (text->timer)
    {
      g_source_remove (text->timer);
      text->timer = 0;
    }
  
  GTK_OBJECT_CLASS(parent_class)->destroy (object);
}

static void
gtk_text_finalize (GObject *object)
{
  GtkText *text = GTK_TEXT (object);
  GList *tmp_list;

  /* Clean up the internal structures */
  if (text->use_wchar)
    g_free (text->text.wc);
  else
    g_free (text->text.ch);
  
  tmp_list = text->text_properties;
  while (tmp_list)
    {
      destroy_text_property (tmp_list->data);
      tmp_list = tmp_list->next;
    }

  if (text->current_font)
    text_font_unref (text->current_font);
  
  g_list_free (text->text_properties);
  
  if (text->use_wchar)
    {
      g_free (text->scratch_buffer.wc);
    }
  else
    {
      g_free (text->scratch_buffer.ch);
    }
  
  g_list_free (text->tab_stops);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_text_realize (GtkWidget *widget)
{
  GtkText *text = GTK_TEXT (widget);
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (widget);
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);
  
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
  
  attributes.x = (widget->style->xthickness + TEXT_BORDER_ROOM);
  attributes.y = (widget->style->ythickness + TEXT_BORDER_ROOM);
  attributes.width = MAX (1, (gint)widget->allocation.width - (gint)attributes.x * 2);
  attributes.height = MAX (1, (gint)widget->allocation.height - (gint)attributes.y * 2);

  attributes.cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), GDK_XTERM);
  attributes_mask |= GDK_WA_CURSOR;
  
  text->text_area = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (text->text_area, text);

  gdk_cursor_unref (attributes.cursor); /* The X server will keep it around as long as necessary */
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  
  /* Can't call gtk_style_set_background here because it's handled specially */
  gdk_window_set_background (widget->window, &widget->style->base[gtk_widget_get_state (widget)]);
  gdk_window_set_background (text->text_area, &widget->style->base[gtk_widget_get_state (widget)]);

  if (widget->style->bg_pixmap[GTK_STATE_NORMAL])
    text->bg_gc = create_bg_gc (text);
  
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
  gdk_gc_set_foreground (text->gc, &widget->style->text[GTK_STATE_NORMAL]);
  
  realize_properties (text);
  gdk_window_show (text->text_area);
  init_properties (text);

  if (old_editable->selection_start_pos != old_editable->selection_end_pos)
    gtk_old_editable_claim_selection (old_editable, TRUE, GDK_CURRENT_TIME);
  
  recompute_geometry (text);
}

static void 
gtk_text_style_set (GtkWidget *widget,
		    GtkStyle  *previous_style)
{
  GtkText *text = GTK_TEXT (widget);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_set_background (widget->window, &widget->style->base[gtk_widget_get_state (widget)]);
      gdk_window_set_background (text->text_area, &widget->style->base[gtk_widget_get_state (widget)]);
      
      if (text->bg_gc)
	{
	  g_object_unref (text->bg_gc);
	  text->bg_gc = NULL;
	}

      if (widget->style->bg_pixmap[GTK_STATE_NORMAL])
	text->bg_gc = create_bg_gc (text);

      recompute_geometry (text);
    }
  
  if (text->current_font)
    text_font_unref (text->current_font);
  text->current_font = get_text_font (gtk_style_get_font (widget->style));
}

static void
gtk_text_state_changed (GtkWidget   *widget,
			GtkStateType previous_state)
{
  GtkText *text = GTK_TEXT (widget);
  
  if (gtk_widget_get_realized (widget))
    {
      gdk_window_set_background (widget->window, &widget->style->base[gtk_widget_get_state (widget)]);
      gdk_window_set_background (text->text_area, &widget->style->base[gtk_widget_get_state (widget)]);
    }
}

static void
gtk_text_unrealize (GtkWidget *widget)
{
  GtkText *text = GTK_TEXT (widget);

  gdk_window_set_user_data (text->text_area, NULL);
  gdk_window_destroy (text->text_area);
  text->text_area = NULL;
  
  g_object_unref (text->gc);
  text->gc = NULL;

  if (text->bg_gc)
    {
      g_object_unref (text->bg_gc);
      text->bg_gc = NULL;
    }
  
  g_object_unref (text->line_wrap_bitmap);
  g_object_unref (text->line_arrow_bitmap);

  unrealize_properties (text);

  free_cache (text);

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
clear_focus_area (GtkText *text, gint area_x, gint area_y, gint area_width, gint area_height)
{
  GtkWidget *widget = GTK_WIDGET (text);
  GdkGC *gc;
 
  gint ythick = TEXT_BORDER_ROOM + widget->style->ythickness;
  gint xthick = TEXT_BORDER_ROOM + widget->style->xthickness;
  
  gint width, height;
  
  if (area_width == 0 || area_height == 0)
    return;
    
  if (widget->style->bg_pixmap[GTK_STATE_NORMAL])
    {
      gdk_drawable_get_size (widget->style->bg_pixmap[GTK_STATE_NORMAL], &width, &height);
  
      gdk_gc_set_ts_origin (text->bg_gc,
			    (- text->first_onscreen_hor_pixel + xthick) % width,
			    (- text->first_onscreen_ver_pixel + ythick) % height);
      
       gc = text->bg_gc;
    }
  else
    gc = widget->style->base_gc[widget->state];

  gdk_draw_rectangle (GTK_WIDGET (text)->window, gc, TRUE,
		      area_x, area_y, area_width, area_height);
}

static void
gtk_text_draw_focus (GtkWidget *widget)
{
  GtkText *text;
  gint width, height;
  gint x, y;
  
  g_return_if_fail (GTK_IS_TEXT (widget));
  
  text = GTK_TEXT (widget);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gint ythick = widget->style->ythickness;
      gint xthick = widget->style->xthickness;
      gint xextra = TEXT_BORDER_ROOM;
      gint yextra = TEXT_BORDER_ROOM;
      
      TDEBUG (("in gtk_text_draw_focus\n"));
      
      x = 0;
      y = 0;
      width = widget->allocation.width;
      height = widget->allocation.height;
      
      if (gtk_widget_has_focus (widget))
	{
	  x += 1;
	  y += 1;
	  width -=  2;
	  height -= 2;
	  xextra -= 1;
	  yextra -= 1;

	  gtk_paint_focus (widget->style, widget->window, gtk_widget_get_state (widget),
			   NULL, widget, "text",
			   0, 0,
			   widget->allocation.width,
			   widget->allocation.height);
	}

      gtk_paint_shadow (widget->style, widget->window,
			GTK_STATE_NORMAL, GTK_SHADOW_IN,
			NULL, widget, "text",
			x, y, width, height);

      x += xthick; 
      y += ythick;
      width -= 2 * xthick;
      height -= 2 * ythick;
      
      /* top rect */
      clear_focus_area (text, x, y, width, yextra);
      /* left rect */
      clear_focus_area (text, x, y + yextra, 
			xextra, y + height - 2 * yextra);
      /* right rect */
      clear_focus_area (text, x + width - xextra, y + yextra, 
			xextra, height - 2 * ythick);
      /* bottom rect */
      clear_focus_area (text, x, x + height - yextra, width, yextra);
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
  GdkFont *font;
  gint xthickness;
  gint ythickness;
  gint char_height;
  gint char_width;

  xthickness = widget->style->xthickness + TEXT_BORDER_ROOM;
  ythickness = widget->style->ythickness + TEXT_BORDER_ROOM;

  font = gtk_style_get_font (widget->style);
  
  char_height = MIN_TEXT_HEIGHT_LINES * (font->ascent +
					 font->descent);
  
  char_width = MIN_TEXT_WIDTH_LINES * (gdk_text_width (font,
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
  GtkText *text = GTK_TEXT (widget);

  widget->allocation = *allocation;
  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      
      gdk_window_move_resize (text->text_area,
			      widget->style->xthickness + TEXT_BORDER_ROOM,
			      widget->style->ythickness + TEXT_BORDER_ROOM,
			      MAX (1, (gint)widget->allocation.width - (gint)(widget->style->xthickness +
							  (gint)TEXT_BORDER_ROOM) * 2),
			      MAX (1, (gint)widget->allocation.height - (gint)(widget->style->ythickness +
							   (gint)TEXT_BORDER_ROOM) * 2));
      
      recompute_geometry (text);
    }
}

static gint
gtk_text_expose (GtkWidget      *widget,
		 GdkEventExpose *event)
{
  if (event->window == GTK_TEXT (widget)->text_area)
    {
      TDEBUG (("in gtk_text_expose (expose)\n"));
      expose_text (GTK_TEXT (widget), &event->area, TRUE);
    }
  else if (event->count == 0)
    {
      TDEBUG (("in gtk_text_expose (focus)\n"));
      gtk_text_draw_focus (widget);
    }
  
  return FALSE;
}

static gint
gtk_text_scroll_timeout (gpointer data)
{
  GtkText *text;
  gint x, y;
  GdkModifierType mask;
  
  text = GTK_TEXT (data);
  
  text->timer = 0;
  gdk_window_get_pointer (text->text_area, &x, &y, &mask);
  
  if (mask & (GDK_BUTTON1_MASK | GDK_BUTTON3_MASK))
    {
      GdkEvent *event = gdk_event_new (GDK_MOTION_NOTIFY);
      
      event->motion.is_hint = 0;
      event->motion.x = x;
      event->motion.y = y;
      event->motion.state = mask;
      
      gtk_text_motion_notify (GTK_WIDGET (text), (GdkEventMotion *)event);

      gdk_event_free (event);
    }

  return FALSE;
}

static gint
gtk_text_button_press (GtkWidget      *widget,
		       GdkEventButton *event)
{
  GtkText *text = GTK_TEXT (widget);
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (widget);
  
  if (text->button && (event->button != text->button))
    return FALSE;
  
  text->button = event->button;
  
  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);
  
  if (event->button == 1)
    {
      switch (event->type)
	{
	case GDK_BUTTON_PRESS:
	  gtk_grab_add (widget);
	  
	  undraw_cursor (text, FALSE);
	  find_mouse_cursor (text, (gint)event->x, (gint)event->y);
	  draw_cursor (text, FALSE);
	  
	  /* Set it now, so we display things right. We'll unset it
	   * later if things don't work out */
	  old_editable->has_selection = TRUE;
	  gtk_text_set_selection (GTK_OLD_EDITABLE (text),
				  text->cursor_mark.index,
				  text->cursor_mark.index);
	  
	  break;
	  
	case GDK_2BUTTON_PRESS:
	  gtk_text_select_word (text, event->time);
	  break;
	  
	case GDK_3BUTTON_PRESS:
	  gtk_text_select_line (text, event->time);
	  break;
	  
	default:
	  break;
	}
    }
  else if (event->type == GDK_BUTTON_PRESS)
    {
      if ((event->button == 2) && old_editable->editable)
	{
	  if (old_editable->selection_start_pos == old_editable->selection_end_pos ||
	      old_editable->has_selection)
	    {
	      undraw_cursor (text, FALSE);
	      find_mouse_cursor (text, (gint)event->x, (gint)event->y);
	      draw_cursor (text, FALSE);
	      
	    }
	  
	  gtk_selection_convert (widget, GDK_SELECTION_PRIMARY,
				 gdk_atom_intern_static_string ("UTF8_STRING"),
				 event->time);
	}
      else
	{
	  GdkDisplay *display = gtk_widget_get_display (widget);
	  
	  gtk_grab_add (widget);
	  
	  undraw_cursor (text, FALSE);
	  find_mouse_cursor (text, event->x, event->y);
	  draw_cursor (text, FALSE);
	  
	  gtk_text_set_selection (GTK_OLD_EDITABLE (text),
				  text->cursor_mark.index,
				  text->cursor_mark.index);
	  
	  old_editable->has_selection = FALSE;
	  if (gdk_selection_owner_get_for_display (display,
						   GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set_for_display (display,
						 NULL, 
						 GDK_SELECTION_PRIMARY,
						 event->time);
	}
    }
  
  return TRUE;
}

static gint
gtk_text_button_release (GtkWidget      *widget,
			 GdkEventButton *event)
{
  GtkText *text = GTK_TEXT (widget);
  GtkOldEditable *old_editable;
  GdkDisplay *display;

  gtk_grab_remove (widget);
  
  if (text->button != event->button)
    return FALSE;
  
  text->button = 0;
  
  if (text->timer)
    {
      g_source_remove (text->timer);
      text->timer = 0;
    }
  
  if (event->button == 1)
    {
      text = GTK_TEXT (widget);
      old_editable = GTK_OLD_EDITABLE (widget);
      display = gtk_widget_get_display (widget);
      
      gtk_grab_remove (widget);
      
      old_editable->has_selection = FALSE;
      if (old_editable->selection_start_pos != old_editable->selection_end_pos)
	{
	  if (gtk_selection_owner_set_for_display (display,
						   widget,
						   GDK_SELECTION_PRIMARY,
						   event->time))
	    old_editable->has_selection = TRUE;
	  else
	    gtk_text_update_text (old_editable, old_editable->selection_start_pos,
				  old_editable->selection_end_pos);
	}
      else
	{
	  if (gdk_selection_owner_get_for_display (display,
						   GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set_for_display (display, 
						 NULL,
						 GDK_SELECTION_PRIMARY, 
						 event->time);
	}
    }
  else if (event->button == 3)
    {
      gtk_grab_remove (widget);
    }
  
  undraw_cursor (text, FALSE);
  find_cursor (text, TRUE);
  draw_cursor (text, FALSE);
  
  return TRUE;
}

static gint
gtk_text_motion_notify (GtkWidget      *widget,
			GdkEventMotion *event)
{
  GtkText *text = GTK_TEXT (widget);
  gint x, y;
  gint height;
  GdkModifierType mask;

  x = event->x;
  y = event->y;
  mask = event->state;
  if (event->is_hint || (text->text_area != event->window))
    {
      gdk_window_get_pointer (text->text_area, &x, &y, &mask);
    }
  
  if ((text->button == 0) ||
      !(mask & (GDK_BUTTON1_MASK | GDK_BUTTON3_MASK)))
    return FALSE;
  
  gdk_drawable_get_size (text->text_area, NULL, &height);
  
  if ((y < 0) || (y > height))
    {
      if (text->timer == 0)
	{
	  text->timer = gdk_threads_add_timeout (SCROLL_TIME, 
				       gtk_text_scroll_timeout,
				       text);
	  
	  if (y < 0)
	    scroll_int (text, y/2);
	  else
	    scroll_int (text, (y - height)/2);
	}
      else
	return FALSE;
    }
  
  undraw_cursor (GTK_TEXT (widget), FALSE);
  find_mouse_cursor (GTK_TEXT (widget), x, y);
  draw_cursor (GTK_TEXT (widget), FALSE);
  
  gtk_text_set_selection (GTK_OLD_EDITABLE (text), 
			  GTK_OLD_EDITABLE (text)->selection_start_pos,
			  text->cursor_mark.index);
  
  return FALSE;
}

static void 
gtk_text_insert_text    (GtkEditable       *editable,
			 const gchar       *new_text,
			 gint               new_text_length,
			 gint              *position)
{
  GtkText *text = GTK_TEXT (editable);
  GdkFont *font;
  GdkColor *fore, *back;

  TextProperty *property;

  gtk_text_set_point (text, *position);

  property = MARK_CURRENT_PROPERTY (&text->point);
  font = property->flags & PROPERTY_FONT ? property->font->gdk_font : NULL; 
  fore = property->flags & PROPERTY_FOREGROUND ? &property->fore_color : NULL; 
  back = property->flags & PROPERTY_BACKGROUND ? &property->back_color : NULL; 
  
  gtk_text_insert (text, font, fore, back, new_text, new_text_length);

  *position = text->point.index;
}

static void 
gtk_text_delete_text    (GtkEditable       *editable,
			 gint               start_pos,
			 gint               end_pos)
{
  GtkText *text = GTK_TEXT (editable);
  
  g_return_if_fail (start_pos >= 0);

  gtk_text_set_point (text, start_pos);
  if (end_pos < 0)
    end_pos = TEXT_LENGTH (text);
  
  if (end_pos > start_pos)
    gtk_text_forward_delete (text, end_pos - start_pos);
}

static gint
gtk_text_key_press (GtkWidget   *widget,
		    GdkEventKey *event)
{
  GtkText *text = GTK_TEXT (widget);
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (widget);
  gchar key;
  gint return_val;
  gint position;

  key = event->keyval;
  return_val = TRUE;
  
  if ((GTK_OLD_EDITABLE(text)->editable == FALSE))
    {
      switch (event->keyval)
	{
	case GDK_Home:
        case GDK_KP_Home:
	  if (event->state & GDK_CONTROL_MASK)
	    scroll_int (text, -text->vadj->value);
	  else
	    return_val = FALSE;
	  break;
	case GDK_End:
        case GDK_KP_End:
	  if (event->state & GDK_CONTROL_MASK)
	    scroll_int (text, +text->vadj->upper); 
	  else
	    return_val = FALSE;
	  break;
        case GDK_KP_Page_Up:
	case GDK_Page_Up:   scroll_int (text, -text->vadj->page_increment); break;
        case GDK_KP_Page_Down:
	case GDK_Page_Down: scroll_int (text, +text->vadj->page_increment); break;
        case GDK_KP_Up:
	case GDK_Up:        scroll_int (text, -KEY_SCROLL_PIXELS); break;
        case GDK_KP_Down:
	case GDK_Down:      scroll_int (text, +KEY_SCROLL_PIXELS); break;
	case GDK_Return:
        case GDK_ISO_Enter:
        case GDK_KP_Enter:
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_signal_emit_by_name (GTK_OBJECT (text), "activate");
	  else
	    return_val = FALSE;
	  break;
	default:
	  return_val = FALSE;
	  break;
	}
    }
  else
    {
      gint extend_selection;
      gint extend_start;
      guint initial_pos = old_editable->current_pos;
      
      text->point = find_mark (text, text->cursor_mark.index);
      
      extend_selection = event->state & GDK_SHIFT_MASK;
      extend_start = FALSE;
      
      if (extend_selection)
	{
	  old_editable->has_selection = TRUE;
	  
	  if (old_editable->selection_start_pos == old_editable->selection_end_pos)
	    {
	      old_editable->selection_start_pos = text->point.index;
	      old_editable->selection_end_pos = text->point.index;
	    }
	  
	  extend_start = (text->point.index == old_editable->selection_start_pos);
	}
      
      switch (event->keyval)
	{
        case GDK_KP_Home:
	case GDK_Home:
	  if (event->state & GDK_CONTROL_MASK)
	    move_cursor_buffer_ver (text, -1);
	  else
	    gtk_text_move_beginning_of_line (text);
	  break;
        case GDK_KP_End:
	case GDK_End:
	  if (event->state & GDK_CONTROL_MASK)
	    move_cursor_buffer_ver (text, +1);
	  else
	    gtk_text_move_end_of_line (text);
	  break;
        case GDK_KP_Page_Up:
	case GDK_Page_Up:   move_cursor_page_ver (text, -1); break;
        case GDK_KP_Page_Down:
	case GDK_Page_Down: move_cursor_page_ver (text, +1); break;
	  /* CUA has Ctrl-Up/Ctrl-Down as paragraph up down */
        case GDK_KP_Up:
	case GDK_Up:        move_cursor_ver (text, -1); break;
        case GDK_KP_Down:
	case GDK_Down:      move_cursor_ver (text, +1); break;
        case GDK_KP_Left:
	case GDK_Left:
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_text_move_backward_word (text);
	  else
	    move_cursor_hor (text, -1); 
	  break;
        case GDK_KP_Right:
	case GDK_Right:     
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_text_move_forward_word (text);
	  else
	    move_cursor_hor (text, +1); 
	  break;
	  
	case GDK_BackSpace:
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_text_delete_backward_word (text);
	  else
	    gtk_text_delete_backward_character (text);
	  break;
	case GDK_Clear:
	  gtk_text_delete_line (text);
	  break;
        case GDK_KP_Insert:
	case GDK_Insert:
	  if (event->state & GDK_SHIFT_MASK)
	    {
	      extend_selection = FALSE;
	      gtk_editable_paste_clipboard (GTK_EDITABLE (old_editable));
	    }
	  else if (event->state & GDK_CONTROL_MASK)
	    {
	      gtk_editable_copy_clipboard (GTK_EDITABLE (old_editable));
	    }
	  else
	    {
	      /* gtk_toggle_insert(text) -- IMPLEMENT */
	    }
	  break;
	case GDK_Delete:
        case GDK_KP_Delete:
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_text_delete_forward_word (text);
	  else if (event->state & GDK_SHIFT_MASK)
	    {
	      extend_selection = FALSE;
	      gtk_editable_cut_clipboard (GTK_EDITABLE (old_editable));
	    }
	  else
	    gtk_text_delete_forward_character (text);
	  break;
	case GDK_Tab:
        case GDK_ISO_Left_Tab:
        case GDK_KP_Tab:
	  position = text->point.index;
	  gtk_editable_insert_text (GTK_EDITABLE (old_editable), "\t", 1, &position);
	  break;
        case GDK_KP_Enter:
        case GDK_ISO_Enter:
	case GDK_Return:
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_signal_emit_by_name (GTK_OBJECT (text), "activate");
	  else
	    {
	      position = text->point.index;
	      gtk_editable_insert_text (GTK_EDITABLE (old_editable), "\n", 1, &position);
	    }
	  break;
	case GDK_Escape:
	  /* Don't insert literally */
	  return_val = FALSE;
	  break;
	  
	default:
	  return_val = FALSE;
	  
	  if (event->state & GDK_CONTROL_MASK)
	    {
	      return_val = TRUE;
	      if ((key >= 'A') && (key <= 'Z'))
		key -= 'A' - 'a';

	      switch (key)
		{
		case 'a':
		  gtk_text_move_beginning_of_line (text);
		  break;
		case 'b':
		  gtk_text_move_backward_character (text);
		  break;
		case 'c':
		  gtk_editable_copy_clipboard (GTK_EDITABLE (text));
		  break;
		case 'd':
		  gtk_text_delete_forward_character (text);
		  break;
		case 'e':
		  gtk_text_move_end_of_line (text);
		  break;
		case 'f':
		  gtk_text_move_forward_character (text);
		  break;
		case 'h':
		  gtk_text_delete_backward_character (text);
		  break;
		case 'k':
		  gtk_text_delete_to_line_end (text);
		  break;
		case 'n':
		  gtk_text_move_next_line (text);
		  break;
		case 'p':
		  gtk_text_move_previous_line (text);
		  break;
		case 'u':
		  gtk_text_delete_line (text);
		  break;
		case 'v':
		  gtk_editable_paste_clipboard (GTK_EDITABLE (text));
		  break;
		case 'w':
		  gtk_text_delete_backward_word (text);
		  break;
		case 'x':
		  gtk_editable_cut_clipboard (GTK_EDITABLE (text));
		  break;
		default:
		  return_val = FALSE;
		}

	      break;
	    }
	  else if (event->state & GDK_MOD1_MASK)
	    {
	      return_val = TRUE;
	      if ((key >= 'A') && (key <= 'Z'))
		key -= 'A' - 'a';
	      
	      switch (key)
		{
		case 'b':
		  gtk_text_move_backward_word (text);
		  break;
		case 'd':
		  gtk_text_delete_forward_word (text);
		  break;
		case 'f':
		  gtk_text_move_forward_word (text);
		  break;
		default:
		  return_val = FALSE;
		}
	      
	      break;
	    }
	  else if (event->length > 0)
	    {
	      extend_selection = FALSE;
	      
	      gtk_editable_delete_selection (GTK_EDITABLE (old_editable));
	      position = text->point.index;
	      gtk_editable_insert_text (GTK_EDITABLE (old_editable), event->string, event->length, &position);
	      
	      return_val = TRUE;
	    }
	}
      
      if (return_val && (old_editable->current_pos != initial_pos))
	{
	  if (extend_selection)
	    {
	      if (old_editable->current_pos < old_editable->selection_start_pos)
		gtk_text_set_selection (old_editable, old_editable->current_pos,
					old_editable->selection_end_pos);
	      else if (old_editable->current_pos > old_editable->selection_end_pos)
		gtk_text_set_selection (old_editable, old_editable->selection_start_pos,
					old_editable->current_pos);
	      else
		{
		  if (extend_start)
		    gtk_text_set_selection (old_editable, old_editable->current_pos,
					    old_editable->selection_end_pos);
		  else
		    gtk_text_set_selection (old_editable, old_editable->selection_start_pos,
					    old_editable->current_pos);
		}
	    }
	  else
	    gtk_text_set_selection (old_editable, 0, 0);
	  
	  gtk_old_editable_claim_selection (old_editable,
					    old_editable->selection_start_pos != old_editable->selection_end_pos,
					    event->time);
	}
    }
  
  return return_val;
}

static void
gtk_text_adjustment (GtkAdjustment *adjustment,
		     GtkText       *text)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (GTK_IS_TEXT (text));
  
  /* Just ignore it if we haven't been size-allocated and realized yet */
  if (text->line_start_cache == NULL) 
    return;
  
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
gtk_text_adjustment_destroyed (GtkAdjustment *adjustment,
                               GtkText       *text)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (GTK_IS_TEXT (text));

  if (adjustment == text->hadj)
    gtk_text_set_adjustments (text, NULL, text->vadj);
  if (adjustment == text->vadj)
    gtk_text_set_adjustments (text, text->hadj, NULL);
}


static GtkPropertyMark
find_this_line_start_mark (GtkText* text, guint point_position, const GtkPropertyMark* near)
{
  GtkPropertyMark mark;
  
  mark = find_mark_near (text, point_position, near);
  
  while (mark.index > 0 &&
	 GTK_TEXT_INDEX (text, mark.index - 1) != LINE_DELIM)
    decrement_mark (&mark);
  
  return mark;
}

static void
init_tab_cont (GtkText* text, PrevTabCont* tab_cont)
{
  tab_cont->pixel_offset          = 0;
  tab_cont->tab_start.tab_stops   = text->tab_stops;
  tab_cont->tab_start.to_next_tab = (gintptr) text->tab_stops->data;
  
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
	lp = g_slice_new (LineParams);
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
  GList *new_line_start;
  GtkPropertyMark mark;
  
  if (CACHE_DATA(text->line_start_cache).start.index == 0)
    return;
  
  mark = find_this_line_start_mark (text,
				    CACHE_DATA(text->line_start_cache).start.index - 1,
				    &CACHE_DATA(text->line_start_cache).start);
  
  new_line_start = fetch_lines (text, &mark, NULL, FetchLinesCount, 1);
  
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

/* Compute the number of lines, and vertical pixels for n characters
 * starting from the point 
 */
static void
compute_lines_pixels (GtkText* text, guint char_count,
		      guint *lines, guint *pixels)
{
  GList *line = text->current_line;
  gint chars_left = char_count;
  
  *lines = 0;
  *pixels = 0;
  
  /* If chars_left == 0, that means we're joining two lines in a
   * deletion, so add in the values for the next line as well 
   */
  for (; line && chars_left >= 0; line = line->next)
    {
      *pixels += LINE_HEIGHT(CACHE_DATA(line));
      
      if (line == text->current_line)
	chars_left -= CACHE_DATA(line).end.index - text->point.index + 1;
      else
	chars_left -= CACHE_DATA(line).end.index - CACHE_DATA(line).start.index + 1;
      
      if (!text->line_wrap || !CACHE_DATA(line).wraps)
	*lines += 1;
      else
	if (chars_left < 0)
	  chars_left = 0;	/* force another loop */
      
      if (!line->next)
	fetch_lines_forward (text, 1);
    }
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
swap_lines (GtkText* text, GList* old, GList* new, guint old_line_count)
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
correct_cache_delete (GtkText* text, gint nchars, gint lines)
{
  GList* cache = text->current_line;
  gint i;
  
  for (i = 0; cache && i < lines; i += 1, cache = cache->next)
    /* nothing */;
  
  for (; cache; cache = cache->next)
    {
      GtkPropertyMark *start = &CACHE_DATA(cache).start;
      GtkPropertyMark *end = &CACHE_DATA(cache).end;
      
      start->index -= nchars;
      end->index -= nchars;
      
      if (LAST_INDEX (text, text->point) &&
	  start->index == text->point.index)
	*start = text->point;
      else if (start->property == text->point.property)
	start->offset = start->index - (text->point.index - text->point.offset);
      
      if (LAST_INDEX (text, text->point) &&
	  end->index == text->point.index)
	*end = text->point;
      if (end->property == text->point.property)
	end->offset = end->index - (text->point.index - text->point.offset);
      
      /*TEXT_ASSERT_MARK(text, start, "start");*/
      /*TEXT_ASSERT_MARK(text, end, "end");*/
    }
}

static void
delete_expose (GtkText* text, guint nchars, guint old_lines, guint old_pixels)
{
  GtkWidget *widget = GTK_WIDGET (text);
  
  gint pixel_height;
  guint new_pixels = 0;
  GdkRectangle rect;
  GList* new_line = NULL;
  gint width, height;
  
  text->cursor_virtual_x = 0;
  
  correct_cache_delete (text, nchars, old_lines);
  
  pixel_height = pixel_height_of(text, text->current_line) -
    LINE_HEIGHT(CACHE_DATA(text->current_line));
  
  if (CACHE_DATA(text->current_line).start.index == text->point.index)
    CACHE_DATA(text->current_line).start = text->point;
  
  new_line = fetch_lines (text,
			  &CACHE_DATA(text->current_line).start,
			  &CACHE_DATA(text->current_line).tab_cont,
			  FetchLinesCount,
			  1);
  
  swap_lines (text, text->current_line, new_line, old_lines);
  
  text->current_line = new_line;
  
  new_pixels = total_line_height (text, new_line, 1);
  
  gdk_drawable_get_size (text->text_area, &width, &height);
  
  if (old_pixels != new_pixels)
    {
      if (!widget->style->bg_pixmap[GTK_STATE_NORMAL])
	{
	  gdk_draw_drawable (text->text_area,
                             text->gc,
                             text->text_area,
                             0,
                             pixel_height + old_pixels,
                             0,
                             pixel_height + new_pixels,
                             width,
                             height);
	}
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
  
  find_cursor (text, TRUE);
  
  if (old_pixels != new_pixels)
    {
      if (widget->style->bg_pixmap[GTK_STATE_NORMAL])
	{
	  rect.x = 0;
	  rect.y = pixel_height + new_pixels;
	  rect.width = width;
	  rect.height = height - rect.y;
	  
	  expose_text (text, &rect, FALSE);
	}
      else
	process_exposes (text);
    }
  
  TEXT_ASSERT (text);
  TEXT_SHOW(text);
}

/* note, the point has already been moved forward */
static void
correct_cache_insert (GtkText* text, gint nchars)
{
  GList *cache;
  GtkPropertyMark *start;
  GtkPropertyMark *end;
  gboolean was_split = FALSE;
  
  /* We need to distinguish whether the property was split in the
   * insert or not, so we check if the point (which points after
   * the insertion here), points to the same character as the
   * point before. Ugh.
   */
  if (nchars > 0)
    {
      GtkPropertyMark tmp_mark = text->point;
      move_mark_n (&tmp_mark, -1);
      
      if (tmp_mark.property != text->point.property)
	was_split = TRUE;
    }
  
  /* If we inserted a property exactly at the beginning of the
   * line, we have to correct here, or fetch_lines will
   * fetch junk.
   */
  start = &CACHE_DATA(text->current_line).start;

  /* Check if if we split exactly at the beginning of the line:
   * (was_split won't be set if we are inserting at the end of the text, 
   *  so we don't check)
   */
  if (start->offset ==  MARK_CURRENT_PROPERTY (start)->length)
    SET_PROPERTY_MARK (start, start->property->next, 0);
  /* Check if we inserted a property at the beginning of the text: */
  else if (was_split &&
	   (start->property == text->point.property) &&
	   (start->index == text->point.index - nchars))
    SET_PROPERTY_MARK (start, start->property->prev, 0);

  /* Now correct the offsets, and check for start or end marks that
   * are after the point, yet point to a property before the point's
   * property. This indicates that they are meant to point to the
   * second half of a property we split in insert_text_property(), so
   * we fix them up that way.  
   */
  cache = text->current_line->next;
  
  for (; cache; cache = cache->next)
    {
      start = &CACHE_DATA(cache).start;
      end = &CACHE_DATA(cache).end;
      
      if (LAST_INDEX (text, text->point) &&
	  start->index == text->point.index)
	*start = text->point;
      else if (start->index >= text->point.index - nchars)
	{
	  if (!was_split && start->property == text->point.property)
	    move_mark_n(start, nchars);
	  else
	    {
	      if (start->property->next &&
		  (start->property->next->next == text->point.property))
		{
		  g_assert (start->offset >=  MARK_CURRENT_PROPERTY (start)->length);
		  start->offset -= MARK_CURRENT_PROPERTY (start)->length;
		  start->property = text->point.property;
		}
	      start->index += nchars;
	    }
	}
      
      if (LAST_INDEX (text, text->point) &&
	  end->index == text->point.index)
	*end = text->point;
      if (end->index >= text->point.index - nchars)
	{
	  if (!was_split && end->property == text->point.property)
	    move_mark_n(end, nchars);
	  else 
	    {
	      if (end->property->next &&
		  (end->property->next->next == text->point.property))
		{
		  g_assert (end->offset >=  MARK_CURRENT_PROPERTY (end)->length);
		  end->offset -= MARK_CURRENT_PROPERTY (end)->length;
		  end->property = text->point.property;
		}
	      end->index += nchars;
	    }
	}
      
      /*TEXT_ASSERT_MARK(text, start, "start");*/
      /*TEXT_ASSERT_MARK(text, end, "end");*/
    }
}


static void
insert_expose (GtkText* text, guint old_pixels, gint nchars,
	       guint new_line_count)
{
  GtkWidget *widget = GTK_WIDGET (text);
  
  gint pixel_height;
  guint new_pixels = 0;
  GdkRectangle rect;
  GList* new_lines = NULL;
  gint width, height;
  
  text->cursor_virtual_x = 0;
  
  undraw_cursor (text, FALSE);
  
  correct_cache_insert (text, nchars);
  
  TEXT_SHOW_ADJ (text, text->vadj, "vadj");
  
  pixel_height = pixel_height_of(text, text->current_line) -
    LINE_HEIGHT(CACHE_DATA(text->current_line));
  
  new_lines = fetch_lines (text,
			   &CACHE_DATA(text->current_line).start,
			   &CACHE_DATA(text->current_line).tab_cont,
			   FetchLinesCount,
			   new_line_count);
  
  swap_lines (text, text->current_line, new_lines, 1);
  
  text->current_line = new_lines;
  
  new_pixels = total_line_height (text, new_lines, new_line_count);
  
  gdk_drawable_get_size (text->text_area, &width, &height);
  
  if (old_pixels != new_pixels)
    {
      if (!widget->style->bg_pixmap[GTK_STATE_NORMAL])
	{
	  gdk_draw_drawable (text->text_area,
                             text->gc,
                             text->text_area,
                             0,
                             pixel_height + old_pixels,
                             0,
                             pixel_height + new_pixels,
                             width,
                             height + (old_pixels - new_pixels) - pixel_height);
	  
	}
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
  
  find_cursor (text, TRUE);
  
  draw_cursor (text, FALSE);
  
  if (old_pixels != new_pixels)
    {
      if (widget->style->bg_pixmap[GTK_STATE_NORMAL])
	{
	  rect.x = 0;
	  rect.y = pixel_height + new_pixels;
	  rect.width = width;
	  rect.height = height - rect.y;
	  
	  expose_text (text, &rect, FALSE);
	}
      else
	process_exposes (text);
    }
  
  TEXT_SHOW_ADJ (text, text->vadj, "vadj");
  TEXT_ASSERT (text);
  TEXT_SHOW(text);
}

/* Text property functions */

static guint
font_hash (gconstpointer font)
{
  return gdk_font_id ((const GdkFont*) font);
}

static GHashTable *font_cache_table = NULL;

static GtkTextFont*
get_text_font (GdkFont* gfont)
{
  GtkTextFont* tf;
  gint i;
  
  if (!font_cache_table)
    font_cache_table = g_hash_table_new (font_hash, (GEqualFunc) gdk_font_equal);
  
  tf = g_hash_table_lookup (font_cache_table, gfont);
  
  if (tf)
    {
      tf->ref_count++;
      return tf;
    }

  tf = g_new (GtkTextFont, 1);
  tf->ref_count = 1;

  tf->gdk_font = gfont;
  gdk_font_ref (gfont);
  
  for(i = 0; i < 256; i += 1)
    tf->char_widths[i] = gdk_char_width (gfont, (char)i);
  
  g_hash_table_insert (font_cache_table, gfont, tf);
  
  return tf;
}

static void
text_font_unref (GtkTextFont *text_font)
{
  text_font->ref_count--;
  if (text_font->ref_count == 0)
    {
      g_hash_table_remove (font_cache_table, text_font->gdk_font);
      gdk_font_unref (text_font->gdk_font);
      g_free (text_font);
    }
}

static gint
text_properties_equal (TextProperty* prop, GdkFont* font, const GdkColor *fore, const GdkColor *back)
{
  if (prop->flags & PROPERTY_FONT)
    {
      gboolean retval;
      GtkTextFont *text_font;

      if (!font)
	return FALSE;

      text_font = get_text_font (font);

      retval = (prop->font == text_font);
      text_font_unref (text_font);
      
      if (!retval)
	return FALSE;
    }
  else
    if (font != NULL)
      return FALSE;

  if (prop->flags & PROPERTY_FOREGROUND)
    {
      if (!fore || !gdk_color_equal (&prop->fore_color, fore))
	return FALSE;
    }
  else
    if (fore != NULL)
      return FALSE;

  if (prop->flags & PROPERTY_BACKGROUND)
    {
      if (!back || !gdk_color_equal (&prop->back_color, back))
	return FALSE;
    }
  else
    if (back != NULL)
      return FALSE;
  
  return TRUE;
}

static void
realize_property (GtkText *text, TextProperty *prop)
{
  GdkColormap *colormap = gtk_widget_get_colormap (GTK_WIDGET (text));

  if (prop->flags & PROPERTY_FOREGROUND)
    gdk_colormap_alloc_color (colormap, &prop->fore_color, FALSE, FALSE);
  
  if (prop->flags & PROPERTY_BACKGROUND)
    gdk_colormap_alloc_color (colormap, &prop->back_color, FALSE, FALSE);
}

static void
realize_properties (GtkText *text)
{
  GList *tmp_list = text->text_properties;

  while (tmp_list)
    {
      realize_property (text, tmp_list->data);
      
      tmp_list = tmp_list->next;
    }
}

static void
unrealize_property (GtkText *text, TextProperty *prop)
{
  GdkColormap *colormap = gtk_widget_get_colormap (GTK_WIDGET (text));

  if (prop->flags & PROPERTY_FOREGROUND)
    gdk_colormap_free_colors (colormap, &prop->fore_color, 1);
  
  if (prop->flags & PROPERTY_BACKGROUND)
    gdk_colormap_free_colors (colormap, &prop->back_color, 1);
}

static void
unrealize_properties (GtkText *text)
{
  GList *tmp_list = text->text_properties;

  while (tmp_list)
    {
      unrealize_property (text, tmp_list->data);

      tmp_list = tmp_list->next;
    }
}

static TextProperty*
new_text_property (GtkText *text, GdkFont *font, const GdkColor* fore,
		   const GdkColor* back, guint length)
{
  TextProperty *prop;
  
  prop = g_slice_new (TextProperty);

  prop->flags = 0;
  if (font)
    {
      prop->flags |= PROPERTY_FONT;
      prop->font = get_text_font (font);
    }
  else
    prop->font = NULL;
  
  if (fore)
    {
      prop->flags |= PROPERTY_FOREGROUND;
      prop->fore_color = *fore;
    }
      
  if (back)
    {
      prop->flags |= PROPERTY_BACKGROUND;
      prop->back_color = *back;
    }

  prop->length = length;

  if (gtk_widget_get_realized (GTK_WIDGET (text)))
    realize_property (text, prop);

  return prop;
}

static void
destroy_text_property (TextProperty *prop)
{
  if (prop->font)
    text_font_unref (prop->font);
  
  g_slice_free (TextProperty, prop);
}

/* Flop the memory between the point and the gap around like a
 * dead fish. */
static void
move_gap (GtkText* text, guint index)
{
  if (text->gap_position < index)
    {
      gint diff = index - text->gap_position;
      
      if (text->use_wchar)
	g_memmove (text->text.wc + text->gap_position,
		   text->text.wc + text->gap_position + text->gap_size,
		   diff*sizeof (GdkWChar));
      else
	g_memmove (text->text.ch + text->gap_position,
		   text->text.ch + text->gap_position + text->gap_size,
		   diff);
      
      text->gap_position = index;
    }
  else if (text->gap_position > index)
    {
      gint diff = text->gap_position - index;
      
      if (text->use_wchar)
	g_memmove (text->text.wc + index + text->gap_size,
		   text->text.wc + index,
		   diff*sizeof (GdkWChar));
      else
	g_memmove (text->text.ch + index + text->gap_size,
		   text->text.ch + index,
		   diff);
      
      text->gap_position = index;
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
	  
	  if (text->use_wchar)
	    text->text.wc = (GdkWChar *)g_realloc(text->text.wc,
						  i*sizeof(GdkWChar));
	  else
	    text->text.ch = (guchar *)g_realloc(text->text.ch, i);
	  text->text_len = i;
	}
      
      if (text->use_wchar)
	g_memmove (text->text.wc + text->gap_position + text->gap_size + 2*len,
		   text->text.wc + text->gap_position + text->gap_size,
		   (text->text_end - (text->gap_position + text->gap_size))
		   *sizeof(GdkWChar));
      else
	g_memmove (text->text.ch + text->gap_position + text->gap_size + 2*len,
		   text->text.ch + text->gap_position + text->gap_size,
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
		      const GdkColor *fore, const GdkColor* back, guint len)
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
      else if ((MARK_NEXT_LIST_PTR(mark) == NULL) &&
	       (forward_prop->length == 1))
	{
	  /* Next property just has last position, take it over */

	  if (gtk_widget_get_realized (GTK_WIDGET (text)))
	    unrealize_property (text, forward_prop);

	  forward_prop->flags = 0;
	  if (font)
	    {
	      forward_prop->flags |= PROPERTY_FONT;
	      forward_prop->font = get_text_font (font);
	    }
	  else
	    forward_prop->font = NULL;
	    
	  if (fore)
	    {
	      forward_prop->flags |= PROPERTY_FOREGROUND;
	      forward_prop->fore_color = *fore;
	    }
	  if (back)
	    {
	      forward_prop->flags |= PROPERTY_BACKGROUND;
	      forward_prop->back_color = *back;
	    }
	  forward_prop->length += len;

	  if (gtk_widget_get_realized (GTK_WIDGET (text)))
	    realize_property (text, forward_prop);
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

	  new_prop->data = new_text_property (text, font, fore, back, len);

	  SET_PROPERTY_MARK (mark, new_prop, 0);
	}
    }
  else
    {
      /* The following will screw up the line_start cache,
       * we'll fix it up in correct_cache_insert
       */
      
      /* In the middle of forward_prop, if properties are equal,
       * just add to its length, else split it into two and splice
       * in a new one. */
      if (text_properties_equal (forward_prop, font, fore, back))
	{
	  forward_prop->length += len;
	}
      else if ((MARK_NEXT_LIST_PTR(mark) == NULL) &&
	       (MARK_OFFSET(mark) + 1 == forward_prop->length))
	{
	  /* Inserting before only the last position in the text */
	  
	  GList* new_prop;
	  forward_prop->length -= 1;
	  
	  new_prop = g_list_alloc();
	  new_prop->data = new_text_property (text, font, fore, back, len+1);
	  new_prop->prev = MARK_LIST_PTR(mark);
	  new_prop->next = NULL;
	  MARK_NEXT_LIST_PTR(mark) = new_prop;
	  
	  SET_PROPERTY_MARK (mark, new_prop, 0);
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

	  new_prop_forward->data = 
	    new_text_property(text,
			      forward_prop->flags & PROPERTY_FONT ? 
                                     forward_prop->font->gdk_font : NULL,
			      forward_prop->flags & PROPERTY_FOREGROUND ? 
  			             &forward_prop->fore_color : NULL,
			      forward_prop->flags & PROPERTY_BACKGROUND ? 
  			             &forward_prop->back_color : NULL,
			      old_length - forward_prop->length);

	  new_prop->data = new_text_property(text, font, fore, back, len);

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
  
  /* Deleting text properties is problematical, because we
   * might be storing around marks pointing to a property.
   *
   * The marks in question and how we handle them are:
   *
   *  point: We know the new value, since it will be at the
   *         end of the deleted text, and we move it there
   *         first.
   *  cursor: We just remove the mark and set it equal to the
   *         point after the operation.
   *  line-start cache: We replace most affected lines.
   *         The current line gets used to fetch the new
   *         lines so, if necessary, (delete at the beginning
   *         of a line) we fix it up by setting it equal to the
   *         point.
   */
  
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

	  if (gtk_widget_get_realized (GTK_WIDGET (text)))
	    unrealize_property (text, prop);

	  destroy_text_property (prop);
	  g_list_free_1 (tmp);
	  
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
  
  /* Check to see if we have just the single final position remaining
   * along in a property; if so, combine it with the previous property
   */
  if (LAST_INDEX (text, text->point) && 
      (MARK_OFFSET (&text->point) == 0) &&
      (MARK_PREV_LIST_PTR(&text->point) != NULL))
    {
      tmp = MARK_LIST_PTR (&text->point);
      prop = MARK_CURRENT_PROPERTY(&text->point);
      
      MARK_LIST_PTR (&text->point) = MARK_PREV_LIST_PTR (&text->point);
      MARK_CURRENT_PROPERTY(&text->point)->length += 1;
      MARK_NEXT_LIST_PTR(&text->point) = NULL;
      
      text->point.offset = MARK_CURRENT_PROPERTY(&text->point)->length - 1;
      
      if (gtk_widget_get_realized (GTK_WIDGET (text)))
	unrealize_property (text, prop);

      destroy_text_property (prop);
      g_list_free_1 (tmp);
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
      text->text_properties->data = new_text_property (text, NULL, NULL, NULL, 1);
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
advance_mark_n (GtkPropertyMark* mark, gint n)
{
  gint i;
  TextProperty* prop;

  g_assert (n > 0);

  i = 0;			/* otherwise it migth not be init. */
  prop = MARK_CURRENT_PROPERTY(mark);

  if ((prop->length - mark->offset - 1) < n) { /* if we need to change prop. */
    /* to make it easier */
    n += (mark->offset);
    mark->index -= mark->offset;
    mark->offset = 0;
    /* first we take seven-mile-leaps to get to the right text
     * property. */
    while ((n-i) > prop->length - 1) {
      i += prop->length;
      mark->index += prop->length;
      mark->property = MARK_NEXT_LIST_PTR (mark);
      prop = MARK_CURRENT_PROPERTY (mark);
    }
  }

  /* and then the rest */
  mark->index += n - i;
  mark->offset += n - i;
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
  g_assert (n > 0);

  while (mark->offset < n) {
    /* jump to end of prev */
    n -= mark->offset + 1;
    mark->index -= mark->offset + 1;
    mark->property = MARK_PREV_LIST_PTR (mark);
    mark->offset = MARK_CURRENT_PROPERTY (mark)->length - 1;
  }

  /* and the rest */
  mark->index -= n;
  mark->offset -= n;
}
 
static GtkPropertyMark
find_mark (GtkText* text, guint mark_position)
{
  return find_mark_near (text, mark_position, &text->point);
}

/*
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

  move_mark_n (&mark, mark_position - mark.index);
   
  return mark;
}

/* This routine must be called with scroll == FALSE, only when
 * point is at least partially on screen
 */

static void
find_line_containing_point (GtkText* text, guint point,
			    gboolean scroll)
{
  GList* cache;
  gint height;
  
  text->current_line = NULL;

  TEXT_SHOW (text);

  /* Scroll backwards until the point is on screen
   */
  while (CACHE_DATA(text->line_start_cache).start.index > point)
    scroll_int (text, - LINE_HEIGHT(CACHE_DATA(text->line_start_cache)));

  /* Now additionally try to make sure that the point is fully on screen
   */
  if (scroll)
    {
      while (text->first_cut_pixels != 0 && 
	     text->line_start_cache->next &&
	     CACHE_DATA(text->line_start_cache->next).start.index > point)
	scroll_int (text, - LINE_HEIGHT(CACHE_DATA(text->line_start_cache->next)));
    }

  gdk_drawable_get_size (text->text_area, NULL, &height);
  
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
      
      if (cache->next == NULL)
	fetch_lines_forward (text, 1);
      
      if (scroll)
	{
	  lph = pixel_height_of (text, cache->next);
	  
	  /* Scroll the bottom of the line is on screen, or until
	   * the line is the first onscreen line.
	   */
	  while (cache->next != text->line_start_cache && lph > height)
	    {
	      TEXT_SHOW_LINE (text, cache, "cache");
	      TEXT_SHOW_LINE (text, cache->next, "cache->next");
	      scroll_int (text, LINE_HEIGHT(CACHE_DATA(cache->next)));
	      lph = pixel_height_of (text, cache->next);
	    }
	}
    }
  
  g_assert_not_reached (); /* Must set text->current_line here */
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
  GdkWChar ch;
  gint16* char_widths;
  
  if (LAST_INDEX (text, *mark))
    return 0;
  
  ch = GTK_TEXT_INDEX (text, mark->index);
  char_widths = MARK_CURRENT_TEXT_FONT (text, mark)->char_widths;

  if (ch == '\t')
    {
      return tab_mark->to_next_tab * char_widths[' '];
    }
  else if (ch < 256)
    {
      return char_widths[ch];
    }
  else
    {
      return gdk_char_width_wc(MARK_CURRENT_TEXT_FONT(text, mark)->gdk_font, ch);
    }
}

static void
advance_tab_mark (GtkText* text, TabStopMark* tab_mark, GdkWChar ch)
{
  if (tab_mark->to_next_tab == 1 || ch == '\t')
    {
      if (tab_mark->tab_stops->next)
	{
	  tab_mark->tab_stops = tab_mark->tab_stops->next;
	  tab_mark->to_next_tab = (gintptr) tab_mark->tab_stops->data;
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
  GdkWChar ch;
  
  GtkPropertyMark mark        = start_line->start;
  TabStopMark  tab_mark    = start_line->tab_cont.tab_start;
  gint         pixel_width = LINE_START_PIXEL (*start_line);
  
  while (mark.index < text->cursor_mark.index)
    {
      pixel_width += find_char_width (text, &mark, &tab_mark);
      
      advance_tab_mark (text, &tab_mark, GTK_TEXT_INDEX(text, mark.index));
      advance_mark (&mark);
    }
  
  text->cursor_pos_x       = pixel_width;
  text->cursor_pos_y       = pixel_height;
  text->cursor_char_offset = start_line->font_descent;
  text->cursor_mark        = mark;
  
  ch = LAST_INDEX (text, mark) ? 
    LINE_DELIM : GTK_TEXT_INDEX (text, mark.index);
  
  if ((text->use_wchar) ? gdk_iswspace (ch) : isspace (ch))
    text->cursor_char = 0;
  else
    text->cursor_char = ch;
}

static void
find_cursor (GtkText* text, gboolean scroll)
{
  if (gtk_widget_get_realized (GTK_WIDGET (text)))
    {
      find_line_containing_point (text, text->cursor_mark.index, scroll);
      
      if (text->current_line)
	find_cursor_at_line (text,
			     &CACHE_DATA(text->current_line),
			     pixel_height_of(text, text->current_line));
    }
  
  GTK_OLD_EDITABLE (text)->current_pos = text->cursor_mark.index;
}

static void
find_mouse_cursor_at_line (GtkText *text, const LineParams* lp,
			   guint line_pixel_height,
			   gint button_x)
{
  GtkPropertyMark mark     = lp->start;
  TabStopMark  tab_mark = lp->tab_cont.tab_start;
  
  gint char_width = find_char_width(text, &mark, &tab_mark);
  gint pixel_width = LINE_START_PIXEL (*lp) + (char_width+1)/2;
  
  text->cursor_pos_y = line_pixel_height;
  
  for (;;)
    {
      GdkWChar ch = LAST_INDEX (text, mark) ? 
	LINE_DELIM : GTK_TEXT_INDEX (text, mark.index);
      
      if (button_x < pixel_width || mark.index == lp->end.index)
	{
	  text->cursor_pos_x       = pixel_width - (char_width+1)/2;
	  text->cursor_mark        = mark;
	  text->cursor_char_offset = lp->font_descent;
	  
	  if ((text->use_wchar) ? gdk_iswspace (ch) : isspace (ch))
	    text->cursor_char = 0;
	  else
	    text->cursor_char = ch;
	  
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
find_mouse_cursor (GtkText* text, gint x, gint y)
{
  gint pixel_height;
  GList* cache = text->line_start_cache;
  
  g_assert (cache);
  
  pixel_height = - text->first_cut_pixels;
  
  for (; cache; cache = cache->next)
    {
      pixel_height += LINE_HEIGHT(CACHE_DATA(cache));
      
      if (y < pixel_height || !cache->next)
	{
	  find_mouse_cursor_at_line (text, &CACHE_DATA(cache), pixel_height, x);
	  
	  find_cursor (text, FALSE);
	  
	  return;
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
  
  if (cache)
    {
      while (cache->prev)
	cache = cache->prev;
      
      text->line_start_cache = cache;
    }
  
  for (; cache; cache = cache->next)
    g_slice_free (LineParams, cache->data);
  
  g_list_free (text->line_start_cache);
  
  text->line_start_cache = NULL;
}

static GList*
remove_cache_line (GtkText* text, GList* member)
{
  GList *list;
  
  if (member == NULL)
    return NULL;
  
  if (member == text->line_start_cache)
    text->line_start_cache = text->line_start_cache->next;
  
  if (member->prev)
    member->prev->next = member->next;
  
  if (member->next)
    member->next->prev = member->prev;
  
  list = member->next;
  
  g_slice_free (LineParams, member->data);
  g_list_free_1 (member);
  
  return list;
}

/**********************************************************************/
/*			     Key Motion                               */
/**********************************************************************/

static void
move_cursor_buffer_ver (GtkText *text, int dir)
{
  undraw_cursor (text, FALSE);
  
  if (dir > 0)
    {
      scroll_int (text, text->vadj->upper);
      text->cursor_mark = find_this_line_start_mark (text,
						     TEXT_LENGTH (text),
						     &text->cursor_mark);
    }
  else
    {
      scroll_int (text, - text->vadj->value);
      text->cursor_mark = find_this_line_start_mark (text,
						     0,
						     &text->cursor_mark);
    }
  
  find_cursor (text, TRUE);
  draw_cursor (text, FALSE);
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
      
      while (!LAST_INDEX(text, mark) && GTK_TEXT_INDEX(text, mark.index) != LINE_DELIM)
	advance_mark (&mark);
      
      if (LAST_INDEX(text, mark))
	return;
      
      advance_mark (&mark);
    }
  
  for (i=0; i < text->cursor_virtual_x; i += 1, advance_mark(&mark))
    if (LAST_INDEX(text, mark) ||
	GTK_TEXT_INDEX(text, mark.index) == LINE_DELIM)
      break;
  
  undraw_cursor (text, FALSE);
  
  text->cursor_mark = mark;
  
  find_cursor (text, TRUE);
  
  draw_cursor (text, FALSE);
}

static void
move_cursor_hor (GtkText *text, int count)
{
  /* count should be +-1. */
  if ( (count > 0 && text->cursor_mark.index + count > TEXT_LENGTH(text)) ||
       (count < 0 && text->cursor_mark.index < (- count)) ||
       (count == 0) )
    return;
  
  text->cursor_virtual_x = 0;
  
  undraw_cursor (text, FALSE);
  
  move_mark_n (&text->cursor_mark, count);
  
  find_cursor (text, TRUE);
  
  draw_cursor (text, FALSE);
}

static void 
gtk_text_move_cursor (GtkOldEditable *old_editable,
		      gint            x,
		      gint            y)
{
  if (x > 0)
    {
      while (x-- != 0)
	move_cursor_hor (GTK_TEXT (old_editable), 1);
    }
  else if (x < 0)
    {
      while (x++ != 0)
	move_cursor_hor (GTK_TEXT (old_editable), -1);
    }
  
  if (y > 0)
    {
      while (y-- != 0)
	move_cursor_ver (GTK_TEXT (old_editable), 1);
    }
  else if (y < 0)
    {
      while (y++ != 0)
	move_cursor_ver (GTK_TEXT (old_editable), -1);
    }
}

static void
gtk_text_move_forward_character (GtkText *text)
{
  move_cursor_hor (text, 1);
}

static void
gtk_text_move_backward_character (GtkText *text)
{
  move_cursor_hor (text, -1);
}

static void
gtk_text_move_next_line (GtkText *text)
{
  move_cursor_ver (text, 1);
}

static void
gtk_text_move_previous_line (GtkText *text)
{
  move_cursor_ver (text, -1);
}

static void 
gtk_text_move_word (GtkOldEditable *old_editable,
		    gint            n)
{
  if (n > 0)
    {
      while (n-- != 0)
	gtk_text_move_forward_word (GTK_TEXT (old_editable));
    }
  else if (n < 0)
    {
      while (n++ != 0)
	gtk_text_move_backward_word (GTK_TEXT (old_editable));
    }
}

static void
gtk_text_move_forward_word (GtkText *text)
{
  text->cursor_virtual_x = 0;
  
  undraw_cursor (text, FALSE);
  
  if (text->use_wchar)
    {
      while (!LAST_INDEX (text, text->cursor_mark) && 
	     !gdk_iswalnum (GTK_TEXT_INDEX(text, text->cursor_mark.index)))
	advance_mark (&text->cursor_mark);
      
      while (!LAST_INDEX (text, text->cursor_mark) && 
	     gdk_iswalnum (GTK_TEXT_INDEX(text, text->cursor_mark.index)))
	advance_mark (&text->cursor_mark);
    }
  else
    {
      while (!LAST_INDEX (text, text->cursor_mark) && 
	     !isalnum (GTK_TEXT_INDEX(text, text->cursor_mark.index)))
	advance_mark (&text->cursor_mark);
      
      while (!LAST_INDEX (text, text->cursor_mark) && 
	     isalnum (GTK_TEXT_INDEX(text, text->cursor_mark.index)))
	advance_mark (&text->cursor_mark);
    }
  
  find_cursor (text, TRUE);
  draw_cursor (text, FALSE);
}

static void
gtk_text_move_backward_word (GtkText *text)
{
  text->cursor_virtual_x = 0;
  
  undraw_cursor (text, FALSE);
  
  if (text->use_wchar)
    {
      while ((text->cursor_mark.index > 0) &&
	     !gdk_iswalnum (GTK_TEXT_INDEX(text, text->cursor_mark.index-1)))
	decrement_mark (&text->cursor_mark);
      
      while ((text->cursor_mark.index > 0) &&
	     gdk_iswalnum (GTK_TEXT_INDEX(text, text->cursor_mark.index-1)))
	decrement_mark (&text->cursor_mark);
    }
  else
    {
      while ((text->cursor_mark.index > 0) &&
	     !isalnum (GTK_TEXT_INDEX(text, text->cursor_mark.index-1)))
	decrement_mark (&text->cursor_mark);
      
      while ((text->cursor_mark.index > 0) &&
	     isalnum (GTK_TEXT_INDEX(text, text->cursor_mark.index-1)))
	decrement_mark (&text->cursor_mark);
    }
  
  find_cursor (text, TRUE);
  draw_cursor (text, FALSE);
}

static void 
gtk_text_move_page (GtkOldEditable *old_editable,
		    gint            x,
		    gint            y)
{
  if (y != 0)
    scroll_int (GTK_TEXT (old_editable), 
		y * GTK_TEXT(old_editable)->vadj->page_increment);  
}

static void 
gtk_text_move_to_row (GtkOldEditable *old_editable,
		      gint            row)
{
}

static void 
gtk_text_move_to_column (GtkOldEditable *old_editable,
			 gint            column)
{
  GtkText *text;
  
  text = GTK_TEXT (old_editable);
  
  text->cursor_virtual_x = 0;	/* FIXME */
  
  undraw_cursor (text, FALSE);
  
  /* Move to the beginning of the line */
  while ((text->cursor_mark.index > 0) &&
	 (GTK_TEXT_INDEX (text, text->cursor_mark.index - 1) != LINE_DELIM))
    decrement_mark (&text->cursor_mark);
  
  while (!LAST_INDEX (text, text->cursor_mark) &&
	 (GTK_TEXT_INDEX (text, text->cursor_mark.index) != LINE_DELIM))
    {
      if (column > 0)
	column--;
      else if (column == 0)
	break;
      
      advance_mark (&text->cursor_mark);
    }
  
  find_cursor (text, TRUE);
  draw_cursor (text, FALSE);
}

static void
gtk_text_move_beginning_of_line (GtkText *text)
{
  gtk_text_move_to_column (GTK_OLD_EDITABLE (text), 0);
  
}

static void
gtk_text_move_end_of_line (GtkText *text)
{
  gtk_text_move_to_column (GTK_OLD_EDITABLE (text), -1);
}

static void 
gtk_text_kill_char (GtkOldEditable *old_editable,
		    gint            direction)
{
  GtkText *text;
  
  text = GTK_TEXT (old_editable);
  
  if (old_editable->selection_start_pos != old_editable->selection_end_pos)
    gtk_editable_delete_selection (GTK_EDITABLE (old_editable));
  else
    {
      if (direction >= 0)
	{
	  if (text->point.index + 1 <= TEXT_LENGTH (text))
	    gtk_editable_delete_text (GTK_EDITABLE (old_editable), text->point.index, text->point.index + 1);
	}
      else
	{
	  if (text->point.index > 0)
	    gtk_editable_delete_text (GTK_EDITABLE (old_editable), text->point.index - 1, text->point.index);
	}
    }
}

static void
gtk_text_delete_forward_character (GtkText *text)
{
  gtk_text_kill_char (GTK_OLD_EDITABLE (text), 1);
}

static void
gtk_text_delete_backward_character (GtkText *text)
{
  gtk_text_kill_char (GTK_OLD_EDITABLE (text), -1);
}

static void 
gtk_text_kill_word (GtkOldEditable *old_editable,
		    gint            direction)
{
  if (old_editable->selection_start_pos != old_editable->selection_end_pos)
    gtk_editable_delete_selection (GTK_EDITABLE (old_editable));
  else
    {
      gint old_pos = old_editable->current_pos;
      if (direction >= 0)
	{
	  gtk_text_move_word (old_editable, 1);
	  gtk_editable_delete_text (GTK_EDITABLE (old_editable), old_pos, old_editable->current_pos);
	}
      else
	{
	  gtk_text_move_word (old_editable, -1);
	  gtk_editable_delete_text (GTK_EDITABLE (old_editable), old_editable->current_pos, old_pos);
	}
    }
}

static void
gtk_text_delete_forward_word (GtkText *text)
{
  gtk_text_kill_word (GTK_OLD_EDITABLE (text), 1);
}

static void
gtk_text_delete_backward_word (GtkText *text)
{
  gtk_text_kill_word (GTK_OLD_EDITABLE (text), -1);
}

static void 
gtk_text_kill_line (GtkOldEditable *old_editable,
		    gint            direction)
{
  gint old_pos = old_editable->current_pos;
  if (direction >= 0)
    {
      gtk_text_move_to_column (old_editable, -1);
      gtk_editable_delete_text (GTK_EDITABLE (old_editable), old_pos, old_editable->current_pos);
    }
  else
    {
      gtk_text_move_to_column (old_editable, 0);
      gtk_editable_delete_text (GTK_EDITABLE (old_editable), old_editable->current_pos, old_pos);
    }
}

static void
gtk_text_delete_line (GtkText *text)
{
  gtk_text_move_to_column (GTK_OLD_EDITABLE (text), 0);
  gtk_text_kill_line (GTK_OLD_EDITABLE (text), 1);
}

static void
gtk_text_delete_to_line_end (GtkText *text)
{
  gtk_text_kill_line (GTK_OLD_EDITABLE (text), 1);
}

static void
gtk_text_select_word (GtkText *text, guint32 time)
{
  gint start_pos;
  gint end_pos;
  
  GtkOldEditable *old_editable;
  old_editable = GTK_OLD_EDITABLE (text);
  
  gtk_text_move_backward_word (text);
  start_pos = text->cursor_mark.index;
  
  gtk_text_move_forward_word (text);
  end_pos = text->cursor_mark.index;
  
  old_editable->has_selection = TRUE;
  gtk_text_set_selection (old_editable, start_pos, end_pos);
  gtk_old_editable_claim_selection (old_editable, start_pos != end_pos, time);
}

static void
gtk_text_select_line (GtkText *text, guint32 time)
{
  gint start_pos;
  gint end_pos;
  
  GtkOldEditable *old_editable;
  old_editable = GTK_OLD_EDITABLE (text);
  
  gtk_text_move_beginning_of_line (text);
  start_pos = text->cursor_mark.index;
  
  gtk_text_move_end_of_line (text);
  gtk_text_move_forward_character (text);
  end_pos = text->cursor_mark.index;
  
  old_editable->has_selection = TRUE;
  gtk_text_set_selection (old_editable, start_pos, end_pos);
  gtk_old_editable_claim_selection (old_editable, start_pos != end_pos, time);
}

/**********************************************************************/
/*			      Scrolling                               */
/**********************************************************************/

static void
adjust_adj (GtkText* text, GtkAdjustment* adj)
{
  gint height;
  
  gdk_drawable_get_size (text->text_area, NULL, &height);
  
  adj->step_increment = MIN (adj->upper, SCROLL_PIXELS);
  adj->page_increment = MIN (adj->upper, height - KEY_SCROLL_PIXELS);
  adj->page_size      = MIN (adj->upper, height);
  adj->value          = MIN (adj->value, adj->upper - adj->page_size);
  adj->value          = MAX (adj->value, 0.0);
  
  gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");
}

static gint
set_vertical_scroll_iterator (GtkText* text, LineParams* lp, void* data)
{
  SetVerticalScrollData *svdata = (SetVerticalScrollData *) data;
  
  if ((text->first_line_start_index >= lp->start.index) &&
      (text->first_line_start_index <= lp->end.index))
    {
      svdata->mark = lp->start;
  
      if (text->first_line_start_index == lp->start.index)
	{
	  text->first_onscreen_ver_pixel = svdata->pixel_height + text->first_cut_pixels;
	}
      else
	{
	  text->first_onscreen_ver_pixel = svdata->pixel_height;
	  text->first_cut_pixels = 0;
	}
      
      text->vadj->value = text->first_onscreen_ver_pixel;
    }
  
  svdata->pixel_height += LINE_HEIGHT (*lp);
  
  return FALSE;
}

static gint
set_vertical_scroll_find_iterator (GtkText* text, LineParams* lp, void* data)
{
  SetVerticalScrollData *svdata = (SetVerticalScrollData *) data;
  gint return_val;
  
  if (svdata->pixel_height <= (gint) text->vadj->value &&
      svdata->pixel_height + LINE_HEIGHT(*lp) > (gint) text->vadj->value)
    {
      svdata->mark = lp->start;
      
      text->first_cut_pixels = (gint)text->vadj->value - svdata->pixel_height;
      text->first_onscreen_ver_pixel = svdata->pixel_height;
      text->first_line_start_index = lp->start.index;
      
      return_val = TRUE;
    }
  else
    {
      svdata->pixel_height += LINE_HEIGHT (*lp);
      
      return_val = FALSE;
    }
  
  return return_val;
}

static GtkPropertyMark
set_vertical_scroll (GtkText* text)
{
  GtkPropertyMark mark = find_mark (text, 0);
  SetVerticalScrollData data;
  gint height;
  gint orig_value;
  
  data.pixel_height = 0;
  line_params_iterate (text, &mark, NULL, FALSE, &data, set_vertical_scroll_iterator);
  
  text->vadj->upper = data.pixel_height;
  orig_value = (gint) text->vadj->value;
  
  gdk_drawable_get_size (text->text_area, NULL, &height);
  
  text->vadj->step_increment = MIN (text->vadj->upper, SCROLL_PIXELS);
  text->vadj->page_increment = MIN (text->vadj->upper, height - KEY_SCROLL_PIXELS);
  text->vadj->page_size      = MIN (text->vadj->upper, height);
  text->vadj->value          = MIN (text->vadj->value, text->vadj->upper - text->vadj->page_size);
  text->vadj->value          = MAX (text->vadj->value, 0.0);
  
  text->last_ver_value = (gint)text->vadj->value;
  
  gtk_signal_emit_by_name (GTK_OBJECT (text->vadj), "changed");
  
  if (text->vadj->value != orig_value)
    {
      /* We got clipped, and don't really know which line to put first. */
      data.pixel_height = 0;
      data.last_didnt_wrap = TRUE;
      
      line_params_iterate (text, &mark, NULL,
			   FALSE, &data,
			   set_vertical_scroll_find_iterator);
    }

  return data.mark;
}

static void
scroll_int (GtkText* text, gint diff)
{
  gdouble upper;
  
  text->vadj->value += diff;
  
  upper = text->vadj->upper - text->vadj->page_size;
  text->vadj->value = MIN (text->vadj->value, upper);
  text->vadj->value = MAX (text->vadj->value, 0.0);
  
  gtk_signal_emit_by_name (GTK_OBJECT (text->vadj), "value-changed");
}

static void 
process_exposes (GtkText *text)
{
  GdkEvent *event;
  
  /* Make sure graphics expose events are processed before scrolling
   * again */
  
  while ((event = gdk_event_get_graphics_expose (text->text_area)) != NULL)
    {
      gtk_widget_send_expose (GTK_WIDGET (text), event);
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
  
  gdk_drawable_get_size (text->text_area, NULL, &height);
  
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
      g_assert (text->line_start_cache);
      
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
  
  gdk_drawable_get_size (text->text_area, &width, &height);
  if (height > real_diff)
    gdk_draw_drawable (text->text_area,
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
	find_mouse_cursor (text, text->cursor_pos_x,
			   first_visible_line_height (text));
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
  
  gdk_drawable_get_size (text->text_area, &width, &height);
  if (height > real_diff)
    gdk_draw_drawable (text->text_area,
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
      gdk_drawable_get_size (text->text_area, NULL, &height);
      
      if (cursor_max >= height)
	find_mouse_cursor (text, text->cursor_pos_x,
			   last_visible_line_height (text));
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
  GdkWChar ch;
  gint ch_width;
  GdkFont *font;
  
  gdk_drawable_get_size (text->text_area, (gint*) &max_display_pixels, NULL);
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
      
      ch   = GTK_TEXT_INDEX (text, lp.end.index);
      font = MARK_CURRENT_FONT (text, &lp.end);

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
      
      if ((ch_width + lp.pixel_width > max_display_pixels) &&
	  (lp.end.index > lp.start.index))
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
		  gint space_width  = MARK_CURRENT_TEXT_FONT(text, &lp.end)->char_widths[' '];
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
		  if (text->word_wrap)
		    {
		      GtkPropertyMark saved_mark = lp.end;
		      guint saved_characters = lp.displayable_chars;
		      
		      lp.displayable_chars += 1;
		      
		      if (text->use_wchar)
			{
			  while (!gdk_iswspace (GTK_TEXT_INDEX (text, lp.end.index)) &&
				 (lp.end.index > lp.start.index))
			    {
			      decrement_mark (&lp.end);
			      lp.displayable_chars -= 1;
			    }
			}
		      else
			{
			  while (!isspace(GTK_TEXT_INDEX (text, lp.end.index)) &&
				 (lp.end.index > lp.start.index))
			    {
			      decrement_mark (&lp.end);
			      lp.displayable_chars -= 1;
			    }
			}
		      
		      /* If whole line is one word, revert to char wrapping */
		      if (lp.end.index == lp.start.index)
			{
			  lp.end = saved_mark;
			  lp.displayable_chars = saved_characters;
			  decrement_mark (&lp.end);
			}
		    }
		  else
		    {
		      /* Don't include this character, it will wrap. */
		      decrement_mark (&lp.end);
		    }
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
      font = MARK_CURRENT_FONT (text, &lp.end);

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
      
      if (text->use_wchar)
        {
	  if (text->scratch_buffer.wc)
	    text->scratch_buffer.wc = g_new (GdkWChar, i);
	  else
	    text->scratch_buffer.wc = g_realloc (text->scratch_buffer.wc,
					      i*sizeof (GdkWChar));
        }
      else
        {
	  if (text->scratch_buffer.ch)
	    text->scratch_buffer.ch = g_new (guchar, i);
	  else
	    text->scratch_buffer.ch = g_realloc (text->scratch_buffer.ch, i);
        }
      
      text->scratch_buffer_len = i;
    }
}

/* Side effect: modifies text->gc
 */
static void
draw_bg_rect (GtkText* text, GtkPropertyMark *mark,
	      gint x, gint y, gint width, gint height,
	      gboolean already_cleared)
{
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (text);

  if ((mark->index >= MIN(old_editable->selection_start_pos, old_editable->selection_end_pos) &&
       mark->index < MAX(old_editable->selection_start_pos, old_editable->selection_end_pos)))
    {
      gtk_paint_flat_box(GTK_WIDGET(text)->style, text->text_area,
			 old_editable->has_selection ?
			    GTK_STATE_SELECTED : GTK_STATE_ACTIVE, 
			 GTK_SHADOW_NONE,
			 NULL, GTK_WIDGET(text), "text",
			 x, y, width, height);
    }
  else if (!gdk_color_equal(MARK_CURRENT_BACK (text, mark),
			    &GTK_WIDGET(text)->style->base[gtk_widget_get_state (GTK_WIDGET (text))]))
    {
      gdk_gc_set_foreground (text->gc, MARK_CURRENT_BACK (text, mark));

      gdk_draw_rectangle (text->text_area,
			  text->gc,
			  TRUE, x, y, width, height);
    }
  else if (GTK_WIDGET (text)->style->bg_pixmap[GTK_STATE_NORMAL])
    {
      GdkRectangle rect;
      
      rect.x = x;
      rect.y = y;
      rect.width = width;
      rect.height = height;
      
      clear_area (text, &rect);
    }
  else if (!already_cleared)
    gdk_window_clear_area (text->text_area, x, y, width, height);
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
  union { GdkWChar *wc; guchar *ch; } buffer;
  GdkGC *fg_gc;
  
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (text);
  
  guint selection_start_pos = MIN (old_editable->selection_start_pos, old_editable->selection_end_pos);
  guint selection_end_pos = MAX (old_editable->selection_start_pos, old_editable->selection_end_pos);
  
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
      
      if (text->use_wchar)
	{
	  for (i = 0; i < chars; i += 1)
	    text->scratch_buffer.wc[i] = GTK_TEXT_INDEX(text, mark.index + i);
          buffer.wc = text->scratch_buffer.wc;
	}
      else
	{
	  for (i = 0; i < chars; i += 1)
	    text->scratch_buffer.ch[i] = GTK_TEXT_INDEX(text, mark.index + i);
	  buffer.ch = text->scratch_buffer.ch;
	}
    }
  else
    {
      if (text->use_wchar)
	{
	  if (mark.index >= text->gap_position)
	    buffer.wc = text->text.wc + mark.index + text->gap_size;
	  else
	    buffer.wc = text->text.wc + mark.index;
	}
      else
	{
	  if (mark.index >= text->gap_position)
	    buffer.ch = text->text.ch + mark.index + text->gap_size;
	  else
	    buffer.ch = text->text.ch + mark.index;
	}
    }
  
  
  if (running_offset > 0)
    {
      draw_bg_rect (text, &mark, 0, pixel_start_height, running_offset,
		    LINE_HEIGHT (*lp), TRUE);
    }
  
  while (chars > 0)
    {
      len = 0;
      if ((text->use_wchar && buffer.wc[0] != '\t') ||
	  (!text->use_wchar && buffer.ch[0] != '\t'))
	{
	  union { GdkWChar *wc; guchar *ch; } next_tab;
	  gint pixel_width;
	  GdkFont *font;

	  next_tab.wc = NULL;
	  if (text->use_wchar)
	    for (i=0; i<chars; i++)
	      {
		if (buffer.wc[i] == '\t')
		  {
		    next_tab.wc = buffer.wc + i;
		    break;
		  }
	      }
	  else
	    next_tab.ch = memchr (buffer.ch, '\t', chars);

	  len = MIN (MARK_CURRENT_PROPERTY (&mark)->length - mark.offset, chars);
	  
	  if (text->use_wchar)
	    {
	      if (next_tab.wc)
		len = MIN (len, next_tab.wc - buffer.wc);
	    }
	  else
	    {
	      if (next_tab.ch)
		len = MIN (len, next_tab.ch - buffer.ch);
	    }

	  if (mark.index < selection_start_pos)
	    len = MIN (len, selection_start_pos - mark.index);
	  else if (mark.index < selection_end_pos)
	    len = MIN (len, selection_end_pos - mark.index);

	  font = MARK_CURRENT_FONT (text, &mark);
	  if (font->type == GDK_FONT_FONT)
	    {
	      gdk_gc_set_font (text->gc, font);
	      gdk_gc_get_values (text->gc, &gc_values);
	      if (text->use_wchar)
	        pixel_width = gdk_text_width_wc (gc_values.font,
						 buffer.wc, len);
	      else
	      pixel_width = gdk_text_width (gc_values.font,
					    (gchar *)buffer.ch, len);
	    }
	  else
	    {
	      if (text->use_wchar)
		pixel_width = gdk_text_width_wc (font, buffer.wc, len);
	      else
		pixel_width = gdk_text_width (font, (gchar *)buffer.ch, len);
	    }
	  
	  draw_bg_rect (text, &mark, running_offset, pixel_start_height,
			pixel_width, LINE_HEIGHT (*lp), TRUE);
	  
	  if ((mark.index >= selection_start_pos) && 
	      (mark.index < selection_end_pos))
	    {
	      if (old_editable->has_selection)
		fg_gc = GTK_WIDGET(text)->style->text_gc[GTK_STATE_SELECTED];
	      else
		fg_gc = GTK_WIDGET(text)->style->text_gc[GTK_STATE_ACTIVE];
	    }
	  else
	    {
	      gdk_gc_set_foreground (text->gc, MARK_CURRENT_FORE (text, &mark));
	      fg_gc = text->gc;
	    }

	  if (text->use_wchar)
	    gdk_draw_text_wc (text->text_area, MARK_CURRENT_FONT (text, &mark),
			      fg_gc,
			      running_offset,
			      pixel_height,
			      buffer.wc,
			      len);
	  else
	    gdk_draw_text (text->text_area, MARK_CURRENT_FONT (text, &mark),
			   fg_gc,
			   running_offset,
			   pixel_height,
			   (gchar *)buffer.ch,
			   len);
	  
	  running_offset += pixel_width;
	  
	  advance_tab_mark_n (text, &tab_mark, len);
	}
      else
	{
	  gint pixels_remaining;
	  gint space_width;
	  gint spaces_avail;
	      
	  len = 1;
	  
	  gdk_drawable_get_size (text->text_area, &pixels_remaining, NULL);
	  pixels_remaining -= (LINE_WRAP_ROOM + running_offset);
	  
	  space_width = MARK_CURRENT_TEXT_FONT(text, &mark)->char_widths[' '];
	  
	  spaces_avail = pixels_remaining / space_width;
	  spaces_avail = MIN (spaces_avail, tab_mark.to_next_tab);

	  draw_bg_rect (text, &mark, running_offset, pixel_start_height,
			spaces_avail * space_width, LINE_HEIGHT (*lp), TRUE);

	  running_offset += tab_mark.to_next_tab *
	    MARK_CURRENT_TEXT_FONT(text, &mark)->char_widths[' '];

	  advance_tab_mark (text, &tab_mark, '\t');
	}
      
      advance_mark_n (&mark, len);
      if (text->use_wchar)
	buffer.wc += len;
      else
	buffer.ch += len;
      chars -= len;
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
  
  gdk_drawable_get_size (text->text_area, &width, NULL);
  width -= LINE_WRAP_ROOM;
  
  gdk_gc_set_stipple (text->gc,
		      bitmap);
  
  gdk_gc_set_fill (text->gc, GDK_STIPPLED);
  
  gdk_gc_set_foreground (text->gc, &GTK_WIDGET (text)->style->text[GTK_STATE_NORMAL]);
  
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
  GtkOldEditable *old_editable = (GtkOldEditable *) text;

  TDEBUG (("in undraw_cursor\n"));
  
  if (absolute)
    text->cursor_drawn_level = 0;
  
  if ((text->cursor_drawn_level ++ == 0) &&
      (old_editable->selection_start_pos == old_editable->selection_end_pos) &&
      GTK_WIDGET_DRAWABLE (text) && text->line_start_cache)
    {
      GdkFont* font;
      
      g_assert(text->cursor_mark.property);

      font = MARK_CURRENT_FONT(text, &text->cursor_mark);

      draw_bg_rect (text, &text->cursor_mark, 
		    text->cursor_pos_x,
		    text->cursor_pos_y - text->cursor_char_offset - font->ascent,
		    1, font->ascent + 1, FALSE);
      
      if (text->cursor_char)
	{
	  if (font->type == GDK_FONT_FONT)
	    gdk_gc_set_font (text->gc, font);

	  gdk_gc_set_foreground (text->gc, MARK_CURRENT_FORE (text, &text->cursor_mark));

	  gdk_draw_text_wc (text->text_area, font,
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
  GdkFont* font;
  
  g_assert(text->cursor_mark.property);
  
  font = MARK_CURRENT_FONT(text, &text->cursor_mark);
  
  return text->cursor_pos_y - text->cursor_char_offset - font->ascent;
}

static gint
drawn_cursor_max (GtkText* text)
{
  g_assert(text->cursor_mark.property);
  
  return text->cursor_pos_y - text->cursor_char_offset;
}

static void
draw_cursor (GtkText* text, gint absolute)
{
  GtkOldEditable *old_editable = (GtkOldEditable *)text;
  
  TDEBUG (("in draw_cursor\n"));
  
  if (absolute)
    text->cursor_drawn_level = 1;
  
  if ((--text->cursor_drawn_level == 0) &&
      old_editable->editable &&
      (old_editable->selection_start_pos == old_editable->selection_end_pos) &&
      GTK_WIDGET_DRAWABLE (text) && text->line_start_cache)
    {
      GdkFont* font;
      
      g_assert (text->cursor_mark.property);

      font = MARK_CURRENT_FONT (text, &text->cursor_mark);

      gdk_gc_set_foreground (text->gc, &GTK_WIDGET (text)->style->text[GTK_STATE_NORMAL]);
      
      gdk_draw_line (text->text_area, text->gc, text->cursor_pos_x,
		     text->cursor_pos_y - text->cursor_char_offset,
		     text->cursor_pos_x,
		     text->cursor_pos_y - text->cursor_char_offset - font->ascent);
    }
}

static GdkGC *
create_bg_gc (GtkText *text)
{
  GdkGCValues values;
  
  values.tile = GTK_WIDGET (text)->style->bg_pixmap[GTK_STATE_NORMAL];
  values.fill = GDK_TILED;

  return gdk_gc_new_with_values (text->text_area, &values,
				 GDK_GC_FILL | GDK_GC_TILE);
}

static void
clear_area (GtkText *text, GdkRectangle *area)
{
  GtkWidget *widget = GTK_WIDGET (text);
  
  if (text->bg_gc)
    {
      gint width, height;
      
      gdk_drawable_get_size (widget->style->bg_pixmap[GTK_STATE_NORMAL], &width, &height);
      
      gdk_gc_set_ts_origin (text->bg_gc,
			    (- text->first_onscreen_hor_pixel) % width,
			    (- text->first_onscreen_ver_pixel) % height);

      gdk_draw_rectangle (text->text_area, text->bg_gc, TRUE,
			  area->x, area->y, area->width, area->height);
    }
  else
    gdk_window_clear_area (text->text_area, area->x, area->y, area->width, area->height);
}

static void
expose_text (GtkText* text, GdkRectangle *area, gboolean cursor)
{
  GList *cache = text->line_start_cache;
  gint pixels = - text->first_cut_pixels;
  gint min_y = MAX (0, area->y);
  gint max_y = MAX (0, area->y + area->height);
  gint height;
  
  gdk_drawable_get_size (text->text_area, NULL, &height);
  max_y = MIN (max_y, height);
  
  TDEBUG (("in expose x=%d y=%d w=%d h=%d\n", area->x, area->y, area->width, area->height));
  
  clear_area (text, area);
  
  for (; pixels < height; cache = cache->next)
    {
      if (pixels < max_y && (pixels + (gint)LINE_HEIGHT(CACHE_DATA(cache))) >= min_y)
	{
	  draw_line (text, pixels, &CACHE_DATA(cache));
	  
	  if (CACHE_DATA(cache).wraps)
	    draw_line_wrap (text, pixels + CACHE_DATA(cache).font_ascent);
	}
      
      if (cursor && gtk_widget_has_focus (GTK_WIDGET (text)))
	{
	  if (CACHE_DATA(cache).start.index <= text->cursor_mark.index &&
	      CACHE_DATA(cache).end.index >= text->cursor_mark.index)
	    {
	      /* We undraw and draw the cursor here to get the drawn
	       * level right ... FIXME - maybe the second parameter
	       * of draw_cursor should work differently
	       */
	      undraw_cursor (text, FALSE);
	      draw_cursor (text, FALSE);
	    }
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
gtk_text_update_text (GtkOldEditable    *old_editable,
		      gint               start_pos,
		      gint               end_pos)
{
  GtkText *text = GTK_TEXT (old_editable);
  
  GList *cache = text->line_start_cache;
  gint pixels = - text->first_cut_pixels;
  GdkRectangle area;
  gint width;
  gint height;
  
  if (end_pos < 0)
    end_pos = TEXT_LENGTH (text);
  
  if (end_pos < start_pos)
    return;
  
  gdk_drawable_get_size (text->text_area, &width, &height);
  area.x = 0;
  area.y = -1;
  area.width = width;
  area.height = 0;
  
  TDEBUG (("in expose span start=%d stop=%d\n", start_pos, end_pos));
  
  for (; pixels < height; cache = cache->next)
    {
      if (CACHE_DATA(cache).start.index < end_pos)
	{
	  if (CACHE_DATA(cache).end.index >= start_pos)
	    {
	      if (area.y < 0)
		area.y = MAX(0,pixels);
	      area.height = pixels + LINE_HEIGHT(CACHE_DATA(cache)) - area.y;
	    }
	}
      else
	break;
      
      pixels += LINE_HEIGHT(CACHE_DATA(cache));
      
      if (!cache->next)
	{
	  fetch_lines_forward (text, 1);
	  
	  if (!cache->next)
	    break;
	}
    }
  
  if (area.y >= 0)
    expose_text (text, &area, TRUE);
}

static void
recompute_geometry (GtkText* text)
{
  GtkPropertyMark mark, start_mark;
  GList *new_lines;
  gint height;
  gint width;
  
  free_cache (text);
  
  mark = start_mark = set_vertical_scroll (text);

  /* We need a real start of a line when calling fetch_lines().
   * not the start of a wrapped line.
   */
  while (mark.index > 0 &&
	 GTK_TEXT_INDEX (text, mark.index - 1) != LINE_DELIM)
    decrement_mark (&mark);

  gdk_drawable_get_size (text->text_area, &width, &height);

  /* Fetch an entire line, to make sure that we get all the text
   * we backed over above, in addition to enough text to fill up
   * the space vertically
   */

  new_lines = fetch_lines (text,
			   &mark,
			   NULL,
			   FetchLinesCount,
			   1);

  mark = CACHE_DATA (g_list_last (new_lines)).end;
  if (!LAST_INDEX (text, mark))
    {
      advance_mark (&mark);

      new_lines = g_list_concat (new_lines, 
				 fetch_lines (text,
					      &mark,
					      NULL,
					      FetchLinesPixels,
					      height + text->first_cut_pixels));
    }

  /* Now work forward to the actual first onscreen line */

  while (CACHE_DATA (new_lines).start.index < start_mark.index)
    new_lines = new_lines->next;
  
  text->line_start_cache = new_lines;
  
  find_cursor (text, TRUE);
}

/**********************************************************************/
/*                            Selection                               */
/**********************************************************************/

static void 
gtk_text_set_selection  (GtkOldEditable  *old_editable,
			 gint             start,
			 gint             end)
{
  GtkText *text = GTK_TEXT (old_editable);
  
  guint start1, end1, start2, end2;
  
  if (end < 0)
    end = TEXT_LENGTH (text);
  
  start1 = MIN(start,end);
  end1 = MAX(start,end);
  start2 = MIN(old_editable->selection_start_pos, old_editable->selection_end_pos);
  end2 = MAX(old_editable->selection_start_pos, old_editable->selection_end_pos);
  
  if (start2 < start1)
    {
      guint tmp;
      
      tmp = start1; start1 = start2; start2 = tmp;
      tmp = end1;   end1   = end2;   end2   = tmp;
    }
  
  undraw_cursor (text, FALSE);
  old_editable->selection_start_pos = start;
  old_editable->selection_end_pos = end;
  draw_cursor (text, FALSE);
  
  /* Expose only what changed */
  
  if (start1 < start2)
    gtk_text_update_text (old_editable, start1, MIN(end1, start2));
  
  if (end2 > end1)
    gtk_text_update_text (old_editable, MAX(end1, start2), end2);
  else if (end2 < end1)
    gtk_text_update_text (old_editable, end2, end1);
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
    g_message ("Line Start Cache: ");
  
  if (cache == text->current_line)
    g_message("Current Line: ");
  
  g_message ("%s:%d: cache line %s s=%d,e=%d,lh=%d (",
	     func,
	     line,
	     what,
	     lp->start.index,
	     lp->end.index,
	     LINE_HEIGHT(*lp));
  
  for (i = lp->start.index; i < (lp->end.index + lp->wraps); i += 1)
    g_message ("%c", GTK_TEXT_INDEX (text, i));
  
  g_message (")\n");
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
  
  g_message ("*** line cache ***\n");
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
  
  g_message ("*** line markers ***\n");
  
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
  g_message ("*** adjustment ***\n");
  
  g_message ("%s:%d: %s adjustment l=%.1f u=%.1f v=%.1f si=%.1f pi=%.1f ps=%.1f\n",
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
  
  g_message ("%s:%d: ", msg, line);
  
  for (; props; props = props->next)
    {
      TextProperty *p = (TextProperty*)props->data;
      
      proplen += p->length;

      g_message ("[%d,%p,", p->length, p);
      if (p->flags & PROPERTY_FONT)
	g_message ("%p,", p->font);
      else
	g_message ("-,");
      if (p->flags & PROPERTY_FOREGROUND)
	g_message ("%ld, ", p->fore_color.pixel);
      else
	g_message ("-,");
      if (p->flags & PROPERTY_BACKGROUND)
	g_message ("%ld] ", p->back_color.pixel);
      else
	g_message ("-] ");
    }
  
  g_message ("\n");
  
  if (proplen - 1 != TEXT_LENGTH(text))
    g_warning ("incorrect property list length in %s:%d -- bad!", msg, line);
}
#endif

#define __GTK_TEXT_C__
#include "gtkaliasdef.c"
