/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2004-2006 Christian Hammond
 * Copyright (C) 2008 Cody Russell
 * Copyright (C) 2008 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkentryprivate.h"

#include "gtkadjustment.h"
#include "gtkbindings.h"
#include "gtkbox.h"
#include "gtkbutton.h"
#include "gtkcelleditable.h"
#include "gtkcelllayout.h"
#include "gtkcssnodeprivate.h"
#include "gtkdebug.h"
#include "gtkdnd.h"
#include "gtkdndprivate.h"
#include "gtkemojichooser.h"
#include "gtkentrybuffer.h"
#include "gtkgesturedrag.h"
#include "gtkgesturemultipress.h"
#include "gtkgesturesingle.h"
#include "gtkimageprivate.h"
#include "gtkimcontextsimple.h"
#include "gtkimmulticontext.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmagnifierprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkpango.h"
#include "gtkpopover.h"
#include "gtkprivate.h"
#include "gtkprogressbar.h"
#include "gtkseparatormenuitem.h"
#include "gtkselection.h"
#include "gtksettings.h"
#include "gtksnapshot.h"
#include "gtkstylecontextprivate.h"
#include "gtktexthandleprivate.h"
#include "gtktextutil.h"
#include "gtktooltip.h"
#include "gtktreeselection.h"
#include "gtktreeview.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindow.h"
#include "gtkeventcontrollerkey.h"

#include "a11y/gtkentryaccessible.h"

#include <cairo-gobject.h>
#include <string.h>

#include "fallback-c89.c"

/**
 * SECTION:gtkentry
 * @Short_description: A single line text entry field
 * @Title: GtkEntry
 * @See_also: #GtkTextView, #GtkEntryCompletion
 *
 * The #GtkEntry widget is a single line text entry
 * widget. A fairly large set of key bindings are supported
 * by default. If the entered text is longer than the allocation
 * of the widget, the widget will scroll so that the cursor
 * position is visible.
 *
 * When using an entry for passwords and other sensitive information,
 * it can be put into “password mode” using gtk_entry_set_visibility().
 * In this mode, entered text is displayed using a “invisible” character.
 * By default, GTK+ picks the best invisible character that is available
 * in the current font, but it can be changed with
 * gtk_entry_set_invisible_char(). Since 2.16, GTK+ displays a warning
 * when Caps Lock or input methods might interfere with entering text in
 * a password entry. The warning can be turned off with the
 * #GtkEntry:caps-lock-warning property.
 *
 * Since 2.16, GtkEntry has the ability to display progress or activity
 * information behind the text. To make an entry display such information,
 * use gtk_entry_set_progress_fraction() or gtk_entry_set_progress_pulse_step().
 *
 * Additionally, GtkEntry can show icons at either side of the entry. These
 * icons can be activatable by clicking, can be set up as drag source and
 * can have tooltips. To add an icon, use gtk_entry_set_icon_from_gicon() or
 * one of the various other functions that set an icon from an icon name or a
 * paintable. To trigger an action when the user clicks an icon,
 * connect to the #GtkEntry::icon-press signal. To allow DND operations
 * from an icon, use gtk_entry_set_icon_drag_source(). To set a tooltip on
 * an icon, use gtk_entry_set_icon_tooltip_text() or the corresponding function
 * for markup.
 *
 * Note that functionality or information that is only available by clicking
 * on an icon in an entry may not be accessible at all to users which are not
 * able to use a mouse or other pointing device. It is therefore recommended
 * that any such functionality should also be available by other means, e.g.
 * via the context menu of the entry.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * entry[.read-only][.flat][.warning][.error]
 * ├── image.left
 * ├── image.right
 * ├── undershoot.left
 * ├── undershoot.right
 * ├── [selection]
 * ├── [block-cursor]
 * ├── [progress[.pulse]]
 * ╰── [window.popup]
 * ]|
 *
 * GtkEntry has a main node with the name entry. Depending on the properties
 * of the entry, the style classes .read-only and .flat may appear. The style
 * classes .warning and .error may also be used with entries.
 *
 * When the entry shows icons, it adds subnodes with the name image and the
 * style class .left or .right, depending on where the icon appears.
 *
 * When the entry has a selection, it adds a subnode with the name selection.
 *
 * When the entry is in overwrite mode, it adds a subnode with the name block-cursor
 * that determines how the block cursor is drawn.
 *
 * When the entry shows progress, it adds a subnode with the name progress.
 * The node has the style class .pulse when the shown progress is pulsing.
 *
 * The CSS node for a context menu is added as a subnode below entry as well.
 *
 * The undershoot nodes are used to draw the underflow indication when content
 * is scrolled out of view. These nodes get the .left and .right style classes
 * added depending on where the indication is drawn.
 *
 * When touch is used and touch selection handles are shown, they are using
 * CSS nodes with name cursor-handle. They get the .top or .bottom style class
 * depending on where they are shown in relation to the selection. If there is
 * just a single handle for the text cursor, it gets the style class
 * .insertion-cursor.
 */

#define MIN_ENTRY_WIDTH  150

#define MAX_ICONS 2

#define UNDERSHOOT_SIZE 20

#define IS_VALID_ICON_POSITION(pos)               \
  ((pos) == GTK_ENTRY_ICON_PRIMARY ||                   \
   (pos) == GTK_ENTRY_ICON_SECONDARY)

static GQuark          quark_password_hint  = 0;
static GQuark          quark_capslock_feedback = 0;
static GQuark          quark_gtk_signal = 0;
static GQuark          quark_entry_completion = 0;

typedef struct _EntryIconInfo EntryIconInfo;
typedef struct _GtkEntryPasswordHint GtkEntryPasswordHint;
typedef struct _GtkEntryCapslockFeedback GtkEntryCapslockFeedback;

struct _GtkEntryPrivate
{
  EntryIconInfo         *icons[MAX_ICONS];

  GtkEntryBuffer        *buffer;
  GtkIMContext          *im_context;
  GtkWidget             *popup_menu;

  int                    text_baseline;

  PangoLayout           *cached_layout;
  PangoAttrList         *attrs;
  PangoTabArray         *tabs;

  GdkContentProvider    *selection_content;

  gchar        *im_module;

  gchar        *placeholder_text;

  GtkTextHandle *text_handle;
  GtkWidget     *selection_bubble;
  guint          selection_bubble_timeout_id;

  GtkWidget     *magnifier_popover;
  GtkWidget     *magnifier;

  GtkGesture    *drag_gesture;
  GtkGesture    *multipress_gesture;
  GtkEventController *motion_controller;
  GtkEventController *key_controller;

  GtkWidget     *progress_widget;
  GtkCssNode    *selection_node;
  GtkCssNode    *block_cursor_node;
  GtkCssNode    *undershoot_node[2];

  int           text_x;
  int           text_width;

  gfloat        xalign;

  gint          ascent;                     /* font ascent in pango units  */
  gint          current_pos;
  gint          descent;                    /* font descent in pango units */
  gint          dnd_position;               /* In chars, -1 == no DND cursor */
  gint          drag_start_x;
  gint          drag_start_y;
  gint          drop_position;              /* where the drop should happen */
  gint          insert_pos;
  gint          selection_bound;
  gint          scroll_offset;
  gint          start_x;
  gint          start_y;
  gint          width_chars;
  gint          max_width_chars;

  gunichar      invisible_char;

  guint         blink_time;                  /* time in msec the cursor has blinked since last user event */
  guint         blink_timeout;

  guint16       preedit_length;              /* length of preedit string, in bytes */
  guint16	preedit_cursor;	             /* offset of cursor within preedit string, in chars */

  gint64        handle_place_time;

  guint         editable                : 1;
  guint         show_emoji_icon         : 1;
  guint         in_drag                 : 1;
  guint         overwrite_mode          : 1;
  guint         visible                 : 1;

  guint         activates_default       : 1;
  guint         cache_includes_preedit  : 1;
  guint         caps_lock_warning       : 1;
  guint         caps_lock_warning_shown : 1;
  guint         change_count            : 8;
  guint         cursor_visible          : 1;
  guint         editing_canceled        : 1; /* Only used by GtkCellRendererText */
  guint         in_click                : 1; /* Flag so we don't select all when clicking in entry to focus in */
  guint         invisible_char_set      : 1;
  guint         mouse_cursor_obscured   : 1;
  guint         need_im_reset           : 1;
  guint         real_changed            : 1;
  guint         resolved_dir            : 4; /* PangoDirection */
  guint         select_words            : 1;
  guint         select_lines            : 1;
  guint         truncate_multiline      : 1;
  guint         cursor_handle_dragged   : 1;
  guint         selection_handle_dragged : 1;
  guint         populate_all            : 1;
};

struct _EntryIconInfo
{
  GtkWidget *widget;
  gchar *tooltip;
  guint nonactivatable : 1;
  guint in_drag        : 1;
  guint pressed        : 1;

  GdkDragAction actions;
  GdkContentFormats *target_list;
  GdkEventSequence *current_sequence;
  GdkDevice *device;
};

struct _GtkEntryPasswordHint
{
  gint position;      /* Position (in text) of the last password hint */
  guint source_id;    /* Timeout source id */
};

struct _GtkEntryCapslockFeedback
{
  GtkWidget *entry;
  GtkWidget *window;
  GtkWidget *label;
};

enum {
  ACTIVATE,
  POPULATE_POPUP,
  MOVE_CURSOR,
  INSERT_AT_CURSOR,
  DELETE_FROM_CURSOR,
  BACKSPACE,
  CUT_CLIPBOARD,
  COPY_CLIPBOARD,
  PASTE_CLIPBOARD,
  TOGGLE_OVERWRITE,
  ICON_PRESS,
  ICON_RELEASE,
  PREEDIT_CHANGED,
  INSERT_EMOJI,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_CURSOR_POSITION,
  PROP_SELECTION_BOUND,
  PROP_EDITABLE,
  PROP_MAX_LENGTH,
  PROP_VISIBILITY,
  PROP_HAS_FRAME,
  PROP_INVISIBLE_CHAR,
  PROP_ACTIVATES_DEFAULT,
  PROP_WIDTH_CHARS,
  PROP_MAX_WIDTH_CHARS,
  PROP_SCROLL_OFFSET,
  PROP_TEXT,
  PROP_XALIGN,
  PROP_TRUNCATE_MULTILINE,
  PROP_OVERWRITE_MODE,
  PROP_TEXT_LENGTH,
  PROP_INVISIBLE_CHAR_SET,
  PROP_CAPS_LOCK_WARNING,
  PROP_PROGRESS_FRACTION,
  PROP_PROGRESS_PULSE_STEP,
  PROP_PAINTABLE_PRIMARY,
  PROP_PAINTABLE_SECONDARY,
  PROP_ICON_NAME_PRIMARY,
  PROP_ICON_NAME_SECONDARY,
  PROP_GICON_PRIMARY,
  PROP_GICON_SECONDARY,
  PROP_STORAGE_TYPE_PRIMARY,
  PROP_STORAGE_TYPE_SECONDARY,
  PROP_ACTIVATABLE_PRIMARY,
  PROP_ACTIVATABLE_SECONDARY,
  PROP_SENSITIVE_PRIMARY,
  PROP_SENSITIVE_SECONDARY,
  PROP_TOOLTIP_TEXT_PRIMARY,
  PROP_TOOLTIP_TEXT_SECONDARY,
  PROP_TOOLTIP_MARKUP_PRIMARY,
  PROP_TOOLTIP_MARKUP_SECONDARY,
  PROP_IM_MODULE,
  PROP_PLACEHOLDER_TEXT,
  PROP_COMPLETION,
  PROP_INPUT_PURPOSE,
  PROP_INPUT_HINTS,
  PROP_ATTRIBUTES,
  PROP_POPULATE_ALL,
  PROP_TABS,
  PROP_SHOW_EMOJI_ICON,
  PROP_EDITING_CANCELED,
  NUM_PROPERTIES = PROP_EDITING_CANCELED
};

static GParamSpec *entry_props[NUM_PROPERTIES] = { NULL, };

static guint signals[LAST_SIGNAL] = { 0 };

typedef enum {
  CURSOR_STANDARD,
  CURSOR_DND
} CursorType;

typedef enum
{
  DISPLAY_NORMAL,       /* The entry text is being shown */
  DISPLAY_INVISIBLE,    /* In invisible mode, text replaced by (eg) bullets */
  DISPLAY_BLANK         /* In invisible mode, nothing shown at all */
} DisplayMode;

/* GObject methods
 */
static void   gtk_entry_editable_init        (GtkEditableInterface *iface);
static void   gtk_entry_cell_editable_init   (GtkCellEditableIface *iface);
static void   gtk_entry_set_property         (GObject          *object,
                                              guint             prop_id,
                                              const GValue     *value,
                                              GParamSpec       *pspec);
static void   gtk_entry_get_property         (GObject          *object,
                                              guint             prop_id,
                                              GValue           *value,
                                              GParamSpec       *pspec);
static void   gtk_entry_finalize             (GObject          *object);
static void   gtk_entry_dispose              (GObject          *object);

/* GtkWidget methods
 */
static void   gtk_entry_destroy              (GtkWidget        *widget);
static void   gtk_entry_realize              (GtkWidget        *widget);
static void   gtk_entry_unrealize            (GtkWidget        *widget);
static void   gtk_entry_unmap                (GtkWidget        *widget);
static void   gtk_entry_size_allocate        (GtkWidget           *widget,
                                              const GtkAllocation *allocation,
                                              int                  baseline);
static void   gtk_entry_snapshot             (GtkWidget        *widget,
                                              GtkSnapshot      *snapshot);
static gboolean gtk_entry_event              (GtkWidget        *widget,
                                              GdkEvent         *event);
static void   gtk_entry_focus_in             (GtkWidget        *widget);
static void   gtk_entry_focus_out            (GtkWidget        *widget);
static void   gtk_entry_grab_focus           (GtkWidget        *widget);
static void   gtk_entry_style_updated        (GtkWidget        *widget);
static gboolean gtk_entry_query_tooltip      (GtkWidget        *widget,
                                              gint              x,
                                              gint              y,
                                              gboolean          keyboard_tip,
                                              GtkTooltip       *tooltip);
static void   gtk_entry_direction_changed    (GtkWidget        *widget,
					      GtkTextDirection  previous_dir);
static void   gtk_entry_state_flags_changed  (GtkWidget        *widget,
					      GtkStateFlags     previous_state);
static void   gtk_entry_display_changed      (GtkWidget        *widget,
					      GdkDisplay       *old_display);

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
					      GtkSelectionData *selection_data,
					      guint             time);
static void     gtk_entry_drag_data_get      (GtkWidget        *widget,
					      GdkDragContext   *context,
					      GtkSelectionData *selection_data,
					      guint             time);
static void     gtk_entry_drag_data_delete   (GtkWidget        *widget,
					      GdkDragContext   *context);
static void     gtk_entry_drag_begin         (GtkWidget        *widget,
                                              GdkDragContext   *context);
static void     gtk_entry_drag_end           (GtkWidget        *widget,
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
static void gtk_entry_backspace          (GtkEntry        *entry);
static void gtk_entry_cut_clipboard      (GtkEntry        *entry);
static void gtk_entry_copy_clipboard     (GtkEntry        *entry);
static void gtk_entry_paste_clipboard    (GtkEntry        *entry);
static void gtk_entry_toggle_overwrite   (GtkEntry        *entry);
static void gtk_entry_insert_emoji       (GtkEntry        *entry);
static void gtk_entry_select_all         (GtkEntry        *entry);
static void gtk_entry_real_activate      (GtkEntry        *entry);
static gboolean gtk_entry_popup_menu     (GtkWidget       *widget);

static void keymap_direction_changed     (GdkKeymap       *keymap,
					  GtkEntry        *entry);
static void keymap_state_changed         (GdkKeymap       *keymap,
					  GtkEntry        *entry);
static void remove_capslock_feedback     (GtkEntry        *entry);

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

/* Event controller callbacks */
static void   gtk_entry_multipress_gesture_pressed (GtkGestureMultiPress *gesture,
                                                    gint                  n_press,
                                                    gdouble               x,
                                                    gdouble               y,
                                                    GtkEntry             *entry);
static void   gtk_entry_drag_gesture_update        (GtkGestureDrag *gesture,
                                                    gdouble         offset_x,
                                                    gdouble         offset_y,
                                                    GtkEntry       *entry);
static void   gtk_entry_drag_gesture_end           (GtkGestureDrag *gesture,
                                                    gdouble         offset_x,
                                                    gdouble         offset_y,
                                                    GtkEntry       *entry);
static gboolean gtk_entry_key_controller_key_pressed  (GtkEventControllerKey *controller,
                                                       guint                  keyval,
                                                       guint                  keycode,
                                                       GdkModifierType        state,
                                                       GtkWidget             *widget);

/* Internal routines
 */
static void         gtk_entry_draw_text                (GtkEntry       *entry,
                                                        GtkSnapshot    *snapshot);
static void         gtk_entry_draw_cursor              (GtkEntry       *entry,
                                                        GtkSnapshot    *snapshot,
							CursorType      type);
static PangoLayout *gtk_entry_ensure_layout            (GtkEntry       *entry,
                                                        gboolean        include_preedit);
static void         gtk_entry_reset_layout             (GtkEntry       *entry);
static void         gtk_entry_recompute                (GtkEntry       *entry);
static gint         gtk_entry_find_position            (GtkEntry       *entry,
							gint            x);
static void         gtk_entry_get_cursor_locations     (GtkEntry       *entry,
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
							gint            start,
                                                        gboolean        allow_whitespace);
static gint         gtk_entry_move_backward_word       (GtkEntry       *entry,
							gint            start,
                                                        gboolean        allow_whitespace);
static void         gtk_entry_delete_whitespace        (GtkEntry       *entry);
static void         gtk_entry_select_word              (GtkEntry       *entry);
static void         gtk_entry_select_line              (GtkEntry       *entry);
static void         gtk_entry_paste                    (GtkEntry       *entry,
							GdkClipboard   *clipboard);
static void         gtk_entry_update_primary_selection (GtkEntry       *entry);
static void         gtk_entry_do_popup                 (GtkEntry       *entry,
							const GdkEvent *event);
static gboolean     gtk_entry_mnemonic_activate        (GtkWidget      *widget,
							gboolean        group_cycling);
static void         gtk_entry_grab_notify              (GtkWidget      *widget,
                                                        gboolean        was_grabbed);
static void         gtk_entry_check_cursor_blink       (GtkEntry       *entry);
static void         gtk_entry_pend_cursor_blink        (GtkEntry       *entry);
static void         gtk_entry_reset_blink_time         (GtkEntry       *entry);
static void         gtk_entry_update_cached_style_values(GtkEntry      *entry);
static gboolean     get_middle_click_paste             (GtkEntry *entry);
static void         gtk_entry_get_scroll_limits        (GtkEntry       *entry,
                                                        gint           *min_offset,
                                                        gint           *max_offset);

/* GtkTextHandle handlers */
static void         gtk_entry_handle_drag_started      (GtkTextHandle         *handle,
                                                        GtkTextHandlePosition  pos,
                                                        GtkEntry              *entry);
static void         gtk_entry_handle_dragged           (GtkTextHandle         *handle,
                                                        GtkTextHandlePosition  pos,
                                                        gint                   x,
                                                        gint                   y,
                                                        GtkEntry              *entry);
static void         gtk_entry_handle_drag_finished     (GtkTextHandle         *handle,
                                                        GtkTextHandlePosition  pos,
                                                        GtkEntry              *entry);

static void         gtk_entry_selection_bubble_popup_set   (GtkEntry *entry);
static void         gtk_entry_selection_bubble_popup_unset (GtkEntry *entry);

static void         begin_change                       (GtkEntry *entry);
static void         end_change                         (GtkEntry *entry);
static void         emit_changed                       (GtkEntry *entry);

static void         buffer_inserted_text               (GtkEntryBuffer *buffer, 
                                                        guint           position,
                                                        const gchar    *chars,
                                                        guint           n_chars,
                                                        GtkEntry       *entry);
static void         buffer_deleted_text                (GtkEntryBuffer *buffer, 
                                                        guint           position,
                                                        guint           n_chars,
                                                        GtkEntry       *entry);
static void         buffer_notify_text                 (GtkEntryBuffer *buffer, 
                                                        GParamSpec     *spec,
                                                        GtkEntry       *entry);
static void         buffer_notify_length               (GtkEntryBuffer *buffer, 
                                                        GParamSpec     *spec,
                                                        GtkEntry       *entry);
static void         buffer_notify_max_length           (GtkEntryBuffer *buffer, 
                                                        GParamSpec     *spec,
                                                        GtkEntry       *entry);
static void         buffer_connect_signals             (GtkEntry       *entry);
static void         buffer_disconnect_signals          (GtkEntry       *entry);
static GtkEntryBuffer *get_buffer                      (GtkEntry       *entry);
static void         set_show_emoji_icon                (GtkEntry       *entry,
                                                        gboolean        value);

static void     gtk_entry_measure (GtkWidget           *widget,
                                   GtkOrientation       orientation,
                                   int                  for_size,
                                   int                 *minimum,
                                   int                 *natural,
                                   int                 *minimum_baseline,
                                   int                 *natural_baseline);

G_DEFINE_TYPE_WITH_CODE (GtkEntry, gtk_entry, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkEntry)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_entry_editable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE,
                                                gtk_entry_cell_editable_init))

#define GTK_TYPE_ENTRY_CONTENT            (gtk_entry_content_get_type ())
#define GTK_ENTRY_CONTENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ENTRY_CONTENT, GtkEntryContent))
#define GTK_IS_ENTRY_CONTENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ENTRY_CONTENT))
#define GTK_ENTRY_CONTENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ENTRY_CONTENT, GtkEntryContentClass))
#define GTK_IS_ENTRY_CONTENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ENTRY_CONTENT))
#define GTK_ENTRY_CONTENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ENTRY_CONTENT, GtkEntryContentClass))

typedef struct _GtkEntryContent GtkEntryContent;
typedef struct _GtkEntryContentClass GtkEntryContentClass;

struct _GtkEntryContent
{
  GdkContentProvider parent;

  GtkEntry *entry;
};

struct _GtkEntryContentClass
{
  GdkContentProviderClass parent_class;
};

GType gtk_entry_content_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GtkEntryContent, gtk_entry_content, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
gtk_entry_content_ref_formats (GdkContentProvider *provider)
{
  return gdk_content_formats_new_for_gtype (G_TYPE_STRING);
}

static gboolean
gtk_entry_content_get_value (GdkContentProvider  *provider,
                             GValue              *value,
                             GError             **error)
{
  GtkEntryContent *content = GTK_ENTRY_CONTENT (provider);

  if (G_VALUE_HOLDS (value, G_TYPE_STRING))
    {
      int start, end;

      if (gtk_editable_get_selection_bounds (GTK_EDITABLE (content->entry), &start, &end))
        {
          gchar *str = _gtk_entry_get_display_text (content->entry, start, end);
          g_value_take_string (value, str);
        }
      return TRUE;
    }

  return GDK_CONTENT_PROVIDER_CLASS (gtk_entry_content_parent_class)->get_value (provider, value, error);
}

static void
gtk_entry_content_detach (GdkContentProvider *provider,
                          GdkClipboard       *clipboard)
{
  GtkEntryContent *content = GTK_ENTRY_CONTENT (provider);
  GtkEntry *entry = content->entry;
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  gtk_editable_select_region (GTK_EDITABLE (entry), priv->current_pos, priv->current_pos);
}

static void
gtk_entry_content_class_init (GtkEntryContentClass *class)
{
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);

  provider_class->ref_formats = gtk_entry_content_ref_formats;
  provider_class->get_value = gtk_entry_content_get_value;
  provider_class->detach_clipboard = gtk_entry_content_detach;
}

static void
gtk_entry_content_init (GtkEntryContent *content)
{
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
				"move-cursor", 3,
				G_TYPE_ENUM, step,
				G_TYPE_INT, count,
				G_TYPE_BOOLEAN, FALSE);

  /* Selection-extending version */
  gtk_binding_entry_add_signal (binding_set, keyval, modmask | GDK_SHIFT_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, step,
				G_TYPE_INT, count,
				G_TYPE_BOOLEAN, TRUE);
}

