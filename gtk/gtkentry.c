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
#ifdef USE_XIM
#include "gdk/gdkx.h"
#endif
#include "gdk/gdkkeysyms.h"
#include "gdk/gdki18n.h"
#include "gtkentry.h"
#include "gtkmain.h"
#include "gtkselection.h"
#include "gtksignal.h"

#define MIN_ENTRY_WIDTH  150
#define DRAW_TIMEOUT     20
#define INNER_BORDER     2

enum {
  INSERT_TEXT,
  DELETE_TEXT,
  CHANGED,
  SET_TEXT,
  ACTIVATE,
  LAST_SIGNAL
};


typedef void (*GtkTextFunction) (GtkEntry  *entry, GdkEventKey *event);
typedef void (*GtkEntrySignal1) (GtkObject *object,
				 gpointer   arg1,
				 gint       arg2,
				 gpointer   arg3,
				 gpointer   data);
typedef void (*GtkEntrySignal2) (GtkObject *object,
				 gint       arg1,
				 gint       arg2,
				 gpointer   data);


static void gtk_entry_marshal_signal_1     (GtkObject         *object,
					    GtkSignalFunc      func,
					    gpointer           func_data,
					    GtkArg            *args);
static void gtk_entry_marshal_signal_2     (GtkObject         *object,
					    GtkSignalFunc      func,
					    gpointer           func_data,
					    GtkArg            *args);

static void gtk_entry_class_init          (GtkEntryClass     *klass);
static void gtk_entry_init                (GtkEntry          *entry);
static void gtk_entry_destroy             (GtkObject         *object);
static void gtk_entry_realize             (GtkWidget         *widget);
static void gtk_entry_unrealize           (GtkWidget         *widget);
static void gtk_entry_draw_focus          (GtkWidget         *widget);
static void gtk_entry_size_request        (GtkWidget         *widget,
					   GtkRequisition    *requisition);
static void gtk_entry_size_allocate       (GtkWidget         *widget,
					   GtkAllocation     *allocation);
static void gtk_entry_draw                (GtkWidget         *widget,
					   GdkRectangle      *area);
static gint gtk_entry_expose              (GtkWidget         *widget,
					   GdkEventExpose    *event);
static gint gtk_entry_button_press        (GtkWidget         *widget,
					   GdkEventButton    *event);
static gint gtk_entry_button_release      (GtkWidget         *widget,
					   GdkEventButton    *event);
static gint gtk_entry_motion_notify       (GtkWidget         *widget,
					   GdkEventMotion    *event);
static gint gtk_entry_key_press           (GtkWidget         *widget,
					   GdkEventKey       *event);
static gint gtk_entry_focus_in            (GtkWidget         *widget,
					   GdkEventFocus     *event);
static gint gtk_entry_focus_out           (GtkWidget         *widget,
					   GdkEventFocus     *event);
static gint gtk_entry_selection_clear     (GtkWidget         *widget,
					   GdkEventSelection *event);
static void gtk_entry_selection_handler   (GtkWidget    *widget,
					   GtkSelectionData  *selection_data,
					   gpointer      data);
static void gtk_entry_selection_received  (GtkWidget         *widget,
					   GtkSelectionData  *selection_data);
static void gtk_entry_draw_text           (GtkEntry          *entry);
static void gtk_entry_draw_cursor         (GtkEntry          *entry);
static void gtk_entry_queue_draw          (GtkEntry          *entry);
static gint gtk_entry_timer               (gpointer           data);
static gint gtk_entry_position            (GtkEntry          *entry,
					   gint               x);
       void gtk_entry_adjust_scroll       (GtkEntry          *entry);
static void gtk_entry_grow_text           (GtkEntry          *entry);
static void gtk_entry_insert_text         (GtkEntry          *entry,
					   const gchar       *new_text,
					   gint               new_text_length,
					   gint              *position);
static void gtk_entry_delete_text         (GtkEntry          *entry,
					   gint               start_pos,
					   gint               end_pos);
static void gtk_real_entry_insert_text    (GtkEntry          *entry,
					   const gchar       *new_text,
					   gint               new_text_length,
					   gint              *position);
static void gtk_real_entry_delete_text    (GtkEntry          *entry,
					   gint               start_pos,
					   gint               end_pos);

static gint move_backward_character	  (gchar *str, gint index);
static void gtk_move_forward_character    (GtkEntry          *entry);
static void gtk_move_backward_character   (GtkEntry          *entry);
static void gtk_move_forward_word         (GtkEntry          *entry);
static void gtk_move_backward_word        (GtkEntry          *entry);
static void gtk_move_beginning_of_line    (GtkEntry          *entry);
static void gtk_move_end_of_line          (GtkEntry          *entry);
static void gtk_delete_forward_character  (GtkEntry          *entry);
static void gtk_delete_backward_character (GtkEntry          *entry);
static void gtk_delete_forward_word       (GtkEntry          *entry);
static void gtk_delete_backward_word      (GtkEntry          *entry);
static void gtk_delete_line               (GtkEntry          *entry);
static void gtk_delete_to_line_end        (GtkEntry          *entry);
static void gtk_delete_selection          (GtkEntry          *entry);
static void gtk_select_word               (GtkEntry          *entry);
static void gtk_select_line               (GtkEntry          *entry);
static void gtk_entry_cut_clipboard     (GtkEntry *entry, 
					 GdkEventKey *event);
static void gtk_entry_copy_clipboard    (GtkEntry *entry, 
					 GdkEventKey *event);
static void gtk_entry_paste_clipboard   (GtkEntry *entry, 
					 GdkEventKey *event);

static GtkWidgetClass *parent_class = NULL;
static gint entry_signals[LAST_SIGNAL] = { 0 };
static GdkAtom ctext_atom = GDK_NONE;
static GdkAtom text_atom = GDK_NONE;
static GdkAtom clipboard_atom = GDK_NONE;

static GtkTextFunction control_keys[26] =
{
  (GtkTextFunction)gtk_move_beginning_of_line,    /* a */
  (GtkTextFunction)gtk_move_backward_character,   /* b */
  gtk_entry_copy_clipboard,                       /* c */
  (GtkTextFunction)gtk_delete_forward_character,  /* d */
  (GtkTextFunction)gtk_move_end_of_line,          /* e */
  (GtkTextFunction)gtk_move_forward_character,    /* f */
  NULL,                                           /* g */
  (GtkTextFunction)gtk_delete_backward_character, /* h */
  NULL,                                           /* i */
  NULL,                                           /* j */
  (GtkTextFunction)gtk_delete_to_line_end,        /* k */
  NULL,                                           /* l */
  NULL,                                           /* m */
  NULL,                                           /* n */
  NULL,                                           /* o */
  NULL,                                           /* p */
  NULL,                                           /* q */
  NULL,                                           /* r */
  NULL,                                           /* s */
  NULL,                                           /* t */
  (GtkTextFunction)gtk_delete_line,               /* u */
  gtk_entry_paste_clipboard,                      /* v */
  (GtkTextFunction)gtk_delete_backward_word,      /* w */
  gtk_entry_cut_clipboard,                        /* x */
  NULL,                                           /* y */
  NULL,                                           /* z */
};

