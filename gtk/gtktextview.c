/* gtktext.c - A "view" widget for the GtkTextBuffer object
 * 
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 * Copyright (c) 1999 by Scriptics Corporation.
 * Copyright (c) 2000      Red Hat, Inc.
 * Tk -> Gtk port by Havoc Pennington <hp@redhat.com>
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The
 * following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 * 
 * The authors hereby grant permission to use, copy, modify,
 * distribute, and license this software and its documentation for any
 * purpose, provided that existing copyright notices are retained in
 * all copies and that this notice is included verbatim in any
 * distributions. No written agreement, license, or royalty fee is
 * required for any of the authorized uses.  Modifications to this
 * software may be copyrighted by their authors and need not follow
 * the licensing terms described here, provided that the new terms are
 * clearly indicated on the first page of each file where they apply.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION,
 * OR ANY DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense,
 * the software shall be classified as "Commercial Computer Software"
 * and the Government shall have only "Restricted Rights" as defined
 * in Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 * foregoing, the authors grant the U.S. Government and others acting
 * in its behalf permission to use and distribute the software in
 * accordance with the terms specified in this license.
 * 
 */

#include <string.h>

#include "gtkbindings.h"
#include "gtkdnd.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtktext.h"
#include "gtktextdisplay.h"
#include "gtktextview.h"
#include "gtkimmulticontext.h"
#include "gdk/gdkkeysyms.h"

enum {
  MOVE_INSERT,
  SET_ANCHOR,
  SCROLL_TEXT,
  DELETE_TEXT,
  CUT_TEXT,
  COPY_TEXT,
  PASTE_TEXT,
  TOGGLE_OVERWRITE,
  SET_SCROLL_ADJUSTMENTS,
  LAST_SIGNAL
};

enum {
  ARG_0,
  ARG_HEIGHT_LINES,
  ARG_WIDTH_COLUMNS,
  ARG_PIXELS_ABOVE_LINES,
  ARG_PIXELS_BELOW_LINES,
  ARG_PIXELS_INSIDE_WRAP,
  ARG_EDITABLE,
  ARG_WRAP_MODE,
  LAST_ARG
};

static void gtk_text_view_init                 (GtkTextView      *text_view);
static void gtk_text_view_class_init           (GtkTextViewClass *klass);
static void gtk_text_view_destroy              (GtkObject        *object);
static void gtk_text_view_finalize             (GObject          *object);
static void gtk_text_view_set_arg              (GtkObject        *object,
						GtkArg           *arg,
						guint             arg_id);
static void gtk_text_view_get_arg              (GtkObject        *object,
						GtkArg           *arg,
						guint             arg_id);
static void gtk_text_view_size_request         (GtkWidget        *widget,
						GtkRequisition   *requisition);
static void gtk_text_view_size_allocate        (GtkWidget        *widget,
						GtkAllocation    *allocation);
static void gtk_text_view_realize              (GtkWidget        *widget);
static void gtk_text_view_unrealize            (GtkWidget        *widget);
static void gtk_text_view_style_set            (GtkWidget        *widget,
						GtkStyle         *previous_style);
static void gtk_text_view_direction_changed    (GtkWidget        *widget,
						GtkTextDirection  previous_direction);
static gint gtk_text_view_event                (GtkWidget        *widget,
						GdkEvent         *event);
static gint gtk_text_view_key_press_event      (GtkWidget        *widget,
						GdkEventKey      *event);
static gint gtk_text_view_key_release_event    (GtkWidget        *widget,
						GdkEventKey      *event);
static gint gtk_text_view_button_press_event   (GtkWidget        *widget,
						GdkEventButton   *event);
static gint gtk_text_view_button_release_event (GtkWidget        *widget,
						GdkEventButton   *event);
static gint gtk_text_view_focus_in_event       (GtkWidget        *widget,
						GdkEventFocus    *event);
static gint gtk_text_view_focus_out_event      (GtkWidget        *widget,
						GdkEventFocus    *event);
static gint gtk_text_view_motion_event         (GtkWidget        *widget,
						GdkEventMotion   *event);
static void gtk_text_view_draw                 (GtkWidget        *widget,
						GdkRectangle     *area);
static gint gtk_text_view_expose_event         (GtkWidget        *widget,
						GdkEventExpose   *expose);


/* Source side drag signals */
static void gtk_text_view_drag_begin       (GtkWidget        *widget,
					    GdkDragContext   *context);
static void gtk_text_view_drag_end         (GtkWidget        *widget,
					    GdkDragContext   *context);
static void gtk_text_view_drag_data_get    (GtkWidget        *widget,
					    GdkDragContext   *context,
					    GtkSelectionData *selection_data,
					    guint             info,
					    guint             time);
static void gtk_text_view_drag_data_delete (GtkWidget        *widget,
					    GdkDragContext   *context);

/* Target side drag signals */
static void     gtk_text_view_drag_leave         (GtkWidget        *widget,
						  GdkDragContext   *context,
						  guint             time);
static gboolean gtk_text_view_drag_motion        (GtkWidget        *widget,
						  GdkDragContext   *context,
						  gint              x,
						  gint              y,
						  guint             time);
static gboolean gtk_text_view_drag_drop          (GtkWidget        *widget,
						  GdkDragContext   *context,
						  gint              x,
						  gint              y,
						  guint             time);
static void     gtk_text_view_drag_data_received (GtkWidget        *widget,
						  GdkDragContext   *context,
						  gint              x,
						  gint              y,
						  GtkSelectionData *selection_data,
						  guint             info,
						  guint             time);

static void gtk_text_view_set_scroll_adjustments (GtkTextView   *text_view,
						  GtkAdjustment *hadj,
						  GtkAdjustment *vadj);

static void gtk_text_view_move_insert      (GtkTextView             *text_view,
					    GtkTextViewMovementStep  step,
					    gint                     count,
					    gboolean                 extend_selection);
static void gtk_text_view_set_anchor       (GtkTextView             *text_view);
static void gtk_text_view_scroll_text      (GtkTextView             *text_view,
					    GtkTextViewScrollType    type);
static void gtk_text_view_delete_text      (GtkTextView             *text_view,
					    GtkTextViewDeleteType    type,
					    gint                     count);
static void gtk_text_view_cut_text         (GtkTextView             *text_view);
static void gtk_text_view_copy_text        (GtkTextView             *text_view);
static void gtk_text_view_paste_text       (GtkTextView             *text_view);
static void gtk_text_view_toggle_overwrite (GtkTextView             *text_view);


static void     gtk_text_view_validate_onscreen     (GtkTextView        *text_view);
static void     gtk_text_view_get_first_para_iter   (GtkTextView        *text_view,
						     GtkTextIter        *iter);
static void     gtk_text_view_scroll_calc_now       (GtkTextView        *text_view);
static void     gtk_text_view_set_values_from_style (GtkTextView        *text_view,
						     GtkTextStyleValues *values,
						     GtkStyle           *style);
static void     gtk_text_view_ensure_layout         (GtkTextView        *text_view);
static void     gtk_text_view_destroy_layout        (GtkTextView        *text_view);
static void     gtk_text_view_start_selection_drag  (GtkTextView        *text_view,
						     const GtkTextIter  *iter,
						     GdkEventButton     *event);
static gboolean gtk_text_view_end_selection_drag    (GtkTextView        *text_view,
						     GdkEventButton     *event);
static void     gtk_text_view_start_selection_dnd   (GtkTextView        *text_view,
						     const GtkTextIter  *iter,
						     GdkEventButton     *event);
static void     gtk_text_view_start_cursor_blink    (GtkTextView        *text_view);
static void     gtk_text_view_stop_cursor_blink     (GtkTextView        *text_view);

static void gtk_text_view_value_changed (GtkAdjustment *adj,
					 GtkTextView   *view);
static void gtk_text_view_commit_handler            (GtkIMContext  *context,
						     const gchar   *str,
						     GtkTextView   *text_view);

static void gtk_text_view_mark_set_handler       (GtkTextBuffer     *buffer,
						  const GtkTextIter *location,
						  const char        *mark_name,
						  gpointer           data);
static void gtk_text_view_get_virtual_cursor_pos (GtkTextView       *text_view,
						  gint              *x,
						  gint              *y);
static void gtk_text_view_set_virtual_cursor_pos (GtkTextView       *text_view,
						  gint               x,
						  gint               y);

enum {
  TARGET_STRING,
  TARGET_TEXT,
  TARGET_COMPOUND_TEXT,
  TARGET_UTF8_STRING
};

static GdkAtom clipboard_atom = GDK_NONE;
static GdkAtom text_atom = GDK_NONE;
static GdkAtom ctext_atom = GDK_NONE;
static GdkAtom utf8_atom = GDK_NONE;