static void
gtk_entry_class_init (GtkEntryClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;

  widget_class = (GtkWidgetClass*) class;

  gobject_class->dispose = gtk_entry_dispose;
  gobject_class->finalize = gtk_entry_finalize;
  gobject_class->set_property = gtk_entry_set_property;
  gobject_class->get_property = gtk_entry_get_property;

  widget_class->destroy = gtk_entry_destroy;
  widget_class->unmap = gtk_entry_unmap;
  widget_class->realize = gtk_entry_realize;
  widget_class->unrealize = gtk_entry_unrealize;
  widget_class->measure = gtk_entry_measure;
  widget_class->size_allocate = gtk_entry_size_allocate;
  widget_class->snapshot = gtk_entry_snapshot;
  widget_class->event = gtk_entry_event;
  widget_class->grab_focus = gtk_entry_grab_focus;
  widget_class->style_updated = gtk_entry_style_updated;
  widget_class->query_tooltip = gtk_entry_query_tooltip;
  widget_class->drag_begin = gtk_entry_drag_begin;
  widget_class->drag_end = gtk_entry_drag_end;
  widget_class->direction_changed = gtk_entry_direction_changed;
  widget_class->state_flags_changed = gtk_entry_state_flags_changed;
  widget_class->display_changed = gtk_entry_display_changed;
  widget_class->mnemonic_activate = gtk_entry_mnemonic_activate;
  widget_class->grab_notify = gtk_entry_grab_notify;

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
  class->backspace = gtk_entry_backspace;
  class->cut_clipboard = gtk_entry_cut_clipboard;
  class->copy_clipboard = gtk_entry_copy_clipboard;
  class->paste_clipboard = gtk_entry_paste_clipboard;
  class->toggle_overwrite = gtk_entry_toggle_overwrite;
  class->insert_emoji = gtk_entry_insert_emoji;
  class->activate = gtk_entry_real_activate;
  
  quark_password_hint = g_quark_from_static_string ("gtk-entry-password-hint");
  quark_capslock_feedback = g_quark_from_static_string ("gtk-entry-capslock-feedback");
  quark_gtk_signal = g_quark_from_static_string ("gtk-signal");
  quark_entry_completion = g_quark_from_static_string ("gtk-entry-completion-key");

  g_object_class_override_property (gobject_class,
                                    PROP_EDITING_CANCELED,
                                    "editing-canceled");

  entry_props[PROP_BUFFER] =
      g_param_spec_object ("buffer",
                           P_("Text Buffer"),
                           P_("Text buffer object which actually stores entry text"),
                           GTK_TYPE_ENTRY_BUFFER,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_CURSOR_POSITION] =
      g_param_spec_int ("cursor-position",
                        P_("Cursor Position"),
                        P_("The current position of the insertion cursor in chars"),
                        0, GTK_ENTRY_BUFFER_MAX_SIZE,
                        0,
                        GTK_PARAM_READABLE);

  entry_props[PROP_SELECTION_BOUND] =
      g_param_spec_int ("selection-bound",
                        P_("Selection Bound"),
                        P_("The position of the opposite end of the selection from the cursor in chars"),
                        0, GTK_ENTRY_BUFFER_MAX_SIZE,
                        0,
                        GTK_PARAM_READABLE);

  entry_props[PROP_EDITABLE] =
      g_param_spec_boolean ("editable",
                            P_("Editable"),
                            P_("Whether the entry contents can be edited"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_MAX_LENGTH] =
      g_param_spec_int ("max-length",
                        P_("Maximum length"),
                        P_("Maximum number of characters for this entry. Zero if no maximum"),
                        0, GTK_ENTRY_BUFFER_MAX_SIZE,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_VISIBILITY] =
      g_param_spec_boolean ("visibility",
                            P_("Visibility"),
                            P_("FALSE displays the “invisible char” instead of the actual text (password mode)"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_HAS_FRAME] =
      g_param_spec_boolean ("has-frame",
                            P_("Has Frame"),
                            P_("FALSE removes outside bevel from entry"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_INVISIBLE_CHAR] =
      g_param_spec_unichar ("invisible-char",
                            P_("Invisible character"),
                            P_("The character to use when masking entry contents (in “password mode”)"),
                            '*',
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_ACTIVATES_DEFAULT] =
      g_param_spec_boolean ("activates-default",
                            P_("Activates default"),
                            P_("Whether to activate the default widget (such as the default button in a dialog) when Enter is pressed"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_WIDTH_CHARS] =
      g_param_spec_int ("width-chars",
                        P_("Width in chars"),
                        P_("Number of characters to leave space for in the entry"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:max-width-chars:
   *
   * The desired maximum width of the entry, in characters.
   * If this property is set to -1, the width will be calculated
   * automatically.
   */
  entry_props[PROP_MAX_WIDTH_CHARS] =
      g_param_spec_int ("max-width-chars",
                        P_("Maximum width in characters"),
                        P_("The desired maximum width of the entry, in characters"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_SCROLL_OFFSET] =
      g_param_spec_int ("scroll-offset",
                        P_("Scroll offset"),
                        P_("Number of pixels of the entry scrolled off the screen to the left"),
                        0, G_MAXINT,
                        0,
                        GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_TEXT] =
      g_param_spec_string ("text",
                           P_("Text"),
                           P_("The contents of the entry"),
                           "",
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:xalign:
   *
   * The horizontal alignment, from 0 (left) to 1 (right).
   * Reversed for RTL layouts.
   */
  entry_props[PROP_XALIGN] =
      g_param_spec_float ("xalign",
                          P_("X align"),
                          P_("The horizontal alignment, from 0 (left) to 1 (right). Reversed for RTL layouts."),
                          0.0, 1.0,
                          0.0,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:truncate-multiline:
   *
   * When %TRUE, pasted multi-line text is truncated to the first line.
   */
  entry_props[PROP_TRUNCATE_MULTILINE] =
      g_param_spec_boolean ("truncate-multiline",
                            P_("Truncate multiline"),
                            P_("Whether to truncate multiline pastes to one line."),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:overwrite-mode:
   *
   * If text is overwritten when typing in the #GtkEntry.
   */
  entry_props[PROP_OVERWRITE_MODE] =
      g_param_spec_boolean ("overwrite-mode",
                            P_("Overwrite mode"),
                            P_("Whether new text overwrites existing text"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:text-length:
   *
   * The length of the text in the #GtkEntry.
   */
  entry_props[PROP_TEXT_LENGTH] =
      g_param_spec_uint ("text-length",
                         P_("Text length"),
                         P_("Length of the text currently in the entry"),
                         0, G_MAXUINT16,
                         0,
                         GTK_PARAM_READABLE);

  /**
   * GtkEntry:invisible-char-set:
   *
   * Whether the invisible char has been set for the #GtkEntry.
   */
  entry_props[PROP_INVISIBLE_CHAR_SET] =
      g_param_spec_boolean ("invisible-char-set",
                            P_("Invisible character set"),
                            P_("Whether the invisible character has been set"),
                            FALSE,
                            GTK_PARAM_READWRITE);

  /**
   * GtkEntry:caps-lock-warning:
   *
   * Whether password entries will show a warning when Caps Lock is on.
   *
   * Note that the warning is shown using a secondary icon, and thus
   * does not work if you are using the secondary icon position for some
   * other purpose.
   */
  entry_props[PROP_CAPS_LOCK_WARNING] =
      g_param_spec_boolean ("caps-lock-warning",
                            P_("Caps Lock warning"),
                            P_("Whether password entries will show a warning when Caps Lock is on"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:progress-fraction:
   *
   * The current fraction of the task that's been completed.
   */
  entry_props[PROP_PROGRESS_FRACTION] =
      g_param_spec_double ("progress-fraction",
                           P_("Progress Fraction"),
                           P_("The current fraction of the task that’s been completed"),
                           0.0, 1.0,
                           0.0,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:progress-pulse-step:
   *
   * The fraction of total entry width to move the progress
   * bouncing block for each call to gtk_entry_progress_pulse().
   */
  entry_props[PROP_PROGRESS_PULSE_STEP] =
      g_param_spec_double ("progress-pulse-step",
                           P_("Progress Pulse Step"),
                           P_("The fraction of total entry width to move the progress bouncing block for each call to gtk_entry_progress_pulse()"),
                           0.0, 1.0,
                           0.0,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
  * GtkEntry:placeholder-text:
  *
  * The text that will be displayed in the #GtkEntry when it is empty
  * and unfocused.
  */
  entry_props[PROP_PLACEHOLDER_TEXT] =
      g_param_spec_string ("placeholder-text",
                           P_("Placeholder text"),
                           P_("Show text in the entry when it’s empty and unfocused"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

   /**
   * GtkEntry:primary-icon-paintable:
   *
   * A #GdkPaintable to use as the primary icon for the entry.
   */
  entry_props[PROP_PAINTABLE_PRIMARY] =
      g_param_spec_object ("primary-icon-paintable",
                           P_("Primary paintable"),
                           P_("Primary paintable for the entry"),
                           GDK_TYPE_PAINTABLE,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-paintable:
   *
   * A #GdkPaintable to use as the secondary icon for the entry.
   */
  entry_props[PROP_PAINTABLE_SECONDARY] =
      g_param_spec_object ("secondary-icon-paintable",
                           P_("Secondary paintable"),
                           P_("Secondary paintable for the entry"),
                           GDK_TYPE_PAINTABLE,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:primary-icon-name:
   *
   * The icon name to use for the primary icon for the entry.
   */
  entry_props[PROP_ICON_NAME_PRIMARY] =
      g_param_spec_string ("primary-icon-name",
                           P_("Primary icon name"),
                           P_("Icon name for primary icon"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-name:
   *
   * The icon name to use for the secondary icon for the entry.
   */
  entry_props[PROP_ICON_NAME_SECONDARY] =
      g_param_spec_string ("secondary-icon-name",
                           P_("Secondary icon name"),
                           P_("Icon name for secondary icon"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:primary-icon-gicon:
   *
   * The #GIcon to use for the primary icon for the entry.
   */
  entry_props[PROP_GICON_PRIMARY] =
      g_param_spec_object ("primary-icon-gicon",
                           P_("Primary GIcon"),
                           P_("GIcon for primary icon"),
                           G_TYPE_ICON,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-gicon:
   *
   * The #GIcon to use for the secondary icon for the entry.
   */
  entry_props[PROP_GICON_SECONDARY] =
      g_param_spec_object ("secondary-icon-gicon",
                           P_("Secondary GIcon"),
                           P_("GIcon for secondary icon"),
                           G_TYPE_ICON,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:primary-icon-storage-type:
   *
   * The representation which is used for the primary icon of the entry.
   */
  entry_props[PROP_STORAGE_TYPE_PRIMARY] =
      g_param_spec_enum ("primary-icon-storage-type",
                         P_("Primary storage type"),
                         P_("The representation being used for primary icon"),
                         GTK_TYPE_IMAGE_TYPE,
                         GTK_IMAGE_EMPTY,
                         GTK_PARAM_READABLE);

  /**
   * GtkEntry:secondary-icon-storage-type:
   *
   * The representation which is used for the secondary icon of the entry.
   */
  entry_props[PROP_STORAGE_TYPE_SECONDARY] =
      g_param_spec_enum ("secondary-icon-storage-type",
                         P_("Secondary storage type"),
                         P_("The representation being used for secondary icon"),
                         GTK_TYPE_IMAGE_TYPE,
                         GTK_IMAGE_EMPTY,
                         GTK_PARAM_READABLE);

  /**
   * GtkEntry:primary-icon-activatable:
   *
   * Whether the primary icon is activatable.
   *
   * GTK+ emits the #GtkEntry::icon-press and #GtkEntry::icon-release
   * signals only on sensitive, activatable icons.
   *
   * Sensitive, but non-activatable icons can be used for purely
   * informational purposes.
   */
  entry_props[PROP_ACTIVATABLE_PRIMARY] =
      g_param_spec_boolean ("primary-icon-activatable",
                            P_("Primary icon activatable"),
                            P_("Whether the primary icon is activatable"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-activatable:
   *
   * Whether the secondary icon is activatable.
   *
   * GTK+ emits the #GtkEntry::icon-press and #GtkEntry::icon-release
   * signals only on sensitive, activatable icons.
   *
   * Sensitive, but non-activatable icons can be used for purely
   * informational purposes.
   */
  entry_props[PROP_ACTIVATABLE_SECONDARY] =
      g_param_spec_boolean ("secondary-icon-activatable",
                            P_("Secondary icon activatable"),
                            P_("Whether the secondary icon is activatable"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:primary-icon-sensitive:
   *
   * Whether the primary icon is sensitive.
   *
   * An insensitive icon appears grayed out. GTK+ does not emit the
   * #GtkEntry::icon-press and #GtkEntry::icon-release signals and
   * does not allow DND from insensitive icons.
   *
   * An icon should be set insensitive if the action that would trigger
   * when clicked is currently not available.
   */
  entry_props[PROP_SENSITIVE_PRIMARY] =
      g_param_spec_boolean ("primary-icon-sensitive",
                            P_("Primary icon sensitive"),
                            P_("Whether the primary icon is sensitive"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-sensitive:
   *
   * Whether the secondary icon is sensitive.
   *
   * An insensitive icon appears grayed out. GTK+ does not emit the
   * #GtkEntry::icon-press and #GtkEntry::icon-release signals and
   * does not allow DND from insensitive icons.
   *
   * An icon should be set insensitive if the action that would trigger
   * when clicked is currently not available.
   */
  entry_props[PROP_SENSITIVE_SECONDARY] =
      g_param_spec_boolean ("secondary-icon-sensitive",
                            P_("Secondary icon sensitive"),
                            P_("Whether the secondary icon is sensitive"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:primary-icon-tooltip-text:
   *
   * The contents of the tooltip on the primary icon.
   *
   * Also see gtk_entry_set_icon_tooltip_text().
   */
  entry_props[PROP_TOOLTIP_TEXT_PRIMARY] =
      g_param_spec_string ("primary-icon-tooltip-text",
                           P_("Primary icon tooltip text"),
                           P_("The contents of the tooltip on the primary icon"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-tooltip-text:
   *
   * The contents of the tooltip on the secondary icon.
   *
   * Also see gtk_entry_set_icon_tooltip_text().
   */
  entry_props[PROP_TOOLTIP_TEXT_SECONDARY] =
      g_param_spec_string ("secondary-icon-tooltip-text",
                           P_("Secondary icon tooltip text"),
                           P_("The contents of the tooltip on the secondary icon"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:primary-icon-tooltip-markup:
   *
   * The contents of the tooltip on the primary icon, which is marked up
   * with the [Pango text markup language][PangoMarkupFormat].
   *
   * Also see gtk_entry_set_icon_tooltip_markup().
   */
  entry_props[PROP_TOOLTIP_MARKUP_PRIMARY] =
      g_param_spec_string ("primary-icon-tooltip-markup",
                           P_("Primary icon tooltip markup"),
                           P_("The contents of the tooltip on the primary icon"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-tooltip-markup:
   *
   * The contents of the tooltip on the secondary icon, which is marked up
   * with the [Pango text markup language][PangoMarkupFormat].
   *
   * Also see gtk_entry_set_icon_tooltip_markup().
   */
  entry_props[PROP_TOOLTIP_MARKUP_SECONDARY] =
      g_param_spec_string ("secondary-icon-tooltip-markup",
                           P_("Secondary icon tooltip markup"),
                           P_("The contents of the tooltip on the secondary icon"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:im-module:
   *
   * Which IM (input method) module should be used for this entry.
   * See #GtkIMContext.
   *
   * Setting this to a non-%NULL value overrides the
   * system-wide IM module setting. See the GtkSettings
   * #GtkSettings:gtk-im-module property.
   */
  entry_props[PROP_IM_MODULE] =
      g_param_spec_string ("im-module",
                           P_("IM module"),
                           P_("Which IM module should be used"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:completion:
   *
   * The auxiliary completion object to use with the entry.
   */
  entry_props[PROP_COMPLETION] =
      g_param_spec_object ("completion",
                           P_("Completion"),
                           P_("The auxiliary completion object"),
                           GTK_TYPE_ENTRY_COMPLETION,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:input-purpose:
   *
   * The purpose of this text field.
   *
   * This property can be used by on-screen keyboards and other input
   * methods to adjust their behaviour.
   *
   * Note that setting the purpose to %GTK_INPUT_PURPOSE_PASSWORD or
   * %GTK_INPUT_PURPOSE_PIN is independent from setting
   * #GtkEntry:visibility.
   */
  entry_props[PROP_INPUT_PURPOSE] =
      g_param_spec_enum ("input-purpose",
                         P_("Purpose"),
                         P_("Purpose of the text field"),
                         GTK_TYPE_INPUT_PURPOSE,
                         GTK_INPUT_PURPOSE_FREE_FORM,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:input-hints:
   *
   * Additional hints (beyond #GtkEntry:input-purpose) that
   * allow input methods to fine-tune their behaviour.
   */
  entry_props[PROP_INPUT_HINTS] =
      g_param_spec_flags ("input-hints",
                          P_("hints"),
                          P_("Hints for the text field behaviour"),
                          GTK_TYPE_INPUT_HINTS,
                          GTK_INPUT_HINT_NONE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:attributes:
   *
   * A list of Pango attributes to apply to the text of the entry.
   *
   * This is mainly useful to change the size or weight of the text.
   *
   * The #PangoAttribute's @start_index and @end_index must refer to the
   * #GtkEntryBuffer text, i.e. without the preedit string.
   */
  entry_props[PROP_ATTRIBUTES] =
      g_param_spec_boxed ("attributes",
                          P_("Attributes"),
                          P_("A list of style attributes to apply to the text of the entry"),
                          PANGO_TYPE_ATTR_LIST,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:populate-all:
   *
   * If :populate-all is %TRUE, the #GtkEntry::populate-popup
   * signal is also emitted for touch popups.
   */
  entry_props[PROP_POPULATE_ALL] =
      g_param_spec_boolean ("populate-all",
                            P_("Populate all"),
                            P_("Whether to emit ::populate-popup for touch popups"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry::tabs:
   *
   * A list of tabstops to apply to the text of the entry.
   */
  entry_props[PROP_TABS] =
      g_param_spec_boxed ("tabs",
                          P_("Tabs"),
                          P_("A list of tabstop locations to apply to the text of the entry"),
                          PANGO_TYPE_TAB_ARRAY,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry::show-emoji-icon:
   *
   * When this is %TRUE, the entry will show an emoji icon in the secondary
   * icon position that brings up the Emoji chooser when clicked.
   */
  entry_props[PROP_SHOW_EMOJI_ICON] =
      g_param_spec_boolean ("show-emoji-icon",
                            P_("Emoji icon"),
                            P_("Whether to show an icon for Emoji"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, entry_props);

  /**
   * GtkEntry::populate-popup:
   * @entry: The entry on which the signal is emitted
   * @widget: the container that is being populated
   *
   * The ::populate-popup signal gets emitted before showing the
   * context menu of the entry.
   *
   * If you need to add items to the context menu, connect
   * to this signal and append your items to the @widget, which
   * will be a #GtkMenu in this case.
   *
   * If #GtkEntry:populate-all is %TRUE, this signal will
   * also be emitted to populate touch popups. In this case,
   * @widget will be a different container, e.g. a #GtkToolbar.
   * The signal handler should not make assumptions about the
   * type of @widget.
   */
  signals[POPULATE_POPUP] =
    g_signal_new (I_("populate-popup"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkEntryClass, populate_popup),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);
  
 /* Action signals */
  
  /**
   * GtkEntry::activate:
   * @entry: The entry on which the signal is emitted
   *
   * The ::activate signal is emitted when the user hits
   * the Enter key.
   *
   * The default bindings for this signal are all forms of the Enter key.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, activate),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkEntry::move-cursor:
   * @entry: the object which received the signal
   * @step: the granularity of the move, as a #GtkMovementStep
   * @count: the number of @step units to move
   * @extend_selection: %TRUE if the move should extend the selection
   *
   * The ::move-cursor signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user initiates a cursor movement.
   * If the cursor is not visible in @entry, this signal causes
   * the viewport to be moved instead.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control the cursor
   * programmatically.
   *
   * The default bindings for this signal come in two variants,
   * the variant with the Shift modifier extends the selection,
   * the variant without the Shift modifer does not.
   * There are too many key combinations to list them all here.
   * - Arrow keys move by individual characters/lines
   * - Ctrl-arrow key combinations move by words/paragraphs
   * - Home/End keys move to the ends of the buffer
   */
  signals[MOVE_CURSOR] = 
    g_signal_new (I_("move-cursor"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, move_cursor),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM_INT_BOOLEAN,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_MOVEMENT_STEP,
		  G_TYPE_INT,
		  G_TYPE_BOOLEAN);

  /**
   * GtkEntry::insert-at-cursor:
   * @entry: the object which received the signal
   * @string: the string to insert
   *
   * The ::insert-at-cursor signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user initiates the insertion of a
   * fixed string at the cursor.
   *
   * This signal has no default bindings.
   */
  signals[INSERT_AT_CURSOR] = 
    g_signal_new (I_("insert-at-cursor"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, insert_at_cursor),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1,
		  G_TYPE_STRING);

  /**
   * GtkEntry::delete-from-cursor:
   * @entry: the object which received the signal
   * @type: the granularity of the deletion, as a #GtkDeleteType
   * @count: the number of @type units to delete
   *
   * The ::delete-from-cursor signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user initiates a text deletion.
   *
   * If the @type is %GTK_DELETE_CHARS, GTK+ deletes the selection
   * if there is one, otherwise it deletes the requested number
   * of characters.
   *
   * The default bindings for this signal are
   * Delete for deleting a character and Ctrl-Delete for
   * deleting a word.
   */
  signals[DELETE_FROM_CURSOR] = 
    g_signal_new (I_("delete-from-cursor"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, delete_from_cursor),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM_INT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_DELETE_TYPE,
		  G_TYPE_INT);

  /**
   * GtkEntry::backspace:
   * @entry: the object which received the signal
   *
   * The ::backspace signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user asks for it.
   *
   * The default bindings for this signal are
   * Backspace and Shift-Backspace.
   */
  signals[BACKSPACE] =
    g_signal_new (I_("backspace"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, backspace),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkEntry::cut-clipboard:
   * @entry: the object which received the signal
   *
   * The ::cut-clipboard signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to cut the selection to the clipboard.
   *
   * The default bindings for this signal are
   * Ctrl-x and Shift-Delete.
   */
  signals[CUT_CLIPBOARD] =
    g_signal_new (I_("cut-clipboard"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, cut_clipboard),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkEntry::copy-clipboard:
   * @entry: the object which received the signal
   *
   * The ::copy-clipboard signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to copy the selection to the clipboard.
   *
   * The default bindings for this signal are
   * Ctrl-c and Ctrl-Insert.
   */
  signals[COPY_CLIPBOARD] =
    g_signal_new (I_("copy-clipboard"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, copy_clipboard),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkEntry::paste-clipboard:
   * @entry: the object which received the signal
   *
   * The ::paste-clipboard signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to paste the contents of the clipboard
   * into the text view.
   *
   * The default bindings for this signal are
   * Ctrl-v and Shift-Insert.
   */
  signals[PASTE_CLIPBOARD] =
    g_signal_new (I_("paste-clipboard"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, paste_clipboard),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkEntry::toggle-overwrite:
   * @entry: the object which received the signal
   *
   * The ::toggle-overwrite signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to toggle the overwrite mode of the entry.
   *
   * The default bindings for this signal is Insert.
   */
  signals[TOGGLE_OVERWRITE] =
    g_signal_new (I_("toggle-overwrite"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, toggle_overwrite),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkEntry::icon-press:
   * @entry: The entry on which the signal is emitted
   * @icon_pos: The position of the clicked icon
   * @event: (type Gdk.EventButton): the button press event
   *
   * The ::icon-press signal is emitted when an activatable icon
   * is clicked.
   */
  signals[ICON_PRESS] =
    g_signal_new (I_("icon-press"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM_OBJECT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_ENTRY_ICON_POSITION,
                  GDK_TYPE_EVENT);
  
  /**
   * GtkEntry::icon-release:
   * @entry: The entry on which the signal is emitted
   * @icon_pos: The position of the clicked icon
   * @event: (type Gdk.EventButton): the button release event
   *
   * The ::icon-release signal is emitted on the button release from a
   * mouse click over an activatable icon.
   */
  signals[ICON_RELEASE] =
    g_signal_new (I_("icon-release"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM_OBJECT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_ENTRY_ICON_POSITION,
                  GDK_TYPE_EVENT);

  /**
   * GtkEntry::preedit-changed:
   * @entry: the object which received the signal
   * @preedit: the current preedit string
   *
   * If an input method is used, the typed text will not immediately
   * be committed to the buffer. So if you are interested in the text,
   * connect to this signal.
   */
  signals[PREEDIT_CHANGED] =
    g_signal_new_class_handler (I_("preedit-changed"),
                                G_OBJECT_CLASS_TYPE (gobject_class),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                NULL,
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1,
                                G_TYPE_STRING);


  /**
   * GtkEntry::insert-emoji:
   * @entry: the object which received the signal
   *
   * The ::insert-emoji signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to present the Emoji chooser for the @entry.
   *
   * The default bindings for this signal are Ctrl-. and Ctrl-;
   */
  signals[INSERT_EMOJI] =
    g_signal_new (I_("insert-emoji"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, insert_emoji),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /*
   * Key bindings
   */

  binding_set = gtk_binding_set_by_class (class);

  /* Moving the insertion point */
  add_move_binding (binding_set, GDK_KEY_Right, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_KEY_Left, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  add_move_binding (binding_set, GDK_KEY_KP_Right, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_KEY_KP_Left, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, -1);
  
  add_move_binding (binding_set, GDK_KEY_Right, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_KEY_Left, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (binding_set, GDK_KEY_KP_Right, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_KEY_KP_Left, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, -1);
  
  add_move_binding (binding_set, GDK_KEY_Home, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_KEY_End, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (binding_set, GDK_KEY_KP_Home, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_KEY_KP_End, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);
  
  add_move_binding (binding_set, GDK_KEY_Home, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_KEY_End, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, 1);

  add_move_binding (binding_set, GDK_KEY_KP_Home, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_KEY_KP_End, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, 1);

  /* Select all
   */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK,
                                "move-cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, -1,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK,
                                "move-cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, 1,
				G_TYPE_BOOLEAN, TRUE);  

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_slash, GDK_CONTROL_MASK,
                                "move-cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, -1,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_slash, GDK_CONTROL_MASK,
                                "move-cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, 1,
				G_TYPE_BOOLEAN, TRUE);  
  /* Unselect all 
   */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_backslash, GDK_CONTROL_MASK,
                                "move-cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_VISUAL_POSITIONS,
                                G_TYPE_INT, 0,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                "move-cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_VISUAL_POSITIONS,
                                G_TYPE_INT, 0,
				G_TYPE_BOOLEAN, FALSE);

  /* Activate
   */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, 0,
				"activate", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_ISO_Enter, 0,
				"activate", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter, 0,
				"activate", 0);
  
  /* Deleting text */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, 0,
				"delete-from-cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_CHARS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, 0,
				"delete-from-cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_CHARS,
				G_TYPE_INT, 1);
  
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_BackSpace, 0,
				"backspace", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_u, GDK_CONTROL_MASK,
				"delete-from-cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_PARAGRAPH_ENDS,
				G_TYPE_INT, -1);

  /* Make this do the same as Backspace, to help with mis-typing */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_BackSpace, GDK_SHIFT_MASK,
				"backspace", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, GDK_CONTROL_MASK,
				"delete-from-cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, GDK_CONTROL_MASK,
				"delete-from-cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				G_TYPE_INT, 1);
  
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_BackSpace, GDK_CONTROL_MASK,
				"delete-from-cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				G_TYPE_INT, -1);

  /* Cut/copy/paste */

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_x, GDK_CONTROL_MASK,
				"cut-clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_c, GDK_CONTROL_MASK,
				"copy-clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_v, GDK_CONTROL_MASK,
				"paste-clipboard", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, GDK_SHIFT_MASK,
				"cut-clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Insert, GDK_CONTROL_MASK,
				"copy-clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Insert, GDK_SHIFT_MASK,
				"paste-clipboard", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, GDK_SHIFT_MASK,
                                "cut-clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Insert, GDK_CONTROL_MASK,
                                "copy-clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Insert, GDK_SHIFT_MASK,
                                "paste-clipboard", 0);

  /* Overwrite */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Insert, 0,
				"toggle-overwrite", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Insert, 0,
				"toggle-overwrite", 0);

  /* Emoji */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_period, GDK_CONTROL_MASK,
                                "insert-emoji", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_semicolon, GDK_CONTROL_MASK,
                                "insert-emoji", 0);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_ENTRY_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("entry"));
}

static void
gtk_entry_editable_init (GtkEditableInterface *iface)
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
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  switch (prop_id)
    {
    case PROP_BUFFER:
      gtk_entry_set_buffer (entry, g_value_get_object (value));
      break;

    case PROP_EDITABLE:
      {
        gboolean new_value = g_value_get_boolean (value);
        GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (entry));

        if (new_value != priv->editable)
	  {
            GtkWidget *widget = GTK_WIDGET (entry);

	    if (!new_value)
	      {
		gtk_entry_reset_im_context (entry);
		if (gtk_widget_has_focus (widget))
		  gtk_im_context_focus_out (priv->im_context);

		priv->preedit_length = 0;
		priv->preedit_cursor = 0;

                gtk_style_context_remove_class (context, GTK_STYLE_CLASS_READ_ONLY);
	      }
            else
              {
                gtk_style_context_add_class (context, GTK_STYLE_CLASS_READ_ONLY);
              }

	    priv->editable = new_value;

	    if (new_value && gtk_widget_has_focus (widget))
              gtk_im_context_focus_in (priv->im_context);

            gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (priv->key_controller),
                                                     new_value ? priv->im_context : NULL);

            g_object_notify_by_pspec (object, pspec);
	    gtk_widget_queue_draw (widget);
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

    case PROP_MAX_WIDTH_CHARS:
      gtk_entry_set_max_width_chars (entry, g_value_get_int (value));
      break;

    case PROP_TEXT:
      gtk_entry_set_text (entry, g_value_get_string (value));
      break;

    case PROP_XALIGN:
      gtk_entry_set_alignment (entry, g_value_get_float (value));
      break;

    case PROP_TRUNCATE_MULTILINE:
      if (priv->truncate_multiline != g_value_get_boolean (value))
        {
          priv->truncate_multiline = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_OVERWRITE_MODE:
      gtk_entry_set_overwrite_mode (entry, g_value_get_boolean (value));
      break;

    case PROP_INVISIBLE_CHAR_SET:
      if (g_value_get_boolean (value))
        priv->invisible_char_set = TRUE;
      else
        gtk_entry_unset_invisible_char (entry);
      break;

    case PROP_CAPS_LOCK_WARNING:
      if (priv->caps_lock_warning != g_value_get_boolean (value))
        {
          priv->caps_lock_warning = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_PROGRESS_FRACTION:
      gtk_entry_set_progress_fraction (entry, g_value_get_double (value));
      break;

    case PROP_PROGRESS_PULSE_STEP:
      gtk_entry_set_progress_pulse_step (entry, g_value_get_double (value));
      break;

    case PROP_PLACEHOLDER_TEXT:
      gtk_entry_set_placeholder_text (entry, g_value_get_string (value));
      break;

    case PROP_PAINTABLE_PRIMARY:
      gtk_entry_set_icon_from_paintable (entry,
                                         GTK_ENTRY_ICON_PRIMARY,
                                         g_value_get_object (value));
      break;

    case PROP_PAINTABLE_SECONDARY:
      gtk_entry_set_icon_from_paintable (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         g_value_get_object (value));
      break;

    case PROP_ICON_NAME_PRIMARY:
      gtk_entry_set_icon_from_icon_name (entry,
                                         GTK_ENTRY_ICON_PRIMARY,
                                         g_value_get_string (value));
      break;

    case PROP_ICON_NAME_SECONDARY:
      gtk_entry_set_icon_from_icon_name (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         g_value_get_string (value));
      break;

    case PROP_GICON_PRIMARY:
      gtk_entry_set_icon_from_gicon (entry,
                                     GTK_ENTRY_ICON_PRIMARY,
                                     g_value_get_object (value));
      break;

    case PROP_GICON_SECONDARY:
      gtk_entry_set_icon_from_gicon (entry,
                                     GTK_ENTRY_ICON_SECONDARY,
                                     g_value_get_object (value));
      break;

    case PROP_ACTIVATABLE_PRIMARY:
      gtk_entry_set_icon_activatable (entry,
                                      GTK_ENTRY_ICON_PRIMARY,
                                      g_value_get_boolean (value));
      break;

    case PROP_ACTIVATABLE_SECONDARY:
      gtk_entry_set_icon_activatable (entry,
                                      GTK_ENTRY_ICON_SECONDARY,
                                      g_value_get_boolean (value));
      break;

    case PROP_SENSITIVE_PRIMARY:
      gtk_entry_set_icon_sensitive (entry,
                                    GTK_ENTRY_ICON_PRIMARY,
                                    g_value_get_boolean (value));
      break;

    case PROP_SENSITIVE_SECONDARY:
      gtk_entry_set_icon_sensitive (entry,
                                    GTK_ENTRY_ICON_SECONDARY,
                                    g_value_get_boolean (value));
      break;

    case PROP_TOOLTIP_TEXT_PRIMARY:
      gtk_entry_set_icon_tooltip_text (entry,
                                       GTK_ENTRY_ICON_PRIMARY,
                                       g_value_get_string (value));
      break;

    case PROP_TOOLTIP_TEXT_SECONDARY:
      gtk_entry_set_icon_tooltip_text (entry,
                                       GTK_ENTRY_ICON_SECONDARY,
                                       g_value_get_string (value));
      break;

    case PROP_TOOLTIP_MARKUP_PRIMARY:
      gtk_entry_set_icon_tooltip_markup (entry,
                                         GTK_ENTRY_ICON_PRIMARY,
                                         g_value_get_string (value));
      break;

    case PROP_TOOLTIP_MARKUP_SECONDARY:
      gtk_entry_set_icon_tooltip_markup (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         g_value_get_string (value));
      break;

    case PROP_IM_MODULE:
      g_free (priv->im_module);
      priv->im_module = g_value_dup_string (value);
      if (GTK_IS_IM_MULTICONTEXT (priv->im_context))
        gtk_im_multicontext_set_context_id (GTK_IM_MULTICONTEXT (priv->im_context), priv->im_module);
      g_object_notify_by_pspec (object, pspec);
      break;

    case PROP_EDITING_CANCELED:
      if (priv->editing_canceled != g_value_get_boolean (value))
        {
          priv->editing_canceled = g_value_get_boolean (value);
          g_object_notify (object, "editing-canceled");
        }
      break;

    case PROP_COMPLETION:
      gtk_entry_set_completion (entry, GTK_ENTRY_COMPLETION (g_value_get_object (value)));
      break;

    case PROP_INPUT_PURPOSE:
      gtk_entry_set_input_purpose (entry, g_value_get_enum (value));
      break;

    case PROP_INPUT_HINTS:
      gtk_entry_set_input_hints (entry, g_value_get_flags (value));
      break;

    case PROP_ATTRIBUTES:
      gtk_entry_set_attributes (entry, g_value_get_boxed (value));
      break;

    case PROP_POPULATE_ALL:
      if (priv->populate_all != g_value_get_boolean (value))
        {
          priv->populate_all = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_TABS:
      gtk_entry_set_tabs (entry, g_value_get_boxed (value));
      break;

    case PROP_SHOW_EMOJI_ICON:
      set_show_emoji_icon (entry, g_value_get_boolean (value));
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
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, gtk_entry_get_buffer (entry));
      break;

    case PROP_CURSOR_POSITION:
      g_value_set_int (value, priv->current_pos);
      break;

    case PROP_SELECTION_BOUND:
      g_value_set_int (value, priv->selection_bound);
      break;

    case PROP_EDITABLE:
      g_value_set_boolean (value, priv->editable);
      break;

    case PROP_MAX_LENGTH:
      g_value_set_int (value, gtk_entry_buffer_get_max_length (get_buffer (entry)));
      break;

    case PROP_VISIBILITY:
      g_value_set_boolean (value, priv->visible);
      break;

    case PROP_HAS_FRAME:
      g_value_set_boolean (value, gtk_entry_get_has_frame (entry));
      break;

    case PROP_INVISIBLE_CHAR:
      g_value_set_uint (value, priv->invisible_char);
      break;

    case PROP_ACTIVATES_DEFAULT:
      g_value_set_boolean (value, priv->activates_default);
      break;

    case PROP_WIDTH_CHARS:
      g_value_set_int (value, priv->width_chars);
      break;

    case PROP_MAX_WIDTH_CHARS:
      g_value_set_int (value, priv->max_width_chars);
      break;

    case PROP_SCROLL_OFFSET:
      g_value_set_int (value, priv->scroll_offset);
      break;

    case PROP_TEXT:
      g_value_set_string (value, gtk_entry_get_text (entry));
      break;

    case PROP_XALIGN:
      g_value_set_float (value, gtk_entry_get_alignment (entry));
      break;

    case PROP_TRUNCATE_MULTILINE:
      g_value_set_boolean (value, priv->truncate_multiline);
      break;

    case PROP_OVERWRITE_MODE:
      g_value_set_boolean (value, priv->overwrite_mode);
      break;

    case PROP_TEXT_LENGTH:
      g_value_set_uint (value, gtk_entry_buffer_get_length (get_buffer (entry)));
      break;

    case PROP_INVISIBLE_CHAR_SET:
      g_value_set_boolean (value, priv->invisible_char_set);
      break;

    case PROP_IM_MODULE:
      g_value_set_string (value, priv->im_module);
      break;

    case PROP_CAPS_LOCK_WARNING:
      g_value_set_boolean (value, priv->caps_lock_warning);
      break;

    case PROP_PROGRESS_FRACTION:
      g_value_set_double (value, gtk_entry_get_progress_fraction (entry));
      break;

    case PROP_PROGRESS_PULSE_STEP:
      g_value_set_double (value, gtk_entry_get_progress_pulse_step (entry));
      break;

    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, gtk_entry_get_placeholder_text (entry));
      break;

    case PROP_PAINTABLE_PRIMARY:
      g_value_set_object (value,
                          gtk_entry_get_icon_paintable (entry,
                                                        GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_PAINTABLE_SECONDARY:
      g_value_set_object (value,
                          gtk_entry_get_icon_paintable (entry,
                                                        GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_ICON_NAME_PRIMARY:
      g_value_set_string (value,
                          gtk_entry_get_icon_name (entry,
                                                   GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_ICON_NAME_SECONDARY:
      g_value_set_string (value,
                          gtk_entry_get_icon_name (entry,
                                                   GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_GICON_PRIMARY:
      g_value_set_object (value,
                          gtk_entry_get_icon_gicon (entry,
                                                    GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_GICON_SECONDARY:
      g_value_set_object (value,
                          gtk_entry_get_icon_gicon (entry,
                                                    GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_STORAGE_TYPE_PRIMARY:
      g_value_set_enum (value,
                        gtk_entry_get_icon_storage_type (entry, 
                                                         GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_STORAGE_TYPE_SECONDARY:
      g_value_set_enum (value,
                        gtk_entry_get_icon_storage_type (entry, 
                                                         GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_ACTIVATABLE_PRIMARY:
      g_value_set_boolean (value,
                           gtk_entry_get_icon_activatable (entry, GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_ACTIVATABLE_SECONDARY:
      g_value_set_boolean (value,
                           gtk_entry_get_icon_activatable (entry, GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_SENSITIVE_PRIMARY:
      g_value_set_boolean (value,
                           gtk_entry_get_icon_sensitive (entry, GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_SENSITIVE_SECONDARY:
      g_value_set_boolean (value,
                           gtk_entry_get_icon_sensitive (entry, GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_TOOLTIP_TEXT_PRIMARY:
      g_value_take_string (value,
                           gtk_entry_get_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_TOOLTIP_TEXT_SECONDARY:
      g_value_take_string (value,
                           gtk_entry_get_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_TOOLTIP_MARKUP_PRIMARY:
      g_value_take_string (value,
                           gtk_entry_get_icon_tooltip_markup (entry, GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_TOOLTIP_MARKUP_SECONDARY:
      g_value_take_string (value,
                           gtk_entry_get_icon_tooltip_markup (entry, GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_EDITING_CANCELED:
      g_value_set_boolean (value,
                           priv->editing_canceled);
      break;

    case PROP_COMPLETION:
      g_value_set_object (value, G_OBJECT (gtk_entry_get_completion (entry)));
      break;

    case PROP_INPUT_PURPOSE:
      g_value_set_enum (value, gtk_entry_get_input_purpose (entry));
      break;

    case PROP_INPUT_HINTS:
      g_value_set_flags (value, gtk_entry_get_input_hints (entry));
      break;

    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, priv->attrs);
      break;

    case PROP_POPULATE_ALL:
      g_value_set_boolean (value, priv->populate_all);
      break;

    case PROP_TABS:
      g_value_set_boxed (value, priv->tabs);
      break;

    case PROP_SHOW_EMOJI_ICON:
      g_value_set_boolean (value, priv->show_emoji_icon);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
set_text_cursor (GtkWidget *widget)
{
  gtk_widget_set_cursor_from_name (widget, "text");
}

static void
entry_motion_cb (GtkEventControllerMotion *event_controller,
                 double                    x,
                 double                    y,
                 gpointer                  user_data)
{
  GtkEntry *entry = user_data;
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->mouse_cursor_obscured)
    {
      set_text_cursor (GTK_WIDGET (entry));
      priv->mouse_cursor_obscured = FALSE;
    }
}

static gunichar
find_invisible_char (GtkWidget *widget)
{
  PangoLayout *layout;
  PangoAttrList *attr_list;
  gint i;
  gunichar invisible_chars [] = {
    0x25cf, /* BLACK CIRCLE */
    0x2022, /* BULLET */
    0x2731, /* HEAVY ASTERISK */
    0x273a  /* SIXTEEN POINTED ASTERISK */
  };

  layout = gtk_widget_create_pango_layout (widget, NULL);

  attr_list = pango_attr_list_new ();
  pango_attr_list_insert (attr_list, pango_attr_fallback_new (FALSE));

  pango_layout_set_attributes (layout, attr_list);
  pango_attr_list_unref (attr_list);

  for (i = 0; i < G_N_ELEMENTS (invisible_chars); i++)
    {
      gchar text[7] = { 0, };
      gint len, count;

      len = g_unichar_to_utf8 (invisible_chars[i], text);
      pango_layout_set_text (layout, text, len);

      count = pango_layout_get_unknown_glyphs_count (layout);

      if (count == 0)
        {
          g_object_unref (layout);
          return invisible_chars[i];
        }
    }

  g_object_unref (layout);

  return '*';
}

static void
gtk_entry_schedule_im_reset (GtkEntry *entry)
{
  GtkEntryPrivate *priv;

  priv = gtk_entry_get_instance_private (entry);

  priv->need_im_reset = TRUE;
}

static void
gtk_entry_init (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkCssNode *widget_node;
  gint i;

  gtk_widget_set_can_focus (GTK_WIDGET (entry), TRUE);
  gtk_widget_set_has_surface (GTK_WIDGET (entry), FALSE);

  priv->editable = TRUE;
  priv->visible = TRUE;
  priv->dnd_position = -1;
  priv->width_chars = -1;
  priv->max_width_chars = -1;
  priv->editing_canceled = FALSE;
  priv->truncate_multiline = FALSE;
  priv->xalign = 0.0;
  priv->caps_lock_warning = TRUE;
  priv->caps_lock_warning_shown = FALSE;
  priv->insert_pos = -1;

  priv->selection_content = g_object_new (GTK_TYPE_ENTRY_CONTENT, NULL);
  GTK_ENTRY_CONTENT (priv->selection_content)->entry = entry;

  gtk_drag_dest_set (GTK_WIDGET (entry), 0, NULL,
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_drag_dest_add_text_targets (GTK_WIDGET (entry));

  /* This object is completely private. No external entity can gain a reference
   * to it; so we create it here and destroy it in finalize().
   */
  priv->im_context = gtk_im_multicontext_new ();

  g_signal_connect (priv->im_context, "commit",
		    G_CALLBACK (gtk_entry_commit_cb), entry);
  g_signal_connect (priv->im_context, "preedit-changed",
		    G_CALLBACK (gtk_entry_preedit_changed_cb), entry);
  g_signal_connect (priv->im_context, "retrieve-surrounding",
		    G_CALLBACK (gtk_entry_retrieve_surrounding_cb), entry);
  g_signal_connect (priv->im_context, "delete-surrounding",
		    G_CALLBACK (gtk_entry_delete_surrounding_cb), entry);

  gtk_entry_update_cached_style_values (entry);

  priv->drag_gesture = gtk_gesture_drag_new (GTK_WIDGET (entry));
  g_signal_connect (priv->drag_gesture, "drag-update",
                    G_CALLBACK (gtk_entry_drag_gesture_update), entry);
  g_signal_connect (priv->drag_gesture, "drag-end",
                    G_CALLBACK (gtk_entry_drag_gesture_end), entry);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->drag_gesture), 0);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (priv->drag_gesture), TRUE);

  priv->multipress_gesture = gtk_gesture_multi_press_new (GTK_WIDGET (entry));
  g_signal_connect (priv->multipress_gesture, "pressed",
                    G_CALLBACK (gtk_entry_multipress_gesture_pressed), entry);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->multipress_gesture), 0);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (priv->multipress_gesture), TRUE);

  priv->motion_controller = gtk_event_controller_motion_new (GTK_WIDGET (entry));
  g_signal_connect (priv->motion_controller, "motion",
                    G_CALLBACK (entry_motion_cb), entry);

  priv->key_controller = gtk_event_controller_key_new (GTK_WIDGET (entry));
  g_signal_connect (priv->key_controller, "key-pressed",
                    G_CALLBACK (gtk_entry_key_controller_key_pressed), entry);
  g_signal_connect_swapped (priv->key_controller, "im-update",
                            G_CALLBACK (gtk_entry_schedule_im_reset), entry);
  g_signal_connect_swapped (priv->key_controller, "focus-in",
                            G_CALLBACK (gtk_entry_focus_in), entry);
  g_signal_connect_swapped (priv->key_controller, "focus-out",
                            G_CALLBACK (gtk_entry_focus_out), entry);
  gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (priv->key_controller),
                                           priv->im_context);

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (entry));
  for (i = 0; i < 2; i++)
    {
      priv->undershoot_node[i] = gtk_css_node_new ();
      gtk_css_node_set_name (priv->undershoot_node[i], I_("undershoot"));
      gtk_css_node_add_class (priv->undershoot_node[i], g_quark_from_static_string (i == 0 ? GTK_STYLE_CLASS_LEFT : GTK_STYLE_CLASS_RIGHT));
      gtk_css_node_set_parent (priv->undershoot_node[i], widget_node);
      gtk_css_node_set_state (priv->undershoot_node[i], gtk_css_node_get_state (widget_node) & ~GTK_STATE_FLAG_DROP_ACTIVE);
      g_object_unref (priv->undershoot_node[i]);
    }

  set_text_cursor (GTK_WIDGET (entry));
}

static void
gtk_entry_ensure_magnifier (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->magnifier_popover)
    return;

  priv->magnifier = _gtk_magnifier_new (GTK_WIDGET (entry));
  gtk_widget_set_size_request (priv->magnifier, 100, 60);
  _gtk_magnifier_set_magnification (GTK_MAGNIFIER (priv->magnifier), 2.0);
  priv->magnifier_popover = gtk_popover_new (GTK_WIDGET (entry));
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->magnifier_popover),
                               "magnifier");
  gtk_popover_set_modal (GTK_POPOVER (priv->magnifier_popover), FALSE);
  gtk_container_add (GTK_CONTAINER (priv->magnifier_popover),
                     priv->magnifier);
  gtk_widget_show (priv->magnifier);
}

static void
gtk_entry_ensure_text_handles (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->text_handle)
    return;

  priv->text_handle = _gtk_text_handle_new (GTK_WIDGET (entry));
  g_signal_connect (priv->text_handle, "drag-started",
                    G_CALLBACK (gtk_entry_handle_drag_started), entry);
  g_signal_connect (priv->text_handle, "handle-dragged",
                    G_CALLBACK (gtk_entry_handle_dragged), entry);
  g_signal_connect (priv->text_handle, "drag-finished",
                    G_CALLBACK (gtk_entry_handle_drag_finished), entry);
}

static gint
get_icon_width (GtkEntry             *entry,
                GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info = priv->icons[icon_pos];
  gint width;

  if (!icon_info)
    return 0;

  gtk_widget_measure (icon_info->widget,
                      GTK_ORIENTATION_HORIZONTAL,
                      -1,
                      &width, NULL,
                      NULL, NULL);

  return width;
}

static void
begin_change (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  priv->change_count++;

  g_object_freeze_notify (G_OBJECT (entry));
}

static void
end_change (GtkEntry *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (priv->change_count > 0);

  g_object_thaw_notify (G_OBJECT (entry));

  priv->change_count--;

  if (priv->change_count == 0)
    {
       if (priv->real_changed)
         {
           g_signal_emit_by_name (editable, "changed");
           priv->real_changed = FALSE;
         }
    }
}

static void
emit_changed (GtkEntry *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->change_count == 0)
    g_signal_emit_by_name (editable, "changed");
  else
    priv->real_changed = TRUE;
}

static void
gtk_entry_destroy (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  priv->current_pos = priv->selection_bound = 0;
  gtk_entry_reset_im_context (entry);
  gtk_entry_reset_layout (entry);

  if (priv->blink_timeout)
    {
      g_source_remove (priv->blink_timeout);
      priv->blink_timeout = 0;
    }

  if (priv->magnifier)
    _gtk_magnifier_set_inspected (GTK_MAGNIFIER (priv->magnifier), NULL);

  GTK_WIDGET_CLASS (gtk_entry_parent_class)->destroy (widget);
}

static void
gtk_entry_dispose (GObject *object)
{
  GtkEntry *entry = GTK_ENTRY (object);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GdkKeymap *keymap;

  gtk_entry_set_icon_from_paintable (entry, GTK_ENTRY_ICON_PRIMARY, NULL);
  gtk_entry_set_icon_tooltip_markup (entry, GTK_ENTRY_ICON_PRIMARY, NULL);
  gtk_entry_set_icon_from_paintable (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
  gtk_entry_set_icon_tooltip_markup (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
  gtk_entry_set_completion (entry, NULL);

  priv->current_pos = 0;

  if (priv->buffer)
    {
      buffer_disconnect_signals (entry);
      g_object_unref (priv->buffer);
      priv->buffer = NULL;
    }

  keymap = gdk_display_get_keymap (gtk_widget_get_display (GTK_WIDGET (object)));
  g_signal_handlers_disconnect_by_func (keymap, keymap_state_changed, entry);
  g_signal_handlers_disconnect_by_func (keymap, keymap_direction_changed, entry);
  G_OBJECT_CLASS (gtk_entry_parent_class)->dispose (object);
}

static void
gtk_entry_finalize (GObject *object)
{
  GtkEntry *entry = GTK_ENTRY (object);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info = NULL;
  gint i;

  g_clear_object (&priv->selection_content);

  for (i = 0; i < MAX_ICONS; i++)
    {
      icon_info = priv->icons[i];
      if (icon_info == NULL)
        continue;

      if (icon_info->target_list != NULL)
        gdk_content_formats_unref (icon_info->target_list);

      gtk_widget_unparent (icon_info->widget);

      g_slice_free (EntryIconInfo, icon_info);
    }

  if (priv->cached_layout)
    g_object_unref (priv->cached_layout);

  g_object_unref (priv->im_context);

  if (priv->blink_timeout)
    g_source_remove (priv->blink_timeout);

  if (priv->selection_bubble)
    gtk_widget_destroy (priv->selection_bubble);

  if (priv->magnifier_popover)
    gtk_widget_destroy (priv->magnifier_popover);

  if (priv->text_handle)
    g_object_unref (priv->text_handle);
  g_free (priv->placeholder_text);
  g_free (priv->im_module);

  g_clear_object (&priv->drag_gesture);
  g_clear_object (&priv->multipress_gesture);
  g_clear_object (&priv->motion_controller);

  if (priv->tabs)
    pango_tab_array_free (priv->tabs);

  if (priv->attrs)
    pango_attr_list_unref (priv->attrs);

  if (priv->progress_widget)
    gtk_widget_unparent (priv->progress_widget);

  G_OBJECT_CLASS (gtk_entry_parent_class)->finalize (object);
}

static DisplayMode
gtk_entry_get_display_mode (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->visible)
    return DISPLAY_NORMAL;

  if (priv->invisible_char == 0 && priv->invisible_char_set)
    return DISPLAY_BLANK;

  return DISPLAY_INVISIBLE;
}

gchar*
_gtk_entry_get_display_text (GtkEntry *entry,
                             gint      start_pos,
                             gint      end_pos)
{
  GtkEntryPasswordHint *password_hint;
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gunichar invisible_char;
  const gchar *start;
  const gchar *end;
  const gchar *text;
  gchar char_str[7];
  gint char_len;
  GString *str;
  guint length;
  gint i;

  text = gtk_entry_buffer_get_text (get_buffer (entry));
  length = gtk_entry_buffer_get_length (get_buffer (entry));

  if (end_pos < 0 || end_pos > length)
    end_pos = length;
  if (start_pos > length)
    start_pos = length;

  if (end_pos <= start_pos)
      return g_strdup ("");
  else if (priv->visible)
    {
      start = g_utf8_offset_to_pointer (text, start_pos);
      end = g_utf8_offset_to_pointer (start, end_pos - start_pos);
      return g_strndup (start, end - start);
    }
  else
    {
      str = g_string_sized_new (length * 2);

      /* Figure out what our invisible char is and encode it */
      if (!priv->invisible_char)
          invisible_char = priv->invisible_char_set ? ' ' : '*';
      else
          invisible_char = priv->invisible_char;
      char_len = g_unichar_to_utf8 (invisible_char, char_str);

      /*
       * Add hidden characters for each character in the text
       * buffer. If there is a password hint, then keep that
       * character visible.
       */

      password_hint = g_object_get_qdata (G_OBJECT (entry), quark_password_hint);
      for (i = start_pos; i < end_pos; ++i)
        {
          if (password_hint && i == password_hint->position)
            {
              start = g_utf8_offset_to_pointer (text, i);
              g_string_append_len (str, start, g_utf8_next_char (start) - start);
            }
          else
            {
              g_string_append_len (str, char_str, char_len);
            }
        }

      return g_string_free (str, FALSE);
    }
}

static void
update_icon_style (GtkWidget            *widget,
                   GtkEntryIconPosition  icon_pos)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info = priv->icons[icon_pos];
  const gchar *sides[2] = { GTK_STYLE_CLASS_LEFT, GTK_STYLE_CLASS_RIGHT };
  GtkStyleContext *context;

  if (icon_info == NULL)
    return;

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    icon_pos = 1 - icon_pos;

  context = gtk_widget_get_style_context (icon_info->widget);
  gtk_style_context_add_class (context, sides[icon_pos]);
  gtk_style_context_remove_class (context, sides[1 - icon_pos]);
}

static void
update_node_state (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkStateFlags state;

  state = gtk_widget_get_state_flags (GTK_WIDGET (entry));
  state &= ~GTK_STATE_FLAG_DROP_ACTIVE;

  if (priv->selection_node)
    gtk_css_node_set_state (priv->selection_node, state);

  if (priv->block_cursor_node)
    gtk_css_node_set_state (priv->block_cursor_node, state);

  gtk_css_node_set_state (priv->undershoot_node[0], state);
  gtk_css_node_set_state (priv->undershoot_node[1], state);
}

static void
update_node_ordering (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;
  GtkEntryIconPosition first_icon_pos, second_icon_pos;
  GtkCssNode *parent;

  if (priv->progress_widget)
    {
      gtk_css_node_insert_before (gtk_widget_get_css_node (GTK_WIDGET (entry)),
                                  gtk_widget_get_css_node (priv->progress_widget),
                                  NULL);
    }

  if (gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_RTL)
    {
      first_icon_pos = GTK_ENTRY_ICON_SECONDARY;
      second_icon_pos = GTK_ENTRY_ICON_PRIMARY;
    }
  else
    {
      first_icon_pos = GTK_ENTRY_ICON_PRIMARY;
      second_icon_pos = GTK_ENTRY_ICON_SECONDARY;
    }

  parent = gtk_widget_get_css_node (GTK_WIDGET (entry));

  icon_info = priv->icons[first_icon_pos];
  if (icon_info)
    gtk_css_node_insert_after (parent, gtk_widget_get_css_node (icon_info->widget), NULL);

  icon_info = priv->icons[second_icon_pos];
  if (icon_info)
    gtk_css_node_insert_before (parent, gtk_widget_get_css_node (icon_info->widget), NULL);
}

static EntryIconInfo*
construct_icon_info (GtkWidget            *widget,
                     GtkEntryIconPosition  icon_pos)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (priv->icons[icon_pos] == NULL, NULL);

  icon_info = g_slice_new0 (EntryIconInfo);
  priv->icons[icon_pos] = icon_info;

  icon_info->widget = gtk_image_new ();
  gtk_widget_set_cursor_from_name (icon_info->widget, "default");
  gtk_widget_set_parent (icon_info->widget, widget);

  update_icon_style (widget, icon_pos);
  update_node_ordering (entry);

  return icon_info;
}

static void
gtk_entry_unmap (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->text_handle)
    _gtk_text_handle_set_mode (priv->text_handle,
                               GTK_TEXT_HANDLE_MODE_NONE);

  GTK_WIDGET_CLASS (gtk_entry_parent_class)->unmap (widget);
}

static void  
gtk_entry_get_text_allocation (GtkEntry     *entry,
                               GdkRectangle *allocation)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  allocation->x = priv->text_x;
  allocation->y = 0;
  allocation->width = priv->text_width;
  allocation->height = gtk_widget_get_height (GTK_WIDGET (entry));
}

static void
gtk_entry_realize (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  GTK_WIDGET_CLASS (gtk_entry_parent_class)->realize (widget);

  gtk_im_context_set_client_widget (priv->im_context, widget);

  gtk_entry_adjust_scroll (entry);
  gtk_entry_update_primary_selection (entry);
}

static void
gtk_entry_unrealize (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GdkClipboard *clipboard;

  gtk_entry_reset_layout (entry);
  
  gtk_im_context_set_client_widget (priv->im_context, NULL);

  clipboard = gtk_widget_get_primary_clipboard (widget);
  if (gdk_clipboard_get_content (clipboard) == priv->selection_content)
    gdk_clipboard_set_content (clipboard, NULL);

  if (priv->popup_menu)
    {
      gtk_widget_destroy (priv->popup_menu);
      priv->popup_menu = NULL;
    }

  GTK_WIDGET_CLASS (gtk_entry_parent_class)->unrealize (widget);
}

static void
gtk_entry_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int             *minimum,
                   int             *natural,
                   int             *minimum_baseline,
                   int             *natural_baseline)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  PangoContext *context;
  PangoFontMetrics *metrics;

  context = gtk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (context,
                                       pango_context_get_font_description (context),
                                       pango_context_get_language (context));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gint icon_width, i;
      gint min, nat;
      gint char_width;
      gint digit_width;
      gint char_pixels;

      char_width = pango_font_metrics_get_approximate_char_width (metrics);
      digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
      char_pixels = (MAX (char_width, digit_width) + PANGO_SCALE - 1) / PANGO_SCALE;

      if (priv->width_chars < 0)
        min = MIN_ENTRY_WIDTH;
      else
        min = char_pixels * priv->width_chars;

      if (priv->max_width_chars < 0)
        nat = min;
      else
        nat = char_pixels * priv->max_width_chars;

      icon_width = 0;
      for (i = 0; i < MAX_ICONS; i++)
        icon_width += get_icon_width (entry, i);

      min = MAX (min, icon_width);
      nat = MAX (min, nat);

      *minimum = min;
      *natural = nat;
    }
  else
    {
      gint height, baseline;
      gint icon_height, i;
      PangoLayout *layout;

      layout = gtk_entry_ensure_layout (entry, TRUE);

      priv->ascent = pango_font_metrics_get_ascent (metrics);
      priv->descent = pango_font_metrics_get_descent (metrics);

      pango_layout_get_pixel_size (layout, NULL, &height);

      height = MAX (height, PANGO_PIXELS (priv->ascent + priv->descent));

      baseline = pango_layout_get_baseline (layout) / PANGO_SCALE;

      icon_height = 0;
      for (i = 0; i < MAX_ICONS; i++)
        {
          EntryIconInfo *icon_info = priv->icons[i];
          gint h;

          if (!icon_info)
            continue;

          gtk_widget_measure (icon_info->widget,
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              NULL, &h,
                              NULL, NULL);
          icon_height = MAX (icon_height, h);
        }

      *minimum = MAX (height, icon_height);
      *natural = MAX (height, icon_height);

      if (icon_height > height)
        baseline += (icon_height - height) / 2;

      if (minimum_baseline)
        *minimum_baseline = baseline;
      if (natural_baseline)
        *natural_baseline = baseline;
    }

  pango_font_metrics_unref (metrics);

  if (priv->progress_widget && gtk_widget_get_visible (priv->progress_widget))
    {
      int prog_min, prog_nat;

      gtk_widget_measure (priv->progress_widget,
                          orientation,
                          for_size,
                          &prog_min, &prog_nat,
                          NULL, NULL);

      *minimum = MAX (*minimum, prog_min);
      *natural = MAX (*natural, prog_nat);
    }
}

static void
gtk_entry_size_allocate (GtkWidget           *widget,
                         const GtkAllocation *allocation,
                         int                  baseline)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gint i;

  priv->text_baseline = baseline;
  priv->text_x = 0;
  priv->text_width = allocation->width;

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];
      GtkAllocation icon_alloc;
      int width;

      if (!icon_info)
        continue;

      gtk_widget_measure (icon_info->widget,
                          GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          NULL, &width,
                          NULL, NULL);

      if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL && i == GTK_ENTRY_ICON_PRIMARY) ||
          (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR && i == GTK_ENTRY_ICON_SECONDARY))
        {
          icon_alloc.x = allocation->x + priv->text_x + priv->text_width - width;
        }
      else
        {
          icon_alloc.x = allocation->x + priv->text_x;
          priv->text_x += width;
        }
      icon_alloc.y = 0;
      icon_alloc.width = width;
      icon_alloc.height = allocation->height;
      priv->text_width -= width;

      gtk_widget_size_allocate (icon_info->widget, &icon_alloc, baseline);
    }

  if (priv->progress_widget && gtk_widget_get_visible (priv->progress_widget))
    {
      GtkAllocation progress_alloc;
      int min, nat;

      gtk_widget_measure (priv->progress_widget,
                          GTK_ORIENTATION_VERTICAL,
                          -1,
                          &min, &nat,
                          NULL, NULL);
      progress_alloc.x = 0;
      progress_alloc.y = allocation->height - nat;
      progress_alloc.width = allocation->width;
      progress_alloc.height = nat;

      gtk_widget_size_allocate (priv->progress_widget, &progress_alloc, -1);
    }

  /* Do this here instead of gtk_entry_size_allocate() so it works
   * inside spinbuttons, which don't chain up.
   */
  if (gtk_widget_get_realized (widget))
    {
      GtkEntryCompletion *completion;

      gtk_entry_recompute (entry);

      completion = gtk_entry_get_completion (entry);
      if (completion)
        _gtk_entry_completion_resize_popup (completion);
    }
}

static void
gtk_entry_draw_undershoot (GtkEntry    *entry,
                           GtkSnapshot *snapshot)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkStyleContext *context;
  gint min_offset, max_offset;
  GdkRectangle rect;
  gboolean rtl;

  context = gtk_widget_get_style_context (GTK_WIDGET (entry));
  rtl = gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_RTL;

  gtk_entry_get_scroll_limits (entry, &min_offset, &max_offset);

  gtk_entry_get_text_allocation (entry, &rect);

  if (priv->scroll_offset > min_offset)
    {
      int icon_width = 0;
      int icon_idx = rtl ? 1 : 0;
      if (priv->icons[icon_idx] != NULL)
        {
           gtk_widget_measure (priv->icons[icon_idx]->widget,
                               GTK_ORIENTATION_HORIZONTAL,
                               -1,
                               &icon_width, NULL,
                               NULL, NULL);
        }

      gtk_style_context_save_to_node (context, priv->undershoot_node[0]);
      gtk_snapshot_render_background (snapshot, context, rect.x, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_snapshot_render_frame (snapshot, context, rect.x, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_style_context_restore (context);
    }

  if (priv->scroll_offset < max_offset)
    {
      int icon_width = 0;
      int icon_idx = rtl ? 0 : 1;
      if (priv->icons[icon_idx] != NULL)
        {
           gtk_widget_measure (priv->icons[icon_idx]->widget,
                               GTK_ORIENTATION_HORIZONTAL,
                               -1,
                               &icon_width, NULL,
                               NULL, NULL);
        }
      gtk_style_context_save_to_node (context, priv->undershoot_node[1]);
      gtk_snapshot_render_background (snapshot, context, rect.x + rect.width - UNDERSHOOT_SIZE, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_snapshot_render_frame (snapshot, context, rect.x + rect.width - UNDERSHOOT_SIZE, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_style_context_restore (context);
    }
}

static void
gtk_entry_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  int i;

  /* Draw progress */
  if (priv->progress_widget && gtk_widget_get_visible (priv->progress_widget))
    gtk_widget_snapshot_child (widget, priv->progress_widget, snapshot);

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT (
                            priv->text_x,
                            0,
                            priv->text_width,
                            gtk_widget_get_height (widget)),
                          "Entry Clip");

  /* Draw text and cursor */
  if (priv->dnd_position != -1)
    gtk_entry_draw_cursor (GTK_ENTRY (widget), snapshot, CURSOR_DND);

  gtk_entry_draw_text (GTK_ENTRY (widget), snapshot);

  /* When no text is being displayed at all, don't show the cursor */
  if (gtk_entry_get_display_mode (entry) != DISPLAY_BLANK &&
      gtk_widget_has_focus (widget) &&
      priv->selection_bound == priv->current_pos && priv->cursor_visible)
    gtk_entry_draw_cursor (GTK_ENTRY (widget), snapshot, CURSOR_STANDARD);

  gtk_snapshot_pop (snapshot);

  /* Draw icons */
  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];

      if (icon_info != NULL)
        gtk_widget_snapshot_child (widget, icon_info->widget, snapshot);
    }

  gtk_entry_draw_undershoot (entry, snapshot);
}

static void
gtk_entry_get_pixel_ranges (GtkEntry  *entry,
			    gint     **ranges,
			    gint      *n_ranges)
{
  gint start_char, end_char;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start_char, &end_char))
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, TRUE);
      PangoLayoutLine *line = pango_layout_get_lines_readonly (layout)->data;
      const char *text = pango_layout_get_text (layout);
      gint start_index = g_utf8_offset_to_pointer (text, start_char) - text;
      gint end_index = g_utf8_offset_to_pointer (text, end_char) - text;
      gint real_n_ranges, i;

      pango_layout_line_get_x_ranges (line, start_index, end_index, ranges, &real_n_ranges);

      if (ranges)
	{
	  gint *r = *ranges;
	  
	  for (i = 0; i < real_n_ranges; ++i)
	    {
	      r[2 * i + 1] = (r[2 * i + 1] - r[2 * i]) / PANGO_SCALE;
	      r[2 * i] = r[2 * i] / PANGO_SCALE;
	    }
	}
      
      if (n_ranges)
	*n_ranges = real_n_ranges;
    }
  else
    {
      if (n_ranges)
	*n_ranges = 0;
      if (ranges)
	*ranges = NULL;
    }
}

static gboolean
in_selection (GtkEntry *entry,
	      gint	x)
{
  gint *ranges;
  gint n_ranges, i;
  gint retval = FALSE;

  gtk_entry_get_pixel_ranges (entry, &ranges, &n_ranges);

  for (i = 0; i < n_ranges; ++i)
    {
      if (x >= ranges[2 * i] && x < ranges[2 * i] + ranges[2 * i + 1])
	{
	  retval = TRUE;
	  break;
	}
    }

  g_free (ranges);
  return retval;
}

static void
gtk_entry_move_handle (GtkEntry              *entry,
                       GtkTextHandlePosition  pos,
                       gint                   x,
                       gint                   y,
                       gint                   height)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkAllocation text_allocation;

  gtk_entry_get_text_allocation (entry, &text_allocation);

  if (!_gtk_text_handle_get_is_dragged (priv->text_handle, pos) &&
      (x < 0 || x > text_allocation.width))
    {
      /* Hide the handle if it's not being manipulated
       * and fell outside of the visible text area.
       */
      _gtk_text_handle_set_visible (priv->text_handle, pos, FALSE);
    }
  else
    {
      GtkAllocation allocation;
      GdkRectangle rect;

      gtk_widget_get_allocation (GTK_WIDGET (entry), &allocation);
      rect.x = x + text_allocation.x - allocation.x;
      rect.y = y + text_allocation.y - allocation.y;
      rect.width = 1;
      rect.height = height;

      _gtk_text_handle_set_visible (priv->text_handle, pos, TRUE);
      _gtk_text_handle_set_position (priv->text_handle, pos, &rect);
      _gtk_text_handle_set_direction (priv->text_handle, pos, priv->resolved_dir);
    }
}

static gint
gtk_entry_get_selection_bound_location (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  PangoLayout *layout;
  PangoRectangle pos;
  gint x;
  const gchar *text;
  gint index;

  layout = gtk_entry_ensure_layout (entry, FALSE);
  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, priv->selection_bound) - text;
  pango_layout_index_to_pos (layout, index, &pos);

  if (gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_RTL)
    x = (pos.x + pos.width) / PANGO_SCALE;
  else
    x = pos.x / PANGO_SCALE;

  return x;
}

static void
gtk_entry_update_handles (GtkEntry          *entry,
                          GtkTextHandleMode  mode)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkAllocation text_allocation;
  gint strong_x;
  gint cursor, bound;

  _gtk_text_handle_set_mode (priv->text_handle, mode);
  gtk_entry_get_text_allocation (entry, &text_allocation);

  gtk_entry_get_cursor_locations (entry, &strong_x, NULL);
  cursor = strong_x - priv->scroll_offset;

  if (mode == GTK_TEXT_HANDLE_MODE_SELECTION)
    {
      gint start, end;

      bound = gtk_entry_get_selection_bound_location (entry) - priv->scroll_offset;

      if (priv->selection_bound > priv->current_pos)
        {
          start = cursor;
          end = bound;
        }
      else
        {
          start = bound;
          end = cursor;
        }

      /* Update start selection bound */
      gtk_entry_move_handle (entry, GTK_TEXT_HANDLE_POSITION_SELECTION_START,
                             start, 0, text_allocation.height);
      gtk_entry_move_handle (entry, GTK_TEXT_HANDLE_POSITION_SELECTION_END,
                             end, 0, text_allocation.height);
    }
  else
    gtk_entry_move_handle (entry, GTK_TEXT_HANDLE_POSITION_CURSOR,
                           cursor, 0, text_allocation.height);
}

static gboolean
gtk_entry_event (GtkWidget *widget,
                 GdkEvent  *event)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (GTK_ENTRY (widget));
  EntryIconInfo *icon_info = NULL;
  GdkEventSequence *sequence;
  GdkDevice *device;
  gdouble x, y;
  gint i = 0;

  if (!gdk_event_get_coords (event, &x, &y))
    return GDK_EVENT_PROPAGATE;

  for (i = 0; i < MAX_ICONS; i++)
    {
      if (priv->icons[i])
        {
          int icon_x, icon_y;
          gtk_widget_translate_coordinates (widget, priv->icons[i]->widget,
                                            x, y, &icon_x, &icon_y);
          if (gtk_widget_contains (priv->icons[i]->widget, icon_x, icon_y))
            {
              icon_info = priv->icons[i];
              break;
            }
        }
    }

  if (!icon_info)
    return GDK_EVENT_PROPAGATE;

  if (!gtk_widget_get_sensitive (icon_info->widget))
    return GDK_EVENT_STOP;

  sequence = gdk_event_get_event_sequence (event);
  device = gdk_event_get_device (event);

  switch ((guint) gdk_event_get_event_type (event))
    {
    case GDK_TOUCH_BEGIN:
      if (icon_info->current_sequence)
        break;

      icon_info->current_sequence = sequence;
      /* Fall through */
    case GDK_BUTTON_PRESS:
      priv->start_x = x;
      priv->start_y = y;
      icon_info->pressed = TRUE;
      icon_info->device = device;

      if (!icon_info->nonactivatable) {
        g_signal_emit (widget, signals[ICON_PRESS], 0, i, event);
      }

      break;
    case GDK_TOUCH_UPDATE:
      if (icon_info->device != device ||
          icon_info->current_sequence != sequence)
        break;
      /* Fall through */
    case GDK_MOTION_NOTIFY:
      if (icon_info->pressed &&
          icon_info->target_list != NULL &&
              gtk_drag_check_threshold (widget,
                                        priv->start_x,
                                        priv->start_y,
                                        x, y))
        {
          icon_info->in_drag = TRUE;
          gtk_drag_begin_with_coordinates (widget,
                                           device,
                                           icon_info->target_list,
                                           icon_info->actions,
                                           priv->start_x,
                                           priv->start_y);
        }

      break;
    case GDK_TOUCH_END:
      if (icon_info->device != device ||
          icon_info->current_sequence != sequence)
        break;

      icon_info->current_sequence = NULL;
      /* Fall through */
    case GDK_BUTTON_RELEASE:
      icon_info->pressed = FALSE;
      icon_info->device = NULL;

      if (!icon_info->nonactivatable)
        g_signal_emit (widget, signals[ICON_RELEASE], 0, i, event);

      break;
    default:
      return GDK_EVENT_PROPAGATE;
    }

  return GDK_EVENT_STOP;
}

static void
gesture_get_current_point_in_layout (GtkGestureSingle *gesture,
                                     GtkEntry         *entry,
                                     gint             *x,
                                     gint             *y)
{
  gint tx, ty;
  GdkEventSequence *sequence;
  gdouble px, py;

  sequence = gtk_gesture_single_get_current_sequence (gesture);
  gtk_gesture_get_point (GTK_GESTURE (gesture), sequence, &px, &py);
  gtk_entry_get_layout_offsets (entry, &tx, &ty);

  if (x)
    *x = px - tx;
  if (y)
    *y = py - ty;
}

static void
gtk_entry_multipress_gesture_pressed (GtkGestureMultiPress *gesture,
                                      gint                  n_press,
                                      gdouble               widget_x,
                                      gdouble               widget_y,
                                      GtkEntry             *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GdkEventSequence *current;
  const GdkEvent *event;
  gint x, y, sel_start, sel_end;
  guint button;
  gint tmp_pos;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), current);

  gtk_gesture_set_sequence_state (GTK_GESTURE (gesture), current,
                                  GTK_EVENT_SEQUENCE_CLAIMED);
  gesture_get_current_point_in_layout (GTK_GESTURE_SINGLE (gesture), entry, &x, &y);
  gtk_entry_reset_blink_time (entry);

  if (!gtk_widget_has_focus (widget))
    {
      priv->in_click = TRUE;
      gtk_widget_grab_focus (widget);
      priv->in_click = FALSE;
    }

  tmp_pos = gtk_entry_find_position (entry, x);

  if (gdk_event_triggers_context_menu (event))
    {
      gtk_entry_do_popup (entry, event);
    }
  else if (n_press == 1 && button == GDK_BUTTON_MIDDLE &&
           get_middle_click_paste (entry))
    {
      if (priv->editable)
        {
          priv->insert_pos = tmp_pos;
          gtk_entry_paste (entry, gtk_widget_get_primary_clipboard (widget));
        }
      else
        {
          gtk_widget_error_bell (widget);
        }
    }
  else if (button == GDK_BUTTON_PRIMARY)
    {
      gboolean have_selection = gtk_editable_get_selection_bounds (editable, &sel_start, &sel_end);
      GtkTextHandleMode mode;
      gboolean is_touchscreen, extend_selection;
      GdkDevice *source;
      guint state;

      source = gdk_event_get_source_device (event);
      is_touchscreen = gtk_simulate_touchscreen () ||
                       gdk_device_get_source (source) == GDK_SOURCE_TOUCHSCREEN;

      if (!is_touchscreen)
        mode = GTK_TEXT_HANDLE_MODE_NONE;
      else if (have_selection)
        mode = GTK_TEXT_HANDLE_MODE_SELECTION;
      else
        mode = GTK_TEXT_HANDLE_MODE_CURSOR;

      if (is_touchscreen)
        gtk_entry_ensure_text_handles (entry);

      priv->in_drag = FALSE;
      priv->select_words = FALSE;
      priv->select_lines = FALSE;

      gdk_event_get_state (event, &state);

      extend_selection =
        (state &
         gtk_widget_get_modifier_mask (widget,
                                       GDK_MODIFIER_INTENT_EXTEND_SELECTION));

      if (extend_selection)
        gtk_entry_reset_im_context (entry);

      switch (n_press)
        {
        case 1:
          if (in_selection (entry, x))
            {
              if (is_touchscreen)
                {
                  if (priv->selection_bubble &&
                      gtk_widget_get_visible (priv->selection_bubble))
                    gtk_entry_selection_bubble_popup_unset (entry);
                  else
                    gtk_entry_selection_bubble_popup_set (entry);
                }
              else if (extend_selection)
                {
                  /* Truncate current selection, but keep it as big as possible */
                  if (tmp_pos - sel_start > sel_end - tmp_pos)
                    gtk_entry_set_positions (entry, sel_start, tmp_pos);
                  else
                    gtk_entry_set_positions (entry, tmp_pos, sel_end);

                  /* all done, so skip the extend_to_left stuff later */
                  extend_selection = FALSE;
                }
              else
                {
                  /* We'll either start a drag, or clear the selection */
                  priv->in_drag = TRUE;
                  priv->drag_start_x = x;
                  priv->drag_start_y = y;
                }
            }
          else
            {
              gtk_entry_selection_bubble_popup_unset (entry);

              if (!extend_selection)
                {
                  gtk_editable_set_position (editable, tmp_pos);
                  priv->handle_place_time = g_get_monotonic_time ();
                }
              else
                {
                  /* select from the current position to the clicked position */
                  if (!have_selection)
                    sel_start = sel_end = priv->current_pos;

                  gtk_entry_set_positions (entry, tmp_pos, tmp_pos);
                }
            }

          break;

        case 2:
          priv->select_words = TRUE;
          gtk_entry_select_word (entry);
          if (is_touchscreen)
            mode = GTK_TEXT_HANDLE_MODE_SELECTION;
          break;

        case 3:
          priv->select_lines = TRUE;
          gtk_entry_select_line (entry);
          if (is_touchscreen)
            mode = GTK_TEXT_HANDLE_MODE_SELECTION;
          break;

        default:
          break;
        }

      if (extend_selection)
        {
          gboolean extend_to_left;
          gint start, end;

          start = MIN (priv->current_pos, priv->selection_bound);
          start = MIN (sel_start, start);

          end = MAX (priv->current_pos, priv->selection_bound);
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

      gtk_gesture_set_state (priv->drag_gesture,
                             GTK_EVENT_SEQUENCE_CLAIMED);

      if (priv->text_handle)
        gtk_entry_update_handles (entry, mode);
    }

  if (n_press >= 3)
    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
}

static gchar *
_gtk_entry_get_selected_text (GtkEntry *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint         start_text, end_text;
  gchar       *text = NULL;

  if (gtk_editable_get_selection_bounds (editable, &start_text, &end_text))
    text = gtk_editable_get_chars (editable, start_text, end_text);

  return text;
}

static void
gtk_entry_show_magnifier (GtkEntry *entry,
                          gint      x,
                          gint      y)
{
  GtkAllocation allocation;
  cairo_rectangle_int_t rect;
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkAllocation text_allocation;

  gtk_entry_get_text_allocation (entry, &text_allocation);

  gtk_entry_ensure_magnifier (entry);

  gtk_widget_get_allocation (GTK_WIDGET (entry), &allocation);

  rect.x = x + text_allocation.x;
  rect.width = 1;
  rect.y = text_allocation.y;
  rect.height = text_allocation.height;

  _gtk_magnifier_set_coords (GTK_MAGNIFIER (priv->magnifier), rect.x,
                             rect.y + rect.height / 2);
  gtk_popover_set_pointing_to (GTK_POPOVER (priv->magnifier_popover),
                               &rect);
  gtk_popover_popup (GTK_POPOVER (priv->magnifier_popover));
}

static void
gtk_entry_drag_gesture_update (GtkGestureDrag *gesture,
                               gdouble         offset_x,
                               gdouble         offset_y,
                               GtkEntry       *entry)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GdkEventSequence *sequence;
  const GdkEvent *event;
  gint x, y;

  gtk_entry_selection_bubble_popup_unset (entry);

  gesture_get_current_point_in_layout (GTK_GESTURE_SINGLE (gesture), entry, &x, &y);
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  if (priv->mouse_cursor_obscured)
    {
      set_text_cursor (widget);
      priv->mouse_cursor_obscured = FALSE;
    }

  if (priv->select_lines)
    return;

  if (priv->in_drag)
    {
      if (gtk_entry_get_display_mode (entry) == DISPLAY_NORMAL &&
          gtk_drag_check_threshold (widget,
                                    priv->drag_start_x, priv->drag_start_y,
                                    x, y))
        {
          gint *ranges;
          gint n_ranges;
          GdkContentFormats  *target_list = gdk_content_formats_new (NULL, 0);
          guint actions = priv->editable ? GDK_ACTION_COPY | GDK_ACTION_MOVE : GDK_ACTION_COPY;

          target_list = gtk_content_formats_add_text_targets (target_list);

          gtk_entry_get_pixel_ranges (entry, &ranges, &n_ranges);

          gtk_drag_begin_with_coordinates (widget,
                                           gdk_event_get_device ((GdkEvent*) event),
                                           target_list, actions,
                                           priv->drag_start_x + ranges[0],
                                           priv->drag_start_y);
          g_free (ranges);

          priv->in_drag = FALSE;

          gdk_content_formats_unref (target_list);
        }
    }
  else
    {
      GtkAllocation text_allocation;
      GdkInputSource input_source;
      GdkDevice *source;
      guint length;
      gint tmp_pos;

      length = gtk_entry_buffer_get_length (get_buffer (entry));
      gtk_entry_get_text_allocation (entry, &text_allocation);

      if (y < 0)
	tmp_pos = 0;
      else if (y >= text_allocation.height)
	tmp_pos = length;
      else
	tmp_pos = gtk_entry_find_position (entry, x);

      source = gdk_event_get_source_device (event);
      input_source = gdk_device_get_source (source);

      if (priv->select_words)
	{
	  gint min, max;
	  gint old_min, old_max;
	  gint pos, bound;

	  min = gtk_entry_move_backward_word (entry, tmp_pos, TRUE);
	  max = gtk_entry_move_forward_word (entry, tmp_pos, TRUE);

	  pos = priv->current_pos;
	  bound = priv->selection_bound;

	  old_min = MIN (priv->current_pos, priv->selection_bound);
	  old_max = MAX (priv->current_pos, priv->selection_bound);

	  if (min < old_min)
	    {
	      pos = min;
	      bound = old_max;
	    }
	  else if (old_max < max)
	    {
	      pos = max;
	      bound = old_min;
	    }
	  else if (pos == old_min)
	    {
	      if (priv->current_pos != min)
		pos = max;
	    }
	  else
	    {
	      if (priv->current_pos != max)
		pos = min;
	    }

	  gtk_entry_set_positions (entry, pos, bound);
	}
      else
        gtk_entry_set_positions (entry, tmp_pos, -1);

      /* Update touch handles' position */
      if (gtk_simulate_touchscreen () ||
          input_source == GDK_SOURCE_TOUCHSCREEN)
        {
          gtk_entry_ensure_text_handles (entry);
          gtk_entry_update_handles (entry,
                                    (priv->current_pos == priv->selection_bound) ?
                                    GTK_TEXT_HANDLE_MODE_CURSOR :
                                    GTK_TEXT_HANDLE_MODE_SELECTION);
          gtk_entry_show_magnifier (entry, x - priv->scroll_offset, y);
        }
    }
}

static void
gtk_entry_drag_gesture_end (GtkGestureDrag *gesture,
                            gdouble         offset_x,
                            gdouble         offset_y,
                            GtkEntry       *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gboolean in_drag, is_touchscreen;
  GdkEventSequence *sequence;
  const GdkEvent *event;
  GdkDevice *source;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  in_drag = priv->in_drag;
  priv->in_drag = FALSE;

  if (priv->magnifier_popover)
    gtk_popover_popdown (GTK_POPOVER (priv->magnifier_popover));

  /* Check whether the drag was cancelled rather than finished */
  if (!gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
    return;

  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  source = gdk_event_get_source_device (event);
  is_touchscreen = gtk_simulate_touchscreen () ||
                   gdk_device_get_source (source) == GDK_SOURCE_TOUCHSCREEN;

  if (in_drag)
    {
      gint tmp_pos = gtk_entry_find_position (entry, priv->drag_start_x);

      gtk_editable_set_position (GTK_EDITABLE (entry), tmp_pos);
    }

  if (is_touchscreen &&
      !gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), NULL, NULL))
    gtk_entry_update_handles (entry, GTK_TEXT_HANDLE_MODE_CURSOR);

  gtk_entry_update_primary_selection (entry);
}

static void
gtk_entry_obscure_mouse_cursor (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GdkCursor *cursor;

  if (priv->mouse_cursor_obscured)
    return;

  cursor = gdk_cursor_new_from_name ("none", NULL);
  gtk_widget_set_cursor (GTK_WIDGET (entry), cursor);
  g_object_unref (cursor);

  priv->mouse_cursor_obscured = TRUE;
}

static gboolean
gtk_entry_key_controller_key_pressed (GtkEventControllerKey *controller,
                                      guint                  keyval,
                                      guint                  keycode,
                                      GdkModifierType        state,
                                      GtkWidget             *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gunichar unichar;

  gtk_entry_reset_blink_time (entry);
  gtk_entry_pend_cursor_blink (entry);

  gtk_entry_selection_bubble_popup_unset (entry);

  if (priv->text_handle)
    _gtk_text_handle_set_mode (priv->text_handle,
                               GTK_TEXT_HANDLE_MODE_NONE);

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter ||
      keyval == GDK_KEY_Escape)
    gtk_entry_reset_im_context (entry);

  unichar = gdk_keyval_to_unicode (keyval);

  if (!priv->editable && unichar != 0)
    gtk_widget_error_bell (widget);

  gtk_entry_obscure_mouse_cursor (entry);

  return FALSE;
}

static void
gtk_entry_focus_in (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GdkKeymap *keymap;

  gtk_widget_queue_draw (widget);

  keymap = gdk_display_get_keymap (gtk_widget_get_display (widget));

  if (priv->editable)
    {
      gtk_entry_schedule_im_reset (entry);
      gtk_im_context_focus_in (priv->im_context);
      keymap_state_changed (keymap, entry);
      g_signal_connect (keymap, "state-changed", 
                        G_CALLBACK (keymap_state_changed), entry);
    }

  g_signal_connect (keymap, "direction-changed",
		    G_CALLBACK (keymap_direction_changed), entry);

  if (gtk_entry_buffer_get_bytes (get_buffer (entry)) == 0 &&
      priv->placeholder_text != NULL)
    {
      gtk_entry_recompute (entry);
    }
  else
    {
      gtk_entry_reset_blink_time (entry);
      gtk_entry_check_cursor_blink (entry);
    }
}

static void
gtk_entry_focus_out (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEntryCompletion *completion;
  GdkKeymap *keymap;

  gtk_entry_selection_bubble_popup_unset (entry);

  if (priv->text_handle)
    _gtk_text_handle_set_mode (priv->text_handle,
                               GTK_TEXT_HANDLE_MODE_NONE);

  gtk_widget_queue_draw (widget);

  keymap = gdk_display_get_keymap (gtk_widget_get_display (widget));

  if (priv->editable)
    {
      gtk_entry_schedule_im_reset (entry);
      gtk_im_context_focus_out (priv->im_context);
      remove_capslock_feedback (entry);
    }

  if (gtk_entry_buffer_get_bytes (get_buffer (entry)) == 0 &&
      priv->placeholder_text != NULL)
    {
      gtk_entry_recompute (entry);
    }
  else
    {
      gtk_entry_check_cursor_blink (entry);
    }

  g_signal_handlers_disconnect_by_func (keymap, keymap_state_changed, entry);
  g_signal_handlers_disconnect_by_func (keymap, keymap_direction_changed, entry);

  completion = gtk_entry_get_completion (entry);
  if (completion)
    _gtk_entry_completion_popdown (completion);
}

void
_gtk_entry_grab_focus (GtkEntry  *entry,
                       gboolean   select_all)
{
  GTK_WIDGET_CLASS (gtk_entry_parent_class)->grab_focus (GTK_WIDGET (entry));
  if (select_all)
    gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}

static void
gtk_entry_grab_focus (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gboolean select_on_focus;

  if (priv->editable && !priv->in_click)
    {
      g_object_get (gtk_widget_get_settings (widget),
                    "gtk-entry-select-on-focus",
                    &select_on_focus,
                    NULL);

      _gtk_entry_grab_focus (entry, select_on_focus);
    }
  else
    {
      _gtk_entry_grab_focus (entry, FALSE);
    }
}

/**
 * gtk_entry_grab_focus_without_selecting:
 * @entry: a #GtkEntry
 *
 * Causes @entry to have keyboard focus.
 *
 * It behaves like gtk_widget_grab_focus(),
 * except that it doesn't select the contents of the entry.
 * You only want to call this on some special entries
 * which the user usually doesn't want to replace all text in,
 * such as search-as-you-type entries.
 */
void
gtk_entry_grab_focus_without_selecting (GtkEntry *entry)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  _gtk_entry_grab_focus (entry, FALSE);
}

static void
gtk_entry_direction_changed (GtkWidget        *widget,
                             GtkTextDirection  previous_dir)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  gtk_entry_recompute (entry);

  update_icon_style (widget, GTK_ENTRY_ICON_PRIMARY);
  update_icon_style (widget, GTK_ENTRY_ICON_SECONDARY);

  update_node_ordering (entry);

  GTK_WIDGET_CLASS (gtk_entry_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_entry_state_flags_changed (GtkWidget     *widget,
                               GtkStateFlags  previous_state)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (gtk_widget_get_realized (widget))
    {
      set_text_cursor (widget);
      priv->mouse_cursor_obscured = FALSE;
    }

  if (!gtk_widget_is_sensitive (widget))
    {
      /* Clear any selection */
      gtk_editable_select_region (GTK_EDITABLE (entry), priv->current_pos, priv->current_pos);
    }

  update_node_state (entry);

  gtk_entry_update_cached_style_values (entry);
}

static void
gtk_entry_display_changed (GtkWidget  *widget,
                           GdkDisplay *old_display)
{
  gtk_entry_recompute (GTK_ENTRY (widget));
}

/* GtkEditable method implementations
 */
static void
gtk_entry_insert_text (GtkEditable *editable,
		       const gchar *new_text,
		       gint         new_text_length,
		       gint        *position)
{
  g_object_ref (editable);

  /*
   * The incoming text may a password or other secret. We make sure
   * not to copy it into temporary buffers.
   */

  g_signal_emit_by_name (editable, "insert-text", new_text, new_text_length, position);

  g_object_unref (editable);
}

static void
gtk_entry_delete_text (GtkEditable *editable,
		       gint         start_pos,
		       gint         end_pos)
{
  g_object_ref (editable);

  g_signal_emit_by_name (editable, "delete-text", start_pos, end_pos);

  g_object_unref (editable);
}

static gchar *    
gtk_entry_get_chars      (GtkEditable   *editable,
			  gint           start_pos,
			  gint           end_pos)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  const gchar *text;
  gint text_length;
  gint start_index, end_index;

  text = gtk_entry_buffer_get_text (get_buffer (entry));
  text_length = gtk_entry_buffer_get_length (get_buffer (entry));

  if (end_pos < 0)
    end_pos = text_length;

  start_pos = MIN (text_length, start_pos);
  end_pos = MIN (text_length, end_pos);

  start_index = g_utf8_offset_to_pointer (text, start_pos) - text;
  end_index = g_utf8_offset_to_pointer (text, end_pos) - text;

  return g_strndup (text + start_index, end_index - start_index);
}

static void
gtk_entry_real_set_position (GtkEditable *editable,
			     gint         position)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (entry));
  if (position < 0 || position > length)
    position = length;

  if (position != priv->current_pos ||
      position != priv->selection_bound)
    {
      gtk_entry_reset_im_context (entry);
      gtk_entry_set_positions (entry, position, position);
    }
}

static gint
gtk_entry_get_position (GtkEditable *editable)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  return priv->current_pos;
}

static void
gtk_entry_set_selection_bounds (GtkEditable *editable,
				gint         start,
				gint         end)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (entry));
  if (start < 0)
    start = length;
  if (end < 0)
    end = length;

  gtk_entry_reset_im_context (entry);

  gtk_entry_set_positions (entry,
			   MIN (end, length),
			   MIN (start, length));

  gtk_entry_update_primary_selection (entry);
}

static gboolean
gtk_entry_get_selection_bounds (GtkEditable *editable,
				gint        *start,
				gint        *end)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  *start = priv->selection_bound;
  *end = priv->current_pos;

  return (priv->selection_bound != priv->current_pos);
}

static void
gtk_entry_update_cached_style_values (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (!priv->invisible_char_set)
    {
      gunichar ch = find_invisible_char (GTK_WIDGET (entry));

      if (priv->invisible_char != ch)
        {
          priv->invisible_char = ch;
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_INVISIBLE_CHAR]);
        }
    }
}

static void 
gtk_entry_style_updated (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  GTK_WIDGET_CLASS (gtk_entry_parent_class)->style_updated (widget);

  gtk_entry_update_cached_style_values (entry);
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
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  guint keyval;

  if (!gdk_event_get_keyval ((GdkEvent *) key_event, &keyval))
    return GDK_EVENT_PROPAGATE;

  if (keyval == GDK_KEY_Escape)
    {
      priv->editing_canceled = TRUE;
      gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
      gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));

      return GDK_EVENT_STOP;
    }

  /* override focus */
  if (keyval == GDK_KEY_Up || keyval == GDK_KEY_Down)
    {
      gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
      gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
gtk_entry_start_editing (GtkCellEditable *cell_editable,
			 GdkEvent        *event)
{
  g_signal_connect (cell_editable, "activate",
		    G_CALLBACK (gtk_cell_editable_entry_activated), NULL);
  g_signal_connect (cell_editable, "key-press-event",
		    G_CALLBACK (gtk_cell_editable_key_press_event), NULL);
}

static void
gtk_entry_password_hint_free (GtkEntryPasswordHint *password_hint)
{
  if (password_hint->source_id)
    g_source_remove (password_hint->source_id);

  g_slice_free (GtkEntryPasswordHint, password_hint);
}


static gboolean
gtk_entry_remove_password_hint (gpointer data)
{
  GtkEntryPasswordHint *password_hint = g_object_get_qdata (data, quark_password_hint);
  password_hint->position = -1;

  /* Force the string to be redrawn, but now without a visible character */
  gtk_entry_recompute (GTK_ENTRY (data));
  return G_SOURCE_REMOVE;
}

/* Default signal handlers
 */
static void
gtk_entry_real_insert_text (GtkEditable *editable,
			    const gchar *new_text,
			    gint         new_text_length,
			    gint        *position)
{
  guint n_inserted;
  gint n_chars;

  n_chars = g_utf8_strlen (new_text, new_text_length);

  /*
   * The actual insertion into the buffer. This will end up firing the
   * following signal handlers: buffer_inserted_text(), buffer_notify_display_text(),
   * buffer_notify_text(), buffer_notify_length()
   */
  begin_change (GTK_ENTRY (editable));

  n_inserted = gtk_entry_buffer_insert_text (get_buffer (GTK_ENTRY (editable)), *position, new_text, n_chars);

  end_change (GTK_ENTRY (editable));

  if (n_inserted != n_chars)
      gtk_widget_error_bell (GTK_WIDGET (editable));

  *position += n_inserted;
}

static void
gtk_entry_real_delete_text (GtkEditable *editable,
                            gint         start_pos,
                            gint         end_pos)
{
  /*
   * The actual deletion from the buffer. This will end up firing the
   * following signal handlers: buffer_deleted_text(), buffer_notify_display_text(),
   * buffer_notify_text(), buffer_notify_length()
   */

  begin_change (GTK_ENTRY (editable));

  gtk_entry_buffer_delete_text (get_buffer (GTK_ENTRY (editable)), start_pos, end_pos - start_pos);

  end_change (GTK_ENTRY (editable));
}

/* GtkEntryBuffer signal handlers
 */
static void
buffer_inserted_text (GtkEntryBuffer *buffer,
                      guint           position,
                      const gchar    *chars,
                      guint           n_chars,
                      GtkEntry       *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  guint password_hint_timeout;
  guint current_pos;
  gint selection_bound;

  current_pos = priv->current_pos;
  if (current_pos > position)
    current_pos += n_chars;

  selection_bound = priv->selection_bound;
  if (selection_bound > position)
    selection_bound += n_chars;

  gtk_entry_set_positions (entry, current_pos, selection_bound);
  gtk_entry_recompute (entry);

  /* Calculate the password hint if it needs to be displayed. */
  if (n_chars == 1 && !priv->visible)
    {
      g_object_get (gtk_widget_get_settings (GTK_WIDGET (entry)),
                    "gtk-entry-password-hint-timeout", &password_hint_timeout,
                    NULL);

      if (password_hint_timeout > 0)
        {
          GtkEntryPasswordHint *password_hint = g_object_get_qdata (G_OBJECT (entry),
                                                                    quark_password_hint);
          if (!password_hint)
            {
              password_hint = g_slice_new0 (GtkEntryPasswordHint);
              g_object_set_qdata_full (G_OBJECT (entry), quark_password_hint, password_hint,
                                       (GDestroyNotify)gtk_entry_password_hint_free);
            }

          password_hint->position = position;
          if (password_hint->source_id)
            g_source_remove (password_hint->source_id);
          password_hint->source_id = g_timeout_add (password_hint_timeout,
                                                    (GSourceFunc)gtk_entry_remove_password_hint,
                                                    entry);
          g_source_set_name_by_id (password_hint->source_id, "[gtk+] gtk_entry_remove_password_hint");
        }
    }
}

static void
buffer_deleted_text (GtkEntryBuffer *buffer,
                     guint           position,
                     guint           n_chars,
                     GtkEntry       *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  guint end_pos = position + n_chars;
  gint selection_bound;
  guint current_pos;

  current_pos = priv->current_pos;
  if (current_pos > position)
    current_pos -= MIN (current_pos, end_pos) - position;

  selection_bound = priv->selection_bound;
  if (selection_bound > position)
    selection_bound -= MIN (selection_bound, end_pos) - position;

  gtk_entry_set_positions (entry, current_pos, selection_bound);
  gtk_entry_recompute (entry);

  /* We might have deleted the selection */
  gtk_entry_update_primary_selection (entry);

  /* Disable the password hint if one exists. */
  if (!priv->visible)
    {
      GtkEntryPasswordHint *password_hint = g_object_get_qdata (G_OBJECT (entry),
                                                                quark_password_hint);
      if (password_hint)
        {
          if (password_hint->source_id)
            g_source_remove (password_hint->source_id);
          password_hint->source_id = 0;
          password_hint->position = -1;
        }
    }
}

static void
buffer_notify_text (GtkEntryBuffer *buffer,
                    GParamSpec     *spec,
                    GtkEntry       *entry)
{
  emit_changed (entry);
  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_TEXT]);
}

static void
buffer_notify_length (GtkEntryBuffer *buffer,
                      GParamSpec     *spec,
                      GtkEntry       *entry)
{
  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_TEXT_LENGTH]);
}

static void
buffer_notify_max_length (GtkEntryBuffer *buffer,
                          GParamSpec     *spec,
                          GtkEntry       *entry)
{
  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_MAX_LENGTH]);
}

static void
buffer_connect_signals (GtkEntry *entry)
{
  g_signal_connect (get_buffer (entry), "inserted-text", G_CALLBACK (buffer_inserted_text), entry);
  g_signal_connect (get_buffer (entry), "deleted-text", G_CALLBACK (buffer_deleted_text), entry);
  g_signal_connect (get_buffer (entry), "notify::text", G_CALLBACK (buffer_notify_text), entry);
  g_signal_connect (get_buffer (entry), "notify::length", G_CALLBACK (buffer_notify_length), entry);
  g_signal_connect (get_buffer (entry), "notify::max-length", G_CALLBACK (buffer_notify_max_length), entry);
}

static void
buffer_disconnect_signals (GtkEntry *entry)
{
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_inserted_text, entry);
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_deleted_text, entry);
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_notify_text, entry);
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_notify_length, entry);
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_notify_max_length, entry);
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
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GdkKeymap *keymap = gdk_display_get_keymap (gtk_widget_get_display (GTK_WIDGET (entry)));
  PangoDirection keymap_direction = gdk_keymap_get_direction (keymap);
  gboolean split_cursor;
  
  PangoLayout *layout = gtk_entry_ensure_layout (entry, TRUE);
  const gchar *text = pango_layout_get_text (layout);
  gint index = g_utf8_offset_to_pointer (text, offset) - text;
  
  PangoRectangle strong_pos, weak_pos;
  
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (entry)),
		"gtk-split-cursor", &split_cursor,
		NULL);

  pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);

  if (split_cursor)
    return strong_pos.x / PANGO_SCALE;
  else
    return (keymap_direction == priv->resolved_dir) ? strong_pos.x / PANGO_SCALE : weak_pos.x / PANGO_SCALE;
}

static void
gtk_entry_move_cursor (GtkEntry       *entry,
		       GtkMovementStep step,
		       gint            count,
		       gboolean        extend_selection)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gint new_pos = priv->current_pos;

  gtk_entry_reset_im_context (entry);

  if (priv->current_pos != priv->selection_bound && !extend_selection)
    {
      /* If we have a current selection and aren't extending it, move to the
       * start/or end of the selection as appropriate
       */
      switch (step)
	{
	case GTK_MOVEMENT_VISUAL_POSITIONS:
	  {
	    gint current_x = get_better_cursor_x (entry, priv->current_pos);
	    gint bound_x = get_better_cursor_x (entry, priv->selection_bound);

	    if (count <= 0)
	      new_pos = current_x < bound_x ? priv->current_pos : priv->selection_bound;
	    else 
	      new_pos = current_x > bound_x ? priv->current_pos : priv->selection_bound;
	  }
	  break;

	case GTK_MOVEMENT_WORDS:
          if (priv->resolved_dir == PANGO_DIRECTION_RTL)
            count *= -1;
          /* Fall through */

	case GTK_MOVEMENT_LOGICAL_POSITIONS:
	  if (count < 0)
	    new_pos = MIN (priv->current_pos, priv->selection_bound);
	  else
	    new_pos = MAX (priv->current_pos, priv->selection_bound);

	  break;

	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
	case GTK_MOVEMENT_BUFFER_ENDS:
	  new_pos = count < 0 ? 0 : gtk_entry_buffer_get_length (get_buffer (entry));
	  break;

	case GTK_MOVEMENT_DISPLAY_LINES:
	case GTK_MOVEMENT_PARAGRAPHS:
	case GTK_MOVEMENT_PAGES:
	case GTK_MOVEMENT_HORIZONTAL_PAGES:
        default:
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

          if (priv->current_pos == new_pos)
            {
              if (!extend_selection)
                {
                  if (!gtk_widget_keynav_failed (GTK_WIDGET (entry),
                                                 count > 0 ?
                                                 GTK_DIR_RIGHT : GTK_DIR_LEFT))
                    {
                      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (entry));

                      if (toplevel)
                        gtk_widget_child_focus (toplevel,
                                                count > 0 ?
                                                GTK_DIR_RIGHT : GTK_DIR_LEFT);
                    }
                }
              else
                {
                  gtk_widget_error_bell (GTK_WIDGET (entry));
                }
            }
	  break;

	case GTK_MOVEMENT_WORDS:
          if (priv->resolved_dir == PANGO_DIRECTION_RTL)
            count *= -1;

	  while (count > 0)
	    {
	      new_pos = gtk_entry_move_forward_word (entry, new_pos, FALSE);
	      count--;
	    }

	  while (count < 0)
	    {
	      new_pos = gtk_entry_move_backward_word (entry, new_pos, FALSE);
	      count++;
	    }

          if (priv->current_pos == new_pos)
            gtk_widget_error_bell (GTK_WIDGET (entry));

	  break;

	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
	case GTK_MOVEMENT_BUFFER_ENDS:
	  new_pos = count < 0 ? 0 : gtk_entry_buffer_get_length (get_buffer (entry));

          if (priv->current_pos == new_pos)
            gtk_widget_error_bell (GTK_WIDGET (entry));

	  break;

	case GTK_MOVEMENT_DISPLAY_LINES:
	case GTK_MOVEMENT_PARAGRAPHS:
	case GTK_MOVEMENT_PAGES:
	case GTK_MOVEMENT_HORIZONTAL_PAGES:
        default:
	  break;
	}
    }

  if (extend_selection)
    gtk_editable_select_region (GTK_EDITABLE (entry), priv->selection_bound, new_pos);
  else
    gtk_editable_set_position (GTK_EDITABLE (entry), new_pos);
  
  gtk_entry_pend_cursor_blink (entry);
}

static void
gtk_entry_insert_at_cursor (GtkEntry    *entry,
			    const gchar *str)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint pos = priv->current_pos;

  if (priv->editable)
    {
      gtk_entry_reset_im_context (entry);
      gtk_editable_insert_text (editable, str, -1, &pos);
      gtk_editable_set_position (editable, pos);
    }
}

static void
gtk_entry_delete_from_cursor (GtkEntry       *entry,
			      GtkDeleteType   type,
			      gint            count)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint start_pos = priv->current_pos;
  gint end_pos = priv->current_pos;
  gint old_n_bytes = gtk_entry_buffer_get_bytes (get_buffer (entry));

  gtk_entry_reset_im_context (entry);

  if (!priv->editable)
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
      return;
    }

  if (priv->selection_bound != priv->current_pos)
    {
      gtk_editable_delete_selection (editable);
      return;
    }
  
  switch (type)
    {
    case GTK_DELETE_CHARS:
      end_pos = gtk_entry_move_logically (entry, priv->current_pos, count);
      gtk_editable_delete_text (editable, MIN (start_pos, end_pos), MAX (start_pos, end_pos));
      break;

    case GTK_DELETE_WORDS:
      if (count < 0)
	{
	  /* Move to end of current word, or if not on a word, end of previous word */
	  end_pos = gtk_entry_move_backward_word (entry, end_pos, FALSE);
	  end_pos = gtk_entry_move_forward_word (entry, end_pos, FALSE);
	}
      else if (count > 0)
	{
	  /* Move to beginning of current word, or if not on a word, begining of next word */
	  start_pos = gtk_entry_move_forward_word (entry, start_pos, FALSE);
	  start_pos = gtk_entry_move_backward_word (entry, start_pos, FALSE);
	}
	
      /* Fall through */
    case GTK_DELETE_WORD_ENDS:
      while (count < 0)
	{
	  start_pos = gtk_entry_move_backward_word (entry, start_pos, FALSE);
	  count++;
	}

      while (count > 0)
	{
	  end_pos = gtk_entry_move_forward_word (entry, end_pos, FALSE);
	  count--;
	}

      gtk_editable_delete_text (editable, start_pos, end_pos);
      break;

    case GTK_DELETE_DISPLAY_LINE_ENDS:
    case GTK_DELETE_PARAGRAPH_ENDS:
      if (count < 0)
	gtk_editable_delete_text (editable, 0, priv->current_pos);
      else
	gtk_editable_delete_text (editable, priv->current_pos, -1);

      break;

    case GTK_DELETE_DISPLAY_LINES:
    case GTK_DELETE_PARAGRAPHS:
      gtk_editable_delete_text (editable, 0, -1);  
      break;

    case GTK_DELETE_WHITESPACE:
      gtk_entry_delete_whitespace (entry);
      break;

    default:
      break;
    }

  if (gtk_entry_buffer_get_bytes (get_buffer (entry)) == old_n_bytes)
    gtk_widget_error_bell (GTK_WIDGET (entry));

  gtk_entry_pend_cursor_blink (entry);
}

static void
gtk_entry_backspace (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint prev_pos;

  gtk_entry_reset_im_context (entry);

  if (!priv->editable)
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
      return;
    }

  if (priv->selection_bound != priv->current_pos)
    {
      gtk_editable_delete_selection (editable);
      return;
    }

  prev_pos = gtk_entry_move_logically (entry, priv->current_pos, -1);

  if (prev_pos < priv->current_pos)
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
      PangoLogAttr *log_attrs;
      gint n_attrs;

      pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);

      /* Deleting parts of characters */
      if (log_attrs[priv->current_pos].backspace_deletes_character)
	{
	  gchar *cluster_text;
	  gchar *normalized_text;
          glong  len;

	  cluster_text = _gtk_entry_get_display_text (entry, prev_pos,
                                                      priv->current_pos);
	  normalized_text = g_utf8_normalize (cluster_text,
			  		      strlen (cluster_text),
					      G_NORMALIZE_NFD);
          len = g_utf8_strlen (normalized_text, -1);

          gtk_editable_delete_text (editable, prev_pos, priv->current_pos);
	  if (len > 1)
	    {
	      gint pos = priv->current_pos;

	      gtk_editable_insert_text (editable, normalized_text,
					g_utf8_offset_to_pointer (normalized_text, len - 1) - normalized_text,
					&pos);
	      gtk_editable_set_position (editable, pos);
	    }

	  g_free (normalized_text);
	  g_free (cluster_text);
	}
      else
	{
          gtk_editable_delete_text (editable, prev_pos, priv->current_pos);
	}
      
      g_free (log_attrs);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
    }

  gtk_entry_pend_cursor_blink (entry);
}

static void
gtk_entry_copy_clipboard (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint start, end;
  gchar *str;

  if (gtk_editable_get_selection_bounds (editable, &start, &end))
    {
      if (!priv->visible)
        {
          gtk_widget_error_bell (GTK_WIDGET (entry));
          return;
        }

      str = _gtk_entry_get_display_text (entry, start, end);
      gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (entry)), str);
      g_free (str);
    }
}

static void
gtk_entry_cut_clipboard (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint start, end;

  if (!priv->visible)
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
      return;
    }

  gtk_entry_copy_clipboard (entry);

  if (priv->editable)
    {
      if (gtk_editable_get_selection_bounds (editable, &start, &end))
	gtk_editable_delete_text (editable, start, end);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
    }

  gtk_entry_selection_bubble_popup_unset (entry);

  if (priv->text_handle)
    {
      GtkTextHandleMode handle_mode;

      handle_mode = _gtk_text_handle_get_mode (priv->text_handle);

      if (handle_mode != GTK_TEXT_HANDLE_MODE_NONE)
        gtk_entry_update_handles (entry, GTK_TEXT_HANDLE_MODE_CURSOR);
    }
}

static void
gtk_entry_paste_clipboard (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->editable)
    gtk_entry_paste (entry, gtk_widget_get_clipboard (GTK_WIDGET (entry)));
  else
    gtk_widget_error_bell (GTK_WIDGET (entry));

  if (priv->text_handle)
    {
      GtkTextHandleMode handle_mode;

      handle_mode = _gtk_text_handle_get_mode (priv->text_handle);

      if (handle_mode != GTK_TEXT_HANDLE_MODE_NONE)
        gtk_entry_update_handles (entry, GTK_TEXT_HANDLE_MODE_CURSOR);
    }
}

static void
gtk_entry_delete_cb (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint start, end;

  if (priv->editable)
    {
      if (gtk_editable_get_selection_bounds (editable, &start, &end))
	gtk_editable_delete_text (editable, start, end);
    }
}

static void
gtk_entry_toggle_overwrite (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  priv->overwrite_mode = !priv->overwrite_mode;

  if (priv->overwrite_mode)
    {
      if (!priv->block_cursor_node)
        {
          GtkCssNode *widget_node = gtk_widget_get_css_node (GTK_WIDGET (entry));

          priv->block_cursor_node = gtk_css_node_new ();
          gtk_css_node_set_name (priv->block_cursor_node, I_("block-cursor"));
          gtk_css_node_set_parent (priv->block_cursor_node, widget_node);
          gtk_css_node_set_state (priv->block_cursor_node, gtk_css_node_get_state (widget_node));
          g_object_unref (priv->block_cursor_node);
        }
    }
  else
    {
      if (priv->block_cursor_node)
        {
          gtk_css_node_set_parent (priv->block_cursor_node, NULL);
          priv->block_cursor_node = NULL;
        }
    }

  gtk_entry_pend_cursor_blink (entry);
  gtk_widget_queue_draw (GTK_WIDGET (entry));
}

static void
gtk_entry_select_all (GtkEntry *entry)
{
  gtk_entry_select_line (entry);
}

static void
gtk_entry_real_activate (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkWindow *window;
  GtkWidget *default_widget, *focus_widget;
  GtkWidget *toplevel;
  GtkWidget *widget;

  widget = GTK_WIDGET (entry);

  if (priv->activates_default)
    {
      toplevel = gtk_widget_get_toplevel (widget);
      if (GTK_IS_WINDOW (toplevel))
	{
	  window = GTK_WINDOW (toplevel);

	  if (window)
            {
              default_widget = gtk_window_get_default_widget (window);
              focus_widget = gtk_window_get_focus (window);
              if (widget != default_widget &&
                  !(widget == focus_widget && (!default_widget || !gtk_widget_get_sensitive (default_widget))))
	        gtk_window_activate_default (window);
            }
	}
    }
}

static void
keymap_direction_changed (GdkKeymap *keymap,
                          GtkEntry  *entry)
{
  gtk_entry_recompute (entry);
}

/* IM Context Callbacks
 */

static void
gtk_entry_commit_cb (GtkIMContext *context,
		     const gchar  *str,
		     GtkEntry     *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->editable)
    {
      gtk_entry_enter_text (entry, str);
      gtk_entry_obscure_mouse_cursor (entry);
    }
}

static void 
gtk_entry_preedit_changed_cb (GtkIMContext *context,
			      GtkEntry     *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->editable)
    {
      gchar *preedit_string;
      gint cursor_pos;

      gtk_entry_obscure_mouse_cursor (entry);

      gtk_im_context_get_preedit_string (priv->im_context,
                                         &preedit_string, NULL,
                                         &cursor_pos);
      g_signal_emit (entry, signals[PREEDIT_CHANGED], 0, preedit_string);
      priv->preedit_length = strlen (preedit_string);
      cursor_pos = CLAMP (cursor_pos, 0, g_utf8_strlen (preedit_string, -1));
      priv->preedit_cursor = cursor_pos;
      g_free (preedit_string);
    
      gtk_entry_recompute (entry);
    }
}

static gboolean
gtk_entry_retrieve_surrounding_cb (GtkIMContext *context,
                                   GtkEntry     *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gchar *text;

  /* XXXX ??? does this even make sense when text is not visible? Should we return FALSE? */
  text = _gtk_entry_get_display_text (entry, 0, -1);
  gtk_im_context_set_surrounding (context, text, strlen (text), /* Length in bytes */
				  g_utf8_offset_to_pointer (text, priv->current_pos) - text);
  g_free (text);

  return TRUE;
}

static gboolean
gtk_entry_delete_surrounding_cb (GtkIMContext *slave,
				 gint          offset,
				 gint          n_chars,
				 GtkEntry     *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->editable)
    gtk_editable_delete_text (GTK_EDITABLE (entry),
                              priv->current_pos + offset,
                              priv->current_pos + offset + n_chars);

  return TRUE;
}

/* Internal functions
 */

/* Used for im_commit_cb and inserting Unicode chars */
void
gtk_entry_enter_text (GtkEntry       *entry,
                      const gchar    *str)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint tmp_pos;
  gboolean old_need_im_reset;
  guint text_length;

  old_need_im_reset = priv->need_im_reset;
  priv->need_im_reset = FALSE;

  if (gtk_editable_get_selection_bounds (editable, NULL, NULL))
    gtk_editable_delete_selection (editable);
  else
    {
      if (priv->overwrite_mode)
        {
          text_length = gtk_entry_buffer_get_length (get_buffer (entry));
          if (priv->current_pos < text_length)
            gtk_entry_delete_from_cursor (entry, GTK_DELETE_CHARS, 1);
        }
    }

  tmp_pos = priv->current_pos;
  gtk_editable_insert_text (editable, str, strlen (str), &tmp_pos);
  gtk_editable_set_position (editable, tmp_pos);

  priv->need_im_reset = old_need_im_reset;
}

/* All changes to priv->current_pos and priv->selection_bound
 * should go through this function.
 */
void
gtk_entry_set_positions (GtkEntry *entry,
			 gint      current_pos,
			 gint      selection_bound)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gboolean changed = FALSE;

  g_object_freeze_notify (G_OBJECT (entry));

  if (current_pos != -1 &&
      priv->current_pos != current_pos)
    {
      priv->current_pos = current_pos;
      changed = TRUE;

      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_CURSOR_POSITION]);
    }

  if (selection_bound != -1 &&
      priv->selection_bound != selection_bound)
    {
      priv->selection_bound = selection_bound;
      changed = TRUE;

      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_SELECTION_BOUND]);
    }

  g_object_thaw_notify (G_OBJECT (entry));

  if (priv->current_pos != priv->selection_bound)
    {
      if (!priv->selection_node)
        {
          GtkCssNode *widget_node = gtk_widget_get_css_node (GTK_WIDGET (entry));

          priv->selection_node = gtk_css_node_new ();
          gtk_css_node_set_name (priv->selection_node, I_("selection"));
          gtk_css_node_set_parent (priv->selection_node, widget_node);
          gtk_css_node_set_state (priv->selection_node, gtk_css_node_get_state (widget_node));
          g_object_unref (priv->selection_node);
        }
    }
  else
    {
      if (priv->selection_node)
        {
          gtk_css_node_set_parent (priv->selection_node, NULL);
          priv->selection_node = NULL;
        }
    }

  if (changed)
    {
      gtk_entry_recompute (entry);
    }
}

static void
gtk_entry_reset_layout (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->cached_layout)
    {
      g_object_unref (priv->cached_layout);
      priv->cached_layout = NULL;
    }
}

static void
update_im_cursor_location (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GdkRectangle area;
  GtkAllocation text_area;
  gint strong_x;
  gint strong_xoffset;

  gtk_entry_get_cursor_locations (entry, &strong_x, NULL);
  gtk_entry_get_text_allocation (entry, &text_area);

  strong_xoffset = strong_x - priv->scroll_offset;
  if (strong_xoffset < 0)
    {
      strong_xoffset = 0;
    }
  else if (strong_xoffset > text_area.width)
    {
      strong_xoffset = text_area.width;
    }
  area.x = strong_xoffset;
  area.y = 0;
  area.width = 0;
  area.height = text_area.height;

  gtk_im_context_set_cursor_location (priv->im_context, &area);
}

static void
gtk_entry_recompute (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkTextHandleMode handle_mode;

  gtk_entry_reset_layout (entry);
  gtk_entry_check_cursor_blink (entry);

  gtk_entry_adjust_scroll (entry);

  update_im_cursor_location (entry);

  if (priv->text_handle)
    {
      handle_mode = _gtk_text_handle_get_mode (priv->text_handle);

      if (handle_mode != GTK_TEXT_HANDLE_MODE_NONE)
        gtk_entry_update_handles (entry, handle_mode);
    }

  gtk_widget_queue_draw (GTK_WIDGET (entry));
}

static void
gtk_entry_get_placeholder_text_color (GtkEntry   *entry,
                                      PangoColor *color)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkStyleContext *context;
  GdkRGBA fg = { 0.5, 0.5, 0.5 };

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_lookup_color (context, "placeholder_text_color", &fg);

  color->red = CLAMP (fg.red * 65535. + 0.5, 0, 65535);
  color->green = CLAMP (fg.green * 65535. + 0.5, 0, 65535);
  color->blue = CLAMP (fg.blue * 65535. + 0.5, 0, 65535);
}

static inline gboolean
show_placeholder_text (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (!gtk_widget_has_focus (GTK_WIDGET (entry)) &&
      gtk_entry_buffer_get_bytes (get_buffer (entry)) == 0 &&
      priv->placeholder_text != NULL)
    return TRUE;

  return FALSE;
}

static PangoLayout *
gtk_entry_create_layout (GtkEntry *entry,
			 gboolean  include_preedit)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkStyleContext *context;
  PangoLayout *layout;
  PangoAttrList *tmp_attrs;
  gboolean placeholder_layout;

  gchar *preedit_string = NULL;
  gint preedit_length = 0;
  PangoAttrList *preedit_attrs = NULL;

  gchar *display_text;
  guint n_bytes;

  context = gtk_widget_get_style_context (widget);

  layout = gtk_widget_create_pango_layout (widget, NULL);
  pango_layout_set_single_paragraph_mode (layout, TRUE);

  tmp_attrs = _gtk_style_context_get_pango_attributes (context);
  tmp_attrs = _gtk_pango_attr_list_merge (tmp_attrs, priv->attrs);
  if (!tmp_attrs)
    tmp_attrs = pango_attr_list_new ();

  placeholder_layout = show_placeholder_text (entry);
  if (placeholder_layout)
    display_text = g_strdup (priv->placeholder_text);
  else
    display_text = _gtk_entry_get_display_text (entry, 0, -1);

  n_bytes = strlen (display_text);

  if (!placeholder_layout && include_preedit)
    {
      gtk_im_context_get_preedit_string (priv->im_context,
					 &preedit_string, &preedit_attrs, NULL);
      preedit_length = priv->preedit_length;
    }
  else if (placeholder_layout)
    {
      PangoColor color;
      PangoAttribute *attr;

      gtk_entry_get_placeholder_text_color (entry, &color);
      attr = pango_attr_foreground_new (color.red, color.green, color.blue);
      attr->start_index = 0;
      attr->end_index = G_MAXINT;
      pango_attr_list_insert (tmp_attrs, attr);
      pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
    }

  if (preedit_length)
    {
      GString *tmp_string = g_string_new (display_text);
      gint pos;

      pos = g_utf8_offset_to_pointer (display_text, priv->current_pos) - display_text;
      g_string_insert (tmp_string, pos, preedit_string);
      pango_layout_set_text (layout, tmp_string->str, tmp_string->len);
      pango_attr_list_splice (tmp_attrs, preedit_attrs, pos, preedit_length);
      g_string_free (tmp_string, TRUE);
    }
  else
    {
      PangoDirection pango_dir;

      if (gtk_entry_get_display_mode (entry) == DISPLAY_NORMAL)
	pango_dir = pango_find_base_dir (display_text, n_bytes);
      else
	pango_dir = PANGO_DIRECTION_NEUTRAL;

      if (pango_dir == PANGO_DIRECTION_NEUTRAL)
        {
          if (gtk_widget_has_focus (widget))
	    {
	      GdkDisplay *display = gtk_widget_get_display (widget);
	      GdkKeymap *keymap = gdk_display_get_keymap (display);
	      if (gdk_keymap_get_direction (keymap) == PANGO_DIRECTION_RTL)
		pango_dir = PANGO_DIRECTION_RTL;
	      else
		pango_dir = PANGO_DIRECTION_LTR;
	    }
          else
	    {
	      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
		pango_dir = PANGO_DIRECTION_RTL;
	      else
		pango_dir = PANGO_DIRECTION_LTR;
	    }
        }

      pango_context_set_base_dir (gtk_widget_get_pango_context (widget), pango_dir);

      priv->resolved_dir = pango_dir;

      pango_layout_set_text (layout, display_text, n_bytes);
    }

  pango_layout_set_attributes (layout, tmp_attrs);

  if (priv->tabs)
    pango_layout_set_tabs (layout, priv->tabs);

  g_free (preedit_string);
  g_free (display_text);

  if (preedit_attrs)
    pango_attr_list_unref (preedit_attrs);

  pango_attr_list_unref (tmp_attrs);

  return layout;
}