static GtkTextFunction alt_keys[26] =
{
  NULL,                                           /* a */
  (GtkTextFunction)gtk_move_backward_word,        /* b */
  NULL,                                           /* c */
  (GtkTextFunction)gtk_delete_forward_word,       /* d */
  NULL,                                           /* e */
  (GtkTextFunction)gtk_move_forward_word,         /* f */
  NULL,                                           /* g */
  NULL,                                           /* h */
  NULL,                                           /* i */
  NULL,                                           /* j */
  NULL,                                           /* k */
  NULL,                                           /* l */
  NULL,                                           /* m */
  NULL,                                           /* n */
  NULL,                                           /* o */
  NULL,                                           /* p */
  NULL,                                           /* q */
  NULL,                                           /* r */
  NULL,                                           /* s */
  NULL,                                           /* t */
  NULL,                                           /* u */
  NULL,                                           /* v */
  NULL,                                           /* w */
  NULL,                                           /* x */
  NULL,                                           /* y */
  NULL,                                           /* z */
};


guint
gtk_entry_get_type ()
{
  static guint entry_type = 0;

  if (!entry_type)
    {
      GtkTypeInfo entry_info =
      {
	"GtkEntry",
	sizeof (GtkEntry),
	sizeof (GtkEntryClass),
	(GtkClassInitFunc) gtk_entry_class_init,
	(GtkObjectInitFunc) gtk_entry_init,
	(GtkArgFunc) NULL,
      };

      entry_type = gtk_type_unique (gtk_widget_get_type (), &entry_info);
    }

  return entry_type;
}

static void
gtk_entry_class_init (GtkEntryClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class (gtk_widget_get_type ());

  entry_signals[INSERT_TEXT] =
    gtk_signal_new ("insert_text",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEntryClass, insert_text),
		    gtk_entry_marshal_signal_1,
		    GTK_TYPE_NONE, 3,
		    GTK_TYPE_STRING, GTK_TYPE_INT,
		    GTK_TYPE_POINTER);
  entry_signals[DELETE_TEXT] =
    gtk_signal_new ("delete_text",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEntryClass, delete_text),
		    gtk_entry_marshal_signal_2,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_INT, GTK_TYPE_INT);
  entry_signals[CHANGED] =
    gtk_signal_new ("changed",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEntryClass, changed),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  entry_signals[SET_TEXT] =
    gtk_signal_new ("set_text",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEntryClass, set_text),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  entry_signals[ACTIVATE] =
    gtk_signal_new ("activate",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEntryClass, activate),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, entry_signals, LAST_SIGNAL);

  object_class->destroy = gtk_entry_destroy;

  widget_class->realize = gtk_entry_realize;
  widget_class->unrealize = gtk_entry_unrealize;
  widget_class->draw_focus = gtk_entry_draw_focus;
  widget_class->size_request = gtk_entry_size_request;
  widget_class->size_allocate = gtk_entry_size_allocate;
  widget_class->draw = gtk_entry_draw;
  widget_class->expose_event = gtk_entry_expose;
  widget_class->button_press_event = gtk_entry_button_press;
  widget_class->button_release_event = gtk_entry_button_release;
  widget_class->motion_notify_event = gtk_entry_motion_notify;
  widget_class->key_press_event = gtk_entry_key_press;
  widget_class->focus_in_event = gtk_entry_focus_in;
  widget_class->focus_out_event = gtk_entry_focus_out;
  widget_class->selection_clear_event = gtk_entry_selection_clear;
  widget_class->selection_received = gtk_entry_selection_received;

  class->insert_text = gtk_real_entry_insert_text;
  class->delete_text = gtk_real_entry_delete_text;
  class->changed = gtk_entry_adjust_scroll;
  class->set_text = NULL; 	  /* user defined handling */
  class->activate = NULL; 	  /* user defined handling */
}

static void
gtk_entry_init (GtkEntry *entry)
{
  GTK_WIDGET_SET_FLAGS (entry, GTK_CAN_FOCUS);

  entry->text_area = NULL;
  entry->text = NULL;
  entry->text_size = 0;
  entry->text_length = 0;
  entry->text_max_length = 0;
  entry->current_pos = 0;
  entry->selection_start_pos = 0;
  entry->selection_end_pos = 0;
  entry->scroll_offset = 0;
  entry->have_selection = FALSE;
  entry->timer = 0;
  entry->visible = 1;
  entry->editable = 1;
  entry->clipboard_text = NULL;

#ifdef USE_XIM
  entry->ic = NULL;
#endif

  if (!clipboard_atom)
    clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);

  gtk_selection_add_handler (GTK_WIDGET(entry), GDK_SELECTION_PRIMARY,
			     GDK_TARGET_STRING, gtk_entry_selection_handler,
			     NULL, NULL);
  gtk_selection_add_handler (GTK_WIDGET(entry), clipboard_atom,
			     GDK_TARGET_STRING, gtk_entry_selection_handler,
			     NULL, NULL);

  if (!text_atom)
    text_atom = gdk_atom_intern ("TEXT", FALSE);

  gtk_selection_add_handler (GTK_WIDGET(entry), GDK_SELECTION_PRIMARY,
			     text_atom,
			     gtk_entry_selection_handler,
			     NULL, NULL);
  gtk_selection_add_handler (GTK_WIDGET(entry), clipboard_atom,
			     text_atom,
			     gtk_entry_selection_handler,
			     NULL, NULL);

  if (!ctext_atom)
    ctext_atom = gdk_atom_intern ("COMPOUND_TEXT", FALSE);

  gtk_selection_add_handler (GTK_WIDGET(entry), GDK_SELECTION_PRIMARY,
			     ctext_atom,
			     gtk_entry_selection_handler,
			     NULL, NULL);
  gtk_selection_add_handler (GTK_WIDGET(entry), clipboard_atom,
			     ctext_atom,
			     gtk_entry_selection_handler,
			     NULL, NULL);
}

GtkWidget*
gtk_entry_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_entry_get_type ()));
}

GtkWidget*
gtk_entry_new_with_max_length (guint16 max)
{
  GtkEntry *entry;
  entry = gtk_type_new (gtk_entry_get_type ());
  entry->text_max_length = max;
  return GTK_WIDGET (entry);
}

