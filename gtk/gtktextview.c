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

#include "gtktext.h"
#include "gtktextdisplay.h"
#include "gtktextview.h"
#include <gdk/gdkkeysyms.h>

enum {
  MOVE_INSERT,
  SET_ANCHOR,
  SCROLL_TEXT,
  DELETE_TEXT,
  CUT_TEXT,
  COPY_TEXT,
  PASTE_TEXT,
  TOGGLE_OVERWRITE,
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

static void gtk_text_view_init (GtkTextView *tkxt);
static void gtk_text_view_class_init (GtkTextViewClass *klass);
static void gtk_text_view_destroy (GtkObject *object);
static void gtk_text_view_finalize (GtkObject *object);
static void gtk_text_view_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void gtk_text_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static void gtk_text_view_size_request (GtkWidget *widget,
                                    GtkRequisition *requisition);

static void gtk_text_view_size_allocate(GtkWidget *widget,
                                    GtkAllocation *allocation);


static void gtk_text_view_realize (GtkWidget *widget);
static void gtk_text_view_unrealize (GtkWidget *widget);

static gint gtk_text_view_event(GtkWidget *widget, GdkEvent *event);
static gint gtk_text_view_key_press_event(GtkWidget *widget, GdkEventKey *event);
static gint gtk_text_view_key_release_event(GtkWidget *widget, GdkEventKey *event);
static gint gtk_text_view_button_press_event(GtkWidget *widget, GdkEventButton *event);
static gint gtk_text_view_button_release_event(GtkWidget *widget, GdkEventButton *event);
static gint gtk_text_view_focus_in_event (GtkWidget *widget, GdkEventFocus *event);
static gint gtk_text_view_focus_out_event (GtkWidget *widget, GdkEventFocus *event);
static gint gtk_text_view_motion_event (GtkWidget *widget, GdkEventMotion *event);

static void gtk_text_view_draw (GtkWidget *widget, GdkRectangle *area);
static gint gtk_text_view_expose_event (GtkWidget *widget, GdkEventExpose *expose);

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


static void gtk_text_view_move_insert         (GtkTextView *tkxt,
                                           GtkTextViewMovementStep step,
                                           gint count,
                                           gboolean extend_selection);

static void gtk_text_view_set_anchor          (GtkTextView *tkxt);
static void gtk_text_view_scroll_text         (GtkTextView *tkxt,
                                           GtkTextViewScrollType type);
static void gtk_text_view_delete_text         (GtkTextView *tkxt,
                                           GtkTextViewDeleteType type,
                                           gint count);

static void gtk_text_view_cut_text            (GtkTextView *tkxt);
static void gtk_text_view_copy_text           (GtkTextView *tkxt);
static void gtk_text_view_paste_text          (GtkTextView *tkxt);
static void gtk_text_view_toggle_overwrite    (GtkTextView *tkxt);

static void gtk_text_view_scroll_calc_now(GtkTextView *tkxt);

static void gtk_text_view_ensure_layout(GtkTextView *tkxt);
static void gtk_text_view_destroy_layout(GtkTextView *tkxt);
static void gtk_text_view_start_selection_drag(GtkTextView *tkxt, const GtkTextIter *iter, GdkEventButton *event);
static gboolean gtk_text_view_end_selection_drag(GtkTextView *tkxt, GdkEventButton *event);
static void gtk_text_view_start_selection_dnd(GtkTextView *tkxt,
                                          const GtkTextIter *iter,
                                          GdkEventButton *event);

static void gtk_text_view_start_cursor_blink(GtkTextView *tkxt);
static void gtk_text_view_stop_cursor_blink(GtkTextView *tkxt);

static void gtk_marshal_NONE__INT_INT_INT (GtkObject  *object,
                                           GtkSignalFunc func,
                                           gpointer func_data,
                                           GtkArg  *args);

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

static guint n_targets = sizeof(target_table) / sizeof(target_table[0]);

static GtkLayout *parent_class = NULL;
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

    our_type = gtk_type_unique (GTK_TYPE_LAYOUT, &our_info);
  }

  return our_type;
}

static void
add_move_insert_binding(GtkBindingSet *binding_set,
                        guint keyval,
                        guint modmask,
                        GtkTextViewMovementStep step,
                        gint count)
{
  g_return_if_fail((modmask & GDK_SHIFT_MASK) == 0);
  
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
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;
  
  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  
  parent_class = gtk_type_class (GTK_TYPE_LAYOUT);

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
                      object_class->type,
                      GTK_SIGNAL_OFFSET (GtkTextViewClass, move_insert),
                      gtk_marshal_NONE__INT_INT_INT,
                      GTK_TYPE_NONE, 3, GTK_TYPE_ENUM, GTK_TYPE_INT, GTK_TYPE_BOOL);

  signals[SET_ANCHOR] = 
      gtk_signal_new ("set_anchor",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (GtkTextViewClass, set_anchor),
                      gtk_marshal_NONE__NONE,
                      GTK_TYPE_NONE, 0);

  signals[SCROLL_TEXT] = 
      gtk_signal_new ("scroll_text",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (GtkTextViewClass, scroll_text),
                      gtk_marshal_NONE__INT,
                      GTK_TYPE_NONE, 1, GTK_TYPE_ENUM);

  signals[DELETE_TEXT] = 
      gtk_signal_new ("delete_text",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (GtkTextViewClass, delete_text),
                      gtk_marshal_NONE__INT_INT,
                      GTK_TYPE_NONE, 2, GTK_TYPE_ENUM, GTK_TYPE_INT);

  signals[CUT_TEXT] =
    gtk_signal_new ("cut_text",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, cut_text),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);

  signals[COPY_TEXT] =
    gtk_signal_new ("copy_text",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, copy_text),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);

  signals[PASTE_TEXT] =
    gtk_signal_new ("paste_text",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, paste_text),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);

  signals[TOGGLE_OVERWRITE] =
    gtk_signal_new ("toggle_overwrite",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, toggle_overwrite),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);
  
  gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

  /*
   * Key bindings
   */

  binding_set = gtk_binding_set_by_class (klass);

  /* Moving the insertion point */
  add_move_insert_binding(binding_set, GDK_Right, 0,
                          GTK_TEXT_MOVEMENT_CHAR, 1);
  
  add_move_insert_binding(binding_set, GDK_Left, 0,
                          GTK_TEXT_MOVEMENT_CHAR, -1);

  add_move_insert_binding(binding_set, GDK_f, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_CHAR, 1);
  
  add_move_insert_binding(binding_set, GDK_b, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_CHAR, -1);
  
  add_move_insert_binding(binding_set, GDK_Right, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_WORD, 1);

  add_move_insert_binding(binding_set, GDK_Left, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_WORD, -1);
  
  /* Eventually we want to move by display lines, not paragraphs */
  add_move_insert_binding(binding_set, GDK_Up, 0,
                          GTK_TEXT_MOVEMENT_PARAGRAPH, -1);
  
  add_move_insert_binding(binding_set, GDK_Down, 0,
                          GTK_TEXT_MOVEMENT_PARAGRAPH, 1);

  add_move_insert_binding(binding_set, GDK_p, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_PARAGRAPH, -1);
  
  add_move_insert_binding(binding_set, GDK_n, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_PARAGRAPH, 1);
  
  add_move_insert_binding(binding_set, GDK_a, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_PARAGRAPH_ENDS, -1);

  add_move_insert_binding(binding_set, GDK_e, GDK_CONTROL_MASK,
                          GTK_TEXT_MOVEMENT_PARAGRAPH_ENDS, 1);

  add_move_insert_binding(binding_set, GDK_f, GDK_MOD1_MASK,
                          GTK_TEXT_MOVEMENT_WORD, 1);

  add_move_insert_binding(binding_set, GDK_b, GDK_MOD1_MASK,
                          GTK_TEXT_MOVEMENT_WORD, -1);

  add_move_insert_binding(binding_set, GDK_Home, 0,
                          GTK_TEXT_MOVEMENT_BUFFER_ENDS, -1);

  add_move_insert_binding(binding_set, GDK_End, 0,
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
  object_class->finalize = gtk_text_view_finalize;

  widget_class->realize = gtk_text_view_realize;
  widget_class->unrealize = gtk_text_view_unrealize;
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
}