static PangoLayout *
gtk_entry_ensure_layout (GtkEntry *entry,
                         gboolean  include_preedit)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->preedit_length > 0 &&
      !include_preedit != !priv->cache_includes_preedit)
    gtk_entry_reset_layout (entry);

  if (!priv->cached_layout)
    {
      priv->cached_layout = gtk_entry_create_layout (entry, include_preedit);
      priv->cache_includes_preedit = include_preedit;
    }

  return priv->cached_layout;
}

static void
get_layout_position (GtkEntry *entry,
                     gint     *x,
                     gint     *y)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gint y_pos, area_height;
  PangoLayoutLine *line;
  GtkAllocation text_allocation;

  gtk_entry_get_text_allocation (entry, &text_allocation);

  layout = gtk_entry_ensure_layout (entry, TRUE);

  area_height = PANGO_SCALE * text_allocation.height;

  line = pango_layout_get_lines_readonly (layout)->data;
  pango_layout_line_get_extents (line, NULL, &logical_rect);

  /* Align primarily for locale's ascent/descent */
  if (priv->text_baseline < 0)
    y_pos = ((area_height - priv->ascent - priv->descent) / 2 +
             priv->ascent + logical_rect.y);
  else
    y_pos = PANGO_SCALE * priv->text_baseline - pango_layout_get_baseline (layout);

  /* Now see if we need to adjust to fit in actual drawn string */
  if (logical_rect.height > area_height)
    y_pos = (area_height - logical_rect.height) / 2;
  else if (y_pos < 0)
    y_pos = 0;
  else if (y_pos + logical_rect.height > area_height)
    y_pos = area_height - logical_rect.height;
  
  y_pos = y_pos / PANGO_SCALE;

  if (x)
    *x = priv->text_x - priv->scroll_offset;

  if (y)
    *y = y_pos;
}