void
gtk_entry_set_text (GtkEntry *entry,
		    const gchar *text)
{
  gint tmp_pos;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_real_entry_delete_text (entry, 0, entry->text_length);

  tmp_pos = 0;
  gtk_entry_insert_text (entry, text, strlen (text), &tmp_pos);
  entry->current_pos = tmp_pos;

  entry->selection_start_pos = 0;
  entry->selection_end_pos = 0;

  if (GTK_WIDGET_DRAWABLE (entry))
    gtk_entry_draw_text (entry);

  gtk_signal_emit (GTK_OBJECT (entry), entry_signals[SET_TEXT]);
}

void
gtk_entry_append_text (GtkEntry *entry,
		       const gchar *text)
{
  gint tmp_pos;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  tmp_pos = entry->text_length;
  gtk_entry_insert_text (entry, text, strlen (text), &tmp_pos);
  entry->current_pos = tmp_pos;

  gtk_signal_emit (GTK_OBJECT (entry), entry_signals[SET_TEXT]);
}

void
gtk_entry_prepend_text (GtkEntry *entry,
			const gchar *text)
{
  gint tmp_pos;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  tmp_pos = 0;
  gtk_entry_insert_text (entry, text, strlen (text), &tmp_pos);
  entry->current_pos = tmp_pos;

  gtk_signal_emit (GTK_OBJECT (entry), entry_signals[SET_TEXT]);
}

void
gtk_entry_set_position (GtkEntry *entry,
			gint      position)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if ((position == -1) || (position > entry->text_length))
    entry->current_pos = entry->text_length;
  else
    entry->current_pos = position;
}

void
gtk_entry_set_visibility (GtkEntry *entry,
			  gboolean visible)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  entry->visible = visible;
}

void
gtk_entry_set_editable(GtkEntry *entry,
		       gboolean editable)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));
  entry->editable = editable;
  gtk_entry_queue_draw(entry);
}

gchar*
gtk_entry_get_text (GtkEntry *entry)
{
  static char empty_str[2] = "";

  g_return_val_if_fail (entry != NULL, NULL);
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  if (!entry->text)
    return empty_str;
  return entry->text;
}


static void
gtk_entry_marshal_signal_1 (GtkObject      *object,
			    GtkSignalFunc   func,
			    gpointer        func_data,
			    GtkArg         *args)
{
  GtkEntrySignal1 rfunc;

  rfunc = (GtkEntrySignal1) func;

  (* rfunc) (object, GTK_VALUE_STRING (args[0]), GTK_VALUE_INT (args[1]),
	     GTK_VALUE_POINTER (args[2]), func_data);
}

static void
gtk_entry_marshal_signal_2 (GtkObject      *object,
			    GtkSignalFunc   func,
			    gpointer        func_data,
			    GtkArg         *args)
{
  GtkEntrySignal2 rfunc;

  rfunc = (GtkEntrySignal2) func;

  (* rfunc) (object, GTK_VALUE_INT (args[0]), GTK_VALUE_INT (args[1]),
	     func_data);
}

static void
gtk_entry_destroy (GtkObject *object)
{
  GtkEntry *entry;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_ENTRY (object));

  entry = GTK_ENTRY (object);

#ifdef USE_XIM
  if (entry->ic)
    {
      gdk_ic_destroy (entry->ic);
      entry->ic = NULL;
    }
#endif

  if (entry->timer)
    gtk_timeout_remove (entry->timer);

  if (entry->text)
    g_free (entry->text);
  entry->text = NULL;

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_entry_realize (GtkWidget *widget)
{
  GtkEntry *entry;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  entry = GTK_ENTRY (widget);

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
			    GDK_BUTTON1_MOTION_MASK |
			    GDK_BUTTON3_MOTION_MASK |
			    GDK_POINTER_MOTION_HINT_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
			    GDK_KEY_PRESS_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, entry);

  attributes.x = widget->style->klass->xthickness + INNER_BORDER;
  attributes.y = widget->style->klass->ythickness + INNER_BORDER;
  attributes.width = widget->allocation.width - attributes.x * 2;
  attributes.height = widget->allocation.height - attributes.y * 2;

  entry->text_area = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (entry->text_area, entry);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_background (widget->window, &widget->style->white);
  gdk_window_set_background (entry->text_area, &widget->style->white);

#ifdef USE_XIM
  if (gdk_im_ready ())
    {
      GdkPoint spot;
      GdkRectangle rect;
      gint width, height;
      GdkEventMask mask;
      GdkIMStyle style;
      GdkIMStyle supported_style = GdkIMPreeditNone | GdkIMPreeditNothing |
			GdkIMPreeditPosition |
			GdkIMStatusNone | GdkIMStatusNothing;

      if (widget->style && widget->style->font->type != GDK_FONT_FONTSET)
	supported_style &= ~GdkIMPreeditPosition;

      style = gdk_im_decide_style (supported_style);
      switch (style & GdkIMPreeditMask)
	{
	case GdkIMPreeditPosition:
	  if (widget->style && widget->style->font->type != GDK_FONT_FONTSET)
	    {
	      g_warning ("over-the-spot style requires fontset");
	      break;
	    }
	  gdk_window_get_size (entry->text_area, &width, &height);
	  rect.x = 0;
	  rect.y = 0;
	  rect.width = width;
	  rect.height = height;
	  spot.x = 0;
	  spot.y = height;
	  entry->ic = gdk_ic_new (entry->text_area, entry->text_area,
  			       style,
			       "spotLocation", &spot,
			       "area", &rect,
			       "fontSet", GDK_FONT_XFONT (widget->style->font),
			       NULL);
	  break;
	default:
	  entry->ic = gdk_ic_new (entry->text_area, entry->text_area,
				  style, NULL);
	}
     
      if (entry->ic == NULL)
	g_warning ("Can't create input context.");
      else
	{
	  GdkColormap *colormap;

	  mask = gdk_window_get_events (entry->text_area);
	  mask |= gdk_ic_get_events (entry->ic);
	  gdk_window_set_events (entry->text_area, mask);

	  if ((colormap = gtk_widget_get_colormap (widget)) !=
	    	gtk_widget_get_default_colormap ())
	    {
	      gdk_ic_set_attr (entry->ic, "preeditAttributes",
	      		       "colorMap", GDK_COLORMAP_XCOLORMAP (colormap),
			       NULL);
	    }
	  gdk_ic_set_attr (entry->ic,"preeditAttributes",
		     "foreground", widget->style->fg[GTK_STATE_NORMAL].pixel,
		     "background", widget->style->white.pixel,
		     NULL);
	}
    }
#endif

  gdk_window_show (entry->text_area);
}