void
gtk_text_view_init (GtkTextView *tkxt)
{
  GtkWidget *widget;
  
  widget = GTK_WIDGET(tkxt);

  GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);

  if (!clipboard_atom)
    clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);

  if (!text_atom)
    text_atom = gdk_atom_intern ("TEXT", FALSE);

  if (!ctext_atom)
    ctext_atom = gdk_atom_intern ("COMPOUND_TEXT", FALSE);

  if (!utf8_atom)
    utf8_atom = gdk_atom_intern ("UTF8_STRING", FALSE);
  
  gtk_drag_dest_set(widget,
                    GTK_DEST_DEFAULT_DROP | GTK_DEST_DEFAULT_MOTION,
                    target_table, n_targets,
                    GDK_ACTION_COPY | GDK_ACTION_MOVE);
}

GtkWidget*
gtk_text_view_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_text_view_get_type ()));
}

GtkWidget*
gtk_text_view_new_with_buffer(GtkTextBuffer *buffer)
{
  GtkTextView *tkxt;

  tkxt = (GtkTextView*)gtk_text_view_new();

  gtk_text_view_set_buffer(tkxt, buffer);

  return GTK_WIDGET(tkxt);
}

void
gtk_text_view_set_buffer(GtkTextView *tkxt,
                     GtkTextBuffer *buffer)
{
  g_return_if_fail(GTK_IS_TEXT_VIEW(tkxt));
  g_return_if_fail(buffer == NULL || GTK_IS_TEXT_VIEW_BUFFER(buffer));
  
  if (tkxt->buffer == buffer)
    return;

  if (tkxt->buffer != NULL)
    {
      gtk_object_unref(GTK_OBJECT(tkxt->buffer));
      tkxt->dnd_mark = NULL;
    }

  tkxt->buffer = buffer;
  
  if (buffer != NULL)
    {
      GtkTextIter start;
      
      gtk_object_ref(GTK_OBJECT(buffer));
      gtk_object_sink(GTK_OBJECT(buffer));

      if (tkxt->layout)
        gtk_text_layout_set_buffer(tkxt->layout, buffer);

      gtk_text_buffer_get_iter_at_char(tkxt->buffer, &start, 0);
      
      tkxt->dnd_mark = gtk_text_buffer_create_mark(tkxt->buffer,
                                                    "__drag_target",
                                                    &start, FALSE);
    }

  if (GTK_WIDGET_VISIBLE(tkxt))    
    gtk_widget_queue_draw(GTK_WIDGET(tkxt));
}

GtkTextBuffer*
gtk_text_view_get_buffer(GtkTextView *tkxt)
{
  g_return_val_if_fail(GTK_IS_TEXT_VIEW(tkxt), NULL);
  
  return tkxt->buffer;
}

void
gtk_text_view_get_iter_at_pixel(GtkTextView *tkxt,
                            GtkTextIter *iter,
                            gint x, gint y)
{
  g_return_if_fail(GTK_IS_TEXT_VIEW(tkxt));
  g_return_if_fail(iter != NULL);
  g_return_if_fail(tkxt->layout != NULL);

  gtk_text_layout_get_iter_at_pixel(tkxt->layout,
                                     iter,
                                     x + GTK_LAYOUT(tkxt)->xoffset,
                                     y + GTK_LAYOUT(tkxt)->yoffset);
}


static void
set_adjustment_clamped(GtkAdjustment *adj, gfloat val)
{
  /* We don't really want to clamp to upper; we want to clamp to
     upper - page_size which is the highest value the scrollbar
     will let us reach. */
  if (val > (adj->upper - adj->page_size))
    val = adj->upper - adj->page_size;

  if (val < adj->lower)
    val = adj->lower;
  
  gtk_adjustment_set_value(adj, val);
}

gboolean
gtk_text_view_scroll_to_mark_adjusted (GtkTextView *tkxt,
                                   const gchar *mark_name,
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
  
  g_return_val_if_fail(GTK_IS_TEXT_VIEW(tkxt), FALSE);
  g_return_val_if_fail(mark_name != NULL, FALSE);

  widget = GTK_WIDGET(tkxt);
  
  if (!GTK_WIDGET_MAPPED(widget))
    {
      g_warning("FIXME need to implement scroll_to_mark for unmapped GtkTextView?");
      return FALSE;
    }
  
  if (!gtk_text_buffer_get_iter_at_mark(tkxt->buffer, &iter, mark_name))
    {
      g_warning("Mark %s does not exist! can't scroll to it.", mark_name);
      return FALSE;
    }
  
  gtk_text_layout_get_iter_location(tkxt->layout,
                                     &iter,
                                     &rect);
  
  /* Be sure the scroll region is up-to-date */
  gtk_text_view_scroll_calc_now(tkxt);
  
  screen.x = GTK_LAYOUT(tkxt)->xoffset;
  screen.y = GTK_LAYOUT(tkxt)->yoffset;
  screen.width = widget->allocation.width;
  screen.height = widget->allocation.height;

  {
    /* Clamp margin so it's not too large. */
    gint small_dimension = MIN(screen.width, screen.height);
    gint max_rect_dim;
    
    if (margin > (small_dimension/2 - 5)) /* 5 is arbitrary */
      margin = (small_dimension/2 - 5);

    if (margin < 0)
      margin = 0;
    
    /* make sure rectangle fits in the leftover space */

    max_rect_dim = MAX(rect.width, rect.height);
    
    if (max_rect_dim > (small_dimension - margin*2))
      margin -= max_rect_dim - (small_dimension - margin*2);
                 
    if (margin < 0)
      margin = 0;
  }

  g_assert(margin >= 0);
  
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
      set_adjustment_clamped(GTK_LAYOUT(tkxt)->vadjustment,
                             ((gint)GTK_LAYOUT(tkxt)->yoffset) + scroll_inc);
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
      set_adjustment_clamped(GTK_LAYOUT(tkxt)->hadjustment,
                             ((gint)GTK_LAYOUT(tkxt)->xoffset) + scroll_inc);
      retval = TRUE;
    }

  return retval;
}

