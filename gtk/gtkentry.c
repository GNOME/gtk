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

static void gtk_entry_class_init          (GtkEntryClass     *klass);
static void gtk_entry_init                (GtkEntry          *entry);
static void gtk_entry_finalize            (GtkObject         *object);
static void gtk_entry_realize             (GtkWidget         *widget);
static void gtk_entry_unrealize           (GtkWidget         *widget);
static void gtk_entry_draw_focus          (GtkWidget         *widget);
static void gtk_entry_size_request        (GtkWidget         *widget,
					   GtkRequisition    *requisition);
static void gtk_entry_size_allocate       (GtkWidget         *widget,
					   GtkAllocation     *allocation);
static void gtk_entry_make_backing_pixmap (GtkEntry *entry,
					   gint width, gint height);
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
static void gtk_entry_draw_text           (GtkEntry          *entry);
static void gtk_entry_draw_cursor         (GtkEntry          *entry);
static void gtk_entry_draw_cursor_on_drawable
					  (GtkEntry          *entry,
					   GdkDrawable       *drawable);
static void gtk_entry_queue_draw          (GtkEntry          *entry);
static gint gtk_entry_timer               (gpointer           data);
static gint gtk_entry_position            (GtkEntry          *entry,
					   gint               x);
       void gtk_entry_adjust_scroll       (GtkEntry          *entry);
static void gtk_entry_grow_text           (GtkEntry          *entry);
static void gtk_entry_insert_text         (GtkEditable       *editable,
					   const gchar       *new_text,
					   gint               new_text_length,
					   gint              *position);
static void gtk_entry_delete_text         (GtkEditable       *editable,
					   gint               start_pos,
					   gint               end_pos);
static void gtk_entry_update_text         (GtkEditable       *editable,
					   gint               start_pos,
					   gint               end_pos);
static gchar *gtk_entry_get_chars         (GtkEditable       *editable,
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
static void gtk_select_word               (GtkEntry          *entry,
					   guint32            time);
static void gtk_select_line               (GtkEntry          *entry,
					   guint32            time);


static void gtk_entry_set_selection       (GtkEditable       *editable,
					   gint               start,
					   gint               end);

static GtkWidgetClass *parent_class = NULL;
static GdkAtom ctext_atom = GDK_NONE;

static GtkTextFunction control_keys[26] =
{
  (GtkTextFunction)gtk_move_beginning_of_line,    /* a */
  (GtkTextFunction)gtk_move_backward_character,   /* b */
  gtk_editable_copy_clipboard,                    /* c */
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
  gtk_editable_paste_clipboard,                   /* v */
  (GtkTextFunction)gtk_delete_backward_word,      /* w */
  gtk_editable_cut_clipboard,                     /* x */
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
	(GtkArgSetFunc) NULL,
	(GtkArgGetFunc) NULL,
      };

      entry_type = gtk_type_unique (gtk_editable_get_type (), &entry_info);
    }

  return entry_type;
}

static void
gtk_entry_class_init (GtkEntryClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkEditableClass *editable_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  editable_class = (GtkEditableClass*) class;

  parent_class = gtk_type_class (gtk_editable_get_type ());

  object_class->finalize = gtk_entry_finalize;

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

  editable_class->insert_text = gtk_entry_insert_text;
  editable_class->delete_text = gtk_entry_delete_text;
  editable_class->update_text = gtk_entry_update_text;
  editable_class->get_chars   = gtk_entry_get_chars;
  editable_class->set_selection = gtk_entry_set_selection;
  editable_class->changed = (void (*)(GtkEditable *)) gtk_entry_adjust_scroll;
  editable_class->activate = NULL;
}