static GtkTargetEntry target_table[] = {
  { "UTF8_STRING", 0, TARGET_UTF8_STRING },
  { "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
  { "TEXT", 0, TARGET_TEXT },
  { "text/plain", 0, TARGET_STRING },
  { "STRING",     0, TARGET_STRING }
};

static guint n_targets = sizeof (target_table) / sizeof (target_table[0]);

static GtkContainerClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_text_view_get_type (void)
{
  static GtkType our_type = 0;

  if (our_type == 0)
    {
      static const GtkTypeInfo our_info =
      {
        "GtkTextView",
        sizeof (GtkTextView),
        sizeof (GtkTextViewClass),
        (GtkClassInitFunc) gtk_text_view_class_init,
        (GtkObjectInitFunc) gtk_text_view_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

    our_type = gtk_type_unique (GTK_TYPE_CONTAINER, &our_info);
  }

  return our_type;
}

static void
add_move_insert_binding (GtkBindingSet *binding_set,
			 guint keyval,
			 guint modmask,
			 GtkTextViewMovementStep step,
			 gint count)
{
  g_return_if_fail ((modmask & GDK_SHIFT_MASK) == 0);
  
  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
				"move_insert", 3,
				GTK_TYPE_ENUM, step,
				GTK_TYPE_INT, count,
                                GTK_TYPE_BOOL, FALSE);

  /* Selection-extending version */
  gtk_binding_entry_add_signal (binding_set, keyval, modmask | GDK_SHIFT_MASK,
				"move_insert", 3,
				GTK_TYPE_ENUM, step,
				GTK_TYPE_INT, count,
                                GTK_TYPE_BOOL, TRUE);
}

static void
gtk_text_view_class_init (GtkTextViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set;
  
  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);

  /*
   * Arguments
   */
  gtk_object_add_arg_type ("GtkTextView::height_lines", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_HEIGHT_LINES);
  gtk_object_add_arg_type ("GtkTextView::width_columns", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_WIDTH_COLUMNS);
  gtk_object_add_arg_type ("GtkTextView::pixels_above_lines", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_PIXELS_ABOVE_LINES);
  gtk_object_add_arg_type ("GtkTextView::pixels_below_lines", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_PIXELS_BELOW_LINES);
  gtk_object_add_arg_type ("GtkTextView::pixels_inside_wrap", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_PIXELS_INSIDE_WRAP);
  gtk_object_add_arg_type ("GtkTextView::editable", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_EDITABLE);
  gtk_object_add_arg_type ("GtkTextView::wrap_mode", GTK_TYPE_ENUM,
                           GTK_ARG_READWRITE, ARG_WRAP_MODE);


  /*
   * Signals
   */

  signals[MOVE_INSERT] = 
      gtk_signal_new ("move_insert",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      GTK_CLASS_TYPE (object_class),
                      GTK_SIGNAL_OFFSET (GtkTextViewClass, move_insert),
                      gtk_marshal_NONE__INT_INT_INT,
                      GTK_TYPE_NONE, 3, GTK_TYPE_ENUM, GTK_TYPE_INT, GTK_TYPE_BOOL);

  signals[SET_ANCHOR] = 
      gtk_signal_new ("set_anchor",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      GTK_CLASS_TYPE (object_class),
                      GTK_SIGNAL_OFFSET (GtkTextViewClass, set_anchor),
                      gtk_marshal_NONE__NONE,
                      GTK_TYPE_NONE, 0);

  signals[SCROLL_TEXT] = 
      gtk_signal_new ("scroll_text",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      GTK_CLASS_TYPE (object_class),
                      GTK_SIGNAL_OFFSET (GtkTextViewClass, scroll_text),
                      gtk_marshal_NONE__INT,
                      GTK_TYPE_NONE, 1, GTK_TYPE_ENUM);

  signals[DELETE_TEXT] = 
      gtk_signal_new ("delete_text",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      GTK_CLASS_TYPE (object_class),
                      GTK_SIGNAL_OFFSET (GtkTextViewClass, delete_text),
                      gtk_marshal_NONE__INT_INT,
                      GTK_TYPE_NONE, 2, GTK_TYPE_ENUM, GTK_TYPE_INT);

  signals[CUT_TEXT] =
    gtk_signal_new ("cut_text",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, cut_text),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);

  signals[COPY_TEXT] =
    gtk_signal_new ("copy_text",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, copy_text),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);

  signals[PASTE_TEXT] =
    gtk_signal_new ("paste_text",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, paste_text),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);

  signals[TOGGLE_OVERWRITE] =
    gtk_signal_new ("toggle_overwrite",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, toggle_overwrite),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);

  signals[SET_SCROLL_ADJUSTMENTS] = widget_class->set_scroll_adjustments_signal =
    gtk_signal_new ("set_scroll_adjustments",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTextViewClass, set_scroll_adjustments),
		    gtk_marshal_NONE__POINTER_POINTER,
		    GTK_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);
  
  gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

  /*
   * Key bindings
   */

  binding_set = gtk_binding_set_by_class (klass);

  /* Moving the insertion point */
  add_move_insert_binding (binding_set, GDK_Right, 0,
                          GTK_TEXT_MOVEMENT_POSITIONS, 1);
  
  add_move_insert_binding (binding_set, GDK_Left, 0,
                          GTK_TEXT_MOVEMENT_POSITIONS, -1);

  add_move_insert_binding (binding_set, GDK_f, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_CHAR, 1);
  
  add_move_insert_binding (binding_set, GDK_b, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_CHAR, -1);
  
  add_move_insert_binding (binding_set, GDK_Right, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_WORD, 1);

  add_move_insert_binding (binding_set, GDK_Left, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_WORD, -1);
  
  /* Eventually we want to move by display lines, not paragraphs */
  add_move_insert_binding (binding_set, GDK_Up, 0,
                          GTK_TEXT_MOVEMENT_LINE, -1);
  
  add_move_insert_binding (binding_set, GDK_Down, 0,
                          GTK_TEXT_MOVEMENT_LINE, 1);

  add_move_insert_binding (binding_set, GDK_p, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_LINE, -1);
  
  add_move_insert_binding (binding_set, GDK_n, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_LINE, 1);
  
  add_move_insert_binding (binding_set, GDK_a, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_PARAGRAPH_ENDS, -1);

  add_move_insert_binding (binding_set, GDK_e, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_PARAGRAPH_ENDS, 1);

  add_move_insert_binding (binding_set, GDK_f, GDK_MOD1_MASK,
                          GTK_TEXT_MOVEMENT_WORD, 1);

  add_move_insert_binding (binding_set, GDK_b, GDK_MOD1_MASK,
                          GTK_TEXT_MOVEMENT_WORD, -1);

  add_move_insert_binding (binding_set, GDK_Home, 0,
                          GTK_TEXT_MOVEMENT_BUFFER_ENDS, -1);

  add_move_insert_binding (binding_set, GDK_End, 0,
                          GTK_TEXT_MOVEMENT_BUFFER_ENDS, 1);
  
  /* Setting the cut/paste/copy anchor */
  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_CONTROL_MASK,
				"set_anchor", 0);

  
  /* Scrolling around */
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Up, 0,
				"scroll_text", 1,
				GTK_TYPE_ENUM, GTK_TEXT_SCROLL_PAGE_UP);

  gtk_binding_entry_add_signal (binding_set, GDK_Page_Down, 0,
				"scroll_text", 1,
				GTK_TYPE_ENUM, GTK_TEXT_SCROLL_PAGE_DOWN);

  /* Deleting text */
  gtk_binding_entry_add_signal (binding_set, GDK_Delete, 0,
				"delete_text", 2,
				GTK_TYPE_ENUM, GTK_TEXT_DELETE_CHAR,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, 0,
				"delete_text", 2,
				GTK_TYPE_ENUM, GTK_TEXT_DELETE_CHAR,
				GTK_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_Delete, GDK_CONTROL_MASK,
				"delete_text", 2,
				GTK_TYPE_ENUM, GTK_TEXT_DELETE_HALF_WORD,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, GDK_CONTROL_MASK,
				"delete_text", 2,
				GTK_TYPE_ENUM, GTK_TEXT_DELETE_HALF_WORD,
				GTK_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_k, GDK_CONTROL_MASK,
				"delete_text", 2,
				GTK_TYPE_ENUM, GTK_TEXT_DELETE_HALF_PARAGRAPH,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_u, GDK_CONTROL_MASK,
				"delete_text", 2,
				GTK_TYPE_ENUM, GTK_TEXT_DELETE_WHOLE_PARAGRAPH,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_MOD1_MASK,
				"delete_text", 2,
				GTK_TYPE_ENUM, GTK_TEXT_DELETE_WHITESPACE_LEAVE_ONE,
				GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_backslash, GDK_MOD1_MASK,
				"delete_text", 2,
				GTK_TYPE_ENUM, GTK_TEXT_DELETE_WHITESPACE,
				GTK_TYPE_INT, 1);
  
  /* Cut/copy/paste */

  gtk_binding_entry_add_signal (binding_set, GDK_x, GDK_CONTROL_MASK,
				"cut_text", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_w, GDK_CONTROL_MASK,
				"cut_text", 0);
  
  gtk_binding_entry_add_signal (binding_set, GDK_c, GDK_CONTROL_MASK,
				"copy_text", 0);
  
  gtk_binding_entry_add_signal (binding_set, GDK_y, GDK_CONTROL_MASK,
				"paste_text", 0);

  /* Overwrite */
  gtk_binding_entry_add_signal (binding_set, GDK_Insert, 0,
				"toggle_overwrite", 0);
  
  /*
   * Default handlers and virtual methods
   */
  object_class->set_arg = gtk_text_view_set_arg;
  object_class->get_arg = gtk_text_view_get_arg;

  object_class->destroy = gtk_text_view_destroy;
  gobject_class->finalize = gtk_text_view_finalize;

  widget_class->realize = gtk_text_view_realize;
  widget_class->unrealize = gtk_text_view_unrealize;
  widget_class->style_set = gtk_text_view_style_set;
  widget_class->direction_changed = gtk_text_view_direction_changed;
  widget_class->size_request = gtk_text_view_size_request;
  widget_class->size_allocate = gtk_text_view_size_allocate;
  widget_class->event = gtk_text_view_event;
  widget_class->key_press_event = gtk_text_view_key_press_event;
  widget_class->key_release_event = gtk_text_view_key_release_event;
  widget_class->button_press_event = gtk_text_view_button_press_event;
  widget_class->button_release_event = gtk_text_view_button_release_event;
  widget_class->focus_in_event = gtk_text_view_focus_in_event;
  widget_class->focus_out_event = gtk_text_view_focus_out_event;
  widget_class->motion_notify_event = gtk_text_view_motion_event;
  widget_class->expose_event = gtk_text_view_expose_event;
  widget_class->draw = gtk_text_view_draw;

  widget_class->drag_begin = gtk_text_view_drag_begin;
  widget_class->drag_end = gtk_text_view_drag_end;
  widget_class->drag_data_get = gtk_text_view_drag_data_get;
  widget_class->drag_data_delete = gtk_text_view_drag_data_delete;

  widget_class->drag_leave = gtk_text_view_drag_leave;
  widget_class->drag_motion = gtk_text_view_drag_motion;
  widget_class->drag_drop = gtk_text_view_drag_drop;
  widget_class->drag_data_received = gtk_text_view_drag_data_received;

  klass->move_insert = gtk_text_view_move_insert;
  klass->set_anchor = gtk_text_view_set_anchor;
  klass->scroll_text = gtk_text_view_scroll_text;
  klass->delete_text = gtk_text_view_delete_text;
  klass->cut_text = gtk_text_view_cut_text;
  klass->copy_text = gtk_text_view_copy_text;
  klass->paste_text = gtk_text_view_paste_text;
  klass->toggle_overwrite = gtk_text_view_toggle_overwrite;
  klass->set_scroll_adjustments = gtk_text_view_set_scroll_adjustments;
}