gboolean
gtk_text_view_scroll_to_mark (GtkTextView *tkxt,
                          const gchar *mark_name,
                          gint mark_within_margin)
{
  g_return_val_if_fail(mark_within_margin >= 0, FALSE);
  
  return gtk_text_view_scroll_to_mark_adjusted(tkxt, mark_name,
                                           mark_within_margin, 1.0);
}

static gboolean
clamp_iter_onscreen(GtkTextView *tkxt, GtkTextIter *iter)
{
  GtkTextIter start, end;
  GtkWidget *widget;

  widget = GTK_WIDGET(tkxt);

  gtk_text_view_get_iter_at_pixel(tkxt, &start, 0, 0);
  gtk_text_view_get_iter_at_pixel(tkxt, &end,
                              widget->allocation.width,
                              widget->allocation.height);

  if (gtk_text_iter_compare(iter, &start) < 0)
    {
      *iter = start;
      return TRUE;
    }
  else if (gtk_text_iter_compare(iter, &end) > 0)
    {
      *iter = end;
      return TRUE;
    }
  else
    return FALSE;
}

gboolean
gtk_text_view_move_mark_onscreen(GtkTextView *tkxt,
                             const gchar *mark_name)
{
  GtkTextIter mark;
  
  g_return_val_if_fail(GTK_IS_TEXT_VIEW(tkxt), FALSE);
  g_return_val_if_fail(mark_name != NULL, FALSE);
  
  if (!gtk_text_buffer_get_iter_at_mark(tkxt->buffer, &mark, mark_name))
    return FALSE;
  
  if (clamp_iter_onscreen(tkxt, &mark))
    {
      gtk_text_buffer_move_mark(tkxt->buffer, mark_name, &mark);
      return TRUE;
    }
  else
    return FALSE;
}

