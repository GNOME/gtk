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

#include <string.h>

#include <pango/pango.h>

#include "gdk/gdkkeysyms.h"
#include "gtkbindings.h"
#include "gtkcelleditable.h"
#include "gtkclipboard.h"
#include "gtkdnd.h"
#include "gtkentry.h"
#include "gtkimagemenuitem.h"
#include "gtkimmulticontext.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkseparatormenuitem.h"
#include "gtkselection.h"
#include "gtksettings.h"
#include "gtkstock.h"
#include "gtksignal.h"
#include "gtktextutil.h"
#include "gtkwindow.h"

#define MIN_ENTRY_WIDTH  150
#define DRAW_TIMEOUT     20
#define INNER_BORDER     2

/* Initial size of buffer, in bytes */
#define MIN_SIZE 16

/* Maximum size of text buffer, in bytes */
#define MAX_SIZE G_MAXUSHORT

enum {
  ACTIVATE,
  POPULATE_POPUP,
  MOVE_CURSOR,
  INSERT_AT_CURSOR,
  DELETE_FROM_CURSOR,
  CUT_CLIPBOARD,
  COPY_CLIPBOARD,
  PASTE_CLIPBOARD,
  TOGGLE_OVERWRITE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_CURSOR_POSITION,
  PROP_SELECTION_BOUND,
  PROP_EDITABLE,
  PROP_MAX_LENGTH,
  PROP_VISIBILITY,
  PROP_HAS_FRAME,
  PROP_INVISIBLE_CHAR,
  PROP_ACTIVATES_DEFAULT,
  PROP_WIDTH_CHARS,
  PROP_SCROLL_OFFSET,
  PROP_TEXT
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef enum {
  CURSOR_STANDARD,
  CURSOR_DND
} CursorType;

static GtkTargetEntry target_table[] = {
  { "UTF8_STRING",   0, 0 },
  { "COMPOUND_TEXT", 0, 0 },
  { "TEXT",          0, 0 },
  { "text/plain",    0, 0 },
  { "STRING",        0, 0 }
};

/* GObject, GtkObject methods
 */
static void   gtk_entry_class_init           (GtkEntryClass        *klass);
static void   gtk_entry_editable_init        (GtkEditableClass     *iface);
static void   gtk_entry_cell_editable_init   (GtkCellEditableIface *iface);
static void   gtk_entry_init                 (GtkEntry         *entry);
static void   gtk_entry_set_property (GObject         *object,
				      guint            prop_id,
				      const GValue    *value,
				      GParamSpec      *pspec);
static void   gtk_entry_get_property (GObject         *object,
				      guint            prop_id,
				      GValue          *value,
				      GParamSpec      *pspec);
static void   gtk_entry_finalize             (GObject          *object);

/* GtkWidget methods
 */
static void   gtk_entry_realize              (GtkWidget        *widget);
static void   gtk_entry_unrealize            (GtkWidget        *widget);
static void   gtk_entry_size_request         (GtkWidget        *widget,
					      GtkRequisition   *requisition);
static void   gtk_entry_size_allocate        (GtkWidget        *widget,
					      GtkAllocation    *allocation);
static void   gtk_entry_draw_frame           (GtkWidget        *widget);
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
static gint   gtk_entry_key_release          (GtkWidget        *widget,
					      GdkEventKey      *event);
static gint   gtk_entry_focus_in             (GtkWidget        *widget,
					      GdkEventFocus    *event);
static gint   gtk_entry_focus_out            (GtkWidget        *widget,
					      GdkEventFocus    *event);
static void   gtk_entry_grab_focus           (GtkWidget        *widget);
static void   gtk_entry_style_set            (GtkWidget        *widget,
					      GtkStyle         *previous_style);
static void   gtk_entry_direction_changed    (GtkWidget        *widget,
					      GtkTextDirection  previous_dir);
static void   gtk_entry_state_changed        (GtkWidget        *widget,
					      GtkStateType      previous_state);

static gboolean gtk_entry_drag_drop          (GtkWidget        *widget,
                                              GdkDragContext   *context,
                                              gint              x,
                                              gint              y,
                                              guint             time);
static gboolean gtk_entry_drag_motion        (GtkWidget        *widget,
					      GdkDragContext   *context,
					      gint              x,
					      gint              y,
					      guint             time);
static void     gtk_entry_drag_leave         (GtkWidget        *widget,
					      GdkDragContext   *context,
					      guint             time);
static void     gtk_entry_drag_data_received (GtkWidget        *widget,
					      GdkDragContext   *context,
					      gint              x,
					      gint              y,
					      GtkSelectionData *selection_data,
					      guint             info,
					      guint             time);
static void     gtk_entry_drag_data_get      (GtkWidget        *widget,
					      GdkDragContext   *context,
					      GtkSelectionData *selection_data,
					      guint             info,
					      guint             time);
static void     gtk_entry_drag_data_delete   (GtkWidget        *widget,
					      GdkDragContext   *context);

/* GtkEditable method implementations
 */
static void     gtk_entry_insert_text          (GtkEditable *editable,
						const gchar *new_text,
						gint         new_text_length,
						gint        *position);
static void     gtk_entry_delete_text          (GtkEditable *editable,
						gint         start_pos,
						gint         end_pos);
static gchar *  gtk_entry_get_chars            (GtkEditable *editable,
						gint         start_pos,
						gint         end_pos);
static void     gtk_entry_real_set_position    (GtkEditable *editable,
						gint         position);
static gint     gtk_entry_get_position         (GtkEditable *editable);
static void     gtk_entry_set_selection_bounds (GtkEditable *editable,
						gint         start,
						gint         end);
static gboolean gtk_entry_get_selection_bounds (GtkEditable *editable,
						gint        *start,
						gint        *end);

/* GtkCellEditable method implementations
 */
static void gtk_entry_start_editing (GtkCellEditable *cell_editable,
				     GdkEvent        *event);

/* Default signal handlers
 */
static void gtk_entry_real_insert_text   (GtkEditable     *editable,
					  const gchar     *new_text,
					  gint             new_text_length,
					  gint            *position);
static void gtk_entry_real_delete_text   (GtkEditable     *editable,
					  gint             start_pos,
					  gint             end_pos);
static void gtk_entry_move_cursor        (GtkEntry        *entry,
					  GtkMovementStep  step,
					  gint             count,
					  gboolean         extend_selection);
static void gtk_entry_insert_at_cursor   (GtkEntry        *entry,
					  const gchar     *str);
static void gtk_entry_delete_from_cursor (GtkEntry        *entry,
					  GtkDeleteType    type,
					  gint             count);
static void gtk_entry_cut_clipboard      (GtkEntry        *entry);
static void gtk_entry_copy_clipboard     (GtkEntry        *entry);
static void gtk_entry_paste_clipboard    (GtkEntry        *entry);
static void gtk_entry_toggle_overwrite   (GtkEntry        *entry);
static void gtk_entry_select_all         (GtkEntry        *entry);
static void gtk_entry_real_activate      (GtkEntry        *entry);
static void gtk_entry_popup_menu         (GtkWidget      *widget);

static void gtk_entry_keymap_direction_changed (GdkKeymap *keymap,
						GtkEntry  *entry);
/* IM Context Callbacks
 */
static void     gtk_entry_commit_cb               (GtkIMContext *context,
						   const gchar  *str,
						   GtkEntry     *entry);
static void     gtk_entry_preedit_changed_cb      (GtkIMContext *context,
						   GtkEntry     *entry);
static gboolean gtk_entry_retrieve_surrounding_cb (GtkIMContext *context,
						   GtkEntry     *entry);
static gboolean gtk_entry_delete_surrounding_cb   (GtkIMContext *context,
						   gint          offset,
						   gint          n_chars,
						   GtkEntry     *entry);

/* Internal routines
 */
static void         gtk_entry_enter_text               (GtkEntry       *entry,
                                                        const gchar    *str);
static void         gtk_entry_set_positions            (GtkEntry       *entry,
							gint            current_pos,
							gint            selection_bound);
static void         gtk_entry_draw_text                (GtkEntry       *entry);
static void         gtk_entry_draw_cursor              (GtkEntry       *entry,
							CursorType      type);
static PangoLayout *gtk_entry_ensure_layout            (GtkEntry       *entry,
                                                        gboolean        include_preedit);
static void         gtk_entry_queue_draw               (GtkEntry       *entry);
static void         gtk_entry_reset_im_context         (GtkEntry       *entry);
static void         gtk_entry_recompute                (GtkEntry       *entry);
static gint         gtk_entry_find_position            (GtkEntry       *entry,
							gint            x);
static void         gtk_entry_get_cursor_locations     (GtkEntry       *entry,
							CursorType      type,
							gint           *strong_x,
							gint           *weak_x);
static void         gtk_entry_adjust_scroll            (GtkEntry       *entry);
static gint         gtk_entry_move_visually            (GtkEntry       *editable,
							gint            start,
							gint            count);
static gint         gtk_entry_move_logically           (GtkEntry       *entry,
							gint            start,
							gint            count);
static gint         gtk_entry_move_forward_word        (GtkEntry       *entry,
							gint            start);
static gint         gtk_entry_move_backward_word       (GtkEntry       *entry,
							gint            start);
static void         gtk_entry_delete_whitespace        (GtkEntry       *entry);
static void         gtk_entry_select_word              (GtkEntry       *entry);
static void         gtk_entry_select_line              (GtkEntry       *entry);
static char *       gtk_entry_get_public_chars         (GtkEntry       *entry,
							gint            start,
							gint            end);
static void         gtk_entry_paste                    (GtkEntry       *entry,
							GdkAtom         selection);
static void         gtk_entry_update_primary_selection (GtkEntry       *entry);
static void         gtk_entry_do_popup                 (GtkEntry       *entry,
							GdkEventButton *event);
static gboolean     gtk_entry_mnemonic_activate        (GtkWidget      *widget,
							gboolean        group_cycling);
static void         gtk_entry_check_cursor_blink       (GtkEntry       *entry);
static void         gtk_entry_pend_cursor_blink        (GtkEntry       *entry);
static void         get_text_area_size                 (GtkEntry       *entry,
							gint           *x,
							gint           *y,
							gint           *width,
							gint           *height);
static void         get_widget_window_size             (GtkEntry       *entry,
							gint           *x,
							gint           *y,
							gint           *width,
							gint           *height);

static GtkWidgetClass *parent_class = NULL;

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
      
      static const GInterfaceInfo editable_info =
      {
	(GInterfaceInitFunc) gtk_entry_editable_init,	 /* interface_init */
	NULL,			                         /* interface_finalize */
	NULL			                         /* interface_data */
      };

      static const GInterfaceInfo cell_editable_info =
      {
	(GInterfaceInitFunc) gtk_entry_cell_editable_init,    /* interface_init */
	NULL,                                                 /* interface_finalize */
	NULL                                                  /* interface_data */
      };
      
      entry_type = gtk_type_unique (GTK_TYPE_WIDGET, &entry_info);
      g_type_add_interface_static (entry_type,
				   GTK_TYPE_EDITABLE,
				   &editable_info);
      g_type_add_interface_static (entry_type,
				   GTK_TYPE_CELL_EDITABLE,
				   &cell_editable_info);
    }

  return entry_type;
}

static void
add_move_binding (GtkBindingSet  *binding_set,
		  guint           keyval,
		  guint           modmask,
		  GtkMovementStep step,
		  gint            count)
{
  g_return_if_fail ((modmask & GDK_SHIFT_MASK) == 0);
  
  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
				"move_cursor", 3,
				GTK_TYPE_ENUM, step,
				G_TYPE_INT, count,
                                G_TYPE_BOOLEAN, FALSE);

  /* Selection-extending version */
  gtk_binding_entry_add_signal (binding_set, keyval, modmask | GDK_SHIFT_MASK,
				"move_cursor", 3,
				GTK_TYPE_ENUM, step,
				G_TYPE_INT, count,
                                G_TYPE_BOOLEAN, TRUE);
}

