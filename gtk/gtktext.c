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

#include "config.h"

#include "gtktextprivate.h"

#include "gtkadjustment.h"
#include "gtkbindings.h"
#include "gtkbox.h"
#include "gtkbutton.h"
#include "gtkcssnodeprivate.h"
#include "gtkdebug.h"
#include "gtkdnd.h"
#include "gtkdndprivate.h"
#include "gtkeditableprivate.h"
#include "gtkemojichooser.h"
#include "gtkemojicompletion.h"
#include "gtkentrybuffer.h"
#include "gtkeventcontrollerkey.h"
#include "gtkeventcontrollermotion.h"
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

#include "a11y/gtktextaccessible.h"

#include <cairo-gobject.h>
#include <string.h>

#include "fallback-c89.c"

/**
 * SECTION:gtktext
 * @Short_description: A simple single-line text entry field
 * @Title: GtkText
 * @See_also: #GtkEntry, #GtkTextView
 *
 * The #GtkText widget is a single line text entry widget.
 *
 * A fairly large set of key bindings are supported by default. If the
 * entered text is longer than the allocation of the widget, the widget
 * will scroll so that the cursor position is visible.
 *
 * When using an entry for passwords and other sensitive information,
 * it can be put into “password mode” using gtk_text_set_visibility().
 * In this mode, entered text is displayed using a “invisible” character.
 * By default, GTK picks the best invisible character that is available
 * in the current font, but it can be changed with gtk_text_set_invisible_char().
 *
 * If you are looking to add icons or progress display in an entry, look
 * at #GtkEntry. There other alternatives for more specialized use cases,
 * such as #GtkSearchEntry.
 *
 * If you need multi-line editable text, look at #GtkTextView.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * entry[.read-only][.flat][.warning][.error]
 * ├── placeholder
 * ├── undershoot.left
 * ├── undershoot.right
 * ├── [selection]
 * ├── [block-cursor]
 * ╰── [window.popup]
 * ]|
 *
 * GtkText has a main node with the name entry. Depending on the properties
 * of the entry, the style classes .read-only and .flat may appear. The style
 * classes .warning and .error may also be used with entries.
 *
 * When the entry has a selection, it adds a subnode with the name selection.
 *
 * When the entry is in overwrite mode, it adds a subnode with the name block-cursor
 * that determines how the block cursor is drawn.
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
 * just a single handle for the text cursor, it gets the style class .insertion-cursor.
 */

#define NAT_ENTRY_WIDTH  150

#define UNDERSHOOT_SIZE 20

static GQuark          quark_password_hint  = 0;
static GQuark          quark_gtk_signal = 0;

typedef struct _GtkTextPasswordHint GtkTextPasswordHint;

typedef struct _GtkTextPrivate GtkTextPrivate;
struct _GtkTextPrivate
{
  GtkEntryBuffer *buffer;
  GtkIMContext   *im_context;
  GtkWidget      *popup_menu;

  int             text_baseline;

  PangoLayout    *cached_layout;
  PangoAttrList  *attrs;
  PangoTabArray  *tabs;

  GdkContentProvider *selection_content;

  char         *im_module;

  GtkTextHandle *text_handle;
  GtkWidget     *selection_bubble;
  guint          selection_bubble_timeout_id;

  GtkWidget     *magnifier_popover;
  GtkWidget     *magnifier;

  GtkWidget     *placeholder;

  GtkGesture    *drag_gesture;
  GtkEventController *key_controller;

  GtkCssNode    *selection_node;
  GtkCssNode    *block_cursor_node;
  GtkCssNode    *undershoot_node[2];

  int           text_x;
  int           text_width;

  float         xalign;

  int           ascent;                     /* font ascent in pango units  */
  int           current_pos;
  int           descent;                    /* font descent in pango units */
  int           dnd_position;               /* In chars, -1 == no DND cursor */
  int           drag_start_x;
  int           drag_start_y;
  int           drop_position;              /* where the drop should happen */
  int           insert_pos;
  int           selection_bound;
  int           scroll_offset;
  int           start_x;
  int           start_y;
  int           width_chars;
  int           max_width_chars;

  gunichar      invisible_char;

  guint         blink_time;                  /* time in msec the cursor has blinked since last user event */
  guint         blink_timeout;

  guint16       preedit_length;              /* length of preedit string, in bytes */
  guint16       preedit_cursor;              /* offset of cursor within preedit string, in chars */

  gint64        handle_place_time;

  guint         editable                : 1;
  guint         enable_emoji_completion : 1;
  guint         in_drag                 : 1;
  guint         overwrite_mode          : 1;
  guint         visible                 : 1;

  guint         activates_default       : 1;
  guint         cache_includes_preedit  : 1;
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

struct _GtkTextPasswordHint
{
  int position;      /* Position (in text) of the last password hint */
  guint source_id;    /* Timeout source id */
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
  PREEDIT_CHANGED,
  INSERT_EMOJI,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_MAX_LENGTH,
  PROP_HAS_FRAME,
  PROP_VISIBILITY,
  PROP_INVISIBLE_CHAR,
  PROP_INVISIBLE_CHAR_SET,
  PROP_ACTIVATES_DEFAULT,
  PROP_SCROLL_OFFSET,
  PROP_TRUNCATE_MULTILINE,
  PROP_OVERWRITE_MODE,
  PROP_IM_MODULE,
  PROP_PLACEHOLDER_TEXT,
  PROP_INPUT_PURPOSE,
  PROP_INPUT_HINTS,
  PROP_ATTRIBUTES,
  PROP_POPULATE_ALL,
  PROP_TABS,
  PROP_ENABLE_EMOJI_COMPLETION,
  NUM_PROPERTIES
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
static void   gtk_text_set_property         (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);
static void   gtk_text_get_property         (GObject      *object,
                                             guint         prop_id,
                                             GValue       *value,
                                             GParamSpec   *pspec);
static void   gtk_text_finalize             (GObject      *object);
static void   gtk_text_dispose              (GObject      *object);

/* GtkWidget methods
 */
static void   gtk_text_destroy              (GtkWidget        *widget);
static void   gtk_text_realize              (GtkWidget        *widget);
static void   gtk_text_unrealize            (GtkWidget        *widget);
static void   gtk_text_unmap                (GtkWidget        *widget);
static void   gtk_text_measure              (GtkWidget        *widget,
                                             GtkOrientation    orientation,
                                             int               for_size,
                                             int              *minimum,
                                             int              *natural,
                                             int              *minimum_baseline,
                                             int              *natural_baseline);
static void   gtk_text_size_allocate        (GtkWidget        *widget,
                                             int               width,
                                             int               height,
                                             int               baseline);
static void   gtk_text_snapshot             (GtkWidget        *widget,
                                            GtkSnapshot      *snapshot);
static void   gtk_text_focus_in             (GtkWidget        *widget);
static void   gtk_text_focus_out            (GtkWidget        *widget);
static void   gtk_text_grab_focus           (GtkWidget        *widget);
static void   gtk_text_style_updated        (GtkWidget        *widget);
static void   gtk_text_direction_changed    (GtkWidget        *widget,
                                             GtkTextDirection  previous_dir);
static void   gtk_text_state_flags_changed  (GtkWidget        *widget,
                                             GtkStateFlags     previous_state);
static void   gtk_text_display_changed      (GtkWidget        *widget,
                                             GdkDisplay       *old_display);

static gboolean gtk_text_drag_drop          (GtkWidget        *widget,
                                             GdkDrop          *drop,
                                             int               x,
                                             int               y);
static gboolean gtk_text_drag_motion        (GtkWidget        *widget,
                                             GdkDrop          *drop,
                                             int               x,
                                             int               y);
static void     gtk_text_drag_leave         (GtkWidget        *widget,
                                             GdkDrop          *drop);
static void     gtk_text_drag_data_received (GtkWidget        *widget,
                                             GdkDrop          *drop,
                                             GtkSelectionData *selection_data);
static void     gtk_text_drag_data_get      (GtkWidget        *widget,
                                             GdkDrag          *drag,
                                             GtkSelectionData *selection_data);
static void     gtk_text_drag_data_delete   (GtkWidget        *widget,
                                             GdkDrag          *drag);
static void     gtk_text_drag_begin         (GtkWidget        *widget,
                                             GdkDrag          *drag);
static void     gtk_text_drag_end           (GtkWidget        *widget,
                                             GdkDrag          *drag);


/* GtkEditable method implementations
 */
static void        gtk_text_editable_init        (GtkEditableInterface *iface);
static void        gtk_text_insert_text          (GtkEditable          *editable,
                                                  const char           *text,
                                                  int                   length,
                                                  int                  *position);
static void        gtk_text_delete_text          (GtkEditable          *editable,
                                                  int                   start_pos,
                                                  int                   end_pos);
static const char *gtk_text_get_text             (GtkEditable          *editable);
static void        gtk_text_real_set_position    (GtkEditable          *editable,
                                                  int                   position);
static int         gtk_text_get_position         (GtkEditable          *editable);
static void        gtk_text_set_selection_bounds (GtkEditable          *editable,
                                                  int                   start,
                                                  int                   end);
static gboolean    gtk_text_get_selection_bounds (GtkEditable          *editable,
                                                  int                  *start,
                                                  int                  *end);

static void        gtk_text_set_editable         (GtkText              *text,
                                                  gboolean              is_editable);
static void        gtk_text_set_text             (GtkText              *entry,
                                                  const char           *text);
static void        gtk_text_set_width_chars      (GtkText              *text,
                                                  int                   width_chars); 
static void        gtk_text_set_max_width_chars  (GtkText              *text,
                                                  int                   max_width_chars); 
static void        gtk_text_set_alignment        (GtkText              *text,
                                                  float                 xalign);

/* Default signal handlers
 */
static gboolean gtk_text_popup_menu         (GtkWidget       *widget);

static void     gtk_text_real_insert_text   (GtkEditable     *editable,
                                             const char      *text,
                                             int              length,
                                             int             *position);
static void     gtk_text_real_delete_text   (GtkEditable     *editable,
                                             int              start_pos,
                                             int              end_pos);

static void     gtk_text_move_cursor        (GtkText         *entry,
                                             GtkMovementStep  step,
                                             int              count,
                                             gboolean         extend_selection);
static void     gtk_text_insert_at_cursor   (GtkText         *entry,
                                             const char      *str);
static void     gtk_text_delete_from_cursor (GtkText         *entry,
                                             GtkDeleteType    type,
                                             int              count);
static void     gtk_text_backspace          (GtkText        *entry);
static void     gtk_text_cut_clipboard      (GtkText        *entry);
static void     gtk_text_copy_clipboard     (GtkText        *entry);
static void     gtk_text_paste_clipboard    (GtkText        *entry);
static void     gtk_text_toggle_overwrite   (GtkText        *entry);
static void     gtk_text_insert_emoji       (GtkText        *entry);
static void     gtk_text_select_all         (GtkText        *entry);
static void     gtk_text_real_activate      (GtkText        *entry);

static void     keymap_direction_changed     (GdkKeymap       *keymap,
                                              GtkText        *entry);

/* IM Context Callbacks
 */
static void     gtk_text_commit_cb               (GtkIMContext *context,
                                                  const char   *str,
                                                  GtkText      *entry);
static void     gtk_text_preedit_changed_cb      (GtkIMContext *context,
                                                  GtkText      *entry);
static gboolean gtk_text_retrieve_surrounding_cb (GtkIMContext *context,
                                                  GtkText      *entry);
static gboolean gtk_text_delete_surrounding_cb   (GtkIMContext *context,
                                                  int           offset,
                                                  int           n_chars,
                                                  GtkText      *entry);

/* Entry buffer signal handlers
 */
static void buffer_inserted_text     (GtkEntryBuffer *buffer, 
                                      guint           position,
                                      const char     *chars,
                                      guint           n_chars,
                                      GtkText       *entry);
static void buffer_deleted_text      (GtkEntryBuffer *buffer, 
                                      guint           position,
                                      guint           n_chars,
                                      GtkText       *entry);
static void buffer_notify_text       (GtkEntryBuffer *buffer, 
                                      GParamSpec     *spec,
                                      GtkText       *entry);
static void buffer_notify_max_length (GtkEntryBuffer *buffer, 
                                      GParamSpec     *spec,
                                      GtkText       *entry);

/* Event controller callbacks
 */
static void     gtk_text_motion_controller_motion   (GtkEventControllerMotion *event_controller,
                                                     double                    x,
                                                     double                    y,
                                                     gpointer                  user_data);
static void     gtk_text_multipress_gesture_pressed (GtkGestureMultiPress     *gesture,
                                                     int                       n_press,
                                                     double                    x,
                                                     double                    y,
                                                     GtkText                  *entry);
static void     gtk_text_drag_gesture_update        (GtkGestureDrag           *gesture,
                                                     double                    offset_x,
                                                     double                    offset_y,
                                                     GtkText                  *entry);
static void     gtk_text_drag_gesture_end           (GtkGestureDrag           *gesture,
                                                     double                     offset_x,
                                                     double                    offset_y,
                                                     GtkText                  *entry);
static gboolean gtk_text_key_controller_key_pressed  (GtkEventControllerKey   *controller,
                                                      guint                    keyval,
                                                      guint                    keycode,
                                                      GdkModifierType          state,
                                                      GtkWidget               *widget);


/* GtkTextHandle handlers */
static void gtk_text_handle_drag_started  (GtkTextHandle          *handle,
                                           GtkTextHandlePosition   pos,
                                           GtkText                *entry);
static void gtk_text_handle_dragged       (GtkTextHandle          *handle,
                                           GtkTextHandlePosition   pos,
                                           int                     x,
                                           int                     y,
                                           GtkText                *entry);
static void gtk_text_handle_drag_finished (GtkTextHandle          *handle,
                                           GtkTextHandlePosition   pos,
                                           GtkText                *entry);

/* Internal routines
 */
static void         gtk_text_draw_text                (GtkText       *entry,
                                                       GtkSnapshot   *snapshot);
static void         gtk_text_draw_cursor              (GtkText       *entry,
                                                       GtkSnapshot   *snapshot,
                                                       CursorType     type);
static PangoLayout *gtk_text_ensure_layout            (GtkText       *entry,
                                                       gboolean       include_preedit);
static void         gtk_text_reset_layout             (GtkText       *entry);
static void         gtk_text_recompute                (GtkText       *entry);
static int          gtk_text_find_position            (GtkText       *entry,
                                                       int            x);
static void         gtk_text_get_cursor_locations     (GtkText       *entry,
                                                       int           *strong_x,
                                                       int           *weak_x);
static void         gtk_text_adjust_scroll            (GtkText       *entry);
static int          gtk_text_move_visually            (GtkText        *editable,
                                                       int             start,
                                                       int             count);
static int          gtk_text_move_logically           (GtkText        *entry,
                                                       int             start,
                                                       int             count);
static int          gtk_text_move_forward_word        (GtkText        *entry,
                                                       int             start,
                                                       gboolean        allow_whitespace);
static int          gtk_text_move_backward_word       (GtkText        *entry,
                                                       int             start,
                                                       gboolean        allow_whitespace);
static void         gtk_text_delete_whitespace        (GtkText        *entry);
static void         gtk_text_select_word              (GtkText        *entry);
static void         gtk_text_select_line              (GtkText        *entry);
static void         gtk_text_paste                    (GtkText        *entry,
                                                       GdkClipboard   *clipboard);
static void         gtk_text_update_primary_selection (GtkText        *entry);
static void         gtk_text_schedule_im_reset        (GtkText        *entry);
static void         gtk_text_do_popup                 (GtkText        *entry,
                                                       const GdkEvent *event);
static gboolean     gtk_text_mnemonic_activate        (GtkWidget      *widget,
                                                       gboolean        group_cycling);
static void         gtk_text_check_cursor_blink       (GtkText        *entry);
static void         gtk_text_pend_cursor_blink        (GtkText        *entry);
static void         gtk_text_reset_blink_time         (GtkText        *entry);
static void         gtk_text_update_cached_style_values(GtkText       *entry);
static gboolean     get_middle_click_paste             (GtkText       *entry);
static void         gtk_text_get_scroll_limits        (GtkText        *entry,
                                                       int            *min_offset,
                                                       int            *max_offset);
static GtkEntryBuffer *get_buffer                      (GtkText       *entry);
static void         set_enable_emoji_completion        (GtkText       *entry,
                                                        gboolean       value);
static void         set_text_cursor                    (GtkWidget     *widget);

static void         buffer_connect_signals             (GtkText       *entry);
static void         buffer_disconnect_signals          (GtkText       *entry);

static void         gtk_text_selection_bubble_popup_set   (GtkText *entry);
static void         gtk_text_selection_bubble_popup_unset (GtkText *entry);

static void         begin_change                       (GtkText *entry);
static void         end_change                         (GtkText *entry);
static void         emit_changed                       (GtkText *entry);


/* GtkTextContent implementation
 */
#define GTK_TYPE_TEXT_CONTENT            (gtk_text_content_get_type ())
#define GTK_TEXT_CONTENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TEXT_CONTENT, GtkTextContent))
#define GTK_IS_TEXT_CONTENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TEXT_CONTENT))
#define GTK_TEXT_CONTENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_CONTENT, GtkTextContentClass))
#define GTK_IS_TEXT_CONTENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_CONTENT))
#define GTK_TEXT_CONTENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TEXT_CONTENT, GtkTextContentClass))

typedef struct _GtkTextContent GtkTextContent;
typedef struct _GtkTextContentClass GtkTextContentClass;

struct _GtkTextContent
{
  GdkContentProvider parent;

