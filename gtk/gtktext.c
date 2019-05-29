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
#include "gtkeditable.h"
#include "gtkemojichooser.h"
#include "gtkemojicompletion.h"
#include "gtkentrybuffer.h"
#include "gtkeventcontrollerkey.h"
#include "gtkeventcontrollermotion.h"
#include "gtkgesturedrag.h"
#include "gtkgestureclick.h"
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
#include "gtknative.h"

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
 * text[.read-only]
 * ├── placeholder
 * ├── undershoot.left
 * ├── undershoot.right
 * ├── [selection]
 * ├── [block-cursor]
 * ╰── [window.popup]
 * ]|
 *
 * GtkText has a main node with the name text. Depending on the properties
 * of the widget, the .read-only style class may appear.
 *
 * When the entry has a selection, it adds a subnode with the name selection.
 *
 * When the entry is in overwrite mode, it adds a subnode with the name block-cursor
 * that determines how the block cursor is drawn.
 *
 * The CSS node for a context menu is added as a subnode below text as well.
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

  GtkWidget     *emoji_completion;
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
  guint         propagate_text_width    : 1;
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
  PROP_PROPAGATE_TEXT_WIDTH,
  NUM_PROPERTIES
};

static GParamSpec *text_props[NUM_PROPERTIES] = { NULL, };

static guint signals[LAST_SIGNAL] = { 0 };

typedef enum {
  CURSOR_STANDARD,
  CURSOR_DND
} CursorType;