#define GRAPHENE_RECT_FROM_RECT(_r) (GRAPHENE_RECT_INIT ((_r)->x, (_r)->y, (_r)->width, (_r)->height))

static void
gtk_entry_draw_text (GtkEntry    *entry,
                     GtkSnapshot *snapshot)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkStyleContext *context;
  PangoLayout *layout;
  gint x, y;
  gint start_pos, end_pos;
  int width, height;

  /* Nothing to display at all */
  if (gtk_entry_get_display_mode (entry) == DISPLAY_BLANK)
    return;

  context = gtk_widget_get_style_context (widget);
  layout = gtk_entry_ensure_layout (entry, TRUE);
  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  gtk_entry_get_layout_offsets (entry, &x, &y);

  if (show_placeholder_text (entry))
    pango_layout_set_width (layout, PANGO_SCALE * priv->text_width);

  gtk_snapshot_render_layout (snapshot, context, x, y, layout);

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start_pos, &end_pos))
    {
      const char *text = pango_layout_get_text (layout);
      gint start_index = g_utf8_offset_to_pointer (text, start_pos) - text;
      gint end_index = g_utf8_offset_to_pointer (text, end_pos) - text;
      cairo_region_t *clip;
      cairo_rectangle_int_t clip_extents;
      gint range[2];

      range[0] = MIN (start_index, end_index);
      range[1] = MAX (start_index, end_index);

      gtk_style_context_save_to_node (context, priv->selection_node);

      clip = gdk_pango_layout_get_clip_region (layout, x, y, range, 1);
      cairo_region_get_extents (clip, &clip_extents);

      gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_FROM_RECT (&clip_extents), "Selected Text");
      gtk_snapshot_render_background (snapshot, context, 0, 0, width, height);
      gtk_snapshot_render_layout (snapshot, context, x, y, layout);
      gtk_snapshot_pop (snapshot);

      cairo_region_destroy (clip);

      gtk_style_context_restore (context);
    }
}