  GtkText *entry;
};

struct _GtkTextContentClass
{
  GdkContentProviderClass parent_class;
};

GType gtk_text_content_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GtkTextContent, gtk_text_content, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
gtk_text_content_ref_formats (GdkContentProvider *provider)
{
  return gdk_content_formats_new_for_gtype (G_TYPE_STRING);
}

static gboolean
gtk_text_content_get_value (GdkContentProvider  *provider,
                            GValue              *value,
                            GError             **error)
{
  GtkTextContent *content = GTK_TEXT_CONTENT (provider);

  if (G_VALUE_HOLDS (value, G_TYPE_STRING))
    {
      int start, end;

      if (gtk_text_get_selection_bounds (GTK_EDITABLE (content->entry), &start, &end))
        {
          char *str = gtk_text_get_display_text (content->entry, start, end);
          g_value_take_string (value, str);
        }
      return TRUE;
    }

  return GDK_CONTENT_PROVIDER_CLASS (gtk_text_content_parent_class)->get_value (provider, value, error);
}

static void
gtk_text_content_detach (GdkContentProvider *provider,
                          GdkClipboard       *clipboard)
{
  GtkTextContent *content = GTK_TEXT_CONTENT (provider);
  int current_pos, selection_bound;

  gtk_text_get_selection_bounds (GTK_EDITABLE (content->entry), &current_pos, &selection_bound);
  gtk_text_set_selection_bounds (GTK_EDITABLE (content->entry), current_pos, current_pos);
}

static void
gtk_text_content_class_init (GtkTextContentClass *class)
{
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);

  provider_class->ref_formats = gtk_text_content_ref_formats;
  provider_class->get_value = gtk_text_content_get_value;
  provider_class->detach_clipboard = gtk_text_content_detach;
}

static void
gtk_text_content_init (GtkTextContent *content)
{
}

/* GtkText
 */

G_DEFINE_TYPE_WITH_CODE (GtkText, gtk_text, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkText)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE, gtk_text_editable_init))