void
gtk_text_view_init (GtkTextView *text_view)
{
  GtkWidget *widget;
  
  widget = GTK_WIDGET (text_view);

  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);

  text_view->wrap_mode = GTK_WRAPMODE_NONE;

  if (!clipboard_atom)
    clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);

  if (!text_atom)
    text_atom = gdk_atom_intern ("TEXT", FALSE);

  if (!ctext_atom)
    ctext_atom = gdk_atom_intern ("COMPOUND_TEXT", FALSE);

  if (!utf8_atom)
    utf8_atom = gdk_atom_intern ("UTF8_STRING", FALSE);
  
  gtk_drag_dest_set (widget,
                    GTK_DEST_DEFAULT_DROP | GTK_DEST_DEFAULT_MOTION,
                    target_table, n_targets,
                    GDK_ACTION_COPY | GDK_ACTION_MOVE);

  text_view->virtual_cursor_x = -1;
  text_view->virtual_cursor_y = -1;

  /* This object is completely private. No external entity can gain a reference
   * to it; so we create it here and destroy it in finalize().
   */
  text_view->im_context = gtk_im_multicontext_new ();
  
  gtk_signal_connect (GTK_OBJECT (text_view->im_context), "commit",
		      GTK_SIGNAL_FUNC (gtk_text_view_commit_handler), text_view);
}

GtkWidget*
gtk_text_view_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_text_view_get_type ()));
}

GtkWidget*
gtk_text_view_new_with_buffer (GtkTextBuffer *buffer)
{
  GtkTextView *text_view;

  text_view = (GtkTextView*)gtk_text_view_new ();

  gtk_text_view_set_buffer (text_view, buffer);

  return GTK_WIDGET (text_view);
}

void
gtk_text_view_set_buffer (GtkTextView *text_view,
			  GtkTextBuffer *buffer)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (buffer == NULL || GTK_IS_TEXT_BUFFER (buffer));
  
  if (text_view->buffer == buffer)
    return;

  if (text_view->buffer != NULL)
    {
      gtk_signal_disconnect_by_func (GTK_OBJECT (text_view->buffer),
				     gtk_text_view_mark_set_handler, text_view);
      gtk_object_unref (GTK_OBJECT (text_view->buffer));
      text_view->dnd_mark = NULL;
    }

  text_view->buffer = buffer;
  
  if (buffer != NULL)
    {
      GtkTextIter start;
      
      gtk_object_ref (GTK_OBJECT (buffer));
      gtk_object_sink (GTK_OBJECT (buffer));

      if (text_view->layout)
        gtk_text_layout_set_buffer (text_view->layout, buffer);

      gtk_text_buffer_get_iter_at_char (text_view->buffer, &start, 0);
      
      text_view->dnd_mark = gtk_text_buffer_create_mark (text_view->buffer,
							 "__drag_target",
							 &start, FALSE);

      text_view->first_para_mark = gtk_text_buffer_create_mark (text_view->buffer,
                                                                NULL,
								&start, TRUE);
      
      text_view->first_para_pixels = 0;
      
      gtk_signal_connect (GTK_OBJECT (text_view->buffer), "mark_set",
			  gtk_text_view_mark_set_handler, text_view);
    }

  if (GTK_WIDGET_VISIBLE (text_view))    
    gtk_widget_queue_draw (GTK_WIDGET (text_view));
}

GtkTextBuffer*
gtk_text_view_get_buffer (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);
  
  return text_view->buffer;
}

void
gtk_text_view_get_iter_at_pixel (GtkTextView *text_view,
				 GtkTextIter *iter,
				 gint x, gint y)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (text_view->layout != NULL);

  gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                     iter,
                                     x + text_view->xoffset,
                                     y + text_view->yoffset);
}


static void
set_adjustment_clamped (GtkAdjustment *adj, gfloat val)
{
  /* We don't really want to clamp to upper; we want to clamp to
     upper - page_size which is the highest value the scrollbar
     will let us reach. */
  if (val > (adj->upper - adj->page_size))
    val = adj->upper - adj->page_size;

  if (val < adj->lower)
    val = adj->lower;
  
  gtk_adjustment_set_value (adj, val);
}

gboolean
gtk_text_view_scroll_to_mark_adjusted (GtkTextView *text_view,
                                       GtkTextMark *mark,
				       gint margin,
				       gfloat percentage)
{
  GtkTextIter iter;
  GdkRectangle rect;
  GdkRectangle screen;
  gint screen_bottom;
  gint screen_right;
  GtkWidget *widget;
  gboolean retval = FALSE;
  gint scroll_inc;

  gint current_x_scroll, current_y_scroll;
  
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (mark != NULL, FALSE);

  widget = GTK_WIDGET (text_view);
  
  if (!GTK_WIDGET_MAPPED (widget))
    {
      g_warning ("FIXME need to implement scroll_to_mark for unmapped GtkTextView?");
      return FALSE;
    }

  gtk_text_buffer_get_iter_at_mark (text_view->buffer, &iter, mark);
  
  gtk_text_layout_get_iter_location (text_view->layout,
                                     &iter,
                                     &rect);
  
  /* Be sure the scroll region is up-to-date */
  gtk_text_view_scroll_calc_now (text_view);
  
  current_x_scroll = text_view->xoffset;
  current_y_scroll = text_view->yoffset;

  screen.x = current_x_scroll;
  screen.y = current_y_scroll;
  screen.width = widget->allocation.width;
  screen.height = widget->allocation.height;

  {
    /* Clamp margin so it's not too large. */
    gint small_dimension = MIN (screen.width, screen.height);
    gint max_rect_dim;
    
    if (margin > (small_dimension/2 - 5)) /* 5 is arbitrary */
      margin = (small_dimension/2 - 5);

    if (margin < 0)
      margin = 0;
    
    /* make sure rectangle fits in the leftover space */

    max_rect_dim = MAX (rect.width, rect.height);
    
    if (max_rect_dim > (small_dimension - margin*2))
      margin -= max_rect_dim - (small_dimension - margin*2);
                 
    if (margin < 0)
      margin = 0;
  }

  g_assert (margin >= 0);
  
  screen.x += margin;
  screen.y += margin;

  screen.width -= margin*2;
  screen.height -= margin*2;

  screen_bottom = screen.y + screen.height;
  screen_right = screen.x + screen.width;
  
  /* Vertical scroll (only vertical gets adjusted) */

  scroll_inc = 0;
  if (rect.y < screen.y)
    {
      gint scroll_dest = rect.y;
      scroll_inc = (scroll_dest - screen.y) * percentage;
    }
  else if ((rect.y + rect.height) > screen_bottom)
    {
      gint scroll_dest = rect.y + rect.height;
      scroll_inc = (scroll_dest - screen_bottom) * percentage;
    }

  if (scroll_inc != 0)
    {
      set_adjustment_clamped (text_view->vadjustment,
			      current_y_scroll + scroll_inc);
      retval = TRUE;
    }
  
  /* Horizontal scroll */

  scroll_inc = 0;
  if (rect.x < screen.x)
    {
      gint scroll_dest = rect.x;
      scroll_inc = scroll_dest - screen.x;
    }
  else if ((rect.x + rect.width) > screen_right)
    {
      gint scroll_dest = rect.x + rect.width;
      scroll_inc = scroll_dest - screen_right;
    }

  if (scroll_inc != 0)
    {
      set_adjustment_clamped (text_view->hadjustment,
			      current_x_scroll + scroll_inc);
      retval = TRUE;
    }

  return retval;
}

gboolean
gtk_text_view_scroll_to_mark (GtkTextView *text_view,
                              GtkTextMark *mark,
                              gint mark_within_margin)
{
  g_return_val_if_fail (mark_within_margin >= 0, FALSE);
  
  return gtk_text_view_scroll_to_mark_adjusted (text_view, mark,
						mark_within_margin, 1.0);
}

static gboolean
clamp_iter_onscreen (GtkTextView *text_view, GtkTextIter *iter)
{
  GdkRectangle visible_rect;
  gtk_text_view_get_visible_rect (text_view, &visible_rect);

  return gtk_text_layout_clamp_iter_to_vrange (text_view->layout, iter,
					       visible_rect.y,
					       visible_rect.y + visible_rect.height);
}

gboolean
gtk_text_view_move_mark_onscreen (GtkTextView *text_view,
                                  GtkTextMark *mark)
{
  GtkTextIter iter;
  
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (mark != NULL, FALSE);
  
  gtk_text_buffer_get_iter_at_mark (text_view->buffer, &iter, mark);
  
  if (clamp_iter_onscreen (text_view, &iter))
    {
      gtk_text_buffer_move_mark (text_view->buffer, mark, &iter);
      return TRUE;
    }
  else
    return FALSE;
}

void
gtk_text_view_get_visible_rect (GtkTextView  *text_view,
				GdkRectangle *visible_rect)
{
  GtkWidget *widget;

  g_return_if_fail (text_view != NULL);
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  widget = GTK_WIDGET (text_view);
  
  if (visible_rect)
    {
      visible_rect->x = text_view->xoffset;
      visible_rect->y = text_view->yoffset;
      visible_rect->width = widget->allocation.width;
      visible_rect->height = widget->allocation.height;
    }
}

void
gtk_text_view_set_wrap_mode (GtkTextView *text_view,
			     GtkWrapMode  wrap_mode)
{
  g_return_if_fail (text_view != NULL);
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (text_view->wrap_mode != wrap_mode)
    {
      text_view->wrap_mode = wrap_mode;

      if (text_view->layout)
	{
	  text_view->layout->default_style->wrap_mode = wrap_mode;
	  gtk_text_layout_default_style_changed (text_view->layout);
	}
    }
}

GtkWrapMode
gtk_text_view_get_wrap_mode (GtkTextView *text_view)
{
  g_return_val_if_fail (text_view != NULL, GTK_WRAPMODE_NONE);
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), GTK_WRAPMODE_NONE);

  return text_view->wrap_mode;
}