static void
gtk_entry_draw_cursor (GtkEntry    *entry,
                       GtkSnapshot *snapshot,
		       CursorType   type)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkStyleContext *context;
  PangoRectangle cursor_rect;
  gint cursor_index;
  gboolean block;
  gboolean block_at_line_end;
  PangoLayout *layout;
  const char *text;
  gint x, y;
  int width, height;

  context = gtk_widget_get_style_context (widget);

  layout = gtk_entry_ensure_layout (entry, TRUE);
  text = pango_layout_get_text (layout);
  gtk_entry_get_layout_offsets (entry, &x, &y);
  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  if (type == CURSOR_DND)
    cursor_index = g_utf8_offset_to_pointer (text, priv->dnd_position) - text;
  else
    cursor_index = g_utf8_offset_to_pointer (text, priv->current_pos + priv->preedit_cursor) - text;

  if (!priv->overwrite_mode)
    block = FALSE;
  else
    block = _gtk_text_util_get_block_cursor_location (layout,
                                                      cursor_index, &cursor_rect, &block_at_line_end);
  if (!block)
    {
      gtk_snapshot_render_insertion_cursor (snapshot, context,
                                            x, y,
                                            layout, cursor_index, priv->resolved_dir);
    }
  else /* overwrite_mode */
    {
      graphene_rect_t bounds;

      bounds.origin.x = PANGO_PIXELS (cursor_rect.x) + x;
      bounds.origin.y = PANGO_PIXELS (cursor_rect.y) + y;
      bounds.size.width = PANGO_PIXELS (cursor_rect.width);
      bounds.size.height = PANGO_PIXELS (cursor_rect.height);

      gtk_style_context_save_to_node (context, priv->block_cursor_node);

      gtk_snapshot_push_clip (snapshot, &bounds, "Block Cursor");
      gtk_snapshot_render_background (snapshot, context,  0, 0, width, height);
      gtk_snapshot_render_layout (snapshot, context,  x, y, layout);
      gtk_snapshot_pop (snapshot);

      gtk_style_context_restore (context);
    }
}