static void
gtk_entry_init (GtkEntry *entry)
{
  GTK_WIDGET_SET_FLAGS (entry, GTK_CAN_FOCUS);

  entry->text_area = NULL;
  entry->backing_pixmap = NULL;
  entry->text = NULL;
  entry->text_size = 0;
  entry->text_length = 0;
  entry->text_max_length = 0;
  entry->scroll_offset = 0;
  entry->timer = 0;
  entry->button = 0;
  entry->visible = 1;

  gtk_entry_grow_text (entry);
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

  GtkEditable *editable;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (text != NULL);

  editable = GTK_EDITABLE (entry);
  
  gtk_entry_delete_text (GTK_EDITABLE(entry), 0, entry->text_length);

  tmp_pos = 0;
  gtk_editable_insert_text (editable, text, strlen (text), &tmp_pos);
  editable->current_pos = tmp_pos;

  editable->selection_start_pos = 0;
  editable->selection_end_pos = 0;

  if (GTK_WIDGET_DRAWABLE (entry))
    gtk_entry_draw_text (entry);
}

void
gtk_entry_append_text (GtkEntry *entry,
		       const gchar *text)
{
  gint tmp_pos;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  tmp_pos = entry->text_length;
  gtk_editable_insert_text (GTK_EDITABLE(entry), text, strlen (text), &tmp_pos);
  GTK_EDITABLE(entry)->current_pos = tmp_pos;
}

void
gtk_entry_prepend_text (GtkEntry *entry,
			const gchar *text)
{
  gint tmp_pos;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  tmp_pos = 0;
  gtk_editable_insert_text (GTK_EDITABLE(entry), text, strlen (text), &tmp_pos);
  GTK_EDITABLE(entry)->current_pos = tmp_pos;
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
  GTK_EDITABLE(entry)->editable = editable;
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
gtk_entry_finalize (GtkObject *object)
{
  GtkEntry *entry;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_ENTRY (object));

  entry = GTK_ENTRY (object);

#ifdef USE_XIM
  if (GTK_EDITABLE(entry)->ic)
    {
      gdk_ic_destroy (GTK_EDITABLE(entry)->ic);
      GTK_EDITABLE(entry)->ic = NULL;
    }
#endif

  if (entry->timer)
    gtk_timeout_remove (entry->timer);

  entry->text_size = 0;
  if (entry->text)
    g_free (entry->text);
  entry->text = NULL;

  if (entry->backing_pixmap)
    gdk_pixmap_unref (entry->backing_pixmap);

  (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_entry_realize (GtkWidget *widget)
{
  GtkEntry *entry;
  GtkEditable *editable;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);

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

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, entry);

  attributes.x = widget->style->klass->xthickness + INNER_BORDER;
  attributes.y = widget->style->klass->ythickness + INNER_BORDER;
  attributes.width = widget->allocation.width - attributes.x * 2;
  attributes.height = widget->allocation.height - attributes.y * 2;
  attributes.cursor = entry->cursor = gdk_cursor_new (GDK_XTERM);
  attributes_mask |= GDK_WA_CURSOR;

  entry->text_area = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (entry->text_area, entry);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_background (widget->window, &widget->style->base[GTK_STATE_NORMAL]);
  gdk_window_set_background (entry->text_area, &widget->style->base[GTK_STATE_NORMAL]);

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
	  editable->ic = gdk_ic_new (entry->text_area, entry->text_area,
  			       style,
			       "spotLocation", &spot,
			       "area", &rect,
			       "fontSet", GDK_FONT_XFONT (widget->style->font),
			       NULL);
	  break;
	default:
	  editable->ic = gdk_ic_new (entry->text_area, entry->text_area,
				  style, NULL);
	}
     
      if (editable->ic == NULL)
	g_warning ("Can't create input context.");
      else
	{
	  GdkColormap *colormap;

	  mask = gdk_window_get_events (entry->text_area);
	  mask |= gdk_ic_get_events (editable->ic);
	  gdk_window_set_events (entry->text_area, mask);

	  if ((colormap = gtk_widget_get_colormap (widget)) !=
	    	gtk_widget_get_default_colormap ())
	    {
	      gdk_ic_set_attr (editable->ic, "preeditAttributes",
	      		       "colorMap", GDK_COLORMAP_XCOLORMAP (colormap),
			       NULL);
	    }
	  gdk_ic_set_attr (editable->ic,"preeditAttributes",
		     "foreground", widget->style->fg[GTK_STATE_NORMAL].pixel,
		     "background", widget->style->base[GTK_STATE_NORMAL].pixel,
		     NULL);
	}
    }