typedef enum
{
  DISPLAY_NORMAL,       /* The text is being shown */
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
static void   gtk_text_root                 (GtkWidget        *widget);

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
static void        gtk_text_insert_text          (GtkText    *self,
                                                  const char *text,
                                                  int         length,
                                                  int        *position);
static void        gtk_text_delete_text          (GtkText    *self,
                                                  int         start_pos,
                                                  int         end_pos);
static void        gtk_text_delete_selection     (GtkText    *self);
static void        gtk_text_set_selection_bounds (GtkText    *self,
                                                  int         start,
                                                  int         end);
static gboolean    gtk_text_get_selection_bounds (GtkText    *self,
                                                  int        *start,
                                                  int        *end);

static void        gtk_text_set_editable         (GtkText    *self,
                                                  gboolean    is_editable);
static void        gtk_text_set_text             (GtkText    *self,
                                                  const char *text);
static void        gtk_text_set_width_chars      (GtkText    *self,
                                                  int         n_chars); 
static void        gtk_text_set_max_width_chars  (GtkText    *self,
                                                  int         n_chars); 
static void        gtk_text_set_alignment        (GtkText    *self,
                                                  float       xalign);

/* Default signal handlers
 */
static gboolean gtk_text_popup_menu         (GtkWidget       *widget);
static void     gtk_text_move_cursor        (GtkText         *self,
                                             GtkMovementStep  step,
                                             int              count,
                                             gboolean         extend);
static void     gtk_text_insert_at_cursor   (GtkText         *self,
                                             const char      *str);
static void     gtk_text_delete_from_cursor (GtkText         *self,
                                             GtkDeleteType    type,
                                             int              count);
static void     gtk_text_backspace          (GtkText         *self);
static void     gtk_text_cut_clipboard      (GtkText         *self);
static void     gtk_text_copy_clipboard     (GtkText         *self);
static void     gtk_text_paste_clipboard    (GtkText         *self);
static void     gtk_text_toggle_overwrite   (GtkText         *self);
static void     gtk_text_insert_emoji       (GtkText         *self);
static void     gtk_text_select_all         (GtkText         *self);
static void     gtk_text_real_activate      (GtkText         *self);
 
static void     keymap_direction_changed    (GdkKeymap       *keymap,
                                             GtkText         *self);

/* IM Context Callbacks
 */
static void     gtk_text_commit_cb               (GtkIMContext *context,
                                                  const char   *str,
                                                  GtkText      *self);
static void     gtk_text_preedit_changed_cb      (GtkIMContext *context,
                                                  GtkText      *self);
static gboolean gtk_text_retrieve_surrounding_cb (GtkIMContext *context,
                                                  GtkText      *self);
static gboolean gtk_text_delete_surrounding_cb   (GtkIMContext *context,
                                                  int           offset,
                                                  int           n_chars,
                                                  GtkText      *self);

/* Entry buffer signal handlers
 */
static void buffer_inserted_text     (GtkEntryBuffer *buffer, 
                                      guint           position,
                                      const char     *chars,
                                      guint           n_chars,
                                      GtkText        *self);
static void buffer_deleted_text      (GtkEntryBuffer *buffer, 
                                      guint           position,
                                      guint           n_chars,
                                      GtkText        *self);
static void buffer_notify_text       (GtkEntryBuffer *buffer, 
                                      GParamSpec     *spec,
                                      GtkText        *self);
static void buffer_notify_max_length (GtkEntryBuffer *buffer, 
                                      GParamSpec     *spec,
                                      GtkText        *self);

/* Event controller callbacks
 */
static void     gtk_text_motion_controller_motion   (GtkEventControllerMotion *controller,
                                                     double                    x,
                                                     double                    y,
                                                     GtkText                  *self);
static void     gtk_text_click_gesture_pressed (GtkGestureClick     *gesture,
                                                     int                       n_press,
                                                     double                    x,
                                                     double                    y,
                                                     GtkText                  *self);
static void     gtk_text_drag_gesture_update        (GtkGestureDrag           *gesture,
                                                     double                    offset_x,
                                                     double                    offset_y,
                                                     GtkText                  *self);
static void     gtk_text_drag_gesture_end           (GtkGestureDrag           *gesture,
                                                     double                     offset_x,
                                                     double                    offset_y,
                                                     GtkText                  *self);
static gboolean gtk_text_key_controller_key_pressed  (GtkEventControllerKey   *controller,
                                                      guint                    keyval,
                                                      guint                    keycode,
                                                      GdkModifierType          state,
                                                      GtkText                 *self);


/* GtkTextHandle handlers */
static void gtk_text_handle_drag_started  (GtkTextHandle          *handle,
                                           GtkTextHandlePosition   pos,
                                           GtkText                *self);
static void gtk_text_handle_dragged       (GtkTextHandle          *handle,
                                           GtkTextHandlePosition   pos,
                                           int                     x,
                                           int                     y,
                                           GtkText                *self);
static void gtk_text_handle_drag_finished (GtkTextHandle          *handle,
                                           GtkTextHandlePosition   pos,
                                           GtkText                *self);

/* Internal routines
 */
static void         gtk_text_draw_text                (GtkText       *self,
                                                       GtkSnapshot   *snapshot);
static void         gtk_text_draw_cursor              (GtkText       *self,
                                                       GtkSnapshot   *snapshot,
                                                       CursorType     type);
static PangoLayout *gtk_text_ensure_layout            (GtkText       *self,
                                                       gboolean       include_preedit);
static void         gtk_text_reset_layout             (GtkText       *self);
static void         gtk_text_recompute                (GtkText       *self);
static int          gtk_text_find_position            (GtkText       *self,
                                                       int            x);
static void         gtk_text_get_cursor_locations     (GtkText       *self,
                                                       int           *strong_x,
                                                       int           *weak_x);
static void         gtk_text_adjust_scroll            (GtkText       *self);
static int          gtk_text_move_visually            (GtkText        *editable,
                                                       int             start,
                                                       int             count);
static int          gtk_text_move_logically           (GtkText        *self,
                                                       int             start,
                                                       int             count);
static int          gtk_text_move_forward_word        (GtkText        *self,
                                                       int             start,
                                                       gboolean        allow_whitespace);
static int          gtk_text_move_backward_word       (GtkText        *self,
                                                       int             start,
                                                       gboolean        allow_whitespace);
static void         gtk_text_delete_whitespace        (GtkText        *self);
static void         gtk_text_select_word              (GtkText        *self);
static void         gtk_text_select_line              (GtkText        *self);
static void         gtk_text_paste                    (GtkText        *self,
                                                       GdkClipboard   *clipboard);
static void         gtk_text_update_primary_selection (GtkText        *self);
static void         gtk_text_schedule_im_reset        (GtkText        *self);
static void         gtk_text_do_popup                 (GtkText        *self,
                                                       const GdkEvent *event);
static gboolean     gtk_text_mnemonic_activate        (GtkWidget      *widget,
                                                       gboolean        group_cycling);
static void         gtk_text_check_cursor_blink       (GtkText        *self);
static void         gtk_text_pend_cursor_blink        (GtkText        *self);
static void         gtk_text_reset_blink_time         (GtkText        *self);
static void         gtk_text_update_cached_style_values(GtkText       *self);
static gboolean     get_middle_click_paste             (GtkText       *self);
static void         gtk_text_get_scroll_limits        (GtkText        *self,
                                                       int            *min_offset,
                                                       int            *max_offset);
static GtkEntryBuffer *get_buffer                      (GtkText       *self);
static void         set_enable_emoji_completion        (GtkText       *self,
                                                        gboolean       value);
static void         set_text_cursor                    (GtkWidget     *widget);
static void         update_placeholder_visibility      (GtkText       *self);

static void         buffer_connect_signals             (GtkText       *self);
static void         buffer_disconnect_signals          (GtkText       *self);

static void         gtk_text_selection_bubble_popup_set   (GtkText *self);
static void         gtk_text_selection_bubble_popup_unset (GtkText *self);

static void         begin_change                       (GtkText *self);
static void         end_change                         (GtkText *self);
static void         emit_changed                       (GtkText *self);


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

  GtkText *self;
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

      if (gtk_text_get_selection_bounds (content->self, &start, &end))
        {
          char *str = gtk_text_get_display_text (content->self, start, end);
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

  gtk_text_get_selection_bounds (content->self, &current_pos, &selection_bound);
  gtk_text_set_selection_bounds (content->self, current_pos, current_pos);
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
  widget_class->root = gtk_text_root;
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

  text_props[PROP_BUFFER] =
      g_param_spec_object ("buffer",
                           P_("Text Buffer"),
                           P_("Text buffer object which actually stores self text"),
                           GTK_TYPE_ENTRY_BUFFER,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  text_props[PROP_MAX_LENGTH] =
      g_param_spec_int ("max-length",
                        P_("Maximum length"),
                        P_("Maximum number of characters for this self. Zero if no maximum"),
                        0, GTK_ENTRY_BUFFER_MAX_SIZE,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  text_props[PROP_INVISIBLE_CHAR] =
      g_param_spec_unichar ("invisible-char",
                            P_("Invisible character"),
                            P_("The character to use when masking self contents (in “password mode”)"),
                            '*',
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  text_props[PROP_ACTIVATES_DEFAULT] =
      g_param_spec_boolean ("activates-default",
                            P_("Activates default"),
                            P_("Whether to activate the default widget (such as the default button in a dialog) when Enter is pressed"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  text_props[PROP_SCROLL_OFFSET] =
      g_param_spec_int ("scroll-offset",
                        P_("Scroll offset"),
                        P_("Number of pixels of the self scrolled off the screen to the left"),
                        0, G_MAXINT,
                        0,
                        GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:truncate-multiline:
   *
   * When %TRUE, pasted multi-line text is truncated to the first line.
   */
  text_props[PROP_TRUNCATE_MULTILINE] =
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
  text_props[PROP_OVERWRITE_MODE] =
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
  text_props[PROP_INVISIBLE_CHAR_SET] =
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
  text_props[PROP_PLACEHOLDER_TEXT] =
      g_param_spec_string ("placeholder-text",
                           P_("Placeholder text"),
                           P_("Show text in the self when it’s empty and unfocused"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:im-module:
   *
   * Which IM (input method) module should be used for this self.
   * See #GtkIMContext.
   *
   * Setting this to a non-%NULL value overrides the
   * system-wide IM module setting. See the GtkSettings
   * #GtkSettings:gtk-im-module property.
   */
  text_props[PROP_IM_MODULE] =
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
  text_props[PROP_INPUT_PURPOSE] =
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
  text_props[PROP_INPUT_HINTS] =
      g_param_spec_flags ("input-hints",
                          P_("hints"),
                          P_("Hints for the text field behaviour"),
                          GTK_TYPE_INPUT_HINTS,
                          GTK_INPUT_HINT_NONE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:attributes:
   *
   * A list of Pango attributes to apply to the text of the self.
   *
   * This is mainly useful to change the size or weight of the text.
   *
   * The #PangoAttribute's @start_index and @end_index must refer to the
   * #GtkEntryBuffer text, i.e. without the preedit string.
   */
  text_props[PROP_ATTRIBUTES] =
      g_param_spec_boxed ("attributes",
                          P_("Attributes"),
                          P_("A list of style attributes to apply to the text of the self"),
                          PANGO_TYPE_ATTR_LIST,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:populate-all:
   *
   * If :populate-all is %TRUE, the #GtkText::populate-popup
   * signal is also emitted for touch popups.
   */
  text_props[PROP_POPULATE_ALL] =
      g_param_spec_boolean ("populate-all",
                            P_("Populate all"),
                            P_("Whether to emit ::populate-popup for touch popups"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText::tabs:
   *
   * A list of tabstops to apply to the text of the self.
   */
  text_props[PROP_TABS] =
      g_param_spec_boxed ("tabs",
                          P_("Tabs"),
                          P_("A list of tabstop locations to apply to the text of the self"),
                          PANGO_TYPE_TAB_ARRAY,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  text_props[PROP_ENABLE_EMOJI_COMPLETION] =
      g_param_spec_boolean ("enable-emoji-completion",
                            P_("Enable Emoji completion"),
                            P_("Whether to suggest Emoji replacements"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  text_props[PROP_VISIBILITY] =
      g_param_spec_boolean ("visibility",
                            P_("Visibility"),
                            P_("FALSE displays the “invisible char” instead of the actual text (password mode)"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  text_props[PROP_PROPAGATE_TEXT_WIDTH] =
      g_param_spec_boolean ("propagate-text-width",
                            P_("Propagate text width"),
                            P_("Whether the entry should grow and shrink with the content"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, text_props);

  gtk_editable_install_properties (gobject_class, NUM_PROPERTIES);

  /**
   * GtkText::populate-popup:
   * @self: The self on which the signal is emitted
   * @widget: the container that is being populated
   *
   * The ::populate-popup signal gets emitted before showing the
   * context menu of the self.
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
   * @self: The self on which the signal is emitted
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
   * @self: the object which received the signal
   * @step: the granularity of the move, as a #GtkMovementStep
   * @count: the number of @step units to move
   * @extend: %TRUE if the move should extend the selection
   *
   * The ::move-cursor signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user initiates a cursor movement.
   * If the cursor is not visible in @self, this signal causes
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
   * @self: the object which received the signal
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
   * @self: the object which received the signal
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
   * @self: the object which received the signal
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
   * @self: the object which received the signal
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
   * @self: the object which received the signal
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
   * @self: the object which received the signal
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
   * @self: the object which received the signal
   *
   * The ::toggle-overwrite signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to toggle the overwrite mode of the self.
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
   * @self: the object which received the signal
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
   * @self: the object which received the signal
   *
   * The ::insert-emoji signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to present the Emoji chooser for the @self.
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
editable_insert_text (GtkEditable *editable,
                      const char  *text,
                      int          length,
                      int         *position)
{
  gtk_text_insert_text (GTK_TEXT (editable), text, length, position);
}

static void
editable_delete_text (GtkEditable *editable,
                      int          start_pos,
                      int          end_pos)
{
  gtk_text_delete_text (GTK_TEXT (editable), start_pos, end_pos);
}

static const char *
editable_get_text (GtkEditable *editable)
{
  return gtk_entry_buffer_get_text (get_buffer (GTK_TEXT (editable)));
}

static void
editable_set_selection_bounds (GtkEditable *editable,
                               int          start_pos,
                               int          end_pos)
{
  gtk_text_set_selection_bounds (GTK_TEXT (editable), start_pos, end_pos);
}

static gboolean
editable_get_selection_bounds (GtkEditable *editable,
                               int         *start_pos,
                               int         *end_pos)
{
  return gtk_text_get_selection_bounds (GTK_TEXT (editable), start_pos, end_pos);
}

static void
gtk_text_editable_init (GtkEditableInterface *iface)
{
  iface->insert_text = editable_insert_text;
  iface->delete_text = editable_delete_text;
  iface->get_text = editable_get_text;
  iface->set_selection_bounds = editable_set_selection_bounds;
  iface->get_selection_bounds = editable_get_selection_bounds;
}

static void
gtk_text_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtkText *self = GTK_TEXT (object);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  switch (prop_id)
    {
    /* GtkEditable properties */
    case NUM_PROPERTIES + GTK_EDITABLE_PROP_EDITABLE:
      gtk_text_set_editable (self, g_value_get_boolean (value));
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_WIDTH_CHARS:
      gtk_text_set_width_chars (self, g_value_get_int (value));
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_MAX_WIDTH_CHARS:
      gtk_text_set_max_width_chars (self, g_value_get_int (value));
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_TEXT:
      gtk_text_set_text (self, g_value_get_string (value));
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_XALIGN:
      gtk_text_set_alignment (self, g_value_get_float (value));
      break;

    /* GtkText properties */
    case PROP_BUFFER:
      gtk_text_set_buffer (self, g_value_get_object (value));
      break;

    case PROP_MAX_LENGTH:
      gtk_text_set_max_length (self, g_value_get_int (value));
      break;

    case PROP_VISIBILITY:
      gtk_text_set_visibility (self, g_value_get_boolean (value));
      break;

    case PROP_INVISIBLE_CHAR:
      gtk_text_set_invisible_char (self, g_value_get_uint (value));
      break;

    case PROP_ACTIVATES_DEFAULT:
      gtk_text_set_activates_default (self, g_value_get_boolean (value));
      break;

    case PROP_TRUNCATE_MULTILINE:
      if (priv->truncate_multiline != g_value_get_boolean (value))
        {
          priv->truncate_multiline = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_OVERWRITE_MODE:
      gtk_text_set_overwrite_mode (self, g_value_get_boolean (value));
      break;

    case PROP_INVISIBLE_CHAR_SET:
      if (g_value_get_boolean (value))
        priv->invisible_char_set = TRUE;
      else
        gtk_text_unset_invisible_char (self);
      break;

    case PROP_PLACEHOLDER_TEXT:
      gtk_text_set_placeholder_text (self, g_value_get_string (value));
      break;

    case PROP_IM_MODULE:
      g_free (priv->im_module);
      priv->im_module = g_value_dup_string (value);
      if (GTK_IS_IM_MULTICONTEXT (priv->im_context))
        gtk_im_multicontext_set_context_id (GTK_IM_MULTICONTEXT (priv->im_context), priv->im_module);
      g_object_notify_by_pspec (object, pspec);
      break;

    case PROP_INPUT_PURPOSE:
      gtk_text_set_input_purpose (self, g_value_get_enum (value));
      break;

    case PROP_INPUT_HINTS:
      gtk_text_set_input_hints (self, g_value_get_flags (value));
      break;

    case PROP_ATTRIBUTES:
      gtk_text_set_attributes (self, g_value_get_boxed (value));
      break;

    case PROP_POPULATE_ALL:
      if (priv->populate_all != g_value_get_boolean (value))
        {
          priv->populate_all = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_TABS:
      gtk_text_set_tabs (self, g_value_get_boxed (value));
      break;

    case PROP_ENABLE_EMOJI_COMPLETION:
      set_enable_emoji_completion (self, g_value_get_boolean (value));
      break;

    case PROP_PROPAGATE_TEXT_WIDTH:
      if (priv->propagate_text_width != g_value_get_boolean (value))
        {
          priv->propagate_text_width = g_value_get_boolean (value);
          gtk_widget_queue_resize (GTK_WIDGET (self));
          g_object_notify_by_pspec (object, pspec);
        }
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
  GtkText *self = GTK_TEXT (object);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  switch (prop_id)
    {
    /* GtkEditable properties */
    case NUM_PROPERTIES + GTK_EDITABLE_PROP_CURSOR_POSITION:
      g_value_set_int (value, priv->current_pos);
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_SELECTION_BOUND:
      g_value_set_int (value, priv->selection_bound);
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_EDITABLE:
      g_value_set_boolean (value, priv->editable);
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_WIDTH_CHARS:
      g_value_set_int (value, priv->width_chars);
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_MAX_WIDTH_CHARS:
      g_value_set_int (value, priv->max_width_chars);
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_TEXT:
      g_value_set_string (value, gtk_entry_buffer_get_text (get_buffer (self)));
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_XALIGN:
      g_value_set_float (value, priv->xalign);
      break;

    /* GtkText properties */
    case PROP_BUFFER:
      g_value_set_object (value, get_buffer (self));
      break;

    case PROP_MAX_LENGTH:
      g_value_set_int (value, gtk_entry_buffer_get_max_length (get_buffer (self)));
      break;

    case PROP_VISIBILITY:
      g_value_set_boolean (value, priv->visible);
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
      g_value_set_string (value, gtk_text_get_placeholder_text (self));
      break;

    case PROP_INPUT_PURPOSE:
      g_value_set_enum (value, gtk_text_get_input_purpose (self));
      break;

    case PROP_INPUT_HINTS:
      g_value_set_flags (value, gtk_text_get_input_hints (self));
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

    case PROP_PROPAGATE_TEXT_WIDTH:
      g_value_set_boolean (value, priv->propagate_text_width);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_text_init (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkCssNode *widget_node;
  GtkGesture *gesture;
  GtkEventController *controller;
  int i;

  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);

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
  GTK_TEXT_CONTENT (priv->selection_content)->self = self;

  gtk_drag_dest_set (GTK_WIDGET (self), 0, NULL,
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_drag_dest_add_text_targets (GTK_WIDGET (self));

  /* This object is completely private. No external entity can gain a reference
   * to it; so we create it here and destroy it in finalize().
   */
  priv->im_context = gtk_im_multicontext_new ();

  g_signal_connect (priv->im_context, "commit",
                    G_CALLBACK (gtk_text_commit_cb), self);
  g_signal_connect (priv->im_context, "preedit-changed",
                    G_CALLBACK (gtk_text_preedit_changed_cb), self);
  g_signal_connect (priv->im_context, "retrieve-surrounding",
                    G_CALLBACK (gtk_text_retrieve_surrounding_cb), self);
  g_signal_connect (priv->im_context, "delete-surrounding",
                    G_CALLBACK (gtk_text_delete_surrounding_cb), self);

  gtk_text_update_cached_style_values (self);

  priv->drag_gesture = gtk_gesture_drag_new ();
  g_signal_connect (priv->drag_gesture, "drag-update",
                    G_CALLBACK (gtk_text_drag_gesture_update), self);
  g_signal_connect (priv->drag_gesture, "drag-end",
                    G_CALLBACK (gtk_text_drag_gesture_end), self);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->drag_gesture), 0);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (priv->drag_gesture), TRUE);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (priv->drag_gesture));

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_text_click_gesture_pressed), self);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion",
                    G_CALLBACK (gtk_text_motion_controller_motion), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  priv->key_controller = gtk_event_controller_key_new ();
  g_signal_connect (priv->key_controller, "key-pressed",
                    G_CALLBACK (gtk_text_key_controller_key_pressed), self);
  g_signal_connect_swapped (priv->key_controller, "im-update",
                            G_CALLBACK (gtk_text_schedule_im_reset), self);
  g_signal_connect_swapped (priv->key_controller, "focus-in",
                            G_CALLBACK (gtk_text_focus_in), self);
  g_signal_connect_swapped (priv->key_controller, "focus-out",
                            G_CALLBACK (gtk_text_focus_out), self);
  gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (priv->key_controller),
                                           priv->im_context);
  gtk_widget_add_controller (GTK_WIDGET (self), priv->key_controller);

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
  for (i = 0; i < 2; i++)
    {
      priv->undershoot_node[i] = gtk_css_node_new ();
      gtk_css_node_set_name (priv->undershoot_node[i], I_("undershoot"));
      gtk_css_node_add_class (priv->undershoot_node[i], g_quark_from_static_string (i == 0 ? GTK_STYLE_CLASS_LEFT : GTK_STYLE_CLASS_RIGHT));
      gtk_css_node_set_parent (priv->undershoot_node[i], widget_node);
      gtk_css_node_set_state (priv->undershoot_node[i], gtk_css_node_get_state (widget_node) & ~GTK_STATE_FLAG_DROP_ACTIVE);
      g_object_unref (priv->undershoot_node[i]);
    }

  set_text_cursor (GTK_WIDGET (self));
}

static void
gtk_text_destroy (GtkWidget *widget)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  priv->current_pos = priv->selection_bound = 0;
  gtk_text_reset_im_context (self);
  gtk_text_reset_layout (self);

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
  GtkText *self = GTK_TEXT (object);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkKeymap *keymap;

  priv->current_pos = 0;

  if (priv->buffer)
    {
      buffer_disconnect_signals (self);
      g_object_unref (priv->buffer);
      priv->buffer = NULL;
    }

  g_clear_pointer (&priv->emoji_completion, gtk_widget_unparent);

  keymap = gdk_display_get_keymap (gtk_widget_get_display (GTK_WIDGET (object)));
  g_signal_handlers_disconnect_by_func (keymap, keymap_direction_changed, self);

  G_OBJECT_CLASS (gtk_text_parent_class)->dispose (object);
}

static void
gtk_text_finalize (GObject *object)
{
  GtkText *self = GTK_TEXT (object);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

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
gtk_text_ensure_magnifier (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->magnifier_popover)
    return;

  priv->magnifier = _gtk_magnifier_new (GTK_WIDGET (self));
  gtk_widget_set_size_request (priv->magnifier, 100, 60);
  _gtk_magnifier_set_magnification (GTK_MAGNIFIER (priv->magnifier), 2.0);
  priv->magnifier_popover = gtk_popover_new (GTK_WIDGET (self));
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->magnifier_popover),
                               "magnifier");
  gtk_popover_set_autohide (GTK_POPOVER (priv->magnifier_popover), FALSE);
  gtk_container_add (GTK_CONTAINER (priv->magnifier_popover),
                     priv->magnifier);
  gtk_widget_show (priv->magnifier);
}

static void
gtk_text_ensure_text_handles (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->text_handle)
    return;

  priv->text_handle = _gtk_text_handle_new (GTK_WIDGET (self));
  g_signal_connect (priv->text_handle, "drag-started",
                    G_CALLBACK (gtk_text_handle_drag_started), self);
  g_signal_connect (priv->text_handle, "handle-dragged",
                    G_CALLBACK (gtk_text_handle_dragged), self);
  g_signal_connect (priv->text_handle, "drag-finished",
                    G_CALLBACK (gtk_text_handle_drag_finished), self);
}

static void
begin_change (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  priv->change_count++;

  g_object_freeze_notify (G_OBJECT (self));
}

static void
end_change (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (priv->change_count > 0);

  g_object_thaw_notify (G_OBJECT (self));

  priv->change_count--;

  if (priv->change_count == 0)
    {
       if (priv->real_changed)
         {
           g_signal_emit_by_name (self, "changed");
           priv->real_changed = FALSE;
         }
    }
}

static void
emit_changed (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->change_count == 0)
    g_signal_emit_by_name (self, "changed");
  else
    priv->real_changed = TRUE;
}

static DisplayMode
gtk_text_get_display_mode (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->visible)
    return DISPLAY_NORMAL;

  if (priv->invisible_char == 0 && priv->invisible_char_set)
    return DISPLAY_BLANK;

  return DISPLAY_INVISIBLE;
}

char *
gtk_text_get_display_text (GtkText *self,
                           int      start_pos,
                           int      end_pos)
{
  GtkTextPasswordHint *password_hint;
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  gunichar invisible_char;
  const char *start;
  const char *end;
  const char *text;
  char char_str[7];
  int char_len;
  GString *str;
  guint length;
  int i;

  text = gtk_entry_buffer_get_text (get_buffer (self));
  length = gtk_entry_buffer_get_length (get_buffer (self));

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

      password_hint = g_object_get_qdata (G_OBJECT (self), quark_password_hint);
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
update_node_state (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkStateFlags state;

  state = gtk_widget_get_state_flags (GTK_WIDGET (self));
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
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->text_handle)
    _gtk_text_handle_set_mode (priv->text_handle,
                               GTK_TEXT_HANDLE_MODE_NONE);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->unmap (widget);
}

static void
gtk_text_get_text_allocation (GtkText      *self,
                              GdkRectangle *allocation)
{
  allocation->x = 0;
  allocation->y = 0;
  allocation->width = gtk_widget_get_width (GTK_WIDGET (self));
  allocation->height = gtk_widget_get_height (GTK_WIDGET (self));
}

static void
gtk_text_realize (GtkWidget *widget)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->realize (widget);

  gtk_im_context_set_client_widget (priv->im_context, widget);

  gtk_text_adjust_scroll (self);
  gtk_text_update_primary_selection (self);
}

static void
gtk_text_unrealize (GtkWidget *widget)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkClipboard *clipboard;

  gtk_text_reset_layout (self);
  
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
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  PangoContext *context;
  PangoFontMetrics *metrics;

  context = gtk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (context, NULL, NULL);

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

      if (priv->propagate_text_width)
        {
          PangoLayout *layout;
          int act;

          layout = gtk_text_ensure_layout (self, TRUE);
          pango_layout_get_pixel_size (layout, &act, NULL);

          nat = MIN (act, nat);
        }

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

      layout = gtk_text_ensure_layout (self, TRUE);

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
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkEmojiChooser *chooser;

  priv->text_baseline = baseline;

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
    gtk_text_recompute (self);

  chooser = g_object_get_data (G_OBJECT (self), "gtk-emoji-chooser");
  if (chooser)
    gtk_native_check_resize (GTK_NATIVE (chooser));

  if (priv->emoji_completion)
    gtk_native_check_resize (GTK_NATIVE (priv->emoji_completion));

  if (priv->magnifier_popover)
    gtk_native_check_resize (GTK_NATIVE (priv->magnifier_popover));
}

static void
gtk_text_draw_undershoot (GtkText     *self,
                          GtkSnapshot *snapshot)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkStyleContext *context;
  int min_offset, max_offset;
  GdkRectangle rect;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  gtk_text_get_scroll_limits (self, &min_offset, &max_offset);

  gtk_text_get_text_allocation (self, &rect);

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
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  /* Draw text and cursor */
  if (priv->dnd_position != -1)
    gtk_text_draw_cursor (GTK_TEXT (widget), snapshot, CURSOR_DND);

  if (priv->placeholder)
    gtk_widget_snapshot_child (widget, priv->placeholder, snapshot);

  gtk_text_draw_text (GTK_TEXT (widget), snapshot);

  /* When no text is being displayed at all, don't show the cursor */
  if (gtk_text_get_display_mode (self) != DISPLAY_BLANK &&
      gtk_widget_has_focus (widget) &&
      priv->selection_bound == priv->current_pos && priv->cursor_visible)
    gtk_text_draw_cursor (GTK_TEXT (widget), snapshot, CURSOR_STANDARD);

  gtk_text_draw_undershoot (self, snapshot);
}

static void
gtk_text_get_pixel_ranges (GtkText  *self,
                           int     **ranges,
                           int      *n_ranges)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->selection_bound != priv->current_pos)
    {
      PangoLayout *layout = gtk_text_ensure_layout (self, TRUE);
      PangoLayoutLine *line = pango_layout_get_lines_readonly (layout)->data;
      const char *text = pango_layout_get_text (layout);
      int start_index = g_utf8_offset_to_pointer (text, priv->selection_bound) - text;
      int end_index = g_utf8_offset_to_pointer (text, priv->current_pos) - text;
      int real_n_ranges, i;

      pango_layout_line_get_x_ranges (line,
                                      MIN (start_index, end_index),
                                      MAX (start_index, end_index),
                                      ranges,
                                      &real_n_ranges);

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
in_selection (GtkText *self,
              int      x)
{
  int *ranges;
  int n_ranges, i;
  int retval = FALSE;

  gtk_text_get_pixel_ranges (self, &ranges, &n_ranges);

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
gtk_text_move_handle (GtkText               *self,
                      GtkTextHandlePosition  pos,
                      int                    x,
                      int                    y,
                      int                    height)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkAllocation text_allocation;

  gtk_text_get_text_allocation (self, &text_allocation);

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
gtk_text_get_selection_bound_location (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  PangoLayout *layout;
  PangoRectangle pos;
  int x;
  const char *text;
  int index;

  layout = gtk_text_ensure_layout (self, FALSE);
  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, priv->selection_bound) - text;
  pango_layout_index_to_pos (layout, index, &pos);

  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    x = (pos.x + pos.width) / PANGO_SCALE;
  else
    x = pos.x / PANGO_SCALE;

  return x;
}

static void
gtk_text_update_handles (GtkText           *self,
                         GtkTextHandleMode  mode)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkAllocation text_allocation;
  int strong_x;
  int cursor, bound;

  _gtk_text_handle_set_mode (priv->text_handle, mode);
  gtk_text_get_text_allocation (self, &text_allocation);

  gtk_text_get_cursor_locations (self, &strong_x, NULL);
  cursor = strong_x - priv->scroll_offset;

  if (mode == GTK_TEXT_HANDLE_MODE_SELECTION)
    {
      int start, end;

      bound = gtk_text_get_selection_bound_location (self) - priv->scroll_offset;

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
      gtk_text_move_handle (self, GTK_TEXT_HANDLE_POSITION_SELECTION_START,
                             start, 0, text_allocation.height);
      gtk_text_move_handle (self, GTK_TEXT_HANDLE_POSITION_SELECTION_END,
                             end, 0, text_allocation.height);
    }
  else
    gtk_text_move_handle (self, GTK_TEXT_HANDLE_POSITION_CURSOR,
                           cursor, 0, text_allocation.height);
}

static void
gesture_get_current_point_in_layout (GtkGestureSingle *gesture,
                                     GtkText          *self,
                                     int              *x,
                                     int              *y)
{
  int tx, ty;
  GdkEventSequence *sequence;
  double px, py;

  sequence = gtk_gesture_single_get_current_sequence (gesture);
  gtk_gesture_get_point (GTK_GESTURE (gesture), sequence, &px, &py);
  gtk_text_get_layout_offsets (self, &tx, &ty);

  if (x)
    *x = px - tx;
  if (y)
    *y = py - ty;
}

static void
gtk_text_click_gesture_pressed (GtkGestureClick *gesture,
                                int              n_press,
                                double           widget_x,
                                double           widget_y,
                                GtkText         *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
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
  gesture_get_current_point_in_layout (GTK_GESTURE_SINGLE (gesture), self, &x, &y);
  gtk_text_reset_blink_time (self);

  if (!gtk_widget_has_focus (widget))
    {
      priv->in_click = TRUE;
      gtk_widget_grab_focus (widget);
      priv->in_click = FALSE;
    }

  tmp_pos = gtk_text_find_position (self, x);

  if (gdk_event_triggers_context_menu (event))
    {
      gtk_text_do_popup (self, event);
    }
  else if (n_press == 1 && button == GDK_BUTTON_MIDDLE &&
           get_middle_click_paste (self))
    {
      if (priv->editable)
        {
          priv->insert_pos = tmp_pos;
          gtk_text_paste (self, gtk_widget_get_primary_clipboard (widget));
        }
      else
        {
          gtk_widget_error_bell (widget);
        }
    }
  else if (button == GDK_BUTTON_PRIMARY)
    {
      gboolean have_selection;
      GtkTextHandleMode mode;
      gboolean is_touchscreen, extend_selection;
      GdkDevice *source;
      guint state;

      sel_start = priv->selection_bound;
      sel_end = priv->current_pos;
      have_selection = sel_start != sel_end;

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
        gtk_text_ensure_text_handles (self);

      priv->in_drag = FALSE;
      priv->select_words = FALSE;
      priv->select_lines = FALSE;

      gdk_event_get_state (event, &state);

      extend_selection =
        (state &
         gtk_widget_get_modifier_mask (widget,
                                       GDK_MODIFIER_INTENT_EXTEND_SELECTION));

      if (extend_selection)
        gtk_text_reset_im_context (self);

      switch (n_press)
        {
        case 1:
          if (in_selection (self, x))
            {
              if (is_touchscreen)
                {
                  if (priv->selection_bubble &&
                      gtk_widget_get_visible (priv->selection_bubble))
                    gtk_text_selection_bubble_popup_unset (self);
                  else
                    gtk_text_selection_bubble_popup_set (self);
                }
              else if (extend_selection)
                {
                  /* Truncate current selection, but keep it as big as possible */
                  if (tmp_pos - sel_start > sel_end - tmp_pos)
                    gtk_text_set_positions (self, sel_start, tmp_pos);
                  else
                    gtk_text_set_positions (self, tmp_pos, sel_end);

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
              gtk_text_selection_bubble_popup_unset (self);

              if (!extend_selection)
                {
                  gtk_text_set_selection_bounds (self, tmp_pos, tmp_pos);
                  priv->handle_place_time = g_get_monotonic_time ();
                }
              else
                {
                  /* select from the current position to the clicked position */
                  if (!have_selection)
                    sel_start = sel_end = priv->current_pos;

                  gtk_text_set_positions (self, tmp_pos, tmp_pos);
                }
            }

          break;

        case 2:
          priv->select_words = TRUE;
          gtk_text_select_word (self);
          if (is_touchscreen)
            mode = GTK_TEXT_HANDLE_MODE_SELECTION;
          break;

        case 3:
          priv->select_lines = TRUE;
          gtk_text_select_line (self);
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
            gtk_text_set_positions (self, start, end);
          else
            gtk_text_set_positions (self, end, start);
        }

      gtk_gesture_set_state (priv->drag_gesture,
                             GTK_EVENT_SEQUENCE_CLAIMED);

      if (priv->text_handle)
        gtk_text_update_handles (self, mode);
    }

  if (n_press >= 3)
    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
}

static char *
_gtk_text_get_selected_text (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->selection_bound != priv->current_pos)
    {
      const char *text = gtk_entry_buffer_get_text (get_buffer (self));
      int start_index = g_utf8_offset_to_pointer (text, priv->selection_bound) - text;
      int end_index = g_utf8_offset_to_pointer (text, priv->current_pos) - text;
      return g_strndup (text + start_index, end_index - start_index);
    }

  return NULL;
}

static void
gtk_text_show_magnifier (GtkText *self,
                         int      x,
                         int      y)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  cairo_rectangle_int_t rect;
  GtkAllocation text_allocation;

  gtk_text_get_text_allocation (self, &text_allocation);

  gtk_text_ensure_magnifier (self);

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
gtk_text_motion_controller_motion (GtkEventControllerMotion *controller,
                                   double                    x,
                                   double                    y,
                                   GtkText                  *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->mouse_cursor_obscured)
    {
      set_text_cursor (GTK_WIDGET (self));
      priv->mouse_cursor_obscured = FALSE;
    }
}

static void
gtk_text_drag_gesture_update (GtkGestureDrag *gesture,
                              double          offset_x,
                              double          offset_y,
                              GtkText        *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkEventSequence *sequence;
  const GdkEvent *event;
  int x, y;

  gtk_text_selection_bubble_popup_unset (self);

  gesture_get_current_point_in_layout (GTK_GESTURE_SINGLE (gesture), self, &x, &y);
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
      if (gtk_text_get_display_mode (self) == DISPLAY_NORMAL &&
          gtk_drag_check_threshold (widget,
                                    priv->drag_start_x, priv->drag_start_y,
                                    x, y))
        {
          int *ranges;
          int n_ranges;
          GdkContentFormats  *target_list = gdk_content_formats_new (NULL, 0);
          guint actions = priv->editable ? GDK_ACTION_COPY | GDK_ACTION_MOVE : GDK_ACTION_COPY;

          target_list = gtk_content_formats_add_text_targets (target_list);

          gtk_text_get_pixel_ranges (self, &ranges, &n_ranges);

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

      length = gtk_entry_buffer_get_length (get_buffer (self));
      gtk_text_get_text_allocation (self, &text_allocation);

      if (y < 0)
        tmp_pos = 0;
      else if (y >= text_allocation.height)
        tmp_pos = length;
      else
        tmp_pos = gtk_text_find_position (self, x);

      source = gdk_event_get_source_device (event);
      input_source = gdk_device_get_source (source);

      if (priv->select_words)
        {
          int min, max;
          int old_min, old_max;
          int pos, bound;

          min = gtk_text_move_backward_word (self, tmp_pos, TRUE);
          max = gtk_text_move_forward_word (self, tmp_pos, TRUE);

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

          gtk_text_set_positions (self, pos, bound);
        }
      else
        gtk_text_set_positions (self, tmp_pos, -1);

      /* Update touch handles' position */
      if (gtk_simulate_touchscreen () ||
          input_source == GDK_SOURCE_TOUCHSCREEN)
        {
          gtk_text_ensure_text_handles (self);
          gtk_text_update_handles (self,
                                    (priv->current_pos == priv->selection_bound) ?
                                    GTK_TEXT_HANDLE_MODE_CURSOR :
                                    GTK_TEXT_HANDLE_MODE_SELECTION);
          gtk_text_show_magnifier (self, x - priv->scroll_offset, y);
        }
    }
}

static void
gtk_text_drag_gesture_end (GtkGestureDrag *gesture,
                           double          offset_x,
                           double          offset_y,
                           GtkText        *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
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
      int tmp_pos = gtk_text_find_position (self, priv->drag_start_x);

      gtk_text_set_selection_bounds (self, tmp_pos, tmp_pos);
    }

  if (is_touchscreen && priv->selection_bound != priv->current_pos)
    gtk_text_update_handles (self, GTK_TEXT_HANDLE_MODE_CURSOR);

  gtk_text_update_primary_selection (self);
}

static void
gtk_text_obscure_mouse_cursor (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->mouse_cursor_obscured)
    return;

  gtk_widget_set_cursor_from_name (GTK_WIDGET (self), "none");

  priv->mouse_cursor_obscured = TRUE;
}

static gboolean
gtk_text_key_controller_key_pressed (GtkEventControllerKey *controller,
                                      guint                  keyval,
                                      guint                  keycode,
                                      GdkModifierType        state,
                                      GtkText               *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  gunichar unichar;

  gtk_text_reset_blink_time (self);
  gtk_text_pend_cursor_blink (self);

  gtk_text_selection_bubble_popup_unset (self);

  if (priv->text_handle)
    _gtk_text_handle_set_mode (priv->text_handle,
                               GTK_TEXT_HANDLE_MODE_NONE);

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter ||
      keyval == GDK_KEY_Escape)
    gtk_text_reset_im_context (self);

  unichar = gdk_keyval_to_unicode (keyval);

  if (!priv->editable && unichar != 0)
    gtk_widget_error_bell (GTK_WIDGET (self));

  gtk_text_obscure_mouse_cursor (self);

  return FALSE;
}

static void
gtk_text_focus_in (GtkWidget *widget)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkKeymap *keymap;

  gtk_widget_queue_draw (widget);

  keymap = gdk_display_get_keymap (gtk_widget_get_display (widget));

  if (priv->editable)
    {
      gtk_text_schedule_im_reset (self);
      gtk_im_context_focus_in (priv->im_context);
    }

  g_signal_connect (keymap, "direction-changed",
                    G_CALLBACK (keymap_direction_changed), self);

  gtk_text_reset_blink_time (self);
  gtk_text_check_cursor_blink (self);
}

static void
gtk_text_focus_out (GtkWidget *widget)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkKeymap *keymap;

  gtk_text_selection_bubble_popup_unset (self);

  if (priv->text_handle)
    _gtk_text_handle_set_mode (priv->text_handle,
                               GTK_TEXT_HANDLE_MODE_NONE);

  gtk_widget_queue_draw (widget);

  keymap = gdk_display_get_keymap (gtk_widget_get_display (widget));

  if (priv->editable)
    {
      gtk_text_schedule_im_reset (self);
      gtk_im_context_focus_out (priv->im_context);
    }

  gtk_text_check_cursor_blink (self);

  g_signal_handlers_disconnect_by_func (keymap, keymap_direction_changed, self);
}

