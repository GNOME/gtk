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

#include <ctype.h>
#include <string.h>
#include "gdk/gdkkeysyms.h"
#include "gdk/gdki18n.h"
#include "gtkentry.h"
#include "gtkimmulticontext.h"
#include "gtkmain.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtkstyle.h"

#include <pango/pango.h>
#include <glib-object.h>

#define MIN_ENTRY_WIDTH  150
#define DRAW_TIMEOUT     20
#define INNER_BORDER     2

/* Initial size of buffer, in bytes */
#define MIN_SIZE 16

/* Maximum size of text buffer, in bytes */
#define MAX_SIZE G_MAXUSHORT

enum {
  ARG_0,
  ARG_MAX_LENGTH,
  ARG_VISIBILITY
};


static void   gtk_entry_class_init           (GtkEntryClass    *klass);
static void   gtk_entry_init                 (GtkEntry         *entry);
static void   gtk_entry_set_arg              (GtkObject        *object,
					      GtkArg           *arg,
					      guint             arg_id);
static void   gtk_entry_get_arg              (GtkObject        *object,
					      GtkArg           *arg,
					      guint             arg_id);
static void   gtk_entry_finalize             (GObject          *object);
static void   gtk_entry_realize              (GtkWidget        *widget);
static void   gtk_entry_unrealize            (GtkWidget        *widget);
static void   gtk_entry_draw_focus           (GtkWidget        *widget);
static void   gtk_entry_size_request         (GtkWidget        *widget,
					      GtkRequisition   *requisition);
static void   gtk_entry_size_allocate        (GtkWidget        *widget,
					      GtkAllocation    *allocation);
static void   gtk_entry_draw                 (GtkWidget        *widget,
					      GdkRectangle     *area);
static gint   gtk_entry_expose               (GtkWidget        *widget,
					      GdkEventExpose   *event);
static gint   gtk_entry_button_press         (GtkWidget        *widget,
					      GdkEventButton   *event);
static gint   gtk_entry_button_release       (GtkWidget        *widget,
					      GdkEventButton   *event);
static gint   gtk_entry_motion_notify        (GtkWidget        *widget,
					      GdkEventMotion   *event);
static gint   gtk_entry_key_press            (GtkWidget        *widget,
					      GdkEventKey      *event);
static gint   gtk_entry_focus_in             (GtkWidget        *widget,
					      GdkEventFocus    *event);
static gint   gtk_entry_focus_out            (GtkWidget        *widget,
					      GdkEventFocus    *event);
static void   gtk_entry_draw_text            (GtkEntry         *entry);
static void   gtk_entry_ensure_layout        (GtkEntry         *entry);
static void   gtk_entry_draw_cursor          (GtkEntry         *entry);
static void   gtk_entry_style_set            (GtkWidget        *widget,
					      GtkStyle         *previous_style);
static void   gtk_entry_direction_changed    (GtkWidget        *widget,
					      GtkTextDirection  previous_dir);
static void   gtk_entry_state_changed        (GtkWidget        *widget,
					      GtkStateType      previous_state);
static void   gtk_entry_queue_draw           (GtkEntry         *entry);
static gint   gtk_entry_find_position        (GtkEntry         *entry,
					      gint              x);
static void   gtk_entry_get_cursor_locations (GtkEntry         *entry,
					      gint             *strong_x,
					      gint             *weak_x);
static void   entry_adjust_scroll            (GtkEntry         *entry);
static void   gtk_entry_insert_text          (GtkEditable      *editable,
					      const gchar      *new_text,
					      gint              new_text_length,
					      gint             *position);
static void   gtk_entry_delete_text          (GtkEditable      *editable,
					      gint              start_pos,
					      gint              end_pos);
static void   gtk_entry_update_text          (GtkEditable      *editable,
					      gint              start_pos,
					      gint              end_pos);
static gchar *gtk_entry_get_chars            (GtkEditable      *editable,
					      gint              start_pos,
					      gint              end_pos);

/* Binding actions */
static void gtk_entry_move_cursor         (GtkEditable *editable,
					   gint         x,
					   gint         y);
static void gtk_entry_move_word           (GtkEditable *editable,
					   gint         n);
static void gtk_entry_move_to_column      (GtkEditable *editable,
					   gint         row);
static void gtk_entry_kill_char           (GtkEditable *editable,
					   gint         direction);
static void gtk_entry_kill_word           (GtkEditable *editable,
					   gint         direction);
static void gtk_entry_kill_line           (GtkEditable *editable,
					   gint         direction);

/* To be removed */
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
static void gtk_select_word               (GtkEntry          *entry,
					   guint32            time);
static void gtk_select_line               (GtkEntry          *entry,
					   guint32            time);


static void gtk_entry_set_selection       (GtkEditable       *editable,
					   gint               start,
					   gint               end);

static void gtk_entry_set_position_from_editable (GtkEditable *editable,
						  gint         position);

static void gtk_entry_commit_cb           (GtkIMContext      *context,
					   const gchar       *str,
					   GtkEntry          *entry);


static GtkWidgetClass *parent_class = NULL;
static GdkAtom ctext_atom = GDK_NONE;

static const GtkTextFunction control_keys[26] =
{
  (GtkTextFunction)gtk_move_beginning_of_line,    /* a */
  (GtkTextFunction)gtk_move_backward_character,   /* b */
  (GtkTextFunction)gtk_editable_copy_clipboard,   /* c */
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
  (GtkTextFunction)gtk_editable_paste_clipboard,  /* v */
  (GtkTextFunction)gtk_delete_backward_word,      /* w */
  (GtkTextFunction)gtk_editable_cut_clipboard,    /* x */
  NULL,                                           /* y */
  NULL,                                           /* z */
};

static const GtkTextFunction alt_keys[26] =
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