#endif

  gdk_window_show (entry->text_area);

  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_claim_selection (editable, TRUE, GDK_CURRENT_TIME);
}

static void
gtk_entry_unrealize (GtkWidget *widget)
{
  GtkEntry *entry;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));

  entry = GTK_ENTRY (widget);

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
      else
	{
	  gdk_draw_rectangle (widget->window, 
			      widget->style->base_gc[GTK_WIDGET_STATE(widget)],
			      FALSE, x + 2, y + 2, width - 5, height - 5);
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
  GtkEditable *editable;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);

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
      if (editable->ic && (gdk_ic_get_style (editable->ic) & GdkIMPreeditPosition))
	{
	  gint width, height;
	  GdkRectangle rect;

	  gdk_window_get_size (entry->text_area, &width, &height);
	  rect.x = 0;
	  rect.y = 0;
	  rect.width = width;
	  rect.height = height;
	  gdk_ic_set_attr (editable->ic, "preeditAttributes", "area", &rect, NULL);
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
  GtkEditable *editable;
  gint tmp_pos;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (ctext_atom == GDK_NONE)
    ctext_atom = gdk_atom_intern ("COMPOUND_TEXT", FALSE);

  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);
  
  if (entry->button && (event->type == GDK_BUTTON_PRESS))
    {
      GdkEventButton release_event = *event;

      release_event.type = GDK_BUTTON_RELEASE;
      release_event.button = entry->button;

      gtk_entry_button_release (widget, &release_event);
    }

  entry->button = event->button;
  
  if (!GTK_WIDGET_HAS_FOCUS (widget))
    gtk_widget_grab_focus (widget);

  if (event->button == 1)
    {
      switch (event->type)
	{
	case GDK_BUTTON_PRESS:
	  gtk_grab_add (widget);

	  tmp_pos = gtk_entry_position (entry, event->x + entry->scroll_offset);
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
    }
  else if (event->type == GDK_BUTTON_PRESS)
    {
      if ((event->button == 2) && editable->editable)
	{
	  if (editable->selection_start_pos == editable->selection_end_pos ||
	      editable->has_selection)
	    editable->current_pos = gtk_entry_position (entry, event->x + entry->scroll_offset);
	  gtk_selection_convert (widget, GDK_SELECTION_PRIMARY,
				 ctext_atom, event->time);
	}
      else
	{
	  gtk_grab_add (widget);

	  tmp_pos = gtk_entry_position (entry, event->x + entry->scroll_offset);
	  gtk_entry_set_selection (editable, tmp_pos, tmp_pos);
	  editable->has_selection = FALSE;
	  editable->current_pos = editable->selection_start_pos;

	  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, event->time);
	}
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
    }
  else if (event->button == 3)
    {
      gtk_grab_remove (widget);
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

  GTK_EDITABLE(entry)->selection_end_pos = gtk_entry_position (entry, event->x + entry->scroll_offset);
  GTK_EDITABLE(entry)->current_pos = GTK_EDITABLE(entry)->selection_end_pos;
  gtk_entry_adjust_scroll (entry);
  gtk_entry_queue_draw (entry);

  return FALSE;
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
  guint tmp_pos;
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
	  gtk_editable_paste_clipboard (editable, event->time);
	}
      else if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_editable_copy_clipboard (editable, event->time);
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
	  gtk_editable_cut_clipboard (editable, event->time);
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
      gtk_signal_emit_by_name (GTK_OBJECT (entry), "activate");
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
      if (event->length > 0)
	{
	  extend_selection = FALSE;
	  gtk_editable_delete_selection (editable);

	  tmp_pos = editable->current_pos;
	  gtk_editable_insert_text (editable, event->string, event->length, &tmp_pos);
	  editable->current_pos = tmp_pos;

	  return_val = TRUE;
	}
      break;
    }

  if (return_val && (editable->current_pos != initial_pos))
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
  if (GTK_EDITABLE(widget)->ic)
    gdk_im_begin (GTK_EDITABLE(widget)->ic, GTK_ENTRY(widget)->text_area);
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