static void
gtk_entry_class_init (GtkEntryClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  parent_class = gtk_type_class (GTK_TYPE_WIDGET);

  gobject_class->finalize = gtk_entry_finalize;
  gobject_class->set_property = gtk_entry_set_property;
  gobject_class->get_property = gtk_entry_get_property;

  widget_class->realize = gtk_entry_realize;
  widget_class->unrealize = gtk_entry_unrealize;
  widget_class->size_request = gtk_entry_size_request;
  widget_class->size_allocate = gtk_entry_size_allocate;
  widget_class->expose_event = gtk_entry_expose;
  widget_class->button_press_event = gtk_entry_button_press;
  widget_class->button_release_event = gtk_entry_button_release;
  widget_class->motion_notify_event = gtk_entry_motion_notify;
  widget_class->key_press_event = gtk_entry_key_press;
  widget_class->key_release_event = gtk_entry_key_release;
  widget_class->focus_in_event = gtk_entry_focus_in;
  widget_class->focus_out_event = gtk_entry_focus_out;
  widget_class->grab_focus = gtk_entry_grab_focus;
  widget_class->style_set = gtk_entry_style_set;
  widget_class->direction_changed = gtk_entry_direction_changed;
  widget_class->state_changed = gtk_entry_state_changed;
  widget_class->mnemonic_activate = gtk_entry_mnemonic_activate;

  widget_class->drag_drop = gtk_entry_drag_drop;
  widget_class->drag_motion = gtk_entry_drag_motion;
  widget_class->drag_leave = gtk_entry_drag_leave;
  widget_class->drag_data_received = gtk_entry_drag_data_received;
  widget_class->drag_data_get = gtk_entry_drag_data_get;
  widget_class->drag_data_delete = gtk_entry_drag_data_delete;

  widget_class->popup_menu = gtk_entry_popup_menu;

  class->move_cursor = gtk_entry_move_cursor;
  class->insert_at_cursor = gtk_entry_insert_at_cursor;
  class->delete_from_cursor = gtk_entry_delete_from_cursor;
  class->cut_clipboard = gtk_entry_cut_clipboard;
  class->copy_clipboard = gtk_entry_copy_clipboard;
  class->paste_clipboard = gtk_entry_paste_clipboard;
  class->toggle_overwrite = gtk_entry_toggle_overwrite;
  class->activate = gtk_entry_real_activate;
  
  g_object_class_install_property (gobject_class,
                                   PROP_CURSOR_POSITION,
                                   g_param_spec_int ("cursor_position",
                                                     _("Cursor Position"),
                                                     _("The current position of the insertion cursor in chars."),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTION_BOUND,
                                   g_param_spec_int ("selection_bound",
                                                     _("Selection Bound"),
                                                     _("The position of the opposite end of the selection from the cursor in chars."),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_EDITABLE,
                                   g_param_spec_boolean ("editable",
							 _("Editable"),
							 _("Whether the entry contents can be edited"),
                                                         TRUE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_MAX_LENGTH,
                                   g_param_spec_int ("max_length",
                                                     _("Maximum length"),
                                                     _("Maximum number of characters for this entry"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_VISIBILITY,
                                   g_param_spec_boolean ("visibility",
							 _("Visibility"),
							 _("FALSE displays the \"invisible char\" instead of the actual text (password mode)"),
                                                         TRUE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_HAS_FRAME,
                                   g_param_spec_boolean ("has_frame",
							 _("Has Frame"),
							 _("FALSE removes outside bevel from entry."),
                                                         TRUE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE));

    g_object_class_install_property (gobject_class,
                                   PROP_INVISIBLE_CHAR,
                                   g_param_spec_unichar ("invisible_char",
							 _("Invisible character"),
							 _("The character to use when masking entry contents (in \"password mode\")"),
							 '*',
							 G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVATES_DEFAULT,
                                   g_param_spec_boolean ("activates_default",
							 _("Activates default"),
							 _("Whether to activate the default widget (such as the default button in a dialog) when Enter is pressed."),
                                                         FALSE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_WIDTH_CHARS,
                                   g_param_spec_int ("width_chars",
                                                     _("Width in chars"),
                                                     _("Number of characters to leave space for in the entry."),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_SCROLL_OFFSET,
                                   g_param_spec_int ("scroll_offset",
                                                     _("Scroll offset"),
                                                     _("Number of pixels of the entry scrolled off the screen to the left"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
							_("Text"),
							_("The contents of the entry"),
							"",
							G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("cursor_color",
							       _("Cursor color"),
							       _("Color with which to draw insertion cursor"),
							       GDK_TYPE_COLOR,
							       G_PARAM_READABLE));

  signals[POPULATE_POPUP] =
    gtk_signal_new ("populate_popup",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkEntryClass, populate_popup),
		    _gtk_marshal_VOID__OBJECT,
		    GTK_TYPE_NONE, 1, GTK_TYPE_MENU);
  
 /* Action signals */
  
  signals[ACTIVATE] =
    gtk_signal_new ("activate",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkEntryClass, activate),
		    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  widget_class->activate_signal = signals[ACTIVATE];

  signals[MOVE_CURSOR] = 
      gtk_signal_new ("move_cursor",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      GTK_CLASS_TYPE (object_class),
                      GTK_SIGNAL_OFFSET (GtkEntryClass, move_cursor),
                      _gtk_marshal_VOID__ENUM_INT_BOOLEAN,
                      GTK_TYPE_NONE, 3, GTK_TYPE_MOVEMENT_STEP, GTK_TYPE_INT, GTK_TYPE_BOOL);

  signals[INSERT_AT_CURSOR] = 
      gtk_signal_new ("insert_at_cursor",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      GTK_CLASS_TYPE (object_class),
                      GTK_SIGNAL_OFFSET (GtkEntryClass, insert_at_cursor),
                      _gtk_marshal_VOID__STRING,
                      GTK_TYPE_NONE, 1, GTK_TYPE_STRING);

  signals[DELETE_FROM_CURSOR] = 
      gtk_signal_new ("delete_from_cursor",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      GTK_CLASS_TYPE (object_class),
                      GTK_SIGNAL_OFFSET (GtkEntryClass, delete_from_cursor),
                      _gtk_marshal_VOID__ENUM_INT,
                      GTK_TYPE_NONE, 2, GTK_TYPE_DELETE_TYPE, GTK_TYPE_INT);

  signals[CUT_CLIPBOARD] =
    gtk_signal_new ("cut_clipboard",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkEntryClass, cut_clipboard),
                    _gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);

  signals[COPY_CLIPBOARD] =
    gtk_signal_new ("copy_clipboard",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkEntryClass, copy_clipboard),
                    _gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);

  signals[PASTE_CLIPBOARD] =
    gtk_signal_new ("paste_clipboard",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkEntryClass, paste_clipboard),
                    _gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);

  signals[TOGGLE_OVERWRITE] =
    gtk_signal_new ("toggle_overwrite",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkEntryClass, toggle_overwrite),
                    _gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);

  /*
   * Key bindings
   */

  binding_set = gtk_binding_set_by_class (class);

  /* Moving the insertion point */
  add_move_binding (binding_set, GDK_Right, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_Left, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  add_move_binding (binding_set, GDK_KP_Right, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_KP_Left, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, -1);
  
  add_move_binding (binding_set, GDK_f, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_LOGICAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_b, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_LOGICAL_POSITIONS, -1);
  
  add_move_binding (binding_set, GDK_Right, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_Left, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (binding_set, GDK_KP_Right, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_KP_Left, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, -1);
  
  add_move_binding (binding_set, GDK_a, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_PARAGRAPH_ENDS, -1);

  add_move_binding (binding_set, GDK_e, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_PARAGRAPH_ENDS, 1);

  add_move_binding (binding_set, GDK_f, GDK_MOD1_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_b, GDK_MOD1_MASK,
		    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (binding_set, GDK_Home, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_End, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (binding_set, GDK_KP_Home, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_KP_End, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);
  
  add_move_binding (binding_set, GDK_Home, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_End, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, 1);

  add_move_binding (binding_set, GDK_KP_Home, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_KP_End, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, 1);

  /* Activate */

  gtk_binding_entry_add_signal (binding_set, GDK_Return, 0,
				"activate", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KP_Enter, 0,
				"activate", 0);
  
  /* Deleting text */
  gtk_binding_entry_add_signal (binding_set, GDK_Delete, 0,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_CHARS,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KP_Delete, 0,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_CHARS,
				GTK_TYPE_INT, 1);
  
  gtk_binding_entry_add_signal (binding_set, GDK_d, GDK_CONTROL_MASK,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_CHARS,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, 0,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_CHARS,
				GTK_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_Delete, GDK_CONTROL_MASK,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KP_Delete, GDK_CONTROL_MASK,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				GTK_TYPE_INT, 1);
  
  gtk_binding_entry_add_signal (binding_set, GDK_d, GDK_MOD1_MASK,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, GDK_CONTROL_MASK,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				GTK_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_k, GDK_CONTROL_MASK,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_PARAGRAPH_ENDS,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_u, GDK_CONTROL_MASK,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_PARAGRAPHS,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_MOD1_MASK,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_WHITESPACE,
				GTK_TYPE_INT, 1);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Space, GDK_MOD1_MASK,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_WHITESPACE,
				GTK_TYPE_INT, 1);
  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_MOD1_MASK,
				"insert_at_cursor", 1,
				GTK_TYPE_STRING, " ");
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Space, GDK_MOD1_MASK,
				"insert_at_cursor", 1,
				GTK_TYPE_STRING, " ");
  
  gtk_binding_entry_add_signal (binding_set, GDK_backslash, GDK_MOD1_MASK,
				"delete_from_cursor", 2,
				GTK_TYPE_ENUM, GTK_DELETE_WHITESPACE,
				GTK_TYPE_INT, 1);
  
  /* Cut/copy/paste */

  gtk_binding_entry_add_signal (binding_set, GDK_x, GDK_CONTROL_MASK,
				"cut_clipboard", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_w, GDK_CONTROL_MASK,
				"cut_clipboard", 0);
  
  gtk_binding_entry_add_signal (binding_set, GDK_c, GDK_CONTROL_MASK,
				"copy_clipboard", 0);
  
  gtk_binding_entry_add_signal (binding_set, GDK_v, GDK_CONTROL_MASK,
				"paste_clipboard", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_y, GDK_CONTROL_MASK,
				"paste_clipboard", 0);

  /* Overwrite */
  gtk_binding_entry_add_signal (binding_set, GDK_Insert, 0,
				"toggle_overwrite", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Insert, 0,
				"toggle_overwrite", 0);
}

static void
gtk_entry_editable_init (GtkEditableClass *iface)
{
  iface->do_insert_text = gtk_entry_insert_text;
  iface->do_delete_text = gtk_entry_delete_text;
  iface->insert_text = gtk_entry_real_insert_text;
  iface->delete_text = gtk_entry_real_delete_text;
  iface->get_chars = gtk_entry_get_chars;
  iface->set_selection_bounds = gtk_entry_set_selection_bounds;
  iface->get_selection_bounds = gtk_entry_get_selection_bounds;
  iface->set_position = gtk_entry_real_set_position;
  iface->get_position = gtk_entry_get_position;
}

static void
gtk_entry_cell_editable_init (GtkCellEditableIface *iface)
{
  iface->start_editing = gtk_entry_start_editing;
}

static void
gtk_entry_set_property (GObject         *object,
                        guint            prop_id,
                        const GValue    *value,
                        GParamSpec      *pspec)
{
  GtkEntry *entry = GTK_ENTRY (object);

  switch (prop_id)
    {
    case PROP_EDITABLE:
      {
        gboolean new_value = g_value_get_boolean (value);

      	if (new_value != entry->editable)
	  {
	    entry->editable = new_value;
	    gtk_entry_queue_draw (entry);
	  }
      }
      break;

    case PROP_MAX_LENGTH:
      gtk_entry_set_max_length (entry, g_value_get_int (value));
      break;
      
    case PROP_VISIBILITY:
      gtk_entry_set_visibility (entry, g_value_get_boolean (value));
      break;

    case PROP_HAS_FRAME:
      gtk_entry_set_has_frame (entry, g_value_get_boolean (value));
      break;

    case PROP_INVISIBLE_CHAR:
      gtk_entry_set_invisible_char (entry, g_value_get_uint (value));
      break;

    case PROP_ACTIVATES_DEFAULT:
      gtk_entry_set_activates_default (entry, g_value_get_boolean (value));
      break;

    case PROP_WIDTH_CHARS:
      gtk_entry_set_width_chars (entry, g_value_get_int (value));
      break;

    case PROP_TEXT:
      gtk_entry_set_text (entry, g_value_get_string (value));
      break;

    case PROP_SCROLL_OFFSET:
    case PROP_CURSOR_POSITION:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_entry_get_property (GObject         *object,
                        guint            prop_id,
                        GValue          *value,
                        GParamSpec      *pspec)
{
  GtkEntry *entry = GTK_ENTRY (object);

  switch (prop_id)
    {
    case PROP_CURSOR_POSITION:
      g_value_set_int (value, entry->current_pos);
      break;
    case PROP_SELECTION_BOUND:
      g_value_set_int (value, entry->selection_bound);
      break;
    case PROP_EDITABLE:
      g_value_set_boolean (value, entry->editable);
      break;
    case PROP_MAX_LENGTH:
      g_value_set_int (value, entry->text_max_length); 
      break;
    case PROP_VISIBILITY:
      g_value_set_boolean (value, entry->visible);
      break;
    case PROP_HAS_FRAME:
      g_value_set_boolean (value, entry->has_frame);
      break;
    case PROP_INVISIBLE_CHAR:
      g_value_set_uint (value, entry->invisible_char);
      break;
    case PROP_ACTIVATES_DEFAULT:
      g_value_set_boolean (value, entry->activates_default);
      break;
    case PROP_WIDTH_CHARS:
      g_value_set_int (value, entry->width_chars);
      break;
    case PROP_SCROLL_OFFSET:
      g_value_set_int (value, entry->scroll_offset);
      break;
    case PROP_TEXT:
      g_value_set_string (value, gtk_entry_get_text (entry));
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_entry_init (GtkEntry *entry)
{
  GTK_WIDGET_SET_FLAGS (entry, GTK_CAN_FOCUS);

  entry->text_size = MIN_SIZE;
  entry->text = g_malloc (entry->text_size);
  entry->text[0] = '\0';

  entry->editable = TRUE;
  entry->visible = TRUE;
  entry->invisible_char = '*';
  entry->dnd_position = -1;
  entry->width_chars = -1;
  entry->is_cell_renderer = FALSE;
  entry->editing_canceled = FALSE;
  entry->has_frame = TRUE;

  gtk_drag_dest_set (GTK_WIDGET (entry),
                     GTK_DEST_DEFAULT_HIGHLIGHT,
                     target_table, G_N_ELEMENTS (target_table),
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);

  /* This object is completely private. No external entity can gain a reference
   * to it; so we create it here and destroy it in finalize().
   */
  entry->im_context = gtk_im_multicontext_new ();
  
  g_signal_connect (G_OBJECT (entry->im_context), "commit",
		    G_CALLBACK (gtk_entry_commit_cb), entry);
  g_signal_connect (G_OBJECT (entry->im_context), "preedit_changed",
		    G_CALLBACK (gtk_entry_preedit_changed_cb), entry);
  g_signal_connect (G_OBJECT (entry->im_context), "retrieve_surrounding",
		    G_CALLBACK (gtk_entry_retrieve_surrounding_cb), entry);
  g_signal_connect (G_OBJECT (entry->im_context), "delete_surrounding",
		    G_CALLBACK (gtk_entry_delete_surrounding_cb), entry);
}

static void
gtk_entry_finalize (GObject *object)
{
  GtkEntry *entry = GTK_ENTRY (object);

  if (entry->cached_layout)
    g_object_unref (G_OBJECT (entry->cached_layout));

  g_object_unref (G_OBJECT (entry->im_context));

  if (entry->blink_timeout)
    g_source_remove (entry->blink_timeout);

  if (entry->recompute_idle)
    g_source_remove (entry->recompute_idle);

  entry->text_size = 0;

  if (entry->text)
    g_free (entry->text);
  entry->text = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_entry_realize_cursor_gc (GtkEntry *entry)
{
  GdkColor *cursor_color;
  GdkColor red = {0, 0xffff, 0x0000, 0x0000};
  
  if (entry->cursor_gc)
    gdk_gc_unref (entry->cursor_gc);

  gtk_widget_style_get (GTK_WIDGET (entry), "cursor_color", &cursor_color, NULL);
      entry->cursor_gc = gdk_gc_new (entry->text_area);
  if (cursor_color)
    gdk_gc_set_rgb_fg_color (entry->cursor_gc, cursor_color);
  else
    gdk_gc_set_rgb_fg_color (entry->cursor_gc, &red);
}

static void
gtk_entry_realize (GtkWidget *widget)
{
  GtkEntry *entry;
  GtkEditable *editable;
  GdkWindowAttr attributes;
  gint attributes_mask;

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);

  attributes.window_type = GDK_WINDOW_CHILD;
  
  get_widget_window_size (entry, &attributes.x, &attributes.y, &attributes.width, &attributes.height);

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
			    GDK_LEAVE_NOTIFY_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, entry);

  get_text_area_size (entry, &attributes.x, &attributes.y, &attributes.width, &attributes.height);

  attributes.cursor = gdk_cursor_new (GDK_XTERM);
  attributes_mask |= GDK_WA_CURSOR;

  entry->text_area = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (entry->text_area, entry);

  gdk_cursor_destroy (attributes.cursor);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gtk_entry_realize_cursor_gc (entry);

  gdk_window_set_background (widget->window, &widget->style->base[GTK_WIDGET_STATE (widget)]);
  gdk_window_set_background (entry->text_area, &widget->style->base[GTK_WIDGET_STATE (widget)]);

  gdk_window_show (entry->text_area);

  gtk_im_context_set_client_window (entry->im_context, entry->text_area);

  gtk_entry_adjust_scroll (entry);
}

static void
gtk_entry_unrealize (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  gtk_im_context_set_client_window (entry->im_context, entry->text_area);
  
  if (entry->text_area)
    {
      gdk_window_set_user_data (entry->text_area, NULL);
      gdk_window_destroy (entry->text_area);
      entry->text_area = NULL;
    }

  if (entry->cursor_gc)
    {
      gdk_gc_unref (entry->cursor_gc);
      entry->cursor_gc = NULL;
    }

  if (entry->popup_menu)
    {
      gtk_widget_destroy (entry->popup_menu);
      entry->popup_menu = NULL;
    }

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
get_borders (GtkEntry *entry,
             gint     *xborder,
             gint     *yborder)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  gint focus_width;
  gboolean interior_focus;

  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			NULL);

  if (entry->has_frame)
    {
      *xborder = widget->style->xthickness;
      *yborder = widget->style->ythickness;
    }
  else
    {
      *xborder = 0;
      *yborder = 0;
    }

  if (!interior_focus)
    {
      *xborder += focus_width;
      *yborder += focus_width;
    }
}

static void
gtk_entry_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  PangoFontMetrics *metrics;
  gint xborder, yborder;
  PangoContext *context;
  
  context = gtk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (context,
				       widget->style->font_desc,
				       pango_context_get_language (context));

  entry->ascent = pango_font_metrics_get_ascent (metrics);
  entry->descent = pango_font_metrics_get_descent (metrics);

  get_borders (entry, &xborder, &yborder);
  
  xborder += INNER_BORDER;
  yborder += INNER_BORDER;
  
  if (entry->width_chars < 0)
    requisition->width = MIN_ENTRY_WIDTH + xborder * 2;
  else
    {
      gint char_width = pango_font_metrics_get_approximate_char_width (metrics);
      requisition->width = PANGO_PIXELS (char_width) * entry->width_chars + xborder * 2;
    }
    
  requisition->height = PANGO_PIXELS (entry->ascent + entry->descent) + yborder * 2;

  pango_font_metrics_unref (metrics);
}

static void
get_text_area_size (GtkEntry *entry,
                    gint     *x,
                    gint     *y,
                    gint     *width,
                    gint     *height)
{
  gint xborder, yborder;
  GtkRequisition requisition;
  GtkWidget *widget = GTK_WIDGET (entry);

  gtk_widget_get_child_requisition (widget, &requisition);

  get_borders (entry, &xborder, &yborder);

  if (x)
    *x = xborder;

  if (y)
    *y = yborder;
  
  if (width)
    *width = GTK_WIDGET (entry)->allocation.width - xborder * 2;

  if (height)
    *height = requisition.height - yborder * 2;
}

static void
get_widget_window_size (GtkEntry *entry,
                        gint     *x,
                        gint     *y,
                        gint     *width,
                        gint     *height)
{
  GtkRequisition requisition;
  GtkWidget *widget = GTK_WIDGET (entry);
      
  gtk_widget_get_child_requisition (widget, &requisition);

  if (x)
    *x = widget->allocation.x;

  if (y)
    {
      if (entry->is_cell_renderer)
	*y = widget->allocation.y;
      else
	*y = widget->allocation.y + (widget->allocation.height - requisition.height) / 2;
    }

  if (width)
    *width = widget->allocation.width;

  if (height)
    {
      if (entry->is_cell_renderer)
	*height = widget->allocation.height;
      else
	*height = requisition.height;
    }
}

static void
gtk_entry_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  
  widget->allocation = *allocation;
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      /* We call gtk_widget_get_child_requisition, since we want (for
       * backwards compatibility reasons) the realization here to
       * be affected by the usize of the entry, if set
       */
      gint x, y, width, height;

      get_widget_window_size (entry, &x, &y, &width, &height);
      
      gdk_window_move_resize (widget->window,
                              x, y, width, height);   

      get_text_area_size (entry, &x, &y, &width, &height);
      
      gdk_window_move_resize (entry->text_area,
                              x, y, width, height);

      gtk_entry_recompute (entry);
    }
}

static void
gtk_entry_draw_frame (GtkWidget *widget)
{
  gint x = 0, y = 0;
  gint width, height;
  gboolean interior_focus;
  gint focus_width;
  
  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			NULL);
  
  gdk_window_get_size (widget->window, &width, &height);
  
  if (GTK_WIDGET_HAS_FOCUS (widget) && !interior_focus)
    {
      gtk_paint_focus (widget->style, widget->window, GTK_WIDGET_STATE (widget), 
		       NULL, widget, "entry",
		       0, 0, width, height);
      
      x += focus_width;
      y += focus_width;
      width -= 2 * focus_width;
      height -= 2 * focus_width;
    }

  gtk_paint_shadow (widget->style, widget->window,
		    GTK_STATE_NORMAL, GTK_SHADOW_IN,
		    NULL, widget, "entry",
		    x, y, width, height);
}

static gint
gtk_entry_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  if (widget->window == event->window)
    gtk_entry_draw_frame (widget);
  else if (entry->text_area == event->window)
    {
      gtk_entry_draw_text (GTK_ENTRY (widget));

      if ((entry->visible || entry->invisible_char != 0) &&
	  GTK_WIDGET_HAS_FOCUS (widget) &&
	  entry->selection_bound == entry->current_pos && entry->cursor_visible)
	gtk_entry_draw_cursor (GTK_ENTRY (widget), CURSOR_STANDARD);

      if (entry->dnd_position != -1)
	gtk_entry_draw_cursor (GTK_ENTRY (widget), CURSOR_DND);
    }

  return FALSE;
}

static gint
gtk_entry_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEditable *editable = GTK_EDITABLE (widget);
  gint tmp_pos;
  gint sel_start, sel_end;

  if (event->window != entry->text_area ||
      (entry->button && event->button != entry->button))
    return FALSE;

  entry->button = event->button;
  
  if (!GTK_WIDGET_HAS_FOCUS (widget))
    {
      entry->in_click = TRUE;
      gtk_widget_grab_focus (widget);
      entry->in_click = FALSE;
    }
  
  tmp_pos = gtk_entry_find_position (entry, event->x + entry->scroll_offset);
    
  if (event->button == 1)
    {
      gboolean have_selection = gtk_editable_get_selection_bounds (editable, &sel_start, &sel_end);
      
      if (event->state & GDK_SHIFT_MASK)
	{
	  gtk_entry_reset_im_context (entry);
	  
	  if (!have_selection) /* select from the current position to the clicked position */
	    sel_start = sel_end = entry->current_pos;
	  
	  if (tmp_pos > sel_start && tmp_pos < sel_end)
	    {
	      /* Truncate current selection */
	      gtk_entry_set_positions (entry, tmp_pos, -1);
	    }
	  else
	    {
	      gboolean extend_to_left;
	      gint start, end;

	      /* Figure out what click selects and extend current selection */
	      switch (event->type)
		{
		case GDK_BUTTON_PRESS:
		  gtk_entry_set_positions (entry, tmp_pos, tmp_pos);
		  break;
		  
		case GDK_2BUTTON_PRESS:
		  gtk_entry_select_word (entry);
		  break;
		  
		case GDK_3BUTTON_PRESS:
		  gtk_entry_select_line (entry);
		  break;

		default:
		  break;
		}

	      start = MIN (entry->current_pos, entry->selection_bound);
	      start = MIN (sel_start, start);
	      
	      end = MAX (entry->current_pos, entry->selection_bound);
	      end = MAX (sel_end, end);

	      if (tmp_pos == sel_start || tmp_pos == sel_end)
		extend_to_left = (tmp_pos == start);
	      else
		extend_to_left = (end == sel_end);
	      
	      if (extend_to_left)
		gtk_entry_set_positions (entry, start, end);
	      else
		gtk_entry_set_positions (entry, end, start);
	    }
	}
      else /* no shift key */
	switch (event->type)
	{
	case GDK_BUTTON_PRESS:
	  if (have_selection && tmp_pos >= sel_start && tmp_pos <= sel_end)
	    {
	      /* Click inside the selection - we'll either start a drag, or
	       * clear the selection
	       */

	      entry->in_drag = TRUE;
	      entry->drag_start_x = event->x + entry->scroll_offset;
	      entry->drag_start_y = event->y + entry->scroll_offset;
	    }
	  else
	    gtk_editable_set_position (editable, tmp_pos);
	  
	  break;

 
	case GDK_2BUTTON_PRESS:
	  /* We ALWAYS receive a GDK_BUTTON_PRESS immediately before 
	   * receiving a GDK_2BUTTON_PRESS so we need to reset
 	   * entry->in_drag which may have been set above
           */
	  entry->in_drag = FALSE;
	  gtk_entry_select_word (entry);
	  break;
	
	case GDK_3BUTTON_PRESS:
	  /* We ALWAYS receive a GDK_BUTTON_PRESS immediately before
	   * receiving a GDK_3BUTTON_PRESS so we need to reset
	   * entry->in_drag which may have been set above
	   */
	  entry->in_drag = FALSE;
	  gtk_entry_select_line (entry);
	  break;

	default:
	  break;
	}

      return TRUE;
    }
  else if (event->button == 2 && event->type == GDK_BUTTON_PRESS && entry->editable)
    {
      gtk_editable_select_region (editable, tmp_pos, tmp_pos);
      gtk_entry_paste (entry, GDK_SELECTION_PRIMARY);

      return TRUE;
    }
  else if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
      gtk_entry_do_popup (entry, event);
      entry->button = 0;	/* Don't wait for release, since the menu will gtk_grab_add */

      return TRUE;
    }

  return FALSE;
}

static gint
gtk_entry_button_release (GtkWidget      *widget,
			  GdkEventButton *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  if (event->window != entry->text_area || entry->button != event->button)
    return FALSE;

  if (entry->in_drag)
    {
      gint tmp_pos = gtk_entry_find_position (entry, entry->drag_start_x);

      gtk_editable_set_position (GTK_EDITABLE (entry), tmp_pos);

      entry->in_drag = 0;
    }
  
  entry->button = 0;
  
  gtk_entry_update_primary_selection (entry);
	      
  return TRUE;
}

static gint
gtk_entry_motion_notify (GtkWidget      *widget,
			 GdkEventMotion *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  gint tmp_pos;

  if (event->window != entry->text_area || entry->button != 1)
    return FALSE;

  if (event->is_hint || (entry->text_area != event->window))
    gdk_window_get_pointer (entry->text_area, NULL, NULL, NULL);

  if (entry->in_drag)
    {
      if (gtk_drag_check_threshold (widget,
				    entry->drag_start_x, entry->drag_start_y,
				    event->x + entry->scroll_offset, event->y))
	{
	  GdkDragContext *context;
	  GtkTargetList *target_list = gtk_target_list_new (target_table, G_N_ELEMENTS (target_table));
	  guint actions = entry->editable ? GDK_ACTION_COPY | GDK_ACTION_MOVE : GDK_ACTION_COPY;
	  
	  context = gtk_drag_begin (widget, target_list, actions,
			  entry->button, (GdkEvent *)event);

	  
	  entry->in_drag = FALSE;
	  entry->button = 0;
	  
	  gtk_target_list_unref (target_list);
	  gtk_drag_set_icon_default (context);
	}
    }
  else
    {
      gint height;
      gdk_window_get_size (entry->text_area, NULL, &height);

      if (event->y < 0)
	tmp_pos = 0;
      else if (event->y >= height)
	tmp_pos = entry->text_length;
      else
	tmp_pos = gtk_entry_find_position (entry, event->x + entry->scroll_offset);
      
      gtk_entry_set_positions (entry, tmp_pos, -1);
    }
      
  return TRUE;
}

static gint
gtk_entry_key_press (GtkWidget   *widget,
		     GdkEventKey *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  if (!entry->editable)
    return FALSE;

  gtk_entry_pend_cursor_blink (entry);
  
  if (gtk_im_context_filter_keypress (entry->im_context, event))
    {
      entry->need_im_reset = TRUE;
      return TRUE;
    }
  else if (GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event))
    /* Activate key bindings
     */
    return TRUE;

  return FALSE;
}

static gint
gtk_entry_key_release (GtkWidget   *widget,
		       GdkEventKey *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  if (!entry->editable)
    return FALSE;

  if (gtk_im_context_filter_keypress (entry->im_context, event))
    {
      entry->need_im_reset = TRUE;
      return TRUE;
    }
  else
    return GTK_WIDGET_CLASS (parent_class)->key_release_event (widget, event);
}

static gint
gtk_entry_focus_in (GtkWidget     *widget,
		    GdkEventFocus *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_queue_draw (widget);
  
  entry->need_im_reset = TRUE;
  gtk_im_context_focus_in (entry->im_context);

  g_signal_connect (gdk_keymap_get_default (),
		    "direction_changed",
		    G_CALLBACK (gtk_entry_keymap_direction_changed), entry);

  gtk_entry_check_cursor_blink (entry);

  return FALSE;
}

static gint
gtk_entry_focus_out (GtkWidget     *widget,
		     GdkEventFocus *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_queue_draw (widget);

  entry->need_im_reset = TRUE;
  gtk_im_context_focus_out (entry->im_context);

  gtk_entry_check_cursor_blink (entry);
  
  g_signal_handlers_disconnect_by_func (gdk_keymap_get_default (),
                                        gtk_entry_keymap_direction_changed,
                                        entry);
  
  return FALSE;
}

static void
gtk_entry_grab_focus (GtkWidget        *widget)
{
   GTK_WIDGET_CLASS (parent_class)->grab_focus (widget);

  if (!GTK_ENTRY (widget)->in_click)
    gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
}

static void 
gtk_entry_direction_changed (GtkWidget        *widget,
			     GtkTextDirection  previous_dir)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  gtk_entry_recompute (entry);
      
  GTK_WIDGET_CLASS (parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_entry_state_changed (GtkWidget      *widget,
			 GtkStateType    previous_state)
{
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_set_background (widget->window, &widget->style->base[GTK_WIDGET_STATE (widget)]);
      gdk_window_set_background (GTK_ENTRY (widget)->text_area, &widget->style->base[GTK_WIDGET_STATE (widget)]);
    }

  gtk_widget_queue_clear (widget);
}

/* GtkEditable method implementations
 */
static void
gtk_entry_insert_text (GtkEditable *editable,
		       const gchar *new_text,
		       gint         new_text_length,
		       gint        *position)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  gchar buf[64];
  gchar *text;

  if (*position < 0 || *position > entry->text_length)
    *position = entry->text_length;
  
  g_object_ref (G_OBJECT (editable));
  
  if (new_text_length <= 63)
    text = buf;
  else
    text = g_new (gchar, new_text_length + 1);

  text[new_text_length] = '\0';
  strncpy (text, new_text, new_text_length);
  
  g_signal_emit_by_name (editable, "insert_text", text, new_text_length, position);
  g_signal_emit_by_name (editable, "changed");
  g_object_notify (G_OBJECT (editable), "text");

  if (new_text_length > 63)
    g_free (text);

  g_object_unref (G_OBJECT (editable));
}