static void
gtk_text_grab_focus (GtkWidget *widget)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  gboolean select_on_focus;

  GTK_WIDGET_CLASS (gtk_text_parent_class)->grab_focus (GTK_WIDGET (self));

  if (priv->editable && !priv->in_click)
    {
      g_object_get (gtk_widget_get_settings (widget),
                    "gtk-entry-select-on-focus",
                    &select_on_focus,
                    NULL);

      if (select_on_focus)
        gtk_text_set_selection_bounds (self, 0, -1);
    }
}

/**
 * gtk_text_grab_focus_without_selecting:
 * @self: a #GtkText
 *
 * Causes @self to have keyboard focus.
 *
 * It behaves like gtk_widget_grab_focus(),
 * except that it doesn't select the contents of the self.
 * You only want to call this on some special entries
 * which the user usually doesn't want to replace all text in,
 * such as search-as-you-type entries.
 */
void
gtk_text_grab_focus_without_selecting (GtkText *self)
{
  g_return_if_fail (GTK_IS_TEXT (self));

  GTK_WIDGET_CLASS (gtk_text_parent_class)->grab_focus (GTK_WIDGET (self));
}

static void
gtk_text_direction_changed (GtkWidget        *widget,
                            GtkTextDirection  previous_dir)
{
  GtkText *self = GTK_TEXT (widget);

  gtk_text_recompute (self);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_text_state_flags_changed (GtkWidget     *widget,
                              GtkStateFlags  previous_state)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (gtk_widget_get_realized (widget))
    {
      set_text_cursor (widget);
      priv->mouse_cursor_obscured = FALSE;
    }

  if (!gtk_widget_is_sensitive (widget))
    {
      /* Clear any selection */
      gtk_text_set_selection_bounds (self, priv->current_pos, priv->current_pos);
    }

  update_node_state (self);

  gtk_text_update_cached_style_values (self);
}