static void
gtk_entry_make_backing_pixmap (GtkEntry *entry, gint width, gint height)
{
  gint pixmap_width, pixmap_height;

  if (!entry->backing_pixmap)
    {
      /* allocate */
      entry->backing_pixmap = gdk_pixmap_new (entry->text_area,
					      width, height,
					      -1);
    }
  else
    {
      /* reallocate if sizes don't match */
      gdk_window_get_size (entry->backing_pixmap,
			   &pixmap_width, &pixmap_height);
      if ((pixmap_width != width) || (pixmap_height != height))
	{
	  gdk_pixmap_unref (entry->backing_pixmap);
	  entry->backing_pixmap = gdk_pixmap_new (entry->text_area,
						  width, height,
						  -1);
	}
    }
}

static void
gtk_entry_draw_text (GtkEntry *entry)
{
  GtkWidget *widget;
  GtkEditable *editable;
  GtkStateType selected_state;
  gint selection_start_pos;
  gint selection_end_pos;
  gint selection_start_xoffset;
  gint selection_end_xoffset;
  gint width, height;
  gint y;
  GdkDrawable *drawable;
  gint use_backing_pixmap;

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
      editable = GTK_EDITABLE (entry);

      if (!entry->text)
	{	  
	  gdk_window_clear (entry->text_area);
	  if (editable->editable)
	    gtk_entry_draw_cursor (entry);
	  return;
	}

      gdk_window_get_size (entry->text_area, &width, &height);

      /*
	If the widget has focus, draw on a backing pixmap to avoid flickering
	and copy it to the text_area.
	Otherwise draw to text_area directly for better speed.
      */
      use_backing_pixmap = GTK_WIDGET_HAS_FOCUS (widget) && (entry->text != NULL);
      if (use_backing_pixmap)
	{
	  gtk_entry_make_backing_pixmap (entry, width, height);
	  drawable = entry->backing_pixmap;
	  gdk_draw_rectangle (drawable,
			      widget->style->base_gc[GTK_WIDGET_STATE(widget)],
			      TRUE,
			      0, 0,
			      width,
			      height);
	}
      else
	{
	  drawable = entry->text_area;
	  gdk_window_clear (entry->text_area);
	}
 
      y = (height - (widget->style->font->ascent + widget->style->font->descent)) / 2;
      y += widget->style->font->ascent;

      if (editable->selection_start_pos != editable->selection_end_pos)
	{
	  selected_state = GTK_STATE_SELECTED;
	  if (!editable->has_selection)
	    selected_state = GTK_STATE_ACTIVE;

	  selection_start_pos = MIN (editable->selection_start_pos, editable->selection_end_pos);
	  selection_end_pos = MAX (editable->selection_start_pos, editable->selection_end_pos);

	  selection_start_xoffset = gdk_text_width (widget->style->font,
						    entry->text,
						    selection_start_pos);
	  selection_end_xoffset = gdk_text_width (widget->style->font,
						  entry->text,
						  selection_end_pos);

	  if (selection_start_pos > 0)
	    gdk_draw_text (drawable, widget->style->font,
			   widget->style->fg_gc[GTK_STATE_NORMAL],
			   -entry->scroll_offset, y,
			   entry->text, selection_start_pos);

	  gdk_draw_rectangle (drawable,
			      widget->style->bg_gc[selected_state],
			      TRUE,
			      -entry->scroll_offset + selection_start_xoffset,
			      0,
			      selection_end_xoffset - selection_start_xoffset,
			      -1);

	  gdk_draw_text (drawable, widget->style->font,
			 widget->style->fg_gc[selected_state],
			 -entry->scroll_offset + selection_start_xoffset, y,
			 entry->text + selection_start_pos,
			 selection_end_pos - selection_start_pos);

	  if (selection_end_pos < entry->text_length)
	    gdk_draw_string (drawable, widget->style->font,
			     widget->style->fg_gc[GTK_STATE_NORMAL],
			     -entry->scroll_offset + selection_end_xoffset, y,
			     entry->text + selection_end_pos);
	}
      else
	{
	  GdkGCValues values;
	  
	  gdk_gc_get_values (widget->style->fg_gc[GTK_STATE_NORMAL], &values);
	  gdk_draw_string (drawable, widget->style->font,
			   widget->style->fg_gc[GTK_STATE_NORMAL],
			   -entry->scroll_offset, y,
			   entry->text);
	}

      if (editable->editable)
	gtk_entry_draw_cursor_on_drawable (entry, drawable);

      if (use_backing_pixmap)
	gdk_draw_pixmap(entry->text_area,
			widget->style->fg_gc[GTK_STATE_NORMAL],
			entry->backing_pixmap,
			0, 0, 0, 0, width, height);	  
    }
}