static void
gtk_entry_delete_text (GtkEditable *editable,
		       gint         start_pos,
		       gint         end_pos)
{
  GtkEntry *entry = GTK_ENTRY (editable);

  if (end_pos < 0 || end_pos > entry->text_length)
    end_pos = entry->text_length;
  if (start_pos < 0)
    start_pos = 0;
  if (start_pos > end_pos)
    start_pos = end_pos;
  
  g_object_ref (G_OBJECT (editable));

  g_signal_emit_by_name (editable, "delete_text", start_pos, end_pos);
  g_signal_emit_by_name (editable, "changed");
  g_object_notify (G_OBJECT (editable), "text");

  g_object_unref (G_OBJECT (editable));
}

static gchar *    
gtk_entry_get_chars      (GtkEditable   *editable,
			  gint           start_pos,
			  gint           end_pos)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  gint start_index, end_index;
  
  if (end_pos < 0)
    end_pos = entry->text_length;

  start_pos = MIN (entry->text_length, start_pos);
  end_pos = MIN (entry->text_length, end_pos);

  start_index = g_utf8_offset_to_pointer (entry->text, start_pos) - entry->text;
  end_index = g_utf8_offset_to_pointer (entry->text, end_pos) - entry->text;

  return g_strndup (entry->text + start_index, end_index - start_index);
}