static void
gtk_entry_handle_dragged (GtkTextHandle         *handle,
                          GtkTextHandlePosition  pos,
                          gint                   x,
                          gint                   y,
                          GtkEntry              *entry)
{
  gint cursor_pos, selection_bound_pos, tmp_pos;
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkTextHandleMode mode;
  gint *min, *max;

  gtk_entry_selection_bubble_popup_unset (entry);

  cursor_pos = priv->current_pos;
  selection_bound_pos = priv->selection_bound;
  mode = _gtk_text_handle_get_mode (handle);

  tmp_pos = gtk_entry_find_position (entry, x + priv->scroll_offset);

  if (mode == GTK_TEXT_HANDLE_MODE_CURSOR ||
      cursor_pos >= selection_bound_pos)
    {
      max = &cursor_pos;
      min = &selection_bound_pos;
    }
  else
    {
      max = &selection_bound_pos;
      min = &cursor_pos;
    }

  if (pos == GTK_TEXT_HANDLE_POSITION_SELECTION_END)
    {
      if (mode == GTK_TEXT_HANDLE_MODE_SELECTION)
        {
          gint min_pos;

          min_pos = MAX (*min + 1, 0);
          tmp_pos = MAX (tmp_pos, min_pos);
        }

      *max = tmp_pos;
    }
  else
    {
      if (mode == GTK_TEXT_HANDLE_MODE_SELECTION)
        {
          gint max_pos;

          max_pos = *max - 1;
          *min = MIN (tmp_pos, max_pos);
        }
    }

  if (cursor_pos != priv->current_pos ||
      selection_bound_pos != priv->selection_bound)
    {
      if (mode == GTK_TEXT_HANDLE_MODE_CURSOR)
        {
          priv->cursor_handle_dragged = TRUE;
          gtk_entry_set_positions (entry, cursor_pos, cursor_pos);
        }
      else
        {
          priv->selection_handle_dragged = TRUE;
          gtk_entry_set_positions (entry, cursor_pos, selection_bound_pos);
        }

      gtk_entry_update_handles (entry, mode);
    }

  gtk_entry_show_magnifier (entry, x, y);
}

static void
gtk_entry_handle_drag_started (GtkTextHandle         *handle,
                               GtkTextHandlePosition  pos,
                               GtkEntry              *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  priv->cursor_handle_dragged = FALSE;
  priv->selection_handle_dragged = FALSE;
}

static void
gtk_entry_handle_drag_finished (GtkTextHandle         *handle,
                                GtkTextHandlePosition  pos,
                                GtkEntry              *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (!priv->cursor_handle_dragged && !priv->selection_handle_dragged)
    {
      GtkSettings *settings;
      guint double_click_time;

      settings = gtk_widget_get_settings (GTK_WIDGET (entry));
      g_object_get (settings, "gtk-double-click-time", &double_click_time, NULL);
      if (g_get_monotonic_time() - priv->handle_place_time < double_click_time * 1000)
        {
          gtk_entry_select_word (entry);
          gtk_entry_update_handles (entry, GTK_TEXT_HANDLE_MODE_SELECTION);
        }
      else
        gtk_entry_selection_bubble_popup_set (entry);
    }

  if (priv->magnifier_popover)
    gtk_popover_popdown (GTK_POPOVER (priv->magnifier_popover));
}


/**
 * gtk_entry_reset_im_context:
 * @entry: a #GtkEntry
 *
 * Reset the input method context of the entry if needed.
 *
 * This can be necessary in the case where modifying the buffer
 * would confuse on-going input method behavior.
 */
void
gtk_entry_reset_im_context (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (priv->need_im_reset)
    {
      priv->need_im_reset = FALSE;
      gtk_im_context_reset (priv->im_context);
    }
}

/**
 * gtk_entry_im_context_filter_keypress:
 * @entry: a #GtkEntry
 * @event: (type Gdk.EventKey): the key event
 *
 * Allow the #GtkEntry input method to internally handle key press
 * and release events. If this function returns %TRUE, then no further
 * processing should be done for this key event. See
 * gtk_im_context_filter_keypress().
 *
 * Note that you are expected to call this function from your handler
 * when overriding key event handling. This is needed in the case when
 * you need to insert your own key handling between the input method
 * and the default key event handling of the #GtkEntry.
 * See gtk_text_view_reset_im_context() for an example of use.
 *
 * Returns: %TRUE if the input method handled the key event.
 */
gboolean
gtk_entry_im_context_filter_keypress (GtkEntry    *entry,
                                      GdkEventKey *event)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return gtk_im_context_filter_keypress (priv->im_context, event);
}

GtkIMContext*
_gtk_entry_get_im_context (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  return priv->im_context;
}

static gint
gtk_entry_find_position (GtkEntry *entry,
			 gint      x)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  PangoLayout *layout;
  PangoLayoutLine *line;
  gint index;
  gint pos;
  gint trailing;
  const gchar *text;
  gint cursor_index;
  
  layout = gtk_entry_ensure_layout (entry, TRUE);
  text = pango_layout_get_text (layout);
  cursor_index = g_utf8_offset_to_pointer (text, priv->current_pos) - text;

  line = pango_layout_get_lines_readonly (layout)->data;
  pango_layout_line_x_to_index (line, x * PANGO_SCALE, &index, &trailing);

  if (index >= cursor_index && priv->preedit_length)
    {
      if (index >= cursor_index + priv->preedit_length)
	index -= priv->preedit_length;
      else
	{
	  index = cursor_index;
	  trailing = 0;
	}
    }

  pos = g_utf8_pointer_to_offset (text, text + index);
  pos += trailing;

  return pos;
}

static void
gtk_entry_get_cursor_locations (GtkEntry   *entry,
				gint       *strong_x,
				gint       *weak_x)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  DisplayMode mode = gtk_entry_get_display_mode (entry);

  /* Nothing to display at all, so no cursor is relevant */
  if (mode == DISPLAY_BLANK)
    {
      if (strong_x)
	*strong_x = 0;
      
      if (weak_x)
	*weak_x = 0;
    }
  else
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, TRUE);
      const gchar *text = pango_layout_get_text (layout);
      PangoRectangle strong_pos, weak_pos;
      gint index;
  
      index = g_utf8_offset_to_pointer (text, priv->current_pos + priv->preedit_cursor) - text;

      pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);
      
      if (strong_x)
	*strong_x = strong_pos.x / PANGO_SCALE;
      
      if (weak_x)
	*weak_x = weak_pos.x / PANGO_SCALE;
    }
}

static gboolean
gtk_entry_get_is_selection_handle_dragged (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkTextHandlePosition pos;

  if (!priv->text_handle)
    return FALSE;

  if (_gtk_text_handle_get_mode (priv->text_handle) != GTK_TEXT_HANDLE_MODE_SELECTION)
    return FALSE;

  if (priv->current_pos >= priv->selection_bound)
    pos = GTK_TEXT_HANDLE_POSITION_SELECTION_START;
  else
    pos = GTK_TEXT_HANDLE_POSITION_SELECTION_END;

  return _gtk_text_handle_get_is_dragged (priv->text_handle, pos);
}

static void
gtk_entry_get_scroll_limits (GtkEntry *entry,
                             gint     *min_offset,
                             gint     *max_offset)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gfloat xalign;
  PangoLayout *layout;
  PangoLayoutLine *line;
  PangoRectangle logical_rect;
  gint text_width;

  layout = gtk_entry_ensure_layout (entry, TRUE);
  line = pango_layout_get_lines_readonly (layout)->data;

  pango_layout_line_get_extents (line, NULL, &logical_rect);

  /* Display as much text as we can */

  if (priv->resolved_dir == PANGO_DIRECTION_LTR)
      xalign = priv->xalign;
  else
      xalign = 1.0 - priv->xalign;

  text_width = PANGO_PIXELS(logical_rect.width);

  if (text_width > priv->text_width)
    {
      *min_offset = 0;
      *max_offset = text_width - priv->text_width;
    }
  else
    {
      *min_offset = (text_width - priv->text_width) * xalign;
      *max_offset = *min_offset;
    }
}

static void
gtk_entry_adjust_scroll (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gint min_offset, max_offset;
  gint strong_x, weak_x;
  gint strong_xoffset, weak_xoffset;
  GtkTextHandleMode handle_mode;
  GtkAllocation text_allocation;

  if (!gtk_widget_get_realized (GTK_WIDGET (entry)))
    return;

  gtk_entry_get_text_allocation (entry, &text_allocation);

  gtk_entry_get_scroll_limits (entry, &min_offset, &max_offset);

  priv->scroll_offset = CLAMP (priv->scroll_offset, min_offset, max_offset);

  if (gtk_entry_get_is_selection_handle_dragged (entry))
    {
      /* The text handle corresponding to the selection bound is
       * being dragged, ensure it stays onscreen even if we scroll
       * cursors away, this is so both handles can cause content
       * to scroll.
       */
      strong_x = weak_x = gtk_entry_get_selection_bound_location (entry);
    }
  else
    {
      /* And make sure cursors are on screen. Note that the cursor is
       * actually drawn one pixel into the INNER_BORDER space on
       * the right, when the scroll is at the utmost right. This
       * looks better to to me than confining the cursor inside the
       * border entirely, though it means that the cursor gets one
       * pixel closer to the edge of the widget on the right than
       * on the left. This might need changing if one changed
       * INNER_BORDER from 2 to 1, as one would do on a
       * small-screen-real-estate display.
       *
       * We always make sure that the strong cursor is on screen, and
       * put the weak cursor on screen if possible.
       */
      gtk_entry_get_cursor_locations (entry, &strong_x, &weak_x);
    }

  strong_xoffset = strong_x - priv->scroll_offset;

  if (strong_xoffset < 0)
    {
      priv->scroll_offset += strong_xoffset;
      strong_xoffset = 0;
    }
  else if (strong_xoffset > text_allocation.width)
    {
      priv->scroll_offset += strong_xoffset - text_allocation.width;
      strong_xoffset = text_allocation.width;
    }

  weak_xoffset = weak_x - priv->scroll_offset;

  if (weak_xoffset < 0 && strong_xoffset - weak_xoffset <= text_allocation.width)
    {
      priv->scroll_offset += weak_xoffset;
    }
  else if (weak_xoffset > text_allocation.width &&
	   strong_xoffset - (weak_xoffset - text_allocation.width) >= 0)
    {
      priv->scroll_offset += weak_xoffset - text_allocation.width;
    }

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_SCROLL_OFFSET]);

  if (priv->text_handle)
    {
      handle_mode = _gtk_text_handle_get_mode (priv->text_handle);

      if (handle_mode != GTK_TEXT_HANDLE_MODE_NONE)
        gtk_entry_update_handles (entry, handle_mode);
    }
}

static gint
gtk_entry_move_visually (GtkEntry *entry,
			 gint      start,
			 gint      count)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
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
	  GdkKeymap *keymap = gdk_display_get_keymap (gtk_widget_get_display (GTK_WIDGET (entry)));
	  PangoDirection keymap_direction = gdk_keymap_get_direction (keymap);

	  strong = keymap_direction == priv->resolved_dir;
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

      if (new_index < 0)
        index = 0;
      else if (new_index != G_MAXINT)
        index = new_index;
      
      while (new_trailing--)
	index = g_utf8_next_char (text + index) - text;
    }
  
  return g_utf8_pointer_to_offset (text, text + index);
}

static gint
gtk_entry_move_logically (GtkEntry *entry,
			  gint      start,
			  gint      count)
{
  gint new_pos = start;
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (entry));

  /* Prevent any leak of information */
  if (gtk_entry_get_display_mode (entry) != DISPLAY_NORMAL)
    {
      new_pos = CLAMP (start + count, 0, length);
    }
  else
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
      PangoLogAttr *log_attrs;
      gint n_attrs;

      pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);

      while (count > 0 && new_pos < length)
	{
	  do
	    new_pos++;
	  while (new_pos < length && !log_attrs[new_pos].is_cursor_position);
	  
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
			     gint      start,
                             gboolean  allow_whitespace)
{
  gint new_pos = start;
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (entry));

  /* Prevent any leak of information */
  if (gtk_entry_get_display_mode (entry) != DISPLAY_NORMAL)
    {
      new_pos = length;
    }
  else if (new_pos < length)
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
      PangoLogAttr *log_attrs;
      gint n_attrs;

      pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);
      
      /* Find the next word boundary */
      new_pos++;
      while (new_pos < n_attrs - 1 && !(log_attrs[new_pos].is_word_end ||
                                        (log_attrs[new_pos].is_word_start && allow_whitespace)))
	new_pos++;

      g_free (log_attrs);
    }

  return new_pos;
}


static gint
gtk_entry_move_backward_word (GtkEntry *entry,
			      gint      start,
                              gboolean  allow_whitespace)
{
  gint new_pos = start;

  /* Prevent any leak of information */
  if (gtk_entry_get_display_mode (entry) != DISPLAY_NORMAL)
    {
      new_pos = 0;
    }
  else if (start > 0)
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
      PangoLogAttr *log_attrs;
      gint n_attrs;

      pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);

      new_pos = start - 1;

      /* Find the previous word boundary */
      while (new_pos > 0 && !(log_attrs[new_pos].is_word_start || 
                              (log_attrs[new_pos].is_word_end && allow_whitespace)))
	new_pos--;

      g_free (log_attrs);
    }

  return new_pos;
}

static void
gtk_entry_delete_whitespace (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
  PangoLogAttr *log_attrs;
  gint n_attrs;
  gint start, end;

  pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);

  start = end = priv->current_pos;
  
  while (start > 0 && log_attrs[start-1].is_white)
    start--;

  while (end < n_attrs && log_attrs[end].is_white)
    end++;

  g_free (log_attrs);

  if (start != end)
    gtk_editable_delete_text (GTK_EDITABLE (entry), start, end);
}


static void
gtk_entry_select_word (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gint start_pos = gtk_entry_move_backward_word (entry, priv->current_pos, TRUE);
  gint end_pos = gtk_entry_move_forward_word (entry, priv->current_pos, TRUE);

  gtk_editable_select_region (GTK_EDITABLE (entry), start_pos, end_pos);
}

static void
gtk_entry_select_line (GtkEntry *entry)
{
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}

static gint
truncate_multiline (const gchar *text)
{
  gint length;

  for (length = 0;
       text[length] && text[length] != '\n' && text[length] != '\r';
       length++);

  return length;
}

static void
paste_received (GObject      *clipboard,
                GAsyncResult *result,
		gpointer      data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  GtkEditable *editable = GTK_EDITABLE (entry);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  char *text;
  gint pos, start, end;
  gint length = -1;
  gboolean popup_completion;
  GtkEntryCompletion *completion;

  text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (clipboard), result, NULL);
  if (text == NULL)
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
      return;
    }

  if (priv->insert_pos >= 0)
    {
      gint pos, start, end;
      pos = priv->insert_pos;
      gtk_editable_get_selection_bounds (editable, &start, &end);
      if (!((start <= pos && pos <= end) || (end <= pos && pos <= start)))
	gtk_editable_select_region (editable, pos, pos);
      priv->insert_pos = -1;
    }
      
  completion = gtk_entry_get_completion (entry);

  if (priv->truncate_multiline)
    length = truncate_multiline (text);

  /* only complete if the selection is at the end */
  popup_completion = (gtk_entry_buffer_get_length (get_buffer (entry)) ==
                      MAX (priv->current_pos, priv->selection_bound));

  if (completion)
    {
      if (gtk_widget_get_mapped (completion->priv->popup_window))
        _gtk_entry_completion_popdown (completion);

      if (!popup_completion && completion->priv->changed_id > 0)
        g_signal_handler_block (entry, completion->priv->changed_id);
    }

  begin_change (entry);
  if (gtk_editable_get_selection_bounds (editable, &start, &end))
    gtk_editable_delete_text (editable, start, end);

  pos = priv->current_pos;
  gtk_editable_insert_text (editable, text, length, &pos);
  gtk_editable_set_position (editable, pos);
  end_change (entry);

  if (completion &&
      !popup_completion && completion->priv->changed_id > 0)
    g_signal_handler_unblock (entry, completion->priv->changed_id);

  g_free (text);
  g_object_unref (entry);
}

static void
gtk_entry_paste (GtkEntry     *entry,
		 GdkClipboard *clipboard)
{
  g_object_ref (entry);
  gdk_clipboard_read_text_async (clipboard,
                                 NULL,
                                 paste_received,
                                 entry);
}

static void
gtk_entry_update_primary_selection (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GdkClipboard *clipboard;
  gint start, end;

  if (!gtk_widget_get_realized (GTK_WIDGET (entry)))
    return;

  clipboard = gtk_widget_get_primary_clipboard (GTK_WIDGET (entry));
  
  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
    {
      gdk_clipboard_set_content (clipboard, priv->selection_content);
    }
  else
    {
      if (gdk_clipboard_get_content (clipboard) == priv->selection_content)
	gdk_clipboard_set_content (clipboard, NULL);
    }
}

static void
gtk_entry_clear_icon (GtkEntry             *entry,
                      GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info = priv->icons[icon_pos];
  GtkImageType storage_type;

  if (icon_info == NULL)
    return;

  storage_type = gtk_image_get_storage_type (GTK_IMAGE (icon_info->widget));

  if (storage_type == GTK_IMAGE_EMPTY)
    return;

  g_object_freeze_notify (G_OBJECT (entry));

  switch (storage_type)
    {
    case GTK_IMAGE_PAINTABLE:
      g_object_notify_by_pspec (G_OBJECT (entry),
                                entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                            ? PROP_PAINTABLE_PRIMARY
                                            : PROP_PAINTABLE_SECONDARY]);
      break;

    case GTK_IMAGE_ICON_NAME:
      g_object_notify_by_pspec (G_OBJECT (entry),
                                entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                            ? PROP_ICON_NAME_PRIMARY
                                            : PROP_ICON_NAME_SECONDARY]);
      break;

    case GTK_IMAGE_GICON:
      g_object_notify_by_pspec (G_OBJECT (entry),
                                entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                            ? PROP_GICON_PRIMARY
                                            : PROP_GICON_SECONDARY]);
      break;

    case GTK_IMAGE_EMPTY:
    default:
      g_assert_not_reached ();
      break;
    }

  gtk_image_clear (GTK_IMAGE (icon_info->widget));

  g_object_notify_by_pspec (G_OBJECT (entry),
                            entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                            ? PROP_STORAGE_TYPE_PRIMARY
                            : PROP_STORAGE_TYPE_SECONDARY]);

  g_object_thaw_notify (G_OBJECT (entry));
}

/* Public API
 */

/**
 * gtk_entry_new:
 *
 * Creates a new entry.
 *
 * Returns: a new #GtkEntry.
 */
GtkWidget*
gtk_entry_new (void)
{
  return g_object_new (GTK_TYPE_ENTRY, NULL);
}

/**
 * gtk_entry_new_with_buffer:
 * @buffer: The buffer to use for the new #GtkEntry.
 *
 * Creates a new entry with the specified text buffer.
 *
 * Returns: a new #GtkEntry
 */
GtkWidget*
gtk_entry_new_with_buffer (GtkEntryBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_ENTRY_BUFFER (buffer), NULL);
  return g_object_new (GTK_TYPE_ENTRY, "buffer", buffer, NULL);
}

static GtkEntryBuffer*
get_buffer (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->buffer == NULL)
    {
      GtkEntryBuffer *buffer;
      buffer = gtk_entry_buffer_new (NULL, 0);
      gtk_entry_set_buffer (entry, buffer);
      g_object_unref (buffer);
    }

  return priv->buffer;
}

/**
 * gtk_entry_get_buffer:
 * @entry: a #GtkEntry
 *
 * Get the #GtkEntryBuffer object which holds the text for
 * this widget.
 *
 * Returns: (transfer none): A #GtkEntryBuffer object.
 */
GtkEntryBuffer*
gtk_entry_get_buffer (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return get_buffer (entry);
}

/**
 * gtk_entry_set_buffer:
 * @entry: a #GtkEntry
 * @buffer: a #GtkEntryBuffer
 *
 * Set the #GtkEntryBuffer object which holds the text for
 * this widget.
 */
void
gtk_entry_set_buffer (GtkEntry       *entry,
                      GtkEntryBuffer *buffer)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GObject *obj;
  gboolean had_buffer = FALSE;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (buffer)
    {
      g_return_if_fail (GTK_IS_ENTRY_BUFFER (buffer));
      g_object_ref (buffer);
    }

  if (priv->buffer)
    {
      had_buffer = TRUE;
      buffer_disconnect_signals (entry);
      g_object_unref (priv->buffer);
    }

  priv->buffer = buffer;

  if (priv->buffer)
     buffer_connect_signals (entry);

  obj = G_OBJECT (entry);
  g_object_freeze_notify (obj);
  g_object_notify_by_pspec (obj, entry_props[PROP_BUFFER]);
  g_object_notify_by_pspec (obj, entry_props[PROP_TEXT]);
  g_object_notify_by_pspec (obj, entry_props[PROP_TEXT_LENGTH]);
  g_object_notify_by_pspec (obj, entry_props[PROP_MAX_LENGTH]);
  g_object_notify_by_pspec (obj, entry_props[PROP_VISIBILITY]);
  g_object_notify_by_pspec (obj, entry_props[PROP_INVISIBLE_CHAR]);
  g_object_notify_by_pspec (obj, entry_props[PROP_INVISIBLE_CHAR_SET]);
  g_object_thaw_notify (obj);

  if (had_buffer)
    {
      gtk_editable_set_position (GTK_EDITABLE (entry), 0);
      gtk_entry_recompute (entry);
    }
}

/**
 * gtk_entry_set_text:
 * @entry: a #GtkEntry
 * @text: the new text
 *
 * Sets the text in the widget to the given
 * value, replacing the current contents.
 *
 * See gtk_entry_buffer_set_text().
 */
void
gtk_entry_set_text (GtkEntry    *entry,
		    const gchar *text)
{
  gint tmp_pos;
  GtkEntryCompletion *completion;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (text != NULL);

  /* Actually setting the text will affect the cursor and selection;
   * if the contents don't actually change, this will look odd to the user.
   */
  if (strcmp (gtk_entry_buffer_get_text (get_buffer (entry)), text) == 0)
    return;

  completion = gtk_entry_get_completion (entry);
  if (completion && completion->priv->changed_id > 0)
    g_signal_handler_block (entry, completion->priv->changed_id);

  begin_change (entry);
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);
  tmp_pos = 0;
  gtk_editable_insert_text (GTK_EDITABLE (entry), text, strlen (text), &tmp_pos);
  end_change (entry);

  if (completion && completion->priv->changed_id > 0)
    g_signal_handler_unblock (entry, completion->priv->changed_id);
}

/**
 * gtk_entry_set_visibility:
 * @entry: a #GtkEntry
 * @visible: %TRUE if the contents of the entry are displayed
 *           as plaintext
 *
 * Sets whether the contents of the entry are visible or not.
 * When visibility is set to %FALSE, characters are displayed
 * as the invisible char, and will also appear that way when
 * the text in the entry widget is copied elsewhere.
 *
 * By default, GTK+ picks the best invisible character available
 * in the current font, but it can be changed with
 * gtk_entry_set_invisible_char().
 *
 * Note that you probably want to set #GtkEntry:input-purpose
 * to %GTK_INPUT_PURPOSE_PASSWORD or %GTK_INPUT_PURPOSE_PIN to
 * inform input methods about the purpose of this entry,
 * in addition to setting visibility to %FALSE.
 */
void
gtk_entry_set_visibility (GtkEntry *entry,
			  gboolean visible)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  visible = visible != FALSE;

  if (priv->visible != visible)
    {
      priv->visible = visible;

      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_VISIBILITY]);
      gtk_entry_recompute (entry);
    }
}

/**
 * gtk_entry_get_visibility:
 * @entry: a #GtkEntry
 *
 * Retrieves whether the text in @entry is visible. See
 * gtk_entry_set_visibility().
 *
 * Returns: %TRUE if the text is currently visible
 **/
gboolean
gtk_entry_get_visibility (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return priv->visible;

}

/**
 * gtk_entry_set_invisible_char:
 * @entry: a #GtkEntry
 * @ch: a Unicode character
 * 
 * Sets the character to use in place of the actual text when
 * gtk_entry_set_visibility() has been called to set text visibility
 * to %FALSE. i.e. this is the character used in “password mode” to
 * show the user how many characters have been typed. By default, GTK+
 * picks the best invisible char available in the current font. If you
 * set the invisible char to 0, then the user will get no feedback
 * at all; there will be no text on the screen as they type.
 **/
void
gtk_entry_set_invisible_char (GtkEntry *entry,
                              gunichar  ch)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (!priv->invisible_char_set)
    {
      priv->invisible_char_set = TRUE;
      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_INVISIBLE_CHAR_SET]);
    }

  if (ch == priv->invisible_char)
    return;

  priv->invisible_char = ch;
  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_INVISIBLE_CHAR]);
  gtk_entry_recompute (entry);
}

/**
 * gtk_entry_get_invisible_char:
 * @entry: a #GtkEntry
 *
 * Retrieves the character displayed in place of the real characters
 * for entries with visibility set to false. See gtk_entry_set_invisible_char().
 *
 * Returns: the current invisible char, or 0, if the entry does not
 *               show invisible text at all. 
 **/
gunichar
gtk_entry_get_invisible_char (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return priv->invisible_char;
}

/**
 * gtk_entry_unset_invisible_char:
 * @entry: a #GtkEntry
 *
 * Unsets the invisible char previously set with
 * gtk_entry_set_invisible_char(). So that the
 * default invisible char is used again.
 **/
void
gtk_entry_unset_invisible_char (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gunichar ch;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (!priv->invisible_char_set)
    return;

  priv->invisible_char_set = FALSE;
  ch = find_invisible_char (GTK_WIDGET (entry));

  if (priv->invisible_char != ch)
    {
      priv->invisible_char = ch;
      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_INVISIBLE_CHAR]);
    }

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_INVISIBLE_CHAR_SET]);
  gtk_entry_recompute (entry);
}

/**
 * gtk_entry_set_overwrite_mode:
 * @entry: a #GtkEntry
 * @overwrite: new value
 *
 * Sets whether the text is overwritten when typing in the #GtkEntry.
 **/
void
gtk_entry_set_overwrite_mode (GtkEntry *entry,
                              gboolean  overwrite)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (priv->overwrite_mode == overwrite)
    return;

  gtk_entry_toggle_overwrite (entry);

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_OVERWRITE_MODE]);
}

/**
 * gtk_entry_get_overwrite_mode:
 * @entry: a #GtkEntry
 *
 * Gets the value set by gtk_entry_set_overwrite_mode().
 *
 * Returns: whether the text is overwritten when typing.
 **/
gboolean
gtk_entry_get_overwrite_mode (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return priv->overwrite_mode;

}

/**
 * gtk_entry_get_text:
 * @entry: a #GtkEntry
 *
 * Retrieves the contents of the entry widget.
 * See also gtk_editable_get_chars().
 *
 * This is equivalent to getting @entry's #GtkEntryBuffer and calling
 * gtk_entry_buffer_get_text() on it.
 *
 * Returns: a pointer to the contents of the widget as a
 *      string. This string points to internally allocated
 *      storage in the widget and must not be freed, modified or
 *      stored.
 **/
const gchar*
gtk_entry_get_text (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return gtk_entry_buffer_get_text (get_buffer (entry));
}

/**
 * gtk_entry_set_max_length:
 * @entry: a #GtkEntry
 * @max: the maximum length of the entry, or 0 for no maximum.
 *   (other than the maximum length of entries.) The value passed in will
 *   be clamped to the range 0-65536.
 *
 * Sets the maximum allowed length of the contents of the widget. If
 * the current contents are longer than the given length, then they
 * will be truncated to fit.
 *
 * This is equivalent to getting @entry's #GtkEntryBuffer and
 * calling gtk_entry_buffer_set_max_length() on it.
 * ]|
 **/
void
gtk_entry_set_max_length (GtkEntry     *entry,
                          gint          max)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));
  gtk_entry_buffer_set_max_length (get_buffer (entry), max);
}

/**
 * gtk_entry_get_max_length:
 * @entry: a #GtkEntry
 *
 * Retrieves the maximum allowed length of the text in
 * @entry. See gtk_entry_set_max_length().
 *
 * This is equivalent to getting @entry's #GtkEntryBuffer and
 * calling gtk_entry_buffer_get_max_length() on it.
 *
 * Returns: the maximum allowed number of characters
 *               in #GtkEntry, or 0 if there is no maximum.
 **/
gint
gtk_entry_get_max_length (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return gtk_entry_buffer_get_max_length (get_buffer (entry));
}

/**
 * gtk_entry_get_text_length:
 * @entry: a #GtkEntry
 *
 * Retrieves the current length of the text in
 * @entry. 
 *
 * This is equivalent to getting @entry's #GtkEntryBuffer and
 * calling gtk_entry_buffer_get_length() on it.

 *
 * Returns: the current number of characters
 *               in #GtkEntry, or 0 if there are none.
 **/
guint16
gtk_entry_get_text_length (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return gtk_entry_buffer_get_length (get_buffer (entry));
}

/**
 * gtk_entry_set_activates_default:
 * @entry: a #GtkEntry
 * @setting: %TRUE to activate window’s default widget on Enter keypress
 *
 * If @setting is %TRUE, pressing Enter in the @entry will activate the default
 * widget for the window containing the entry. This usually means that
 * the dialog box containing the entry will be closed, since the default
 * widget is usually one of the dialog buttons.
 *
 * (For experts: if @setting is %TRUE, the entry calls
 * gtk_window_activate_default() on the window containing the entry, in
 * the default handler for the #GtkEntry::activate signal.)
 **/
void
gtk_entry_set_activates_default (GtkEntry *entry,
                                 gboolean  setting)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  setting = setting != FALSE;

  if (setting != priv->activates_default)
    {
      priv->activates_default = setting;
      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_ACTIVATES_DEFAULT]);
    }
}

/**
 * gtk_entry_get_activates_default:
 * @entry: a #GtkEntry
 *
 * Retrieves the value set by gtk_entry_set_activates_default().
 *
 * Returns: %TRUE if the entry will activate the default widget
 */
gboolean
gtk_entry_get_activates_default (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return priv->activates_default;
}

/**
 * gtk_entry_set_width_chars:
 * @entry: a #GtkEntry
 * @n_chars: width in chars
 *
 * Changes the size request of the entry to be about the right size
 * for @n_chars characters. Note that it changes the size
 * request, the size can still be affected by
 * how you pack the widget into containers. If @n_chars is -1, the
 * size reverts to the default entry size.
 **/