gboolean
gtk_text_view_place_cursor_onscreen(GtkTextView *tkxt)
{
  GtkTextIter insert;
  
  g_return_val_if_fail(GTK_IS_TEXT_VIEW(tkxt), FALSE);
  
  gtk_text_buffer_get_iter_at_mark(tkxt->buffer, &insert, "insert");
  
  if (clamp_iter_onscreen(tkxt, &insert))
    {
      gtk_text_buffer_place_cursor(tkxt->buffer, &insert);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_text_view_destroy (GtkObject *object)
{
  GtkTextView *tkxt;

  tkxt = GTK_TEXT_VIEW(object);
  
  gtk_text_view_destroy_layout(tkxt);
  
  if (tkxt->buffer != NULL)
    gtk_object_unref(GTK_OBJECT(tkxt->buffer));

  (* GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}

static void
gtk_text_view_finalize (GtkObject *object)
{
  GtkTextView *tkxt;

  tkxt = GTK_TEXT_VIEW(object);  

  (* GTK_OBJECT_CLASS(parent_class)->finalize) (object);
}

static void
gtk_text_view_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
  GtkTextView *tkxt;

  tkxt = GTK_TEXT_VIEW(object);

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
      g_assert_not_reached();
      break;
    }
}

static void
gtk_text_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
  GtkTextView *tkxt;

  tkxt = GTK_TEXT_VIEW(object);

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
gtk_text_view_size_request (GtkWidget *widget,
                        GtkRequisition *requisition)
{
  /* Hrm */
  requisition->width = 1;
  requisition->height = 1;
}

static void
gtk_text_view_size_allocate(GtkWidget *widget,
                        GtkAllocation *allocation)
{
  GtkTextView *tkxt;

  tkxt = GTK_TEXT_VIEW(widget);
  
  (* GTK_WIDGET_CLASS(parent_class)->size_allocate) (widget, allocation);

  g_assert(widget->allocation.width == allocation->width);
  g_assert(widget->allocation.height == allocation->height);
  
  gtk_text_view_scroll_calc_now(tkxt);
}

static void
need_repaint_handler(GtkTextLayout *layout, gint x, gint y,
                     gint width, gint height, gpointer data)
{
  GtkTextView *tkxt;
  GdkRectangle screen;
  GdkRectangle area;
  GdkRectangle intersect;
  
  tkxt = GTK_TEXT_VIEW(data);

#if 0
  gtk_widget_queue_draw(GTK_WIDGET(tkxt));
#endif
  
#if 1
  screen.x = GTK_LAYOUT(tkxt)->xoffset;
  screen.y = GTK_LAYOUT(tkxt)->yoffset;
  screen.width = GTK_WIDGET(tkxt)->allocation.width;
  screen.height = GTK_WIDGET(tkxt)->allocation.height;

  area.x = x;
  area.y = y;
  area.width = width;
  area.height = height;

#if 0
  printf("repaint needed, x: %d y: %d width: %d height: %d\n",
         x, y, width, height);
  printf(" screen         x: %d y: %d width: %d height: %d\n",
         screen.x, screen.y, screen.width, screen.height);
#endif
  
  if (gdk_rectangle_intersect(&screen, &area, &intersect))
    {
#if 0
      printf(" intersect, x: %d y: %d width: %d height: %d\n",
             intersect.x, intersect.y, intersect.width, intersect.height);
#endif 
      gtk_widget_queue_draw_area(GTK_WIDGET(tkxt),
                                 intersect.x - GTK_LAYOUT(tkxt)->xoffset,
                                 intersect.y - GTK_LAYOUT(tkxt)->yoffset,
                                 intersect.width,
                                 intersect.height);
    }
#endif
}

static void
gtk_text_view_realize (GtkWidget *widget)
{
  GtkTextView *tkxt;
  GdkCursor *cursor;
  
  tkxt = GTK_TEXT_VIEW(widget);
  
  (* GTK_WIDGET_CLASS(parent_class)->realize) (widget);

  gtk_text_view_ensure_layout(tkxt);
  
  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
  gdk_window_set_back_pixmap (GTK_LAYOUT(widget)->bin_window, NULL, FALSE);
  
  gtk_widget_add_events(widget,
                        GDK_KEY_PRESS_MASK |
                        GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK |
                        GDK_POINTER_MOTION_MASK |
                        GDK_POINTER_MOTION_HINT_MASK);

  /* I-beam cursor */
  cursor = gdk_cursor_new(GDK_XTERM);
  gdk_window_set_cursor(GTK_LAYOUT(widget)->bin_window, cursor);
  gdk_cursor_destroy(cursor);
}

static void
gtk_text_view_unrealize (GtkWidget *widget)
{
  GtkTextView *tkxt;

  tkxt = GTK_TEXT_VIEW(widget);

  gtk_text_view_destroy_layout(tkxt);
  
  (* GTK_WIDGET_CLASS(parent_class)->unrealize) (widget);
}

/*
 * Events
 */

static gboolean
get_event_coordinates(GdkEvent *event, gint *x, gint *y)
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
gtk_text_view_event(GtkWidget *widget, GdkEvent *event)
{
  GtkTextView *tkxt;
  gint x = 0, y = 0;
  
  tkxt = GTK_TEXT_VIEW(widget);
  
  if (tkxt->layout == NULL ||
      tkxt->buffer == NULL)
    return FALSE;

  /* FIXME eventually we really want to synthesize enter/leave
     events here as the canvas does for canvas items */
  
  if (get_event_coordinates(event, &x, &y))
    {
      GtkTextIter iter;
      gint retval = FALSE;

      x += GTK_LAYOUT(widget)->xoffset;
      y += GTK_LAYOUT(widget)->yoffset;

      /* FIXME this is slow and we do it twice per event.
         My favorite solution is to have GtkTextLayout cache
         the last couple lookups. */
      gtk_text_layout_get_iter_at_pixel(tkxt->layout,
                                         &iter,
                                         x, y);

        {
          GSList *tags;
          GSList *tmp;
          
          tags = gtk_text_buffer_get_tags(tkxt->buffer, &iter);
          
          tmp = tags;
          while (tmp != NULL)
            {
              GtkTextTag *tag = tmp->data;

              if (gtk_text_tag_event(tag, GTK_OBJECT(widget), event, &iter))
                {
                  retval = TRUE;
                  break;
                }

              tmp = g_slist_next(tmp);
            }

          g_slist_free(tags);
        }
        
      return retval;
    }

  return FALSE;
}

static gint
gtk_text_view_key_press_event(GtkWidget *widget, GdkEventKey *event)
{
  GtkTextView *tkxt;

  tkxt = GTK_TEXT_VIEW(widget);

  if (tkxt->layout == NULL ||
      tkxt->buffer == NULL)
    return FALSE;

  if (GTK_WIDGET_CLASS (parent_class)->key_press_event &&
      GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event))
    return TRUE;

  if (event->length > 0)
    {
      if ((event->state & GDK_MOD1_MASK) ||
          (event->state & GDK_CONTROL_MASK))
        {
          return FALSE;
        }
      
      gtk_text_buffer_delete_selection(tkxt->buffer);

      switch (event->keyval)
        {
        case GDK_Return:
          gtk_text_buffer_insert_at_cursor(tkxt->buffer, "\n", 1);
          break;
          
        default:
          if (tkxt->overwrite_mode)
            gtk_text_view_delete_text(tkxt, GTK_TEXT_DELETE_CHAR, 1);
          gtk_text_buffer_insert_at_cursor(tkxt->buffer,
                                            event->string, event->length);
          break;
        }
      
      gtk_text_view_scroll_to_mark(tkxt, "insert", 0);
      return TRUE;
    }
      
  return FALSE;
}

static gint
gtk_text_view_key_release_event(GtkWidget *widget, GdkEventKey *event)
{

  return FALSE;
}

static gint
gtk_text_view_button_press_event(GtkWidget *widget, GdkEventButton *event)
{
  GtkTextView *tkxt;

  tkxt = GTK_TEXT_VIEW(widget);
  
  gtk_widget_grab_focus(widget);

  /* debug hack */
  if (event->button == 3 && (event->state & GDK_CONTROL_MASK) != 0)
    gtk_text_buffer_spew(GTK_TEXT_VIEW(widget)->buffer);
  else if (event->button == 3)
    gtk_text_layout_spew(GTK_TEXT_VIEW(widget)->layout);

  if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 1)
        {
          /* If we're in the selection, start a drag copy/move of the
             selection; otherwise, start creating a new selection. */
          GtkTextIter iter;
          GtkTextIter start, end;
          
          gtk_text_layout_get_iter_at_pixel(tkxt->layout,
                                             &iter,
                                             event->x + GTK_LAYOUT(tkxt)->xoffset,
                                             event->y + GTK_LAYOUT(tkxt)->yoffset);
          
          if (gtk_text_buffer_get_selection_bounds(tkxt->buffer,
                                                    &start, &end) &&
              gtk_text_iter_in_region(&iter, &start, &end))
            {
              gtk_text_view_start_selection_dnd(tkxt, &iter, event);
            }
          else
            {
              gtk_text_view_start_selection_drag(tkxt, &iter, event);
            }
          
          return TRUE;
        }
      else if (event->button == 2)
        {
          GtkTextIter iter;

          gtk_text_layout_get_iter_at_pixel(tkxt->layout,
                                             &iter,
                                             event->x + GTK_LAYOUT(tkxt)->xoffset,
                                             event->y + GTK_LAYOUT(tkxt)->yoffset);
          
          gtk_text_buffer_paste_primary_selection(tkxt->buffer,
                                                   &iter,
                                                   event->time);
          return TRUE;
        }
      else if (event->button == 3)
        {
          if (gtk_text_view_end_selection_drag(tkxt, event))
            return TRUE;
          else
            return FALSE;
        }
    }
  
  return FALSE;
}

static gint
gtk_text_view_button_release_event(GtkWidget *widget, GdkEventButton *event)
{
  if (event->button == 1)
    {
      gtk_text_view_end_selection_drag(GTK_TEXT_VIEW(widget), event);
      return TRUE;
    }

  return FALSE;
}