static void
gtk_entry_draw_cursor (GtkEntry *entry)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_entry_draw_cursor_on_drawable (entry, entry->text_area);
}

static void
gtk_entry_draw_cursor_on_drawable (GtkEntry *entry, GdkDrawable *drawable)
{
  GtkWidget *widget;
  GtkEditable *editable;
  GdkGC *gc;
  gint xoffset;
  gint text_area_height;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (GTK_WIDGET_DRAWABLE (entry))
    {
      widget = GTK_WIDGET (entry);
      editable = GTK_EDITABLE (entry);

      if (editable->current_pos > 0 && entry->visible)
	xoffset = gdk_text_width (widget->style->font, entry->text, editable->current_pos);
      else
	xoffset = 0;
      xoffset -= entry->scroll_offset;

      if (GTK_WIDGET_HAS_FOCUS (widget) &&
	  (editable->selection_start_pos == editable->selection_end_pos))
	gc = widget->style->fg_gc[GTK_STATE_NORMAL];
      else
	gc = widget->style->base_gc[GTK_WIDGET_STATE(widget)];

      gdk_window_get_size (entry->text_area, NULL, &text_area_height);
      gdk_draw_line (drawable, gc, xoffset, 0, xoffset, text_area_height);
#ifdef USE_XIM
      if (gdk_im_ready() && editable->ic && 
	  gdk_ic_get_style (editable->ic) & GdkIMPreeditPosition)
	{
	  GdkPoint spot;

	  spot.x = xoffset;
	  spot.y = (text_area_height + (widget->style->font->ascent - widget->style->font->descent) + 1) / 2;
	  gdk_ic_set_attr (editable->ic, "preeditAttributes", "spotLocation", &spot, NULL);
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

  if (GTK_EDITABLE(entry)->current_pos > 0)
    xoffset = gdk_text_width (GTK_WIDGET (entry)->style->font, entry->text, GTK_EDITABLE(entry)->current_pos);
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
gtk_entry_insert_text (GtkEditable *editable,
		       const gchar *new_text,
		       gint         new_text_length,
		       gint        *position)
{
  gchar *text;
  gint start_pos;
  gint end_pos;
  gint last_pos;
  gint i;

  GtkEntry *entry;
  
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_ENTRY (editable));

  entry = GTK_ENTRY (editable);

  /* Make sure we do not exceed the maximum size of the entry. */
  if (entry->text_max_length != 0 &&
      new_text_length + entry->text_length > entry->text_max_length)
    new_text_length = entry->text_max_length - entry->text_length;

  /* Don't insert anything, if there was nothing to insert. */
  if (new_text_length == 0)
    return;

  if (new_text_length < 0)
    new_text_length = strlen (new_text);

  start_pos = *position;
  end_pos = start_pos + new_text_length;
  last_pos = new_text_length + entry->text_length;

  if (editable->selection_start_pos >= *position)
    editable->selection_start_pos += new_text_length;
  if (editable->selection_end_pos >= *position)
    editable->selection_end_pos += new_text_length;

  while (last_pos >= entry->text_size)
    gtk_entry_grow_text (entry);

  text = entry->text;
  for (i = last_pos - 1; i >= end_pos; i--)
    text[i] = text[i- (end_pos - start_pos)];
  for (i = start_pos; i < end_pos; i++)
    text[i] = new_text[i - start_pos];

  entry->text_length += new_text_length;
  *position = end_pos;

  gtk_entry_queue_draw (entry);
}

static void
gtk_entry_delete_text (GtkEditable *editable,
		       gint         start_pos,
		       gint         end_pos)
{
  gchar *text;
  gint deletion_length;
  gint i;

  GtkEntry *entry;
  
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_ENTRY (editable));

  entry = GTK_ENTRY (editable);

  if (editable->selection_start_pos > start_pos)
    editable->selection_start_pos -= MIN(end_pos, editable->selection_start_pos) - start_pos;
  if (editable->selection_end_pos > start_pos)
    editable->selection_end_pos -= MIN(end_pos, editable->selection_end_pos) - start_pos;

  if (end_pos < 0)
    end_pos = entry->text_length;

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
      editable->current_pos = start_pos;
    }

  gtk_entry_queue_draw (entry);
}