static void
gtk_entry_real_set_position (GtkEditable *editable,
			     gint         position)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  
  if (position < 0 || position > entry->text_length)
    position = entry->text_length;

  if (position != entry->current_pos ||
      position != entry->selection_bound)
    {
      gtk_entry_reset_im_context (entry);
      gtk_entry_set_positions (entry, position, position);
    }
}

static gint
gtk_entry_get_position (GtkEditable *editable)
{
  return GTK_ENTRY (editable)->current_pos;
}

static void
gtk_entry_set_selection_bounds (GtkEditable *editable,
				gint         start,
				gint         end)
{
  GtkEntry *entry = GTK_ENTRY (editable);

  if (start < 0)
    start = entry->text_length;
  if (end < 0)
    end = entry->text_length;
  
  gtk_entry_reset_im_context (entry);

  gtk_entry_set_positions (entry,
			   MIN (end, entry->text_length),
			   MIN (start, entry->text_length));

  gtk_entry_update_primary_selection (entry);
}

static gboolean
gtk_entry_get_selection_bounds (GtkEditable *editable,
				gint        *start,
				gint        *end)
{
  GtkEntry *entry = GTK_ENTRY (editable);

  *start = entry->selection_bound;
  *end = entry->current_pos;

  return (entry->selection_bound != entry->current_pos);
}

static void 
gtk_entry_style_set	(GtkWidget      *widget,
			 GtkStyle       *previous_style)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  if (previous_style && GTK_WIDGET_REALIZED (widget))
    {
      gtk_entry_recompute (entry);

      gdk_window_set_background (widget->window, &widget->style->base[GTK_WIDGET_STATE (widget)]);
      gdk_window_set_background (entry->text_area, &widget->style->base[GTK_WIDGET_STATE (widget)]);

      gtk_entry_realize_cursor_gc (entry);
    }
}

/* GtkCellEditable method implementations
 */
static void
gtk_cell_editable_entry_activated (GtkEntry *entry, gpointer data)
{
  gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
  gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));
}

static gboolean
gtk_cell_editable_key_press_event (GtkEntry    *entry,
				   GdkEventKey *key_event,
				   gpointer     data)
{
    if (key_event->keyval == GDK_Escape)
      {
	entry->editing_canceled = TRUE;
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));

	return TRUE;
      }

    return FALSE;
}

static void
gtk_entry_start_editing (GtkCellEditable *cell_editable,
			 GdkEvent        *event)
{
  GTK_ENTRY (cell_editable)->is_cell_renderer = TRUE;

  g_signal_connect (G_OBJECT (cell_editable), "activate",
		    G_CALLBACK (gtk_cell_editable_entry_activated), NULL);
  g_signal_connect (G_OBJECT (cell_editable), "key_press_event",
		    G_CALLBACK (gtk_cell_editable_key_press_event), NULL);
}

/* Default signal handlers
 */
static void
gtk_entry_real_insert_text (GtkEditable *editable,
			    const gchar *new_text,
			    gint         new_text_length,
			    gint        *position)
{
  gint index;
  gint n_chars;

  GtkEntry *entry = GTK_ENTRY (editable);

  if (new_text_length < 0)
    new_text_length = strlen (new_text);

  n_chars = g_utf8_strlen (new_text, new_text_length);
  if (entry->text_max_length > 0 && n_chars + entry->text_length > entry->text_max_length)
    {
      gdk_beep ();
      n_chars = entry->text_max_length - entry->text_length;
      new_text_length = g_utf8_offset_to_pointer (new_text, n_chars) - new_text;
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
		  new_text_length = entry->text_size - entry->n_bytes - 1;
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
  
  if (entry->current_pos > *position)
    entry->current_pos += n_chars;
  
  if (entry->selection_bound > *position)
    entry->selection_bound += n_chars;

  *position += n_chars;

  gtk_entry_recompute (entry);
}

static void
gtk_entry_real_delete_text (GtkEditable *editable,
			    gint         start_pos,
			    gint         end_pos)
{
  GtkEntry *entry = GTK_ENTRY (editable);

  if (start_pos < 0)
    start_pos = 0;
  if (end_pos < 0 || end_pos > entry->text_length)
    end_pos = entry->text_length;
  
  if (start_pos < end_pos)
    {
      gint start_index = g_utf8_offset_to_pointer (entry->text, start_pos) - entry->text;
      gint end_index = g_utf8_offset_to_pointer (entry->text, end_pos) - entry->text;

      g_memmove (entry->text + start_index, entry->text + end_index, entry->n_bytes + 1 - end_index);
      entry->text_length -= (end_pos - start_pos);
      entry->n_bytes -= (end_index - start_index);
      
      if (entry->current_pos > start_pos)
	entry->current_pos -= MIN (entry->current_pos, end_pos) - start_pos;

      if (entry->selection_bound > start_pos)
	entry->selection_bound -= MIN (entry->selection_bound, end_pos) - start_pos;
    }

  /* We might have deleted the selection
   */
  gtk_entry_update_primary_selection (entry);

  gtk_entry_recompute (entry);
}

/* Compute the X position for an offset that corresponds to the "more important
 * cursor position for that offset. We use this when trying to guess to which
 * end of the selection we should go to when the user hits the left or
 * right arrow key.
 */
static gint
get_better_cursor_x (GtkEntry *entry,
		     gint      offset)
{
  GtkTextDirection keymap_direction =
    (gdk_keymap_get_direction (gdk_keymap_get_default ()) == PANGO_DIRECTION_LTR) ?
    GTK_TEXT_DIR_LTR : GTK_TEXT_DIR_RTL;
  GtkTextDirection widget_direction = gtk_widget_get_direction (GTK_WIDGET (entry));
  gboolean split_cursor;
  
  PangoLayout *layout = gtk_entry_ensure_layout (entry, TRUE);
  gint index = g_utf8_offset_to_pointer (entry->text, offset) - entry->text;
  
  PangoRectangle strong_pos, weak_pos;
  
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (entry)),
		"gtk-split-cursor", &split_cursor,
		NULL);

  pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);

  if (split_cursor)
    return strong_pos.x / PANGO_SCALE;
  else
    return (keymap_direction == widget_direction) ? strong_pos.x / PANGO_SCALE : weak_pos.x / PANGO_SCALE;
}

static void
gtk_entry_move_cursor (GtkEntry       *entry,
		       GtkMovementStep step,
		       gint            count,
		       gboolean        extend_selection)
{
  gint new_pos = entry->current_pos;

  gtk_entry_reset_im_context (entry);

  if (entry->current_pos != entry->selection_bound && !extend_selection)
    {
      /* If we have a current selection and aren't extending it, move to the
       * start/or end of the selection as appropriate
       */
      switch (step)
	{
	case GTK_MOVEMENT_VISUAL_POSITIONS:
	  {
	    gint current_x = get_better_cursor_x (entry, entry->current_pos);
	    gint bound_x = get_better_cursor_x (entry, entry->selection_bound);

	    if (count < 0)
	      new_pos = current_x < bound_x ? entry->current_pos : entry->selection_bound;
	    else
	      new_pos = current_x > bound_x ? entry->current_pos : entry->selection_bound;

	    break;
	  }
	case GTK_MOVEMENT_LOGICAL_POSITIONS:
	case GTK_MOVEMENT_WORDS:
	  if (count < 0)
	    new_pos = MIN (entry->current_pos, entry->selection_bound);
	  else
	    new_pos = MAX (entry->current_pos, entry->selection_bound);
	  break;
	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
	case GTK_MOVEMENT_BUFFER_ENDS:
	  new_pos = count < 0 ? 0 : entry->text_length;
	  break;
	case GTK_MOVEMENT_DISPLAY_LINES:
	case GTK_MOVEMENT_PARAGRAPHS:
	case GTK_MOVEMENT_PAGES:
	  break;
	}
    }
  else
    {
      switch (step)
	{
	case GTK_MOVEMENT_LOGICAL_POSITIONS:
	  new_pos = gtk_entry_move_logically (entry, new_pos, count);
	  break;
	case GTK_MOVEMENT_VISUAL_POSITIONS:
	  new_pos = gtk_entry_move_visually (entry, new_pos, count);
	  break;
	case GTK_MOVEMENT_WORDS:
	  while (count > 0)
	    {
	      new_pos = gtk_entry_move_forward_word (entry, new_pos);
	      count--;
	    }
	  while (count < 0)
	    {
	      new_pos = gtk_entry_move_backward_word (entry, new_pos);
	      count++;
	    }
	  break;
	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
	case GTK_MOVEMENT_BUFFER_ENDS:
	  new_pos = count < 0 ? 0 : entry->text_length;
	  break;
	case GTK_MOVEMENT_DISPLAY_LINES:
	case GTK_MOVEMENT_PARAGRAPHS:
	case GTK_MOVEMENT_PAGES:
	  break;
	}
    }

  if (extend_selection)
    gtk_editable_select_region (GTK_EDITABLE (entry), entry->selection_bound, new_pos);
  else
    gtk_editable_set_position (GTK_EDITABLE (entry), new_pos);
  
  gtk_entry_pend_cursor_blink (entry);
}