static void
add_move_binding (GtkBindingSet  *binding_set,
                  guint           keyval,
                  guint           modmask,
                  GtkMovementStep step,
                  int             count)
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
gtk_text_class_init (GtkTextClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;

  widget_class = (GtkWidgetClass*) class;

  gobject_class->dispose = gtk_text_dispose;
  gobject_class->finalize = gtk_text_finalize;
  gobject_class->set_property = gtk_text_set_property;
  gobject_class->get_property = gtk_text_get_property;

  widget_class->destroy = gtk_text_destroy;
  widget_class->unmap = gtk_text_unmap;
  widget_class->realize = gtk_text_realize;
  widget_class->unrealize = gtk_text_unrealize;
  widget_class->measure = gtk_text_measure;
  widget_class->size_allocate = gtk_text_size_allocate;
  widget_class->snapshot = gtk_text_snapshot;
  widget_class->grab_focus = gtk_text_grab_focus;
  widget_class->style_updated = gtk_text_style_updated;
  widget_class->drag_begin = gtk_text_drag_begin;
  widget_class->drag_end = gtk_text_drag_end;
  widget_class->direction_changed = gtk_text_direction_changed;
  widget_class->state_flags_changed = gtk_text_state_flags_changed;
  widget_class->display_changed = gtk_text_display_changed;
  widget_class->mnemonic_activate = gtk_text_mnemonic_activate;
  widget_class->popup_menu = gtk_text_popup_menu;
  widget_class->drag_drop = gtk_text_drag_drop;
  widget_class->drag_motion = gtk_text_drag_motion;
  widget_class->drag_leave = gtk_text_drag_leave;
  widget_class->drag_data_received = gtk_text_drag_data_received;
  widget_class->drag_data_get = gtk_text_drag_data_get;
  widget_class->drag_data_delete = gtk_text_drag_data_delete;

  class->move_cursor = gtk_text_move_cursor;
  class->insert_at_cursor = gtk_text_insert_at_cursor;
  class->delete_from_cursor = gtk_text_delete_from_cursor;
  class->backspace = gtk_text_backspace;
  class->cut_clipboard = gtk_text_cut_clipboard;
  class->copy_clipboard = gtk_text_copy_clipboard;
  class->paste_clipboard = gtk_text_paste_clipboard;
  class->toggle_overwrite = gtk_text_toggle_overwrite;
  class->insert_emoji = gtk_text_insert_emoji;
  class->activate = gtk_text_real_activate;
  
  quark_password_hint = g_quark_from_static_string ("gtk-entry-password-hint");
  quark_gtk_signal = g_quark_from_static_string ("gtk-signal");

  entry_props[PROP_BUFFER] =
      g_param_spec_object ("buffer",
                           P_("Text Buffer"),
                           P_("Text buffer object which actually stores entry text"),
                           GTK_TYPE_ENTRY_BUFFER,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_MAX_LENGTH] =
      g_param_spec_int ("max-length",
                        P_("Maximum length"),
                        P_("Maximum number of characters for this entry. Zero if no maximum"),
                        0, GTK_ENTRY_BUFFER_MAX_SIZE,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_HAS_FRAME] =
      g_param_spec_boolean ("has-frame",
                            P_("Has Frame"),
                            P_("FALSE removes outside bevel from entry"),
                            FALSE,
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

  entry_props[PROP_SCROLL_OFFSET] =
      g_param_spec_int ("scroll-offset",
                        P_("Scroll offset"),
                        P_("Number of pixels of the entry scrolled off the screen to the left"),
                        0, G_MAXINT,
                        0,
                        GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:truncate-multiline:
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
   * GtkText:overwrite-mode:
   *
   * If text is overwritten when typing in the #GtkText.
   */
  entry_props[PROP_OVERWRITE_MODE] =
      g_param_spec_boolean ("overwrite-mode",
                            P_("Overwrite mode"),
                            P_("Whether new text overwrites existing text"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:invisible-char-set:
   *
   * Whether the invisible char has been set for the #GtkText.
   */
  entry_props[PROP_INVISIBLE_CHAR_SET] =
      g_param_spec_boolean ("invisible-char-set",
                            P_("Invisible character set"),
                            P_("Whether the invisible character has been set"),
                            FALSE,
                            GTK_PARAM_READWRITE);

  /**
  * GtkText:placeholder-text:
  *
  * The text that will be displayed in the #GtkText when it is empty
  * and unfocused.
  */
  entry_props[PROP_PLACEHOLDER_TEXT] =
      g_param_spec_string ("placeholder-text",
                           P_("Placeholder text"),
                           P_("Show text in the entry when it’s empty and unfocused"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:im-module:
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
   * GtkText:input-purpose:
   *
   * The purpose of this text field.
   *
   * This property can be used by on-screen keyboards and other input
   * methods to adjust their behaviour.
   *
   * Note that setting the purpose to %GTK_INPUT_PURPOSE_PASSWORD or
   * %GTK_INPUT_PURPOSE_PIN is independent from setting
   * #GtkText:visibility.
   */
  entry_props[PROP_INPUT_PURPOSE] =
      g_param_spec_enum ("input-purpose",
                         P_("Purpose"),
                         P_("Purpose of the text field"),
                         GTK_TYPE_INPUT_PURPOSE,
                         GTK_INPUT_PURPOSE_FREE_FORM,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:input-hints:
   *
   * Additional hints (beyond #GtkText:input-purpose) that
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
   * GtkText:attributes:
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
   * GtkText:populate-all:
   *
   * If :populate-all is %TRUE, the #GtkText::populate-popup
   * signal is also emitted for touch popups.
   */
  entry_props[PROP_POPULATE_ALL] =
      g_param_spec_boolean ("populate-all",
                            P_("Populate all"),
                            P_("Whether to emit ::populate-popup for touch popups"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText::tabs:
   *
   * A list of tabstops to apply to the text of the entry.
   */
  entry_props[PROP_TABS] =
      g_param_spec_boxed ("tabs",
                          P_("Tabs"),
                          P_("A list of tabstop locations to apply to the text of the entry"),
                          PANGO_TYPE_TAB_ARRAY,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_ENABLE_EMOJI_COMPLETION] =
      g_param_spec_boolean ("enable-emoji-completion",
                            P_("Enable Emoji completion"),
                            P_("Whether to suggest Emoji replacements"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_VISIBILITY] =
      g_param_spec_boolean ("visibility",
                            P_("Visibility"),
                            P_("FALSE displays the “invisible char” instead of the actual text (password mode)"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, entry_props);

  gtk_editable_install_properties (gobject_class);

  /**
   * GtkText::populate-popup:
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
   * If #GtkText:populate-all is %TRUE, this signal will
   * also be emitted to populate touch popups. In this case,
   * @widget will be a different container, e.g. a #GtkToolbar.
   * The signal handler should not make assumptions about the
   * type of @widget.
   */
  signals[POPULATE_POPUP] =
    g_signal_new (I_("populate-popup"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextClass, populate_popup),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_WIDGET);
  
 /* Action signals */
  
  /**
   * GtkText::activate:
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
                  G_STRUCT_OFFSET (GtkTextClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkText::move-cursor:
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
                  G_STRUCT_OFFSET (GtkTextClass, move_cursor),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM_INT_BOOLEAN,
                  G_TYPE_NONE, 3,
                  GTK_TYPE_MOVEMENT_STEP,
                  G_TYPE_INT,
                  G_TYPE_BOOLEAN);

  /**
   * GtkText::insert-at-cursor:
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
                  G_STRUCT_OFFSET (GtkTextClass, insert_at_cursor),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  /**
   * GtkText::delete-from-cursor:
   * @entry: the object which received the signal
   * @type: the granularity of the deletion, as a #GtkDeleteType
   * @count: the number of @type units to delete
   *
   * The ::delete-from-cursor signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user initiates a text deletion.
   *
   * If the @type is %GTK_DELETE_CHARS, GTK deletes the selection
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
                  G_STRUCT_OFFSET (GtkTextClass, delete_from_cursor),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM_INT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_DELETE_TYPE,
                  G_TYPE_INT);

  /**
   * GtkText::backspace:
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
                  G_STRUCT_OFFSET (GtkTextClass, backspace),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkText::cut-clipboard:
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
                  G_STRUCT_OFFSET (GtkTextClass, cut_clipboard),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkText::copy-clipboard:
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
                  G_STRUCT_OFFSET (GtkTextClass, copy_clipboard),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkText::paste-clipboard:
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
                  G_STRUCT_OFFSET (GtkTextClass, paste_clipboard),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkText::toggle-overwrite:
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
                  G_STRUCT_OFFSET (GtkTextClass, toggle_overwrite),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkText::preedit-changed:
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
   * GtkText::insert-emoji:
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
                  G_STRUCT_OFFSET (GtkTextClass, insert_emoji),
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

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_TEXT_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("text"));
}

static void
gtk_text_editable_init (GtkEditableInterface *iface)
{
  iface->do_insert_text = gtk_text_insert_text;
  iface->do_delete_text = gtk_text_delete_text;
  iface->insert_text = gtk_text_real_insert_text;
  iface->delete_text = gtk_text_real_delete_text;
  iface->get_text = gtk_text_get_text;
  iface->set_selection_bounds = gtk_text_set_selection_bounds;
  iface->get_selection_bounds = gtk_text_get_selection_bounds;
  iface->set_position = gtk_text_real_set_position;
  iface->get_position = gtk_text_get_position;
}

static void
gtk_text_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtkText *entry = GTK_TEXT (object);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  switch (prop_id)
    {
    /* GtkEditable properties */
    case GTK_EDITABLE_PROP_EDITABLE:
      gtk_text_set_editable (entry, g_value_get_boolean (value));
      break;

    case GTK_EDITABLE_PROP_WIDTH_CHARS:
      gtk_text_set_width_chars (entry, g_value_get_int (value));
      break;

    case GTK_EDITABLE_PROP_MAX_WIDTH_CHARS:
      gtk_text_set_max_width_chars (entry, g_value_get_int (value));
      break;

    case GTK_EDITABLE_PROP_TEXT:
      gtk_text_set_text (entry, g_value_get_string (value));
      break;

    case GTK_EDITABLE_PROP_XALIGN:
      gtk_text_set_alignment (entry, g_value_get_float (value));
      break;

    /* GtkText properties */
    case PROP_BUFFER:
      gtk_text_set_buffer (entry, g_value_get_object (value));
      break;

    case PROP_MAX_LENGTH:
      gtk_text_set_max_length (entry, g_value_get_int (value));
      break;

    case PROP_VISIBILITY:
      gtk_text_set_visibility (entry, g_value_get_boolean (value));
      break;

    case PROP_HAS_FRAME:
      gtk_text_set_has_frame (entry, g_value_get_boolean (value));
      break;

    case PROP_INVISIBLE_CHAR:
      gtk_text_set_invisible_char (entry, g_value_get_uint (value));
      break;

    case PROP_ACTIVATES_DEFAULT:
      gtk_text_set_activates_default (entry, g_value_get_boolean (value));
      break;

    case PROP_TRUNCATE_MULTILINE:
      if (priv->truncate_multiline != g_value_get_boolean (value))
        {
          priv->truncate_multiline = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_OVERWRITE_MODE:
      gtk_text_set_overwrite_mode (entry, g_value_get_boolean (value));
      break;

    case PROP_INVISIBLE_CHAR_SET:
      if (g_value_get_boolean (value))
        priv->invisible_char_set = TRUE;
      else
        gtk_text_unset_invisible_char (entry);
      break;

    case PROP_PLACEHOLDER_TEXT:
      gtk_text_set_placeholder_text (entry, g_value_get_string (value));
      break;

    case PROP_IM_MODULE:
      g_free (priv->im_module);
      priv->im_module = g_value_dup_string (value);
      if (GTK_IS_IM_MULTICONTEXT (priv->im_context))
        gtk_im_multicontext_set_context_id (GTK_IM_MULTICONTEXT (priv->im_context), priv->im_module);
      g_object_notify_by_pspec (object, pspec);
      break;

    case PROP_INPUT_PURPOSE:
      gtk_text_set_input_purpose (entry, g_value_get_enum (value));
      break;

    case PROP_INPUT_HINTS:
      gtk_text_set_input_hints (entry, g_value_get_flags (value));
      break;

    case PROP_ATTRIBUTES:
      gtk_text_set_attributes (entry, g_value_get_boxed (value));
      break;

    case PROP_POPULATE_ALL:
      if (priv->populate_all != g_value_get_boolean (value))
        {
          priv->populate_all = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_TABS:
      gtk_text_set_tabs (entry, g_value_get_boxed (value));
      break;

    case PROP_ENABLE_EMOJI_COMPLETION:
      set_enable_emoji_completion (entry, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_text_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GtkText *entry = GTK_TEXT (object);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  switch (prop_id)
    {
    /* GtkEditable properties */
    case GTK_EDITABLE_PROP_CURSOR_POSITION:
      g_value_set_int (value, priv->current_pos);
      break;

    case GTK_EDITABLE_PROP_SELECTION_BOUND:
      g_value_set_int (value, priv->selection_bound);
      break;

    case GTK_EDITABLE_PROP_EDITABLE:
      g_value_set_boolean (value, priv->editable);
      break;

    case GTK_EDITABLE_PROP_WIDTH_CHARS:
      g_value_set_int (value, priv->width_chars);
      break;

    case GTK_EDITABLE_PROP_MAX_WIDTH_CHARS:
      g_value_set_int (value, priv->max_width_chars);
      break;

    case GTK_EDITABLE_PROP_TEXT:
      g_value_set_string (value, gtk_entry_buffer_get_text (get_buffer (entry)));
      break;

    case GTK_EDITABLE_PROP_XALIGN:
      g_value_set_float (value, priv->xalign);
      break;

    /* GtkText properties */
    case PROP_BUFFER:
      g_value_set_object (value, gtk_text_get_buffer (entry));
      break;

    case PROP_MAX_LENGTH:
      g_value_set_int (value, gtk_entry_buffer_get_max_length (get_buffer (entry)));
      break;

    case PROP_VISIBILITY:
      g_value_set_boolean (value, priv->visible);
      break;

    case PROP_HAS_FRAME:
      g_value_set_boolean (value, gtk_text_get_has_frame (entry));
      break;

    case PROP_INVISIBLE_CHAR:
      g_value_set_uint (value, priv->invisible_char);
      break;

    case PROP_ACTIVATES_DEFAULT:
      g_value_set_boolean (value, priv->activates_default);
      break;

    case PROP_SCROLL_OFFSET:
      g_value_set_int (value, priv->scroll_offset);
      break;

    case PROP_TRUNCATE_MULTILINE:
      g_value_set_boolean (value, priv->truncate_multiline);
      break;

    case PROP_OVERWRITE_MODE:
      g_value_set_boolean (value, priv->overwrite_mode);
      break;

    case PROP_INVISIBLE_CHAR_SET:
      g_value_set_boolean (value, priv->invisible_char_set);
      break;

    case PROP_IM_MODULE:
      g_value_set_string (value, priv->im_module);
      break;

    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, gtk_text_get_placeholder_text (entry));
      break;

    case PROP_INPUT_PURPOSE:
      g_value_set_enum (value, gtk_text_get_input_purpose (entry));
      break;

    case PROP_INPUT_HINTS:
      g_value_set_flags (value, gtk_text_get_input_hints (entry));
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

    case PROP_ENABLE_EMOJI_COMPLETION:
      g_value_set_boolean (value, priv->enable_emoji_completion);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_text_init (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkCssNode *widget_node;
  GtkGesture *gesture;
  GtkEventController *controller;
  int i;

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
  priv->insert_pos = -1;

  priv->selection_content = g_object_new (GTK_TYPE_TEXT_CONTENT, NULL);
  GTK_TEXT_CONTENT (priv->selection_content)->entry = entry;

  gtk_drag_dest_set (GTK_WIDGET (entry), 0, NULL,
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_drag_dest_add_text_targets (GTK_WIDGET (entry));

  /* This object is completely private. No external entity can gain a reference
   * to it; so we create it here and destroy it in finalize().
   */
  priv->im_context = gtk_im_multicontext_new ();

  g_signal_connect (priv->im_context, "commit",
                    G_CALLBACK (gtk_text_commit_cb), entry);
  g_signal_connect (priv->im_context, "preedit-changed",
                    G_CALLBACK (gtk_text_preedit_changed_cb), entry);
  g_signal_connect (priv->im_context, "retrieve-surrounding",
                    G_CALLBACK (gtk_text_retrieve_surrounding_cb), entry);
  g_signal_connect (priv->im_context, "delete-surrounding",
                    G_CALLBACK (gtk_text_delete_surrounding_cb), entry);

  gtk_text_update_cached_style_values (entry);

  priv->drag_gesture = gtk_gesture_drag_new ();
  g_signal_connect (priv->drag_gesture, "drag-update",
                    G_CALLBACK (gtk_text_drag_gesture_update), entry);
  g_signal_connect (priv->drag_gesture, "drag-end",
                    G_CALLBACK (gtk_text_drag_gesture_end), entry);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->drag_gesture), 0);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (priv->drag_gesture), TRUE);
  gtk_widget_add_controller (GTK_WIDGET (entry), GTK_EVENT_CONTROLLER (priv->drag_gesture));

  gesture = gtk_gesture_multi_press_new ();
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_text_multipress_gesture_pressed), entry);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
  gtk_widget_add_controller (GTK_WIDGET (entry), GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion",
                    G_CALLBACK (gtk_text_motion_controller_motion), entry);
  gtk_widget_add_controller (GTK_WIDGET (entry), controller);

  priv->key_controller = gtk_event_controller_key_new ();
  g_signal_connect (priv->key_controller, "key-pressed",
                    G_CALLBACK (gtk_text_key_controller_key_pressed), entry);
  g_signal_connect_swapped (priv->key_controller, "im-update",
                            G_CALLBACK (gtk_text_schedule_im_reset), entry);
  g_signal_connect_swapped (priv->key_controller, "focus-in",
                            G_CALLBACK (gtk_text_focus_in), entry);
  g_signal_connect_swapped (priv->key_controller, "focus-out",
                            G_CALLBACK (gtk_text_focus_out), entry);
  gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (priv->key_controller),
                                           priv->im_context);
  gtk_widget_add_controller (GTK_WIDGET (entry), priv->key_controller);

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
  gtk_text_set_has_frame (entry, FALSE);
}

static void
gtk_text_destroy (GtkWidget *widget)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  priv->current_pos = priv->selection_bound = 0;
  gtk_text_reset_im_context (entry);
  gtk_text_reset_layout (entry);

  if (priv->blink_timeout)
    {
      g_source_remove (priv->blink_timeout);
      priv->blink_timeout = 0;
    }

  if (priv->magnifier)
    _gtk_magnifier_set_inspected (GTK_MAGNIFIER (priv->magnifier), NULL);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->destroy (widget);
}

static void
gtk_text_dispose (GObject *object)
{
  GtkText *entry = GTK_TEXT (object);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GdkKeymap *keymap;

  priv->current_pos = 0;

  if (priv->buffer)
    {
      buffer_disconnect_signals (entry);
      g_object_unref (priv->buffer);
      priv->buffer = NULL;
    }

  keymap = gdk_display_get_keymap (gtk_widget_get_display (GTK_WIDGET (object)));
  g_signal_handlers_disconnect_by_func (keymap, keymap_direction_changed, entry);
  G_OBJECT_CLASS (gtk_text_parent_class)->dispose (object);
}

static void
gtk_text_finalize (GObject *object)
{
  GtkText *entry = GTK_TEXT (object);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_clear_object (&priv->selection_content);

  g_clear_object (&priv->cached_layout);
  g_clear_object (&priv->im_context);
  g_clear_pointer (&priv->selection_bubble, gtk_widget_destroy);
  g_clear_pointer (&priv->magnifier_popover, gtk_widget_destroy);
  g_clear_object (&priv->text_handle);
  g_free (priv->im_module);

  g_clear_pointer (&priv->placeholder, gtk_widget_unparent);

  if (priv->blink_timeout)
    g_source_remove (priv->blink_timeout);

  if (priv->tabs)
    pango_tab_array_free (priv->tabs);

  if (priv->attrs)
    pango_attr_list_unref (priv->attrs);


  G_OBJECT_CLASS (gtk_text_parent_class)->finalize (object);
}

static void
gtk_text_ensure_magnifier (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

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
gtk_text_ensure_text_handles (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->text_handle)
    return;

  priv->text_handle = _gtk_text_handle_new (GTK_WIDGET (entry));
  g_signal_connect (priv->text_handle, "drag-started",
                    G_CALLBACK (gtk_text_handle_drag_started), entry);
  g_signal_connect (priv->text_handle, "handle-dragged",
                    G_CALLBACK (gtk_text_handle_dragged), entry);
  g_signal_connect (priv->text_handle, "drag-finished",
                    G_CALLBACK (gtk_text_handle_drag_finished), entry);
}

static void
begin_change (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  priv->change_count++;

  g_object_freeze_notify (G_OBJECT (entry));
}

static void
end_change (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_if_fail (priv->change_count > 0);

  g_object_thaw_notify (G_OBJECT (entry));

  priv->change_count--;

  if (priv->change_count == 0)
    {
       if (priv->real_changed)
         {
           g_signal_emit_by_name (entry, "changed");
           priv->real_changed = FALSE;
         }
    }
}

static void
emit_changed (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->change_count == 0)
    g_signal_emit_by_name (entry, "changed");
  else
    priv->real_changed = TRUE;
}

static DisplayMode
gtk_text_get_display_mode (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->visible)
    return DISPLAY_NORMAL;

  if (priv->invisible_char == 0 && priv->invisible_char_set)
    return DISPLAY_BLANK;

  return DISPLAY_INVISIBLE;
}

char *
gtk_text_get_display_text (GtkText *entry,
                           int      start_pos,
                           int      end_pos)
{
  GtkTextPasswordHint *password_hint;
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  gunichar invisible_char;
  const char *start;
  const char *end;
  const char *text;
  char char_str[7];
  int char_len;
  GString *str;
  guint length;
  int i;

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
update_node_state (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
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
gtk_text_unmap (GtkWidget *widget)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->text_handle)
    _gtk_text_handle_set_mode (priv->text_handle,
                               GTK_TEXT_HANDLE_MODE_NONE);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->unmap (widget);
}

static void
gtk_text_get_text_allocation (GtkText      *entry,
                              GdkRectangle *allocation)
{
  allocation->x = 0;
  allocation->y = 0;
  allocation->width = gtk_widget_get_width (GTK_WIDGET (entry));
  allocation->height = gtk_widget_get_height (GTK_WIDGET (entry));
}

static void
gtk_text_realize (GtkWidget *widget)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->realize (widget);

  gtk_im_context_set_client_widget (priv->im_context, widget);

  gtk_text_adjust_scroll (entry);
  gtk_text_update_primary_selection (entry);
}

static void
gtk_text_unrealize (GtkWidget *widget)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GdkClipboard *clipboard;

  gtk_text_reset_layout (entry);
  
  gtk_im_context_set_client_widget (priv->im_context, NULL);

  clipboard = gtk_widget_get_primary_clipboard (widget);
  if (gdk_clipboard_get_content (clipboard) == priv->selection_content)
    gdk_clipboard_set_content (clipboard, NULL);

  if (priv->popup_menu)
    {
      gtk_widget_destroy (priv->popup_menu);
      priv->popup_menu = NULL;
    }

  GTK_WIDGET_CLASS (gtk_text_parent_class)->unrealize (widget);
}

static void
gtk_text_measure (GtkWidget      *widget,
                  GtkOrientation  orientation,
                  int             for_size,
                  int            *minimum,
                  int            *natural,
                  int            *minimum_baseline,
                  int            *natural_baseline)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  PangoContext *context;
  PangoFontMetrics *metrics;

  context = gtk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (context,
                                       pango_context_get_font_description (context),
                                       pango_context_get_language (context));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      int min, nat;
      int char_width;
      int digit_width;
      int char_pixels;

      char_width = pango_font_metrics_get_approximate_char_width (metrics);
      digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
      char_pixels = (MAX (char_width, digit_width) + PANGO_SCALE - 1) / PANGO_SCALE;

      if (priv->width_chars >= 0)
        min = char_pixels * priv->width_chars;
      else
        min = 0;

      if (priv->max_width_chars < 0)
        nat = NAT_ENTRY_WIDTH;
      else
        nat = char_pixels * priv->max_width_chars;

      nat = MAX (min, nat);

      if (priv->placeholder)
        {
          int pmin, pnat;

          gtk_widget_measure (priv->placeholder, GTK_ORIENTATION_HORIZONTAL, -1,
                              &pmin, &pnat, NULL, NULL);
          min = MAX (min, pmin);
          nat = MAX (nat, pnat);
        }

      *minimum = min;
      *natural = nat;
    }
  else
    {
      int height, baseline;
      PangoLayout *layout;

      layout = gtk_text_ensure_layout (entry, TRUE);

      priv->ascent = pango_font_metrics_get_ascent (metrics);
      priv->descent = pango_font_metrics_get_descent (metrics);

      pango_layout_get_pixel_size (layout, NULL, &height);

      height = MAX (height, PANGO_PIXELS (priv->ascent + priv->descent));

      baseline = pango_layout_get_baseline (layout) / PANGO_SCALE;

      *minimum = *natural = height;

      if (priv->placeholder)
        {
          int min, nat;

          gtk_widget_measure (priv->placeholder, GTK_ORIENTATION_VERTICAL, -1,
                              &min, &nat, NULL, NULL);
          *minimum = MAX (*minimum, min);
          *natural = MAX (*natural, nat);
        }

      if (minimum_baseline)
        *minimum_baseline = baseline;
      if (natural_baseline)
        *natural_baseline = baseline;
    }

  pango_font_metrics_unref (metrics);
}

static void
gtk_text_size_allocate (GtkWidget *widget,
                        int        width,
                        int        height,
                        int        baseline)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  priv->text_baseline = baseline;
  priv->text_x = 0;
  priv->text_width = width;

  if (priv->placeholder)
    {
      gtk_widget_size_allocate (priv->placeholder,
                                &(GtkAllocation) { 0, 0, width, height },
                                -1);
    }

  /* Do this here instead of gtk_text_size_allocate() so it works
   * inside spinbuttons, which don't chain up.
   */
  if (gtk_widget_get_realized (widget))
    gtk_text_recompute (entry);
}

static void
gtk_text_draw_undershoot (GtkText     *entry,
                          GtkSnapshot *snapshot)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkStyleContext *context;
  int min_offset, max_offset;
  GdkRectangle rect;

  context = gtk_widget_get_style_context (GTK_WIDGET (entry));

  gtk_text_get_scroll_limits (entry, &min_offset, &max_offset);

  gtk_text_get_text_allocation (entry, &rect);

  if (priv->scroll_offset > min_offset)
    {
      gtk_style_context_save_to_node (context, priv->undershoot_node[0]);
      gtk_snapshot_render_background (snapshot, context, rect.x, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_snapshot_render_frame (snapshot, context, rect.x, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_style_context_restore (context);
    }

  if (priv->scroll_offset < max_offset)
    {
      gtk_style_context_save_to_node (context, priv->undershoot_node[1]);
      gtk_snapshot_render_background (snapshot, context, rect.x + rect.width - UNDERSHOOT_SIZE, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_snapshot_render_frame (snapshot, context, rect.x + rect.width - UNDERSHOOT_SIZE, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_style_context_restore (context);
    }
}

static void
gtk_text_snapshot (GtkWidget   *widget,
                   GtkSnapshot *snapshot)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT (
                            priv->text_x,
                            0,
                            priv->text_width,
                            gtk_widget_get_height (widget)));

  /* Draw text and cursor */
  if (priv->dnd_position != -1)
    gtk_text_draw_cursor (GTK_TEXT (widget), snapshot, CURSOR_DND);

  if (priv->placeholder)
    gtk_widget_snapshot_child (widget, priv->placeholder, snapshot);

  gtk_text_draw_text (GTK_TEXT (widget), snapshot);

  /* When no text is being displayed at all, don't show the cursor */
  if (gtk_text_get_display_mode (entry) != DISPLAY_BLANK &&
      gtk_widget_has_focus (widget) &&
      priv->selection_bound == priv->current_pos && priv->cursor_visible)
    gtk_text_draw_cursor (GTK_TEXT (widget), snapshot, CURSOR_STANDARD);

  gtk_snapshot_pop (snapshot);

  gtk_text_draw_undershoot (entry, snapshot);
}

static void
gtk_text_get_pixel_ranges (GtkText  *entry,
                           int     **ranges,
                           int      *n_ranges)
{
  int start_char, end_char;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start_char, &end_char))
    {
      PangoLayout *layout = gtk_text_ensure_layout (entry, TRUE);
      PangoLayoutLine *line = pango_layout_get_lines_readonly (layout)->data;
      const char *text = pango_layout_get_text (layout);
      int start_index = g_utf8_offset_to_pointer (text, start_char) - text;
      int end_index = g_utf8_offset_to_pointer (text, end_char) - text;
      int real_n_ranges, i;

      pango_layout_line_get_x_ranges (line, start_index, end_index, ranges, &real_n_ranges);

      if (ranges)
        {
          int *r = *ranges;
          
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
in_selection (GtkText *entry,
              int      x)
{
  int *ranges;
  int n_ranges, i;
  int retval = FALSE;

  gtk_text_get_pixel_ranges (entry, &ranges, &n_ranges);

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
gtk_text_move_handle (GtkText               *entry,
                      GtkTextHandlePosition  pos,
                      int                    x,
                      int                    y,
                      int                    height)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkAllocation text_allocation;

  gtk_text_get_text_allocation (entry, &text_allocation);

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
      GdkRectangle rect;

      rect.x = x + text_allocation.x;
      rect.y = y + text_allocation.y;
      rect.width = 1;
      rect.height = height;

      _gtk_text_handle_set_visible (priv->text_handle, pos, TRUE);
      _gtk_text_handle_set_position (priv->text_handle, pos, &rect);
      _gtk_text_handle_set_direction (priv->text_handle, pos, priv->resolved_dir);
    }
}

static int
gtk_text_get_selection_bound_location (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  PangoLayout *layout;
  PangoRectangle pos;
  int x;
  const char *text;
  int index;

  layout = gtk_text_ensure_layout (entry, FALSE);
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
gtk_text_update_handles (GtkText           *entry,
                         GtkTextHandleMode  mode)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkAllocation text_allocation;
  int strong_x;
  int cursor, bound;

  _gtk_text_handle_set_mode (priv->text_handle, mode);
  gtk_text_get_text_allocation (entry, &text_allocation);

  gtk_text_get_cursor_locations (entry, &strong_x, NULL);
  cursor = strong_x - priv->scroll_offset;

  if (mode == GTK_TEXT_HANDLE_MODE_SELECTION)
    {
      int start, end;

      bound = gtk_text_get_selection_bound_location (entry) - priv->scroll_offset;

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
      gtk_text_move_handle (entry, GTK_TEXT_HANDLE_POSITION_SELECTION_START,
                             start, 0, text_allocation.height);
      gtk_text_move_handle (entry, GTK_TEXT_HANDLE_POSITION_SELECTION_END,
                             end, 0, text_allocation.height);
    }
  else
    gtk_text_move_handle (entry, GTK_TEXT_HANDLE_POSITION_CURSOR,
                           cursor, 0, text_allocation.height);
}

static void
gesture_get_current_point_in_layout (GtkGestureSingle *gesture,
                                     GtkText          *entry,
                                     int              *x,
                                     int              *y)
{
  int tx, ty;
  GdkEventSequence *sequence;
  double px, py;

  sequence = gtk_gesture_single_get_current_sequence (gesture);
  gtk_gesture_get_point (GTK_GESTURE (gesture), sequence, &px, &py);
  gtk_text_get_layout_offsets (entry, &tx, &ty);

  if (x)
    *x = px - tx;
  if (y)
    *y = py - ty;
}

static void
gtk_text_multipress_gesture_pressed (GtkGestureMultiPress *gesture,
                                     int                   n_press,
                                     double                widget_x,
                                     double                widget_y,
                                     GtkText              *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GdkEventSequence *current;
  const GdkEvent *event;
  int x, y, sel_start, sel_end;
  guint button;
  int tmp_pos;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), current);

  gtk_gesture_set_sequence_state (GTK_GESTURE (gesture), current,
                                  GTK_EVENT_SEQUENCE_CLAIMED);
  gesture_get_current_point_in_layout (GTK_GESTURE_SINGLE (gesture), entry, &x, &y);
  gtk_text_reset_blink_time (entry);

  if (!gtk_widget_has_focus (widget))
    {
      priv->in_click = TRUE;
      gtk_widget_grab_focus (widget);
      priv->in_click = FALSE;
    }

  tmp_pos = gtk_text_find_position (entry, x);

  if (gdk_event_triggers_context_menu (event))
    {
      gtk_text_do_popup (entry, event);
    }
  else if (n_press == 1 && button == GDK_BUTTON_MIDDLE &&
           get_middle_click_paste (entry))
    {
      if (priv->editable)
        {
          priv->insert_pos = tmp_pos;
          gtk_text_paste (entry, gtk_widget_get_primary_clipboard (widget));
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
        gtk_text_ensure_text_handles (entry);

      priv->in_drag = FALSE;
      priv->select_words = FALSE;
      priv->select_lines = FALSE;

      gdk_event_get_state (event, &state);

      extend_selection =
        (state &
         gtk_widget_get_modifier_mask (widget,
                                       GDK_MODIFIER_INTENT_EXTEND_SELECTION));

      if (extend_selection)
        gtk_text_reset_im_context (entry);

      switch (n_press)
        {
        case 1:
          if (in_selection (entry, x))
            {
              if (is_touchscreen)
                {
                  if (priv->selection_bubble &&
                      gtk_widget_get_visible (priv->selection_bubble))
                    gtk_text_selection_bubble_popup_unset (entry);
                  else
                    gtk_text_selection_bubble_popup_set (entry);
                }
              else if (extend_selection)
                {
                  /* Truncate current selection, but keep it as big as possible */
                  if (tmp_pos - sel_start > sel_end - tmp_pos)
                    gtk_text_set_positions (entry, sel_start, tmp_pos);
                  else
                    gtk_text_set_positions (entry, tmp_pos, sel_end);

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
              gtk_text_selection_bubble_popup_unset (entry);

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

                  gtk_text_set_positions (entry, tmp_pos, tmp_pos);
                }
            }

          break;

        case 2:
          priv->select_words = TRUE;
          gtk_text_select_word (entry);
          if (is_touchscreen)
            mode = GTK_TEXT_HANDLE_MODE_SELECTION;
          break;

        case 3:
          priv->select_lines = TRUE;
          gtk_text_select_line (entry);
          if (is_touchscreen)
            mode = GTK_TEXT_HANDLE_MODE_SELECTION;
          break;

        default:
          break;
        }

      if (extend_selection)
        {
          gboolean extend_to_left;
          int start, end;

          start = MIN (priv->current_pos, priv->selection_bound);
          start = MIN (sel_start, start);

          end = MAX (priv->current_pos, priv->selection_bound);
          end = MAX (sel_end, end);

          if (tmp_pos == sel_start || tmp_pos == sel_end)
            extend_to_left = (tmp_pos == start);
          else
            extend_to_left = (end == sel_end);

          if (extend_to_left)
            gtk_text_set_positions (entry, start, end);
          else
            gtk_text_set_positions (entry, end, start);
        }

      gtk_gesture_set_state (priv->drag_gesture,
                             GTK_EVENT_SEQUENCE_CLAIMED);

      if (priv->text_handle)
        gtk_text_update_handles (entry, mode);
    }

  if (n_press >= 3)
    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
}

static char *
_gtk_text_get_selected_text (GtkText *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  int start_text, end_text;
  char *text = NULL;

  if (gtk_editable_get_selection_bounds (editable, &start_text, &end_text))
    text = gtk_editable_get_chars (editable, start_text, end_text);

  return text;
}

static void
gtk_text_show_magnifier (GtkText *entry,
                         int      x,
                         int      y)
{
  GtkAllocation allocation;
  cairo_rectangle_int_t rect;
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkAllocation text_allocation;

  gtk_text_get_text_allocation (entry, &text_allocation);

  gtk_text_ensure_magnifier (entry);

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
gtk_text_motion_controller_motion (GtkEventControllerMotion *event_controller,
                                   double                    x,
                                   double                    y,
                                   gpointer                  user_data)
{
  GtkText *entry = user_data;
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->mouse_cursor_obscured)
    {
      set_text_cursor (GTK_WIDGET (entry));
      priv->mouse_cursor_obscured = FALSE;
    }
}

static void
gtk_text_drag_gesture_update (GtkGestureDrag *gesture,
                              double          offset_x,
                              double          offset_y,
                              GtkText        *entry)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GdkEventSequence *sequence;
  const GdkEvent *event;
  int x, y;

  gtk_text_selection_bubble_popup_unset (entry);

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
      if (gtk_text_get_display_mode (entry) == DISPLAY_NORMAL &&
          gtk_drag_check_threshold (widget,
                                    priv->drag_start_x, priv->drag_start_y,
                                    x, y))
        {
          int *ranges;
          int n_ranges;
          GdkContentFormats  *target_list = gdk_content_formats_new (NULL, 0);
          guint actions = priv->editable ? GDK_ACTION_COPY | GDK_ACTION_MOVE : GDK_ACTION_COPY;

          target_list = gtk_content_formats_add_text_targets (target_list);

          gtk_text_get_pixel_ranges (entry, &ranges, &n_ranges);

          gtk_drag_begin (widget,
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
      int tmp_pos;

      length = gtk_entry_buffer_get_length (get_buffer (entry));
      gtk_text_get_text_allocation (entry, &text_allocation);

      if (y < 0)
        tmp_pos = 0;
      else if (y >= text_allocation.height)
        tmp_pos = length;
      else
        tmp_pos = gtk_text_find_position (entry, x);

      source = gdk_event_get_source_device (event);
      input_source = gdk_device_get_source (source);

      if (priv->select_words)
        {
          int min, max;
          int old_min, old_max;
          int pos, bound;

          min = gtk_text_move_backward_word (entry, tmp_pos, TRUE);
          max = gtk_text_move_forward_word (entry, tmp_pos, TRUE);

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

          gtk_text_set_positions (entry, pos, bound);
        }
      else
        gtk_text_set_positions (entry, tmp_pos, -1);

      /* Update touch handles' position */
      if (gtk_simulate_touchscreen () ||
          input_source == GDK_SOURCE_TOUCHSCREEN)
        {
          gtk_text_ensure_text_handles (entry);
          gtk_text_update_handles (entry,
                                    (priv->current_pos == priv->selection_bound) ?
                                    GTK_TEXT_HANDLE_MODE_CURSOR :
                                    GTK_TEXT_HANDLE_MODE_SELECTION);
          gtk_text_show_magnifier (entry, x - priv->scroll_offset, y);
        }
    }
}

static void
gtk_text_drag_gesture_end (GtkGestureDrag *gesture,
                           double          offset_x,
                           double          offset_y,
                           GtkText        *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
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
      int tmp_pos = gtk_text_find_position (entry, priv->drag_start_x);

      gtk_editable_set_position (GTK_EDITABLE (entry), tmp_pos);
    }

  if (is_touchscreen &&
      !gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), NULL, NULL))
    gtk_text_update_handles (entry, GTK_TEXT_HANDLE_MODE_CURSOR);

  gtk_text_update_primary_selection (entry);
}