static void
gtk_entry_unrealize (GtkWidget *widget)
{
  GtkEntry *entry;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED);
  entry = GTK_ENTRY (widget);

  gtk_style_detach (widget->style);

  if (entry->text_area)
    {
      gdk_window_set_user_data (entry->text_area, NULL);
      gdk_window_destroy (entry->text_area);
    }
  if (widget->window)
    {
      gdk_window_set_user_data (widget->window, NULL);
      gdk_window_destroy (widget->window);
    }

  entry->text_area = NULL;
  widget->window = NULL;
}

static void
gtk_entry_draw_focus (GtkWidget *widget)
{
  gint width, height;
  gint x, y;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      x = 0;
      y = 0;
      gdk_window_get_size (widget->window, &width, &height);

      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  x += 1;
	  y += 1;
	  width -= 2;
	  height -= 2;
	}
      else
	{
	  gdk_draw_rectangle (widget->window, widget->style->white_gc, FALSE,
			      x + 2, y + 2, width - 5, height - 5);
	}

      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, GTK_SHADOW_IN,
		       x, y, width, height);

      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  gdk_window_get_size (widget->window, &width, &height);
	  gdk_draw_rectangle (widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
			      FALSE, 0, 0, width - 1, height - 1);
	}

      gtk_entry_draw_cursor (GTK_ENTRY (widget));
    }
}

static void
gtk_entry_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));
  g_return_if_fail (requisition != NULL);

  requisition->width = MIN_ENTRY_WIDTH + (widget->style->klass->xthickness + INNER_BORDER) * 2;
  requisition->height = (widget->style->font->ascent +
			 widget->style->font->descent +
			 (widget->style->klass->ythickness + INNER_BORDER) * 2);
}

static void
gtk_entry_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkEntry *entry;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  entry = GTK_ENTRY (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x,
			      allocation->y + (allocation->height - widget->requisition.height) / 2,
			      allocation->width, widget->requisition.height);
      gdk_window_move_resize (entry->text_area,
			      widget->style->klass->xthickness + INNER_BORDER,
			      widget->style->klass->ythickness + INNER_BORDER,
			      allocation->width - (widget->style->klass->xthickness + INNER_BORDER) * 2,
			      widget->requisition.height - (widget->style->klass->ythickness + INNER_BORDER) * 2);

      entry->scroll_offset = 0;
      gtk_entry_adjust_scroll (entry);
#ifdef USE_XIM
      if (entry->ic && (gdk_ic_get_style (entry->ic) & GdkIMPreeditPosition))
	{
	  gint width, height;
	  GdkRectangle rect;

	  gdk_window_get_size (entry->text_area, &width, &height);
	  rect.x = 0;
	  rect.y = 0;
	  rect.width = width;
	  rect.height = height;
	  gdk_ic_set_attr (entry->ic, "preeditAttributes", "area", &rect, NULL);
	}
#endif
    }
}

static void
gtk_entry_draw (GtkWidget    *widget,
		GdkRectangle *area)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_widget_draw_focus (widget);
      gtk_entry_draw_text (GTK_ENTRY (widget));
    }
}

static gint
gtk_entry_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkEntry *entry;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ENTRY (widget);

  if (widget->window == event->window)
    gtk_widget_draw_focus (widget);
  else if (entry->text_area == event->window)
    gtk_entry_draw_text (GTK_ENTRY (widget));

  return FALSE;
}

static gint
gtk_entry_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkEntry *entry;
  gint tmp_pos;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (ctext_atom == GDK_NONE)
    ctext_atom = gdk_atom_intern ("COMPOUND_TEXT", FALSE);

  entry = GTK_ENTRY (widget);
  if (!GTK_WIDGET_HAS_FOCUS (widget))
    gtk_widget_grab_focus (widget);

  if (event->button == 1)
    {
      switch (event->type)
	{
	case GDK_BUTTON_PRESS:
	  gtk_grab_add (widget);

	  tmp_pos = gtk_entry_position (entry, event->x + entry->scroll_offset);
	  gtk_entry_select_region (entry, tmp_pos, tmp_pos);
	  entry->current_pos = entry->selection_start_pos;
	  break;

	case GDK_2BUTTON_PRESS:
	  gtk_select_word (entry);
	  break;

	case GDK_3BUTTON_PRESS:
	  gtk_select_line (entry);
	  break;

	default:
	  break;
	}
    }
  else if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 2)
	{
	  if (entry->selection_start_pos == entry->selection_end_pos ||
	      entry->have_selection)
	    entry->current_pos = gtk_entry_position (entry, event->x + entry->scroll_offset);
	  gtk_selection_convert (widget, GDK_SELECTION_PRIMARY,
				 ctext_atom, event->time);
	}
      else
	{
	  gtk_grab_add (widget);

	  tmp_pos = gtk_entry_position (entry, event->x + entry->scroll_offset);
	  gtk_entry_select_region (entry, tmp_pos, tmp_pos);
	  entry->have_selection = FALSE;
	  entry->current_pos = entry->selection_start_pos;
	}
    }

  return FALSE;
}

static gint
gtk_entry_button_release (GtkWidget      *widget,
			  GdkEventButton *event)
{
  GtkEntry *entry;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->button == 1)
    {
      entry = GTK_ENTRY (widget);
      gtk_grab_remove (widget);

      entry->have_selection = FALSE;
      if (entry->selection_start_pos != entry->selection_end_pos)
	{
	  if (gtk_selection_owner_set (widget,
				       GDK_SELECTION_PRIMARY,
				       event->time))
	    {
	      entry->have_selection = TRUE;
	      gtk_entry_queue_draw (entry);
	    }
	}
      else
	{
	  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, event->time);
	}
    }
  else if (event->button == 3)
    {
      gtk_grab_remove (widget);
      if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == widget->window)
	gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, event->time);
    }

  return FALSE;
}

static gint
gtk_entry_motion_notify (GtkWidget      *widget,
			 GdkEventMotion *event)
{
  GtkEntry *entry;
  gint x;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ENTRY (widget);

  x = event->x;
  if (event->is_hint || (entry->text_area != event->window))
    gdk_window_get_pointer (entry->text_area, &x, NULL, NULL);

  entry->selection_end_pos = gtk_entry_position (entry, event->x + entry->scroll_offset);
  entry->current_pos = entry->selection_end_pos;
  gtk_entry_adjust_scroll (entry);
  gtk_entry_queue_draw (entry);

  return FALSE;
}