gboolean
gtk_text_view_place_cursor_onscreen (GtkTextView *text_view)
{
  GtkTextIter insert;
  
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  
  gtk_text_buffer_get_iter_at_mark (text_view->buffer, &insert,
                                    gtk_text_buffer_get_mark (text_view->buffer,
                                                              "insert"));
  
  if (clamp_iter_onscreen (text_view, &insert))
    {
      gtk_text_buffer_place_cursor (text_view->buffer, &insert);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_text_view_destroy (GtkObject *object)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (object);

  gtk_text_view_destroy_layout (text_view);
  gtk_text_view_set_buffer (text_view, NULL);
  
  (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_text_view_finalize (GObject *object)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (object);  

  gtk_object_unref (GTK_OBJECT (text_view->hadjustment));
  gtk_object_unref (GTK_OBJECT (text_view->vadjustment));
  gtk_object_unref (GTK_OBJECT (text_view->im_context));

  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_text_view_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (object);

  switch (arg_id)
    {
    case ARG_HEIGHT_LINES:
      break;

    case ARG_WIDTH_COLUMNS:
      break;

    case ARG_PIXELS_ABOVE_LINES:
      break;

    case ARG_PIXELS_BELOW_LINES:
      break;

    case ARG_PIXELS_INSIDE_WRAP:
      break;

    case ARG_EDITABLE:
      break;

    case ARG_WRAP_MODE:
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static void
gtk_text_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (object);

  switch (arg_id)
    {
    case ARG_HEIGHT_LINES:
      break;

    case ARG_WIDTH_COLUMNS:
      break;

    case ARG_PIXELS_ABOVE_LINES:
      break;

    case ARG_PIXELS_BELOW_LINES:
      break;

    case ARG_PIXELS_INSIDE_WRAP:
      break;

    case ARG_EDITABLE:
      break;

    case ARG_WRAP_MODE:
      break;

    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_text_view_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  /* Hrm */
  requisition->width = 1;
  requisition->height = 1;
}

static void
gtk_text_view_size_allocate (GtkWidget *widget,
                             GtkAllocation *allocation)
{
  GtkTextView *text_view;
  GtkTextIter first_para;
  gint y;
  GtkAdjustment *vadj;
  gboolean yoffset_changed = FALSE;

  text_view = GTK_TEXT_VIEW (widget);
  
  widget->allocation = *allocation;
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gdk_window_resize (text_view->bin_window,
			 allocation->width, allocation->height);
    }

  gtk_text_view_ensure_layout (text_view);
  gtk_text_layout_set_screen_width (text_view->layout,
                                    GTK_WIDGET (text_view)->allocation.width);
      
  gtk_text_view_validate_onscreen (text_view);
  gtk_text_view_scroll_calc_now (text_view);

  /* Now adjust the value of the adjustment to keep the cursor at the same place in
   * the buffer 
  */
  gtk_text_view_get_first_para_iter (text_view, &first_para);
  y = gtk_text_layout_get_line_y (text_view->layout, &first_para) + text_view->first_para_pixels;

  vadj = text_view->vadjustment;
  if (y > vadj->upper - vadj->page_size)
    y = MAX (0, vadj->upper - vadj->page_size);

  if (y != text_view->yoffset)
    {
      vadj->value = text_view->yoffset = y;
      yoffset_changed = TRUE;
    }

  text_view->hadjustment->page_size = allocation->width;
  text_view->hadjustment->page_increment = allocation->width / 2;
  text_view->hadjustment->lower = 0;
  text_view->hadjustment->upper = MAX (allocation->width, text_view->width);
  gtk_signal_emit_by_name (GTK_OBJECT (text_view->hadjustment), "changed");

  text_view->vadjustment->page_size = allocation->height;
  text_view->vadjustment->page_increment = allocation->height / 2;
  text_view->vadjustment->lower = 0;
  text_view->vadjustment->upper = MAX (allocation->height, text_view->height);
  gtk_signal_emit_by_name (GTK_OBJECT (text_view->vadjustment), "changed");

  if (yoffset_changed)
    gtk_adjustment_value_changed (vadj);
}

static void
gtk_text_view_get_first_para_iter (GtkTextView *text_view,
				   GtkTextIter *iter)
{
  gtk_text_buffer_get_iter_at_mark (text_view->buffer, iter,
                                    text_view->first_para_mark);
}

static void
gtk_text_view_validate_onscreen (GtkTextView *text_view)
{
  GtkWidget *widget = GTK_WIDGET (text_view);
  
  if (widget->allocation.height > 0)
    {
      GtkTextIter first_para;
      gtk_text_view_get_first_para_iter (text_view, &first_para);
      gtk_text_layout_validate_yrange (text_view->layout,
				       &first_para,
				       0, text_view->first_para_pixels + widget->allocation.height);
    }
}

static gboolean
first_validate_callback (gpointer data)
{
  GtkTextView *text_view = data;

  gtk_text_view_validate_onscreen (text_view);
  
  text_view->first_validate_idle = 0;
  return FALSE;
}

static gboolean
incremental_validate_callback (gpointer data)
{
  GtkTextView *text_view = data;

  gtk_text_layout_validate (text_view->layout, 2000);
  if (gtk_text_layout_is_valid (text_view->layout))
    {
      text_view->incremental_validate_idle = 0;
      return FALSE;
    }
  else
    return TRUE;
}

static void
invalidated_handler (GtkTextLayout *layout,
		     gpointer       data)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (data);

  if (!text_view->first_validate_idle)
    text_view->first_validate_idle = g_idle_add_full (GTK_PRIORITY_RESIZE - 1, first_validate_callback, text_view, NULL);

  if (!text_view->incremental_validate_idle)
    text_view->incremental_validate_idle = g_idle_add_full (GDK_PRIORITY_REDRAW + 1, incremental_validate_callback, text_view, NULL);
}

static void
changed_handler (GtkTextLayout *layout,
		 gint           start_y,
		 gint           old_height,
		 gint           new_height,
		 gpointer       data)
{
  GtkTextView *text_view;
  GtkWidget *widget;
  GdkRectangle visible_rect;
  GdkRectangle redraw_rect;
  
  text_view = GTK_TEXT_VIEW (data);
  widget = GTK_WIDGET (data);

  if (GTK_WIDGET_REALIZED (text_view))
    {
      gtk_text_view_get_visible_rect (text_view, &visible_rect);

      redraw_rect.x = visible_rect.x;
      redraw_rect.width = visible_rect.width;
      redraw_rect.y = start_y;

      if (old_height == new_height)
	redraw_rect.height = old_height;
      else
	redraw_rect.height = MAX (0, visible_rect.y + visible_rect.height - start_y);
      
      if (gdk_rectangle_intersect (&redraw_rect, &visible_rect, &redraw_rect))
	{
	  redraw_rect.y -= text_view->yoffset;
	  gdk_window_invalidate_rect (text_view->bin_window, &redraw_rect, FALSE);
	}
    }

  if (old_height != new_height)
    {
      gboolean yoffset_changed = FALSE;

      if (start_y + old_height <= text_view->yoffset - text_view->first_para_pixels)
	{
	  text_view->yoffset += new_height - old_height;
	  text_view->vadjustment->value = text_view->yoffset;
	  yoffset_changed = TRUE;
	}

      gtk_text_view_scroll_calc_now (text_view);

      if (yoffset_changed)
	gtk_adjustment_value_changed (text_view->vadjustment);
	
    }
}

static void
gtk_text_view_realize (GtkWidget *widget)
{
  GtkTextView *text_view;
  GdkCursor *cursor;
  GdkWindowAttr attributes;
  gint attributes_mask;
    
  text_view = GTK_TEXT_VIEW (widget);
  GTK_WIDGET_SET_FLAGS (text_view, GTK_REALIZED);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.event_mask = (GDK_EXPOSURE_MASK            |
			   GDK_SCROLL_MASK              |
			   GDK_KEY_PRESS_MASK           |
			   GDK_BUTTON_PRESS_MASK        |
			   GDK_BUTTON_RELEASE_MASK      |
			   GDK_POINTER_MOTION_MASK      |
			   GDK_POINTER_MOTION_HINT_MASK |
			   gtk_widget_get_events (widget));

  text_view->bin_window = gdk_window_new (widget->window,
					&attributes, attributes_mask);
  gdk_window_show (text_view->bin_window);
  gdk_window_set_user_data (text_view->bin_window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_background (text_view->bin_window, &widget->style->base[GTK_WIDGET_STATE (widget)]);

  gtk_text_view_ensure_layout (text_view);

  /* I-beam cursor */
  cursor = gdk_cursor_new (GDK_XTERM);
  gdk_window_set_cursor (text_view->bin_window, cursor);
  gdk_cursor_destroy (cursor);

  gtk_im_context_set_client_window (text_view->im_context, widget->window);
}

static void
gtk_text_view_unrealize (GtkWidget *widget)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);

  if (text_view->first_validate_idle)
    {
      g_source_remove (text_view->first_validate_idle);
      text_view->first_validate_idle = 0;
    }
    
  if (text_view->incremental_validate_idle)
    {
      g_source_remove (text_view->incremental_validate_idle);
      text_view->incremental_validate_idle = 0;
    }
    
  gtk_text_view_destroy_layout (text_view);
  
  gtk_im_context_set_client_window (text_view->im_context, NULL);

  gdk_window_set_user_data (text_view->bin_window, NULL);
  gdk_window_destroy (text_view->bin_window);
  text_view->bin_window = NULL;

  (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void 
gtk_text_view_style_set (GtkWidget *widget,
			 GtkStyle  *previous_style)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_set_background (text_view->bin_window,
				 &widget->style->base[GTK_WIDGET_STATE (widget)]);

      gtk_text_view_set_values_from_style (text_view, text_view->layout->default_style, widget->style);
      gtk_text_layout_default_style_changed (text_view->layout);
    }
}

static void 
gtk_text_view_direction_changed (GtkWidget        *widget,
				 GtkTextDirection  previous_direction)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);

  if (text_view->layout)
    {
      text_view->layout->default_style->direction = gtk_widget_get_direction (widget);
      gtk_text_layout_default_style_changed (text_view->layout);
    }
}

/*
 * Events
 */

static gboolean
get_event_coordinates (GdkEvent *event, gint *x, gint *y)
{
  if (event)
    switch (event->type)
      {
      case GDK_MOTION_NOTIFY:
        *x = event->motion.x;
        *y = event->motion.y;
        return TRUE;
        break;
        
      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
        *x = event->button.x;
        *y = event->button.y;
        return TRUE;
        break;
        
      case GDK_KEY_PRESS:
      case GDK_KEY_RELEASE:
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
      case GDK_PROPERTY_NOTIFY:
      case GDK_SELECTION_CLEAR:
      case GDK_SELECTION_REQUEST:
      case GDK_SELECTION_NOTIFY:
      case GDK_PROXIMITY_IN:
      case GDK_PROXIMITY_OUT:
      case GDK_DRAG_ENTER:
      case GDK_DRAG_LEAVE:
      case GDK_DRAG_MOTION:
      case GDK_DRAG_STATUS:
      case GDK_DROP_START:
      case GDK_DROP_FINISHED:
      default:
        return FALSE;
	break;
      }

  return FALSE;
}