static void
gtk_text_obscure_mouse_cursor (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GdkCursor *cursor;

  if (priv->mouse_cursor_obscured)
    return;

  cursor = gdk_cursor_new_from_name ("none", NULL);
  gtk_widget_set_cursor (GTK_WIDGET (entry), cursor);
  g_object_unref (cursor);

  priv->mouse_cursor_obscured = TRUE;
}

static gboolean
gtk_text_key_controller_key_pressed (GtkEventControllerKey *controller,
                                      guint                  keyval,
                                      guint                  keycode,
                                      GdkModifierType        state,
                                      GtkWidget             *widget)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  gunichar unichar;

  gtk_text_reset_blink_time (entry);
  gtk_text_pend_cursor_blink (entry);

  gtk_text_selection_bubble_popup_unset (entry);

  if (priv->text_handle)
    _gtk_text_handle_set_mode (priv->text_handle,
                               GTK_TEXT_HANDLE_MODE_NONE);

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter ||
      keyval == GDK_KEY_Escape)
    gtk_text_reset_im_context (entry);

  unichar = gdk_keyval_to_unicode (keyval);

  if (!priv->editable && unichar != 0)
    gtk_widget_error_bell (widget);

  gtk_text_obscure_mouse_cursor (entry);

  return FALSE;
}

static void
gtk_text_focus_in (GtkWidget *widget)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GdkKeymap *keymap;

  gtk_widget_queue_draw (widget);

  keymap = gdk_display_get_keymap (gtk_widget_get_display (widget));

  if (priv->editable)
    {
      gtk_text_schedule_im_reset (entry);
      gtk_im_context_focus_in (priv->im_context);
    }

  g_signal_connect (keymap, "direction-changed",
                    G_CALLBACK (keymap_direction_changed), entry);

  gtk_text_reset_blink_time (entry);
  gtk_text_check_cursor_blink (entry);
}

static void
gtk_text_focus_out (GtkWidget *widget)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GdkKeymap *keymap;

  gtk_text_selection_bubble_popup_unset (entry);

  if (priv->text_handle)
    _gtk_text_handle_set_mode (priv->text_handle,
                               GTK_TEXT_HANDLE_MODE_NONE);

  gtk_widget_queue_draw (widget);

  keymap = gdk_display_get_keymap (gtk_widget_get_display (widget));

  if (priv->editable)
    {
      gtk_text_schedule_im_reset (entry);
      gtk_im_context_focus_out (priv->im_context);
    }

  gtk_text_check_cursor_blink (entry);

  g_signal_handlers_disconnect_by_func (keymap, keymap_direction_changed, entry);
}

static void
gtk_text_grab_focus (GtkWidget *widget)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  gboolean select_on_focus;

  GTK_WIDGET_CLASS (gtk_text_parent_class)->grab_focus (GTK_WIDGET (entry));

  if (priv->editable && !priv->in_click)
    {
      g_object_get (gtk_widget_get_settings (widget),
                    "gtk-entry-select-on-focus",
                    &select_on_focus,
                    NULL);

      if (select_on_focus)
        gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
    }
}

/**
 * gtk_text_grab_focus_without_selecting:
 * @entry: a #GtkText
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
gtk_text_grab_focus_without_selecting (GtkText *entry)
{
  g_return_if_fail (GTK_IS_TEXT (entry));

  GTK_WIDGET_CLASS (gtk_text_parent_class)->grab_focus (GTK_WIDGET (entry));
}

static void
gtk_text_direction_changed (GtkWidget        *widget,
                            GtkTextDirection  previous_dir)
{
  GtkText *entry = GTK_TEXT (widget);

  gtk_text_recompute (entry);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_text_state_flags_changed (GtkWidget     *widget,
                              GtkStateFlags  previous_state)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

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

  gtk_text_update_cached_style_values (entry);
}

static void
gtk_text_display_changed (GtkWidget  *widget,
                          GdkDisplay *old_display)
{
  gtk_text_recompute (GTK_TEXT (widget));
}

/* GtkEditable method implementations
 */
static void
gtk_text_insert_text (GtkEditable *editable,
                      const char  *text,
                      int          length,
                      int         *position)
{
  g_object_ref (editable);

  /*
   * The incoming text may a password or other secret. We make sure
   * not to copy it into temporary buffers.
   */

  g_signal_emit_by_name (editable, "insert-text", text, length, position);

  g_object_unref (editable);
}

static void
gtk_text_delete_text (GtkEditable *editable,
                      int          start_pos,
                      int          end_pos)
{
  g_object_ref (editable);

  g_signal_emit_by_name (editable, "delete-text", start_pos, end_pos);

  g_object_unref (editable);
}

static const char *
gtk_text_get_text (GtkEditable *editable)
{
  return gtk_entry_buffer_get_text (gtk_text_get_buffer (GTK_TEXT (editable)));
}

static void
gtk_text_real_set_position (GtkEditable *editable,
                            int          position)
{
  GtkText *entry = GTK_TEXT (editable);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (entry));
  if (position < 0 || position > length)
    position = length;

  if (position != priv->current_pos ||
      position != priv->selection_bound)
    {
      gtk_text_reset_im_context (entry);
      gtk_text_set_positions (entry, position, position);
    }
}

static int
gtk_text_get_position (GtkEditable *editable)
{
  GtkText *entry = GTK_TEXT (editable);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  return priv->current_pos;
}

static void
gtk_text_set_selection_bounds (GtkEditable *editable,
                               int          start,
                               int          end)
{
  GtkText *entry = GTK_TEXT (editable);
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (entry));
  if (start < 0)
    start = length;
  if (end < 0)
    end = length;

  gtk_text_reset_im_context (entry);

  gtk_text_set_positions (entry,
                           MIN (end, length),
                           MIN (start, length));

  gtk_text_update_primary_selection (entry);
}

static gboolean
gtk_text_get_selection_bounds (GtkEditable *editable,
                               int         *start,
                               int         *end)
{
  GtkText *entry = GTK_TEXT (editable);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  *start = priv->selection_bound;
  *end = priv->current_pos;

  return (priv->selection_bound != priv->current_pos);
}