static gint
gtk_entry_key_press (GtkWidget   *widget,
		     GdkEventKey *event)
{
  GtkEntry *entry;
  gint return_val;
  gint key;
  gint tmp_pos;
  gint extend_selection;
  gint extend_start;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ENTRY (widget);
  return_val = FALSE;

  if(entry->editable == FALSE)
    return FALSE;

  extend_selection = event->state & GDK_SHIFT_MASK;
  extend_start = FALSE;

  if (extend_selection)
    {
      if (entry->selection_start_pos == entry->selection_end_pos)
	{
	  entry->selection_start_pos = entry->current_pos;
	  entry->selection_end_pos = entry->current_pos;
	}
      
      extend_start = (entry->current_pos == entry->selection_start_pos);
    }

  switch (event->keyval)
    {
    case GDK_BackSpace:
      return_val = TRUE;
      if (entry->selection_start_pos != entry->selection_end_pos)
	gtk_delete_selection (entry);
      else if (event->state & GDK_CONTROL_MASK)
	gtk_delete_backward_word (entry);
      else
	gtk_delete_backward_character (entry);
      break;
    case GDK_Clear:
      return_val = TRUE;
      gtk_delete_line (entry);
      break;
     case GDK_Insert:
       return_val = TRUE;
       if (event->state & GDK_SHIFT_MASK)
	 {
	   extend_selection = FALSE;
	   gtk_entry_paste_clipboard (entry, event);
	 }
       else if (event->state & GDK_CONTROL_MASK)
	 {
	   gtk_entry_copy_clipboard (entry, event);
	 }
       else
	 {
	   /* gtk_toggle_insert(entry) -- IMPLEMENT */
	 }
       break;
    case GDK_Delete:
      return_val = TRUE;
      if (entry->selection_start_pos != entry->selection_end_pos)
	gtk_delete_selection (entry);
      else
	{
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_delete_line (entry);
	  else if (event->state & GDK_SHIFT_MASK)
	    {
	      gtk_entry_cut_clipboard (entry, event);
	    }
	  else
	    gtk_delete_forward_character (entry);
	}
      break;
    case GDK_Home:
      return_val = TRUE;
      gtk_move_beginning_of_line (entry);
      break;
    case GDK_End:
      return_val = TRUE;
      gtk_move_end_of_line (entry);
      break;
    case GDK_Left:
      return_val = TRUE;
      gtk_move_backward_character (entry);
      break;
    case GDK_Right:
      return_val = TRUE;
      gtk_move_forward_character (entry);
      break;
    case GDK_Return:
      return_val = TRUE;
      gtk_signal_emit (GTK_OBJECT (entry), entry_signals[ACTIVATE]);
      break;
    /* The next two keys should not be inserted literally. Any others ??? */
    case GDK_Tab:
    case GDK_Escape:
      break;
    default:
      if ((event->keyval >= 0x20) && (event->keyval <= 0xFF))
	{
	  key = event->keyval;

	  if (event->state & GDK_CONTROL_MASK)
	    {
	      if ((key >= 'A') && (key <= 'Z'))
		key -= 'A' - 'a';

	      if ((key >= 'a') && (key <= 'z') && control_keys[key - 'a'])
		{
		  (* control_keys[key - 'a']) (entry, event);
		  return_val = TRUE;
		}
	      break;
	    }
	  else if (event->state & GDK_MOD1_MASK)
	    {
	      if ((key >= 'A') && (key <= 'Z'))
		key -= 'A' - 'a';

	      if ((key >= 'a') && (key <= 'z') && alt_keys[key - 'a'])
		{
		  (* alt_keys[key - 'a']) (entry, event);
		  return_val = TRUE;
		}
	      break;
	    }
	}
      if (event->length > 0)
	{
	  extend_selection = FALSE;
	  gtk_delete_selection (entry);

	  tmp_pos = entry->current_pos;
	  gtk_entry_insert_text (entry, event->string, event->length, &tmp_pos);
	  entry->current_pos = tmp_pos;

	  return_val = TRUE;
	}
      break;
    }

  if (return_val)
    {
      if (extend_selection)
	{
	  if (entry->current_pos < entry->selection_start_pos)
	    entry->selection_start_pos = entry->current_pos;
	  else if (entry->current_pos > entry->selection_end_pos)
	    entry->selection_end_pos = entry->current_pos;
	  else
	    {
	      if (extend_start)
		entry->selection_start_pos = entry->current_pos;
	      else
		entry->selection_end_pos = entry->current_pos;
	    }
	}
      else
	{
	  entry->selection_start_pos = 0;
	  entry->selection_end_pos = 0;
	}

      /* alex stuff */
      if (entry->selection_start_pos != entry->selection_end_pos)
	{
	  if (gtk_selection_owner_set (widget, GDK_SELECTION_PRIMARY, event->time))
	    {
	      entry->have_selection = TRUE;
	      gtk_entry_queue_draw (entry);
	    }
	}
      else
	{
	  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, event->time);
	}
      /* end of alex stuff */
      
      gtk_entry_adjust_scroll (entry);
      gtk_entry_queue_draw (entry);
    }

  return return_val;
}

static gint
gtk_entry_focus_in (GtkWidget     *widget,
		    GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

#ifdef USE_XIM
  if (GTK_ENTRY(widget)->ic)
    gdk_im_begin (GTK_ENTRY(widget)->ic, GTK_ENTRY(widget)->text_area);
#endif

  return FALSE;
}

static gint
gtk_entry_focus_out (GtkWidget     *widget,
		     GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

#ifdef USE_XIM
  gdk_im_end ();
#endif

  return FALSE;
}

static gint
gtk_entry_selection_clear (GtkWidget         *widget,
			   GdkEventSelection *event)
{
  GtkEntry *entry;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  /* Let the selection handling code know that the selection
   * has been changed, since we've overriden the default handler */
  gtk_selection_clear (widget, event);

  entry = GTK_ENTRY (widget);

  if (event->selection == GDK_SELECTION_PRIMARY)
    {
      if (entry->have_selection)
	{
	  entry->have_selection = FALSE;
	  gtk_entry_queue_draw (entry);
	}
    }
  else if (event->selection == clipboard_atom)
    {
      g_free (entry->clipboard_text);
      entry->clipboard_text = NULL;
    }

  return FALSE;
}

static void
gtk_entry_selection_handler (GtkWidget        *widget,
			     GtkSelectionData *selection_data,
			     gpointer          data)
{
  GtkEntry *entry;
  gint selection_start_pos;
  gint selection_end_pos;

  guchar *str;
  gint length;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));

  entry = GTK_ENTRY (widget);

  if (selection_data->selection == GDK_SELECTION_PRIMARY)
    {
      selection_start_pos = MIN (entry->selection_start_pos, entry->selection_end_pos);
      selection_end_pos = MAX (entry->selection_start_pos, entry->selection_end_pos);
      str = &entry->text[selection_start_pos];
      length = selection_end_pos - selection_start_pos;
    }
  else				/* CLIPBOARD */
    {
      if (!entry->clipboard_text)
	return;			/* Refuse */

      str = entry->clipboard_text;
      length = strlen (entry->clipboard_text);
    }
  
  if (selection_data->target == GDK_SELECTION_TYPE_STRING)
    {
      gtk_selection_data_set (selection_data,
                              GDK_SELECTION_TYPE_STRING,
                              8*sizeof(gchar), str, length);
    }
  else if (selection_data->target == text_atom ||
           selection_data->target == ctext_atom)
    {
      guchar *text;
      gchar c;
      GdkAtom encoding;
      gint format;
      gint new_length;

      c = str[length];
      str[length] = '\0';
      gdk_string_to_compound_text (str, &encoding, &format, &text, &new_length);
      gtk_selection_data_set (selection_data, encoding, format, text, new_length);
      gdk_free_compound_text (text);
      str[length] = c;
    }
}