static gint
gtk_text_view_event (GtkWidget *widget, GdkEvent *event)
{
  GtkTextView *text_view;
  gint x = 0, y = 0;
  
  text_view = GTK_TEXT_VIEW (widget);
  
  if (text_view->layout == NULL ||
      text_view->buffer == NULL)
    return FALSE;

  /* FIXME eventually we really want to synthesize enter/leave
     events here as the canvas does for canvas items */
  
  if (get_event_coordinates (event, &x, &y))
    {
      GtkTextIter iter;
      gint retval = FALSE;

      x += text_view->xoffset;
      y += text_view->yoffset;

      /* FIXME this is slow and we do it twice per event.
         My favorite solution is to have GtkTextLayout cache
         the last couple lookups. */
      gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                         &iter,
                                         x, y);

        {
          GSList *tags;
          GSList *tmp;
          
          tags = gtk_text_buffer_get_tags (text_view->buffer, &iter);
          
          tmp = tags;
          while (tmp != NULL)
            {
              GtkTextTag *tag = tmp->data;

              if (gtk_text_tag_event (tag, GTK_OBJECT (widget), event, &iter))
                {
                  retval = TRUE;
                  break;
                }

              tmp = g_slist_next (tmp);
            }

          g_slist_free (tags);
        }
        
      return retval;
    }

  return FALSE;
}

static gint
gtk_text_view_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);

  if (text_view->layout == NULL ||
      text_view->buffer == NULL)
    return FALSE;

  if (GTK_WIDGET_CLASS (parent_class)->key_press_event &&
      GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event))
    return TRUE;

  if (gtk_im_context_filter_keypress (text_view->im_context, event))
    return TRUE;
  else if (event->keyval == GDK_Return)
    {
      gtk_text_buffer_insert_at_cursor (text_view->buffer, "\n", 1);
      gtk_text_view_scroll_to_mark (text_view,
                                    gtk_text_buffer_get_mark (text_view->buffer,
                                                              "insert"),
                                    0);
      return TRUE;
    }
  else
    return FALSE;
}

static gint
gtk_text_view_key_release_event (GtkWidget *widget, GdkEventKey *event)
{
  return FALSE;
}

static gint
gtk_text_view_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);
  
  gtk_widget_grab_focus (widget);

  /* debug hack */
  if (event->button == 3 && (event->state & GDK_CONTROL_MASK) != 0)
    gtk_text_buffer_spew (GTK_TEXT_VIEW (widget)->buffer);
  else if (event->button == 3)
    gtk_text_layout_spew (GTK_TEXT_VIEW (widget)->layout);

  if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 1)
        {
          /* If we're in the selection, start a drag copy/move of the
             selection; otherwise, start creating a new selection. */
          GtkTextIter iter;
          GtkTextIter start, end;

          gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                             &iter,
                                             event->x + text_view->xoffset,
                                             event->y + text_view->yoffset);
          
          if (gtk_text_buffer_get_selection_bounds (text_view->buffer,
                                                    &start, &end) &&
              gtk_text_iter_in_region (&iter, &start, &end))
            {
              gtk_text_view_start_selection_dnd (text_view, &iter, event);
            }
          else
            {
              gtk_text_view_start_selection_drag (text_view, &iter, event);
            }
          
          return TRUE;
        }
      else if (event->button == 2)
        {
          GtkTextIter iter;

          gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                             &iter,
                                             event->x + text_view->xoffset,
                                             event->y + text_view->yoffset);
          
          gtk_text_buffer_paste_primary_selection (text_view->buffer,
                                                   &iter,
                                                   event->time);
          return TRUE;
        }
      else if (event->button == 3)
        {
          if (gtk_text_view_end_selection_drag (text_view, event))
            return TRUE;
          else
            return FALSE;
        }
    }
  
  return FALSE;
}

static gint
gtk_text_view_button_release_event (GtkWidget *widget, GdkEventButton *event)
{
  if (event->button == 1)
    {
      gtk_text_view_end_selection_drag (GTK_TEXT_VIEW (widget), event);
      return TRUE;
    }

  return FALSE;
}

static gint
gtk_text_view_focus_in_event (GtkWidget *widget, GdkEventFocus *event)
{
  GtkTextMark *insert;
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);

  insert = gtk_text_buffer_get_mark (GTK_TEXT_VIEW (widget)->buffer,
                                     "insert");
  gtk_text_mark_set_visible (insert, TRUE);

  gtk_text_view_start_cursor_blink (GTK_TEXT_VIEW (widget));

  gtk_im_context_focus_in (GTK_TEXT_VIEW (widget)->im_context);
  
  return FALSE;
}

static gint
gtk_text_view_focus_out_event (GtkWidget *widget, GdkEventFocus *event)
{
  GtkTextMark *insert;
  
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);

  insert = gtk_text_buffer_get_mark (GTK_TEXT_VIEW (widget)->buffer,
                                     "insert");
  gtk_text_mark_set_visible (insert, FALSE);

  gtk_text_view_stop_cursor_blink (GTK_TEXT_VIEW (widget));

  gtk_im_context_focus_out (GTK_TEXT_VIEW (widget)->im_context);
  
  return FALSE;
}

static gint
gtk_text_view_motion_event (GtkWidget *widget, GdkEventMotion *event)
{
  
  return FALSE;
}

static void
gtk_text_view_paint (GtkWidget *widget, GdkRectangle *area)
{
  GtkTextView *text_view;
  
  text_view = GTK_TEXT_VIEW (widget);

  g_return_if_fail (text_view->layout != NULL);
  g_return_if_fail (text_view->xoffset >= 0);
  g_return_if_fail (text_view->yoffset >= 0);

  gtk_text_view_validate_onscreen (text_view);

#if 0
  printf ("painting %d,%d  %d x %d\n",
         area->x, area->y,
         area->width, area->height);
#endif  

  gtk_text_layout_draw (text_view->layout,
                        widget,
                        text_view->bin_window,
                        text_view->xoffset, text_view->yoffset,
			area->x, area->y,
			area->width, area->height);
}

static void
gtk_text_view_draw (GtkWidget *widget, GdkRectangle *area)
{  
  gtk_text_view_paint (widget, area);
}

static gint
gtk_text_view_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  if (event->window == GTK_TEXT_VIEW (widget)->bin_window)
    gtk_text_view_paint (widget, &event->area);
  
  return TRUE;
}

/*
 * Blink!
 */

static gint
blink_cb (gpointer data)
{
  GtkTextView *text_view;
  GtkTextMark *insert;
  
  text_view = GTK_TEXT_VIEW (data);

  insert = gtk_text_buffer_get_mark (text_view->buffer,
                                     "insert");
  
  if (!GTK_WIDGET_HAS_FOCUS (text_view))
    {
      /* paranoia, in case the user somehow mangles our
         focus_in/focus_out pairing. */
      gtk_text_mark_set_visible (insert, FALSE);
      text_view->blink_timeout = 0;
      return FALSE;
    }
  else
    {
      gtk_text_mark_set_visible (insert,
                                 !gtk_text_mark_is_visible (insert));
      return TRUE;
    }
}

static void
gtk_text_view_start_cursor_blink (GtkTextView *text_view)
{
  return;
  if (text_view->blink_timeout != 0)
    return;

  text_view->blink_timeout = gtk_timeout_add (500, blink_cb, text_view);
}

static void
gtk_text_view_stop_cursor_blink (GtkTextView *text_view)
{
  return;
  if (text_view->blink_timeout == 0)
    return;

  gtk_timeout_remove (text_view->blink_timeout);
  text_view->blink_timeout = 0;
}

/*
 * Key binding handlers
 */

static void
gtk_text_view_move_iter_by_lines (GtkTextView *text_view,
				  GtkTextIter *newplace,
				  gint         count)
{
  while (count < 0)
    {
      gtk_text_layout_move_iter_to_previous_line (text_view->layout, newplace);
      count++;
    }

  while (count > 0)
    {
      gtk_text_layout_move_iter_to_next_line (text_view->layout, newplace);
      count--;
    }
}

static void
gtk_text_view_move_insert (GtkTextView *text_view,
			   GtkTextViewMovementStep step,
			   gint count,
			   gboolean extend_selection)
{
  GtkTextIter insert;
  GtkTextIter newplace;
  
  gint cursor_x_pos = 0;

  gtk_text_buffer_get_iter_at_mark (text_view->buffer, &insert,
                                    gtk_text_buffer_get_mark (text_view->buffer,
                                                              "insert"));
  newplace = insert;

  if (step == GTK_TEXT_MOVEMENT_LINE)
    gtk_text_view_get_virtual_cursor_pos (text_view, &cursor_x_pos, NULL);

  switch (step)
    {
    case GTK_TEXT_MOVEMENT_CHAR:
      gtk_text_iter_forward_chars (&newplace, count);
      break;

    case GTK_TEXT_MOVEMENT_POSITIONS:
      gtk_text_layout_move_iter_visually (text_view->layout,
					  &newplace, count);
      break;

    case GTK_TEXT_MOVEMENT_WORD:
      if (count < 0)
	gtk_text_iter_backward_word_starts (&newplace, -count);
      else if (count > 0)
	gtk_text_iter_forward_word_ends (&newplace, count);
      break;

    case GTK_TEXT_MOVEMENT_LINE:
      gtk_text_view_move_iter_by_lines (text_view, &newplace, count);
      gtk_text_layout_move_iter_to_x (text_view->layout, &newplace, cursor_x_pos);
      break;
      
    case GTK_TEXT_MOVEMENT_PARAGRAPH:
      /* This should almost certainly instead be doing the parallel thing to WORD */
      gtk_text_iter_down_lines (&newplace, count);
      break;
      
    case GTK_TEXT_MOVEMENT_PARAGRAPH_ENDS:
      if (count > 0)
	gtk_text_iter_forward_to_newline (&newplace);
      else if (count < 0)
	gtk_text_iter_set_line_char (&newplace, 0);
      break;
      
    case GTK_TEXT_MOVEMENT_BUFFER_ENDS:
      if (count > 0)
	gtk_text_buffer_get_last_iter (text_view->buffer, &newplace);
      else if (count < 0)
	gtk_text_buffer_get_iter_at_char (text_view->buffer, &newplace, 0);
      break;
      
    default:
      break;
    }
  
  if (!gtk_text_iter_equal (&insert, &newplace))
    {
      if (extend_selection)
	gtk_text_buffer_move_mark (text_view->buffer,
                                   gtk_text_buffer_get_mark (text_view->buffer,
                                                             "insert"),
                                   &newplace);
      else
	gtk_text_buffer_place_cursor (text_view->buffer, &newplace);
      
      gtk_text_view_scroll_to_mark (text_view,
                                    gtk_text_buffer_get_mark (text_view->buffer,
                                                              "insert"), 0);

      if (step == GTK_TEXT_MOVEMENT_LINE)
	{
	  gtk_text_view_set_virtual_cursor_pos (text_view, cursor_x_pos, -1);
	}
    }
}