static gunichar
find_invisible_char (GtkWidget *widget)
{
  PangoLayout *layout;
  PangoAttrList *attr_list;
  int i;
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
      char text[7] = { 0, };
      int len, count;

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
gtk_text_update_cached_style_values (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

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
gtk_text_style_updated (GtkWidget *widget)
{
  GtkText *entry = GTK_TEXT (widget);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->style_updated (widget);

  gtk_text_update_cached_style_values (entry);
}

static void
gtk_text_password_hint_free (GtkTextPasswordHint *password_hint)
{
  if (password_hint->source_id)
    g_source_remove (password_hint->source_id);

  g_slice_free (GtkTextPasswordHint, password_hint);
}


static gboolean
gtk_text_remove_password_hint (gpointer data)
{
  GtkTextPasswordHint *password_hint = g_object_get_qdata (data, quark_password_hint);
  password_hint->position = -1;
  password_hint->source_id = 0;

  /* Force the string to be redrawn, but now without a visible character */
  gtk_text_recompute (GTK_TEXT (data));

  return G_SOURCE_REMOVE;
}

static void
update_placeholder_visibility (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->placeholder)
    gtk_widget_set_child_visible (priv->placeholder,
                                  gtk_entry_buffer_get_length (priv->buffer) == 0);
}

/* Default signal handlers
 */
static void
gtk_text_real_insert_text (GtkEditable *editable,
                           const char  *text,
                           int          length,
                           int         *position)
{
  guint n_inserted;
  int n_chars;

  n_chars = g_utf8_strlen (text, length);

  /*
   * The actual insertion into the buffer. This will end up firing the
   * following signal handlers: buffer_inserted_text(), buffer_notify_display_text(),
   * buffer_notify_text()
   */
  begin_change (GTK_TEXT (editable));

  n_inserted = gtk_entry_buffer_insert_text (get_buffer (GTK_TEXT (editable)), *position, text, n_chars);

  end_change (GTK_TEXT (editable));

  if (n_inserted != n_chars)
    gtk_widget_error_bell (GTK_WIDGET (editable));

  *position += n_inserted;

  update_placeholder_visibility (GTK_TEXT (editable));
}

static void
gtk_text_real_delete_text (GtkEditable *editable,
                           int          start_pos,
                           int          end_pos)
{
  /*
   * The actual deletion from the buffer. This will end up firing the
   * following signal handlers: buffer_deleted_text(), buffer_notify_display_text(),
   * buffer_notify_text()
   */

  begin_change (GTK_TEXT (editable));

  gtk_entry_buffer_delete_text (get_buffer (GTK_TEXT (editable)), start_pos, end_pos - start_pos);

  end_change (GTK_TEXT (editable));
  update_placeholder_visibility (GTK_TEXT (editable));
}

/* GtkEntryBuffer signal handlers
 */
static void
buffer_inserted_text (GtkEntryBuffer *buffer,
                      guint           position,
                      const char     *chars,
                      guint           n_chars,
                      GtkText        *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  guint password_hint_timeout;
  guint current_pos;
  int selection_bound;

  current_pos = priv->current_pos;
  if (current_pos > position)
    current_pos += n_chars;

  selection_bound = priv->selection_bound;
  if (selection_bound > position)
    selection_bound += n_chars;

  gtk_text_set_positions (entry, current_pos, selection_bound);
  gtk_text_recompute (entry);

  /* Calculate the password hint if it needs to be displayed. */
  if (n_chars == 1 && !priv->visible)
    {
      g_object_get (gtk_widget_get_settings (GTK_WIDGET (entry)),
                    "gtk-entry-password-hint-timeout", &password_hint_timeout,
                    NULL);

      if (password_hint_timeout > 0)
        {
          GtkTextPasswordHint *password_hint = g_object_get_qdata (G_OBJECT (entry),
                                                                    quark_password_hint);
          if (!password_hint)
            {
              password_hint = g_slice_new0 (GtkTextPasswordHint);
              g_object_set_qdata_full (G_OBJECT (entry), quark_password_hint, password_hint,
                                       (GDestroyNotify)gtk_text_password_hint_free);
            }

          password_hint->position = position;
          if (password_hint->source_id)
            g_source_remove (password_hint->source_id);
          password_hint->source_id = g_timeout_add (password_hint_timeout,
                                                    (GSourceFunc)gtk_text_remove_password_hint,
                                                    entry);
          g_source_set_name_by_id (password_hint->source_id, "[gtk] gtk_text_remove_password_hint");
        }
    }
}

static void
buffer_deleted_text (GtkEntryBuffer *buffer,
                     guint           position,
                     guint           n_chars,
                     GtkText        *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  guint end_pos = position + n_chars;
  int selection_bound;
  guint current_pos;

  current_pos = priv->current_pos;
  if (current_pos > position)
    current_pos -= MIN (current_pos, end_pos) - position;

  selection_bound = priv->selection_bound;
  if (selection_bound > position)
    selection_bound -= MIN (selection_bound, end_pos) - position;

  gtk_text_set_positions (entry, current_pos, selection_bound);
  gtk_text_recompute (entry);

  /* We might have deleted the selection */
  gtk_text_update_primary_selection (entry);

  /* Disable the password hint if one exists. */
  if (!priv->visible)
    {
      GtkTextPasswordHint *password_hint = g_object_get_qdata (G_OBJECT (entry),
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
                    GtkText        *entry)
{
  emit_changed (entry);
  g_object_notify (G_OBJECT (entry), "text");
}

static void
buffer_notify_max_length (GtkEntryBuffer *buffer,
                          GParamSpec     *spec,
                          GtkText        *entry)
{
  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_MAX_LENGTH]);
}

static void
buffer_connect_signals (GtkText *entry)
{
  g_signal_connect (get_buffer (entry), "inserted-text", G_CALLBACK (buffer_inserted_text), entry);
  g_signal_connect (get_buffer (entry), "deleted-text", G_CALLBACK (buffer_deleted_text), entry);
  g_signal_connect (get_buffer (entry), "notify::text", G_CALLBACK (buffer_notify_text), entry);
  g_signal_connect (get_buffer (entry), "notify::max-length", G_CALLBACK (buffer_notify_max_length), entry);
}

static void
buffer_disconnect_signals (GtkText *entry)
{
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_inserted_text, entry);
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_deleted_text, entry);
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_notify_text, entry);
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_notify_max_length, entry);
}

/* Compute the X position for an offset that corresponds to the "more important
 * cursor position for that offset. We use this when trying to guess to which
 * end of the selection we should go to when the user hits the left or
 * right arrow key.
 */
static int
get_better_cursor_x (GtkText *entry,
                     int      offset)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GdkKeymap *keymap = gdk_display_get_keymap (gtk_widget_get_display (GTK_WIDGET (entry)));
  PangoDirection keymap_direction = gdk_keymap_get_direction (keymap);
  gboolean split_cursor;
  PangoLayout *layout = gtk_text_ensure_layout (entry, TRUE);
  const char *text = pango_layout_get_text (layout);
  int index = g_utf8_offset_to_pointer (text, offset) - text;
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
gtk_text_move_cursor (GtkText         *entry,
                      GtkMovementStep  step,
                      int              count,
                      gboolean         extend_selection)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  int new_pos = priv->current_pos;

  gtk_text_reset_im_context (entry);

  if (priv->current_pos != priv->selection_bound && !extend_selection)
    {
      /* If we have a current selection and aren't extending it, move to the
       * start/or end of the selection as appropriate
       */
      switch (step)
        {
        case GTK_MOVEMENT_VISUAL_POSITIONS:
          {
            int current_x = get_better_cursor_x (entry, priv->current_pos);
            int bound_x = get_better_cursor_x (entry, priv->selection_bound);

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
          new_pos = gtk_text_move_logically (entry, new_pos, count);
          break;

        case GTK_MOVEMENT_VISUAL_POSITIONS:
          new_pos = gtk_text_move_visually (entry, new_pos, count);

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
              new_pos = gtk_text_move_forward_word (entry, new_pos, FALSE);
              count--;
            }

          while (count < 0)
            {
              new_pos = gtk_text_move_backward_word (entry, new_pos, FALSE);
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
  
  gtk_text_pend_cursor_blink (entry);
}

static void
gtk_text_insert_at_cursor (GtkText    *entry,
                           const char *str)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  int pos = priv->current_pos;

  if (priv->editable)
    {
      gtk_text_reset_im_context (entry);
      gtk_editable_insert_text (editable, str, -1, &pos);
      gtk_editable_set_position (editable, pos);
    }
}

static void
gtk_text_delete_from_cursor (GtkText       *entry,
                             GtkDeleteType  type,
                             int            count)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  int start_pos = priv->current_pos;
  int end_pos = priv->current_pos;
  int old_n_bytes = gtk_entry_buffer_get_bytes (get_buffer (entry));

  gtk_text_reset_im_context (entry);

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
      end_pos = gtk_text_move_logically (entry, priv->current_pos, count);
      gtk_editable_delete_text (editable, MIN (start_pos, end_pos), MAX (start_pos, end_pos));
      break;

    case GTK_DELETE_WORDS:
      if (count < 0)
        {
          /* Move to end of current word, or if not on a word, end of previous word */
          end_pos = gtk_text_move_backward_word (entry, end_pos, FALSE);
          end_pos = gtk_text_move_forward_word (entry, end_pos, FALSE);
        }
      else if (count > 0)
        {
          /* Move to beginning of current word, or if not on a word, begining of next word */
          start_pos = gtk_text_move_forward_word (entry, start_pos, FALSE);
          start_pos = gtk_text_move_backward_word (entry, start_pos, FALSE);
        }
        
      /* Fall through */
    case GTK_DELETE_WORD_ENDS:
      while (count < 0)
        {
          start_pos = gtk_text_move_backward_word (entry, start_pos, FALSE);
          count++;
        }

      while (count > 0)
        {
          end_pos = gtk_text_move_forward_word (entry, end_pos, FALSE);
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
      gtk_text_delete_whitespace (entry);
      break;

    default:
      break;
    }

  if (gtk_entry_buffer_get_bytes (get_buffer (entry)) == old_n_bytes)
    gtk_widget_error_bell (GTK_WIDGET (entry));

  gtk_text_pend_cursor_blink (entry);
}

static void
gtk_text_backspace (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  int prev_pos;

  gtk_text_reset_im_context (entry);

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

  prev_pos = gtk_text_move_logically (entry, priv->current_pos, -1);

  if (prev_pos < priv->current_pos)
    {
      PangoLayout *layout = gtk_text_ensure_layout (entry, FALSE);
      const PangoLogAttr *log_attrs;
      int n_attrs;

      log_attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

      /* Deleting parts of characters */
      if (log_attrs[priv->current_pos].backspace_deletes_character)
        {
          char *cluster_text;
          char *normalized_text;
          glong  len;

          cluster_text = gtk_text_get_display_text (entry, prev_pos, priv->current_pos);
          normalized_text = g_utf8_normalize (cluster_text,
                                              strlen (cluster_text),
                                              G_NORMALIZE_NFD);
          len = g_utf8_strlen (normalized_text, -1);

          gtk_editable_delete_text (editable, prev_pos, priv->current_pos);
          if (len > 1)
            {
              int pos = priv->current_pos;

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
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
    }

  gtk_text_pend_cursor_blink (entry);
}

static void
gtk_text_copy_clipboard (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  int start, end;
  char *str;

  if (gtk_editable_get_selection_bounds (editable, &start, &end))
    {
      if (!priv->visible)
        {
          gtk_widget_error_bell (GTK_WIDGET (entry));
          return;
        }

      str = gtk_text_get_display_text (entry, start, end);
      gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (entry)), str);
      g_free (str);
    }
}

static void
gtk_text_cut_clipboard (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  int start, end;

  if (!priv->visible)
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
      return;
    }

  gtk_text_copy_clipboard (entry);

  if (priv->editable)
    {
      if (gtk_editable_get_selection_bounds (editable, &start, &end))
        gtk_editable_delete_text (editable, start, end);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
    }

  gtk_text_selection_bubble_popup_unset (entry);

  if (priv->text_handle)
    {
      GtkTextHandleMode handle_mode;

      handle_mode = _gtk_text_handle_get_mode (priv->text_handle);

      if (handle_mode != GTK_TEXT_HANDLE_MODE_NONE)
        gtk_text_update_handles (entry, GTK_TEXT_HANDLE_MODE_CURSOR);
    }
}

static void
gtk_text_paste_clipboard (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->editable)
    gtk_text_paste (entry, gtk_widget_get_clipboard (GTK_WIDGET (entry)));
  else
    gtk_widget_error_bell (GTK_WIDGET (entry));

  if (priv->text_handle)
    {
      GtkTextHandleMode handle_mode;

      handle_mode = _gtk_text_handle_get_mode (priv->text_handle);

      if (handle_mode != GTK_TEXT_HANDLE_MODE_NONE)
        gtk_text_update_handles (entry, GTK_TEXT_HANDLE_MODE_CURSOR);
    }
}

static void
gtk_text_delete_cb (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  int start, end;

  if (priv->editable)
    {
      if (gtk_editable_get_selection_bounds (editable, &start, &end))
        gtk_editable_delete_text (editable, start, end);
    }
}

static void
gtk_text_toggle_overwrite (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

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

  gtk_text_pend_cursor_blink (entry);
  gtk_widget_queue_draw (GTK_WIDGET (entry));
}

static void
gtk_text_select_all (GtkText *entry)
{
  gtk_text_select_line (entry);
}

static void
gtk_text_real_activate (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
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
                          GtkText   *entry)
{
  gtk_text_recompute (entry);
}

/* IM Context Callbacks
 */

static void
gtk_text_commit_cb (GtkIMContext *context,
                    const char   *str,
                    GtkText      *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->editable)
    {
      gtk_text_enter_text (entry, str);
      gtk_text_obscure_mouse_cursor (entry);
    }
}

static void 
gtk_text_preedit_changed_cb (GtkIMContext *context,
                             GtkText      *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->editable)
    {
      char *preedit_string;
      int cursor_pos;

      gtk_text_obscure_mouse_cursor (entry);

      gtk_im_context_get_preedit_string (priv->im_context,
                                         &preedit_string, NULL,
                                         &cursor_pos);
      g_signal_emit (entry, signals[PREEDIT_CHANGED], 0, preedit_string);
      priv->preedit_length = strlen (preedit_string);
      cursor_pos = CLAMP (cursor_pos, 0, g_utf8_strlen (preedit_string, -1));
      priv->preedit_cursor = cursor_pos;
      g_free (preedit_string);
    
      gtk_text_recompute (entry);
    }
}

static gboolean
gtk_text_retrieve_surrounding_cb (GtkIMContext *context,
                                  GtkText      *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  char *text;

  /* XXXX ??? does this even make sense when text is not visible? Should we return FALSE? */
  text = gtk_text_get_display_text (entry, 0, -1);
  gtk_im_context_set_surrounding (context, text, strlen (text), /* Length in bytes */
                                  g_utf8_offset_to_pointer (text, priv->current_pos) - text);
  g_free (text);

  return TRUE;
}

static gboolean
gtk_text_delete_surrounding_cb (GtkIMContext *slave,
                                int           offset,
                                int           n_chars,
                                GtkText      *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

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
gtk_text_enter_text (GtkText    *entry,
                     const char *str)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (entry);
  int tmp_pos;
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
            gtk_text_delete_from_cursor (entry, GTK_DELETE_CHARS, 1);
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
gtk_text_set_positions (GtkText *entry,
                        int      current_pos,
                        int      selection_bound)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  gboolean changed = FALSE;

  g_object_freeze_notify (G_OBJECT (entry));

  if (current_pos != -1 &&
      priv->current_pos != current_pos)
    {
      priv->current_pos = current_pos;
      changed = TRUE;

      g_object_notify (G_OBJECT (entry), "cursor-position");
    }

  if (selection_bound != -1 &&
      priv->selection_bound != selection_bound)
    {
      priv->selection_bound = selection_bound;
      changed = TRUE;

      g_object_notify (G_OBJECT (entry), "selection-bound");
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
      gtk_text_recompute (entry);
    }
}

static void
gtk_text_reset_layout (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->cached_layout)
    {
      g_object_unref (priv->cached_layout);
      priv->cached_layout = NULL;
    }
}

static void
update_im_cursor_location (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GdkRectangle area;
  GtkAllocation text_area;
  int strong_x;
  int strong_xoffset;

  gtk_text_get_cursor_locations (entry, &strong_x, NULL);
  gtk_text_get_text_allocation (entry, &text_area);

  strong_xoffset = strong_x - priv->scroll_offset;
  if (strong_xoffset < 0)
    strong_xoffset = 0;
  else if (strong_xoffset > text_area.width)
    strong_xoffset = text_area.width;

  area.x = strong_xoffset;
  area.y = 0;
  area.width = 0;
  area.height = text_area.height;

  gtk_im_context_set_cursor_location (priv->im_context, &area);
}