static void
gtk_text_root (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_text_parent_class)->root (widget);

  gtk_text_recompute (GTK_TEXT (widget));
}

/* GtkEditable method implementations
 */
static void
gtk_text_insert_text (GtkText    *self,
                      const char *text,
                      int         length,
                      int        *position)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int n_inserted;
  int n_chars;

  n_chars = g_utf8_strlen (text, length);

  /*
   * The incoming text may a password or other secret. We make sure
   * not to copy it into temporary buffers.
   */
  begin_change (self);

  n_inserted = gtk_entry_buffer_insert_text (get_buffer (self), *position, text, n_chars);

  end_change (self);

  if (n_inserted != n_chars)
    gtk_widget_error_bell (GTK_WIDGET (self));

  *position += n_inserted;

  update_placeholder_visibility (self);
  if (priv->propagate_text_width)
    gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gtk_text_delete_text (GtkText *self,
                      int      start_pos,
                      int      end_pos)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  begin_change (self);

  gtk_entry_buffer_delete_text (get_buffer (self), start_pos, end_pos - start_pos);

  end_change (self);
  update_placeholder_visibility (self);
  if (priv->propagate_text_width)
    gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gtk_text_delete_selection (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  int start_pos = MIN (priv->selection_bound, priv->current_pos);
  int end_pos = MAX (priv->selection_bound, priv->current_pos);

  gtk_text_delete_text (self, start_pos, end_pos);
}

static void
gtk_text_set_selection_bounds (GtkText *self,
                               int      start,
                               int      end)
{
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (self));
  if (start < 0)
    start = length;
  if (end < 0)
    end = length;

  gtk_text_reset_im_context (self);

  gtk_text_set_positions (self, MIN (end, length), MIN (start, length));

  gtk_text_update_primary_selection (self);
}

static gboolean
gtk_text_get_selection_bounds (GtkText *self,
                               int     *start,
                               int     *end)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

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
gtk_text_update_cached_style_values (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (!priv->invisible_char_set)
    {
      gunichar ch = find_invisible_char (GTK_WIDGET (self));

      if (priv->invisible_char != ch)
        {
          priv->invisible_char = ch;
          g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INVISIBLE_CHAR]);
        }
    }
}

static void 
gtk_text_style_updated (GtkWidget *widget)
{
  GtkText *self = GTK_TEXT (widget);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->style_updated (widget);

  gtk_text_update_cached_style_values (self);
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
update_placeholder_visibility (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->placeholder)
    gtk_widget_set_child_visible (priv->placeholder,
                                  gtk_entry_buffer_get_length (priv->buffer) == 0);
}

/* GtkEntryBuffer signal handlers
 */
static void
buffer_inserted_text (GtkEntryBuffer *buffer,
                      guint           position,
                      const char     *chars,
                      guint           n_chars,
                      GtkText        *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  guint password_hint_timeout;
  guint current_pos;
  int selection_bound;

  current_pos = priv->current_pos;
  if (current_pos > position)
    current_pos += n_chars;

  selection_bound = priv->selection_bound;
  if (selection_bound > position)
    selection_bound += n_chars;

  gtk_text_set_positions (self, current_pos, selection_bound);
  gtk_text_recompute (self);

  /* Calculate the password hint if it needs to be displayed. */
  if (n_chars == 1 && !priv->visible)
    {
      g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                    "gtk-entry-password-hint-timeout", &password_hint_timeout,
                    NULL);

      if (password_hint_timeout > 0)
        {
          GtkTextPasswordHint *password_hint = g_object_get_qdata (G_OBJECT (self),
                                                                    quark_password_hint);
          if (!password_hint)
            {
              password_hint = g_slice_new0 (GtkTextPasswordHint);
              g_object_set_qdata_full (G_OBJECT (self), quark_password_hint, password_hint,
                                       (GDestroyNotify)gtk_text_password_hint_free);
            }

          password_hint->position = position;
          if (password_hint->source_id)
            g_source_remove (password_hint->source_id);
          password_hint->source_id = g_timeout_add (password_hint_timeout,
                                                    (GSourceFunc)gtk_text_remove_password_hint,
                                                    self);
          g_source_set_name_by_id (password_hint->source_id, "[gtk] gtk_text_remove_password_hint");
        }
    }
}

static void
buffer_deleted_text (GtkEntryBuffer *buffer,
                     guint           position,
                     guint           n_chars,
                     GtkText        *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  guint end_pos = position + n_chars;
  int selection_bound;
  guint current_pos;

  current_pos = priv->current_pos;
  if (current_pos > position)
    current_pos -= MIN (current_pos, end_pos) - position;

  selection_bound = priv->selection_bound;
  if (selection_bound > position)
    selection_bound -= MIN (selection_bound, end_pos) - position;

  gtk_text_set_positions (self, current_pos, selection_bound);
  gtk_text_recompute (self);

  /* We might have deleted the selection */
  gtk_text_update_primary_selection (self);

  /* Disable the password hint if one exists. */
  if (!priv->visible)
    {
      GtkTextPasswordHint *password_hint = g_object_get_qdata (G_OBJECT (self),
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
                    GtkText        *self)
{
  emit_changed (self);
  g_object_notify (G_OBJECT (self), "text");
}

static void
buffer_notify_max_length (GtkEntryBuffer *buffer,
                          GParamSpec     *spec,
                          GtkText        *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_MAX_LENGTH]);
}

static void
buffer_connect_signals (GtkText *self)
{
  g_signal_connect (get_buffer (self), "inserted-text", G_CALLBACK (buffer_inserted_text), self);
  g_signal_connect (get_buffer (self), "deleted-text", G_CALLBACK (buffer_deleted_text), self);
  g_signal_connect (get_buffer (self), "notify::text", G_CALLBACK (buffer_notify_text), self);
  g_signal_connect (get_buffer (self), "notify::max-length", G_CALLBACK (buffer_notify_max_length), self);
}

static void
buffer_disconnect_signals (GtkText *self)
{
  g_signal_handlers_disconnect_by_func (get_buffer (self), buffer_inserted_text, self);
  g_signal_handlers_disconnect_by_func (get_buffer (self), buffer_deleted_text, self);
  g_signal_handlers_disconnect_by_func (get_buffer (self), buffer_notify_text, self);
  g_signal_handlers_disconnect_by_func (get_buffer (self), buffer_notify_max_length, self);
}

/* Compute the X position for an offset that corresponds to the "more important
 * cursor position for that offset. We use this when trying to guess to which
 * end of the selection we should go to when the user hits the left or
 * right arrow key.
 */
static int
get_better_cursor_x (GtkText *self,
                     int      offset)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkKeymap *keymap = gdk_display_get_keymap (gtk_widget_get_display (GTK_WIDGET (self)));
  PangoDirection keymap_direction = gdk_keymap_get_direction (keymap);
  gboolean split_cursor;
  PangoLayout *layout = gtk_text_ensure_layout (self, TRUE);
  const char *text = pango_layout_get_text (layout);
  int index = g_utf8_offset_to_pointer (text, offset) - text;
  PangoRectangle strong_pos, weak_pos;
  
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                "gtk-split-cursor", &split_cursor,
                NULL);

  pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);

  if (split_cursor)
    return strong_pos.x / PANGO_SCALE;
  else
    return (keymap_direction == priv->resolved_dir) ? strong_pos.x / PANGO_SCALE : weak_pos.x / PANGO_SCALE;
}

static void
gtk_text_move_cursor (GtkText         *self,
                      GtkMovementStep  step,
                      int              count,
                      gboolean         extend_selection)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int new_pos = priv->current_pos;

  gtk_text_reset_im_context (self);

  if (priv->current_pos != priv->selection_bound && !extend_selection)
    {
      /* If we have a current selection and aren't extending it, move to the
       * start/or end of the selection as appropriate
       */
      switch (step)
        {
        case GTK_MOVEMENT_VISUAL_POSITIONS:
          {
            int current_x = get_better_cursor_x (self, priv->current_pos);
            int bound_x = get_better_cursor_x (self, priv->selection_bound);

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
          new_pos = count < 0 ? 0 : gtk_entry_buffer_get_length (get_buffer (self));
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
          new_pos = gtk_text_move_logically (self, new_pos, count);
          break;

        case GTK_MOVEMENT_VISUAL_POSITIONS:
          new_pos = gtk_text_move_visually (self, new_pos, count);

          if (priv->current_pos == new_pos)
            {
              if (!extend_selection)
                {
                  if (!gtk_widget_keynav_failed (GTK_WIDGET (self),
                                                 count > 0 ?
                                                 GTK_DIR_RIGHT : GTK_DIR_LEFT))
                    {
                      GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self)));

                      if (toplevel)
                        gtk_widget_child_focus (toplevel,
                                                count > 0 ?
                                                GTK_DIR_RIGHT : GTK_DIR_LEFT);
                    }
                }
              else
                {
                  gtk_widget_error_bell (GTK_WIDGET (self));
                }
            }
          break;

        case GTK_MOVEMENT_WORDS:
          if (priv->resolved_dir == PANGO_DIRECTION_RTL)
            count *= -1;

          while (count > 0)
            {
              new_pos = gtk_text_move_forward_word (self, new_pos, FALSE);
              count--;
            }

          while (count < 0)
            {
              new_pos = gtk_text_move_backward_word (self, new_pos, FALSE);
              count++;
            }

          if (priv->current_pos == new_pos)
            gtk_widget_error_bell (GTK_WIDGET (self));

          break;

        case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
        case GTK_MOVEMENT_PARAGRAPH_ENDS:
        case GTK_MOVEMENT_BUFFER_ENDS:
          new_pos = count < 0 ? 0 : gtk_entry_buffer_get_length (get_buffer (self));

          if (priv->current_pos == new_pos)
            gtk_widget_error_bell (GTK_WIDGET (self));

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
    gtk_text_set_selection_bounds (self, priv->selection_bound, new_pos);
  else
    gtk_text_set_selection_bounds (self, new_pos, new_pos);
  
  gtk_text_pend_cursor_blink (self);
}

static void
gtk_text_insert_at_cursor (GtkText    *self,
                           const char *str)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int pos = priv->current_pos;

  if (priv->editable)
    {
      gtk_text_reset_im_context (self);
      gtk_text_insert_text (self, str, -1, &pos);
      gtk_text_set_selection_bounds (self, pos, pos);
    }
}