static void
gtk_entry_insert_at_cursor (GtkEntry    *entry,
			    const gchar *str)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint pos = entry->current_pos;

  gtk_entry_reset_im_context (entry);

  gtk_editable_insert_text (editable, str, -1, &pos);
  gtk_editable_set_position (editable, pos);
}

static void
gtk_entry_delete_from_cursor (GtkEntry       *entry,
			      GtkDeleteType   type,
			      gint            count)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint start_pos = entry->current_pos;
  gint end_pos = entry->current_pos;
  
  gtk_entry_reset_im_context (entry);

  if (!entry->editable)
    return;

  if (entry->selection_bound != entry->current_pos)
    {
      gtk_editable_delete_selection (editable);
      return;
    }
  
  switch (type)
    {
    case GTK_DELETE_CHARS:
      end_pos = gtk_entry_move_logically (entry, entry->current_pos, count);
      gtk_editable_delete_text (editable, MIN (start_pos, end_pos), MAX (start_pos, end_pos));
      break;
    case GTK_DELETE_WORDS:
      if (count < 0)
	{
	  /* Move to end of current word, or if not on a word, end of previous word */
	  end_pos = gtk_entry_move_backward_word (entry, end_pos);
	  end_pos = gtk_entry_move_forward_word (entry, end_pos);
	}
      else if (count > 0)
	{
	  /* Move to beginning of current word, or if not on a word, begining of next word */
	  start_pos = gtk_entry_move_forward_word (entry, start_pos);
	  start_pos = gtk_entry_move_backward_word (entry, start_pos);
	}
	
      /* Fall through */
    case GTK_DELETE_WORD_ENDS:
      while (count < 0)
	{
	  start_pos = gtk_entry_move_backward_word (entry, start_pos);
	  count++;
	}
      while (count > 0)
	{
	  end_pos = gtk_entry_move_forward_word (entry, end_pos);
	  count--;
	}
      gtk_editable_delete_text (editable, start_pos, end_pos);
      break;
    case GTK_DELETE_DISPLAY_LINE_ENDS:
    case GTK_DELETE_PARAGRAPH_ENDS:
      if (count < 0)
	gtk_editable_delete_text (editable, 0, entry->current_pos);
      else
	gtk_editable_delete_text (editable, entry->current_pos, -1);
      break;
    case GTK_DELETE_DISPLAY_LINES:
    case GTK_DELETE_PARAGRAPHS:
      gtk_editable_delete_text (editable, 0, -1);  
      break;
    case GTK_DELETE_WHITESPACE:
      gtk_entry_delete_whitespace (entry);
      break;
    }
  
  gtk_entry_pend_cursor_blink (entry);
}

static void
gtk_entry_copy_clipboard (GtkEntry *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint start, end;

  if (gtk_editable_get_selection_bounds (editable, &start, &end))
    {
      gchar *str = gtk_entry_get_public_chars (entry, start, end);
      gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), str, -1);
      g_free (str);
    }
}

static void
gtk_entry_cut_clipboard (GtkEntry *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint start, end;

  gtk_entry_copy_clipboard (entry);
  if (gtk_editable_get_selection_bounds (editable, &start, &end))
    gtk_editable_delete_text (editable, start, end);
}

static void
gtk_entry_paste_clipboard (GtkEntry *entry)
{
  gtk_entry_paste (entry, GDK_NONE);
}

static void
gtk_entry_toggle_overwrite (GtkEntry *entry)
{
  entry->overwrite_mode = !entry->overwrite_mode;
}

static void
gtk_entry_select_all (GtkEntry *entry)
{
  gtk_entry_select_line (entry);
}

static void
gtk_entry_real_activate (GtkEntry *entry)
{
  GtkWindow *window;
  GtkWidget *toplevel;
  GtkWidget *widget;

  widget = GTK_WIDGET (entry);

  if (entry->activates_default)
    {
      toplevel = gtk_widget_get_toplevel (widget);
      if (GTK_IS_WINDOW (toplevel))
	{
	  window = GTK_WINDOW (toplevel);
      
	  if (window &&
	      widget != window->default_widget &&
	      !(!window->default_widget && widget == window->focus_widget))
	    gtk_window_activate_default (window);
	}
    }
}

static void
gtk_entry_keymap_direction_changed (GdkKeymap *keymap,
				    GtkEntry  *entry)
{
  gtk_entry_queue_draw (entry);
}

/* IM Context Callbacks
 */

static void
gtk_entry_commit_cb (GtkIMContext *context,
		     const gchar  *str,
		     GtkEntry     *entry)
{
  gtk_entry_enter_text (entry, str);
}

static void 
gtk_entry_preedit_changed_cb (GtkIMContext *context,
			      GtkEntry     *entry)
{
  gchar *preedit_string;
  gint cursor_pos;
  
  gtk_im_context_get_preedit_string (entry->im_context,
				     &preedit_string, NULL,
				     &cursor_pos);
  entry->preedit_length = strlen (preedit_string);
  cursor_pos = CLAMP (cursor_pos, 0, g_utf8_strlen (preedit_string, -1));
  entry->preedit_cursor = cursor_pos;
  g_free (preedit_string);

  gtk_entry_recompute (entry);
}

static gboolean
gtk_entry_retrieve_surrounding_cb (GtkIMContext *context,
			       GtkEntry     *entry)
{
  gtk_im_context_set_surrounding (context,
				  entry->text,
				  entry->n_bytes,
				  g_utf8_offset_to_pointer (entry->text, entry->current_pos) - entry->text);

  return TRUE;
}

static gboolean
gtk_entry_delete_surrounding_cb (GtkIMContext *slave,
				 gint          offset,
				 gint          n_chars,
				 GtkEntry     *entry)
{
  gtk_editable_delete_text (GTK_EDITABLE (entry),
			    entry->current_pos + offset,
			    entry->current_pos + offset + n_chars);

  return TRUE;
}

/* Internal functions
 */

/* Used for im_commit_cb and inserting Unicode chars */
static void
gtk_entry_enter_text (GtkEntry       *entry,
                      const gchar    *str)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint tmp_pos = entry->current_pos;

  if (gtk_editable_get_selection_bounds (editable, NULL, NULL))
    gtk_editable_delete_selection (editable);
  else
    {
      if (entry->overwrite_mode)
        gtk_entry_delete_from_cursor (entry, GTK_DELETE_CHARS, 1);
    }

  gtk_editable_insert_text (editable, str, strlen (str), &tmp_pos);
  gtk_editable_set_position (editable, tmp_pos);
}

/* All changes to entry->current_pos and entry->selection_bound
 * should go through this function.
 */
static void
gtk_entry_set_positions (GtkEntry *entry,
			 gint      current_pos,
			 gint      selection_bound)
{
  gboolean changed = FALSE;

  g_object_freeze_notify (G_OBJECT (entry));
  
  if (current_pos != -1 &&
      entry->current_pos != current_pos)
    {
      entry->current_pos = current_pos;
      changed = TRUE;

      g_object_notify (G_OBJECT (entry), "cursor_position");
    }

  if (selection_bound != -1 &&
      entry->selection_bound != selection_bound)
    {
      entry->selection_bound = selection_bound;
      changed = TRUE;
      
      g_object_notify (G_OBJECT (entry), "selection_bound");
    }

  g_object_thaw_notify (G_OBJECT (entry));

  if (changed)
    gtk_entry_recompute (entry);
}

static void
gtk_entry_reset_layout (GtkEntry *entry)
{
  if (entry->cached_layout)
    {
      g_object_unref (G_OBJECT (entry->cached_layout));
      entry->cached_layout = NULL;
    }
}

static void
update_im_cursor_location (GtkEntry *entry)
{
  GdkRectangle area;
  gint strong_x;
  gint strong_xoffset;
  gint x, y, area_width, area_height;

  gtk_entry_get_cursor_locations (entry, CURSOR_STANDARD, &strong_x, NULL)
;
  get_text_area_size (entry, &x, &y, &area_width, &area_height);

  strong_xoffset = strong_x - entry->scroll_offset;
  if (strong_xoffset < 0)
    {
      strong_xoffset = 0;
    }
  else if (strong_xoffset > area_width)
    {
      strong_xoffset = area_width;
    }
  area.x = x + strong_xoffset;
  area.y = y + area_height;
  area.width = area_width;
  area.height = area_height;

  gtk_im_context_set_cursor_location (entry->im_context, &area);
}

static gboolean
recompute_idle_func (gpointer data)
{
  GtkEntry *entry;

  GDK_THREADS_ENTER ();

  entry = GTK_ENTRY (data);

  gtk_entry_adjust_scroll (entry);
  gtk_entry_queue_draw (entry);

  entry->recompute_idle = FALSE;
  
  update_im_cursor_location (entry);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
gtk_entry_recompute (GtkEntry *entry)
{
  gtk_entry_reset_layout (entry);
  gtk_entry_check_cursor_blink (entry);
  
  if (!entry->recompute_idle)
    {
      entry->recompute_idle = g_idle_add_full (G_PRIORITY_HIGH_IDLE + 15, /* between resize and redraw */
					       recompute_idle_func, entry, NULL); 
    }
}

static void
append_char (GString *str,
             gunichar ch,
             gint     count)
{
  gint i;
  gint char_len;
  gchar buf[7];
  
  char_len = g_unichar_to_utf8 (ch, buf);
  
  i = 0;
  while (i < count)
    {
      g_string_append_len (str, buf, char_len);
      ++i;
    }
}
     
static PangoLayout *
gtk_entry_create_layout (GtkEntry *entry,
			 gboolean  include_preedit)
{
  PangoLayout *layout = gtk_widget_create_pango_layout (GTK_WIDGET (entry), NULL);
  PangoAttrList *tmp_attrs = pango_attr_list_new ();
  
  gchar *preedit_string = NULL;
  gint preedit_length = 0;
  PangoAttrList *preedit_attrs = NULL;

  pango_layout_set_single_paragraph_mode (layout, TRUE);
  
  if (include_preedit)
    {
      gtk_im_context_get_preedit_string (entry->im_context,
					 &preedit_string, &preedit_attrs, NULL);
      preedit_length = entry->preedit_length;
    }

  if (preedit_length)
    {
      GString *tmp_string = g_string_new (NULL);
      
      gint cursor_index = g_utf8_offset_to_pointer (entry->text, entry->current_pos) - entry->text;
      
      if (entry->visible)
        {
          g_string_prepend_len (tmp_string, entry->text, entry->n_bytes);
          g_string_insert (tmp_string, cursor_index, preedit_string);
        }
      else
        {
          gint ch_len;
          gint preedit_len_chars;
          gunichar invisible_char;
          
          ch_len = g_utf8_strlen (entry->text, entry->n_bytes);
          preedit_len_chars = g_utf8_strlen (preedit_string, -1);
          ch_len += preedit_len_chars;

          if (entry->invisible_char != 0)
            invisible_char = entry->invisible_char;
          else
            invisible_char = ' '; /* just pick a char */
          
          append_char (tmp_string, invisible_char, ch_len);
          
          /* Fix cursor index to point to invisible char corresponding
           * to the preedit, fix preedit_length to be the length of
           * the invisible chars representing the preedit
           */
          cursor_index =
            g_utf8_offset_to_pointer (tmp_string->str, entry->current_pos) -
            tmp_string->str;
          preedit_length =
            preedit_len_chars *
            g_unichar_to_utf8 (invisible_char, NULL);
        }
      
      pango_layout_set_text (layout, tmp_string->str, tmp_string->len);
      
      pango_attr_list_splice (tmp_attrs, preedit_attrs,
			      cursor_index, preedit_length);
      
      g_string_free (tmp_string, TRUE);
    }
  else
    {
      if (entry->visible)
        {
          pango_layout_set_text (layout, entry->text, entry->n_bytes);
        }
      else
        {
          GString *str = g_string_new (NULL);
          gunichar invisible_char;
          
          if (entry->invisible_char != 0)
            invisible_char = entry->invisible_char;
          else
            invisible_char = ' '; /* just pick a char */
          
          append_char (str, invisible_char, entry->text_length);
          pango_layout_set_text (layout, str->str, str->len);
          g_string_free (str, TRUE);
        }
    }
      
  pango_layout_set_attributes (layout, tmp_attrs);

  if (preedit_string)
    g_free (preedit_string);
  if (preedit_attrs)
    pango_attr_list_unref (preedit_attrs);
      
  pango_attr_list_unref (tmp_attrs);

  return layout;
}

static PangoLayout *
gtk_entry_ensure_layout (GtkEntry *entry,
                         gboolean  include_preedit)
{
  if (entry->preedit_length > 0 &&
      !include_preedit != !entry->cache_includes_preedit)
    gtk_entry_reset_layout (entry);

  if (!entry->cached_layout)
    {
      entry->cached_layout = gtk_entry_create_layout (entry, include_preedit);
      entry->cache_includes_preedit = include_preedit;
    }
  
  return entry->cached_layout;
}

static void
get_layout_position (GtkEntry *entry,
                     gint     *x,
                     gint     *y)
{
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gint area_width, area_height;
  gint y_pos;
  PangoLayoutLine *line;
  
  layout = gtk_entry_ensure_layout (entry, TRUE);

  get_text_area_size (entry, NULL, NULL, &area_width, &area_height);      
      
  area_height = PANGO_SCALE * (area_height - 2 * INNER_BORDER);
  
  line = pango_layout_get_lines (layout)->data;
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

  if (x)
    *x = INNER_BORDER - entry->scroll_offset;

  if (y)
    *y = y_pos;
}

static void
gtk_entry_draw_text (GtkEntry *entry)
{
  GtkWidget *widget;
  PangoLayoutLine *line;
  
  if (!entry->visible && entry->invisible_char == 0)
    return;
  
  if (GTK_WIDGET_DRAWABLE (entry))
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, TRUE);
      gint area_width, area_height;
      gint x, y;
      gint start_pos, end_pos;
      
      widget = GTK_WIDGET (entry);
      
      get_layout_position (entry, &x, &y);

      get_text_area_size (entry, NULL, NULL, &area_width, &area_height);

      gtk_paint_flat_box (widget->style, entry->text_area, 
			  GTK_WIDGET_STATE(widget), GTK_SHADOW_NONE,
			  NULL, widget, "entry_bg", 
			  0, 0, area_width, area_height);

      gdk_draw_layout (entry->text_area, widget->style->text_gc [widget->state],       
                       x, y,
		       layout);
      
      if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start_pos, &end_pos))
	{
	  gint *ranges;
	  gint n_ranges, i;
          PangoRectangle logical_rect;
	  gint start_index = g_utf8_offset_to_pointer (entry->text, start_pos) - entry->text;
	  gint end_index = g_utf8_offset_to_pointer (entry->text, end_pos) - entry->text;
	  GdkRegion *clip_region = gdk_region_new ();
	  GdkGC *text_gc;
	  GdkGC *selection_gc;

          line = pango_layout_get_lines (layout)->data;
          
	  pango_layout_line_get_x_ranges (line, start_index, end_index, &ranges, &n_ranges);

          pango_layout_get_extents (layout, NULL, &logical_rect);
          
	  if (GTK_WIDGET_HAS_FOCUS (entry))
	    {
	      selection_gc = widget->style->base_gc [GTK_STATE_SELECTED];
	      text_gc = widget->style->text_gc [GTK_STATE_SELECTED];
	    }
	  else
	    {
	      selection_gc = widget->style->base_gc [GTK_STATE_ACTIVE];
	      text_gc = widget->style->text_gc [GTK_STATE_ACTIVE];
	    }
	  
	  for (i=0; i < n_ranges; i++)
	    {
	      GdkRectangle rect;

	      rect.x = INNER_BORDER - entry->scroll_offset + ranges[2*i] / PANGO_SCALE;
	      rect.y = y;
	      rect.width = (ranges[2*i + 1] - ranges[2*i]) / PANGO_SCALE;
	      rect.height = logical_rect.height / PANGO_SCALE;
		
	      gdk_draw_rectangle (entry->text_area, selection_gc, TRUE,
				  rect.x, rect.y, rect.width, rect.height);

	      gdk_region_union_with_rect (clip_region, &rect);
	    }

	  gdk_gc_set_clip_region (text_gc, clip_region);
	  gdk_draw_layout (entry->text_area, text_gc, 
			   x, y,
			   layout);
	  gdk_gc_set_clip_region (text_gc, NULL);
	  
	  gdk_region_destroy (clip_region);
	  g_free (ranges);
	}
    }
}