GtkType
gtk_entry_get_type (void)
{
  static GtkType entry_type = 0;

  if (!entry_type)
    {
      static const GtkTypeInfo entry_info =
      {
	"GtkEntry",
	sizeof (GtkEntry),
	sizeof (GtkEntryClass),
	(GtkClassInitFunc) gtk_entry_class_init,
	(GtkObjectInitFunc) gtk_entry_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      entry_type = gtk_type_unique (GTK_TYPE_EDITABLE, &entry_info);
    }

  return entry_type;
}

static void
gtk_entry_class_init (GtkEntryClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkEditableClass *editable_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  editable_class = (GtkEditableClass*) class;
  parent_class = gtk_type_class (GTK_TYPE_EDITABLE);

  gobject_class->finalize = gtk_entry_finalize;

  gtk_object_add_arg_type ("GtkEntry::max_length", GTK_TYPE_UINT, GTK_ARG_READWRITE, ARG_MAX_LENGTH);
  gtk_object_add_arg_type ("GtkEntry::visibility", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_VISIBILITY);

  object_class->set_arg = gtk_entry_set_arg;
  object_class->get_arg = gtk_entry_get_arg;

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
  widget_class->style_set = gtk_entry_style_set;
  widget_class->direction_changed = gtk_entry_direction_changed;
  widget_class->state_changed = gtk_entry_state_changed;

  editable_class->insert_text = gtk_entry_insert_text;
  editable_class->delete_text = gtk_entry_delete_text;
  editable_class->changed = (void (*)(GtkEditable *)) entry_adjust_scroll;

  editable_class->move_cursor = gtk_entry_move_cursor;
  editable_class->move_word = gtk_entry_move_word;
  editable_class->move_to_column = gtk_entry_move_to_column;

  editable_class->kill_char = gtk_entry_kill_char;
  editable_class->kill_word = gtk_entry_kill_word;
  editable_class->kill_line = gtk_entry_kill_line;

  editable_class->update_text = gtk_entry_update_text;
  editable_class->get_chars   = gtk_entry_get_chars;
  editable_class->set_selection = gtk_entry_set_selection;
  editable_class->set_position = gtk_entry_set_position_from_editable;
}

static void
gtk_entry_set_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  GtkEntry *entry;

  entry = GTK_ENTRY (object);

  switch (arg_id)
    {
    case ARG_MAX_LENGTH:
      gtk_entry_set_max_length (entry, GTK_VALUE_UINT (*arg));
      break;
    case ARG_VISIBILITY:
      gtk_entry_set_visibility (entry, GTK_VALUE_BOOL (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_entry_get_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  GtkEntry *entry;

  entry = GTK_ENTRY (object);

  switch (arg_id)
    {
    case ARG_MAX_LENGTH:
      GTK_VALUE_UINT (*arg) = entry->text_max_length;
      break;
    case ARG_VISIBILITY:
      GTK_VALUE_BOOL (*arg) = GTK_EDITABLE (entry)->visible;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_entry_init (GtkEntry *entry)
{
  GTK_WIDGET_SET_FLAGS (entry, GTK_CAN_FOCUS);

  entry->text_area = NULL;
  
  entry->text_size = MIN_SIZE;
  entry->text = g_malloc (entry->text_size);
  entry->text[0] = '\0';
  
  entry->text_length = 0;
  entry->text_max_length = 0;
  entry->n_bytes = 0;
  entry->scroll_offset = 0;
  entry->timer = 0;
  entry->button = 0;
  entry->ascent = 0;
  entry->descent = 0;

  /* This object is completely private. No external entity can gain a reference
   * to it; so we create it here and destroy it in finalize().
   */
  entry->im_context = gtk_im_multicontext_new ();
  
  gtk_signal_connect (GTK_OBJECT (entry->im_context), "commit",
		      GTK_SIGNAL_FUNC (gtk_entry_commit_cb), entry);
}

GtkWidget*
gtk_entry_new (void)
{
  return GTK_WIDGET (gtk_type_new (GTK_TYPE_ENTRY));
}

GtkWidget*
gtk_entry_new_with_max_length (guint16 max)
{
  GtkEntry *entry;

  entry = gtk_type_new (GTK_TYPE_ENTRY);
  entry->text_max_length = max;

  return GTK_WIDGET (entry);
}

void
gtk_entry_set_text (GtkEntry *entry,
		    const gchar *text)
{
  gint tmp_pos;

  GtkEditable *editable;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (text != NULL);

  editable = GTK_EDITABLE (entry);
  
  gtk_entry_delete_text (GTK_EDITABLE(entry), 0, entry->text_length);

  tmp_pos = 0;
  gtk_editable_insert_text (editable, text, strlen (text), &tmp_pos);
  editable->current_pos = tmp_pos;
}

void
gtk_entry_append_text (GtkEntry *entry,
		       const gchar *text)
{
  gint tmp_pos;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (text != NULL);

  tmp_pos = entry->text_length;
  gtk_editable_insert_text (GTK_EDITABLE(entry), text, strlen (text), &tmp_pos);
}

void
gtk_entry_prepend_text (GtkEntry *entry,
			const gchar *text)
{
  gint tmp_pos;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (text != NULL);

  tmp_pos = 0;
  gtk_editable_insert_text (GTK_EDITABLE(entry), text, strlen (text), &tmp_pos);
}

void
gtk_entry_set_position (GtkEntry *entry,
			gint      position)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if ((position == -1) || (position > entry->text_length))
    GTK_EDITABLE(entry)->current_pos = entry->text_length;
  else
    GTK_EDITABLE(entry)->current_pos = position;
  entry_adjust_scroll (entry);
}

static void
gtk_entry_set_position_from_editable (GtkEditable *editable,
				      gint position)
{
  gtk_entry_set_position (GTK_ENTRY (editable), position);
}

void
gtk_entry_set_visibility (GtkEntry *entry,
			  gboolean visible)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  GTK_EDITABLE (entry)->visible = visible ? TRUE : FALSE;
  
  gtk_entry_queue_draw (entry);
}

void
gtk_entry_set_editable(GtkEntry *entry,
		       gboolean  editable)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_editable_set_editable (GTK_EDITABLE (entry), editable);
}

gchar*
gtk_entry_get_text (GtkEntry *entry)
{
  g_return_val_if_fail (entry != NULL, NULL);
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return entry->text;
}

static void
gtk_entry_finalize (GObject *object)
{
  GtkEntry *entry;

  g_return_if_fail (GTK_IS_ENTRY (object));

  entry = GTK_ENTRY (object);

  if (entry->layout)
    g_object_unref (G_OBJECT (entry->layout));

  gtk_object_unref (GTK_OBJECT (entry->im_context));

  if (entry->timer)
    gtk_timeout_remove (entry->timer);

  entry->text_size = 0;

  if (entry->text)
    g_free (entry->text);
  entry->text = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_entry_realize (GtkWidget *widget)
{
  GtkEntry *entry;
  GtkEditable *editable;
  GtkRequisition requisition;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);

  gtk_widget_get_child_requisition (widget, &requisition);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y + (widget->allocation.height -
					 requisition.height) / 2;
  attributes.width = widget->allocation.width;
  attributes.height = requisition.height;
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

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, entry);

  attributes.x = widget->style->xthickness;
  attributes.y = widget->style->ythickness;
  attributes.width = widget->allocation.width - attributes.x * 2;
  attributes.height = requisition.height - attributes.y * 2;
  attributes.cursor = entry->cursor = gdk_cursor_new (GDK_XTERM);
  attributes_mask |= GDK_WA_CURSOR;

  entry->text_area = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (entry->text_area, entry);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_background (widget->window, &widget->style->base[GTK_WIDGET_STATE (widget)]);
  gdk_window_set_background (entry->text_area, &widget->style->base[GTK_WIDGET_STATE (widget)]);

  gdk_window_show (entry->text_area);

  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_claim_selection (editable, TRUE, GDK_CURRENT_TIME);

  gtk_im_context_set_client_window (entry->im_context, entry->text_area);
}

static void
gtk_entry_unrealize (GtkWidget *widget)
{
  GtkEntry *entry;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));

  entry = GTK_ENTRY (widget);

  gtk_im_context_set_client_window (entry->im_context, entry->text_area);
  
  if (entry->text_area)
    {
      gdk_window_set_user_data (entry->text_area, NULL);
      gdk_window_destroy (entry->text_area);
      entry->text_area = NULL;
      gdk_cursor_destroy (entry->cursor);
      entry->cursor = NULL;
    }

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
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

      gtk_paint_shadow (widget->style, widget->window,
			GTK_STATE_NORMAL, GTK_SHADOW_IN,
			NULL, widget, "entry",
			x, y, width, height);

      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	   gdk_window_get_size (widget->window, &width, &height);
	   gtk_paint_focus (widget->style, widget->window, 
			    NULL, widget, "entry",
			    0, 0, width - 1, height - 1);
	}
    }
}