static void
gtk_text_view_set_anchor (GtkTextView *text_view)
{
  GtkTextIter insert;
  
  gtk_text_buffer_get_iter_at_mark (text_view->buffer, &insert,
                                    gtk_text_buffer_get_mark (text_view->buffer,
                                                              "insert"));

  gtk_text_buffer_create_mark (text_view->buffer, "anchor", &insert, TRUE);
}

static void
gtk_text_view_scroll_text (GtkTextView *text_view,
			   GtkTextViewScrollType type)
{
  gfloat newval;
  GtkAdjustment *adj;
  gint cursor_x_pos, cursor_y_pos;
  GtkTextIter new_insert;
  GtkTextIter anchor;
  gint y0, y1;
  
  g_return_if_fail (text_view->vadjustment != NULL);

  adj = text_view->vadjustment;

  /* Validate the region that will be brought into view by the cursor motion
   */
  switch (type)
    {
    default:
    case GTK_TEXT_SCROLL_TO_TOP:
      gtk_text_buffer_get_iter_at_char (text_view->buffer, &anchor, 0);
      y0 = 0;
      y1 = adj->page_size;
      break;

    case GTK_TEXT_SCROLL_TO_BOTTOM:
      gtk_text_buffer_get_last_iter (text_view->buffer, &anchor);
      y0 = -adj->page_size;
      y1 = adj->page_size;
      break;

    case GTK_TEXT_SCROLL_PAGE_DOWN:
      gtk_text_view_get_first_para_iter (text_view, &anchor);
      y0 = adj->page_size;
      y1 = adj->page_size + adj->page_increment;
      break;

    case GTK_TEXT_SCROLL_PAGE_UP:
      gtk_text_view_get_first_para_iter (text_view, &anchor);
      y0 = - adj->page_increment + adj->page_size;
      y1 = 0;
      break;
    }
  gtk_text_layout_validate_yrange (text_view->layout, &anchor, y0, y1);


  gtk_text_view_get_virtual_cursor_pos (text_view, &cursor_x_pos, &cursor_y_pos);

  newval = adj->value;
  switch (type)
    {
    case GTK_TEXT_SCROLL_TO_TOP:
      newval = adj->lower;
      break;

    case GTK_TEXT_SCROLL_TO_BOTTOM:
      newval = adj->upper;
      break;

    case GTK_TEXT_SCROLL_PAGE_DOWN:
      newval += adj->page_increment;
      break;

    case GTK_TEXT_SCROLL_PAGE_UP:
      newval -= adj->page_increment;
      break;
      
    default:
      break;
    }

  cursor_y_pos += newval - adj->value;
  set_adjustment_clamped (adj, newval);

  gtk_text_layout_get_iter_at_pixel (text_view->layout, &new_insert, cursor_x_pos, cursor_y_pos);
  clamp_iter_onscreen (text_view, &new_insert);
  gtk_text_buffer_place_cursor (text_view->buffer, &new_insert);

  gtk_text_view_set_virtual_cursor_pos (text_view, cursor_x_pos, cursor_y_pos);

  /* Adjust to have the cursor _entirely_ onscreen, move_mark_onscreen
   * only guarantees 1 pixel onscreen.
   */
  gtk_text_view_scroll_to_mark (text_view,
                                gtk_text_buffer_get_mark (text_view->buffer,
                                                          "insert"),
                                0);
}

static gboolean
whitespace (gunichar ch, gpointer user_data)
{
  return (ch == ' ' || ch == '\t');
}

static gboolean
not_whitespace (gunichar ch, gpointer user_data)
{
  return !whitespace (ch, user_data);
}

static gboolean
find_whitepace_region (const GtkTextIter *center,
                      GtkTextIter *start, GtkTextIter *end)
{
  *start = *center;
  *end = *center;

  if (gtk_text_iter_backward_find_char (start, not_whitespace, NULL))
    gtk_text_iter_forward_char (start); /* we want the first whitespace... */
  if (whitespace (gtk_text_iter_get_char (end), NULL))
    gtk_text_iter_forward_find_char (end, not_whitespace, NULL);
  
  return !gtk_text_iter_equal (start, end);
}

static void
gtk_text_view_delete_text (GtkTextView *text_view,
                       GtkTextViewDeleteType type,
                       gint count)
{
  GtkTextIter insert;
  GtkTextIter start;
  GtkTextIter end;
  gboolean leave_one = FALSE;
  
  if (type == GTK_TEXT_DELETE_CHAR)
    {
      /* Char delete deletes the selection, if one exists */
      if (gtk_text_buffer_delete_selection (text_view->buffer))
        return;
    }
  
  gtk_text_buffer_get_iter_at_mark (text_view->buffer,
                                    &insert,
                                    gtk_text_buffer_get_mark (text_view->buffer,
                                                              "insert"));

  start = insert;
  end = insert;
  
  switch (type)
    {
    case GTK_TEXT_DELETE_CHAR:
      gtk_text_iter_forward_chars (&end, count);
      break;
      
    case GTK_TEXT_DELETE_HALF_WORD:
      if (count > 0)
        gtk_text_iter_forward_word_ends (&end, count);
      else if (count < 0)
        gtk_text_iter_backward_word_starts (&start, 0 - count);
      break;
      
    case GTK_TEXT_DELETE_WHOLE_WORD:
      break;
      
    case GTK_TEXT_DELETE_HALF_LINE:
      break;

    case GTK_TEXT_DELETE_WHOLE_LINE:
      break;

    case GTK_TEXT_DELETE_HALF_PARAGRAPH:
      while (count > 0)
        {
          if (!gtk_text_iter_forward_to_newline (&end))
            break;

          --count;
        }

      /* FIXME figure out what a negative count means
         and support that */
      break;
      
    case GTK_TEXT_DELETE_WHOLE_PARAGRAPH:
      if (count > 0)
        {
          gtk_text_iter_set_line_char (&start, 0);
          gtk_text_iter_forward_to_newline (&end);

          /* Do the lines beyond the first. */
          while (count > 1)
            {
              gtk_text_iter_forward_to_newline (&end);
              
              --count;
            }
        }

      /* FIXME negative count? */
      
      break;

    case GTK_TEXT_DELETE_WHITESPACE_LEAVE_ONE:
      leave_one = TRUE; /* FALL THRU */
    case GTK_TEXT_DELETE_WHITESPACE:
      {
        find_whitepace_region (&insert, &start, &end);
      }
      break;
      
    default:
      break;
    }

  if (!gtk_text_iter_equal (&start, &end))
    {
      gtk_text_buffer_delete (text_view->buffer, &start, &end);
      
      if (leave_one)
        gtk_text_buffer_insert_at_cursor (text_view->buffer, " ", 1);
      
      gtk_text_view_scroll_to_mark (text_view,
                                    gtk_text_buffer_get_mark (text_view->buffer, "insert"),
                                    0);
    }
}

static void
gtk_text_view_cut_text (GtkTextView *text_view)
{
  gtk_text_buffer_cut (text_view->buffer, GDK_CURRENT_TIME);
  gtk_text_view_scroll_to_mark (text_view,
                                gtk_text_buffer_get_mark (text_view->buffer,
                                                          "insert"),
                                0);
}

static void
gtk_text_view_copy_text (GtkTextView *text_view)
{
  gtk_text_buffer_copy (text_view->buffer, GDK_CURRENT_TIME);
  gtk_text_view_scroll_to_mark (text_view,
                                gtk_text_buffer_get_mark (text_view->buffer,
                                                          "insert"),
                                0);
}

static void
gtk_text_view_paste_text (GtkTextView *text_view)
{
  gtk_text_buffer_paste_clipboard (text_view->buffer, GDK_CURRENT_TIME);
  gtk_text_view_scroll_to_mark (text_view,
                                gtk_text_buffer_get_mark (text_view->buffer,
                                                          "insert"),
                                0);
}

static void
gtk_text_view_toggle_overwrite (GtkTextView *text_view)
{
  text_view->overwrite_mode = !text_view->overwrite_mode;
}

/*
 * Selections
 */

static gboolean
move_insert_to_pointer_and_scroll (GtkTextView *text_view, gboolean partial_scroll)
{
  gint x, y;
  GdkModifierType state;
  GtkTextIter newplace;
  gint adjust = 0;
  gboolean in_threshold = FALSE;
  
  gdk_window_get_pointer (text_view->bin_window, &x, &y, &state);

  /* Adjust movement by how long we've been selecting, to
     get an acceleration effect. The exact numbers are
     pretty arbitrary. We have a threshold before we
     start to accelerate. */
  /* uncommenting this printf helps visualize how it works. */     
  /*   printf ("%d\n", text_view->scrolling_accel_factor); */
       
  if (text_view->scrolling_accel_factor > 10)
    adjust = (text_view->scrolling_accel_factor - 10) * 75;
  
  if (y < 0) /* scrolling upward */
    adjust = -adjust; 

  /* No adjust if the pointer has moved back inside the window for sure.
     Also I'm adding a small threshold where no adjust is added,
     in case you want to do a continuous slow scroll. */
#define SLOW_SCROLL_TH 7
  if (x >= (0 - SLOW_SCROLL_TH) &&
      x < (GTK_WIDGET (text_view)->allocation.width + SLOW_SCROLL_TH) &&
      y >= (0 - SLOW_SCROLL_TH) &&
      y < (GTK_WIDGET (text_view)->allocation.height + SLOW_SCROLL_TH))
    {
      adjust = 0;
      in_threshold = TRUE;
    }
  
  gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                     &newplace,
                                     x + text_view->xoffset,
                                     y + text_view->yoffset + adjust);
  
  {
      gboolean scrolled = FALSE;
      GtkTextMark *insert_mark =
        gtk_text_buffer_get_mark (text_view->buffer, "insert");

      gtk_text_buffer_move_mark (text_view->buffer,
                                 insert_mark,
                                 &newplace);
      
      if (partial_scroll)
        scrolled = gtk_text_view_scroll_to_mark_adjusted (text_view, insert_mark, 0, 0.7);
      else
        scrolled = gtk_text_view_scroll_to_mark_adjusted (text_view, insert_mark, 0, 1.0);

      if (scrolled)
        {
          /* We want to avoid rapid jump to super-accelerated when you
             leave the slow scroll threshold after scrolling for a
             while. So we slowly decrease accel when scrolling inside
             the threshold.
          */
          if (in_threshold)
            {
              if (text_view->scrolling_accel_factor > 1)
                text_view->scrolling_accel_factor -= 2;
            }
          else
            text_view->scrolling_accel_factor += 1;
        }
      else
        {
          /* If we don't scroll we're probably inside the window, but
             potentially just a bit outside. We decrease acceleration
             while the user is fooling around inside the window.
             Acceleration decreases faster than it increases. */
          if (text_view->scrolling_accel_factor > 4)
            text_view->scrolling_accel_factor -= 5;
        }
      
      return scrolled;
  }
}