static void
gtk_entry_draw_cursor (GtkEntry  *entry,
		       CursorType type)
{
  GtkTextDirection keymap_direction =
    (gdk_keymap_get_direction (gdk_keymap_get_default ()) == PANGO_DIRECTION_LTR) ?
    GTK_TEXT_DIR_LTR : GTK_TEXT_DIR_RTL;
  GtkTextDirection widget_direction = gtk_widget_get_direction (GTK_WIDGET (entry));
  
  if (GTK_WIDGET_DRAWABLE (entry))
    {
      GtkWidget *widget = GTK_WIDGET (entry);
      GdkRectangle cursor_location;
      gboolean split_cursor;

      gint xoffset = INNER_BORDER - entry->scroll_offset;
      gint strong_x, weak_x;
      gint text_area_height;
      GdkGC *gc1 = NULL;
      GdkGC *gc2 = NULL;
      GtkTextDirection dir1 = GTK_TEXT_DIR_NONE;
      GtkTextDirection dir2 = GTK_TEXT_DIR_NONE;
      gint x1 = 0;
      gint x2 = 0;

      gdk_window_get_size (entry->text_area, NULL, &text_area_height);
      
      gtk_entry_get_cursor_locations (entry, type, &strong_x, &weak_x);

      g_object_get (gtk_widget_get_settings (widget),
		    "gtk-split-cursor", &split_cursor,
		    NULL);

      if (split_cursor)
	{
	  gc1 = entry->cursor_gc;
	  x1 = strong_x;

	  if (weak_x != strong_x)
	    {
	      dir1 = widget_direction;
	      dir2 = (widget_direction == GTK_TEXT_DIR_LTR) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR;
	      
	      gc2 = widget->style->text_gc[GTK_STATE_NORMAL];
	      x2 = weak_x;
	    }
	}
      else
	{
	  gc1 = entry->cursor_gc;
	  
	  if (keymap_direction == widget_direction)
	    x1 = strong_x;
	  else
	    x1 = weak_x;
	}

      cursor_location.x = xoffset + x1;
      cursor_location.y = INNER_BORDER;
      cursor_location.width = 0;
      cursor_location.height = text_area_height - 2 * INNER_BORDER ;
      
      _gtk_draw_insertion_cursor (entry->text_area, gc1,
				  &cursor_location, dir1);
      
      if (gc2)
	{
	  cursor_location.x = xoffset + x2;
	  _gtk_draw_insertion_cursor (entry->text_area, gc2,
				      &cursor_location, dir2);
	}
    }
}

static void
gtk_entry_queue_draw (GtkEntry *entry)
{
  if (GTK_WIDGET_REALIZED (entry))
    gdk_window_invalidate_rect (entry->text_area, NULL, FALSE);
}

static void
gtk_entry_reset_im_context (GtkEntry *entry)
{
  if (entry->need_im_reset)
    {
      entry->need_im_reset = 0;
      gtk_im_context_reset (entry->im_context);
    }
}

static gint
gtk_entry_find_position (GtkEntry *entry,
			 gint      x)
{
  PangoLayout *layout;
  PangoLayoutLine *line;
  gint index;
  gint pos;
  gboolean trailing;
  gint cursor_index = g_utf8_offset_to_pointer (entry->text, entry->current_pos) - entry->text;
  
  layout = gtk_entry_ensure_layout (entry, TRUE);
  
  line = pango_layout_get_lines (layout)->data;
  pango_layout_line_x_to_index (line, x * PANGO_SCALE, &index, &trailing);

  if (index >= cursor_index && entry->preedit_length)
    {
      if (index >= cursor_index + entry->preedit_length)
	index -= entry->preedit_length;
      else
	{
	  index = cursor_index;
	  trailing = 0;
	}
    }

  pos = g_utf8_pointer_to_offset (entry->text, entry->text + index);
  pos += trailing;

  return pos;
}

static void
gtk_entry_get_cursor_locations (GtkEntry   *entry,
				CursorType  type,
				gint       *strong_x,
				gint       *weak_x)
{
  PangoLayout *layout = gtk_entry_ensure_layout (entry, TRUE);
  const gchar *text;
  PangoRectangle strong_pos, weak_pos;
  gint index;
  
  if (type == CURSOR_STANDARD)
    {
      text = pango_layout_get_text (layout);
      index = g_utf8_offset_to_pointer (text, entry->current_pos + entry->preedit_cursor) - text;
    }
  else /* type == CURSOR_DND */
    {
      index = g_utf8_offset_to_pointer (entry->text, entry->dnd_position) - entry->text;
      if (entry->dnd_position > entry->current_pos)
	index += entry->preedit_length;
    }
      
  pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);

  if (strong_x)
    *strong_x = strong_pos.x / PANGO_SCALE;

  if (weak_x)
    *weak_x = weak_pos.x / PANGO_SCALE;
}

static void
gtk_entry_adjust_scroll (GtkEntry *entry)
{
  gint min_offset, max_offset;
  gint text_area_width;
  gint strong_x, weak_x;
  gint strong_xoffset, weak_xoffset;
  PangoLayout *layout;
  PangoLayoutLine *line;
  PangoRectangle logical_rect;

  if (!GTK_WIDGET_REALIZED (entry))
    return;
  
  gdk_window_get_size (entry->text_area, &text_area_width, NULL);
  text_area_width -= 2 * INNER_BORDER;

  layout = gtk_entry_ensure_layout (entry, TRUE);
  line = pango_layout_get_lines (layout)->data;

  pango_layout_line_get_extents (line, NULL, &logical_rect);

  /* Display as much text as we can */

  if (gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_LTR)
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

  gtk_entry_get_cursor_locations (entry, CURSOR_STANDARD, &strong_x, &weak_x);
  
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

  g_object_notify (G_OBJECT (entry), "scroll_offset");
}

static gint
gtk_entry_move_visually (GtkEntry *entry,
			 gint      start,
			 gint      count)
{
  gint index;
  PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
  const gchar *text;

  text = pango_layout_get_text (layout);
  
  index = g_utf8_offset_to_pointer (text, start) - text;

  while (count != 0)
    {
      int new_index, new_trailing;
      gboolean split_cursor;
      gboolean strong;

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (entry)),
		    "gtk-split-cursor", &split_cursor,
		    NULL);

      if (split_cursor)
	strong = TRUE;
      else
	{
	  GtkTextDirection keymap_direction =
	    (gdk_keymap_get_direction (gdk_keymap_get_default ()) == PANGO_DIRECTION_LTR) ?
	    GTK_TEXT_DIR_LTR : GTK_TEXT_DIR_RTL;

	  strong = keymap_direction == gtk_widget_get_direction (GTK_WIDGET (entry));
	}
      
      if (count > 0)
	{
	  pango_layout_move_cursor_visually (layout, strong, index, 0, 1, &new_index, &new_trailing);
	  count--;
	}
      else
	{
	  pango_layout_move_cursor_visually (layout, strong, index, 0, -1, &new_index, &new_trailing);
	  count++;
	}

      if (new_index < 0 || new_index == G_MAXINT)
	break;

      index = new_index;
      
      while (new_trailing--)
	index = g_utf8_next_char (entry->text + new_index) - entry->text;
    }
  
  return g_utf8_pointer_to_offset (text, text + index);
}

static gint
gtk_entry_move_logically (GtkEntry *entry,
			  gint      start,
			  gint      count)
{
  gint new_pos = start;

  /* Prevent any leak of information */
  if (!entry->visible)
    {
      new_pos = CLAMP (start + count, 0, entry->text_length);
    }
  else if (entry->text)
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
      PangoLogAttr *log_attrs;
      gint n_attrs;

      pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);

      while (count > 0 && new_pos < entry->text_length)
	{
	  do
	    new_pos++;
	  while (new_pos < entry->text_length && !log_attrs[new_pos].is_cursor_position);
	  
	  count--;
	}
      while (count < 0 && new_pos > 0)
	{
	  do
	    new_pos--;
	  while (new_pos > 0 && !log_attrs[new_pos].is_cursor_position);
	  
	  count++;
	}
      
      g_free (log_attrs);
    }

  return new_pos;
}

static gint
gtk_entry_move_forward_word (GtkEntry *entry,
			     gint      start)
{
  gint new_pos = start;

  /* Prevent any leak of information */
  if (!entry->visible)
    {
      new_pos = entry->text_length;
    }
  else if (entry->text && (new_pos < entry->text_length))
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
      PangoLogAttr *log_attrs;
      gint n_attrs;

      pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);
      
      /* Find the next word end */
      new_pos++;
      while (new_pos < n_attrs && !log_attrs[new_pos].is_word_end)
	new_pos++;

      g_free (log_attrs);
    }

  return new_pos;
}


static gint
gtk_entry_move_backward_word (GtkEntry *entry,
			      gint      start)
{
  gint new_pos = start;

  /* Prevent any leak of information */
  if (!entry->visible)
    {
      new_pos = 0;
    }
  else if (entry->text && start > 0)
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
      PangoLogAttr *log_attrs;
      gint n_attrs;

      pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);

      new_pos = start - 1;

      /* Find the previous word beginning */
      while (new_pos > 0 && !log_attrs[new_pos].is_word_start)
	new_pos--;

      g_free (log_attrs);
    }

  return new_pos;
}

static void
gtk_entry_delete_whitespace (GtkEntry *entry)
{
  PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
  PangoLogAttr *log_attrs;
  gint n_attrs;
  gint start, end;

  pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);

  start = end = entry->current_pos;
  
  while (start > 0 && log_attrs[start-1].is_white)
    start--;

  while (end < n_attrs && log_attrs[start-1].is_white)
    end++;

  g_free (log_attrs);

  if (start != end)
    gtk_editable_delete_text (GTK_EDITABLE (entry), start, end);
}


static void
gtk_entry_select_word (GtkEntry *entry)
{
  gint start_pos = gtk_entry_move_backward_word (entry, entry->current_pos);
  gint end_pos = gtk_entry_move_forward_word (entry, entry->current_pos);

  gtk_editable_select_region (GTK_EDITABLE (entry), start_pos, end_pos);
}

static void
gtk_entry_select_line (GtkEntry *entry)
{
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}

/*
 * Like gtk_editable_get_chars, but if the editable is not
 * visible, return asterisks; also convert result to UTF-8.
 */