static void
gtk_entry_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkEntry *entry;
  PangoFontMetrics metrics;
  PangoFont *font;
  gchar *lang;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));
  g_return_if_fail (requisition != NULL);

  entry = GTK_ENTRY (widget);
  
  gtk_entry_ensure_layout (entry);

  /* hackish for now, get metrics
   */
  font = pango_context_load_font (pango_layout_get_context (entry->layout),
				  widget->style->font_desc);
  lang = pango_context_get_lang (pango_layout_get_context (entry->layout));
  pango_font_get_metrics (font, lang, &metrics);
  g_free (lang);
  
  g_object_unref (G_OBJECT (font));

  entry->ascent = metrics.ascent;
  entry->descent = metrics.descent;
  
  requisition->width = MIN_ENTRY_WIDTH + (widget->style->xthickness + INNER_BORDER) * 2;
  requisition->height = ((metrics.ascent + metrics.descent) / PANGO_SCALE + 
			 (widget->style->ythickness + INNER_BORDER) * 2);
}

static void
gtk_entry_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkEntry *entry;
  GtkEditable *editable;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      /* We call gtk_widget_get_child_requisition, since we want (for
       * backwards compatibility reasons) the realization here to
       * be affected by the usize of the entry, if set
       */
      GtkRequisition requisition;
      gtk_widget_get_child_requisition (widget, &requisition);
  
      gdk_window_move_resize (widget->window,
			      allocation->x,
			      allocation->y + (allocation->height - requisition.height) / 2,
			      allocation->width, requisition.height);
      gdk_window_move_resize (entry->text_area,
			      widget->style->xthickness,
			      widget->style->ythickness,
			      allocation->width - widget->style->xthickness * 2,
			      requisition.height - widget->style->ythickness * 2);

    }

  /* And make sure the cursor is on screen */
  entry_adjust_scroll (entry);
}

static void
gtk_entry_draw (GtkWidget    *widget,
		GdkRectangle *area)
{
  GtkEntry *entry;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));
  g_return_if_fail (area != NULL);

  entry = GTK_ENTRY (widget);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      GdkRectangle tmp_area = *area;

      tmp_area.x -= widget->style->xthickness;
      tmp_area.y -= widget->style->xthickness;
      
      gdk_window_begin_paint_rect (entry->text_area, &tmp_area);
      gtk_widget_draw_focus (widget);
      gtk_entry_draw_text (GTK_ENTRY (widget));
      gtk_entry_draw_cursor (GTK_ENTRY (widget));
      gdk_window_end_paint (entry->text_area);
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
    {
      gtk_entry_draw_text (GTK_ENTRY (widget));
      gtk_entry_draw_cursor (GTK_ENTRY (widget));
    }

  return FALSE;
}