static void
gtk_text_delete_from_cursor (GtkText       *self,
                             GtkDeleteType  type,
                             int            count)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int start_pos = priv->current_pos;
  int end_pos = priv->current_pos;
  int old_n_bytes = gtk_entry_buffer_get_bytes (get_buffer (self));

  gtk_text_reset_im_context (self);

  if (!priv->editable)
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
      return;
    }

  if (priv->selection_bound != priv->current_pos)
    {
      gtk_text_delete_selection (self);
      return;
    }
  
  switch (type)
    {
    case GTK_DELETE_CHARS:
      end_pos = gtk_text_move_logically (self, priv->current_pos, count);
      gtk_text_delete_text (self, MIN (start_pos, end_pos), MAX (start_pos, end_pos));
      break;

    case GTK_DELETE_WORDS:
      if (count < 0)
        {
          /* Move to end of current word, or if not on a word, end of previous word */
          end_pos = gtk_text_move_backward_word (self, end_pos, FALSE);
          end_pos = gtk_text_move_forward_word (self, end_pos, FALSE);
        }
      else if (count > 0)
        {
          /* Move to beginning of current word, or if not on a word, begining of next word */
          start_pos = gtk_text_move_forward_word (self, start_pos, FALSE);
          start_pos = gtk_text_move_backward_word (self, start_pos, FALSE);
        }
        
      /* Fall through */
    case GTK_DELETE_WORD_ENDS:
      while (count < 0)
        {
          start_pos = gtk_text_move_backward_word (self, start_pos, FALSE);
          count++;
        }

      while (count > 0)
        {
          end_pos = gtk_text_move_forward_word (self, end_pos, FALSE);
          count--;
        }

      gtk_text_delete_text (self, start_pos, end_pos);
      break;

    case GTK_DELETE_DISPLAY_LINE_ENDS:
    case GTK_DELETE_PARAGRAPH_ENDS:
      if (count < 0)
        gtk_text_delete_text (self, 0, priv->current_pos);
      else
        gtk_text_delete_text (self, priv->current_pos, -1);

      break;

    case GTK_DELETE_DISPLAY_LINES:
    case GTK_DELETE_PARAGRAPHS:
      gtk_text_delete_text (self, 0, -1);  
      break;

    case GTK_DELETE_WHITESPACE:
      gtk_text_delete_whitespace (self);
      break;

    default:
      break;
    }

  if (gtk_entry_buffer_get_bytes (get_buffer (self)) == old_n_bytes)
    gtk_widget_error_bell (GTK_WIDGET (self));

  gtk_text_pend_cursor_blink (self);
}

static void
gtk_text_backspace (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int prev_pos;

  gtk_text_reset_im_context (self);

  if (!priv->editable)
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
      return;
    }

  if (priv->selection_bound != priv->current_pos)
    {
      gtk_text_delete_selection (self);
      return;
    }

  prev_pos = gtk_text_move_logically (self, priv->current_pos, -1);

  if (prev_pos < priv->current_pos)
    {
      PangoLayout *layout = gtk_text_ensure_layout (self, FALSE);
      const PangoLogAttr *log_attrs;
      int n_attrs;

      log_attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

      /* Deleting parts of characters */
      if (log_attrs[priv->current_pos].backspace_deletes_character)
        {
          char *cluster_text;
          char *normalized_text;
          glong  len;

          cluster_text = gtk_text_get_display_text (self, prev_pos, priv->current_pos);
          normalized_text = g_utf8_normalize (cluster_text,
                                              strlen (cluster_text),
                                              G_NORMALIZE_NFD);
          len = g_utf8_strlen (normalized_text, -1);

          gtk_text_delete_text (self, prev_pos, priv->current_pos);
          if (len > 1)
            {
              int pos = priv->current_pos;

              gtk_text_insert_text (self, normalized_text,
                                    g_utf8_offset_to_pointer (normalized_text, len - 1) - normalized_text,
                                    &pos);
              gtk_text_set_selection_bounds (self, pos, pos);
            }

          g_free (normalized_text);
          g_free (cluster_text);
        }
      else
        {
          gtk_text_delete_text (self, prev_pos, priv->current_pos);
        }
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
    }

  gtk_text_pend_cursor_blink (self);
}

static void
gtk_text_copy_clipboard (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->selection_bound != priv->current_pos)
    {
      char *str;

      if (!priv->visible)
        {
          gtk_widget_error_bell (GTK_WIDGET (self));
          return;
        }

      str = gtk_text_get_display_text (self, priv->selection_bound, priv->current_pos);
      gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (self)), str);
      g_free (str);
    }
}

static void
gtk_text_cut_clipboard (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (!priv->visible)
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
      return;
    }

  gtk_text_copy_clipboard (self);

  if (priv->editable)
    {
      if (priv->selection_bound != priv->current_pos)
        gtk_text_delete_selection (self);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
    }

  gtk_text_selection_bubble_popup_unset (self);

  if (priv->text_handle)
    {
      GtkTextHandleMode handle_mode;

      handle_mode = _gtk_text_handle_get_mode (priv->text_handle);

      if (handle_mode != GTK_TEXT_HANDLE_MODE_NONE)
        gtk_text_update_handles (self, GTK_TEXT_HANDLE_MODE_CURSOR);
    }
}

static void
gtk_text_paste_clipboard (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->editable)
    gtk_text_paste (self, gtk_widget_get_clipboard (GTK_WIDGET (self)));
  else
    gtk_widget_error_bell (GTK_WIDGET (self));

  if (priv->text_handle)
    {
      GtkTextHandleMode handle_mode;

      handle_mode = _gtk_text_handle_get_mode (priv->text_handle);

      if (handle_mode != GTK_TEXT_HANDLE_MODE_NONE)
        gtk_text_update_handles (self, GTK_TEXT_HANDLE_MODE_CURSOR);
    }
}

static void
gtk_text_delete_cb (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->editable)
    {
      if (priv->selection_bound != priv->current_pos)
        gtk_text_delete_selection (self);
    }
}

static void
gtk_text_toggle_overwrite (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  priv->overwrite_mode = !priv->overwrite_mode;

  if (priv->overwrite_mode)
    {
      if (!priv->block_cursor_node)
        {
          GtkCssNode *widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));

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

  gtk_text_pend_cursor_blink (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gtk_text_select_all (GtkText *self)
{
  gtk_text_select_line (self);
}

static void
gtk_text_real_activate (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->activates_default)
    gtk_widget_activate_default (GTK_WIDGET (self));
}

static void
keymap_direction_changed (GdkKeymap *keymap,
                          GtkText   *self)
{
  gtk_text_recompute (self);
}

/* IM Context Callbacks
 */

static void
gtk_text_commit_cb (GtkIMContext *context,
                    const char   *str,
                    GtkText      *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->editable)
    {
      gtk_text_enter_text (self, str);
      gtk_text_obscure_mouse_cursor (self);
    }
}

static void 
gtk_text_preedit_changed_cb (GtkIMContext *context,
                             GtkText      *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->editable)
    {
      char *preedit_string;
      int cursor_pos;

      gtk_text_obscure_mouse_cursor (self);

      gtk_im_context_get_preedit_string (priv->im_context,
                                         &preedit_string, NULL,
                                         &cursor_pos);
      g_signal_emit (self, signals[PREEDIT_CHANGED], 0, preedit_string);
      priv->preedit_length = strlen (preedit_string);
      cursor_pos = CLAMP (cursor_pos, 0, g_utf8_strlen (preedit_string, -1));
      priv->preedit_cursor = cursor_pos;
      g_free (preedit_string);
    
      gtk_text_recompute (self);
    }
}

static gboolean
gtk_text_retrieve_surrounding_cb (GtkIMContext *context,
                                  GtkText      *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  char *text;

  /* XXXX ??? does this even make sense when text is not visible? Should we return FALSE? */
  text = gtk_text_get_display_text (self, 0, -1);
  gtk_im_context_set_surrounding (context, text, strlen (text), /* Length in bytes */
                                  g_utf8_offset_to_pointer (text, priv->current_pos) - text);
  g_free (text);

  return TRUE;
}

static gboolean
gtk_text_delete_surrounding_cb (GtkIMContext *slave,
                                int           offset,
                                int           n_chars,
                                GtkText      *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->editable)
    gtk_text_delete_text (self,
                          priv->current_pos + offset,
                          priv->current_pos + offset + n_chars);

  return TRUE;
}

/* Internal functions
 */

/* Used for im_commit_cb and inserting Unicode chars */
void
gtk_text_enter_text (GtkText    *self,
                     const char *str)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int tmp_pos;
  gboolean old_need_im_reset;
  guint text_length;

  old_need_im_reset = priv->need_im_reset;
  priv->need_im_reset = FALSE;

  if (priv->selection_bound != priv->current_pos)
    gtk_text_delete_selection (self);
  else
    {
      if (priv->overwrite_mode)
        {
          text_length = gtk_entry_buffer_get_length (get_buffer (self));
          if (priv->current_pos < text_length)
            gtk_text_delete_from_cursor (self, GTK_DELETE_CHARS, 1);
        }
    }

  tmp_pos = priv->current_pos;
  gtk_text_insert_text (self, str, strlen (str), &tmp_pos);
  gtk_text_set_selection_bounds (self, tmp_pos, tmp_pos);

  priv->need_im_reset = old_need_im_reset;
}

/* All changes to priv->current_pos and priv->selection_bound
 * should go through this function.
 */
void
gtk_text_set_positions (GtkText *self,
                        int      current_pos,
                        int      selection_bound)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  gboolean changed = FALSE;

  g_object_freeze_notify (G_OBJECT (self));

  if (current_pos != -1 &&
      priv->current_pos != current_pos)
    {
      priv->current_pos = current_pos;
      changed = TRUE;

      g_object_notify (G_OBJECT (self), "cursor-position");
    }

  if (selection_bound != -1 &&
      priv->selection_bound != selection_bound)
    {
      priv->selection_bound = selection_bound;
      changed = TRUE;

      g_object_notify (G_OBJECT (self), "selection-bound");
    }

  g_object_thaw_notify (G_OBJECT (self));

  if (priv->current_pos != priv->selection_bound)
    {
      if (!priv->selection_node)
        {
          GtkCssNode *widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));

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
      gtk_text_recompute (self);
    }
}

static void
gtk_text_reset_layout (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->cached_layout)
    {
      g_object_unref (priv->cached_layout);
      priv->cached_layout = NULL;
    }
}

static void
update_im_cursor_location (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkRectangle area;
  GtkAllocation text_area;
  int strong_x;
  int strong_xoffset;

  gtk_text_get_cursor_locations (self, &strong_x, NULL);
  gtk_text_get_text_allocation (self, &text_area);

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
gtk_text_recompute (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkTextHandleMode handle_mode;

  gtk_text_reset_layout (self);
  gtk_text_check_cursor_blink (self);

  gtk_text_adjust_scroll (self);

  update_im_cursor_location (self);

  if (priv->text_handle)
    {
      handle_mode = _gtk_text_handle_get_mode (priv->text_handle);

      if (handle_mode != GTK_TEXT_HANDLE_MODE_NONE)
        gtk_text_update_handles (self, handle_mode);
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static PangoLayout *
gtk_text_create_layout (GtkText  *self,
                        gboolean  include_preedit)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);
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

  display_text = gtk_text_get_display_text (self, 0, -1);

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

      if (gtk_text_get_display_mode (self) == DISPLAY_NORMAL)
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
gtk_text_ensure_layout (GtkText  *self,
                        gboolean  include_preedit)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->preedit_length > 0 &&
      !include_preedit != !priv->cache_includes_preedit)
    gtk_text_reset_layout (self);

  if (!priv->cached_layout)
    {
      priv->cached_layout = gtk_text_create_layout (self, include_preedit);
      priv->cache_includes_preedit = include_preedit;
    }

  return priv->cached_layout;
}

static void
get_layout_position (GtkText *self,
                     int     *x,
                     int     *y)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  PangoLayout *layout;
  PangoRectangle logical_rect;
  int y_pos, area_height;
  PangoLayoutLine *line;
  GtkAllocation text_allocation;

  gtk_text_get_text_allocation (self, &text_allocation);

  layout = gtk_text_ensure_layout (self, TRUE);

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
    *x = - priv->scroll_offset;

  if (y)
    *y = y_pos;
}

#define GRAPHENE_RECT_FROM_RECT(_r) (GRAPHENE_RECT_INIT ((_r)->x, (_r)->y, (_r)->width, (_r)->height))

static void
gtk_text_draw_text (GtkText     *self,
                    GtkSnapshot *snapshot)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);
  GtkStyleContext *context;
  PangoLayout *layout;
  int x, y;
  int width, height;

  /* Nothing to display at all */
  if (gtk_text_get_display_mode (self) == DISPLAY_BLANK)
    return;

  context = gtk_widget_get_style_context (widget);
  layout = gtk_text_ensure_layout (self, TRUE);
  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  gtk_text_get_layout_offsets (self, &x, &y);

  gtk_snapshot_render_layout (snapshot, context, x, y, layout);

  if (priv->selection_bound != priv->current_pos)
    {
      const char *text = pango_layout_get_text (layout);
      int start_index = g_utf8_offset_to_pointer (text, priv->selection_bound) - text;
      int end_index = g_utf8_offset_to_pointer (text, priv->current_pos) - text;
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
gtk_text_draw_cursor (GtkText     *self,
                      GtkSnapshot *snapshot,
                      CursorType   type)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);
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

  layout = gtk_text_ensure_layout (self, TRUE);
  text = pango_layout_get_text (layout);
  gtk_text_get_layout_offsets (self, &x, &y);
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
                         GtkText               *self)
{
  int cursor_pos, selection_bound_pos, tmp_pos;
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkTextHandleMode mode;
  int *min, *max;

  gtk_text_selection_bubble_popup_unset (self);

  cursor_pos = priv->current_pos;
  selection_bound_pos = priv->selection_bound;
  mode = _gtk_text_handle_get_mode (handle);

  tmp_pos = gtk_text_find_position (self, x + priv->scroll_offset);

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
          gtk_text_set_positions (self, cursor_pos, cursor_pos);
        }
      else
        {
          priv->selection_handle_dragged = TRUE;
          gtk_text_set_positions (self, cursor_pos, selection_bound_pos);
        }

      gtk_text_update_handles (self, mode);
    }

  gtk_text_show_magnifier (self, x, y);
}