static char *    
gtk_entry_get_public_chars (GtkEntry *entry,
			    gint      start,
			    gint      end)
{
  if (end < 0)
    end = entry->text_length;
  
  if (entry->visible)
    return gtk_editable_get_chars (GTK_EDITABLE (entry), start, end);
  else
    {
      gchar *str;
      gint i;
      gint n_chars = end - start;
       
      str = g_malloc (n_chars + 1);
      for (i = 0; i < n_chars; i++)
	str[i] = '*';
      str[i] = '\0';
      
      return str;
    }

}

static void
paste_received (GtkClipboard *clipboard,
		const gchar  *text,
		gpointer      data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  GtkEditable *editable = GTK_EDITABLE (entry);
      
  if (text)
    {
      gint pos, start, end;
      
      if (gtk_editable_get_selection_bounds (editable, &start, &end))
        gtk_editable_delete_text (editable, start, end);

      pos = entry->current_pos;
      gtk_editable_insert_text (editable, text, -1, &pos);
      gtk_editable_set_position (editable, pos);
    }

  g_object_unref (G_OBJECT (entry));
}

static void
gtk_entry_paste (GtkEntry *entry,
		 GdkAtom   selection)
{
  g_object_ref (G_OBJECT (entry));
  gtk_clipboard_request_text (gtk_clipboard_get (selection),
			      paste_received, entry);
}

static void
primary_get_cb (GtkClipboard     *clipboard,
		GtkSelectionData *selection_data,
		guint             info,
		gpointer          data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  gint start, end;
  
  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
    {
      gchar *str = gtk_entry_get_public_chars (entry, start, end);
      gtk_selection_data_set_text (selection_data, str, -1);
      g_free (str);
    }
}

static void
primary_clear_cb (GtkClipboard *clipboard,
		  gpointer      data)
{
  GtkEntry *entry = GTK_ENTRY (data);

  gtk_editable_select_region (GTK_EDITABLE (entry), entry->current_pos, entry->current_pos);
}

static void
gtk_entry_update_primary_selection (GtkEntry *entry)
{
  static const GtkTargetEntry targets[] = {
    { "UTF8_STRING", 0, 0 },
    { "STRING", 0, 0 },
    { "TEXT",   0, 0 }, 
    { "COMPOUND_TEXT", 0, 0 }
  };
  
  GtkClipboard *clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
  gint start, end;
  
  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
    {
      if (!gtk_clipboard_set_with_owner (clipboard, targets, G_N_ELEMENTS (targets),
					 primary_get_cb, primary_clear_cb, G_OBJECT (entry)))
	primary_clear_cb (clipboard, entry);
    }
  else
    {
      if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (entry))
	gtk_clipboard_clear (clipboard);
    }
}

/* Public API
 */

GtkWidget*
gtk_entry_new (void)
{
  return GTK_WIDGET (gtk_type_new (GTK_TYPE_ENTRY));
}

GtkWidget*
gtk_entry_new_with_max_length (gint max)
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

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (text != NULL);
  
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);

  tmp_pos = 0;
  gtk_editable_insert_text (GTK_EDITABLE (editable), text, strlen (text), &tmp_pos);
}

void
gtk_entry_append_text (GtkEntry *entry,
		       const gchar *text)
{
  gint tmp_pos;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (text != NULL);

  tmp_pos = entry->text_length;
  gtk_editable_insert_text (GTK_EDITABLE (entry), text, -1, &tmp_pos);
}

void
gtk_entry_prepend_text (GtkEntry *entry,
			const gchar *text)
{
  gint tmp_pos;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (text != NULL);

  tmp_pos = 0;
  gtk_editable_insert_text (GTK_EDITABLE (entry), text, -1, &tmp_pos);
}

void
gtk_entry_set_position (GtkEntry *entry,
			gint       position)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_editable_set_position (GTK_EDITABLE (entry), position);
}

void
gtk_entry_set_visibility (GtkEntry *entry,
			  gboolean visible)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  entry->visible = visible ? TRUE : FALSE;
  g_object_notify (G_OBJECT (entry), "visibility");
  gtk_entry_recompute (entry);
}

/**
 * gtk_entry_get_visibility:
 * @entry: a #GtkEntry
 *
 * Retrieves whether the text in @entry is visible. See
 * gtk_entry_set_visibility().
 *
 * Return value: %TRUE if the text is currently visible
 **/
gboolean
gtk_entry_get_visibility (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return entry->visible;
}

/**
 * gtk_entry_set_invisible_char:
 * @entry: a #GtkEntry
 * @ch: a Unicode character
 * 
 * Sets the character to use in place of the actual text when
 * gtk_entry_set_visibility() has been called to set text visibility
 * to %FALSE. i.e. this is the character used in "password mode" to
 * show the user how many characters have been typed. The default
 * invisible char is an asterisk ('*').  If you set the invisible char
 * to 0, then the user will get no feedback at all; there will be
 * no text on the screen as they type.
 * 
 **/
void
gtk_entry_set_invisible_char (GtkEntry *entry,
                              gunichar  ch)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (ch == entry->invisible_char)
    return;

  entry->invisible_char = ch;
  g_object_notify (G_OBJECT (entry), "invisible_char");
  gtk_entry_recompute (entry);  
}

/**
 * gtk_entry_get_invisible_char:
 * @entry: a #GtkEntry
 *
 * Retrieves the character displayed in place of the real characters
 * for entries with visisbility set to false. See gtk_entry_set_invisible_char().
 *
 * Return value: the current invisible char, or 0, if the entry does not
 *               show invisible text at all. 
 **/
gunichar
gtk_entry_get_invisible_char (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return entry->invisible_char;
}

void
gtk_entry_set_editable (GtkEntry *entry,
			gboolean  editable)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_editable_set_editable (GTK_EDITABLE (entry), editable);
}

G_CONST_RETURN gchar*
gtk_entry_get_text (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return entry->text;
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
                          gint          max)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (max > 0 && entry->text_length > max)
    gtk_editable_delete_text (GTK_EDITABLE (entry), max, -1);
  
  entry->text_max_length = max;
  g_object_notify (G_OBJECT (entry), "max_length");
}

/**
 * gtk_entry_get_max_length:
 * @entry: a #GtkEntry
 *
 * Retrieves the maximum allowed length of the text in
 * @entry. See gtk_entry_set_max_length().
 *
 * Return value: the maximum allowed number of characters
 *               in #GtkEntry, or 0 if there is no maximum.
 **/
gint
gtk_entry_get_max_length (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return entry->text_max_length;
}

/**
 * gtk_entry_set_activates_default:
 * @entry: a #GtkEntry
 * @setting: %TRUE to activate window's default widget on Enter keypress
 *
 * If @setting is %TRUE, pressing Enter in the @entry will activate the default
 * widget for the window containing the entry. This usually means that
 * the dialog box containing the entry will be closed, since the default
 * widget is usually one of the dialog buttons.
 *
 * (For experts: if @setting is %TRUE, the entry calls
 * gtk_window_activate_default() on the window containing the entry, in
 * the default handler for the "activate" signal.)
 * 
 **/
void
gtk_entry_set_activates_default (GtkEntry *entry,
                                 gboolean  setting)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));
  setting = setting != FALSE;

  if (setting != entry->activates_default)
    {
      entry->activates_default = setting;
      g_object_notify (G_OBJECT (entry), "activates_default");
    }
}

/**
 * gtk_entry_get_activates_default:
 * @entry: a #GtkEntry
 * 
 * Retrieves the value set by gtk_entry_set_activates_default().
 * 
 * Return value: %TRUE if the entry will activate the default widget
 **/
gboolean
gtk_entry_get_activates_default (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return entry->activates_default;
}

/**
 * gtk_entry_set_width_chars:
 * @entry: a #GtkEntry
 * @n_chars: width in chars
 *
 * Changes the size request of the entry to be about the right size
 * for @n_chars characters. Note that it changes the size
 * <emphasis>request</emphasis>, the size can still be affected by
 * how you pack the widget into containers. If @n_chars is -1, the
 * size reverts to the default entry size.
 * 
 **/
void
gtk_entry_set_width_chars (GtkEntry *entry,
                           gint      n_chars)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (entry->width_chars != n_chars)
    {
      entry->width_chars = n_chars;
      g_object_notify (G_OBJECT (entry), "width_chars");
      gtk_widget_queue_resize (GTK_WIDGET (entry));
    }
}

/**
 * gtk_entry_get_width_chars:
 * @entry: a #GtkEntry
 * 
 * Gets the value set by gtk_entry_set_width_chars().
 * 
 * Return value: number of chars to request space for, or negative if unset
 **/
gint
gtk_entry_get_width_chars (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return entry->width_chars;
}

/**
 * gtk_entry_set_has_frame:
 * @entry: a #GtkEntry
 * @setting: new value
 * 
 * Sets whether the entry has a beveled frame around it.
 **/
void
gtk_entry_set_has_frame (GtkEntry *entry,
                         gboolean  setting)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  setting = (setting != FALSE);

  if (entry->has_frame == setting)
    return;

  gtk_widget_queue_resize (GTK_WIDGET (entry));
  entry->has_frame = setting;
  g_object_notify (G_OBJECT (entry), "has_frame");
}

/**
 * gtk_entry_get_has_frame:
 * @entry: a #GtkEntry
 * 
 * Gets the value set by gtk_entry_set_has_frame().
 * 
 * Return value: whether the entry has a beveled frame
 **/
gboolean
gtk_entry_get_has_frame (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return entry->has_frame;
}


/**
 * gtk_entry_get_layout:
 * @entry: a #GtkEntry
 * 
 * Gets the #PangoLayout used to display the entry.
 * The layout is useful to e.g. convert text positions to
 * pixel positions, in combination with gtk_entry_get_layout_offsets().
 * The returned layout is owned by the entry so need not be
 * freed by the caller.
 *
 * Keep in mind that the layout text may contain a preedit string, so
 * gtk_entry_layout_index_to_text_index() and
 * gtk_entry_text_index_to_layout_index() are needed to convert byte
 * indices in the layout to byte indices in the entry contents.
 * 
 * Return value: the #PangoLayout for this entry
 **/
PangoLayout*
gtk_entry_get_layout (GtkEntry *entry)
{
  PangoLayout *layout;
  
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  layout = gtk_entry_ensure_layout (entry, TRUE);

  return layout;
}


/**
 * gtk_entry_layout_index_to_text_index:
 * @entry: a #GtkEntry
 * @layout_index: byte index into the entry layout text
 * 
 * Converts from a position in the entry contents (returned
 * by gtk_entry_get_text()) to a position in the
 * entry's #PangoLayout (returned by gtk_entry_get_layout(),
 * with text retrieved via pango_layout_get_text()).
 * 
 * Return value: byte index into the entry contents
 **/
gint
gtk_entry_layout_index_to_text_index (GtkEntry *entry,
                                      gint      layout_index)
{
  gint cursor_index;
  
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  cursor_index = g_utf8_offset_to_pointer (entry->text, entry->current_pos) - entry->text;
  
  if (layout_index >= cursor_index && entry->preedit_length)
    {
      if (layout_index >= cursor_index + entry->preedit_length)
	layout_index -= entry->preedit_length;
      else
        layout_index = cursor_index;
    }

  return layout_index;
}

/**
 * gtk_entry_text_index_to_layout_index:
 * @entry: a #GtkEntry
 * @text_index: byte index into the entry contents
 * 
 * Converts from a position in the entry's #PangoLayout(returned by
 * gtk_entry_get_layout()) to a position in the entry contents
 * (returned by gtk_entry_get_text()).
 * 
 * Return value: byte index into the entry layout text
 **/
gint
gtk_entry_text_index_to_layout_index (GtkEntry *entry,
                                      gint      text_index)
{
  gint cursor_index;
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  cursor_index = g_utf8_offset_to_pointer (entry->text, entry->current_pos) - entry->text;
  
  if (text_index > cursor_index)
    text_index += entry->preedit_length;

  return text_index;
}

/**
 * gtk_entry_get_layout_offsets:
 * @entry: a #GtkEntry
 * @x: location to store X offset of layout, or %NULL
 * @y: location to store Y offset of layout, or %NULL
 *
 *
 * Obtains the position of the #PangoLayout used to render text
 * in the entry, in widget coordinates. Useful if you want to line
 * up the text in an entry with some other text, e.g. when using the
 * entry to implement editable cells in a sheet widget.
 *
 * Also useful to convert mouse events into coordinates inside the
 * #PangoLayout, e.g. to take some action if some part of the entry text
 * is clicked.
 * 
 * Note that as the user scrolls around in the entry the offsets will
 * change; you'll need to connect to the "notify::scroll_offset"
 * signal to track this. Remember when using the #PangoLayout
 * functions you need to convert to and from pixels using
 * PANGO_PIXELS() or #PANGO_SCALE.
 *
 * Keep in mind that the layout text may contain a preedit string, so
 * gtk_entry_layout_index_to_text_index() and
 * gtk_entry_text_index_to_layout_index() are needed to convert byte
 * indices in the layout to byte indices in the entry contents.
 * 
 **/
void
gtk_entry_get_layout_offsets (GtkEntry *entry,
                              gint     *x,
                              gint     *y)
{
  gint text_area_x, text_area_y;
  
  g_return_if_fail (GTK_IS_ENTRY (entry));

  /* this gets coords relative to text area */
  get_layout_position (entry, x, y);

  /* convert to widget coords */
  get_text_area_size (entry, &text_area_x, &text_area_y, NULL, NULL);
  
  if (x)
    *x += text_area_x;

  if (y)
    *y += text_area_y;
}

/* Quick hack of a popup menu
 */