static gint
gtk_entry_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkEntry *entry;
  GtkEditable *editable;
  gint tmp_pos;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (ctext_atom == GDK_NONE)
    ctext_atom = gdk_atom_intern ("COMPOUND_TEXT", FALSE);

  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);
  
  if (entry->button && (event->button != entry->button))
    return FALSE;

  entry->button = event->button;
  
  if (!GTK_WIDGET_HAS_FOCUS (widget))
    gtk_widget_grab_focus (widget);

  if (event->button == 1)
    {
      switch (event->type)
	{
	case GDK_BUTTON_PRESS:
	  gtk_grab_add (widget);

	  tmp_pos = gtk_entry_find_position (entry, event->x + entry->scroll_offset);
	  /* Set it now, so we display things right. We'll unset it
	   * later if things don't work out */
	  editable->has_selection = TRUE;
	  gtk_entry_set_selection (editable, tmp_pos, tmp_pos);
	  editable->current_pos = editable->selection_start_pos;
	  break;

	case GDK_2BUTTON_PRESS:
	  gtk_select_word (entry, event->time);
	  break;

	case GDK_3BUTTON_PRESS:
	  gtk_select_line (entry, event->time);
	  break;

	default:
	  break;
	}

      return TRUE;
    }
  else if (event->type == GDK_BUTTON_PRESS)
    {
      if ((event->button == 2) && editable->editable)
	{
	  if (editable->selection_start_pos == editable->selection_end_pos ||
	      editable->has_selection)
	    editable->current_pos = gtk_entry_find_position (entry, event->x + entry->scroll_offset);
	  gtk_selection_convert (widget, GDK_SELECTION_PRIMARY,
				 ctext_atom, event->time);
	}
      else
	{
	  gtk_grab_add (widget);

	  tmp_pos = gtk_entry_find_position (entry, event->x + entry->scroll_offset);
	  gtk_entry_set_selection (editable, tmp_pos, tmp_pos);
	  editable->has_selection = FALSE;
	  editable->current_pos = editable->selection_start_pos;

	  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, event->time);
	}

      return TRUE;
    }

  return FALSE;
}

static gint
gtk_entry_button_release (GtkWidget      *widget,
			  GdkEventButton *event)
{
  GtkEntry *entry;
  GtkEditable *editable;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);

  if (entry->button != event->button)
    return FALSE;

  entry->button = 0;
  
  if (event->button == 1)
    {
      gtk_grab_remove (widget);

      editable->has_selection = FALSE;
      if (editable->selection_start_pos != editable->selection_end_pos)
	{
	  if (gtk_selection_owner_set (widget,
				       GDK_SELECTION_PRIMARY,
				       event->time))
	    editable->has_selection = TRUE;
	  else
	    gtk_entry_queue_draw (entry);
	}
      else
	{
	  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, event->time);
	}

      return TRUE;
    }
  else if (event->button == 3)
    {
      gtk_grab_remove (widget);

      return TRUE;
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

  if (entry->button == 0)
    return FALSE;

  x = event->x;
  if (event->is_hint || (entry->text_area != event->window))
    gdk_window_get_pointer (entry->text_area, &x, NULL, NULL);

  GTK_EDITABLE(entry)->selection_end_pos = gtk_entry_find_position (entry, x + entry->scroll_offset);
  GTK_EDITABLE(entry)->current_pos = GTK_EDITABLE(entry)->selection_end_pos;
  entry_adjust_scroll (entry);
  gtk_entry_queue_draw (entry);

  return TRUE;
}

static gint
gtk_entry_key_press (GtkWidget   *widget,
		     GdkEventKey *event)
{
  GtkEntry *entry;
  GtkEditable *editable;

  gint return_val;
  gint key;
  guint initial_pos;
  gint extend_selection;
  gint extend_start;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);
  return_val = FALSE;

  if(editable->editable == FALSE)
    return FALSE;

  initial_pos = editable->current_pos;

  extend_selection = event->state & GDK_SHIFT_MASK;
  extend_start = FALSE;

  if (extend_selection)
    {
      if (editable->selection_start_pos == editable->selection_end_pos)
	{
	  editable->selection_start_pos = editable->current_pos;
	  editable->selection_end_pos = editable->current_pos;
	}
      
      extend_start = (editable->current_pos == editable->selection_start_pos);
    }

  switch (event->keyval)
    {
    case GDK_BackSpace:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
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
	  gtk_editable_paste_clipboard (editable);
	}
      else if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_editable_copy_clipboard (editable);
	}
      else
	{
	  /* gtk_toggle_insert(entry) -- IMPLEMENT */
	}
      break;
    case GDK_Delete:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	gtk_delete_forward_word (entry);
      else if (event->state & GDK_SHIFT_MASK)
	{
	  extend_selection = FALSE;
	  gtk_editable_cut_clipboard (editable);
	}
      else
	gtk_delete_forward_character (entry);
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
      if (event->state & GDK_CONTROL_MASK)
	gtk_move_backward_word (entry);
      else
	gtk_move_backward_character (entry);
      break;
    case GDK_Right:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	gtk_move_forward_word (entry);
      else
	gtk_move_forward_character (entry);
      break;
    case GDK_Return:
      return_val = TRUE;
      gtk_widget_activate (widget);
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
		  (* control_keys[key - 'a']) (editable, event->time);
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
		  (* alt_keys[key - 'a']) (editable, event->time);
		  return_val = TRUE;
		}
	      break;
	    }
	}
      gtk_im_context_filter_keypress (entry->im_context, event);
      
      break;
    }

  /* since we emit signals from within the above code,
   * the widget might already be destroyed or at least
   * unrealized.
   */
  if (GTK_WIDGET_REALIZED (editable) &&
      return_val && (editable->current_pos != initial_pos))
    {
      if (extend_selection)
	{
	  if (editable->current_pos < editable->selection_start_pos)
	    editable->selection_start_pos = editable->current_pos;
	  else if (editable->current_pos > editable->selection_end_pos)
	    editable->selection_end_pos = editable->current_pos;
	  else
	    {
	      if (extend_start)
		editable->selection_start_pos = editable->current_pos;
	      else
		editable->selection_end_pos = editable->current_pos;
	    }
	}
      else
	{
	  editable->selection_start_pos = 0;
	  editable->selection_end_pos = 0;
	}

      gtk_editable_claim_selection (editable,
				    editable->selection_start_pos != editable->selection_end_pos,
				    event->time);
      
      entry_adjust_scroll (entry);
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
  gtk_entry_queue_draw (GTK_ENTRY (widget));
  
  gtk_im_context_focus_in (GTK_ENTRY (widget)->im_context);

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
  gtk_entry_queue_draw (GTK_ENTRY (widget));

  gtk_im_context_focus_out (GTK_ENTRY (widget)->im_context);

  return FALSE;
}

static void
gtk_entry_ensure_layout (GtkEntry *entry)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  
  if (!entry->layout)
    {
      entry->layout = gtk_widget_create_pango_layout (widget, NULL);
      pango_layout_set_text (entry->layout, entry->text, entry->n_bytes);
    }
}