static void
gtk_entry_update_text (GtkEditable *editable,
		       gint         start_pos,
		       gint         end_pos)
{
  gtk_entry_queue_draw (GTK_ENTRY(editable));
}

gchar *    
gtk_entry_get_chars      (GtkEditable   *editable,
			  gint           start_pos,
			  gint           end_pos)
{
  gchar *retval;
  GtkEntry *entry;
  gchar c;
  
  g_return_val_if_fail (editable != NULL, NULL);
  g_return_val_if_fail (GTK_IS_ENTRY (editable), NULL);

  entry = GTK_ENTRY (editable);

  if (end_pos < 0)
    end_pos = entry->text_length;

  start_pos = MIN(entry->text_length, start_pos);
  end_pos = MIN(entry->text_length, end_pos);

  if (start_pos <= end_pos)
    {
      c = entry->text[end_pos];
      entry->text[end_pos] = '\0';
      
      retval = g_strdup (&entry->text[start_pos]);
      
      entry->text[end_pos] = c;
      
      return retval;
    }
  else
    return NULL;
}

static void
gtk_move_forward_character (GtkEntry *entry)
{
  gint len;

  GtkEditable *editable;
  editable = GTK_EDITABLE (entry);

  if (editable->current_pos < entry->text_length)
    {
      len = mblen (entry->text+editable->current_pos, MB_CUR_MAX);
      editable->current_pos += (len>0)? len:1;
    }
  if (editable->current_pos > entry->text_length)
    editable->current_pos = entry->text_length;
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
  GtkEditable *editable;
  editable = GTK_EDITABLE (entry);

  /* this routine is correct only if string is state-independent-encoded */

  if (0 < editable->current_pos)
    {
      editable->current_pos = move_backward_character (entry->text,
      						    editable->current_pos);
      if (editable->current_pos < 0)
	editable->current_pos = 0;
    }
}