static void
activate_cb (GtkWidget *menuitem,
	     GtkEntry  *entry)
{
  const gchar *signal = gtk_object_get_data (GTK_OBJECT (menuitem), "gtk-signal");
  gtk_signal_emit_by_name (GTK_OBJECT (entry), signal);
}


static gboolean
gtk_entry_mnemonic_activate (GtkWidget *widget,
			     gboolean   group_cycling)
{
  gtk_widget_grab_focus (widget);
  return TRUE;
}

static void
append_action_signal (GtkEntry     *entry,
		      GtkWidget    *menu,
		      const gchar  *stock_id,
		      const gchar  *signal,
                      gboolean      sensitive)
{
  GtkWidget *menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);

  gtk_object_set_data (GTK_OBJECT (menuitem), "gtk-signal", (char *)signal);
  gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
		      GTK_SIGNAL_FUNC (activate_cb), entry);

  gtk_widget_set_sensitive (menuitem, sensitive);
  
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}
	
static void
popup_menu_detach (GtkWidget *attach_widget,
		   GtkMenu   *menu)
{
  GTK_ENTRY (attach_widget)->popup_menu = NULL;
}

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer	user_data)
{
  GtkEntry *entry = GTK_ENTRY (user_data);
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkRequisition req;
  
  g_return_if_fail (GTK_WIDGET_REALIZED (entry));

  gdk_window_get_origin (widget->window, x, y);      

  gtk_widget_size_request (entry->popup_menu, &req);
  
  *x += widget->allocation.width / 2;
  *y += widget->allocation.height;

  *x = CLAMP (*x, 0, MAX (0, gdk_screen_width () - req.width));
  *y = CLAMP (*y, 0, MAX (0, gdk_screen_height () - req.height));
}


static void
unichar_chosen_func (const char *text,
                     gpointer    data)
{
  GtkEntry *entry = GTK_ENTRY (data);

  gtk_entry_enter_text (entry, text);
}

typedef struct
{
  GtkEntry *entry;
  gint button;
  guint time;
} PopupInfo;

static void
popup_targets_received (GtkClipboard     *clipboard,
			GtkSelectionData *data,
			gpointer          user_data)
{
  PopupInfo *info = user_data;
  GtkEntry *entry = info->entry;
  
  if (GTK_WIDGET_REALIZED (entry))
    {
      gboolean clipboard_contains_text = gtk_selection_data_targets_include_text (data);
      GtkWidget *menuitem;
      GtkWidget *submenu;
      
      if (entry->popup_menu)
	gtk_widget_destroy (entry->popup_menu);
      
      entry->popup_menu = gtk_menu_new ();
      
      gtk_menu_attach_to_widget (GTK_MENU (entry->popup_menu),
				 GTK_WIDGET (entry),
				 popup_menu_detach);
      
      append_action_signal (entry, entry->popup_menu, GTK_STOCK_CUT, "cut_clipboard",
			    entry->editable && entry->current_pos != entry->selection_bound);
      append_action_signal (entry, entry->popup_menu, GTK_STOCK_COPY, "copy_clipboard",
			    entry->current_pos != entry->selection_bound);
      append_action_signal (entry, entry->popup_menu, GTK_STOCK_PASTE, "paste_clipboard",
			    entry->editable && clipboard_contains_text);
      
      menuitem = gtk_menu_item_new_with_label (_("Select All"));
      gtk_signal_connect_object (GTK_OBJECT (menuitem), "activate",
				 GTK_SIGNAL_FUNC (gtk_entry_select_all), entry);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (entry->popup_menu), menuitem);
      
      menuitem = gtk_separator_menu_item_new ();
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (entry->popup_menu), menuitem);
      
      menuitem = gtk_menu_item_new_with_label (_("Input Methods"));
      gtk_widget_show (menuitem);
      submenu = gtk_menu_new ();
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
      
      gtk_menu_shell_append (GTK_MENU_SHELL (entry->popup_menu), menuitem);
      
      gtk_im_multicontext_append_menuitems (GTK_IM_MULTICONTEXT (entry->im_context),
					    GTK_MENU_SHELL (submenu));
      
      menuitem = gtk_menu_item_new_with_mnemonic (_("_Insert Unicode control character"));
      gtk_widget_show (menuitem);
      
      submenu = gtk_menu_new ();
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
      gtk_menu_shell_append (GTK_MENU_SHELL (entry->popup_menu), menuitem);      

      _gtk_text_util_append_special_char_menuitems (GTK_MENU_SHELL (submenu),
                                                    unichar_chosen_func,
                                                    entry);
      if (!entry->editable)
        gtk_widget_set_sensitive (menuitem, FALSE);
      
      gtk_signal_emit (GTK_OBJECT (entry),
		       signals[POPULATE_POPUP],
		       entry->popup_menu);
  

      if (info->button)
	gtk_menu_popup (GTK_MENU (entry->popup_menu), NULL, NULL,
			NULL, NULL,
			info->button, info->time);
      else
	gtk_menu_popup (GTK_MENU (entry->popup_menu), NULL, NULL,
			popup_position_func, entry,
			info->button, info->time);
    }

  g_object_unref (entry);
  g_free (info);
}
			
static void
gtk_entry_do_popup (GtkEntry       *entry,
                    GdkEventButton *event)
{
  PopupInfo *info = g_new (PopupInfo, 1);

  /* In order to know what entries we should make sensitive, we
   * ask for the current targets of the clipboard, and when
   * we get them, then we actually pop up the menu.
   */
  info->entry = g_object_ref (entry);
  
  if (event)
    {
      info->button = event->button;
      info->time = event->time;
    }
  else
    {
      info->button = 0;
      info->time = gtk_get_current_event_time ();
    }

  gtk_clipboard_request_contents (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				  gdk_atom_intern ("TARGETS", FALSE),
				  popup_targets_received,
				  info);
}

static void
gtk_entry_popup_menu (GtkWidget *widget)
{
  gtk_entry_do_popup (GTK_ENTRY (widget), NULL);
}

static void
gtk_entry_drag_leave (GtkWidget        *widget,
		      GdkDragContext   *context,
		      guint             time)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  entry->dnd_position = -1;
  gtk_widget_queue_draw (widget);
}

static gboolean
gtk_entry_drag_drop  (GtkWidget        *widget,
		      GdkDragContext   *context,
		      gint              x,
		      gint              y,
		      guint             time)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GdkAtom target = GDK_NONE;
  
  if (entry->editable)
    target = gtk_drag_dest_find_target (widget, context, NULL);

  if (target != GDK_NONE)
    gtk_drag_get_data (widget, context, target, time);
  else
    gtk_drag_finish (context, FALSE, FALSE, time);
  
  return TRUE;
}

static gboolean
gtk_entry_drag_motion (GtkWidget        *widget,
		       GdkDragContext   *context,
		       gint              x,
		       gint              y,
		       guint             time)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkWidget *source_widget;
  GdkDragAction suggested_action;
  gint new_position, old_position;
  gint sel1, sel2;
  
  x -= widget->style->xthickness;
  y -= widget->style->ythickness;
  
  old_position = entry->dnd_position;
  new_position = gtk_entry_find_position (entry, x + entry->scroll_offset);

  if (entry->editable)
    {
      source_widget = gtk_drag_get_source_widget (context);
      suggested_action = context->suggested_action;

      if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &sel1, &sel2) ||
          new_position < sel1 || new_position > sel2)
        {
          if (source_widget == widget)
	    {
	      /* Default to MOVE, unless the user has
	       * pressed ctrl or alt to affect available actions
	       */
	      if ((context->actions & GDK_ACTION_MOVE) != 0)
	        suggested_action = GDK_ACTION_MOVE;
	    }
              
          entry->dnd_position = new_position;
        }
      else
        {
          if (source_widget == widget)
	    suggested_action = 0;	/* Can't drop in selection where drag started */
          
          entry->dnd_position = -1;
        }
    }
  else
    {
      /* Entry not editable */
      suggested_action = 0;
      entry->dnd_position = -1;
    }
  
  gdk_drag_status (context, suggested_action, time);
  
  if (entry->dnd_position != old_position)
    gtk_widget_queue_draw (widget);

  return TRUE;
}

static void
gtk_entry_drag_data_received (GtkWidget        *widget,
			      GdkDragContext   *context,
			      gint              x,
			      gint              y,
			      GtkSelectionData *selection_data,
			      guint             info,
			      guint             time)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEditable *editable = GTK_EDITABLE (widget);
  gchar *str;

  str = gtk_selection_data_get_text (selection_data);

  if (str && entry->editable)
    {
      gint new_position;
      gint sel1, sel2;

      new_position = gtk_entry_find_position (entry, x + entry->scroll_offset);

      if (!gtk_editable_get_selection_bounds (editable, &sel1, &sel2) ||
	  new_position < sel1 || new_position > sel2)
	{
	  gtk_editable_insert_text (editable, str, -1, &new_position);
	}
      else
	{
	  /* Replacing selection */
	  gtk_editable_delete_text (editable, sel1, sel2);
	  gtk_editable_insert_text (editable, str, -1, &sel1);
	}
      
      g_free (str);
      gtk_drag_finish (context, TRUE, context->action == GDK_ACTION_MOVE, time);
    }
  else
    {
      /* Drag and drop didn't happen! */
      gtk_drag_finish (context, FALSE, FALSE, time);
    }
}

static void
gtk_entry_drag_data_get (GtkWidget        *widget,
			 GdkDragContext   *context,
			 GtkSelectionData *selection_data,
			 guint             info,
			 guint             time)
{
  gint sel_start, sel_end;

  GtkEditable *editable = GTK_EDITABLE (widget);
  
  if (gtk_editable_get_selection_bounds (editable, &sel_start, &sel_end))
    {
      gchar *str = gtk_editable_get_chars (editable, sel_start, sel_end);

      gtk_selection_data_set_text (selection_data, str, -1);
      
      g_free (str);
    }

}

static void
gtk_entry_drag_data_delete (GtkWidget      *widget,
			    GdkDragContext *context)
{
  gint sel_start, sel_end;

  GtkEditable *editable = GTK_EDITABLE (widget);
  
  if (GTK_ENTRY (widget)->editable &&
      gtk_editable_get_selection_bounds (editable, &sel_start, &sel_end))
    gtk_editable_delete_text (editable, sel_start, sel_end);
}

/* We display the cursor when
 *
 *  - the selection is empty, AND
 *  - the widget has focus
 */

#define CURSOR_ON_MULTIPLIER 0.66
#define CURSOR_OFF_MULTIPLIER 0.34
#define CURSOR_PEND_MULTIPLIER 1.0

static gboolean
cursor_blinks (GtkEntry *entry)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (entry));
  gboolean blink;

  if (GTK_WIDGET_HAS_FOCUS (entry) &&
      entry->selection_bound == entry->current_pos)
    {
      g_object_get (G_OBJECT (settings), "gtk-cursor-blink", &blink, NULL);
      return blink;
    }
  else
    return FALSE;
}

static gint
get_cursor_time (GtkEntry *entry)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (entry));
  gint time;

  g_object_get (G_OBJECT (settings), "gtk-cursor-blink-time", &time, NULL);

  return time;
}

static void
show_cursor (GtkEntry *entry)
{
  if (!entry->cursor_visible)
    {
      entry->cursor_visible = TRUE;

      if (GTK_WIDGET_HAS_FOCUS (entry) && entry->selection_bound == entry->current_pos)
	gtk_widget_queue_draw (GTK_WIDGET (entry));
    }
}

static void
hide_cursor (GtkEntry *entry)
{
  if (entry->cursor_visible)
    {
      entry->cursor_visible = FALSE;

      if (GTK_WIDGET_HAS_FOCUS (entry) && entry->selection_bound == entry->current_pos)
	gtk_widget_queue_draw (GTK_WIDGET (entry));
    }
}

/*
 * Blink!
 */
static gint
blink_cb (gpointer data)
{
  GtkEntry *entry;

  GDK_THREADS_ENTER ();

  entry = GTK_ENTRY (data);
  
  g_assert (GTK_WIDGET_HAS_FOCUS (entry));
  g_assert (entry->selection_bound == entry->current_pos);

  if (entry->cursor_visible)
    {
      hide_cursor (entry);
      entry->blink_timeout = gtk_timeout_add (get_cursor_time (entry) * CURSOR_OFF_MULTIPLIER,
					      blink_cb,
					      entry);
    }
  else
    {
      show_cursor (entry);
      entry->blink_timeout = gtk_timeout_add (get_cursor_time (entry) * CURSOR_ON_MULTIPLIER,
					      blink_cb,
					      entry);
    }

  GDK_THREADS_LEAVE ();

  /* Remove ourselves */
  return FALSE;
}

static void
gtk_entry_check_cursor_blink (GtkEntry *entry)
{
  if (cursor_blinks (entry))
    {
      if (!entry->blink_timeout)
	{
	  entry->blink_timeout = gtk_timeout_add (get_cursor_time (entry) * CURSOR_ON_MULTIPLIER,
						  blink_cb,
						  entry);
	  show_cursor (entry);
	}
    }
  else
    {
      if (entry->blink_timeout)  
	{ 
	  gtk_timeout_remove (entry->blink_timeout);
	  entry->blink_timeout = 0;
	}
      
      entry->cursor_visible = TRUE;
    }
  
}

static void
gtk_entry_pend_cursor_blink (GtkEntry *entry)
{
  if (cursor_blinks (entry))
    {
      if (entry->blink_timeout != 0)
	gtk_timeout_remove (entry->blink_timeout);
      
      entry->blink_timeout = gtk_timeout_add (get_cursor_time (entry) * CURSOR_PEND_MULTIPLIER,
					      blink_cb,
					      entry);
      show_cursor (entry);
    }
}