static void
gtk_text_handle_drag_started (GtkTextHandle         *handle,
                              GtkTextHandlePosition  pos,
                              GtkText               *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  priv->cursor_handle_dragged = FALSE;
  priv->selection_handle_dragged = FALSE;
}

static void
gtk_text_handle_drag_finished (GtkTextHandle         *handle,
                               GtkTextHandlePosition  pos,
                               GtkText               *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (!priv->cursor_handle_dragged && !priv->selection_handle_dragged)
    {
      GtkSettings *settings;
      guint double_click_time;

      settings = gtk_widget_get_settings (GTK_WIDGET (self));
      g_object_get (settings, "gtk-double-click-time", &double_click_time, NULL);
      if (g_get_monotonic_time() - priv->handle_place_time < double_click_time * 1000)
        {
          gtk_text_select_word (self);
          gtk_text_update_handles (self, GTK_TEXT_HANDLE_MODE_SELECTION);
        }
      else
        gtk_text_selection_bubble_popup_set (self);
    }

  if (priv->magnifier_popover)
    gtk_popover_popdown (GTK_POPOVER (priv->magnifier_popover));
}

static void
gtk_text_schedule_im_reset (GtkText *self)
{
  GtkTextPrivate *priv;

  priv = gtk_text_get_instance_private (self);

  priv->need_im_reset = TRUE;
}

void
gtk_text_reset_im_context (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (priv->need_im_reset)
    {
      priv->need_im_reset = FALSE;
      gtk_im_context_reset (priv->im_context);
    }
}

GtkIMContext *
gtk_text_get_im_context (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  return priv->im_context;
}

static int
gtk_text_find_position (GtkText *self,
                        int      x)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  PangoLayout *layout;
  PangoLayoutLine *line;
  int index;
  int pos;
  int trailing;
  const char *text;
  int cursor_index;
  
  layout = gtk_text_ensure_layout (self, TRUE);
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
gtk_text_get_cursor_locations (GtkText   *self,
                               int       *strong_x,
                               int       *weak_x)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  DisplayMode mode = gtk_text_get_display_mode (self);

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
      PangoLayout *layout = gtk_text_ensure_layout (self, TRUE);
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
gtk_text_get_is_selection_handle_dragged (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
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
gtk_text_get_scroll_limits (GtkText *self,
                            int     *min_offset,
                            int     *max_offset)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  float xalign;
  PangoLayout *layout;
  PangoLayoutLine *line;
  PangoRectangle logical_rect;
  int text_width, width;

  layout = gtk_text_ensure_layout (self, TRUE);
  line = pango_layout_get_lines_readonly (layout)->data;

  pango_layout_line_get_extents (line, NULL, &logical_rect);

  /* Display as much text as we can */

  if (priv->resolved_dir == PANGO_DIRECTION_LTR)
      xalign = priv->xalign;
  else
      xalign = 1.0 - priv->xalign;

  text_width = PANGO_PIXELS(logical_rect.width);
  width = gtk_widget_get_width (GTK_WIDGET (self));

  if (text_width > width)
    {
      *min_offset = 0;
      *max_offset = text_width - width;
    }
  else
    {
      *min_offset = (text_width - width) * xalign;
      *max_offset = *min_offset;
    }
}

static void
gtk_text_adjust_scroll (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int min_offset, max_offset;
  int strong_x, weak_x;
  int strong_xoffset, weak_xoffset;
  GtkTextHandleMode handle_mode;
  GtkAllocation text_allocation;

  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  gtk_text_get_text_allocation (self, &text_allocation);

  gtk_text_get_scroll_limits (self, &min_offset, &max_offset);

  priv->scroll_offset = CLAMP (priv->scroll_offset, min_offset, max_offset);

  if (gtk_text_get_is_selection_handle_dragged (self))
    {
      /* The text handle corresponding to the selection bound is
       * being dragged, ensure it stays onscreen even if we scroll
       * cursors away, this is so both handles can cause content
       * to scroll.
       */
      strong_x = weak_x = gtk_text_get_selection_bound_location (self);
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
      gtk_text_get_cursor_locations (self, &strong_x, &weak_x);
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

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_SCROLL_OFFSET]);

  if (priv->text_handle)
    {
      handle_mode = _gtk_text_handle_get_mode (priv->text_handle);

      if (handle_mode != GTK_TEXT_HANDLE_MODE_NONE)
        gtk_text_update_handles (self, handle_mode);
    }
}

static int
gtk_text_move_visually (GtkText *self,
                        int      start,
                        int      count)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int index;
  PangoLayout *layout = gtk_text_ensure_layout (self, FALSE);
  const char *text;

  text = pango_layout_get_text (layout);
  
  index = g_utf8_offset_to_pointer (text, start) - text;

  while (count != 0)
    {
      int new_index, new_trailing;
      gboolean split_cursor;
      gboolean strong;

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                    "gtk-split-cursor", &split_cursor,
                    NULL);

      if (split_cursor)
        strong = TRUE;
      else
        {
          GdkKeymap *keymap = gdk_display_get_keymap (gtk_widget_get_display (GTK_WIDGET (self)));
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
gtk_text_move_logically (GtkText *self,
                         int      start,
                         int      count)
{
  int new_pos = start;
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (self));

  /* Prevent any leak of information */
  if (gtk_text_get_display_mode (self) != DISPLAY_NORMAL)
    {
      new_pos = CLAMP (start + count, 0, length);
    }
  else
    {
      PangoLayout *layout = gtk_text_ensure_layout (self, FALSE);
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
gtk_text_move_forward_word (GtkText  *self,
                            int       start,
                            gboolean  allow_whitespace)
{
  int new_pos = start;
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (self));

  /* Prevent any leak of information */
  if (gtk_text_get_display_mode (self) != DISPLAY_NORMAL)
    {
      new_pos = length;
    }
  else if (new_pos < length)
    {
      PangoLayout *layout = gtk_text_ensure_layout (self, FALSE);
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
gtk_text_move_backward_word (GtkText  *self,
                             int       start,
                             gboolean  allow_whitespace)
{
  int new_pos = start;

  /* Prevent any leak of information */
  if (gtk_text_get_display_mode (self) != DISPLAY_NORMAL)
    {
      new_pos = 0;
    }
  else if (start > 0)
    {
      PangoLayout *layout = gtk_text_ensure_layout (self, FALSE);
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
gtk_text_delete_whitespace (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  PangoLayout *layout = gtk_text_ensure_layout (self, FALSE);
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
    gtk_text_delete_text (self, start, end);
}


static void
gtk_text_select_word (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int start_pos = gtk_text_move_backward_word (self, priv->current_pos, TRUE);
  int end_pos = gtk_text_move_forward_word (self, priv->current_pos, TRUE);

  gtk_text_set_selection_bounds (self, start_pos, end_pos);
}

static void
gtk_text_select_line (GtkText *self)
{
  gtk_text_set_selection_bounds (self, 0, -1);
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
  GtkText *self = GTK_TEXT (data);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  char *text;
  int pos, start, end;
  int length = -1;

  text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (clipboard), result, NULL);
  if (text == NULL)
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
      return;
    }

  if (priv->insert_pos >= 0)
    {
      pos = priv->insert_pos;
      start = priv->selection_bound;
      end = priv->current_pos;
      if (!((start <= pos && pos <= end) || (end <= pos && pos <= start)))
        gtk_text_set_selection_bounds (self, pos, pos);
      priv->insert_pos = -1;
    }
      
  if (priv->truncate_multiline)
    length = truncate_multiline (text);

  begin_change (self);
  if (priv->selection_bound != priv->current_pos)
    gtk_text_delete_selection (self);

  pos = priv->current_pos;
  gtk_text_insert_text (self, text, length, &pos);
  gtk_text_set_selection_bounds (self, pos, pos);
  end_change (self);

  g_free (text);
  g_object_unref (self);
}

static void
gtk_text_paste (GtkText      *self,
                GdkClipboard *clipboard)
{
  gdk_clipboard_read_text_async (clipboard, NULL, paste_received, g_object_ref (self));
}

static void
gtk_text_update_primary_selection (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkClipboard *clipboard;

  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  clipboard = gtk_widget_get_primary_clipboard (GTK_WIDGET (self));
  
  if (priv->selection_bound != priv->current_pos)
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
 * Creates a new self.
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
 * Creates a new self with the specified text buffer.
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
get_buffer (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->buffer == NULL)
    {
      GtkEntryBuffer *buffer;
      buffer = gtk_entry_buffer_new (NULL, 0);
      gtk_text_set_buffer (self, buffer);
      g_object_unref (buffer);
    }

  return priv->buffer;
}

/**
 * gtk_text_get_buffer:
 * @self: a #GtkText
 *
 * Get the #GtkEntryBuffer object which holds the text for
 * this self.
 *
 * Returns: (transfer none): A #GtkEntryBuffer object.
 */
GtkEntryBuffer *
gtk_text_get_buffer (GtkText *self)
{
  g_return_val_if_fail (GTK_IS_TEXT (self), NULL);

  return get_buffer (self);
}

/**
 * gtk_text_set_buffer:
 * @self: a #GtkText
 * @buffer: a #GtkEntryBuffer
 *
 * Set the #GtkEntryBuffer object which holds the text for
 * this widget.
 */
void
gtk_text_set_buffer (GtkText        *self,
                     GtkEntryBuffer *buffer)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GObject *obj;
  gboolean had_buffer = FALSE;
  guint old_length = 0;
  guint new_length = 0;

  g_return_if_fail (GTK_IS_TEXT (self));

  if (buffer)
    {
      g_return_if_fail (GTK_IS_ENTRY_BUFFER (buffer));
      g_object_ref (buffer);
    }

  if (priv->buffer)
    {
      had_buffer = TRUE;
      old_length = gtk_entry_buffer_get_length (priv->buffer);
      buffer_disconnect_signals (self);
      g_object_unref (priv->buffer);
    }

  priv->buffer = buffer;

  if (priv->buffer)
    {
      new_length = gtk_entry_buffer_get_length (priv->buffer);
      buffer_connect_signals (self);
    }

  obj = G_OBJECT (self);
  g_object_freeze_notify (obj);
  g_object_notify_by_pspec (obj, text_props[PROP_BUFFER]);
  g_object_notify_by_pspec (obj, text_props[PROP_MAX_LENGTH]);
  if (old_length != 0 || new_length != 0)
    g_object_notify (obj, "text");

  if (had_buffer)
    {
      gtk_text_set_selection_bounds (self, 0, 0);
      gtk_text_recompute (self);
    }

  g_object_thaw_notify (obj);
}

static void
gtk_text_set_editable (GtkText  *self,
                       gboolean  is_editable)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (is_editable != priv->editable)
    {
      GtkWidget *widget = GTK_WIDGET (self);

      if (!is_editable)
        {
          gtk_text_reset_im_context (self);
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

      g_object_notify (G_OBJECT (self), "editable");
      gtk_widget_queue_draw (widget);
    }
}

static void
gtk_text_set_text (GtkText     *self,
                   const char *text)
{
  int tmp_pos;

  g_return_if_fail (GTK_IS_TEXT (self));
  g_return_if_fail (text != NULL);

  /* Actually setting the text will affect the cursor and selection;
   * if the contents don't actually change, this will look odd to the user.
   */
  if (strcmp (gtk_entry_buffer_get_text (get_buffer (self)), text) == 0)
    return;

  begin_change (self);
  g_object_freeze_notify (G_OBJECT (self));
  gtk_text_delete_text (self, 0, -1);
  tmp_pos = 0;
  gtk_text_insert_text (self, text, strlen (text), &tmp_pos);
  g_object_thaw_notify (G_OBJECT (self));
  end_change (self);
}

/**
 * gtk_text_set_visibility:
 * @self: a #GtkText
 * @visible: %TRUE if the contents of the self are displayed
 *           as plaintext
 *
 * Sets whether the contents of the self are visible or not.
 * When visibility is set to %FALSE, characters are displayed
 * as the invisible char, and will also appear that way when
 * the text in the self widget is copied to the clipboard.
 *
 * By default, GTK picks the best invisible character available
 * in the current font, but it can be changed with
 * gtk_text_set_invisible_char().
 *
 * Note that you probably want to set #GtkText:input-purpose
 * to %GTK_INPUT_PURPOSE_PASSWORD or %GTK_INPUT_PURPOSE_PIN to
 * inform input methods about the purpose of this self,
 * in addition to setting visibility to %FALSE.
 */
void
gtk_text_set_visibility (GtkText  *self,
                         gboolean  visible)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  visible = visible != FALSE;

  if (priv->visible != visible)
    {
      priv->visible = visible;

      g_object_notify (G_OBJECT (self), "visibility");
      gtk_text_recompute (self);
    }
}

/**
 * gtk_text_get_visibility:
 * @self: a #GtkText
 *
 * Retrieves whether the text in @self is visible.
 * See gtk_text_set_visibility().
 *
 * Returns: %TRUE if the text is currently visible
 **/
gboolean
gtk_text_get_visibility (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), FALSE);

  return priv->visible;
}

/**
 * gtk_text_set_invisible_char:
 * @self: a #GtkText
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
gtk_text_set_invisible_char (GtkText  *self,
                             gunichar  ch)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (!priv->invisible_char_set)
    {
      priv->invisible_char_set = TRUE;
      g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INVISIBLE_CHAR_SET]);
    }

  if (ch == priv->invisible_char)
    return;

  priv->invisible_char = ch;
  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INVISIBLE_CHAR]);
  gtk_text_recompute (self);
}

/**
 * gtk_text_get_invisible_char:
 * @self: a #GtkText
 *
 * Retrieves the character displayed in place of the real characters
 * for entries with visibility set to false.
 * See gtk_text_set_invisible_char().
 *
 * Returns: the current invisible char, or 0, if the self does not
 *               show invisible text at all. 
 **/
gunichar
gtk_text_get_invisible_char (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), 0);

  return priv->invisible_char;
}

/**
 * gtk_text_unset_invisible_char:
 * @self: a #GtkText
 *
 * Unsets the invisible char previously set with
 * gtk_text_set_invisible_char(). So that the
 * default invisible char is used again.
 **/