static void
gtk_entry_draw_text (GtkEntry *entry)
{
  GtkWidget *widget;
  PangoLayoutLine *line;
  GtkEditable *editable = GTK_EDITABLE (entry);
  
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (GTK_WIDGET_DRAWABLE (entry))
    {
      PangoRectangle logical_rect;
      gint area_width, area_height;
      gint y_pos;

      gdk_window_get_size (entry->text_area, &area_width, &area_height);
      area_height = PANGO_SCALE * (area_height - 2 * INNER_BORDER);
      
      widget = GTK_WIDGET (entry);

      gtk_paint_flat_box (widget->style, entry->text_area, 
			  GTK_WIDGET_STATE(widget), GTK_SHADOW_NONE,
			  NULL, widget, "entry_bg", 
			  0, 0, area_width, area_height);
      
      gtk_entry_ensure_layout (entry);
      
      line = pango_layout_get_lines (entry->layout)->data;
      pango_layout_line_get_extents (line, NULL, &logical_rect);

      /* Align primarily for locale's ascent/descent */
      y_pos = ((area_height - entry->ascent - entry->descent) / 2 + 
	       entry->ascent + logical_rect.y);

      /* Now see if we need to adjust to fit in actual drawn string */
      if (logical_rect.height > area_height)
	y_pos = (area_height - logical_rect.height) / 2;
      else if (y_pos < 0)
	y_pos = 0;
      else if (y_pos + logical_rect.height > area_height)
	y_pos = area_height - logical_rect.height;
      
      y_pos = INNER_BORDER + y_pos / PANGO_SCALE;

      gdk_draw_layout (entry->text_area, widget->style->text_gc [widget->state], 
		       INNER_BORDER - entry->scroll_offset, y_pos,
		       entry->layout);

      if (editable->selection_start_pos != editable->selection_end_pos)
	{
	  gint *ranges;
	  gint n_ranges, i;
	  gint start_index = g_utf8_offset_to_pointer (entry->text,
						       MIN (editable->selection_start_pos, editable->selection_end_pos)) - entry->text;
	  gint end_index = g_utf8_offset_to_pointer (entry->text,
						     MAX (editable->selection_start_pos, editable->selection_end_pos)) - entry->text;
	  GtkStateType selected_state = editable->has_selection ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE;
	  GdkRegion *clip_region = gdk_region_new ();

	  pango_layout_line_get_x_ranges (line, start_index, end_index, &ranges, &n_ranges);

	  for (i=0; i < n_ranges; i++)
	    {
	      GdkRectangle rect;

	      rect.x = INNER_BORDER - entry->scroll_offset + ranges[2*i] / PANGO_SCALE;
	      rect.y = y_pos;
	      rect.width = (ranges[2*i + 1] - ranges[2*i]) / PANGO_SCALE;
	      rect.height = logical_rect.height / PANGO_SCALE;
	      
	      gdk_draw_rectangle (entry->text_area, widget->style->bg_gc [selected_state], TRUE,
				  rect.x, rect.y, rect.width, rect.height);

	      gdk_region_union_with_rect (clip_region, &rect);
	    }

	  gdk_gc_set_clip_region (widget->style->fg_gc [selected_state], clip_region);
	  gdk_draw_layout (entry->text_area, widget->style->fg_gc [selected_state], 
			   INNER_BORDER - entry->scroll_offset, y_pos,
			   entry->layout);
	  gdk_gc_set_clip_region (widget->style->fg_gc [selected_state], NULL);
	  
	  gdk_region_destroy (clip_region);
	  g_free (ranges);
	}
    }
}

static void
gtk_entry_draw_cursor (GtkEntry *entry)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (GTK_WIDGET_DRAWABLE (entry))
    {
      GtkWidget *widget = GTK_WIDGET (entry);
      GtkEditable *editable = GTK_EDITABLE (entry);

      if (GTK_WIDGET_HAS_FOCUS (widget) &&
	  (editable->selection_start_pos == editable->selection_end_pos))
	{
	  gint xoffset = INNER_BORDER - entry->scroll_offset;
	  gint strong_x, weak_x;
	  gint text_area_height;

	  gdk_window_get_size (entry->text_area, NULL, &text_area_height);

	  gtk_entry_get_cursor_locations (entry, &strong_x, &weak_x);

	  gdk_draw_line (entry->text_area, widget->style->bg_gc[GTK_STATE_SELECTED], 
			 xoffset + strong_x, INNER_BORDER,
			 xoffset + strong_x, text_area_height - INNER_BORDER);

	  if (weak_x != strong_x)
	    gdk_draw_line (entry->text_area, widget->style->fg_gc[GTK_STATE_NORMAL], 
			   xoffset + weak_x, INNER_BORDER,
			   xoffset + weak_x, text_area_height - INNER_BORDER);

	}
    }
}

static void
gtk_entry_queue_draw (GtkEntry *entry)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (GTK_WIDGET_REALIZED (entry))
    {
      GdkRectangle rect = { 0 };

      gdk_window_get_size (entry->text_area, &rect.width, &rect.height);
      gdk_window_invalidate_rect (entry->text_area, &rect, 0);
    }
}

#if 0
static gint
gtk_entry_timer (gpointer data)
{
  GtkEntry *entry;

  GDK_THREADS_ENTER ();

  entry = GTK_ENTRY (data);
  entry->timer = 0;

  GDK_THREADS_LEAVE ();

  return FALSE;
}
#endif

static gint
gtk_entry_find_position (GtkEntry *entry,
			 gint      x)
{
  PangoLayoutLine *line;
  gint index;
  gint pos;
  gboolean trailing;
  
  gtk_entry_ensure_layout (entry);

  line = pango_layout_get_lines (entry->layout)->data;
  pango_layout_line_x_to_index (line, x * PANGO_SCALE, &index, &trailing);

  pos = g_utf8_pointer_to_offset (entry->text, entry->text + index);
  
  if (trailing)
    pos += 1;

  return pos;
}