static gint
gtk_text_view_focus_in_event (GtkWidget *widget, GdkEventFocus *event)
{
  GtkTextMark *insert;
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);

  insert = gtk_text_buffer_get_mark(GTK_TEXT_VIEW(widget)->buffer,
                                     "insert");
  gtk_text_mark_set_visible(insert, TRUE);

  gtk_text_view_start_cursor_blink(GTK_TEXT_VIEW(widget));
  
  return FALSE;
}

static gint
gtk_text_view_focus_out_event (GtkWidget *widget, GdkEventFocus *event)
{
  GtkTextMark *insert;
  
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);

  insert = gtk_text_buffer_get_mark(GTK_TEXT_VIEW(widget)->buffer,
                                     "insert");
  gtk_text_mark_set_visible(insert, FALSE);

  gtk_text_view_stop_cursor_blink(GTK_TEXT_VIEW(widget));
  
  return FALSE;
}

static gint
gtk_text_view_motion_event (GtkWidget *widget, GdkEventMotion *event)
{
  
  return FALSE;
}

static void
gtk_text_view_paint(GtkWidget *widget, GdkRectangle *area)
{
  GtkTextView *tkxt;
  GdkPixmap *buffer;
  gint pixmap_x;
  gint pixmap_y;
  GdkRectangle repaint_area;
  GdkRectangle screen;
  
  tkxt = GTK_TEXT_VIEW(widget);

  g_return_if_fail(tkxt->layout != NULL);
  g_return_if_fail(GTK_LAYOUT(tkxt)->xoffset >= 0);
  g_return_if_fail(GTK_LAYOUT(tkxt)->yoffset >= 0);
  
  gtk_text_view_scroll_calc_now(tkxt);

#if 0
  printf("painting %d,%d  %d x %d\n",
         area->x, area->y,
         area->width, area->height);
#endif
  screen.x = 0;
  screen.y = 0;
  screen.width = widget->allocation.width;
  screen.height = widget->allocation.height;

  if (!gdk_rectangle_intersect(area, &screen, &repaint_area))
    {
      return;
    }
  
  buffer = gdk_pixmap_new(widget->window,
                          repaint_area.width, repaint_area.height,
                          gtk_widget_get_visual(widget)->depth);

  gdk_draw_rectangle(buffer,
                     widget->style->base_gc[GTK_WIDGET_STATE(widget)],
                     TRUE,
                     0, 0,
                     repaint_area.width,
                     repaint_area.height);


  pixmap_x = repaint_area.x + (gint)GTK_LAYOUT(tkxt)->xoffset;
  pixmap_y = repaint_area.y + (gint)GTK_LAYOUT(tkxt)->yoffset;

  gtk_text_layout_draw(tkxt->layout,
                        widget,
                        buffer,
                        pixmap_x,
                        pixmap_y,
                        pixmap_x,
                        pixmap_y,
                        repaint_area.width,
                        repaint_area.height);

  gdk_draw_pixmap(GTK_LAYOUT(tkxt)->bin_window,
                  widget->style->white_gc,
                  buffer,
                  0, 0, /* pixmap coords */
                  repaint_area.x,
                  repaint_area.y,
                  repaint_area.width,
                  repaint_area.height);

  gdk_pixmap_unref(buffer);
}

static void
gtk_text_view_draw (GtkWidget *widget, GdkRectangle *area)
{  
  gtk_text_view_paint(widget, area);
}

static gint
gtk_text_view_expose_event (GtkWidget *widget, GdkEventExpose *event)
{  
  gtk_text_view_paint(widget, &event->area);
  
  return TRUE;
}

/*
 * Blink!
 */

static gint
blink_cb(gpointer data)
{
  GtkTextView *tkxt;
  GtkTextMark *insert;
  
  tkxt = GTK_TEXT_VIEW(data);

  insert = gtk_text_buffer_get_mark(tkxt->buffer,
                                     "insert");
  
  if (!GTK_WIDGET_HAS_FOCUS(tkxt))
    {
      /* paranoia, in case the user somehow mangles our
         focus_in/focus_out pairing. */
      gtk_text_mark_set_visible(insert, FALSE);
      tkxt->blink_timeout = 0;
      return FALSE;
    }
  else
    {
      gtk_text_mark_set_visible(insert,
                                 !gtk_text_mark_is_visible(insert));
      return TRUE;
    }
}

static void
gtk_text_view_start_cursor_blink(GtkTextView *tkxt)
{
  return;
  if (tkxt->blink_timeout != 0)
    return;

  tkxt->blink_timeout = gtk_timeout_add(500, blink_cb, tkxt);
}

static void
gtk_text_view_stop_cursor_blink(GtkTextView *tkxt)
{
  return;
  if (tkxt->blink_timeout == 0)
    return;

  gtk_timeout_remove(tkxt->blink_timeout);
  tkxt->blink_timeout = 0;
}

/*
 * Key binding handlers
 */

static void
gtk_text_view_move_insert (GtkTextView *tkxt,
                       GtkTextViewMovementStep step,
                       gint count,
                       gboolean extend_selection)
{
  GtkTextIter insert;
  GtkTextIter newplace;

  gtk_text_buffer_get_iter_at_mark(tkxt->buffer, &insert, "insert");
  newplace = insert;
  
  switch (step)
    {
    case GTK_TEXT_MOVEMENT_CHAR:
      gtk_text_iter_forward_chars(&newplace, count);
      break;

    case GTK_TEXT_MOVEMENT_WORD:
      if (count < 0)
        gtk_text_iter_backward_word_starts(&newplace, -count);
      else if (count > 0)
        gtk_text_iter_forward_word_ends(&newplace, count);
      break;

    case GTK_TEXT_MOVEMENT_LINE:
      break;

    case GTK_TEXT_MOVEMENT_PARAGRAPH:
      gtk_text_iter_down_lines(&newplace, count);
      break;

    case GTK_TEXT_MOVEMENT_PARAGRAPH_ENDS:
      if (count > 0)
        gtk_text_iter_forward_to_newline(&newplace);
      else if (count < 0)
        gtk_text_iter_set_line_char(&newplace, 0);
      break;

    case GTK_TEXT_MOVEMENT_BUFFER_ENDS:
      if (count > 0)
        gtk_text_buffer_get_last_iter(tkxt->buffer, &newplace);
      else if (count < 0)
        gtk_text_buffer_get_iter_at_char(tkxt->buffer, &newplace, 0);
      break;

    default:
      break;
    }
  
  if (!gtk_text_iter_equal(&insert, &newplace))
    {
      if (extend_selection)
        gtk_text_buffer_move_mark(tkxt->buffer, "insert", &newplace);
      else
        gtk_text_buffer_place_cursor(tkxt->buffer, &newplace);
      gtk_text_view_scroll_to_mark(tkxt, "insert", 0);
    }
}