void
gtk_entry_set_width_chars (GtkEntry *entry,
                           gint      n_chars)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (priv->width_chars != n_chars)
    {
      priv->width_chars = n_chars;
      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_WIDTH_CHARS]);
      gtk_widget_queue_resize (GTK_WIDGET (entry));
    }
}

/**
 * gtk_entry_get_width_chars:
 * @entry: a #GtkEntry
 * 
 * Gets the value set by gtk_entry_set_width_chars().
 * 
 * Returns: number of chars to request space for, or negative if unset
 **/
gint
gtk_entry_get_width_chars (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return priv->width_chars;
}

/**
 * gtk_entry_set_max_width_chars:
 * @entry: a #GtkEntry
 * @n_chars: the new desired maximum width, in characters
 *
 * Sets the desired maximum width in characters of @entry.
 */
void
gtk_entry_set_max_width_chars (GtkEntry *entry,
                               gint      n_chars)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (priv->max_width_chars != n_chars)
    {
      priv->max_width_chars = n_chars;
      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_MAX_WIDTH_CHARS]);
      gtk_widget_queue_resize (GTK_WIDGET (entry));
    }
}

/**
 * gtk_entry_get_max_width_chars:
 * @entry: a #GtkEntry
 *
 * Retrieves the desired maximum width of @entry, in characters.
 * See gtk_entry_set_max_width_chars().
 *
 * Returns: the maximum width of the entry, in characters
 */
gint
gtk_entry_get_max_width_chars (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return priv->max_width_chars;
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
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  setting = (setting != FALSE);

  if (setting == gtk_entry_get_has_frame (entry))
    return;

  context = gtk_widget_get_style_context (GTK_WIDGET (entry));
  if (setting)
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_FLAT);
  else
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_FLAT);

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_HAS_FRAME]);
}

/**
 * gtk_entry_get_has_frame:
 * @entry: a #GtkEntry
 * 
 * Gets the value set by gtk_entry_set_has_frame().
 * 
 * Returns: whether the entry has a beveled frame
 **/
gboolean
gtk_entry_get_has_frame (GtkEntry *entry)
{
  GtkStyleContext *context;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  context = gtk_widget_get_style_context (GTK_WIDGET (entry));

  return !gtk_style_context_has_class (context, GTK_STYLE_CLASS_FLAT);
}

/**
 * gtk_entry_get_layout:
 * @entry: a #GtkEntry
 * 
 * Gets the #PangoLayout used to display the entry.
 * The layout is useful to e.g. convert text positions to
 * pixel positions, in combination with gtk_entry_get_layout_offsets().
 * The returned layout is owned by the entry and must not be 
 * modified or freed by the caller.
 *
 * Keep in mind that the layout text may contain a preedit string, so
 * gtk_entry_layout_index_to_text_index() and
 * gtk_entry_text_index_to_layout_index() are needed to convert byte
 * indices in the layout to byte indices in the entry contents.
 *
 * Returns: (transfer none): the #PangoLayout for this entry
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
 * Converts from a position in the entry’s #PangoLayout (returned by
 * gtk_entry_get_layout()) to a position in the entry contents
 * (returned by gtk_entry_get_text()).
 *
 * Returns: byte index into the entry contents
 **/
gint
gtk_entry_layout_index_to_text_index (GtkEntry *entry,
                                      gint      layout_index)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  PangoLayout *layout;
  const gchar *text;
  gint cursor_index;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  layout = gtk_entry_ensure_layout (entry, TRUE);
  text = pango_layout_get_text (layout);
  cursor_index = g_utf8_offset_to_pointer (text, priv->current_pos) - text;

  if (layout_index >= cursor_index && priv->preedit_length)
    {
      if (layout_index >= cursor_index + priv->preedit_length)
	layout_index -= priv->preedit_length;
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
 * Converts from a position in the entry contents (returned
 * by gtk_entry_get_text()) to a position in the
 * entry’s #PangoLayout (returned by gtk_entry_get_layout(),
 * with text retrieved via pango_layout_get_text()).
 *
 * Returns: byte index into the entry layout text
 **/
gint
gtk_entry_text_index_to_layout_index (GtkEntry *entry,
                                      gint      text_index)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  PangoLayout *layout;
  const gchar *text;
  gint cursor_index;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  layout = gtk_entry_ensure_layout (entry, TRUE);
  text = pango_layout_get_text (layout);
  cursor_index = g_utf8_offset_to_pointer (text, priv->current_pos) - text;

  if (text_index > cursor_index)
    text_index += priv->preedit_length;

  return text_index;
}

/**
 * gtk_entry_get_layout_offsets:
 * @entry: a #GtkEntry
 * @x: (out) (allow-none): location to store X offset of layout, or %NULL
 * @y: (out) (allow-none): location to store Y offset of layout, or %NULL
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
 * change; you’ll need to connect to the “notify::scroll-offset”
 * signal to track this. Remember when using the #PangoLayout
 * functions you need to convert to and from pixels using
 * PANGO_PIXELS() or #PANGO_SCALE.
 *
 * Keep in mind that the layout text may contain a preedit string, so
 * gtk_entry_layout_index_to_text_index() and
 * gtk_entry_text_index_to_layout_index() are needed to convert byte
 * indices in the layout to byte indices in the entry contents.
 **/
void
gtk_entry_get_layout_offsets (GtkEntry *entry,
                              gint     *x,
                              gint     *y)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  get_layout_position (entry, x, y);
}


/**
 * gtk_entry_set_alignment:
 * @entry: a #GtkEntry
 * @xalign: The horizontal alignment, from 0 (left) to 1 (right).
 *          Reversed for RTL layouts
 * 
 * Sets the alignment for the contents of the entry. This controls
 * the horizontal positioning of the contents when the displayed
 * text is shorter than the width of the entry.
 **/
void
gtk_entry_set_alignment (GtkEntry *entry, gfloat xalign)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (xalign < 0.0)
    xalign = 0.0;
  else if (xalign > 1.0)
    xalign = 1.0;

  if (xalign != priv->xalign)
    {
      priv->xalign = xalign;
      gtk_entry_recompute (entry);
      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_XALIGN]);
    }
}

/**
 * gtk_entry_get_alignment:
 * @entry: a #GtkEntry
 *
 * Gets the value set by gtk_entry_set_alignment().
 *
 * Returns: the alignment
 **/
gfloat
gtk_entry_get_alignment (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0.0);

  return priv->xalign;
}

/**
 * gtk_entry_set_icon_from_paintable:
 * @entry: a #GtkEntry
 * @icon_pos: Icon position
 * @paintable: (allow-none): A #GdkPaintable, or %NULL
 *
 * Sets the icon shown in the specified position using a #GdkPaintable
 *
 * If @paintable is %NULL, no icon will be shown in the specified position.
 */
void
gtk_entry_set_icon_from_paintable (GtkEntry             *entry,
                                 GtkEntryIconPosition  icon_pos,
                                 GdkPaintable           *paintable)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_object_freeze_notify (G_OBJECT (entry));

  if (paintable)
    {
      g_object_ref (paintable);

      gtk_image_set_from_paintable (GTK_IMAGE (icon_info->widget), paintable);

      if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
        {
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_PAINTABLE_PRIMARY]);
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_STORAGE_TYPE_PRIMARY]);
        }
      else
        {
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_PAINTABLE_SECONDARY]);
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_STORAGE_TYPE_SECONDARY]);
        }

      g_object_unref (paintable);
    }
  else
    gtk_entry_clear_icon (entry, icon_pos);

  if (gtk_widget_get_visible (GTK_WIDGET (entry)))
    gtk_widget_queue_resize (GTK_WIDGET (entry));

  g_object_thaw_notify (G_OBJECT (entry));
}

/**
 * gtk_entry_set_icon_from_icon_name:
 * @entry: A #GtkEntry
 * @icon_pos: The position at which to set the icon
 * @icon_name: (allow-none): An icon name, or %NULL
 *
 * Sets the icon shown in the entry at the specified position
 * from the current icon theme.
 *
 * If the icon name isn’t known, a “broken image” icon will be displayed
 * instead.
 *
 * If @icon_name is %NULL, no icon will be shown in the specified position.
 */
void
gtk_entry_set_icon_from_icon_name (GtkEntry             *entry,
                                   GtkEntryIconPosition  icon_pos,
                                   const gchar          *icon_name)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_object_freeze_notify (G_OBJECT (entry));


  if (icon_name != NULL)
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (icon_info->widget), icon_name);

      if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
        {
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_ICON_NAME_PRIMARY]);
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_STORAGE_TYPE_PRIMARY]);
        }
      else
        {
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_ICON_NAME_SECONDARY]);
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_STORAGE_TYPE_SECONDARY]);
        }
    }
  else
    gtk_entry_clear_icon (entry, icon_pos);

  if (gtk_widget_get_visible (GTK_WIDGET (entry)))
    gtk_widget_queue_resize (GTK_WIDGET (entry));

  g_object_thaw_notify (G_OBJECT (entry));
}

/**
 * gtk_entry_set_icon_from_gicon:
 * @entry: A #GtkEntry
 * @icon_pos: The position at which to set the icon
 * @icon: (allow-none): The icon to set, or %NULL
 *
 * Sets the icon shown in the entry at the specified position
 * from the current icon theme.
 * If the icon isn’t known, a “broken image” icon will be displayed
 * instead.
 *
 * If @icon is %NULL, no icon will be shown in the specified position.
 */
void
gtk_entry_set_icon_from_gicon (GtkEntry             *entry,
                               GtkEntryIconPosition  icon_pos,
                               GIcon                *icon)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_object_freeze_notify (G_OBJECT (entry));

  if (icon)
    {
      gtk_image_set_from_gicon (GTK_IMAGE (icon_info->widget), icon);

      if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
        {
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_GICON_PRIMARY]);
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_STORAGE_TYPE_PRIMARY]);
        }
      else
        {
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_GICON_SECONDARY]);
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_STORAGE_TYPE_SECONDARY]);
        }
    }
  else
    gtk_entry_clear_icon (entry, icon_pos);

  if (gtk_widget_get_visible (GTK_WIDGET (entry)))
    gtk_widget_queue_resize (GTK_WIDGET (entry));

  g_object_thaw_notify (G_OBJECT (entry));
}

/**
 * gtk_entry_set_icon_activatable:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 * @activatable: %TRUE if the icon should be activatable
 *
 * Sets whether the icon is activatable.
 */
void
gtk_entry_set_icon_activatable (GtkEntry             *entry,
                                GtkEntryIconPosition  icon_pos,
                                gboolean              activatable)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  activatable = activatable != FALSE;

  if (icon_info->nonactivatable != !activatable)
    {
      icon_info->nonactivatable = !activatable;

      g_object_notify_by_pspec (G_OBJECT (entry),
                                entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                            ? PROP_ACTIVATABLE_PRIMARY
                                            : PROP_ACTIVATABLE_SECONDARY]);
    }
}

/**
 * gtk_entry_get_icon_activatable:
 * @entry: a #GtkEntry
 * @icon_pos: Icon position
 *
 * Returns whether the icon is activatable.
 *
 * Returns: %TRUE if the icon is activatable.
 */
gboolean
gtk_entry_get_icon_activatable (GtkEntry             *entry,
                                GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), FALSE);

  icon_info = priv->icons[icon_pos];

  return (!icon_info || !icon_info->nonactivatable);
}

/**
 * gtk_entry_get_icon_paintable:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 *
 * Retrieves the #GdkPaintable used for the icon.
 *
 * If no #GdkPaintable was used for the icon, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): A #GdkPaintable, or %NULL if no icon is
 *     set for this position or the icon set is not a #GdkPaintable.
 */
GdkPaintable *
gtk_entry_get_icon_paintable (GtkEntry             *entry,
                              GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;

  return gtk_image_get_paintable (GTK_IMAGE (icon_info->widget));
}

/**
 * gtk_entry_get_icon_gicon:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 *
 * Retrieves the #GIcon used for the icon, or %NULL if there is
 * no icon or if the icon was set by some other method (e.g., by
 * paintable or icon name).
 *
 * Returns: (transfer none) (nullable): A #GIcon, or %NULL if no icon is set
 *     or if the icon is not a #GIcon
 */
GIcon *
gtk_entry_get_icon_gicon (GtkEntry             *entry,
                          GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;

  return gtk_image_get_gicon (GTK_IMAGE (icon_info->widget));
}

/**
 * gtk_entry_get_icon_name:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 *
 * Retrieves the icon name used for the icon, or %NULL if there is
 * no icon or if the icon was set by some other method (e.g., by
 * paintable or gicon).
 *
 * Returns: (nullable): An icon name, or %NULL if no icon is set or if the icon
 *          wasn’t set from an icon name
 */
const gchar *
gtk_entry_get_icon_name (GtkEntry             *entry,
                         GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;

  return gtk_image_get_icon_name (GTK_IMAGE (icon_info->widget));
}

/**
 * gtk_entry_set_icon_sensitive:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 * @sensitive: Specifies whether the icon should appear
 *             sensitive or insensitive
 *
 * Sets the sensitivity for the specified icon.
 */
void
gtk_entry_set_icon_sensitive (GtkEntry             *entry,
                              GtkEntryIconPosition  icon_pos,
                              gboolean              sensitive)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  if (gtk_widget_get_sensitive (icon_info->widget) != sensitive)
    {
      gtk_widget_set_sensitive (icon_info->widget, sensitive);

      icon_info->pressed = FALSE;

      g_object_notify_by_pspec (G_OBJECT (entry),
                                entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                            ? PROP_SENSITIVE_PRIMARY
                                            : PROP_SENSITIVE_SECONDARY]);
    }
}

/**
 * gtk_entry_get_icon_sensitive:
 * @entry: a #GtkEntry
 * @icon_pos: Icon position
 *
 * Returns whether the icon appears sensitive or insensitive.
 *
 * Returns: %TRUE if the icon is sensitive.
 */
gboolean
gtk_entry_get_icon_sensitive (GtkEntry             *entry,
                              GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), TRUE);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), TRUE);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return TRUE; /* Default of the property */

  return gtk_widget_get_sensitive (icon_info->widget);
}

/**
 * gtk_entry_get_icon_storage_type:
 * @entry: a #GtkEntry
 * @icon_pos: Icon position
 *
 * Gets the type of representation being used by the icon
 * to store image data. If the icon has no image data,
 * the return value will be %GTK_IMAGE_EMPTY.
 *
 * Returns: image representation being used
 **/
GtkImageType
gtk_entry_get_icon_storage_type (GtkEntry             *entry,
                                 GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), GTK_IMAGE_EMPTY);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), GTK_IMAGE_EMPTY);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return GTK_IMAGE_EMPTY;

  return gtk_image_get_storage_type (GTK_IMAGE (icon_info->widget));
}

/**
 * gtk_entry_get_icon_at_pos:
 * @entry: a #GtkEntry
 * @x: the x coordinate of the position to find, relative to @entry
 * @y: the y coordinate of the position to find, relative to @entry
 *
 * Finds the icon at the given position and return its index. The
 * position’s coordinates are relative to the @entry’s top left corner.
 * If @x, @y doesn’t lie inside an icon, -1 is returned.
 * This function is intended for use in a #GtkWidget::query-tooltip
 * signal handler.
 *
 * Returns: the index of the icon at the given position, or -1
 */
gint
gtk_entry_get_icon_at_pos (GtkEntry *entry,
                           gint      x,
                           gint      y)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  guint i;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), -1);

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];
      int icon_x, icon_y;

      if (icon_info == NULL)
        continue;

      gtk_widget_translate_coordinates (GTK_WIDGET (entry), icon_info->widget,
                                        x, y, &icon_x, &icon_y);

      if (gtk_widget_contains (icon_info->widget, icon_x, icon_y))
        return i;
    }

  return -1;
}

/**
 * gtk_entry_set_icon_drag_source:
 * @entry: a #GtkEntry
 * @icon_pos: icon position
 * @formats: the targets (data formats) in which the data can be provided
 * @actions: a bitmask of the allowed drag actions
 *
 * Sets up the icon at the given position so that GTK+ will start a drag
 * operation when the user clicks and drags the icon.
 *
 * To handle the drag operation, you need to connect to the usual
 * #GtkWidget::drag-data-get (or possibly #GtkWidget::drag-data-delete)
 * signal, and use gtk_entry_get_current_icon_drag_source() in
 * your signal handler to find out if the drag was started from
 * an icon.
 *
 * By default, GTK+ uses the icon as the drag icon. You can use the 
 * #GtkWidget::drag-begin signal to set a different icon. Note that you 
 * have to use g_signal_connect_after() to ensure that your signal handler
 * gets executed after the default handler.
 */
void
gtk_entry_set_icon_drag_source (GtkEntry             *entry,
                                GtkEntryIconPosition  icon_pos,
                                GdkContentFormats    *formats,
                                GdkDragAction         actions)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  if (icon_info->target_list)
    gdk_content_formats_unref (icon_info->target_list);
  icon_info->target_list = formats;
  if (icon_info->target_list)
    gdk_content_formats_ref (icon_info->target_list);

  icon_info->actions = actions;
}

/**
 * gtk_entry_get_current_icon_drag_source:
 * @entry: a #GtkEntry
 *
 * Returns the index of the icon which is the source of the current
 * DND operation, or -1.
 *
 * This function is meant to be used in a #GtkWidget::drag-data-get
 * callback.
 *
 * Returns: index of the icon which is the source of the current
 *          DND operation, or -1.
 */
gint
gtk_entry_get_current_icon_drag_source (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info = NULL;
  gint i;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), -1);

  for (i = 0; i < MAX_ICONS; i++)
    {
      if ((icon_info = priv->icons[i]))
        {
          if (icon_info->in_drag)
            return i;
        }
    }

  return -1;
}

/**
 * gtk_entry_get_icon_area:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 * @icon_area: (out): Return location for the icon’s area
 *
 * Gets the area where entry’s icon at @icon_pos is drawn.
 * This function is useful when drawing something to the
 * entry in a draw callback.
 *
 * If the entry is not realized or has no icon at the given position,
 * @icon_area is filled with zeros. Otherwise, @icon_area will be filled
 * with the icon's allocation, relative to @entry's allocation.
 */
void
gtk_entry_get_icon_area (GtkEntry             *entry,
                         GtkEntryIconPosition  icon_pos,
                         GdkRectangle         *icon_area)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (icon_area != NULL);

  icon_info = priv->icons[icon_pos];

  if (icon_info)
    {
      graphene_rect_t r;

      gtk_widget_compute_bounds (icon_info->widget, GTK_WIDGET (entry), &r);

      *icon_area = (GdkRectangle){
        floorf (r.origin.x),
        floorf (r.origin.y),
        ceilf (r.size.width),
        ceilf (r.size.height),
      };
    }
  else
    {
      icon_area->x = 0;
      icon_area->y = 0;
      icon_area->width = 0;
      icon_area->height = 0;
    }
}

static void
ensure_has_tooltip (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gchar *text = gtk_widget_get_tooltip_text (GTK_WIDGET (entry));
  gboolean has_tooltip = text != NULL;

  if (!has_tooltip)
    {
      int i;

      for (i = 0; i < MAX_ICONS; i++)
        {
          EntryIconInfo *icon_info = priv->icons[i];

          if (icon_info != NULL && icon_info->tooltip != NULL)
            {
              has_tooltip = TRUE;
              break;
            }
        }
    }
  else
    {
      g_free (text);
    }

  gtk_widget_set_has_tooltip (GTK_WIDGET (entry), has_tooltip);
}

/**
 * gtk_entry_get_icon_tooltip_text:
 * @entry: a #GtkEntry
 * @icon_pos: the icon position
 *
 * Gets the contents of the tooltip on the icon at the specified 
 * position in @entry.
 * 
 * Returns: (nullable): the tooltip text, or %NULL. Free the returned
 *     string with g_free() when done.
 */
gchar *
gtk_entry_get_icon_tooltip_text (GtkEntry             *entry,
                                 GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;
  gchar *text = NULL;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;
 
  if (icon_info->tooltip && 
      !pango_parse_markup (icon_info->tooltip, -1, 0, NULL, &text, NULL, NULL))
    g_assert (NULL == text); /* text should still be NULL in case of markup errors */

  return text;
}

/**
 * gtk_entry_set_icon_tooltip_text:
 * @entry: a #GtkEntry
 * @icon_pos: the icon position
 * @tooltip: (allow-none): the contents of the tooltip for the icon, or %NULL
 *
 * Sets @tooltip as the contents of the tooltip for the icon
 * at the specified position.
 *
 * Use %NULL for @tooltip to remove an existing tooltip.
 *
 * See also gtk_widget_set_tooltip_text() and 
 * gtk_entry_set_icon_tooltip_markup().
 *
 * If you unset the widget tooltip via gtk_widget_set_tooltip_text() or
 * gtk_widget_set_tooltip_markup(), this sets GtkWidget:has-tooltip to %FALSE,
 * which suppresses icon tooltips too. You can resolve this by then calling
 * gtk_widget_set_has_tooltip() to set GtkWidget:has-tooltip back to %TRUE, or
 * setting at least one non-empty tooltip on any icon achieves the same result.
 */
void
gtk_entry_set_icon_tooltip_text (GtkEntry             *entry,
                                 GtkEntryIconPosition  icon_pos,
                                 const gchar          *tooltip)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_free (icon_info->tooltip);

  /* Treat an empty string as a NULL string,
   * because an empty string would be useless for a tooltip:
   */
  if (tooltip && tooltip[0] == '\0')
    tooltip = NULL;

  icon_info->tooltip = tooltip ? g_markup_escape_text (tooltip, -1) : NULL;

  ensure_has_tooltip (entry);

  g_object_notify_by_pspec (G_OBJECT (entry),
                            entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                        ? PROP_TOOLTIP_TEXT_PRIMARY
                                        : PROP_TOOLTIP_TEXT_SECONDARY]);
}

/**
 * gtk_entry_get_icon_tooltip_markup:
 * @entry: a #GtkEntry
 * @icon_pos: the icon position
 *
 * Gets the contents of the tooltip on the icon at the specified 
 * position in @entry.
 * 
 * Returns: (nullable): the tooltip text, or %NULL. Free the returned
 *     string with g_free() when done.
 */
gchar *
gtk_entry_get_icon_tooltip_markup (GtkEntry             *entry,
                                   GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;
 
  return g_strdup (icon_info->tooltip);
}

/**
 * gtk_entry_set_icon_tooltip_markup:
 * @entry: a #GtkEntry
 * @icon_pos: the icon position
 * @tooltip: (allow-none): the contents of the tooltip for the icon, or %NULL
 *
 * Sets @tooltip as the contents of the tooltip for the icon at
 * the specified position. @tooltip is assumed to be marked up with
 * the [Pango text markup language][PangoMarkupFormat].
 *
 * Use %NULL for @tooltip to remove an existing tooltip.
 *
 * See also gtk_widget_set_tooltip_markup() and 
 * gtk_entry_set_icon_tooltip_text().
 */
void
gtk_entry_set_icon_tooltip_markup (GtkEntry             *entry,
                                   GtkEntryIconPosition  icon_pos,
                                   const gchar          *tooltip)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_free (icon_info->tooltip);

  /* Treat an empty string as a NULL string,
   * because an empty string would be useless for a tooltip:
   */
  if (tooltip && tooltip[0] == '\0')
    tooltip = NULL;

  icon_info->tooltip = g_strdup (tooltip);

  ensure_has_tooltip (entry);

  g_object_notify_by_pspec (G_OBJECT (entry),
                            entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                        ? PROP_TOOLTIP_MARKUP_PRIMARY
                                        : PROP_TOOLTIP_MARKUP_SECONDARY]);
}

static gboolean
gtk_entry_query_tooltip (GtkWidget  *widget,
                         gint        x,
                         gint        y,
                         gboolean    keyboard_tip,
                         GtkTooltip *tooltip)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;
  gint icon_pos;

  if (!keyboard_tip)
    {
      icon_pos = gtk_entry_get_icon_at_pos (entry, x, y);
      if (icon_pos != -1)
        {
          if ((icon_info = priv->icons[icon_pos]) != NULL)
            {
              if (icon_info->tooltip)
                {
                  gtk_tooltip_set_markup (tooltip, icon_info->tooltip);
                  return TRUE;
                }

              return FALSE;
            }
        }
    }

  return GTK_WIDGET_CLASS (gtk_entry_parent_class)->query_tooltip (widget,
                                                                   x, y,
                                                                   keyboard_tip,
                                                                   tooltip);
}


/* Quick hack of a popup menu
 */
static void
activate_cb (GtkWidget *menuitem,
	     GtkEntry  *entry)
{
  const gchar *signal;

  signal = g_object_get_qdata (G_OBJECT (menuitem), quark_gtk_signal);
  g_signal_emit_by_name (entry, signal);
}


static gboolean
gtk_entry_mnemonic_activate (GtkWidget *widget,
			     gboolean   group_cycling)
{
  gtk_widget_grab_focus (widget);
  return GDK_EVENT_STOP;
}

static void
check_undo_icon_grab (GtkEntry      *entry,
                      EntryIconInfo *info)
{
  if (!info->device ||
      !gtk_widget_device_is_shadowed (GTK_WIDGET (entry), info->device))
    return;

  info->pressed = FALSE;
  info->current_sequence = NULL;
  info->device = NULL;
}

static void
gtk_entry_grab_notify (GtkWidget *widget,
                       gboolean   was_grabbed)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (GTK_ENTRY (widget));
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      if (priv->icons[i])
        check_undo_icon_grab (GTK_ENTRY (widget), priv->icons[i]);
    }
}

static void
append_action_signal (GtkEntry     *entry,
		      GtkWidget    *menu,
		      const gchar  *label,
		      const gchar  *signal,
                      gboolean      sensitive)
{
  GtkWidget *menuitem = gtk_menu_item_new_with_mnemonic (label);

  g_object_set_qdata (G_OBJECT (menuitem), quark_gtk_signal, (char *)signal);
  g_signal_connect (menuitem, "activate",
		    G_CALLBACK (activate_cb), entry);

  gtk_widget_set_sensitive (menuitem, sensitive);
  
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}
	
static void
popup_menu_detach (GtkWidget *attach_widget,
		   GtkMenu   *menu)
{
  GtkEntry *entry_attach = GTK_ENTRY (attach_widget);
  GtkEntryPrivate *priv_attach = gtk_entry_get_instance_private (entry_attach);

  priv_attach->popup_menu = NULL;
}

static void
gtk_entry_do_popup (GtkEntry       *entry,
                    const GdkEvent *event)
{
  GtkEntryPrivate *info_entry_priv = gtk_entry_get_instance_private (entry);
  GdkEvent *trigger_event;

  /* In order to know what entries we should make sensitive, we
   * ask for the current targets of the clipboard, and when
   * we get them, then we actually pop up the menu.
   */
  trigger_event = event ? gdk_event_copy (event) : gtk_get_current_event ();

  if (gtk_widget_get_realized (GTK_WIDGET (entry)))
    {
      DisplayMode mode;
      gboolean clipboard_contains_text;
      GtkWidget *menu;
      GtkWidget *menuitem;

      clipboard_contains_text = gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (gtk_widget_get_clipboard (GTK_WIDGET (entry))),
                                                                   G_TYPE_STRING);
      if (info_entry_priv->popup_menu)
	gtk_widget_destroy (info_entry_priv->popup_menu);

      info_entry_priv->popup_menu = menu = gtk_menu_new ();
      gtk_style_context_add_class (gtk_widget_get_style_context (menu),
                                   GTK_STYLE_CLASS_CONTEXT_MENU);

      gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (entry), popup_menu_detach);

      mode = gtk_entry_get_display_mode (entry);
      append_action_signal (entry, menu, _("Cu_t"), "cut-clipboard",
                            info_entry_priv->editable && mode == DISPLAY_NORMAL &&
                            info_entry_priv->current_pos != info_entry_priv->selection_bound);

      append_action_signal (entry, menu, _("_Copy"), "copy-clipboard",
                            mode == DISPLAY_NORMAL &&
                            info_entry_priv->current_pos != info_entry_priv->selection_bound);

      append_action_signal (entry, menu, _("_Paste"), "paste-clipboard",
                            info_entry_priv->editable && clipboard_contains_text);

      menuitem = gtk_menu_item_new_with_mnemonic (_("_Delete"));
      gtk_widget_set_sensitive (menuitem, info_entry_priv->editable && info_entry_priv->current_pos != info_entry_priv->selection_bound);
      g_signal_connect_swapped (menuitem, "activate",
                                G_CALLBACK (gtk_entry_delete_cb), entry);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      menuitem = gtk_separator_menu_item_new ();
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      menuitem = gtk_menu_item_new_with_mnemonic (_("Select _All"));
      gtk_widget_set_sensitive (menuitem, gtk_entry_buffer_get_length (info_entry_priv->buffer) > 0);
      g_signal_connect_swapped (menuitem, "activate",
                                G_CALLBACK (gtk_entry_select_all), entry);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      if (info_entry_priv->show_emoji_icon ||
          (gtk_entry_get_input_hints (entry) & GTK_INPUT_HINT_NO_EMOJI) == 0)
        {
          menuitem = gtk_menu_item_new_with_mnemonic (_("Insert _Emoji"));
          gtk_widget_set_sensitive (menuitem,
                                    mode == DISPLAY_NORMAL &&
                                    info_entry_priv->editable);
          g_signal_connect_swapped (menuitem, "activate",
                                    G_CALLBACK (gtk_entry_insert_emoji), entry);
          gtk_widget_show (menuitem);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        }

      g_signal_emit (entry, signals[POPULATE_POPUP], 0, menu);

      if (trigger_event && gdk_event_triggers_context_menu (trigger_event))
        gtk_menu_popup_at_pointer (GTK_MENU (menu), trigger_event);
      else
        {
          gtk_menu_popup_at_widget (GTK_MENU (menu),
                                    GTK_WIDGET (entry),
                                    GDK_GRAVITY_SOUTH_EAST,
                                    GDK_GRAVITY_NORTH_WEST,
                                    trigger_event);

          gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
        }
    }

  g_clear_object (&trigger_event);
}

static gboolean
gtk_entry_popup_menu (GtkWidget *widget)
{
  gtk_entry_do_popup (GTK_ENTRY (widget), NULL);
  return GDK_EVENT_STOP;
}

static void
show_or_hide_handles (GtkWidget  *popover,
                      GParamSpec *pspec,
                      GtkEntry   *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gboolean visible;
  GtkTextHandle *handle;
  GtkTextHandleMode mode;

  visible = gtk_widget_get_visible (popover);

  handle = priv->text_handle;
  mode = _gtk_text_handle_get_mode (handle);

  if (mode == GTK_TEXT_HANDLE_MODE_CURSOR)
    {
      _gtk_text_handle_set_visible (handle, GTK_TEXT_HANDLE_POSITION_CURSOR, !visible);
    }
  else if (mode == GTK_TEXT_HANDLE_MODE_SELECTION)
    {
      _gtk_text_handle_set_visible (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_START, !visible);
      _gtk_text_handle_set_visible (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_END, !visible);
    }
}