static void
gtk_entry_get_cursor_locations (GtkEntry *entry,
				gint     *strong_x,
				gint     *weak_x)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  int index;

  PangoRectangle strong_pos, weak_pos;

  gtk_entry_ensure_layout (entry);
  
  index = g_utf8_offset_to_pointer (entry->text, editable->current_pos) - entry->text;
  pango_layout_get_cursor_pos (entry->layout, index, &strong_pos, &weak_pos);

  if (strong_x)
    *strong_x = strong_pos.x / PANGO_SCALE;

  if (weak_x)
    *weak_x = weak_pos.x / PANGO_SCALE;
}

static void
entry_adjust_scroll (GtkEntry *entry)
{
  GtkWidget *widget;
  gint min_offset, max_offset;
  gint text_area_width;
  gint strong_x, weak_x;
  gint strong_xoffset, weak_xoffset;
  PangoLayoutLine *line;
  PangoRectangle logical_rect;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  widget = GTK_WIDGET (entry);
  text_area_width = widget->allocation.width - 2 * (widget->style->xthickness + INNER_BORDER);

  if (!entry->layout)
    return;
  
  line = pango_layout_get_lines (entry->layout)->data;
  
  /* Display as much text as we can */

  pango_layout_line_get_extents (line, NULL, &logical_rect);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    {
      min_offset = 0;
      max_offset = MAX (min_offset, logical_rect.width / PANGO_SCALE - text_area_width);
    }
  else
    {
      max_offset = logical_rect.width / PANGO_SCALE - text_area_width;
      min_offset = MIN (0, max_offset);
    }

  entry->scroll_offset = CLAMP (entry->scroll_offset, min_offset, max_offset);

  /* And make sure cursors are on screen. Note that the cursor is
   * actually drawn one pixel into the INNER_BORDER space on
   * the right, when the scroll is at the utmost right. This
   * looks better to to me than confining the cursor inside the
   * border entirely, though it means that the cursor gets one
   * pixel closer to the the edge of the widget on the right than
   * on the left. This might need changing if one changed
   * INNER_BORDER from 2 to 1, as one would do on a
   * small-screen-real-estate display.
   *
   * We always make sure that the strong cursor is on screen, and
   * put the weak cursor on screen if possible.
   */

  gtk_entry_get_cursor_locations (entry, &strong_x, &weak_x);
  
  strong_xoffset = strong_x - entry->scroll_offset;

  if (strong_xoffset < 0)
    {
      entry->scroll_offset += strong_xoffset;
      strong_xoffset = 0;
    }
  else if (strong_xoffset > text_area_width)
    {
      entry->scroll_offset += strong_xoffset - text_area_width;
      strong_xoffset = text_area_width;
    }

  weak_xoffset = weak_x - entry->scroll_offset;

  if (weak_xoffset < 0 && strong_xoffset - weak_xoffset <= text_area_width)
    {
      entry->scroll_offset += weak_xoffset;
    }
  else if (weak_xoffset > text_area_width &&
	   strong_xoffset - (weak_xoffset - text_area_width) >= 0)
    {
      entry->scroll_offset += weak_xoffset - text_area_width;
    }

  gtk_widget_queue_draw (GTK_WIDGET (entry));
}

static void
gtk_entry_insert_text (GtkEditable *editable,
		       const gchar *new_text,
		       gint         new_text_length,
		       gint        *position)
{
  gint index;
  gint n_chars;
  GtkEntry *entry;
  GtkWidget *widget;
  
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_ENTRY (editable));
  g_return_if_fail (position != NULL);
  g_return_if_fail (*position >= 0 || *position < GTK_ENTRY (editable)->text_size);

  entry = GTK_ENTRY (editable);
  widget = GTK_WIDGET (editable);

  if (new_text_length < 0)
    new_text_length = strlen (new_text);

  n_chars = g_utf8_strlen (new_text, new_text_length);
  if (entry->text_max_length > 0 && n_chars + entry->text_length > entry->text_max_length)
    {
      gdk_beep ();
      n_chars = entry->text_max_length - entry->text_length;
    }

  if (new_text_length + entry->n_bytes + 1 > entry->text_size)
    {
      while (new_text_length + entry->n_bytes + 1 > entry->text_size)
	{
	  if (entry->text_size == 0)
	    entry->text_size = MIN_SIZE;
	  else
	    {
	      if (2 * (guint)entry->text_size < MAX_SIZE &&
		  2 * (guint)entry->text_size > entry->text_size)
		entry->text_size *= 2;
	      else
		{
		  entry->text_size = MAX_SIZE;
		  new_text_length = entry->text_size - new_text_length - 1;
		  break;
		}
	    }
	}

      entry->text = g_realloc (entry->text, entry->text_size);
    }

  index = g_utf8_offset_to_pointer (entry->text, *position) - entry->text;

  g_memmove (entry->text + index + new_text_length, entry->text + index, entry->n_bytes - index);
  memcpy (entry->text + index, new_text, new_text_length);

  entry->n_bytes += new_text_length;
  entry->text_length += n_chars;

  /* NUL terminate for safety and convenience */
  entry->text[entry->n_bytes] = '\0';
  
  if (editable->current_pos > *position)
    editable->current_pos += n_chars;
  
  if (editable->selection_start_pos > *position)
    editable->selection_start_pos += n_chars;

  if (editable->selection_end_pos > *position)
    editable->selection_end_pos += n_chars;

  *position += n_chars;

  if (entry->layout)
    pango_layout_set_text (entry->layout, entry->text, entry->n_bytes);

  gtk_entry_queue_draw (entry);
}