void
gtk_text_unset_invisible_char (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  gunichar ch;

  g_return_if_fail (GTK_IS_TEXT (self));

  if (!priv->invisible_char_set)
    return;

  priv->invisible_char_set = FALSE;
  ch = find_invisible_char (GTK_WIDGET (self));

  if (priv->invisible_char != ch)
    {
      priv->invisible_char = ch;
      g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INVISIBLE_CHAR]);
    }

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INVISIBLE_CHAR_SET]);
  gtk_text_recompute (self);
}

/**
 * gtk_text_set_overwrite_mode:
 * @self: a #GtkText
 * @overwrite: new value
 *
 * Sets whether the text is overwritten when typing in the #GtkText.
 **/
void
gtk_text_set_overwrite_mode (GtkText  *self,
                             gboolean  overwrite)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (priv->overwrite_mode == overwrite)
    return;

  gtk_text_toggle_overwrite (self);

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_OVERWRITE_MODE]);
}

/**
 * gtk_text_get_overwrite_mode:
 * @self: a #GtkText
 *
 * Gets the value set by gtk_text_set_overwrite_mode().
 *
 * Returns: whether the text is overwritten when typing.
 **/
gboolean
gtk_text_get_overwrite_mode (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), FALSE);

  return priv->overwrite_mode;

}

/**
 * gtk_text_set_max_length:
 * @self: a #GtkText
 * @length: the maximum length of the self, or 0 for no maximum.
 *   (other than the maximum length of entries.) The value passed in will
 *   be clamped to the range 0-65536.
 *
 * Sets the maximum allowed length of the contents of the widget.
 *
 * If the current contents are longer than the given length, then
 * they will be truncated to fit.
 *
 * This is equivalent to getting @self's #GtkEntryBuffer and
 * calling gtk_entry_buffer_set_max_length() on it.
 * ]|
 **/
void
gtk_text_set_max_length (GtkText *self,
                         int      length)
{
  g_return_if_fail (GTK_IS_TEXT (self));
  gtk_entry_buffer_set_max_length (get_buffer (self), length);
}

/**
 * gtk_text_get_max_length:
 * @self: a #GtkText
 *
 * Retrieves the maximum allowed length of the text in
 * @self. See gtk_text_set_max_length().
 *
 * This is equivalent to getting @self's #GtkEntryBuffer and
 * calling gtk_entry_buffer_get_max_length() on it.
 *
 * Returns: the maximum allowed number of characters
 *               in #GtkText, or 0 if there is no maximum.
 **/
int
gtk_text_get_max_length (GtkText *self)
{
  g_return_val_if_fail (GTK_IS_TEXT (self), 0);

  return gtk_entry_buffer_get_max_length (get_buffer (self));
}

/**
 * gtk_text_get_text_length:
 * @self: a #GtkText
 *
 * Retrieves the current length of the text in
 * @self. 
 *
 * This is equivalent to getting @self's #GtkEntryBuffer and
 * calling gtk_entry_buffer_get_length() on it.

 *
 * Returns: the current number of characters
 *               in #GtkText, or 0 if there are none.
 **/
guint16
gtk_text_get_text_length (GtkText *self)
{
  g_return_val_if_fail (GTK_IS_TEXT (self), 0);

  return gtk_entry_buffer_get_length (get_buffer (self));
}

/**
 * gtk_text_set_activates_default:
 * @self: a #GtkText
 * @activates: %TRUE to activate window’s default widget on Enter keypress
 *
 * If @activates is %TRUE, pressing Enter in the @self will activate the default
 * widget for the window containing the self. This usually means that
 * the dialog box containing the self will be closed, since the default
 * widget is usually one of the dialog buttons.
 **/
void
gtk_text_set_activates_default (GtkText  *self,
                                gboolean  activates)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  activates = activates != FALSE;

  if (priv->activates_default != activates)
    {
      priv->activates_default = activates;
      g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_ACTIVATES_DEFAULT]);
    }
}

/**
 * gtk_text_get_activates_default:
 * @self: a #GtkText
 *
 * Retrieves the value set by gtk_text_set_activates_default().
 *
 * Returns: %TRUE if the self will activate the default widget
 */
gboolean
gtk_text_get_activates_default (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), FALSE);

  return priv->activates_default;
}

static void
gtk_text_set_width_chars (GtkText *self,
                          int      n_chars)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->width_chars != n_chars)
    {
      priv->width_chars = n_chars;
      g_object_notify (G_OBJECT (self), "width-chars");
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

static void
gtk_text_set_max_width_chars (GtkText *self,
                              int      n_chars)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->max_width_chars != n_chars)
    {
      priv->max_width_chars = n_chars;
      g_object_notify (G_OBJECT (self), "max-width-chars");
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

PangoLayout *
gtk_text_get_layout (GtkText *self)
{
  PangoLayout *layout;
  
  g_return_val_if_fail (GTK_IS_TEXT (self), NULL);

  layout = gtk_text_ensure_layout (self, TRUE);

  return layout;
}

void
gtk_text_get_layout_offsets (GtkText *self,
                             int     *x,
                             int     *y)
{
  g_return_if_fail (GTK_IS_TEXT (self));

  get_layout_position (self, x, y);
}

static void
gtk_text_set_alignment (GtkText *self,
                        float    xalign)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (xalign < 0.0)
    xalign = 0.0;
  else if (xalign > 1.0)
    xalign = 1.0;

  if (xalign != priv->xalign)
    {
      priv->xalign = xalign;
      gtk_text_recompute (self);
      g_object_notify (G_OBJECT (self), "xalign");
    }
}

/* Quick hack of a popup menu
 */
static void
activate_cb (GtkWidget *menuitem,
             GtkText   *self)
{
  const char *signal;

  signal = g_object_get_qdata (G_OBJECT (menuitem), quark_gtk_signal);
  g_signal_emit_by_name (self, signal);
}


static gboolean
gtk_text_mnemonic_activate (GtkWidget *widget,
                            gboolean   group_cycling)
{
  gtk_widget_grab_focus (widget);
  return GDK_EVENT_STOP;
}

static void
append_action_signal (GtkText     *self,
                      GtkWidget   *menu,
                      const char  *label,
                      const char  *signal,
                      gboolean     sensitive)
{
  GtkWidget *menuitem = gtk_menu_item_new_with_mnemonic (label);

  g_object_set_qdata (G_OBJECT (menuitem), quark_gtk_signal, (char *)signal);
  g_signal_connect (menuitem, "activate",
                    G_CALLBACK (activate_cb), self);

  gtk_widget_set_sensitive (menuitem, sensitive);
  
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}
        
static void
popup_menu_detach (GtkWidget *attach_widget,
                   GtkMenu   *menu)
{
  GtkText *self_attach = GTK_TEXT (attach_widget);
  GtkTextPrivate *priv_attach = gtk_text_get_instance_private (self_attach);

  priv_attach->popup_menu = NULL;
}

static void
gtk_text_do_popup (GtkText        *self,
                   const GdkEvent *event)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkEvent *trigger_event;

  /* In order to know what entries we should make sensitive, we
   * ask for the current targets of the clipboard, and when
   * we get them, then we actually pop up the menu.
   */
  trigger_event = event ? gdk_event_copy (event) : gtk_get_current_event ();

  if (gtk_widget_get_realized (GTK_WIDGET (self)))
    {
      DisplayMode mode;
      gboolean clipboard_contains_text;
      GtkWidget *menu;
      GtkWidget *menuitem;

      clipboard_contains_text = gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (gtk_widget_get_clipboard (GTK_WIDGET (self))),
                                                                   G_TYPE_STRING);
      if (priv->popup_menu)
        gtk_widget_destroy (priv->popup_menu);

      priv->popup_menu = menu = gtk_menu_new ();
      gtk_style_context_add_class (gtk_widget_get_style_context (menu),
                                   GTK_STYLE_CLASS_CONTEXT_MENU);

      gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (self), popup_menu_detach);

      mode = gtk_text_get_display_mode (self);
      append_action_signal (self, menu, _("Cu_t"), "cut-clipboard",
                            priv->editable && mode == DISPLAY_NORMAL &&
                            priv->current_pos != priv->selection_bound);

      append_action_signal (self, menu, _("_Copy"), "copy-clipboard",
                            mode == DISPLAY_NORMAL &&
                            priv->current_pos != priv->selection_bound);

      append_action_signal (self, menu, _("_Paste"), "paste-clipboard",
                            priv->editable && clipboard_contains_text);

      menuitem = gtk_menu_item_new_with_mnemonic (_("_Delete"));
      gtk_widget_set_sensitive (menuitem, priv->editable && priv->current_pos != priv->selection_bound);
      g_signal_connect_swapped (menuitem, "activate",
                                G_CALLBACK (gtk_text_delete_cb), self);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      menuitem = gtk_separator_menu_item_new ();
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      menuitem = gtk_menu_item_new_with_mnemonic (_("Select _All"));
      gtk_widget_set_sensitive (menuitem, gtk_entry_buffer_get_length (priv->buffer) > 0);
      g_signal_connect_swapped (menuitem, "activate",
                                G_CALLBACK (gtk_text_select_all), self);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      if ((gtk_text_get_input_hints (self) & GTK_INPUT_HINT_NO_EMOJI) == 0)
        {
          menuitem = gtk_menu_item_new_with_mnemonic (_("Insert _Emoji"));
          gtk_widget_set_sensitive (menuitem,
                                    mode == DISPLAY_NORMAL &&
                                    priv->editable);
          g_signal_connect_swapped (menuitem, "activate",
                                    G_CALLBACK (gtk_text_insert_emoji), self);
          gtk_widget_show (menuitem);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        }

      g_signal_emit (self, signals[POPULATE_POPUP], 0, menu);

      if (trigger_event && gdk_event_triggers_context_menu (trigger_event))
        gtk_menu_popup_at_pointer (GTK_MENU (menu), trigger_event);
      else
        {
          gtk_menu_popup_at_widget (GTK_MENU (menu),
                                    GTK_WIDGET (self),
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
                      GtkText    *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
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
                    GtkText   *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  const char *signal;

  signal = g_object_get_qdata (G_OBJECT (item), quark_gtk_signal);
  gtk_widget_hide (priv->selection_bubble);
  if (strcmp (signal, "select-all") == 0)
    gtk_text_select_all (self);
  else
    g_signal_emit_by_name (self, signal);
}

static void
append_bubble_action (GtkText     *self,
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
  g_signal_connect (item, "clicked", G_CALLBACK (activate_bubble_cb), self);
  gtk_widget_set_sensitive (GTK_WIDGET (item), sensitive);
  gtk_widget_show (GTK_WIDGET (item));
  gtk_container_add (GTK_CONTAINER (toolbar), item);
}

static gboolean
gtk_text_selection_bubble_popup_show (gpointer user_data)
{
  GtkText *self = user_data;
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  cairo_rectangle_int_t rect;
  GtkAllocation allocation;
  int start_x, end_x;
  gboolean has_selection;
  gboolean has_clipboard;
  gboolean all_selected;
  DisplayMode mode;
  GtkWidget *box;
  GtkWidget *toolbar;
  int length;
  GtkAllocation text_allocation;

  gtk_text_get_text_allocation (self, &text_allocation);

  has_selection = priv->selection_bound != priv->current_pos;
  length = gtk_entry_buffer_get_length (get_buffer (self));
  all_selected = (priv->selection_bound == 0) && (priv->current_pos == length);

  if (!has_selection && !priv->editable)
    {
      priv->selection_bubble_timeout_id = 0;
      return G_SOURCE_REMOVE;
    }

  if (priv->selection_bubble)
    gtk_widget_destroy (priv->selection_bubble);

  priv->selection_bubble = gtk_popover_new (GTK_WIDGET (self));
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->selection_bubble),
                               GTK_STYLE_CLASS_TOUCH_SELECTION);
  gtk_popover_set_position (GTK_POPOVER (priv->selection_bubble), GTK_POS_BOTTOM);
  gtk_popover_set_autohide (GTK_POPOVER (priv->selection_bubble), FALSE);
  g_signal_connect (priv->selection_bubble, "notify::visible",
                    G_CALLBACK (show_or_hide_handles), self);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  g_object_set (box, "margin", 10, NULL);
  gtk_widget_show (box);
  toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (toolbar), "linked");
  gtk_container_add (GTK_CONTAINER (priv->selection_bubble), box);
  gtk_container_add (GTK_CONTAINER (box), toolbar);

  has_clipboard = gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (gtk_widget_get_clipboard (GTK_WIDGET (self))),
                                                     G_TYPE_STRING);
  mode = gtk_text_get_display_mode (self);

  if (priv->editable && has_selection && mode == DISPLAY_NORMAL)
    append_bubble_action (self, toolbar, _("Select all"), "edit-select-all-symbolic", "select-all", !all_selected);

  if (priv->editable && has_selection && mode == DISPLAY_NORMAL)
    append_bubble_action (self, toolbar, _("Cut"), "edit-cut-symbolic", "cut-clipboard", TRUE);

  if (has_selection && mode == DISPLAY_NORMAL)
    append_bubble_action (self, toolbar, _("Copy"), "edit-copy-symbolic", "copy-clipboard", TRUE);

  if (priv->editable)
    append_bubble_action (self, toolbar, _("Paste"), "edit-paste-symbolic", "paste-clipboard", has_clipboard);

  if (priv->populate_all)
    g_signal_emit (self, signals[POPULATE_POPUP], 0, box);

  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);

  gtk_text_get_cursor_locations (self, &start_x, NULL);

  start_x -= priv->scroll_offset;
  start_x = CLAMP (start_x, 0, text_allocation.width);
  rect.y = text_allocation.y - allocation.y;
  rect.height = text_allocation.height;

  if (has_selection)
    {
      end_x = gtk_text_get_selection_bound_location (self) - priv->scroll_offset;
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
gtk_text_selection_bubble_popup_unset (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->selection_bubble)
    gtk_widget_hide (priv->selection_bubble);

  if (priv->selection_bubble_timeout_id)
    {
      g_source_remove (priv->selection_bubble_timeout_id);
      priv->selection_bubble_timeout_id = 0;
    }
}