static void
gtk_text_recompute (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkTextHandleMode handle_mode;

  gtk_text_reset_layout (entry);
  gtk_text_check_cursor_blink (entry);

  gtk_text_adjust_scroll (entry);

  update_im_cursor_location (entry);

  if (priv->text_handle)
    {
      handle_mode = _gtk_text_handle_get_mode (priv->text_handle);

      if (handle_mode != GTK_TEXT_HANDLE_MODE_NONE)
        gtk_text_update_handles (entry, handle_mode);
    }

  gtk_widget_queue_draw (GTK_WIDGET (entry));
}

static PangoLayout *
gtk_text_create_layout (GtkText  *entry,
                        gboolean  include_preedit)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkStyleContext *context;
  PangoLayout *layout;
  PangoAttrList *tmp_attrs;
  char *preedit_string = NULL;
  int preedit_length = 0;
  PangoAttrList *preedit_attrs = NULL;
  char *display_text;
  guint n_bytes;

  context = gtk_widget_get_style_context (widget);

  layout = gtk_widget_create_pango_layout (widget, NULL);
  pango_layout_set_single_paragraph_mode (layout, TRUE);

  tmp_attrs = _gtk_style_context_get_pango_attributes (context);
  tmp_attrs = _gtk_pango_attr_list_merge (tmp_attrs, priv->attrs);
  if (!tmp_attrs)
    tmp_attrs = pango_attr_list_new ();

  display_text = gtk_text_get_display_text (entry, 0, -1);

  n_bytes = strlen (display_text);

  if (include_preedit)
    {
      gtk_im_context_get_preedit_string (priv->im_context,
                                         &preedit_string, &preedit_attrs, NULL);
      preedit_length = priv->preedit_length;
    }

  if (preedit_length)
    {
      GString *tmp_string = g_string_new (display_text);
      int pos;

      pos = g_utf8_offset_to_pointer (display_text, priv->current_pos) - display_text;
      g_string_insert (tmp_string, pos, preedit_string);
      pango_layout_set_text (layout, tmp_string->str, tmp_string->len);
      pango_attr_list_splice (tmp_attrs, preedit_attrs, pos, preedit_length);
      g_string_free (tmp_string, TRUE);
    }
  else
    {
      PangoDirection pango_dir;

      if (gtk_text_get_display_mode (entry) == DISPLAY_NORMAL)
        pango_dir = gdk_find_base_dir (display_text, n_bytes);
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
gtk_text_ensure_layout (GtkText  *entry,
                        gboolean  include_preedit)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->preedit_length > 0 &&
      !include_preedit != !priv->cache_includes_preedit)
    gtk_text_reset_layout (entry);

  if (!priv->cached_layout)
    {
      priv->cached_layout = gtk_text_create_layout (entry, include_preedit);
      priv->cache_includes_preedit = include_preedit;
    }

  return priv->cached_layout;
}

static void
get_layout_position (GtkText *entry,
                     int     *x,
                     int     *y)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  PangoLayout *layout;
  PangoRectangle logical_rect;
  int y_pos, area_height;
  PangoLayoutLine *line;
  GtkAllocation text_allocation;

  gtk_text_get_text_allocation (entry, &text_allocation);

  layout = gtk_text_ensure_layout (entry, TRUE);

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
gtk_text_draw_text (GtkText     *entry,
                    GtkSnapshot *snapshot)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkStyleContext *context;
  PangoLayout *layout;
  int x, y;
  int start_pos, end_pos;
  int width, height;

  /* Nothing to display at all */
  if (gtk_text_get_display_mode (entry) == DISPLAY_BLANK)
    return;

  context = gtk_widget_get_style_context (widget);
  layout = gtk_text_ensure_layout (entry, TRUE);
  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  gtk_text_get_layout_offsets (entry, &x, &y);

  gtk_snapshot_render_layout (snapshot, context, x, y, layout);

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start_pos, &end_pos))
    {
      const char *text = pango_layout_get_text (layout);
      int start_index = g_utf8_offset_to_pointer (text, start_pos) - text;
      int end_index = g_utf8_offset_to_pointer (text, end_pos) - text;
      cairo_region_t *clip;
      cairo_rectangle_int_t clip_extents;
      int range[2];

      range[0] = MIN (start_index, end_index);
      range[1] = MAX (start_index, end_index);

      gtk_style_context_save_to_node (context, priv->selection_node);

      clip = gdk_pango_layout_get_clip_region (layout, x, y, range, 1);
      cairo_region_get_extents (clip, &clip_extents);

      gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_FROM_RECT (&clip_extents));
      gtk_snapshot_render_background (snapshot, context, 0, 0, width, height);
      gtk_snapshot_render_layout (snapshot, context, x, y, layout);
      gtk_snapshot_pop (snapshot);

      cairo_region_destroy (clip);

      gtk_style_context_restore (context);
    }
}

static void
gtk_text_draw_cursor (GtkText     *entry,
                      GtkSnapshot *snapshot,
                      CursorType   type)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkStyleContext *context;
  PangoRectangle cursor_rect;
  int cursor_index;
  gboolean block;
  gboolean block_at_line_end;
  PangoLayout *layout;
  const char *text;
  int x, y;
  int width, height;

  context = gtk_widget_get_style_context (widget);

  layout = gtk_text_ensure_layout (entry, TRUE);
  text = pango_layout_get_text (layout);
  gtk_text_get_layout_offsets (entry, &x, &y);
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

      gtk_snapshot_push_clip (snapshot, &bounds);
      gtk_snapshot_render_background (snapshot, context,  0, 0, width, height);
      gtk_snapshot_render_layout (snapshot, context,  x, y, layout);
      gtk_snapshot_pop (snapshot);

      gtk_style_context_restore (context);
    }
}

static void
gtk_text_handle_dragged (GtkTextHandle         *handle,
                         GtkTextHandlePosition  pos,
                         int                    x,
                         int                    y,
                         GtkText               *entry)
{
  int cursor_pos, selection_bound_pos, tmp_pos;
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkTextHandleMode mode;
  int *min, *max;

  gtk_text_selection_bubble_popup_unset (entry);

  cursor_pos = priv->current_pos;
  selection_bound_pos = priv->selection_bound;
  mode = _gtk_text_handle_get_mode (handle);

  tmp_pos = gtk_text_find_position (entry, x + priv->scroll_offset);

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
          int min_pos;

          min_pos = MAX (*min + 1, 0);
          tmp_pos = MAX (tmp_pos, min_pos);
        }

      *max = tmp_pos;
    }
  else
    {
      if (mode == GTK_TEXT_HANDLE_MODE_SELECTION)
        {
          int max_pos;

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
          gtk_text_set_positions (entry, cursor_pos, cursor_pos);
        }
      else
        {
          priv->selection_handle_dragged = TRUE;
          gtk_text_set_positions (entry, cursor_pos, selection_bound_pos);
        }

      gtk_text_update_handles (entry, mode);
    }

  gtk_text_show_magnifier (entry, x, y);
}

static void
gtk_text_handle_drag_started (GtkTextHandle         *handle,
                              GtkTextHandlePosition  pos,
                              GtkText               *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  priv->cursor_handle_dragged = FALSE;
  priv->selection_handle_dragged = FALSE;
}

static void
gtk_text_handle_drag_finished (GtkTextHandle         *handle,
                               GtkTextHandlePosition  pos,
                               GtkText               *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (!priv->cursor_handle_dragged && !priv->selection_handle_dragged)
    {
      GtkSettings *settings;
      guint double_click_time;

      settings = gtk_widget_get_settings (GTK_WIDGET (entry));
      g_object_get (settings, "gtk-double-click-time", &double_click_time, NULL);
      if (g_get_monotonic_time() - priv->handle_place_time < double_click_time * 1000)
        {
          gtk_text_select_word (entry);
          gtk_text_update_handles (entry, GTK_TEXT_HANDLE_MODE_SELECTION);
        }
      else
        gtk_text_selection_bubble_popup_set (entry);
    }

  if (priv->magnifier_popover)
    gtk_popover_popdown (GTK_POPOVER (priv->magnifier_popover));
}

static void
gtk_text_schedule_im_reset (GtkText *entry)
{
  GtkTextPrivate *priv;

  priv = gtk_text_get_instance_private (entry);

  priv->need_im_reset = TRUE;
}

void
gtk_text_reset_im_context (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_if_fail (GTK_IS_TEXT (entry));

  if (priv->need_im_reset)
    {
      priv->need_im_reset = FALSE;
      gtk_im_context_reset (priv->im_context);
    }
}

GtkIMContext *
gtk_text_get_im_context (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  return priv->im_context;
}

static int
gtk_text_find_position (GtkText *entry,
                        int      x)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  PangoLayout *layout;
  PangoLayoutLine *line;
  int index;
  int pos;
  int trailing;
  const char *text;
  int cursor_index;
  
  layout = gtk_text_ensure_layout (entry, TRUE);
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
gtk_text_get_cursor_locations (GtkText   *entry,
                               int       *strong_x,
                               int       *weak_x)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  DisplayMode mode = gtk_text_get_display_mode (entry);

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
      PangoLayout *layout = gtk_text_ensure_layout (entry, TRUE);
      const char *text = pango_layout_get_text (layout);
      PangoRectangle strong_pos, weak_pos;
      int index;
  
      index = g_utf8_offset_to_pointer (text, priv->current_pos + priv->preedit_cursor) - text;

      pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);
      
      if (strong_x)
        *strong_x = strong_pos.x / PANGO_SCALE;
      
      if (weak_x)
        *weak_x = weak_pos.x / PANGO_SCALE;
    }
}

static gboolean
gtk_text_get_is_selection_handle_dragged (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
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
gtk_text_get_scroll_limits (GtkText *entry,
                            int     *min_offset,
                            int     *max_offset)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  float xalign;
  PangoLayout *layout;
  PangoLayoutLine *line;
  PangoRectangle logical_rect;
  int text_width;

  layout = gtk_text_ensure_layout (entry, TRUE);
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
gtk_text_adjust_scroll (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  int min_offset, max_offset;
  int strong_x, weak_x;
  int strong_xoffset, weak_xoffset;
  GtkTextHandleMode handle_mode;
  GtkAllocation text_allocation;

  if (!gtk_widget_get_realized (GTK_WIDGET (entry)))
    return;

  gtk_text_get_text_allocation (entry, &text_allocation);

  gtk_text_get_scroll_limits (entry, &min_offset, &max_offset);

  priv->scroll_offset = CLAMP (priv->scroll_offset, min_offset, max_offset);

  if (gtk_text_get_is_selection_handle_dragged (entry))
    {
      /* The text handle corresponding to the selection bound is
       * being dragged, ensure it stays onscreen even if we scroll
       * cursors away, this is so both handles can cause content
       * to scroll.
       */
      strong_x = weak_x = gtk_text_get_selection_bound_location (entry);
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
      gtk_text_get_cursor_locations (entry, &strong_x, &weak_x);
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
        gtk_text_update_handles (entry, handle_mode);
    }
}

static int
gtk_text_move_visually (GtkText *entry,
                        int      start,
                        int      count)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  int index;
  PangoLayout *layout = gtk_text_ensure_layout (entry, FALSE);
  const char *text;

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

static int
gtk_text_move_logically (GtkText *entry,
                         int      start,
                         int      count)
{
  int new_pos = start;
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (entry));

  /* Prevent any leak of information */
  if (gtk_text_get_display_mode (entry) != DISPLAY_NORMAL)
    {
      new_pos = CLAMP (start + count, 0, length);
    }
  else
    {
      PangoLayout *layout = gtk_text_ensure_layout (entry, FALSE);
      const PangoLogAttr *log_attrs;
      int n_attrs;

      log_attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

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
    }

  return new_pos;
}

static int
gtk_text_move_forward_word (GtkText  *entry,
                            int       start,
                            gboolean  allow_whitespace)
{
  int new_pos = start;
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (entry));

  /* Prevent any leak of information */
  if (gtk_text_get_display_mode (entry) != DISPLAY_NORMAL)
    {
      new_pos = length;
    }
  else if (new_pos < length)
    {
      PangoLayout *layout = gtk_text_ensure_layout (entry, FALSE);
      const PangoLogAttr *log_attrs;
      int n_attrs;

      log_attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

      /* Find the next word boundary */
      new_pos++;
      while (new_pos < n_attrs - 1 && !(log_attrs[new_pos].is_word_end ||
                                        (log_attrs[new_pos].is_word_start && allow_whitespace)))
        new_pos++;
    }

  return new_pos;
}


static int
gtk_text_move_backward_word (GtkText  *entry,
                             int       start,
                             gboolean  allow_whitespace)
{
  int new_pos = start;

  /* Prevent any leak of information */
  if (gtk_text_get_display_mode (entry) != DISPLAY_NORMAL)
    {
      new_pos = 0;
    }
  else if (start > 0)
    {
      PangoLayout *layout = gtk_text_ensure_layout (entry, FALSE);
      const PangoLogAttr *log_attrs;
      int n_attrs;

      log_attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

      new_pos = start - 1;

      /* Find the previous word boundary */
      while (new_pos > 0 && !(log_attrs[new_pos].is_word_start || 
                              (log_attrs[new_pos].is_word_end && allow_whitespace)))
        new_pos--;
    }

  return new_pos;
}

static void
gtk_text_delete_whitespace (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  PangoLayout *layout = gtk_text_ensure_layout (entry, FALSE);
  const PangoLogAttr *log_attrs;
  int n_attrs;
  int start, end;

  log_attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  start = end = priv->current_pos;

  while (start > 0 && log_attrs[start-1].is_white)
    start--;

  while (end < n_attrs && log_attrs[end].is_white)
    end++;

  if (start != end)
    gtk_editable_delete_text (GTK_EDITABLE (entry), start, end);
}


static void
gtk_text_select_word (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  int start_pos = gtk_text_move_backward_word (entry, priv->current_pos, TRUE);
  int end_pos = gtk_text_move_forward_word (entry, priv->current_pos, TRUE);

  gtk_editable_select_region (GTK_EDITABLE (entry), start_pos, end_pos);
}

static void
gtk_text_select_line (GtkText *entry)
{
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}

static int
truncate_multiline (const char *text)
{
  int length;

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
  GtkText *entry = GTK_TEXT (data);
  GtkEditable *editable = GTK_EDITABLE (entry);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  char *text;
  int pos, start, end;
  int length = -1;

  text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (clipboard), result, NULL);
  if (text == NULL)
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
      return;
    }

  if (priv->insert_pos >= 0)
    {
      pos = priv->insert_pos;
      gtk_editable_get_selection_bounds (editable, &start, &end);
      if (!((start <= pos && pos <= end) || (end <= pos && pos <= start)))
        gtk_editable_select_region (editable, pos, pos);
      priv->insert_pos = -1;
    }
      
  if (priv->truncate_multiline)
    length = truncate_multiline (text);

  begin_change (entry);
  if (gtk_editable_get_selection_bounds (editable, &start, &end))
    gtk_editable_delete_text (editable, start, end);

  pos = priv->current_pos;
  gtk_editable_insert_text (editable, text, length, &pos);
  gtk_editable_set_position (editable, pos);
  end_change (entry);

  g_free (text);
  g_object_unref (entry);
}

static void
gtk_text_paste (GtkText      *entry,
                GdkClipboard *clipboard)
{
  gdk_clipboard_read_text_async (clipboard, NULL, paste_received, g_object_ref (entry));
}

static void
gtk_text_update_primary_selection (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GdkClipboard *clipboard;
  int start, end;

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

/* Public API
 */

/**
 * gtk_text_new:
 *
 * Creates a new entry.
 *
 * Returns: a new #GtkText.
 */
GtkWidget *
gtk_text_new (void)
{
  return g_object_new (GTK_TYPE_TEXT, NULL);
}

/**
 * gtk_text_new_with_buffer:
 * @buffer: The buffer to use for the new #GtkText.
 *
 * Creates a new entry with the specified text buffer.
 *
 * Returns: a new #GtkText
 */
GtkWidget *
gtk_text_new_with_buffer (GtkEntryBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_ENTRY_BUFFER (buffer), NULL);

  return g_object_new (GTK_TYPE_TEXT, "buffer", buffer, NULL);
}