static void
gtk_entry_selection_received  (GtkWidget         *widget,
			       GtkSelectionData  *selection_data)
{
  GtkEntry *entry;
  gint reselect;
  gint old_pos;
  gint tmp_pos;
  enum {INVALID, STRING, CTEXT} type;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));

  entry = GTK_ENTRY (widget);

  if (selection_data->type == GDK_TARGET_STRING)
    type = STRING;
  else if (selection_data->type == ctext_atom)
    type = CTEXT;
  else
    type = INVALID;

  if (type == INVALID || selection_data->length < 0)
    {
    /* avoid infinite loop */
    if (selection_data->target != GDK_TARGET_STRING)
      gtk_selection_convert (widget, selection_data->selection,
			     GDK_TARGET_STRING, GDK_CURRENT_TIME);
    return;
  }

  reselect = FALSE;

  if ((entry->selection_start_pos != entry->selection_end_pos) && 
      (!entry->have_selection || 
       (selection_data->selection == clipboard_atom)))
    {
      reselect = TRUE;

      /* Don't want to call gtk_delete_selection here if we are going
       * to reclaim the selection to avoid extra server traffic */
      if (entry->have_selection)
	{
	  gtk_entry_delete_text (entry,
				 MIN (entry->selection_start_pos, entry->selection_end_pos),
				 MAX (entry->selection_start_pos, entry->selection_end_pos));
	}
      else
	gtk_delete_selection (entry);
    }

  tmp_pos = old_pos = entry->current_pos;

  switch (type)
    {
    case STRING:
      selection_data->data[selection_data->length] = 0;
      gtk_entry_insert_text (entry, selection_data->data,
			     strlen (selection_data->data), &tmp_pos);
      entry->current_pos = tmp_pos;
      break;
    case CTEXT:
      {
	gchar **list;
	gint count;
	gint i;

	count = gdk_text_property_to_text_list (selection_data->type,
						selection_data->format, 
	      					selection_data->data,
						selection_data->length,
						&list);
	for (i=0; i<count; i++) 
	  {
	    gtk_entry_insert_text (entry, list[i], strlen (list[i]), &tmp_pos);
	    entry->current_pos = tmp_pos;
	  }
	if (count > 0)
	  gdk_free_text_list (list);
      }
      break;
    case INVALID:		/* quiet compiler */
      break;
    }

  if (reselect)
    gtk_entry_select_region (entry, old_pos, entry->current_pos);

  gtk_entry_queue_draw (entry);
}

static void
gtk_entry_draw_text (GtkEntry *entry)
{
  GtkWidget *widget;
  GtkStateType selected_state;
  gint selection_start_pos;
  gint selection_end_pos;
  gint selection_start_xoffset;
  gint selection_end_xoffset;
  gint width, height;
  gint y;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (entry->timer)
    {
      gtk_timeout_remove (entry->timer);
      entry->timer = 0;
    }

  if (!entry->visible)
    {
      gtk_entry_draw_cursor (entry);
      return;
    }

  if (GTK_WIDGET_DRAWABLE (entry))
    {
      widget = GTK_WIDGET (entry);

      gdk_window_clear (entry->text_area);

      if (entry->text)
	{
	  gdk_window_get_size (entry->text_area, &width, &height);
	  y = (height - (widget->style->font->ascent + widget->style->font->descent)) / 2;
	  y += widget->style->font->ascent;

          if (entry->selection_start_pos != entry->selection_end_pos)
            {
	      selected_state = GTK_STATE_SELECTED;
	      if (!entry->have_selection)
		selected_state = GTK_STATE_ACTIVE;

              selection_start_pos = MIN (entry->selection_start_pos, entry->selection_end_pos);
              selection_end_pos = MAX (entry->selection_start_pos, entry->selection_end_pos);

              selection_start_xoffset = gdk_text_width (widget->style->font,
							entry->text,
							selection_start_pos);
              selection_end_xoffset = gdk_text_width (widget->style->font,
						      entry->text,
						      selection_end_pos);

              if (selection_start_pos > 0)
                gdk_draw_text (entry->text_area, widget->style->font,
                               widget->style->fg_gc[GTK_STATE_NORMAL],
                               -entry->scroll_offset, y,
                               entry->text, selection_start_pos);

              gdk_draw_rectangle (entry->text_area,
                                  widget->style->bg_gc[selected_state],
                                  TRUE,
                                  -entry->scroll_offset + selection_start_xoffset,
                                  0,
                                  selection_end_xoffset - selection_start_xoffset,
                                  -1);

              gdk_draw_text (entry->text_area, widget->style->font,
                             widget->style->fg_gc[selected_state],
                             -entry->scroll_offset + selection_start_xoffset, y,
                             entry->text + selection_start_pos,
                             selection_end_pos - selection_start_pos);

              if (selection_end_pos < entry->text_length)
                gdk_draw_string (entry->text_area, widget->style->font,
                                 widget->style->fg_gc[GTK_STATE_NORMAL],
                                 -entry->scroll_offset + selection_end_xoffset, y,
                                 entry->text + selection_end_pos);
	    }
          else
            {
	      GdkGCValues values;

	      gdk_gc_get_values (widget->style->fg_gc[GTK_STATE_NORMAL], &values);
              gdk_draw_string (entry->text_area, widget->style->font,
                               widget->style->fg_gc[GTK_STATE_NORMAL],
                               -entry->scroll_offset, y,
                               entry->text);
            }
	}

      if(entry->editable)
	gtk_entry_draw_cursor (entry);
    }
}