static gint
selection_scan_timeout (gpointer data)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (data);

  if (move_insert_to_pointer_and_scroll (text_view, TRUE))
    {
      return TRUE; /* remain installed. */
    }
  else
    {
      text_view->selection_drag_scan_timeout = 0;
      return FALSE; /* remove ourselves */
    }
}

static gint
selection_motion_event_handler (GtkTextView *text_view, GdkEventMotion *event, gpointer data)
{
  if (move_insert_to_pointer_and_scroll (text_view, TRUE))
    {
      /* If we had to scroll offscreen, insert a timeout to do so
         again. Note that in the timeout, even if the mouse doesn't
         move, due to this scroll xoffset/yoffset will have changed
         and we'll need to scroll again. */
      if (text_view->selection_drag_scan_timeout != 0) /* reset on every motion event */
        gtk_timeout_remove (text_view->selection_drag_scan_timeout);
      
      text_view->selection_drag_scan_timeout =
        gtk_timeout_add (50, selection_scan_timeout, text_view);
    }
  
  return TRUE;
}

static void
gtk_text_view_start_selection_drag (GtkTextView *text_view,
                               const GtkTextIter *iter,
                               GdkEventButton *event)
{
  GtkTextIter newplace;

  g_return_if_fail (text_view->selection_drag_handler == 0);
  
  gtk_grab_add (GTK_WIDGET (text_view));

  text_view->scrolling_accel_factor = 0;

  newplace = *iter;
  
  gtk_text_buffer_place_cursor (text_view->buffer, &newplace);

  text_view->selection_drag_handler = gtk_signal_connect (GTK_OBJECT (text_view),
							  "motion_notify_event",
							  GTK_SIGNAL_FUNC (selection_motion_event_handler),
							  NULL);
}

/* returns whether we were really dragging */
static gboolean
gtk_text_view_end_selection_drag (GtkTextView *text_view, GdkEventButton *event)
{
  if (text_view->selection_drag_handler == 0)
    return FALSE;

  gtk_signal_disconnect (GTK_OBJECT (text_view), text_view->selection_drag_handler);
  text_view->selection_drag_handler = 0;

  text_view->scrolling_accel_factor = 0;
  
  if (text_view->selection_drag_scan_timeout != 0)
    {
      gtk_timeout_remove (text_view->selection_drag_scan_timeout);
      text_view->selection_drag_scan_timeout = 0;
    }

  /* one last update to current position */
  move_insert_to_pointer_and_scroll (text_view, FALSE);
  
  gtk_grab_remove (GTK_WIDGET (text_view));
  
  return TRUE;
}

/*
 * Layout utils
 */

static void
gtk_text_view_set_adjustment_upper (GtkAdjustment *adj, gfloat upper)
{
  if (upper != adj->upper)
    {
      gfloat min = MAX (0., upper - adj->page_size);
      gboolean value_changed = FALSE;
      
      adj->upper = upper;
      
      if (adj->value > min)
	{
	  adj->value = min;
	  value_changed = TRUE;
	}
      
      gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");
      if (value_changed)
	gtk_signal_emit_by_name (GTK_OBJECT (adj), "value_changed");
    }
}

static void
gtk_text_view_scroll_calc_now (GtkTextView *text_view)
{
  gint width = 0, height = 0;
  GtkWidget *widget = GTK_WIDGET (text_view);
  
  gtk_text_view_ensure_layout (text_view);

      
  gtk_text_layout_set_screen_width (text_view->layout,
                                    widget->allocation.width);
      
  gtk_text_layout_get_size (text_view->layout, &width, &height);

#if 0
  /* If the width is less than the screen width (likely
     if we have wrapping turned on for the whole widget),
     then we want to set the scroll region to the screen
     width. If the width is greater (wrapping off) then we
     probably want to set the scroll region to the width
     of the layout. I guess.
  */

  width = MAX (text_view->layout->screen_width, width);
  height = height;
#endif

  if (text_view->width != width || text_view->height != height)
    {
#if 0
      printf ("layout size set, widget width is %d\n",
	      GTK_WIDGET (text_view)->allocation.width);
#endif
      text_view->width = width;
      text_view->height = height;
      
      gtk_text_view_set_adjustment_upper (text_view->hadjustment,
					  MAX (widget->allocation.width, width));
      gtk_text_view_set_adjustment_upper (text_view->vadjustment, 
					  MAX (widget->allocation.height, height));

      /* Set up the step sizes; we'll say that a page is
         our allocation minus one step, and a step is
         1/10 of our allocation. */
      text_view->hadjustment->step_increment =
        GTK_WIDGET (text_view)->allocation.width/10.0;
      text_view->hadjustment->page_increment =
        GTK_WIDGET (text_view)->allocation.width  *0.9;

      text_view->vadjustment->step_increment =
        GTK_WIDGET (text_view)->allocation.height/10.0;
      text_view->vadjustment->page_increment =
        GTK_WIDGET (text_view)->allocation.height  *0.9;
    } 
}

static void
gtk_text_view_set_values_from_style (GtkTextView        *text_view,
				     GtkTextStyleValues *values,
				     GtkStyle           *style)
{
  values->appearance.bg_color = style->base[GTK_STATE_NORMAL];
  values->appearance.fg_color = style->fg[GTK_STATE_NORMAL];
      
  if (values->font_desc)
    pango_font_description_free (values->font_desc);

  values->font_desc = pango_font_description_copy (style->font_desc);
}

static void
gtk_text_view_ensure_layout (GtkTextView *text_view)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (text_view);
  
  if (text_view->layout == NULL)
    {
      GtkTextStyleValues *style;
      PangoContext *ltr_context, *rtl_context;
      
      text_view->layout = gtk_text_layout_new ();

      gtk_signal_connect (GTK_OBJECT (text_view->layout),
			  "invalidated",
			  GTK_SIGNAL_FUNC (invalidated_handler),
			  text_view);

      gtk_signal_connect (GTK_OBJECT (text_view->layout),
			  "changed",
			  GTK_SIGNAL_FUNC (changed_handler),
			  text_view);
      
      if (text_view->buffer)
        gtk_text_layout_set_buffer (text_view->layout, text_view->buffer);

      ltr_context = gtk_widget_create_pango_context (GTK_WIDGET (text_view));
      pango_context_set_base_dir (ltr_context, PANGO_DIRECTION_LTR);
      rtl_context = gtk_widget_create_pango_context (GTK_WIDGET (text_view));
      pango_context_set_base_dir (rtl_context, PANGO_DIRECTION_RTL);

      gtk_text_layout_set_contexts (text_view->layout, ltr_context, rtl_context);

      g_object_unref (G_OBJECT (ltr_context));
      g_object_unref (G_OBJECT (rtl_context));

      style = gtk_text_style_values_new ();

      gtk_widget_ensure_style (widget);
      gtk_text_view_set_values_from_style (text_view, style, widget->style);
      
      style->pixels_above_lines = 2;
      style->pixels_below_lines = 2;
      style->pixels_inside_wrap = 1;
      
      style->wrap_mode = text_view->wrap_mode;
      style->justify = GTK_JUSTIFY_LEFT;
      style->direction = gtk_widget_get_direction (GTK_WIDGET (text_view));
      
      gtk_text_layout_set_default_style (text_view->layout, style);
      
      gtk_text_style_values_unref (style);
    }
}

static void
gtk_text_view_destroy_layout (GtkTextView *text_view)
{
  if (text_view->layout)
    {
      gtk_text_view_end_selection_drag (text_view, NULL);
      
      gtk_signal_disconnect_by_func (GTK_OBJECT (text_view->layout),
				     invalidated_handler, text_view);
      gtk_signal_disconnect_by_func (GTK_OBJECT (text_view->layout),
				     changed_handler, text_view);
      gtk_object_unref (GTK_OBJECT (text_view->layout));
      text_view->layout = NULL;
    }
}


/*
 * DND feature
 */

static void
gtk_text_view_start_selection_dnd (GtkTextView *text_view,
                              const GtkTextIter *iter,
                              GdkEventButton *event)
{
  GdkDragContext *context;
  GtkTargetList *target_list;
  
  /* FIXME we have to handle more formats for the selection,
     and do the conversions to/from UTF8 */
  
  /* FIXME not sure how this is memory-managed. */
  target_list = gtk_target_list_new (target_table, n_targets);
  
  context = gtk_drag_begin (GTK_WIDGET (text_view), target_list,
                           GDK_ACTION_COPY | GDK_ACTION_MOVE,
                           1, (GdkEvent*)event);

  gtk_drag_set_icon_default (context);

  /* We're inside the selection, so start without being able
     to accept the drag. */
  gdk_drag_status (context, 0, event->time);
  gtk_text_mark_set_visible (text_view->dnd_mark, FALSE);
}

static void
gtk_text_view_drag_begin (GtkWidget        *widget,
                      GdkDragContext   *context)
{
  
}

static void
gtk_text_view_drag_end (GtkWidget        *widget,
                    GdkDragContext   *context)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);

  gtk_text_mark_set_visible (text_view->dnd_mark, FALSE);
}

static void
gtk_text_view_drag_data_get (GtkWidget        *widget,
                         GdkDragContext   *context,
                         GtkSelectionData *selection_data,
                         guint             info,
                         guint             time)
{
  gchar *str;
  gint length;
  GtkTextIter start;
  GtkTextIter end;
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);
  
  str = NULL;
  length = 0;
  
  if (gtk_text_buffer_get_selection_bounds (text_view->buffer, &start, &end))
    {
      /* Extract the selected text */
      str = gtk_text_iter_get_visible_text (&start, &end);
      
      length = strlen (str);
    }

  if (str)
    {
      if (info == TARGET_UTF8_STRING)
        {
          /* Pass raw UTF8 */
          gtk_selection_data_set (selection_data,
                                  utf8_atom,
                                  8*sizeof (gchar), (guchar *)str, length);

        }
      else if (info == TARGET_STRING ||
               info == TARGET_TEXT)
        {
          gchar *latin1;

          latin1 = gtk_text_utf_to_latin1(str, length);
          
          gtk_selection_data_set (selection_data,
                                  GDK_SELECTION_TYPE_STRING,
                                  8*sizeof (gchar), latin1, strlen (latin1));
          g_free (latin1);
        }
      else if (info == TARGET_COMPOUND_TEXT)
        {
          /* FIXME convert UTF8 directly to current locale, not via
             latin1 */
          
          guchar *text;
          GdkAtom encoding;
          gint format;
          gint new_length;
          gchar *latin1;

          latin1 = gtk_text_utf_to_latin1(str, length);
          
          gdk_string_to_compound_text (latin1, &encoding, &format, &text, &new_length);
          gtk_selection_data_set (selection_data, encoding, format, text, new_length);
          gdk_free_compound_text (text);

          g_free (latin1);
        }

      g_free (str);
    }
}