static GtkEntryBuffer *
get_buffer (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->buffer == NULL)
    {
      GtkEntryBuffer *buffer;
      buffer = gtk_entry_buffer_new (NULL, 0);
      gtk_text_set_buffer (entry, buffer);
      g_object_unref (buffer);
    }

  return priv->buffer;
}

/**
 * gtk_text_get_buffer:
 * @entry: a #GtkText
 *
 * Get the #GtkEntryBuffer object which holds the text for
 * this entry.
 *
 * Returns: (transfer none): A #GtkEntryBuffer object.
 */
GtkEntryBuffer *
gtk_text_get_buffer (GtkText *entry)
{
  g_return_val_if_fail (GTK_IS_TEXT (entry), NULL);

  return get_buffer (entry);
}

/**
 * gtk_text_set_buffer:
 * @entry: a #GtkText
 * @buffer: a #GtkEntryBuffer
 *
 * Set the #GtkEntryBuffer object which holds the text for
 * this widget.
 */
void
gtk_text_set_buffer (GtkText        *entry,
                     GtkEntryBuffer *buffer)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GObject *obj;
  gboolean had_buffer = FALSE;
  guint old_length = 0;
  guint new_length = 0;

  g_return_if_fail (GTK_IS_TEXT (entry));

  if (buffer)
    {
      g_return_if_fail (GTK_IS_ENTRY_BUFFER (buffer));
      g_object_ref (buffer);
    }

  if (priv->buffer)
    {
      had_buffer = TRUE;
      old_length = gtk_entry_buffer_get_length (priv->buffer);
      buffer_disconnect_signals (entry);
      g_object_unref (priv->buffer);
    }

  priv->buffer = buffer;

  if (priv->buffer)
    {
      new_length = gtk_entry_buffer_get_length (priv->buffer);
      buffer_connect_signals (entry);
    }

  obj = G_OBJECT (entry);
  g_object_freeze_notify (obj);
  g_object_notify_by_pspec (obj, entry_props[PROP_BUFFER]);
  g_object_notify_by_pspec (obj, entry_props[PROP_MAX_LENGTH]);
  if (old_length != 0 || new_length != 0)
    g_object_notify (obj, "text");

  if (had_buffer)
    {
      gtk_editable_set_position (GTK_EDITABLE (entry), 0);
      gtk_text_recompute (entry);
    }

  g_object_thaw_notify (obj);
}

static void
gtk_text_set_editable (GtkText  *entry,
                       gboolean  is_editable)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (entry));

  if (is_editable != priv->editable)
    {
      GtkWidget *widget = GTK_WIDGET (entry);

      if (!is_editable)
        {
          gtk_text_reset_im_context (entry);
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

      priv->editable = is_editable;

      if (is_editable && gtk_widget_has_focus (widget))
        gtk_im_context_focus_in (priv->im_context);

      gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (priv->key_controller),
                                               is_editable ? priv->im_context : NULL);

      g_object_notify (G_OBJECT (entry), "editable");
      gtk_widget_queue_draw (widget);
    }
}

static void
gtk_text_set_text (GtkText     *entry,
                   const char *text)
{
  int tmp_pos;

  g_return_if_fail (GTK_IS_TEXT (entry));
  g_return_if_fail (text != NULL);

  /* Actually setting the text will affect the cursor and selection;
   * if the contents don't actually change, this will look odd to the user.
   */
  if (strcmp (gtk_entry_buffer_get_text (get_buffer (entry)), text) == 0)
    return;

  begin_change (entry);
  g_object_freeze_notify (G_OBJECT (entry));
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);
  tmp_pos = 0;
  gtk_editable_insert_text (GTK_EDITABLE (entry), text, strlen (text), &tmp_pos);
  g_object_thaw_notify (G_OBJECT (entry));
  end_change (entry);
}

/**
 * gtk_text_set_visibility:
 * @entry: a #GtkText
 * @visible: %TRUE if the contents of the entry are displayed
 *           as plaintext
 *
 * Sets whether the contents of the entry are visible or not.
 * When visibility is set to %FALSE, characters are displayed
 * as the invisible char, and will also appear that way when
 * the text in the entry widget is copied to the clipboard.
 *
 * By default, GTK picks the best invisible character available
 * in the current font, but it can be changed with
 * gtk_text_set_invisible_char().
 *
 * Note that you probably want to set #GtkText:input-purpose
 * to %GTK_INPUT_PURPOSE_PASSWORD or %GTK_INPUT_PURPOSE_PIN to
 * inform input methods about the purpose of this entry,
 * in addition to setting visibility to %FALSE.
 */
void
gtk_text_set_visibility (GtkText  *entry,
                         gboolean  visible)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_if_fail (GTK_IS_TEXT (entry));

  visible = visible != FALSE;

  if (priv->visible != visible)
    {
      priv->visible = visible;

      g_object_notify (G_OBJECT (entry), "visibility");
      gtk_text_recompute (entry);
    }
}

/**
 * gtk_text_get_visibility:
 * @entry: a #GtkText
 *
 * Retrieves whether the text in @entry is visible.
 * See gtk_text_set_visibility().
 *
 * Returns: %TRUE if the text is currently visible
 **/
gboolean
gtk_text_get_visibility (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_TEXT (entry), FALSE);

  return priv->visible;
}

/**
 * gtk_text_set_invisible_char:
 * @entry: a #GtkText
 * @ch: a Unicode character
 * 
 * Sets the character to use in place of the actual text when
 * gtk_text_set_visibility() has been called to set text visibility
 * to %FALSE. i.e. this is the character used in “password mode” to
 * show the user how many characters have been typed.
 *
 * By default, GTK picks the best invisible char available in the
 * current font. If you set the invisible char to 0, then the user
 * will get no feedback at all; there will be no text on the screen
 * as they type.
 **/
void
gtk_text_set_invisible_char (GtkText  *entry,
                             gunichar  ch)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_if_fail (GTK_IS_TEXT (entry));

  if (!priv->invisible_char_set)
    {
      priv->invisible_char_set = TRUE;
      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_INVISIBLE_CHAR_SET]);
    }

  if (ch == priv->invisible_char)
    return;

  priv->invisible_char = ch;
  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_INVISIBLE_CHAR]);
  gtk_text_recompute (entry);
}

/**
 * gtk_text_get_invisible_char:
 * @entry: a #GtkText
 *
 * Retrieves the character displayed in place of the real characters
 * for entries with visibility set to false.
 * See gtk_text_set_invisible_char().
 *
 * Returns: the current invisible char, or 0, if the entry does not
 *               show invisible text at all. 
 **/
gunichar
gtk_text_get_invisible_char (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_TEXT (entry), 0);

  return priv->invisible_char;
}

/**
 * gtk_text_unset_invisible_char:
 * @entry: a #GtkText
 *
 * Unsets the invisible char previously set with
 * gtk_text_set_invisible_char(). So that the
 * default invisible char is used again.
 **/
void
gtk_text_unset_invisible_char (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  gunichar ch;

  g_return_if_fail (GTK_IS_TEXT (entry));

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
  gtk_text_recompute (entry);
}

/**
 * gtk_text_set_overwrite_mode:
 * @entry: a #GtkText
 * @overwrite: new value
 *
 * Sets whether the text is overwritten when typing in the #GtkText.
 **/
void
gtk_text_set_overwrite_mode (GtkText  *entry,
                             gboolean  overwrite)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_if_fail (GTK_IS_TEXT (entry));

  if (priv->overwrite_mode == overwrite)
    return;

  gtk_text_toggle_overwrite (entry);

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_OVERWRITE_MODE]);
}

/**
 * gtk_text_get_overwrite_mode:
 * @entry: a #GtkText
 *
 * Gets the value set by gtk_text_set_overwrite_mode().
 *
 * Returns: whether the text is overwritten when typing.
 **/
gboolean
gtk_text_get_overwrite_mode (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_TEXT (entry), FALSE);

  return priv->overwrite_mode;

}

/**
 * gtk_text_set_max_length:
 * @entry: a #GtkText
 * @length: the maximum length of the entry, or 0 for no maximum.
 *   (other than the maximum length of entries.) The value passed in will
 *   be clamped to the range 0-65536.
 *
 * Sets the maximum allowed length of the contents of the widget.
 *
 * If the current contents are longer than the given length, then
 * they will be truncated to fit.
 *
 * This is equivalent to getting @entry's #GtkEntryBuffer and
 * calling gtk_entry_buffer_set_max_length() on it.
 * ]|
 **/
void
gtk_text_set_max_length (GtkText *entry,
                         int      length)
{
  g_return_if_fail (GTK_IS_TEXT (entry));
  gtk_entry_buffer_set_max_length (get_buffer (entry), length);
}

/**
 * gtk_text_get_max_length:
 * @entry: a #GtkText
 *
 * Retrieves the maximum allowed length of the text in
 * @entry. See gtk_text_set_max_length().
 *
 * This is equivalent to getting @entry's #GtkEntryBuffer and
 * calling gtk_entry_buffer_get_max_length() on it.
 *
 * Returns: the maximum allowed number of characters
 *               in #GtkText, or 0 if there is no maximum.
 **/
int
gtk_text_get_max_length (GtkText *entry)
{
  g_return_val_if_fail (GTK_IS_TEXT (entry), 0);

  return gtk_entry_buffer_get_max_length (get_buffer (entry));
}

/**
 * gtk_text_get_text_length:
 * @entry: a #GtkText
 *
 * Retrieves the current length of the text in
 * @entry. 
 *
 * This is equivalent to getting @entry's #GtkEntryBuffer and
 * calling gtk_entry_buffer_get_length() on it.

 *
 * Returns: the current number of characters
 *               in #GtkText, or 0 if there are none.
 **/
guint16
gtk_text_get_text_length (GtkText *entry)
{
  g_return_val_if_fail (GTK_IS_TEXT (entry), 0);

  return gtk_entry_buffer_get_length (get_buffer (entry));
}

/**
 * gtk_text_set_activates_default:
 * @entry: a #GtkText
 * @activates: %TRUE to activate window’s default widget on Enter keypress
 *
 * If @activates is %TRUE, pressing Enter in the @entry will activate the default
 * widget for the window containing the entry. This usually means that
 * the dialog box containing the entry will be closed, since the default
 * widget is usually one of the dialog buttons.
 **/
void
gtk_text_set_activates_default (GtkText  *entry,
                                gboolean  activates)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_if_fail (GTK_IS_TEXT (entry));

  activates = activates != FALSE;

  if (priv->activates_default != activates)
    {
      priv->activates_default = activates;
      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_ACTIVATES_DEFAULT]);
    }
}

/**
 * gtk_text_get_activates_default:
 * @entry: a #GtkText
 *
 * Retrieves the value set by gtk_text_set_activates_default().
 *
 * Returns: %TRUE if the entry will activate the default widget
 */
gboolean
gtk_text_get_activates_default (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_TEXT (entry), FALSE);

  return priv->activates_default;
}

static void
gtk_text_set_width_chars (GtkText *entry,
                          int      n_chars)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->width_chars != n_chars)
    {
      priv->width_chars = n_chars;
      g_object_notify (G_OBJECT (entry), "width-chars");
      gtk_widget_queue_resize (GTK_WIDGET (entry));
    }
}

static void
gtk_text_set_max_width_chars (GtkText *entry,
                              int      n_chars)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->max_width_chars != n_chars)
    {
      priv->max_width_chars = n_chars;
      g_object_notify (G_OBJECT (entry), "max-width-chars");
      gtk_widget_queue_resize (GTK_WIDGET (entry));
    }
}

/**
 * gtk_text_set_has_frame:
 * @entry: a #GtkText
 * @has_frame: new value
 * 
 * Sets whether the entry has a beveled frame around it.
 **/
void
gtk_text_set_has_frame (GtkText  *entry,
                        gboolean  has_frame)
{
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_TEXT (entry));

  has_frame = (has_frame != FALSE);

  if (has_frame == gtk_text_get_has_frame (entry))
    return;

  context = gtk_widget_get_style_context (GTK_WIDGET (entry));
  if (has_frame)
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_FLAT);
  else
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_FLAT);

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_HAS_FRAME]);
}

/**
 * gtk_text_get_has_frame:
 * @entry: a #GtkText
 * 
 * Gets the value set by gtk_text_set_has_frame().
 * 
 * Returns: whether the entry has a beveled frame
 **/
gboolean
gtk_text_get_has_frame (GtkText *entry)
{
  GtkStyleContext *context;

  g_return_val_if_fail (GTK_IS_TEXT (entry), FALSE);

  context = gtk_widget_get_style_context (GTK_WIDGET (entry));

  return !gtk_style_context_has_class (context, GTK_STYLE_CLASS_FLAT);
}

PangoLayout *
gtk_text_get_layout (GtkText *entry)
{
  PangoLayout *layout;
  
  g_return_val_if_fail (GTK_IS_TEXT (entry), NULL);

  layout = gtk_text_ensure_layout (entry, TRUE);

  return layout;
}

void
gtk_text_get_layout_offsets (GtkText *entry,
                             int     *x,
                             int     *y)
{
  g_return_if_fail (GTK_IS_TEXT (entry));

  get_layout_position (entry, x, y);
}

static void
gtk_text_set_alignment (GtkText *entry,
                        float    xalign)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (xalign < 0.0)
    xalign = 0.0;
  else if (xalign > 1.0)
    xalign = 1.0;

  if (xalign != priv->xalign)
    {
      priv->xalign = xalign;
      gtk_text_recompute (entry);
      g_object_notify (G_OBJECT (entry), "xalign");
    }
}

/* Quick hack of a popup menu
 */
static void
activate_cb (GtkWidget *menuitem,
             GtkText   *entry)
{
  const char *signal;

  signal = g_object_get_qdata (G_OBJECT (menuitem), quark_gtk_signal);
  g_signal_emit_by_name (entry, signal);
}


static gboolean
gtk_text_mnemonic_activate (GtkWidget *widget,
                            gboolean   group_cycling)
{
  gtk_widget_grab_focus (widget);
  return GDK_EVENT_STOP;
}

static void
append_action_signal (GtkText     *entry,
                      GtkWidget   *menu,
                      const char  *label,
                      const char  *signal,
                      gboolean     sensitive)
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
  GtkText *entry_attach = GTK_TEXT (attach_widget);
  GtkTextPrivate *priv_attach = gtk_text_get_instance_private (entry_attach);

  priv_attach->popup_menu = NULL;
}