static void
gtk_move_forward_word (GtkEntry *entry)
{
  gchar *text;
  gint i;
  wchar_t c;
  gint len;

  GtkEditable *editable;
  editable = GTK_EDITABLE (entry);

  if (entry->text && (editable->current_pos < entry->text_length))
    {
      text = entry->text;
      i = editable->current_pos;

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

      editable->current_pos = i;
      if (editable->current_pos > entry->text_length)
	editable->current_pos = entry->text_length;
    }
}

static void
gtk_move_backward_word (GtkEntry *entry)
{
  gchar *text;
  gint i;
  wchar_t c;
  gint len;

  GtkEditable *editable;
  editable = GTK_EDITABLE (entry);

  if (entry->text)
    {
      text = entry->text;
      i=move_backward_character(text, editable->current_pos);
      if (i < 0) /* Per */
	{
	  editable->selection_start_pos = 0;
	  editable->selection_end_pos = 0;
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

      editable->current_pos = i;
    }
}

static void
gtk_move_beginning_of_line (GtkEntry *entry)
{
  GTK_EDITABLE(entry)->current_pos = 0;
}

static void
gtk_move_end_of_line (GtkEntry *entry)
{
  GTK_EDITABLE(entry)->current_pos = entry->text_length;
}

static void
gtk_delete_forward_character (GtkEntry *entry)
{
  gint old_pos;

  GtkEditable *editable;
  editable = GTK_EDITABLE (entry);

  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_delete_selection (editable);
  else
    {
      old_pos = editable->current_pos;
      gtk_move_forward_character (entry);
      gtk_editable_delete_text (editable, old_pos, editable->current_pos);
    }
}

static void
gtk_delete_backward_character (GtkEntry *entry)
{
  gint old_pos;

  GtkEditable *editable;
  editable = GTK_EDITABLE (entry);

  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_delete_selection (editable);
  else
    {
      old_pos = editable->current_pos;
      gtk_move_backward_character (entry);
      gtk_editable_delete_text (editable, editable->current_pos, old_pos);
    }
}

static void
gtk_delete_forward_word (GtkEntry *entry)
{
  gint old_pos;

  GtkEditable *editable;
  editable = GTK_EDITABLE (entry);

  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_delete_selection (editable);
  else
    {
      old_pos = editable->current_pos;
      gtk_move_forward_word (entry);
      gtk_editable_delete_text (editable, old_pos, editable->current_pos);
    }
}

static void
gtk_delete_backward_word (GtkEntry *entry)
{
  gint old_pos;

  GtkEditable *editable;
  editable = GTK_EDITABLE (entry);

  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_delete_selection (editable);
  else
    {
      old_pos = editable->current_pos;
      gtk_move_backward_word (entry);
      gtk_editable_delete_text (editable, editable->current_pos, old_pos);
    }
}

static void
gtk_delete_line (GtkEntry *entry)
{
  gtk_editable_delete_text (GTK_EDITABLE(entry), 0, entry->text_length);
}

static void
gtk_delete_to_line_end (GtkEntry *entry)
{
  gtk_editable_delete_text (GTK_EDITABLE(entry), GTK_EDITABLE(entry)->current_pos, entry->text_length);
}

static void
gtk_select_word (GtkEntry *entry, guint32 time)
{
  gint start_pos;
  gint end_pos;

  GtkEditable *editable;
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
gtk_select_line (GtkEntry *entry, guint32 time)
{
  GtkEditable *editable;
  editable = GTK_EDITABLE (entry);

  editable->has_selection = TRUE;
  gtk_entry_set_selection (editable, 0, entry->text_length);
  gtk_editable_claim_selection (editable, entry->text_length != 0, time);

  editable->current_pos = editable->selection_end_pos;
}

static void 
gtk_entry_set_selection       (GtkEditable       *editable,
			       gint               start,
			       gint               end)
{
  g_return_if_fail (GTK_IS_ENTRY (editable));

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
  gtk_editable_select_region (GTK_EDITABLE(entry), start, end);
}