static void
gtk_text_view_drag_data_delete (GtkWidget        *widget,
                            GdkDragContext   *context)
{
  gtk_text_buffer_delete_selection (GTK_TEXT_VIEW (widget)->buffer);
}

static void
gtk_text_view_drag_leave (GtkWidget        *widget,
                      GdkDragContext   *context,
                      guint             time)
{


}

static gboolean
gtk_text_view_drag_motion (GtkWidget        *widget,
			   GdkDragContext   *context,
			   gint              x,
			   gint              y,
			   guint             time)
{
  GtkTextIter newplace;
  GtkTextView *text_view;
  GtkTextIter start;
  GtkTextIter end;
  
  text_view = GTK_TEXT_VIEW (widget);

  gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                     &newplace, 
                                     x + text_view->xoffset,
				     y + text_view->yoffset);

  if (gtk_text_buffer_get_selection_bounds (text_view->buffer,
                                            &start, &end) &&
      gtk_text_iter_in_region (&newplace, &start, &end))
    {
      /* We're inside the selection. */
      gdk_drag_status (context, 0, time);
      gtk_text_mark_set_visible (text_view->dnd_mark, FALSE);
    }
  else
    {
      gtk_text_mark_set_visible (text_view->dnd_mark, TRUE);
      
      gdk_drag_status (context, context->suggested_action, time);
    }

  gtk_text_buffer_move_mark (text_view->buffer,
                             gtk_text_buffer_get_mark (text_view->buffer,
                                                       "__drag_target"),
                             &newplace);

  {
    /* The effect of this is that the text scrolls if you're near
       the edge. We have to scroll whether or not we're inside
       the selection. */
    gint margin;
    
    margin = MIN (widget->allocation.width, widget->allocation.height);
    margin /= 5;
    
    gtk_text_view_scroll_to_mark_adjusted (text_view,
                                           gtk_text_buffer_get_mark (text_view->buffer,
                                                                     "__drag_target"),
                                           margin, 1.0);
  }
  
  return TRUE;
}

static gboolean
gtk_text_view_drag_drop (GtkWidget        *widget,
			 GdkDragContext   *context,
			 gint              x,
			 gint              y,
			 guint             time)
{
#if 0
  /* called automatically. */
  if (context->targets)
    {
      gtk_drag_get_data (widget, context, 
			 GPOINTER_TO_INT (context->targets->data), 
			 time);
      return TRUE;
    }
  else
    return FALSE;
#endif
  return TRUE;
}

static void
gtk_text_view_drag_data_received (GtkWidget        *widget,
				  GdkDragContext   *context,
				  gint              x,
				  gint              y,
				  GtkSelectionData *selection_data,
				  guint             info,
				  guint             time)
{
  GtkTextIter drop_point;
  GtkTextView *text_view;
  GtkTextMark *drag_target_mark;
  
  enum {INVALID, STRING, CTEXT, UTF8} type;

  text_view = GTK_TEXT_VIEW (widget);
  
  if (selection_data->type == GDK_TARGET_STRING)
    type = STRING;
  else if (selection_data->type == ctext_atom)
    type = CTEXT;
  else if (selection_data->type == utf8_atom)
    type = UTF8;
  else
    type = INVALID;

  if (type == INVALID || selection_data->length < 0)
    {
      /* I think the DND code automatically tries asking
         for the various formats. */
      return;
    }

  drag_target_mark = gtk_text_buffer_get_mark (text_view->buffer,
                                               "__drag_target");
  
  if (drag_target_mark == NULL)
    return;

  gtk_text_buffer_get_iter_at_mark (text_view->buffer,
                                    &drop_point,
                                    drag_target_mark);

  
  switch (type)
    {
    case STRING:
      {
        gchar *utf;

        utf = gtk_text_latin1_to_utf ((const gchar*)selection_data->data,
                                      selection_data->length);
        gtk_text_buffer_insert (text_view->buffer, &drop_point,
                                 utf, -1);
        g_free (utf);
      }
      break;
      
    case UTF8:
      gtk_text_buffer_insert (text_view->buffer, &drop_point,
                               (const gchar *)selection_data->data,
                               selection_data->length);
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
            /* FIXME this is broken, it assumes the CTEXT is latin1
               when it probably isn't. */
            gchar *utf;

            utf = gtk_text_latin1_to_utf (list[i], strlen (list[i]));
            
            gtk_text_buffer_insert (text_view->buffer, &drop_point, utf, -1);

            g_free (utf);
          }

	if (count > 0)
	  gdk_free_text_list (list);
      }
      break;
      
    case INVALID:		/* quiet compiler */
      break;
    }
}

static void
gtk_text_view_set_scroll_adjustments (GtkTextView   *text_view,
				      GtkAdjustment *hadj,
				      GtkAdjustment *vadj)
{
  gboolean need_adjust = FALSE;

  g_return_if_fail (text_view != NULL);
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (hadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
  else
    hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  if (vadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
  else
    vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  
  if (text_view->hadjustment && (text_view->hadjustment != hadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (text_view->hadjustment), text_view);
      gtk_object_unref (GTK_OBJECT (text_view->hadjustment));
    }
  
  if (text_view->vadjustment && (text_view->vadjustment != vadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (text_view->vadjustment), text_view);
      gtk_object_unref (GTK_OBJECT (text_view->vadjustment));
    }
  
  if (text_view->hadjustment != hadj)
    {
      text_view->hadjustment = hadj;
      gtk_object_ref (GTK_OBJECT (text_view->hadjustment));
      gtk_object_sink (GTK_OBJECT (text_view->hadjustment));
      
      gtk_signal_connect (GTK_OBJECT (text_view->hadjustment), "value_changed",
			  (GtkSignalFunc) gtk_text_view_value_changed,
			  text_view);
      need_adjust = TRUE;
    }
  
  if (text_view->vadjustment != vadj)
    {
      text_view->vadjustment = vadj;
      gtk_object_ref (GTK_OBJECT (text_view->vadjustment));
      gtk_object_sink (GTK_OBJECT (text_view->vadjustment));
      
      gtk_signal_connect (GTK_OBJECT (text_view->vadjustment), "value_changed",
			  (GtkSignalFunc) gtk_text_view_value_changed,
			  text_view);
      need_adjust = TRUE;
    }

  if (need_adjust)
    gtk_text_view_value_changed (NULL, text_view);
}

static void
gtk_text_view_value_changed (GtkAdjustment *adj,
			     GtkTextView   *text_view)
{
  GtkTextIter iter;
  gint line_top;
  gint dx = 0;
  gint dy = 0;

  if (adj == text_view->hadjustment)
    {
      dx = text_view->xoffset - (gint)adj->value;
      text_view->xoffset = adj->value;
    }
  else if (adj == text_view->vadjustment)
    {
      dy = text_view->yoffset - (gint)adj->value;
      text_view->yoffset = adj->value;

      if (text_view->layout)
	{
	  gtk_text_layout_get_line_at_y (text_view->layout, &iter, adj->value, &line_top);
	  
	  gtk_text_buffer_move_mark (text_view->buffer, text_view->first_para_mark, &iter);
	  
	  text_view->first_para_pixels = adj->value - line_top;
	}
    }

  if (dx != 0 || dy != 0)
    {
      gdk_window_scroll (text_view->bin_window, dx, dy);
      gdk_window_process_updates (text_view->bin_window, TRUE);
    }
}

static void
gtk_text_view_commit_handler (GtkIMContext  *context,
			      const gchar   *str,
			      GtkTextView   *text_view)
{
  gtk_text_buffer_delete_selection (text_view->buffer);

  if (!strcmp (str, "\n"))
    {
      gtk_text_buffer_insert_at_cursor (text_view->buffer, "\n", 1);
    }
  else
    {
      if (text_view->overwrite_mode)
	gtk_text_view_delete_text (text_view, GTK_TEXT_DELETE_CHAR, 1);
      gtk_text_buffer_insert_at_cursor (text_view->buffer, str, strlen (str));
    }
  
  gtk_text_view_scroll_to_mark (text_view,
                                gtk_text_buffer_get_mark (text_view->buffer,
                                                          "insert"),
                                0);
}

static void
gtk_text_view_mark_set_handler (GtkTextBuffer     *buffer,
				const GtkTextIter *location,
				const char        *mark_name,
				gpointer           data)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (data);
  
  if (!strcmp (mark_name, "insert"))
    {
      text_view->virtual_cursor_x = -1;
      text_view->virtual_cursor_y = -1;
    }
}

static void
gtk_text_view_get_virtual_cursor_pos (GtkTextView *text_view,
				      gint        *x,
				      gint        *y)
{
  GdkRectangle strong_pos;
  GtkTextIter insert;

  gtk_text_buffer_get_iter_at_mark (text_view->buffer, &insert,
                                    gtk_text_buffer_get_mark (text_view->buffer,
                                                              "insert"));
  
  if ((x && text_view->virtual_cursor_x == -1) ||
      (y && text_view->virtual_cursor_y == -1))
    gtk_text_layout_get_cursor_locations (text_view->layout, &insert, &strong_pos, NULL);

  if (x)
    {
      if (text_view->virtual_cursor_x != -1)
	*x = text_view->virtual_cursor_x;
      else
	*x = strong_pos.x;
    }

  if (y)
    {
      if (text_view->virtual_cursor_x != -1)
	*y = text_view->virtual_cursor_y;
      else
	*y = strong_pos.y + strong_pos.height / 2;
    }
}

static void
gtk_text_view_set_virtual_cursor_pos (GtkTextView *text_view,
				      gint         x,
				      gint         y)
{
  GdkRectangle strong_pos;
  GtkTextIter insert;

  gtk_text_buffer_get_iter_at_mark (text_view->buffer, &insert,
                                    gtk_text_buffer_get_mark (text_view->buffer,
                                                              "insert"));
  
  if (x == -1 || y == -1)
    gtk_text_layout_get_cursor_locations (text_view->layout, &insert, &strong_pos, NULL);

  text_view->virtual_cursor_x = (x == -1) ? strong_pos.x : x;
  text_view->virtual_cursor_y = (y == -1) ? strong_pos.y + strong_pos.height / 2 : y;
}