static void
gtk_text_selection_bubble_popup_set (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->selection_bubble_timeout_id)
    g_source_remove (priv->selection_bubble_timeout_id);

  priv->selection_bubble_timeout_id =
    g_timeout_add (50, gtk_text_selection_bubble_popup_show, self);
  g_source_set_name_by_id (priv->selection_bubble_timeout_id, "[gtk] gtk_text_selection_bubble_popup_cb");
}

static void
gtk_text_drag_begin (GtkWidget *widget,
                     GdkDrag   *drag)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  char *text;

  text = _gtk_text_get_selected_text (self);

  if (text)
    {
      int *ranges, n_ranges;
      GdkPaintable *paintable;

      paintable = gtk_text_util_create_drag_icon (widget, text, -1);
      gtk_text_get_pixel_ranges (self, &ranges, &n_ranges);

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
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

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
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkAtom target = NULL;

  if (priv->editable)
    target = gtk_drag_dest_find_target (widget, drop, NULL);

  if (target != NULL)
    {
      priv->drop_position = gtk_text_find_position (self, x + priv->scroll_offset);
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
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkDragAction suggested_action;
  int new_position, old_position;

  old_position = priv->dnd_position;
  new_position = gtk_text_find_position (self, x + priv->scroll_offset);

  if (priv->editable &&
      gtk_drag_dest_find_target (widget, drop, NULL) != NULL)
    {
      suggested_action = GDK_ACTION_COPY | GDK_ACTION_MOVE;

      if (priv->selection_bound == priv->current_pos ||
          new_position < priv->selection_bound ||
          new_position > priv->current_pos)
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
gtk_text_get_action (GtkText *self,
                     GdkDrop *drop)
{
  GtkWidget *widget = GTK_WIDGET (self);
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
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkDragAction action;
  char *str;

  str = (char *) gtk_selection_data_get_text (selection_data);
  action = gtk_text_get_action (self, drop);

  if (action && str && priv->editable)
    {
      int length = -1;
      int pos;

      if (priv->truncate_multiline)
        length = truncate_multiline (str);

      if (priv->selection_bound == priv->current_pos ||
          priv->drop_position < priv->selection_bound ||
          priv->drop_position > priv->current_pos)
        {
          gtk_text_insert_text (self, str, length, &priv->drop_position);
        }
      else
        {
          /* Replacing selection */
          begin_change (self);
          gtk_text_delete_selection (self);
          pos = MIN (priv->selection_bound, priv->current_pos);
          gtk_text_insert_text (self, str, length, &pos);
          end_change (self);
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
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->selection_bound != priv->current_pos)
    {
      char *str = gtk_text_get_display_text (self, priv->selection_bound, priv->current_pos);

      gtk_selection_data_set_text (selection_data, str, -1);
      
      g_free (str);
    }
}

static void
gtk_text_drag_data_delete (GtkWidget *widget,
                           GdkDrag   *drag)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->editable &&
      priv->selection_bound != priv->current_pos)
    gtk_text_delete_selection (self);
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
cursor_blinks (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (gtk_widget_has_focus (GTK_WIDGET (self)) &&
      priv->editable &&
      priv->selection_bound == priv->current_pos)
    {
      GtkSettings *settings;
      gboolean blink;

      settings = gtk_widget_get_settings (GTK_WIDGET (self));
      g_object_get (settings, "gtk-cursor-blink", &blink, NULL);

      return blink;
    }
  else
    return FALSE;
}

static gboolean
get_middle_click_paste (GtkText *self)
{
  GtkSettings *settings;
  gboolean paste;

  settings = gtk_widget_get_settings (GTK_WIDGET (self));
  g_object_get (settings, "gtk-enable-primary-paste", &paste, NULL);

  return paste;
}

static int
get_cursor_time (GtkText *self)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (self));
  int time;

  g_object_get (settings, "gtk-cursor-blink-time", &time, NULL);

  return time;
}

static int
get_cursor_blink_timeout (GtkText *self)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (self));
  int timeout;

  g_object_get (settings, "gtk-cursor-blink-timeout", &timeout, NULL);

  return timeout;
}

static void
show_cursor (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkWidget *widget;

  if (!priv->cursor_visible)
    {
      priv->cursor_visible = TRUE;

      widget = GTK_WIDGET (self);
      if (gtk_widget_has_focus (widget) && priv->selection_bound == priv->current_pos)
        gtk_widget_queue_draw (widget);
    }
}

static void
hide_cursor (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkWidget *widget;

  if (priv->cursor_visible)
    {
      priv->cursor_visible = FALSE;

      widget = GTK_WIDGET (self);
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
  GtkText *self = data;
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int blink_timeout;

  if (!gtk_widget_has_focus (GTK_WIDGET (self)))
    {
      g_warning ("GtkText - did not receive a focus-out event.\n"
                 "If you handle this event, you must return\n"
                 "GDK_EVENT_PROPAGATE so the self gets the event as well");

      gtk_text_check_cursor_blink (self);

      return G_SOURCE_REMOVE;
    }
  
  g_assert (priv->selection_bound == priv->current_pos);
  
  blink_timeout = get_cursor_blink_timeout (self);
  if (priv->blink_time > 1000 * blink_timeout && 
      blink_timeout < G_MAXINT/1000) 
    {
      /* we've blinked enough without the user doing anything, stop blinking */
      show_cursor (self);
      priv->blink_timeout = 0;
    } 
  else if (priv->cursor_visible)
    {
      hide_cursor (self);
      priv->blink_timeout = g_timeout_add (get_cursor_time (self) * CURSOR_OFF_MULTIPLIER / CURSOR_DIVIDER,
                                           blink_cb,
                                           self);
      g_source_set_name_by_id (priv->blink_timeout, "[gtk] blink_cb");
    }
  else
    {
      show_cursor (self);
      priv->blink_time += get_cursor_time (self);
      priv->blink_timeout = g_timeout_add (get_cursor_time (self) * CURSOR_ON_MULTIPLIER / CURSOR_DIVIDER,
                                           blink_cb,
                                           self);
      g_source_set_name_by_id (priv->blink_timeout, "[gtk] blink_cb");
    }

  return G_SOURCE_REMOVE;
}

static void
gtk_text_check_cursor_blink (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (cursor_blinks (self))
    {
      if (!priv->blink_timeout)
        {
          show_cursor (self);
          priv->blink_timeout = g_timeout_add (get_cursor_time (self) * CURSOR_ON_MULTIPLIER / CURSOR_DIVIDER,
                                               blink_cb,
                                               self);
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
gtk_text_pend_cursor_blink (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (cursor_blinks (self))
    {
      if (priv->blink_timeout != 0)
        g_source_remove (priv->blink_timeout);

      priv->blink_timeout = g_timeout_add (get_cursor_time (self) * CURSOR_PEND_MULTIPLIER / CURSOR_DIVIDER,
                                           blink_cb,
                                           self);
      g_source_set_name_by_id (priv->blink_timeout, "[gtk] blink_cb");
      show_cursor (self);
    }
}

static void
gtk_text_reset_blink_time (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  priv->blink_time = 0;
}

/**
 * gtk_text_set_placeholder_text:
 * @self: a #GtkText
 * @text: (nullable): a string to be displayed when @self is empty and unfocused, or %NULL
 *
 * Sets text to be displayed in @self when it is empty.
 *
 * This can be used to give a visual hint of the expected
 * contents of the self.
 **/
void
gtk_text_set_placeholder_text (GtkText    *self,
                               const char *text)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (priv->placeholder == NULL)
    {
      priv->placeholder = g_object_new (GTK_TYPE_LABEL,
                                        "label", text,
                                        "css-name", "placeholder",
                                        "xalign", 0.0f,
                                        "ellipsize", PANGO_ELLIPSIZE_END,
                                        NULL);
      gtk_widget_insert_after (priv->placeholder, GTK_WIDGET (self), NULL);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (priv->placeholder), text);
    }

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_PLACEHOLDER_TEXT]);
}

/**
 * gtk_text_get_placeholder_text:
 * @self: a #GtkText
 *
 * Retrieves the text that will be displayed when @self is empty and unfocused
 *
 * Returns: (nullable) (transfer none):a pointer to the placeholder text as a string.
 *   This string points to internally allocated storage in the widget and must
 *   not be freed, modified or stored. If no placeholder text has been set,
 *   %NULL will be returned.
 **/
const char *
gtk_text_get_placeholder_text (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), NULL);

  if (!priv->placeholder)
    return NULL;

  return gtk_label_get_text (GTK_LABEL (priv->placeholder));
}

/**
 * gtk_text_set_input_purpose:
 * @self: a #GtkText
 * @purpose: the purpose
 *
 * Sets the #GtkText:input-purpose property which
 * can be used by on-screen keyboards and other input
 * methods to adjust their behaviour.
 */
void
gtk_text_set_input_purpose (GtkText         *self,
                            GtkInputPurpose  purpose)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (gtk_text_get_input_purpose (self) != purpose)
    {
      g_object_set (G_OBJECT (priv->im_context),
                    "input-purpose", purpose,
                    NULL);

      g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INPUT_PURPOSE]);
    }
}

/**
 * gtk_text_get_input_purpose:
 * @self: a #GtkText
 *
 * Gets the value of the #GtkText:input-purpose property.
 */
GtkInputPurpose
gtk_text_get_input_purpose (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkInputPurpose purpose;

  g_return_val_if_fail (GTK_IS_TEXT (self), GTK_INPUT_PURPOSE_FREE_FORM);

  g_object_get (G_OBJECT (priv->im_context),
                "input-purpose", &purpose,
                NULL);

  return purpose;
}

/**
 * gtk_text_set_input_hints:
 * @self: a #GtkText
 * @hints: the hints
 *
 * Sets the #GtkText:input-hints property, which
 * allows input methods to fine-tune their behaviour.
 */
void
gtk_text_set_input_hints (GtkText       *self,
                          GtkInputHints  hints)

{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (gtk_text_get_input_hints (self) != hints)
    {
      g_object_set (G_OBJECT (priv->im_context),
                    "input-hints", hints,
                    NULL);

      g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INPUT_HINTS]);
    }
}

/**
 * gtk_text_get_input_hints:
 * @self: a #GtkText
 *
 * Gets the value of the #GtkText:input-hints property.
 */
GtkInputHints
gtk_text_get_input_hints (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkInputHints hints;

  g_return_val_if_fail (GTK_IS_TEXT (self), GTK_INPUT_HINT_NONE);

  g_object_get (G_OBJECT (priv->im_context),
                "input-hints", &hints,
                NULL);

  return hints;
}

/**
 * gtk_text_set_attributes:
 * @self: a #GtkText
 * @attrs: a #PangoAttrList
 *
 * Sets a #PangoAttrList; the attributes in the list are applied to the
 * self text.
 */
void
gtk_text_set_attributes (GtkText       *self,
                         PangoAttrList *attrs)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  g_return_if_fail (GTK_IS_TEXT (self));

  if (attrs)
    pango_attr_list_ref (attrs);

  if (priv->attrs)
    pango_attr_list_unref (priv->attrs);
  priv->attrs = attrs;

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_ATTRIBUTES]);

  gtk_text_recompute (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_text_get_attributes:
 * @self: a #GtkText
 *
 * Gets the attribute list that was set on the self using
 * gtk_text_set_attributes(), if any.
 *
 * Returns: (transfer none) (nullable): the attribute list, or %NULL
 *     if none was set.
 */
PangoAttrList *
gtk_text_get_attributes (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), NULL);

  return priv->attrs;
}

/**
 * gtk_text_set_tabs:
 * @self: a #GtkText
 * @tabs: (nullable): a #PangoTabArray
 *
 * Sets a #PangoTabArray; the tabstops in the array are applied to the self
 * text.
 */

void
gtk_text_set_tabs (GtkText       *self,
                   PangoTabArray *tabs)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (priv->tabs)
    pango_tab_array_free(priv->tabs);

  if (tabs)
    priv->tabs = pango_tab_array_copy (tabs);
  else
    priv->tabs = NULL;

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_TABS]);

  gtk_text_recompute (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_text_get_tabs:
 * @self: a #GtkText
 *
 * Gets the tabstops that were set on the self using gtk_text_set_tabs(), if
 * any.
 *
 * Returns: (nullable) (transfer none): the tabstops, or %NULL if none was set.
 */
PangoTabArray *
gtk_text_get_tabs (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), NULL);

  return priv->tabs;
}

static void
emoji_picked (GtkEmojiChooser *chooser,
              const char      *text,
              GtkText         *self)
{
  int current_pos;
  int selection_bound;

  current_pos = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (chooser), "current-pos"));
  selection_bound = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (chooser), "selection-bound"));

  gtk_text_set_positions (self, current_pos, selection_bound);
  gtk_text_enter_text (self, text);
}

static void
gtk_text_insert_emoji (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkWidget *chooser;

  if (gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_EMOJI_CHOOSER) != NULL)
    return;

  chooser = GTK_WIDGET (g_object_get_data (G_OBJECT (self), "gtk-emoji-chooser"));
  if (!chooser)
    {
      chooser = gtk_emoji_chooser_new ();
      g_object_set_data (G_OBJECT (self), "gtk-emoji-chooser", chooser);

      gtk_popover_set_relative_to (GTK_POPOVER (chooser), GTK_WIDGET (self));
      g_signal_connect (chooser, "emoji-picked", G_CALLBACK (emoji_picked), self);
    }

  g_object_set_data (G_OBJECT (chooser), "current-pos", GINT_TO_POINTER (priv->current_pos));
  g_object_set_data (G_OBJECT (chooser), "selection-bound", GINT_TO_POINTER (priv->selection_bound));

  gtk_popover_popup (GTK_POPOVER (chooser));
}

static void
set_enable_emoji_completion (GtkText  *self,
                             gboolean  value)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->enable_emoji_completion == value)
    return;

  priv->enable_emoji_completion = value;

  if (priv->enable_emoji_completion)
    priv->emoji_completion = gtk_emoji_completion_new (self);
  else
    g_clear_pointer (&priv->emoji_completion, gtk_widget_unparent);

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_ENABLE_EMOJI_COMPLETION]);
}

static void
set_text_cursor (GtkWidget *widget)
{
  gtk_widget_set_cursor_from_name (widget, "text");
}

GtkEventController *
gtk_text_get_key_controller (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  return priv->key_controller;
}