static void
activate_bubble_cb (GtkWidget *item,
	            GtkEntry  *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  const gchar *signal;

  signal = g_object_get_qdata (G_OBJECT (item), quark_gtk_signal);
  gtk_widget_hide (priv->selection_bubble);
  if (strcmp (signal, "select-all") == 0)
    gtk_entry_select_all (entry);
  else
    g_signal_emit_by_name (entry, signal);
}

static void
append_bubble_action (GtkEntry     *entry,
                      GtkWidget    *toolbar,
                      const gchar  *label,
                      const gchar  *icon_name,
                      const gchar  *signal,
                      gboolean      sensitive)
{
  GtkWidget *item, *image;

  item = gtk_button_new ();
  gtk_widget_set_focus_on_click (item, FALSE);
  image = gtk_image_new_from_icon_name (icon_name);
  gtk_widget_show (image);
  gtk_container_add (GTK_CONTAINER (item), image);
  gtk_widget_set_tooltip_text (item, label);
  gtk_style_context_add_class (gtk_widget_get_style_context (item), "image-button");
  g_object_set_qdata (G_OBJECT (item), quark_gtk_signal, (char *)signal);
  g_signal_connect (item, "clicked", G_CALLBACK (activate_bubble_cb), entry);
  gtk_widget_set_sensitive (GTK_WIDGET (item), sensitive);
  gtk_widget_show (GTK_WIDGET (item));
  gtk_container_add (GTK_CONTAINER (toolbar), item);
}

static gboolean
gtk_entry_selection_bubble_popup_show (gpointer user_data)
{
  GtkEntry *entry = user_data;
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  cairo_rectangle_int_t rect;
  GtkAllocation allocation;
  gint start_x, end_x;
  gboolean has_selection;
  gboolean has_clipboard;
  gboolean all_selected;
  DisplayMode mode;
  GtkWidget *box;
  GtkWidget *toolbar;
  gint length, start, end;
  GtkAllocation text_allocation;

  gtk_entry_get_text_allocation (entry, &text_allocation);

  has_selection = gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end);
  length = gtk_entry_buffer_get_length (get_buffer (entry));
  all_selected = (start == 0) && (end == length);

  if (!has_selection && !priv->editable)
    {
      priv->selection_bubble_timeout_id = 0;
      return G_SOURCE_REMOVE;
    }

  if (priv->selection_bubble)
    gtk_widget_destroy (priv->selection_bubble);

  priv->selection_bubble = gtk_popover_new (GTK_WIDGET (entry));
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->selection_bubble),
                               GTK_STYLE_CLASS_TOUCH_SELECTION);
  gtk_popover_set_position (GTK_POPOVER (priv->selection_bubble), GTK_POS_BOTTOM);
  gtk_popover_set_modal (GTK_POPOVER (priv->selection_bubble), FALSE);
  g_signal_connect (priv->selection_bubble, "notify::visible",
                    G_CALLBACK (show_or_hide_handles), entry);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  g_object_set (box, "margin", 10, NULL);
  gtk_widget_show (box);
  toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_show (toolbar);
  gtk_container_add (GTK_CONTAINER (priv->selection_bubble), box);
  gtk_container_add (GTK_CONTAINER (box), toolbar);

  has_clipboard = gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (gtk_widget_get_clipboard (GTK_WIDGET (entry))),
                                                     G_TYPE_STRING);
  mode = gtk_entry_get_display_mode (entry);

  if (priv->editable && has_selection && mode == DISPLAY_NORMAL)
    append_bubble_action (entry, toolbar, _("Select all"), "edit-select-all-symbolic", "select-all", !all_selected);

  if (priv->editable && has_selection && mode == DISPLAY_NORMAL)
    append_bubble_action (entry, toolbar, _("Cut"), "edit-cut-symbolic", "cut-clipboard", TRUE);

  if (has_selection && mode == DISPLAY_NORMAL)
    append_bubble_action (entry, toolbar, _("Copy"), "edit-copy-symbolic", "copy-clipboard", TRUE);

  if (priv->editable)
    append_bubble_action (entry, toolbar, _("Paste"), "edit-paste-symbolic", "paste-clipboard", has_clipboard);

  if (priv->populate_all)
    g_signal_emit (entry, signals[POPULATE_POPUP], 0, box);

  gtk_widget_get_allocation (GTK_WIDGET (entry), &allocation);

  gtk_entry_get_cursor_locations (entry, &start_x, NULL);

  start_x -= priv->scroll_offset;
  start_x = CLAMP (start_x, 0, text_allocation.width);
  rect.y = text_allocation.y - allocation.y;
  rect.height = text_allocation.height;

  if (has_selection)
    {
      end_x = gtk_entry_get_selection_bound_location (entry) - priv->scroll_offset;
      end_x = CLAMP (end_x, 0, text_allocation.width);

      rect.x = text_allocation.x - allocation.x + MIN (start_x, end_x);
      rect.width = ABS (end_x - start_x);
    }
  else
    {
      rect.x = text_allocation.x - allocation.x + start_x;
      rect.width = 0;
    }

  rect.x -= 5;
  rect.y -= 5;
  rect.width += 10;
  rect.height += 10;

  gtk_popover_set_pointing_to (GTK_POPOVER (priv->selection_bubble), &rect);
  gtk_widget_show (priv->selection_bubble);

  priv->selection_bubble_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
gtk_entry_selection_bubble_popup_unset (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->selection_bubble)
    gtk_widget_hide (priv->selection_bubble);

  if (priv->selection_bubble_timeout_id)
    {
      g_source_remove (priv->selection_bubble_timeout_id);
      priv->selection_bubble_timeout_id = 0;
    }
}

static void
gtk_entry_selection_bubble_popup_set (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->selection_bubble_timeout_id)
    g_source_remove (priv->selection_bubble_timeout_id);

  priv->selection_bubble_timeout_id =
    g_timeout_add (50, gtk_entry_selection_bubble_popup_show, entry);
  g_source_set_name_by_id (priv->selection_bubble_timeout_id, "[gtk+] gtk_entry_selection_bubble_popup_cb");
}

static void
gtk_entry_drag_begin (GtkWidget      *widget,
                      GdkDragContext *context)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gchar *text;
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];
      
      if (icon_info != NULL)
        {
          if (icon_info->in_drag) 
            {
              gtk_drag_set_icon_definition (context,
                                            gtk_image_get_definition (GTK_IMAGE (icon_info->widget)),
                                            -2, -2);
              return;
            }
        }
    }

  text = _gtk_entry_get_selected_text (entry);

  if (text)
    {
      gint *ranges, n_ranges;
      GdkPaintable *paintable;

      paintable = gtk_text_util_create_drag_icon (widget, text, -1);
      gtk_entry_get_pixel_ranges (entry, &ranges, &n_ranges);

      gtk_drag_set_icon_paintable (context,
                                   paintable,
                                   priv->drag_start_x - ranges[0],
                                   priv->drag_start_y);

      g_free (ranges);
      g_object_unref (paintable);
      g_free (text);
    }
}

static void
gtk_entry_drag_end (GtkWidget      *widget,
                    GdkDragContext *context)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];

      if (icon_info != NULL)
        icon_info->in_drag = 0;
    }
}

static void
gtk_entry_drag_leave (GtkWidget        *widget,
		      GdkDragContext   *context,
		      guint             time)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  gtk_drag_unhighlight (widget);
  priv->dnd_position = -1;
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
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GdkAtom target = NULL;

  if (priv->editable)
    target = gtk_drag_dest_find_target (widget, context, NULL);

  if (target != NULL)
    {
      priv->drop_position = gtk_entry_find_position (entry, x + priv->scroll_offset);
      gtk_drag_get_data (widget, context, target, time);
    }
  else
    gtk_drag_finish (context, FALSE, time);
  
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
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkWidget *source_widget;
  GdkDragAction suggested_action;
  gint new_position, old_position;
  gint sel1, sel2;

  old_position = priv->dnd_position;
  new_position = gtk_entry_find_position (entry, x + priv->scroll_offset);

  if (priv->editable &&
      gtk_drag_dest_find_target (widget, context, NULL) != NULL)
    {
      source_widget = gtk_drag_get_source_widget (context);
      suggested_action = gdk_drag_context_get_suggested_action (context);

      if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &sel1, &sel2) ||
          new_position < sel1 || new_position > sel2)
        {
          if (source_widget == widget)
	    {
	      /* Default to MOVE, unless the user has
	       * pressed ctrl or alt to affect available actions
	       */
	      if ((gdk_drag_context_get_actions (context) & GDK_ACTION_MOVE) != 0)
	        suggested_action = GDK_ACTION_MOVE;
	    }

          priv->dnd_position = new_position;
        }
      else
        {
          if (source_widget == widget)
	    suggested_action = 0;	/* Can't drop in selection where drag started */

          priv->dnd_position = -1;
        }
    }
  else
    {
      /* Entry not editable, or no text */
      suggested_action = 0;
      priv->dnd_position = -1;
    }

  if (show_placeholder_text (entry))
    priv->dnd_position = -1;

  gdk_drag_status (context, suggested_action, time);
  if (suggested_action == 0)
    gtk_drag_unhighlight (widget);
  else
    gtk_drag_highlight (widget);

  if (priv->dnd_position != old_position)
    gtk_widget_queue_draw (widget);

  return TRUE;
}

static void
gtk_entry_drag_data_received (GtkWidget        *widget,
			      GdkDragContext   *context,
			      GtkSelectionData *selection_data,
			      guint             time)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (widget);
  gchar *str;

  str = (gchar *) gtk_selection_data_get_text (selection_data);

  if (str && priv->editable)
    {
      gint sel1, sel2;
      gint length = -1;

      if (priv->truncate_multiline)
        length = truncate_multiline (str);

      if (!gtk_editable_get_selection_bounds (editable, &sel1, &sel2) ||
	  priv->drop_position < sel1 || priv->drop_position > sel2)
	{
	  gtk_editable_insert_text (editable, str, length, &priv->drop_position);
	}
      else
	{
	  /* Replacing selection */
          begin_change (entry);
	  gtk_editable_delete_text (editable, sel1, sel2);
	  gtk_editable_insert_text (editable, str, length, &sel1);
          end_change (entry);
	}
      
      gtk_drag_finish (context, TRUE, time);
    }
  else
    {
      /* Drag and drop didn't happen! */
      gtk_drag_finish (context, FALSE, time);
    }

  g_free (str);
}

static void
gtk_entry_drag_data_get (GtkWidget        *widget,
			 GdkDragContext   *context,
			 GtkSelectionData *selection_data,
			 guint             time)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (widget);
  gint sel_start, sel_end;
  gint i;

  /* If there is an icon drag going on, exit early. */
  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];

      if (icon_info != NULL)
        {
          if (icon_info->in_drag)
            return;
        }
    }

  if (gtk_editable_get_selection_bounds (editable, &sel_start, &sel_end))
    {
      gchar *str = _gtk_entry_get_display_text (GTK_ENTRY (widget), sel_start, sel_end);

      gtk_selection_data_set_text (selection_data, str, -1);
      
      g_free (str);
    }

}

static void
gtk_entry_drag_data_delete (GtkWidget      *widget,
			    GdkDragContext *context)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (widget);
  gint sel_start, sel_end;
  gint i;

  /* If there is an icon drag going on, exit early. */
  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];

      if (icon_info != NULL)
        {
          if (icon_info->in_drag)
            return;
        }
    }

  if (priv->editable &&
      gtk_editable_get_selection_bounds (editable, &sel_start, &sel_end))
    gtk_editable_delete_text (editable, sel_start, sel_end);
}

/* We display the cursor when
 *
 *  - the selection is empty, AND
 *  - the widget has focus
 */

#define CURSOR_ON_MULTIPLIER 2
#define CURSOR_OFF_MULTIPLIER 1
#define CURSOR_PEND_MULTIPLIER 3
#define CURSOR_DIVIDER 3

static gboolean
cursor_blinks (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (gtk_widget_has_focus (GTK_WIDGET (entry)) &&
      priv->editable &&
      priv->selection_bound == priv->current_pos)
    {
      GtkSettings *settings;
      gboolean blink;

      settings = gtk_widget_get_settings (GTK_WIDGET (entry));
      g_object_get (settings, "gtk-cursor-blink", &blink, NULL);

      return blink;
    }
  else
    return FALSE;
}

static gboolean
get_middle_click_paste (GtkEntry *entry)
{
  GtkSettings *settings;
  gboolean paste;

  settings = gtk_widget_get_settings (GTK_WIDGET (entry));
  g_object_get (settings, "gtk-enable-primary-paste", &paste, NULL);

  return paste;
}

static gint
get_cursor_time (GtkEntry *entry)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (entry));
  gint time;

  g_object_get (settings, "gtk-cursor-blink-time", &time, NULL);

  return time;
}

static gint
get_cursor_blink_timeout (GtkEntry *entry)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (entry));
  gint timeout;

  g_object_get (settings, "gtk-cursor-blink-timeout", &timeout, NULL);

  return timeout;
}

static void
show_cursor (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkWidget *widget;

  if (!priv->cursor_visible)
    {
      priv->cursor_visible = TRUE;

      widget = GTK_WIDGET (entry);
      if (gtk_widget_has_focus (widget) && priv->selection_bound == priv->current_pos)
	gtk_widget_queue_draw (widget);
    }
}

static void
hide_cursor (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkWidget *widget;

  if (priv->cursor_visible)
    {
      priv->cursor_visible = FALSE;

      widget = GTK_WIDGET (entry);
      if (gtk_widget_has_focus (widget) && priv->selection_bound == priv->current_pos)
	gtk_widget_queue_draw (widget);
    }
}

/*
 * Blink!
 */
static gint
blink_cb (gpointer data)
{
  GtkEntry *entry = data;
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gint blink_timeout;

  if (!gtk_widget_has_focus (GTK_WIDGET (entry)))
    {
      g_warning ("GtkEntry - did not receive a focus-out event.\n"
		 "If you handle this event, you must return\n"
		 "GDK_EVENT_PROPAGATE so the entry gets the event as well");

      gtk_entry_check_cursor_blink (entry);

      return G_SOURCE_REMOVE;
    }
  
  g_assert (priv->selection_bound == priv->current_pos);
  
  blink_timeout = get_cursor_blink_timeout (entry);
  if (priv->blink_time > 1000 * blink_timeout && 
      blink_timeout < G_MAXINT/1000) 
    {
      /* we've blinked enough without the user doing anything, stop blinking */
      show_cursor (entry);
      priv->blink_timeout = 0;
    } 
  else if (priv->cursor_visible)
    {
      hide_cursor (entry);
      priv->blink_timeout = g_timeout_add (get_cursor_time (entry) * CURSOR_OFF_MULTIPLIER / CURSOR_DIVIDER,
                                           blink_cb,
                                           entry);
      g_source_set_name_by_id (priv->blink_timeout, "[gtk+] blink_cb");
    }
  else
    {
      show_cursor (entry);
      priv->blink_time += get_cursor_time (entry);
      priv->blink_timeout = g_timeout_add (get_cursor_time (entry) * CURSOR_ON_MULTIPLIER / CURSOR_DIVIDER,
                                           blink_cb,
                                           entry);
      g_source_set_name_by_id (priv->blink_timeout, "[gtk+] blink_cb");
    }

  return G_SOURCE_REMOVE;
}

static void
gtk_entry_check_cursor_blink (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (cursor_blinks (entry))
    {
      if (!priv->blink_timeout)
	{
	  show_cursor (entry);
	  priv->blink_timeout = g_timeout_add (get_cursor_time (entry) * CURSOR_ON_MULTIPLIER / CURSOR_DIVIDER,
                                               blink_cb,
                                               entry);
	  g_source_set_name_by_id (priv->blink_timeout, "[gtk+] blink_cb");
	}
    }
  else
    {
      if (priv->blink_timeout)
	{
	  g_source_remove (priv->blink_timeout);
	  priv->blink_timeout = 0;
	}

      priv->cursor_visible = TRUE;
    }
}

static void
gtk_entry_pend_cursor_blink (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (cursor_blinks (entry))
    {
      if (priv->blink_timeout != 0)
	g_source_remove (priv->blink_timeout);

      priv->blink_timeout = g_timeout_add (get_cursor_time (entry) * CURSOR_PEND_MULTIPLIER / CURSOR_DIVIDER,
                                           blink_cb,
                                           entry);
      g_source_set_name_by_id (priv->blink_timeout, "[gtk+] blink_cb");
      show_cursor (entry);
    }
}

static void
gtk_entry_reset_blink_time (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  priv->blink_time = 0;
}

/**
 * gtk_entry_set_completion:
 * @entry: A #GtkEntry
 * @completion: (allow-none): The #GtkEntryCompletion or %NULL
 *
 * Sets @completion to be the auxiliary completion object to use with @entry.
 * All further configuration of the completion mechanism is done on
 * @completion using the #GtkEntryCompletion API. Completion is disabled if
 * @completion is set to %NULL.
 */
void
gtk_entry_set_completion (GtkEntry           *entry,
                          GtkEntryCompletion *completion)
{
  GtkEntryCompletion *old;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (!completion || GTK_IS_ENTRY_COMPLETION (completion));

  old = gtk_entry_get_completion (entry);

  if (old == completion)
    return;
  
  if (old)
    {
      _gtk_entry_completion_disconnect (old);
      g_object_unref (old);
    }

  if (!completion)
    {
      g_object_set_qdata (G_OBJECT (entry), quark_entry_completion, NULL);
      return;
    }

  /* hook into the entry */
  g_object_ref (completion);

  _gtk_entry_completion_connect (completion, entry);

  g_object_set_qdata (G_OBJECT (entry), quark_entry_completion, completion);

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_COMPLETION]);
}

/**
 * gtk_entry_get_completion:
 * @entry: A #GtkEntry
 *
 * Returns the auxiliary completion object currently in use by @entry.
 *
 * Returns: (transfer none): The auxiliary completion object currently
 *     in use by @entry.
 */
GtkEntryCompletion *
gtk_entry_get_completion (GtkEntry *entry)
{
  GtkEntryCompletion *completion;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  completion = GTK_ENTRY_COMPLETION (g_object_get_qdata (G_OBJECT (entry), quark_entry_completion));

  return completion;
}

static void
gtk_entry_ensure_progress_widget (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->progress_widget)
    return;

  priv->progress_widget = g_object_new (GTK_TYPE_PROGRESS_BAR,
                                        "css-name", "progress",
                                        NULL);

  gtk_widget_set_parent (priv->progress_widget, GTK_WIDGET (entry));

  update_node_ordering (entry);
}

/**
 * gtk_entry_set_progress_fraction:
 * @entry: a #GtkEntry
 * @fraction: fraction of the task that’s been completed
 *
 * Causes the entry’s progress indicator to “fill in” the given
 * fraction of the bar. The fraction should be between 0.0 and 1.0,
 * inclusive.
 */
void
gtk_entry_set_progress_fraction (GtkEntry *entry,
                                 gdouble   fraction)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gdouble          old_fraction;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_entry_ensure_progress_widget (entry);
  old_fraction = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (priv->progress_widget));
  fraction = CLAMP (fraction, 0.0, 1.0);

  if (fraction != old_fraction)
    {
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress_widget), fraction);
      gtk_widget_set_visible (priv->progress_widget, fraction > 0);

      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_PROGRESS_FRACTION]);
    }
}

/**
 * gtk_entry_get_progress_fraction:
 * @entry: a #GtkEntry
 *
 * Returns the current fraction of the task that’s been completed.
 * See gtk_entry_set_progress_fraction().
 *
 * Returns: a fraction from 0.0 to 1.0
 */
gdouble
gtk_entry_get_progress_fraction (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0.0);

  if (priv->progress_widget)
    return gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (priv->progress_widget));

  return 0.0;
}

/**
 * gtk_entry_set_progress_pulse_step:
 * @entry: a #GtkEntry
 * @fraction: fraction between 0.0 and 1.0
 *
 * Sets the fraction of total entry width to move the progress
 * bouncing block for each call to gtk_entry_progress_pulse().
 */
void
gtk_entry_set_progress_pulse_step (GtkEntry *entry,
                                   gdouble   fraction)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  fraction = CLAMP (fraction, 0.0, 1.0);
  gtk_entry_ensure_progress_widget (entry);


  if (fraction != gtk_progress_bar_get_pulse_step (GTK_PROGRESS_BAR (priv->progress_widget)))
    {
      gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (priv->progress_widget), fraction);
      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_PROGRESS_PULSE_STEP]);
    }
}

/**
 * gtk_entry_get_progress_pulse_step:
 * @entry: a #GtkEntry
 *
 * Retrieves the pulse step set with gtk_entry_set_progress_pulse_step().
 *
 * Returns: a fraction from 0.0 to 1.0
 */
gdouble
gtk_entry_get_progress_pulse_step (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0.0);

  if (priv->progress_widget)
    return gtk_progress_bar_get_pulse_step (GTK_PROGRESS_BAR (priv->progress_widget));

  return 0.0;
}

/**
 * gtk_entry_progress_pulse:
 * @entry: a #GtkEntry
 *
 * Indicates that some progress is made, but you don’t know how much.
 * Causes the entry’s progress indicator to enter “activity mode,”
 * where a block bounces back and forth. Each call to
 * gtk_entry_progress_pulse() causes the block to move by a little bit
 * (the amount of movement per pulse is determined by
 * gtk_entry_set_progress_pulse_step()).
 */
void
gtk_entry_progress_pulse (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (priv->progress_widget)
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (priv->progress_widget));
}

/**
 * gtk_entry_set_placeholder_text:
 * @entry: a #GtkEntry
 * @text: (nullable): a string to be displayed when @entry is empty and unfocused, or %NULL
 *
 * Sets text to be displayed in @entry when it is empty and unfocused.
 * This can be used to give a visual hint of the expected contents of
 * the #GtkEntry.
 *
 * Note that since the placeholder text gets removed when the entry
 * received focus, using this feature is a bit problematic if the entry
 * is given the initial focus in a window. Sometimes this can be
 * worked around by delaying the initial focus setting until the
 * first key event arrives.
 **/
void
gtk_entry_set_placeholder_text (GtkEntry    *entry,
                                const gchar *text)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (g_strcmp0 (priv->placeholder_text, text) == 0)
    return;

  g_free (priv->placeholder_text);
  priv->placeholder_text = g_strdup (text);

  gtk_entry_recompute (entry);

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_PLACEHOLDER_TEXT]);
}

/**
 * gtk_entry_get_placeholder_text:
 * @entry: a #GtkEntry
 *
 * Retrieves the text that will be displayed when @entry is empty and unfocused
 *
 * Returns: a pointer to the placeholder text as a string. This string points to internally allocated
 * storage in the widget and must not be freed, modified or stored.
 **/
const gchar *
gtk_entry_get_placeholder_text (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return priv->placeholder_text;
}

/* Caps Lock warning for password entries */

static void
show_capslock_feedback (GtkEntry    *entry,
                        const gchar *text)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_SECONDARY) == GTK_IMAGE_EMPTY)
    {
      gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, "dialog-warning-symbolic");
      gtk_entry_set_icon_activatable (entry, GTK_ENTRY_ICON_SECONDARY, FALSE);
      priv->caps_lock_warning_shown = TRUE;
    }

  if (priv->caps_lock_warning_shown)
    gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY, text);
  else
    g_warning ("Can't show Caps Lock warning, since secondary icon is set");
}

static void
remove_capslock_feedback (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->caps_lock_warning_shown)
    {
      gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
      priv->caps_lock_warning_shown = FALSE;
    }
}

static void
keymap_state_changed (GdkKeymap *keymap, 
                      GtkEntry  *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  char *text = NULL;

  if (priv->editable &&
      gtk_entry_get_display_mode (entry) != DISPLAY_NORMAL &&
      priv->caps_lock_warning)
    {
      if (gdk_keymap_get_caps_lock_state (keymap))
        text = _("Caps Lock is on");
    }

  if (text)
    show_capslock_feedback (entry, text);
  else
    remove_capslock_feedback (entry);
}

/**
 * gtk_entry_set_input_purpose:
 * @entry: a #GtkEntry
 * @purpose: the purpose
 *
 * Sets the #GtkEntry:input-purpose property which
 * can be used by on-screen keyboards and other input
 * methods to adjust their behaviour.
 */
void
gtk_entry_set_input_purpose (GtkEntry        *entry,
                             GtkInputPurpose  purpose)

{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (gtk_entry_get_input_purpose (entry) != purpose)
    {
      g_object_set (G_OBJECT (priv->im_context),
                    "input-purpose", purpose,
                    NULL);

      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_INPUT_PURPOSE]);
    }
}

/**
 * gtk_entry_get_input_purpose:
 * @entry: a #GtkEntry
 *
 * Gets the value of the #GtkEntry:input-purpose property.
 */
GtkInputPurpose
gtk_entry_get_input_purpose (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  GtkInputPurpose purpose;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), GTK_INPUT_PURPOSE_FREE_FORM);

  g_object_get (G_OBJECT (priv->im_context),
                "input-purpose", &purpose,
                NULL);

  return purpose;
}

/**
 * gtk_entry_set_input_hints:
 * @entry: a #GtkEntry
 * @hints: the hints
 *
 * Sets the #GtkEntry:input-hints property, which
 * allows input methods to fine-tune their behaviour.
 */
void
gtk_entry_set_input_hints (GtkEntry      *entry,
                           GtkInputHints  hints)

{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (gtk_entry_get_input_hints (entry) != hints)
    {
      g_object_set (G_OBJECT (priv->im_context),
                    "input-hints", hints,
                    NULL);

      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_INPUT_HINTS]);
    }
}

/**
 * gtk_entry_get_input_hints:
 * @entry: a #GtkEntry
 *
 * Gets the value of the #GtkEntry:input-hints property.
 */
GtkInputHints
gtk_entry_get_input_hints (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  GtkInputHints hints;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), GTK_INPUT_HINT_NONE);

  g_object_get (G_OBJECT (priv->im_context),
                "input-hints", &hints,
                NULL);

  return hints;
}

/**
 * gtk_entry_set_attributes:
 * @entry: a #GtkEntry
 * @attrs: a #PangoAttrList
 *
 * Sets a #PangoAttrList; the attributes in the list are applied to the
 * entry text.
 */
void
gtk_entry_set_attributes (GtkEntry      *entry,
                          PangoAttrList *attrs)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (attrs)
    pango_attr_list_ref (attrs);

  if (priv->attrs)
    pango_attr_list_unref (priv->attrs);
  priv->attrs = attrs;

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_ATTRIBUTES]);

  gtk_entry_recompute (entry);
  gtk_widget_queue_resize (GTK_WIDGET (entry));
}

/**
 * gtk_entry_get_attributes:
 * @entry: a #GtkEntry
 *
 * Gets the attribute list that was set on the entry using
 * gtk_entry_set_attributes(), if any.
 *
 * Returns: (transfer none) (nullable): the attribute list, or %NULL
 *     if none was set.
 */
PangoAttrList *
gtk_entry_get_attributes (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return priv->attrs;
}

/**
 * gtk_entry_set_tabs:
 * @entry: a #GtkEntry
 * @tabs: (nullable): a #PangoTabArray
 *
 * Sets a #PangoTabArray; the tabstops in the array are applied to the entry
 * text.
 */

void
gtk_entry_set_tabs (GtkEntry      *entry,
                    PangoTabArray *tabs)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (priv->tabs)
    pango_tab_array_free(priv->tabs);

  if (tabs)
    priv->tabs = pango_tab_array_copy (tabs);
  else
    priv->tabs = NULL;

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_TABS]);

  gtk_entry_recompute (entry);
  gtk_widget_queue_resize (GTK_WIDGET (entry));
}

/**
 * gtk_entry_get_tabs:
 * @entry: a #GtkEntry
 *
 * Gets the tabstops that were set on the entry using gtk_entry_set_tabs(), if
 * any.
 *
 * Returns: (nullable) (transfer none): the tabstops, or %NULL if none was set.
 */

PangoTabArray *
gtk_entry_get_tabs (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return priv->tabs;
}

static void
gtk_entry_insert_emoji (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkWidget *chooser;
  GdkRectangle rect;

  if (gtk_widget_get_ancestor (GTK_WIDGET (entry), GTK_TYPE_EMOJI_CHOOSER) != NULL)
    return;

  chooser = GTK_WIDGET (g_object_get_data (G_OBJECT (entry), "gtk-emoji-chooser"));
  if (!chooser)
    {
      chooser = gtk_emoji_chooser_new ();
      g_object_set_data (G_OBJECT (entry), "gtk-emoji-chooser", chooser);

      gtk_popover_set_relative_to (GTK_POPOVER (chooser), GTK_WIDGET (entry));
      if (priv->show_emoji_icon)
        {
          gtk_entry_get_icon_area (entry, GTK_ENTRY_ICON_SECONDARY, &rect);
          gtk_popover_set_pointing_to (GTK_POPOVER (chooser), &rect);
        }
      g_signal_connect_swapped (chooser, "emoji-picked", G_CALLBACK (gtk_entry_enter_text), entry);
    }

  gtk_popover_popup (GTK_POPOVER (chooser));
}

static void
pick_emoji (GtkEntry *entry,
            int       icon,
            GdkEvent *event,
            gpointer  data)
{
  if (icon == GTK_ENTRY_ICON_SECONDARY)
    gtk_entry_insert_emoji (entry);
}

static void
set_show_emoji_icon (GtkEntry *entry,
                     gboolean  value)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->show_emoji_icon == value)
    return;

  priv->show_emoji_icon = value;

  if (priv->show_emoji_icon)
    {
      gtk_entry_set_icon_from_icon_name (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         "face-smile-symbolic");

      gtk_entry_set_icon_sensitive (entry,
                                    GTK_ENTRY_ICON_SECONDARY,
                                    TRUE);

      gtk_entry_set_icon_activatable (entry,
                                      GTK_ENTRY_ICON_SECONDARY,
                                      TRUE);

      gtk_entry_set_icon_tooltip_text (entry,
                                       GTK_ENTRY_ICON_SECONDARY,
                                       _("Insert Emoji"));

      g_signal_connect (entry, "icon-press", G_CALLBACK (pick_emoji), NULL);
    }
  else
    {
      g_signal_handlers_disconnect_by_func (entry, pick_emoji, NULL);

      gtk_entry_set_icon_from_icon_name (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         NULL);

      gtk_entry_set_icon_tooltip_text (entry,
                                       GTK_ENTRY_ICON_SECONDARY,
                                       NULL);
    }

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_SHOW_EMOJI_ICON]);
  gtk_widget_queue_resize (GTK_WIDGET (entry));
}