static void
gtk_text_view_set_anchor (GtkTextView *tkxt)
{
  GtkTextIter insert;
  
  gtk_text_buffer_get_iter_at_mark(tkxt->buffer, &insert, "insert");

  gtk_text_buffer_create_mark(tkxt->buffer, "anchor", &insert, TRUE);
}

static void
gtk_text_view_scroll_text (GtkTextView *tkxt,
                       GtkTextViewScrollType type)
{
  gfloat newval;
  GtkAdjustment *adj;
  
  g_return_if_fail(GTK_LAYOUT(tkxt)->vadjustment != NULL);

  adj = GTK_LAYOUT(tkxt)->vadjustment;
  
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
  
  set_adjustment_clamped(adj, newval);

  /* Move cursor to be on the new screen, if necessary */
  gtk_text_view_place_cursor_onscreen(tkxt);
  /* Adjust to have the cursor _entirely_ onscreen, move_mark_onscreen
     only guarantees 1 pixel onscreen. */
  gtk_text_view_scroll_to_mark(tkxt, "insert", 0);
}

static gboolean
whitespace(GtkTextUniChar ch, gpointer user_data)
{
  return (ch == ' ' || ch == '\t');
}

static gboolean
not_whitespace(GtkTextUniChar ch, gpointer user_data)
{
  return !whitespace(ch, user_data);
}

static gboolean
find_whitepace_region(const GtkTextIter *center,
                      GtkTextIter *start, GtkTextIter *end)
{
  *start = *center;
  *end = *center;

  if (gtk_text_iter_backward_find_char(start, not_whitespace, NULL))
    gtk_text_iter_forward_char(start); /* we want the first whitespace... */
  if (whitespace(gtk_text_iter_get_char(end), NULL))
    gtk_text_iter_forward_find_char(end, not_whitespace, NULL);
  
  return !gtk_text_iter_equal(start, end);
}

static void
gtk_text_view_delete_text (GtkTextView *tkxt,
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
      if (gtk_text_buffer_delete_selection(tkxt->buffer))
        return;
    }
  
  gtk_text_buffer_get_iter_at_mark(tkxt->buffer,
                                    &insert,
                                    "insert");

  start = insert;
  end = insert;
  
  switch (type)
    {
    case GTK_TEXT_DELETE_CHAR:
      gtk_text_iter_forward_chars(&end, count);
      break;
      
    case GTK_TEXT_DELETE_HALF_WORD:
      if (count > 0)
        gtk_text_iter_forward_word_ends(&end, count);
      else if (count < 0)
        gtk_text_iter_backward_word_starts(&start, 0 - count);
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
          if (!gtk_text_iter_forward_to_newline(&end))
            break;

          --count;
        }

      /* FIXME figure out what a negative count means
         and support that */
      break;
      
    case GTK_TEXT_DELETE_WHOLE_PARAGRAPH:
      if (count > 0)
        {
          gtk_text_iter_set_line_char(&start, 0);
          gtk_text_iter_forward_to_newline(&end);

          /* Do the lines beyond the first. */
          while (count > 1)
            {
              gtk_text_iter_forward_to_newline(&end);
              
              --count;
            }
        }

      /* FIXME negative count? */
      
      break;

    case GTK_TEXT_DELETE_WHITESPACE_LEAVE_ONE:
      leave_one = TRUE; /* FALL THRU */
    case GTK_TEXT_DELETE_WHITESPACE:
      {
        find_whitepace_region(&insert, &start, &end);
      }
      break;
      
    default:
      break;
    }

  if (!gtk_text_iter_equal(&start, &end))
    {
      gtk_text_buffer_delete(tkxt->buffer, &start, &end);
      
      if (leave_one)
        gtk_text_buffer_insert_at_cursor(tkxt->buffer, " ", 1);
      
      gtk_text_view_scroll_to_mark(tkxt, "insert", 0);
    }
}

static void
gtk_text_view_cut_text (GtkTextView *tkxt)
{
  gtk_text_buffer_cut(tkxt->buffer, GDK_CURRENT_TIME);
  gtk_text_view_scroll_to_mark(tkxt, "insert", 0);
}

static void
gtk_text_view_copy_text (GtkTextView *tkxt)
{
  gtk_text_buffer_copy(tkxt->buffer, GDK_CURRENT_TIME);
  gtk_text_view_scroll_to_mark(tkxt, "insert", 0);
}

static void
gtk_text_view_paste_text (GtkTextView *tkxt)
{
  gtk_text_buffer_paste_clipboard(tkxt->buffer, GDK_CURRENT_TIME);
  gtk_text_view_scroll_to_mark(tkxt, "insert", 0);
}

static void
gtk_text_view_toggle_overwrite (GtkTextView *tkxt)
{
  tkxt->overwrite_mode = !tkxt->overwrite_mode;
}

/*
 * Selections
 */

static gboolean
move_insert_to_pointer_and_scroll(GtkTextView *tkxt, gboolean partial_scroll)
{
  gint x, y;
  GdkModifierType state;
  GtkTextIter newplace;
  gint adjust = 0;
  gboolean in_threshold = FALSE;
  
  gdk_window_get_pointer(GTK_LAYOUT(tkxt)->bin_window, &x, &y, &state);

  /* Adjust movement by how long we've been selecting, to
     get an acceleration effect. The exact numbers are
     pretty arbitrary. We have a threshold before we
     start to accelerate. */
  /* uncommenting this printf helps visualize how it works. */     
  /*   printf("%d\n", tkxt->scrolling_accel_factor); */
       
  if (tkxt->scrolling_accel_factor > 10)
    adjust = (tkxt->scrolling_accel_factor - 10) * 75;
  
  if (y < 0) /* scrolling upward */
    adjust = -adjust; 

  /* No adjust if the pointer has moved back inside the window for sure.
     Also I'm adding a small threshold where no adjust is added,
     in case you want to do a continuous slow scroll. */
#define SLOW_SCROLL_TH 7
  if (x >= (0 - SLOW_SCROLL_TH) &&
      x < (GTK_WIDGET(tkxt)->allocation.width + SLOW_SCROLL_TH) &&
      y >= (0 - SLOW_SCROLL_TH) &&
      y < (GTK_WIDGET(tkxt)->allocation.height + SLOW_SCROLL_TH))
    {
      adjust = 0;
      in_threshold = TRUE;
    }
  
  gtk_text_layout_get_iter_at_pixel(tkxt->layout,
                                     &newplace,
                                     x + GTK_LAYOUT(tkxt)->xoffset,
                                     y + GTK_LAYOUT(tkxt)->yoffset + adjust);
  
  {
      gboolean scrolled = FALSE;

      gtk_text_buffer_move_mark(tkxt->buffer,
                                 "insert",
                                 &newplace);
      
      if (partial_scroll)
        scrolled = gtk_text_view_scroll_to_mark_adjusted(tkxt, "insert", 0, 0.7);
      else
        scrolled = gtk_text_view_scroll_to_mark_adjusted(tkxt, "insert", 0, 1.0);

      if (scrolled)
        {
          /* We want to avoid rapid jump to super-accelerated when you
             leave the slow scroll threshold after scrolling for a
             while. So we slowly decrease accel when scrolling inside
             the threshold.
          */
          if (in_threshold)
            {
              if (tkxt->scrolling_accel_factor > 1)
                tkxt->scrolling_accel_factor -= 2;
            }
          else
            tkxt->scrolling_accel_factor += 1;
        }
      else
        {
          /* If we don't scroll we're probably inside the window, but
             potentially just a bit outside. We decrease acceleration
             while the user is fooling around inside the window.
             Acceleration decreases faster than it increases. */
          if (tkxt->scrolling_accel_factor > 4)
            tkxt->scrolling_accel_factor -= 5;
        }
      
      return scrolled;
  }
}