static void
gtk_entry_draw_cursor (GtkEntry *entry)
{
  GtkWidget *widget;
  GdkGC *gc;
  gint xoffset;
  gint text_area_height;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (GTK_WIDGET_DRAWABLE (entry))
    {
      widget = GTK_WIDGET (entry);

      if (entry->current_pos > 0 && entry->visible)
	xoffset = gdk_text_width (widget->style->font, entry->text, entry->current_pos);
      else
	xoffset = 0;
      xoffset -= entry->scroll_offset;

      if (GTK_WIDGET_HAS_FOCUS (widget) &&
	  (entry->selection_start_pos == entry->selection_end_pos))
	gc = widget->style->fg_gc[GTK_STATE_NORMAL];
      else
	gc = widget->style->white_gc;

      gdk_window_get_size (entry->text_area, NULL, &text_area_height);
      gdk_draw_line (entry->text_area, gc, xoffset, 0, xoffset, text_area_height);
#ifdef USE_XIM
      if (gdk_im_ready() && entry->ic && 
	  gdk_ic_get_style (entry->ic) & GdkIMPreeditPosition)
	{
	  GdkPoint spot;

	  spot.x = xoffset;
	  spot.y = (text_area_height + (widget->style->font->ascent - widget->style->font->descent) + 1) / 2;
	  gdk_ic_set_attr (entry->ic, "preeditAttributes", "spotLocation", &spot, NULL);
	}
#endif 
    }
}

static void
gtk_entry_queue_draw (GtkEntry *entry)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (!entry->timer)
    entry->timer = gtk_timeout_add (DRAW_TIMEOUT, gtk_entry_timer, entry);
}

static gint
gtk_entry_timer (gpointer data)
{
  GtkEntry *entry;

  g_return_val_if_fail (data != NULL, FALSE);

  entry = GTK_ENTRY (data);
  entry->timer = 0;
  gtk_entry_draw_text (entry);

  return FALSE;
}

static gint
gtk_entry_position (GtkEntry *entry,
		    gint      x)
{
  gint return_val;
  gint char_width;
  gint sum;
  gint i;
  gint len;

  g_return_val_if_fail (entry != NULL, 0);
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  i = 0;
  sum = 0;

  if (x > sum)
    {
      for (; i < entry->text_length; i+=len)
	{
	  len = mblen (entry->text+i, MB_CUR_MAX);
	  /* character not supported in current locale is included */
	  if (len < 1) 
	    len = 1;
	  char_width = gdk_text_width (GTK_WIDGET (entry)->style->font, 
	  			       entry->text + i, len);

	  if (x < (sum + char_width / 2))
	    break;
	  sum += char_width;
	}
    }

  return_val = i;

  return return_val;
}

void
gtk_entry_adjust_scroll (GtkEntry *entry)
{
  gint xoffset;
  gint text_area_width;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (!entry->text_area)
    return;

  gdk_window_get_size (entry->text_area, &text_area_width, NULL);

  if (entry->current_pos > 0)
    xoffset = gdk_text_width (GTK_WIDGET (entry)->style->font, entry->text, entry->current_pos);
  else
    xoffset = 0;
  xoffset -= entry->scroll_offset;

  if (xoffset < 0)
    entry->scroll_offset += xoffset;
  else if (xoffset > text_area_width)
    entry->scroll_offset += (xoffset - text_area_width) + 1;
}

static void
gtk_entry_grow_text (GtkEntry *entry)
{
  gint previous_size;
  gint i;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  previous_size = entry->text_size;
  if (!entry->text_size)
    entry->text_size = 128;
  else
    entry->text_size *= 2;
  entry->text = g_realloc (entry->text, entry->text_size);

  for (i = previous_size; i < entry->text_size; i++)
    entry->text[i] = '\0';
}

static void
gtk_entry_insert_text (GtkEntry *entry,
		       const gchar *new_text,
		       gint      new_text_length,
		       gint     *position)
{
  gchar buf[64];
  gchar *text;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (new_text_length <= 64)
    text = buf;
  else
    text = g_new (gchar, new_text_length);

  strncpy (text, new_text, new_text_length);

  gtk_signal_emit (GTK_OBJECT (entry), entry_signals[INSERT_TEXT],
		   text, new_text_length, position);
  gtk_signal_emit (GTK_OBJECT (entry), entry_signals[CHANGED]);

  if (new_text_length > 64)
    g_free (text);
}

static void
gtk_entry_delete_text (GtkEntry *entry,
		       gint      start_pos,
		       gint      end_pos)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_signal_emit (GTK_OBJECT (entry), entry_signals[DELETE_TEXT],
		   start_pos, end_pos);
  gtk_signal_emit (GTK_OBJECT (entry), entry_signals[CHANGED]);
}

static void
gtk_real_entry_insert_text (GtkEntry *entry,
			    const gchar *new_text,
			    gint      new_text_length,
			    gint     *position)
{
  gchar *text;
  gint start_pos;
  gint end_pos;
  gint last_pos;
  gint i;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  /* Make sure we do not exceed the maximum size of the entry. */
  if (entry->text_max_length != 0 &&
      new_text_length + entry->text_length > entry->text_max_length)
    new_text_length = entry->text_max_length - entry->text_length;

  /* Don't insert anything, if there was nothing to insert. */
  if (new_text_length == 0)
    return;

  start_pos = *position;
  end_pos = start_pos + new_text_length;
  last_pos = new_text_length + entry->text_length;

  if (entry->selection_start_pos >= *position)
    entry->selection_start_pos += new_text_length;
  if (entry->selection_end_pos >= *position)
    entry->selection_end_pos += new_text_length;

  while (last_pos >= entry->text_size)
    gtk_entry_grow_text (entry);

  text = entry->text;
  for (i = last_pos - 1; i >= end_pos; i--)
    text[i] = text[i- (end_pos - start_pos)];
  for (i = start_pos; i < end_pos; i++)
    text[i] = new_text[i - start_pos];

  entry->text_length += new_text_length;
  *position = end_pos;
}

static void
gtk_real_entry_delete_text (GtkEntry *entry,
			    gint      start_pos,
			    gint      end_pos)
{
  gchar *text;
  gint deletion_length;
  gint i;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (entry->selection_start_pos > start_pos)
    entry->selection_start_pos -= MIN(end_pos, entry->selection_start_pos) - start_pos;
  if (entry->selection_end_pos > start_pos)
    entry->selection_end_pos -= MIN(end_pos, entry->selection_end_pos) - start_pos;

  if ((start_pos < end_pos) &&
      (start_pos >= 0) &&
      (end_pos <= entry->text_length))
    {
      text = entry->text;
      deletion_length = end_pos - start_pos;

      for (i = end_pos; i < entry->text_length; i++)
        text[i - deletion_length] = text[i];

      for (i = entry->text_length - deletion_length; i < entry->text_length; i++)
        text[i] = '\0';

      entry->text_length -= deletion_length;
      entry->current_pos = start_pos;
    }
}


static void
gtk_move_forward_character (GtkEntry *entry)
{
  gint len;

  if (entry->current_pos < entry->text_length)
    {
      len = mblen (entry->text+entry->current_pos, MB_CUR_MAX);
      entry->current_pos += (len>0)? len:1;
    }
  if (entry->current_pos > entry->text_length)
    entry->current_pos = entry->text_length;
}