static void
gtk_entry_delete_text (GtkEditable *editable,
		       gint         start_pos,
		       gint         end_pos)
{
  GtkEntry *entry;
  
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_ENTRY (editable));

  entry = GTK_ENTRY (editable);

  if (end_pos < 0)
    end_pos = entry->text_length;
  
  if ((start_pos < end_pos) &&
      (start_pos >= 0) &&
      (end_pos <= entry->text_length))
    {
      gint start_index = g_utf8_offset_to_pointer (entry->text, start_pos) - entry->text;
      gint end_index = g_utf8_offset_to_pointer (entry->text, end_pos) - entry->text;

      g_memmove (entry->text + start_index, entry->text + end_index, entry->n_bytes - end_index);
      entry->text_length -= (end_pos - start_pos);
      entry->n_bytes -= (end_index - start_index);
      
      if (editable->current_pos > start_pos)
	editable->current_pos -= MIN (editable->current_pos, end_pos) - start_pos;

      if (editable->selection_start_pos > start_pos)
	editable->selection_start_pos -= MIN (editable->selection_start_pos, end_pos) - start_pos;

      if (editable->selection_end_pos > start_pos)
	editable->selection_end_pos -= MIN (editable->selection_end_pos, end_pos) - start_pos;

    }

  gtk_entry_queue_draw (entry);

  if (entry->layout)
    pango_layout_set_text (entry->layout, entry->text, entry->n_bytes);
}

static void
gtk_entry_update_text (GtkEditable *editable,
		       gint         start_pos,
		       gint         end_pos)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  
  gtk_entry_queue_draw (entry);
}

static gchar *    
gtk_entry_get_chars      (GtkEditable   *editable,
			  gint           start_pos,
			  gint           end_pos)
{
  GtkEntry *entry;
  gint start_index, end_index;
  
  g_return_val_if_fail (editable != NULL, NULL);
  g_return_val_if_fail (GTK_IS_ENTRY (editable), NULL);

  entry = GTK_ENTRY (editable);

  if (end_pos < 0)
    end_pos = entry->text_length;

  start_pos = MIN (entry->text_length, start_pos);
  end_pos = MIN (entry->text_length, end_pos);

  start_index = g_utf8_offset_to_pointer (entry->text, start_pos) - entry->text;
  end_index = g_utf8_offset_to_pointer (entry->text, end_pos) - entry->text;

  return g_strndup (entry->text + start_index, end_index - start_index);
}

static void 
gtk_entry_move_cursor (GtkEditable *editable,
		       gint         x,
		       gint         y)
{
  GtkEntry *entry;
  gint index;

  entry = GTK_ENTRY (editable);

  index = g_utf8_offset_to_pointer (entry->text, editable->current_pos) - entry->text;
  
  /* Horizontal motion */

  if ((gint)editable->current_pos < -x)
    editable->current_pos = 0;
  else if (editable->current_pos + x > entry->text_length)
    editable->current_pos = entry->text_length;
  else
    editable->current_pos += x;

  /* Ignore vertical motion */
}

static void 
gtk_entry_move_cursor_visually (GtkEditable *editable,
				gint         count)
{
  GtkEntry *entry;
  gint index;

  entry = GTK_ENTRY (editable);

  index = g_utf8_offset_to_pointer (entry->text, editable->current_pos) - entry->text;
  
  gtk_entry_ensure_layout (entry);

  while (count != 0)
    {
      int new_index, new_trailing;
      
      if (count > 0)
	{
	  pango_layout_move_cursor_visually (entry->layout, index, 0, 1, &new_index, &new_trailing);
	  count--;
	}
      else
	{
	  pango_layout_move_cursor_visually (entry->layout, index, 0, -1, &new_index, &new_trailing);
	  count++;
	}

      if (new_index < 0 || new_index == G_MAXINT)
	break;
      
      if (new_trailing)
	index = g_utf8_next_char (entry->text + new_index) - entry->text;
      else
	index = new_index;
    }

  editable->current_pos = g_utf8_pointer_to_offset (entry->text, entry->text + index);
}

static void
gtk_move_forward_character (GtkEntry *entry)
{
  gtk_entry_move_cursor_visually (GTK_EDITABLE (entry), 1);
}

static void
gtk_move_backward_character (GtkEntry *entry)
{
  gtk_entry_move_cursor_visually (GTK_EDITABLE (entry), -1);
}

static void 
gtk_entry_move_word (GtkEditable *editable,
		     gint         n)
{
  while (n-- > 0)
    gtk_move_forward_word (GTK_ENTRY (editable));
  while (n++ < 0)
    gtk_move_backward_word (GTK_ENTRY (editable));
}

static void
gtk_move_forward_word (GtkEntry *entry)
{
  GtkEditable *editable;
  gint i;

  editable = GTK_EDITABLE (entry);

  /* Prevent any leak of information */
  if (!editable->visible)
    {
      editable->current_pos = entry->text_length;
      return;
    }

  if (entry->text && (editable->current_pos < entry->text_length))
    {
      PangoLogAttr *log_attrs;
      gint n_attrs, old_pos;

      gtk_entry_ensure_layout (entry);
      pango_layout_get_log_attrs (entry->layout, &log_attrs, &n_attrs);

      i = old_pos = editable->current_pos;

      /* Advance over white space */
      while (i < n_attrs && log_attrs[i].is_white)
	i++;
      
      /* Find the next word beginning */
      i++;
      while (i < n_attrs && !log_attrs[i].is_word_stop)
	i++;

      editable->current_pos = MAX (entry->text_length, i);

      /* Back up over white space */
      while (i > 0 && log_attrs[i - 1].is_white)
	i--;

      if (i != old_pos)
	editable->current_pos = i;

      g_free (log_attrs);
    }
}

static void
gtk_move_backward_word (GtkEntry *entry)
{
  GtkEditable *editable;
  gint i;

  editable = GTK_EDITABLE (entry);

  /* Prevent any leak of information */
  if (!editable->visible)
    {
      editable->current_pos = 0;
      return;
    }

  if (entry->text && editable->current_pos > 0)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;

      gtk_entry_ensure_layout (entry);
      pango_layout_get_log_attrs (entry->layout, &log_attrs, &n_attrs);

      i = editable->current_pos - 1;

      /* Find the previous word beginning */
      while (i > 0 && !log_attrs[i].is_word_stop)
	i--;

      g_free (log_attrs);
    }
}

static void
gtk_entry_move_to_column (GtkEditable *editable, gint column)
{
  GtkEntry *entry;

  entry = GTK_ENTRY (editable);
  
  if (column < 0 || column > entry->text_length)
    editable->current_pos = entry->text_length;
  else
    editable->current_pos = column;
}

static void
gtk_move_beginning_of_line (GtkEntry *entry)
{
  gtk_entry_move_to_column (GTK_EDITABLE (entry), 0);
}