static gint
selection_scan_timeout(gpointer data)
{
  GtkTextView *tkxt;

  tkxt = GTK_TEXT_VIEW(data);

  if (move_insert_to_pointer_and_scroll(tkxt, TRUE))
    {
      return TRUE; /* remain installed. */
    }
  else
    {
      tkxt->selection_drag_scan_timeout = 0;
      return FALSE; /* remove ourselves */
    }
}

static gint
selection_motion_event_handler(GtkTextView *tkxt, GdkEventMotion *event, gpointer data)
{
  if (move_insert_to_pointer_and_scroll(tkxt, TRUE))
    {
      /* If we had to scroll offscreen, insert a timeout to do so
         again. Note that in the timeout, even if the mouse doesn't
         move, due to this scroll xoffset/yoffset will have changed
         and we'll need to scroll again. */
      if (tkxt->selection_drag_scan_timeout != 0) /* reset on every motion event */
        gtk_timeout_remove(tkxt->selection_drag_scan_timeout);
      
      tkxt->selection_drag_scan_timeout =
        gtk_timeout_add(50, selection_scan_timeout, tkxt);
    }
  
  return TRUE;
}

static void
gtk_text_view_start_selection_drag(GtkTextView *tkxt,
                               const GtkTextIter *iter,
                               GdkEventButton *event)
{
  GtkTextIter newplace;

  g_return_if_fail(tkxt->selection_drag_handler == 0);
  
  gtk_grab_add(GTK_WIDGET(tkxt));

  tkxt->scrolling_accel_factor = 0;

  newplace = *iter;
  
  gtk_text_buffer_place_cursor(tkxt->buffer, &newplace);

  tkxt->selection_drag_handler = gtk_signal_connect(GTK_OBJECT(tkxt),
                                                    "motion_notify_event",
                                                    GTK_SIGNAL_FUNC(selection_motion_event_handler),
                                                    NULL);
}

/* returns whether we were really dragging */
static gboolean
gtk_text_view_end_selection_drag(GtkTextView *tkxt, GdkEventButton *event)
{
  if (tkxt->selection_drag_handler == 0)
    return FALSE;

  gtk_signal_disconnect(GTK_OBJECT(tkxt), tkxt->selection_drag_handler);
  tkxt->selection_drag_handler = 0;

  tkxt->scrolling_accel_factor = 0;
  
  if (tkxt->selection_drag_scan_timeout != 0)
    {
      gtk_timeout_remove(tkxt->selection_drag_scan_timeout);
      tkxt->selection_drag_scan_timeout = 0;
    }

  /* one last update to current position */
  move_insert_to_pointer_and_scroll(tkxt, FALSE);
  
  gtk_grab_remove(GTK_WIDGET(tkxt));
  
  return TRUE;
}

/*
 * Layout utils
 */

static void
gtk_text_view_scroll_calc_now(GtkTextView *tkxt)
{
  gint width = 0, height = 0;
  
  gtk_text_view_ensure_layout(tkxt);

      
  gtk_text_layout_set_screen_width(tkxt->layout,
                                    GTK_WIDGET(tkxt)->allocation.width);
      
  gtk_text_layout_get_size(tkxt->layout,
                            &width, &height);

  /* If the width is less than the screen width (likely
     if we have wrapping turned on for the whole widget),
     then we want to set the scroll region to the screen
     width. If the width is greater (wrapping off) then we
     probably want to set the scroll region to the width
     of the layout. I guess.
  */

  width = MAX(tkxt->layout->screen_width, width);
  height = height;

  if (GTK_LAYOUT(tkxt)->width != width ||
      GTK_LAYOUT(tkxt)->height != height)
    {
#if 0
      printf("layout size set, widget width is %d\n",
             GTK_WIDGET(tkxt)->allocation.width);
#endif     
      gtk_layout_set_size(GTK_LAYOUT(tkxt),
                          width,
                          height);

      /* Set up the step sizes; we'll say that a page is
         our allocation minus one step, and a step is
         1/10 of our allocation. */
      GTK_LAYOUT(tkxt)->hadjustment->step_increment =
        GTK_WIDGET(tkxt)->allocation.width/10.0;
      GTK_LAYOUT(tkxt)->hadjustment->page_increment =
        GTK_WIDGET(tkxt)->allocation.width  *0.9;

      GTK_LAYOUT(tkxt)->vadjustment->step_increment =
        GTK_WIDGET(tkxt)->allocation.height/10.0;
      GTK_LAYOUT(tkxt)->vadjustment->page_increment =
        GTK_WIDGET(tkxt)->allocation.height  *0.9;
    } 
}

static void
gtk_text_view_ensure_layout(GtkTextView *tkxt)
{
  GtkWidget *widget;

  widget = GTK_WIDGET(tkxt);
  
  if (tkxt->layout == NULL)
    {
      GtkTextStyleValues *style;
      
      tkxt->layout = gtk_text_layout_new();

      tkxt->need_repaint_handler =
        gtk_signal_connect(GTK_OBJECT(tkxt->layout),
                           "need_repaint",
                           GTK_SIGNAL_FUNC(need_repaint_handler),
                           tkxt);
      
      if (tkxt->buffer)
        gtk_text_layout_set_buffer(tkxt->layout, tkxt->buffer);
      
      style = gtk_text_view_style_values_new();

      gtk_widget_ensure_style(widget);
      
      style->bg_color = widget->style->base[GTK_STATE_NORMAL];
      style->fg_color = widget->style->fg[GTK_STATE_NORMAL];
      
      style->font = widget->style->font;
      
      style->pixels_above_lines = 2;
      style->pixels_below_lines = 2;
      style->pixels_inside_wrap = 1;
      
      style->wrap_mode = GTK_WRAPMODE_NONE;
      style->justify = GTK_JUSTIFY_LEFT;
      
      gtk_text_layout_set_default_style(tkxt->layout,
                                         style);
      
      gtk_text_view_style_values_unref(style);
    }
}