static gint
move_backward_character (gchar *str, gint index)
{
  gint i;
  gint len;

  if (index <= 0)
    return -1;
  for (i=0,len=0; i<index; i+=len)
    {
      len = mblen (str+i, MB_CUR_MAX);
      if (len<1)
	return i;
    }
  return i-len;
}

static void
gtk_move_backward_character (GtkEntry *entry)
{
  /* this routine is correct only if string is state-independent-encoded */

  if (0 < entry->current_pos)
    {
      entry->current_pos = move_backward_character (entry->text,
      						    entry->current_pos);
      if (entry->current_pos < 0)
	entry->current_pos = 0;
    }
}

static void
gtk_move_forward_word (GtkEntry *entry)
{
  gchar *text;
  gint i;
  wchar_t c;
  gint len;

  if (entry->text)
    {
      text = entry->text;
      i = entry->current_pos;

      len = mbtowc (&c, text+i, MB_CUR_MAX);
      if (!iswalnum(c))
	for (; i < entry->text_length; i+=len)
	  {
	    len = mbtowc (&c, text+i, MB_CUR_MAX);
	    if (len < 1 || iswalnum(c))
	      break;
	  }
  
      for (; i < entry->text_length; i+=len)
	{
	  len = mbtowc (&c, text+i, MB_CUR_MAX);
	  if (len < 1 || !iswalnum(c))
	    break;
	}

      entry->current_pos = i;
      if (entry->current_pos > entry->text_length)
	entry->current_pos = entry->text_length;
    }
}

static void
gtk_move_backward_word (GtkEntry *entry)
{
  gchar *text;
  gint i;
  wchar_t c;
  gint len;

  if (entry->text)
    {
      text = entry->text;
      i=move_backward_character(text, entry->current_pos);
      if (i < 0) /* Per */
	{
	  entry->selection_start_pos = 0;
	  entry->selection_end_pos = 0;
	  return;
	}

      len = mbtowc (&c, text+i, MB_CUR_MAX);
      if (!iswalnum(c))
        for (; i >= 0; i=move_backward_character(text, i))
	  {
	    len = mbtowc (&c, text+i, MB_CUR_MAX);
	    if (iswalnum(c))
	      break;
	  }

      for (; i >= 0; i=move_backward_character(text, i))
	{
	  len = mbtowc (&c, text+i, MB_CUR_MAX);
	  if (!iswalnum(c))
	    {
	      i += len;
	      break;
	    }
	}

      if (i < 0)
        i = 0;

      entry->current_pos = i;
    }
}

static void
gtk_move_beginning_of_line (GtkEntry *entry)
{
  entry->current_pos = 0;
}

static void
gtk_move_end_of_line (GtkEntry *entry)
{
  entry->current_pos = entry->text_length;
}

static void
gtk_delete_forward_character (GtkEntry *entry)
{
  gint old_pos;

  old_pos = entry->current_pos;
  gtk_move_forward_character (entry);
  gtk_entry_delete_text (entry, old_pos, entry->current_pos);
}

static void
gtk_delete_backward_character (GtkEntry *entry)
{
  gint old_pos;

  old_pos = entry->current_pos;
  gtk_move_backward_character (entry);
  gtk_entry_delete_text (entry, entry->current_pos, old_pos);
}

static void
gtk_delete_forward_word (GtkEntry *entry)
{
  gint old_pos;

  old_pos = entry->current_pos;
  gtk_move_forward_word (entry);
  gtk_entry_delete_text (entry, old_pos, entry->current_pos);
}

static void
gtk_delete_backward_word (GtkEntry *entry)
{
  gint old_pos;

  old_pos = entry->current_pos;
  gtk_move_backward_word (entry);
  gtk_entry_delete_text (entry, entry->current_pos, old_pos);
}

static void
gtk_delete_line (GtkEntry *entry)
{
  gtk_entry_delete_text (entry, 0, entry->text_length);
}

static void
gtk_delete_to_line_end (GtkEntry *entry)
{
  gtk_entry_delete_text (entry, entry->current_pos, entry->text_length);
}

static void
gtk_delete_selection (GtkEntry *entry)
{
  if (entry->selection_start_pos != entry->selection_end_pos)
    gtk_entry_delete_text (entry,
			   MIN (entry->selection_start_pos, entry->selection_end_pos),
			   MAX (entry->selection_start_pos, entry->selection_end_pos));

  entry->selection_start_pos = 0;
  entry->selection_end_pos = 0;

  if (entry->have_selection)
    {
      entry->have_selection = FALSE;
      if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == GTK_WIDGET (entry)->window)
	gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, GDK_CURRENT_TIME);
    }
}

static void
gtk_select_word (GtkEntry *entry)
{
  gint start_pos;
  gint end_pos;

  gtk_move_backward_word (entry);
  start_pos = entry->current_pos;
  end_pos = entry->current_pos;

  gtk_move_forward_word (entry);
  end_pos = entry->current_pos;

  gtk_entry_select_region (entry, start_pos, end_pos);
}

static void
gtk_select_line (GtkEntry *entry)
{
  gtk_entry_select_region (entry, 0, entry->text_length);
  entry->current_pos = entry->selection_end_pos;
}

void
gtk_entry_select_region (GtkEntry *entry,
		   gint      start,
		   gint      end)
{
  entry->have_selection = TRUE;

  entry->selection_start_pos = start;
  entry->selection_end_pos = end;

  gtk_widget_queue_draw (GTK_WIDGET (entry));
}

static void
gtk_entry_cut_clipboard (GtkEntry *entry, GdkEventKey *event)
{
  gtk_entry_copy_clipboard (entry, event);
  gtk_delete_selection (entry);

  gtk_entry_queue_draw (entry);
}

static void
gtk_entry_copy_clipboard (GtkEntry *entry, GdkEventKey *event)
{
  gint selection_start_pos; 
  gint selection_end_pos;

  selection_start_pos = MIN (entry->selection_start_pos, entry->selection_end_pos);
  selection_end_pos = MAX (entry->selection_start_pos, entry->selection_end_pos);
 
  if (selection_start_pos != selection_end_pos)
    {
      if (gtk_selection_owner_set (GTK_WIDGET (entry),
				   clipboard_atom,
				   event->time))
	{
	  char c;

	  c = entry->text[selection_end_pos];
	  entry->text[selection_end_pos] = 0;
	  entry->clipboard_text = g_strdup (entry->text + selection_start_pos);
	  entry->text[selection_end_pos] = c;
	}
    }
}

static void
gtk_entry_paste_clipboard (GtkEntry *entry, GdkEventKey *event)
{
  gtk_selection_convert (GTK_WIDGET(entry), 
			 clipboard_atom, ctext_atom, event->time);
}