static void
gtk_text_do_popup (GtkText         *entry,
                    const GdkEvent *event)
{
  GtkTextPrivate *info_entry_priv = gtk_text_get_instance_private (entry);
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

      mode = gtk_text_get_display_mode (entry);
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
                                G_CALLBACK (gtk_text_delete_cb), entry);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      menuitem = gtk_separator_menu_item_new ();
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      menuitem = gtk_menu_item_new_with_mnemonic (_("Select _All"));
      gtk_widget_set_sensitive (menuitem, gtk_entry_buffer_get_length (info_entry_priv->buffer) > 0);
      g_signal_connect_swapped (menuitem, "activate",
                                G_CALLBACK (gtk_text_select_all), entry);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      if ((gtk_text_get_input_hints (entry) & GTK_INPUT_HINT_NO_EMOJI) == 0)
        {
          menuitem = gtk_menu_item_new_with_mnemonic (_("Insert _Emoji"));
          gtk_widget_set_sensitive (menuitem,
                                    mode == DISPLAY_NORMAL &&
                                    info_entry_priv->editable);
          g_signal_connect_swapped (menuitem, "activate",
                                    G_CALLBACK (gtk_text_insert_emoji), entry);
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
gtk_text_popup_menu (GtkWidget *widget)
{
  gtk_text_do_popup (GTK_TEXT (widget), NULL);
  return GDK_EVENT_STOP;
}

static void
show_or_hide_handles (GtkWidget  *popover,
                      GParamSpec *pspec,
                      GtkText    *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
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
                    GtkText   *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  const char *signal;

  signal = g_object_get_qdata (G_OBJECT (item), quark_gtk_signal);
  gtk_widget_hide (priv->selection_bubble);
  if (strcmp (signal, "select-all") == 0)
    gtk_text_select_all (entry);
  else
    g_signal_emit_by_name (entry, signal);
}

static void
append_bubble_action (GtkText     *entry,
                      GtkWidget   *toolbar,
                      const char  *label,
                      const char  *icon_name,
                      const char  *signal,
                      gboolean     sensitive)
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
gtk_text_selection_bubble_popup_show (gpointer user_data)
{
  GtkText *entry = user_data;
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  cairo_rectangle_int_t rect;
  GtkAllocation allocation;
  int start_x, end_x;
  gboolean has_selection;
  gboolean has_clipboard;
  gboolean all_selected;
  DisplayMode mode;
  GtkWidget *box;
  GtkWidget *toolbar;
  int length, start, end;
  GtkAllocation text_allocation;

  gtk_text_get_text_allocation (entry, &text_allocation);

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
  mode = gtk_text_get_display_mode (entry);

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

  gtk_text_get_cursor_locations (entry, &start_x, NULL);

  start_x -= priv->scroll_offset;
  start_x = CLAMP (start_x, 0, text_allocation.width);
  rect.y = text_allocation.y - allocation.y;
  rect.height = text_allocation.height;

  if (has_selection)
    {
      end_x = gtk_text_get_selection_bound_location (entry) - priv->scroll_offset;
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
gtk_text_selection_bubble_popup_unset (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->selection_bubble)
    gtk_widget_hide (priv->selection_bubble);

  if (priv->selection_bubble_timeout_id)
    {
      g_source_remove (priv->selection_bubble_timeout_id);
      priv->selection_bubble_timeout_id = 0;
    }
}

static void
gtk_text_selection_bubble_popup_set (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->selection_bubble_timeout_id)
    g_source_remove (priv->selection_bubble_timeout_id);

  priv->selection_bubble_timeout_id =
    g_timeout_add (50, gtk_text_selection_bubble_popup_show, entry);
  g_source_set_name_by_id (priv->selection_bubble_timeout_id, "[gtk] gtk_text_selection_bubble_popup_cb");
}

static void
gtk_text_drag_begin (GtkWidget *widget,
                     GdkDrag   *drag)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  char *text;

  text = _gtk_text_get_selected_text (entry);

  if (text)
    {
      int *ranges, n_ranges;
      GdkPaintable *paintable;

      paintable = gtk_text_util_create_drag_icon (widget, text, -1);
      gtk_text_get_pixel_ranges (entry, &ranges, &n_ranges);

      gtk_drag_set_icon_paintable (drag,
                                   paintable,
                                   priv->drag_start_x - ranges[0],
                                   priv->drag_start_y);

      g_free (ranges);
      g_object_unref (paintable);
      g_free (text);
    }
}

static void
gtk_text_drag_end (GtkWidget *widget,
                   GdkDrag   *drag)
{
}

static void
gtk_text_drag_leave (GtkWidget *widget,
                     GdkDrop   *drop)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  gtk_drag_unhighlight (widget);
  priv->dnd_position = -1;
  gtk_widget_queue_draw (widget);
}

static gboolean
gtk_text_drag_drop (GtkWidget *widget,
                    GdkDrop   *drop,
                    int        x,
                    int        y)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GdkAtom target = NULL;

  if (priv->editable)
    target = gtk_drag_dest_find_target (widget, drop, NULL);

  if (target != NULL)
    {
      priv->drop_position = gtk_text_find_position (entry, x + priv->scroll_offset);
      gtk_drag_get_data (widget, drop, target);
    }
  else
    gdk_drop_finish (drop, 0);
  
  return TRUE;
}

static gboolean
gtk_text_drag_motion (GtkWidget *widget,
                      GdkDrop   *drop,
                      int        x,
                      int        y)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GdkDragAction suggested_action;
  int new_position, old_position;
  int sel1, sel2;

  old_position = priv->dnd_position;
  new_position = gtk_text_find_position (entry, x + priv->scroll_offset);

  if (priv->editable &&
      gtk_drag_dest_find_target (widget, drop, NULL) != NULL)
    {
      suggested_action = GDK_ACTION_COPY | GDK_ACTION_MOVE;

      if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &sel1, &sel2) ||
          new_position < sel1 || new_position > sel2)
        {
          priv->dnd_position = new_position;
        }
      else
        {
          priv->dnd_position = -1;
        }
    }
  else
    {
      /* Entry not editable, or no text */
      suggested_action = 0;
      priv->dnd_position = -1;
    }

  gdk_drop_status (drop, suggested_action);
  if (suggested_action == 0)
    gtk_drag_unhighlight (widget);
  else
    gtk_drag_highlight (widget);

  if (priv->dnd_position != old_position)
    gtk_widget_queue_draw (widget);

  return TRUE;
}

static GdkDragAction
gtk_text_get_action (GtkText *entry,
                     GdkDrop *drop)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GdkDrag *drag = gdk_drop_get_drag (drop);
  GtkWidget *source_widget = gtk_drag_get_source_widget (drag);
  GdkDragAction actions;

  actions = gdk_drop_get_actions (drop);

  if (source_widget == widget &&
      actions & GDK_ACTION_MOVE)
    return GDK_ACTION_MOVE;

  if (actions & GDK_ACTION_COPY)
    return GDK_ACTION_COPY;

  if (actions & GDK_ACTION_MOVE)
    return GDK_ACTION_MOVE;

  return 0;
}

static void
gtk_text_drag_data_received (GtkWidget        *widget,
                             GdkDrop          *drop,
                             GtkSelectionData *selection_data)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (widget);
  GdkDragAction action;
  char *str;

  str = (char *) gtk_selection_data_get_text (selection_data);
  action = gtk_text_get_action (entry, drop);

  if (action && str && priv->editable)
    {
      int sel1, sel2;
      int length = -1;

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
      
      gdk_drop_finish (drop, action);
    }
  else
    {
      /* Drag and drop didn't happen! */
      gdk_drop_finish (drop, 0);
    }

  g_free (str);
}

static void
gtk_text_drag_data_get (GtkWidget        *widget,
                        GdkDrag          *drag,
                        GtkSelectionData *selection_data)
{
  GtkEditable *editable = GTK_EDITABLE (widget);
  int sel_start, sel_end;

  if (gtk_editable_get_selection_bounds (editable, &sel_start, &sel_end))
    {
      char *str = gtk_text_get_display_text (GTK_TEXT (widget), sel_start, sel_end);

      gtk_selection_data_set_text (selection_data, str, -1);
      
      g_free (str);
    }
}

static void
gtk_text_drag_data_delete (GtkWidget *widget,
                           GdkDrag   *drag)
{
  GtkText *entry = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkEditable *editable = GTK_EDITABLE (widget);
  int sel_start, sel_end;

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
cursor_blinks (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

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
get_middle_click_paste (GtkText *entry)
{
  GtkSettings *settings;
  gboolean paste;

  settings = gtk_widget_get_settings (GTK_WIDGET (entry));
  g_object_get (settings, "gtk-enable-primary-paste", &paste, NULL);

  return paste;
}

static int
get_cursor_time (GtkText *entry)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (entry));
  int time;

  g_object_get (settings, "gtk-cursor-blink-time", &time, NULL);

  return time;
}

static int
get_cursor_blink_timeout (GtkText *entry)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (entry));
  int timeout;

  g_object_get (settings, "gtk-cursor-blink-timeout", &timeout, NULL);

  return timeout;
}

static void
show_cursor (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
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
hide_cursor (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
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
static int
blink_cb (gpointer data)
{
  GtkText *entry = data;
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  int blink_timeout;

  if (!gtk_widget_has_focus (GTK_WIDGET (entry)))
    {
      g_warning ("GtkText - did not receive a focus-out event.\n"
                 "If you handle this event, you must return\n"
                 "GDK_EVENT_PROPAGATE so the entry gets the event as well");

      gtk_text_check_cursor_blink (entry);

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
      g_source_set_name_by_id (priv->blink_timeout, "[gtk] blink_cb");
    }
  else
    {
      show_cursor (entry);
      priv->blink_time += get_cursor_time (entry);
      priv->blink_timeout = g_timeout_add (get_cursor_time (entry) * CURSOR_ON_MULTIPLIER / CURSOR_DIVIDER,
                                           blink_cb,
                                           entry);
      g_source_set_name_by_id (priv->blink_timeout, "[gtk] blink_cb");
    }

  return G_SOURCE_REMOVE;
}

static void
gtk_text_check_cursor_blink (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (cursor_blinks (entry))
    {
      if (!priv->blink_timeout)
        {
          show_cursor (entry);
          priv->blink_timeout = g_timeout_add (get_cursor_time (entry) * CURSOR_ON_MULTIPLIER / CURSOR_DIVIDER,
                                               blink_cb,
                                               entry);
          g_source_set_name_by_id (priv->blink_timeout, "[gtk] blink_cb");
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
gtk_text_pend_cursor_blink (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (cursor_blinks (entry))
    {
      if (priv->blink_timeout != 0)
        g_source_remove (priv->blink_timeout);

      priv->blink_timeout = g_timeout_add (get_cursor_time (entry) * CURSOR_PEND_MULTIPLIER / CURSOR_DIVIDER,
                                           blink_cb,
                                           entry);
      g_source_set_name_by_id (priv->blink_timeout, "[gtk] blink_cb");
      show_cursor (entry);
    }
}

static void
gtk_text_reset_blink_time (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  priv->blink_time = 0;
}

/**
 * gtk_text_set_placeholder_text:
 * @entry: a #GtkText
 * @text: (nullable): a string to be displayed when @entry is empty and unfocused, or %NULL
 *
 * Sets text to be displayed in @entry when it is empty.
 *
 * This can be used to give a visual hint of the expected
 * contents of the entry.
 **/
void
gtk_text_set_placeholder_text (GtkText    *entry,
                               const char *text)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_if_fail (GTK_IS_TEXT (entry));

  if (priv->placeholder == NULL)
    {
      priv->placeholder = g_object_new (GTK_TYPE_LABEL,
                                        "label", text,
                                        "css-name", "placeholder",
                                        "xalign", 0.0f,
                                        "ellipsize", PANGO_ELLIPSIZE_END,
                                        NULL);
      gtk_widget_insert_after (priv->placeholder, GTK_WIDGET (entry), NULL);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (priv->placeholder), text);
    }

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_PLACEHOLDER_TEXT]);
}

/**
 * gtk_text_get_placeholder_text:
 * @entry: a #GtkText
 *
 * Retrieves the text that will be displayed when @entry is empty and unfocused
 *
 * Returns: (nullable) (transfer none):a pointer to the placeholder text as a string.
 *   This string points to internally allocated storage in the widget and must
 *   not be freed, modified or stored. If no placeholder text has been set,
 *   %NULL will be returned.
 **/
const char *
gtk_text_get_placeholder_text (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_TEXT (entry), NULL);

  if (!priv->placeholder)
    return NULL;

  return gtk_label_get_text (GTK_LABEL (priv->placeholder));
}

/**
 * gtk_text_set_input_purpose:
 * @entry: a #GtkText
 * @purpose: the purpose
 *
 * Sets the #GtkText:input-purpose property which
 * can be used by on-screen keyboards and other input
 * methods to adjust their behaviour.
 */
void
gtk_text_set_input_purpose (GtkText         *entry,
                            GtkInputPurpose  purpose)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_if_fail (GTK_IS_TEXT (entry));

  if (gtk_text_get_input_purpose (entry) != purpose)
    {
      g_object_set (G_OBJECT (priv->im_context),
                    "input-purpose", purpose,
                    NULL);

      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_INPUT_PURPOSE]);
    }
}

/**
 * gtk_text_get_input_purpose:
 * @entry: a #GtkText
 *
 * Gets the value of the #GtkText:input-purpose property.
 */
GtkInputPurpose
gtk_text_get_input_purpose (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkInputPurpose purpose;

  g_return_val_if_fail (GTK_IS_TEXT (entry), GTK_INPUT_PURPOSE_FREE_FORM);

  g_object_get (G_OBJECT (priv->im_context),
                "input-purpose", &purpose,
                NULL);

  return purpose;
}

/**
 * gtk_text_set_input_hints:
 * @entry: a #GtkText
 * @hints: the hints
 *
 * Sets the #GtkText:input-hints property, which
 * allows input methods to fine-tune their behaviour.
 */
void
gtk_text_set_input_hints (GtkText       *entry,
                          GtkInputHints  hints)

{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_if_fail (GTK_IS_TEXT (entry));

  if (gtk_text_get_input_hints (entry) != hints)
    {
      g_object_set (G_OBJECT (priv->im_context),
                    "input-hints", hints,
                    NULL);

      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_INPUT_HINTS]);
    }
}

/**
 * gtk_text_get_input_hints:
 * @entry: a #GtkText
 *
 * Gets the value of the #GtkText:input-hints property.
 */
GtkInputHints
gtk_text_get_input_hints (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  GtkInputHints hints;

  g_return_val_if_fail (GTK_IS_TEXT (entry), GTK_INPUT_HINT_NONE);

  g_object_get (G_OBJECT (priv->im_context),
                "input-hints", &hints,
                NULL);

  return hints;
}

/**
 * gtk_text_set_attributes:
 * @entry: a #GtkText
 * @attrs: a #PangoAttrList
 *
 * Sets a #PangoAttrList; the attributes in the list are applied to the
 * entry text.
 */
void
gtk_text_set_attributes (GtkText       *entry,
                         PangoAttrList *attrs)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);
  g_return_if_fail (GTK_IS_TEXT (entry));

  if (attrs)
    pango_attr_list_ref (attrs);

  if (priv->attrs)
    pango_attr_list_unref (priv->attrs);
  priv->attrs = attrs;

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_ATTRIBUTES]);

  gtk_text_recompute (entry);
  gtk_widget_queue_resize (GTK_WIDGET (entry));
}

/**
 * gtk_text_get_attributes:
 * @entry: a #GtkText
 *
 * Gets the attribute list that was set on the entry using
 * gtk_text_set_attributes(), if any.
 *
 * Returns: (transfer none) (nullable): the attribute list, or %NULL
 *     if none was set.
 */
PangoAttrList *
gtk_text_get_attributes (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_TEXT (entry), NULL);

  return priv->attrs;
}

/**
 * gtk_text_set_tabs:
 * @entry: a #GtkText
 * @tabs: (nullable): a #PangoTabArray
 *
 * Sets a #PangoTabArray; the tabstops in the array are applied to the entry
 * text.
 */

void
gtk_text_set_tabs (GtkText       *entry,
                   PangoTabArray *tabs)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_if_fail (GTK_IS_TEXT (entry));

  if (priv->tabs)
    pango_tab_array_free(priv->tabs);

  if (tabs)
    priv->tabs = pango_tab_array_copy (tabs);
  else
    priv->tabs = NULL;

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_TABS]);

  gtk_text_recompute (entry);
  gtk_widget_queue_resize (GTK_WIDGET (entry));
}

/**
 * gtk_text_get_tabs:
 * @entry: a #GtkText
 *
 * Gets the tabstops that were set on the entry using gtk_text_set_tabs(), if
 * any.
 *
 * Returns: (nullable) (transfer none): the tabstops, or %NULL if none was set.
 */
PangoTabArray *
gtk_text_get_tabs (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_TEXT (entry), NULL);

  return priv->tabs;
}

static void
gtk_text_insert_emoji (GtkText *entry)
{
  GtkWidget *chooser;

  if (gtk_widget_get_ancestor (GTK_WIDGET (entry), GTK_TYPE_EMOJI_CHOOSER) != NULL)
    return;

  chooser = GTK_WIDGET (g_object_get_data (G_OBJECT (entry), "gtk-emoji-chooser"));
  if (!chooser)
    {
      chooser = gtk_emoji_chooser_new ();
      g_object_set_data (G_OBJECT (entry), "gtk-emoji-chooser", chooser);

      gtk_popover_set_relative_to (GTK_POPOVER (chooser), GTK_WIDGET (entry));
      g_signal_connect_swapped (chooser, "emoji-picked", G_CALLBACK (gtk_text_enter_text), entry);
    }

  gtk_popover_popup (GTK_POPOVER (chooser));
}

static void
set_enable_emoji_completion (GtkText  *entry,
                             gboolean  value)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  if (priv->enable_emoji_completion == value)
    return;

  priv->enable_emoji_completion = value;

  if (priv->enable_emoji_completion)
    g_object_set_data (G_OBJECT (entry), "emoji-completion-popup",
                       gtk_emoji_completion_new (entry));
  else
    g_object_set_data (G_OBJECT (entry), "emoji-completion-popup", NULL);

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_ENABLE_EMOJI_COMPLETION]);
}

static void
set_text_cursor (GtkWidget *widget)
{
  gtk_widget_set_cursor_from_name (widget, "text");
}

GtkEventController *
gtk_text_get_key_controller (GtkText *entry)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (entry);

  return priv->key_controller;
}