static void
gtk_text_view_destroy_layout(GtkTextView *tkxt)
{
  if (tkxt->layout)
    {
      gtk_text_view_end_selection_drag(tkxt, NULL);
      
      if (tkxt->need_repaint_handler != 0)
        gtk_signal_disconnect(GTK_OBJECT(tkxt->layout),
                              tkxt->need_repaint_handler);
      tkxt->need_repaint_handler = 0;
      gtk_object_unref(GTK_OBJECT(tkxt->layout));
      tkxt->layout = NULL;
    }
}

typedef void (*GtkSignal_NONE__INT_INT_INT) (GtkObject  *object,
                                             GtkTextViewMovementStep step,
                                             gint count,
                                             gboolean extend_selection,
                                             gpointer user_data);

static void
gtk_marshal_NONE__INT_INT_INT (GtkObject  *object,
                               GtkSignalFunc func,
                               gpointer func_data,
                               GtkArg  *args)
{
  GtkSignal_NONE__INT_INT_INT rfunc;

  rfunc = (GtkSignal_NONE__INT_INT_INT) func;
  (*rfunc) (object,
            GTK_VALUE_ENUM (args[0]),
            GTK_VALUE_INT (args[1]),
            GTK_VALUE_BOOL (args[2]),
            func_data);
}


/*
 * DND feature
 */

static void
gtk_text_view_start_selection_dnd(GtkTextView *tkxt,
                              const GtkTextIter *iter,
                              GdkEventButton *event)
{
  GdkDragContext *context;
  GtkTargetList *target_list;
  GtkTextMark *mark;
  
  /* FIXME we have to handle more formats for the selection,
     and do the conversions to/from UTF8 */
  
  /* FIXME not sure how this is memory-managed. */
  target_list = gtk_target_list_new (target_table, n_targets);
  
  context = gtk_drag_begin(GTK_WIDGET(tkxt), target_list,
                           GDK_ACTION_COPY | GDK_ACTION_MOVE,
                           1, (GdkEvent*)event);

  gtk_drag_set_icon_default (context);

  /* We're inside the selection, so start without being able
     to accept the drag. */
  gdk_drag_status (context, 0, event->time);
  gtk_text_mark_set_visible(tkxt->dnd_mark, FALSE);
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
  GtkTextView *tkxt;

  tkxt = GTK_TEXT_VIEW(widget);

  gtk_text_mark_set_visible(tkxt->dnd_mark, FALSE);
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
  GtkTextView *tkxt;

  tkxt = GTK_TEXT_VIEW(widget);
  
  str = NULL;
  length = 0;
  
  if (gtk_text_buffer_get_selection_bounds(tkxt->buffer, &start, &end))
    {
      /* Extract the selected text */
      str = gtk_text_iter_get_visible_text(&start, &end);
      
      length = strlen(str);
    }

  if (str)
    {
      if (info == TARGET_UTF8_STRING)
        {
          /* Pass raw UTF8 */
          gtk_selection_data_set (selection_data,
                                  utf8_atom,
                                  8*sizeof(gchar), (guchar *)str, length);

        }
      else if (info == TARGET_STRING ||
               info == TARGET_TEXT)
        {
          gchar *latin1;

          latin1 = gtk_text_utf_to_latin1(str, length);
          
          gtk_selection_data_set (selection_data,
                                  GDK_SELECTION_TYPE_STRING,
                                  8*sizeof(gchar), latin1, strlen(latin1));
          g_free(latin1);
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

          g_free(latin1);
        }

      g_free (str);
    }
}

static void
gtk_text_view_drag_data_delete (GtkWidget        *widget,
                            GdkDragContext   *context)
{
  gtk_text_buffer_delete_selection(GTK_TEXT_VIEW(widget)->buffer);
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
  GtkTextView *tkxt;
  GtkTextIter start;
  GtkTextIter end;
  
  tkxt = GTK_TEXT_VIEW(widget);
  
  gtk_text_layout_get_iter_at_pixel(tkxt->layout,
                                     &newplace,
                                     x + GTK_LAYOUT(tkxt)->xoffset,
                                     y + GTK_LAYOUT(tkxt)->yoffset);

  if (gtk_text_buffer_get_selection_bounds(tkxt->buffer,
                                            &start, &end) &&
      gtk_text_iter_in_region(&newplace, &start, &end))
    {
      /* We're inside the selection. */
      gdk_drag_status (context, 0, time);
      gtk_text_mark_set_visible(tkxt->dnd_mark, FALSE);
    }
  else
    {
      gtk_text_mark_set_visible(tkxt->dnd_mark, TRUE);
      
      gdk_drag_status (context, context->suggested_action, time);
    }

  gtk_text_buffer_move_mark(tkxt->buffer,
                             "__drag_target",
                             &newplace);

  {
    /* The effect of this is that the text scrolls if you're near
       the edge. We have to scroll whether or not we're inside
       the selection. */
    gint margin;
    
    margin = MIN(widget->allocation.width, widget->allocation.height);
    margin /= 5;
    
    gtk_text_view_scroll_to_mark_adjusted(tkxt, "__drag_target", margin, 1.0);
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
  GtkTextView *tkxt;
  enum {INVALID, STRING, CTEXT, UTF8} type;

  tkxt = GTK_TEXT_VIEW(widget);
  
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

  if (!gtk_text_buffer_get_iter_at_mark(tkxt->buffer, &drop_point, "__drag_target"))
    return;
  
  switch (type)
    {
    case STRING:
      {
        gchar *utf;

        utf = gtk_text_latin1_to_utf((const gchar*)selection_data->data,
                                      selection_data->length);
        gtk_text_buffer_insert (tkxt->buffer, &drop_point,
                                 utf, -1);
        g_free(utf);
      }
      break;
      
    case UTF8:
      gtk_text_buffer_insert (tkxt->buffer, &drop_point,
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

            utf = gtk_text_latin1_to_utf(list[i], strlen(list[i]));
            
            gtk_text_buffer_insert(tkxt->buffer, &drop_point, utf, -1);

            g_free(utf);
          }

	if (count > 0)
	  gdk_free_text_list (list);
      }
      break;
      
    case INVALID:		/* quiet compiler */
      break;
    }
}