static void
gtk_move_end_of_line (GtkEntry *entry)
{
  gtk_entry_move_to_column (GTK_EDITABLE (entry), -1);
}

static void
gtk_entry_kill_char (GtkEditable *editable,
		     gint         direction)
{
  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_delete_selection (editable);
  else
    {
      gint old_pos = editable->current_pos;
      if (direction >= 0)
	{
	  gtk_entry_move_cursor (editable, 1, 0);
	  gtk_editable_delete_text (editable, old_pos, editable->current_pos);
	}
      else
	{
	  gtk_entry_move_cursor (editable, -1, 0);
	  gtk_editable_delete_text (editable, editable->current_pos, old_pos);
	}
    }
}

static void
gtk_delete_forward_character (GtkEntry *entry)
{
  gtk_entry_kill_char (GTK_EDITABLE (entry), 1);
}

static void
gtk_delete_backward_character (GtkEntry *entry)
{
  gtk_entry_kill_char (GTK_EDITABLE (entry), -1);
}

static void
gtk_entry_kill_word (GtkEditable *editable,
		     gint         direction)
{
  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_delete_selection (editable);
  else
    {
      gint old_pos = editable->current_pos;
      if (direction >= 0)
	{
	  gtk_entry_move_word (editable, 1);
	  gtk_editable_delete_text (editable, old_pos, editable->current_pos);
	}
      else
	{
	  gtk_entry_move_word (editable, -1);
	  gtk_editable_delete_text (editable, editable->current_pos, old_pos);
	}
    }
}

static void
gtk_delete_forward_word (GtkEntry *entry)
{
  gtk_entry_kill_word (GTK_EDITABLE (entry), 1);
}

static void
gtk_delete_backward_word (GtkEntry *entry)
{
  gtk_entry_kill_word (GTK_EDITABLE (entry), -1);
}

static void
gtk_entry_kill_line (GtkEditable *editable,
		     gint         direction)
{
  gint old_pos = editable->current_pos;
  if (direction >= 0)
    {
      gtk_entry_move_to_column (editable, -1);
      gtk_editable_delete_text (editable, old_pos, editable->current_pos);
    }
  else
    {
      gtk_entry_move_to_column (editable, 0);
      gtk_editable_delete_text (editable, editable->current_pos, old_pos);
    }
}

static void
gtk_delete_line (GtkEntry *entry)
{
  gtk_entry_move_to_column (GTK_EDITABLE (entry), 0);
  gtk_entry_kill_line (GTK_EDITABLE (entry), 1);
}

static void
gtk_delete_to_line_end (GtkEntry *entry)
{
  gtk_editable_delete_text (GTK_EDITABLE(entry), GTK_EDITABLE(entry)->current_pos, entry->text_length);
}

static void
gtk_select_word (GtkEntry *entry,
		 guint32   time)
{
  GtkEditable *editable;
  gint start_pos;
  gint end_pos;

  editable = GTK_EDITABLE (entry);

  gtk_move_backward_word (entry);
  start_pos = editable->current_pos;

  gtk_move_forward_word (entry);
  end_pos = editable->current_pos;

  editable->has_selection = TRUE;
  gtk_entry_set_selection (editable, start_pos, end_pos);
  gtk_editable_claim_selection (editable, start_pos != end_pos, time);
}

static void
gtk_select_line (GtkEntry *entry,
		 guint32   time)
{
  GtkEditable *editable;

  editable = GTK_EDITABLE (entry);

  editable->has_selection = TRUE;
  gtk_entry_set_selection (editable, 0, entry->text_length);
  gtk_editable_claim_selection (editable, entry->text_length != 0, time);

  editable->current_pos = editable->selection_end_pos;
}

static void 
gtk_entry_set_selection (GtkEditable       *editable,
			 gint               start,
			 gint               end)
{
  GtkEntry *entry;
  
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_ENTRY (editable));

  entry = GTK_ENTRY (editable);

  if (end < 0)
    end = GTK_ENTRY (editable)->text_length;
  
  editable->selection_start_pos = start;
  editable->selection_end_pos = end;

  gtk_entry_queue_draw (GTK_ENTRY (editable));
}

void       
gtk_entry_select_region  (GtkEntry       *entry,
			  gint            start,
			  gint            end)
{
  gtk_editable_select_region (GTK_EDITABLE (entry), start, end);
}

void
gtk_entry_set_max_length (GtkEntry     *entry,
                          guint16       max)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (max && entry->text_length > max)
    gtk_editable_delete_text (GTK_EDITABLE(entry), max, -1);
  
  entry->text_max_length = max;
}

    			  
static void 
gtk_entry_style_set	(GtkWidget      *widget,
			 GtkStyle       *previous_style)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  if (previous_style && GTK_WIDGET_REALIZED (widget))
    {
      entry_adjust_scroll (entry);

      gdk_window_set_background (widget->window, &widget->style->base[GTK_WIDGET_STATE (widget)]);
      gdk_window_set_background (entry->text_area, &widget->style->base[GTK_WIDGET_STATE (widget)]);
    }

  if (entry->layout)
    pango_layout_context_changed (entry->layout);
}

static void 
gtk_entry_direction_changed (GtkWidget        *widget,
			     GtkTextDirection  previous_dir)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  if (entry->layout)
    pango_layout_context_changed (entry->layout);

  GTK_WIDGET_CLASS (parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_entry_state_changed (GtkWidget      *widget,
			 GtkStateType    previous_state)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_set_background (widget->window, &widget->style->base[GTK_WIDGET_STATE (widget)]);
      gdk_window_set_background (GTK_ENTRY (widget)->text_area, &widget->style->base[GTK_WIDGET_STATE (widget)]);
    }

  if (GTK_WIDGET_DRAWABLE (widget))
    gtk_widget_queue_clear(widget);
}

static void
gtk_entry_commit_cb (GtkIMContext *context,
		     const gchar  *str,
		     GtkEntry     *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint tmp_pos = editable->current_pos;

  gtk_editable_insert_text (editable, str, strlen (str), &tmp_pos);
  editable->current_pos = tmp_pos;
}

