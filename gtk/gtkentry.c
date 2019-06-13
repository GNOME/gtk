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
#include "gtkeditable.h"
#include "gtkemojichooser.h"
#include "gtkemojicompletion.h"
#include "gtkentrybuffer.h"
#include "gtkgesturedrag.h"
#include "gtkimageprivate.h"
#include "gtkimcontextsimple.h"
#include "gtkintl.h"
#include "gtklabel.h"
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
#include "gtktextprivate.h"
#include "gtktexthandleprivate.h"
#include "gtktextutil.h"
#include "gtktooltip.h"
#include "gtktreeselection.h"
#include "gtktreeview.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindow.h"
#include "gtknative.h"
#include "gtkgestureclick.h"

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
 * gtk_entry_set_invisible_char().
 *
 * GtkEntry has the ability to display progress or activity
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
 * entry[.flat][.warning][.error]
 * ├── text[.readonly]
 * ├── image.left
 * ├── image.right
 * ├── [progress[.pulse]]
 * ]|
 *
 * GtkEntry has a main node with the name entry. Depending on the properties
 * of the entry, the style classes .read-only and .flat may appear. The style
 * classes .warning and .error may also be used with entries.
 *
 * When the entry shows icons, it adds subnodes with the name image and the
 * style class .left or .right, depending on where the icon appears.
 *
 * When the entry shows progress, it adds a subnode with the name progress.
 * The node has the style class .pulse when the shown progress is pulsing.
 *
 * For all the subnodes added to the text node in various situations,
 * see #GtkText.
 */

#define MAX_ICONS 2

#define IS_VALID_ICON_POSITION(pos)               \
  ((pos) == GTK_ENTRY_ICON_PRIMARY ||                   \
   (pos) == GTK_ENTRY_ICON_SECONDARY)

static GQuark          quark_entry_completion = 0;

typedef struct _EntryIconInfo EntryIconInfo;

typedef struct _GtkEntryPrivate       GtkEntryPrivate;
struct _GtkEntryPrivate
{
  EntryIconInfo *icons[MAX_ICONS];

  GtkWidget     *text;
  GtkWidget     *progress_widget;
  GtkWidget     *emoji_chooser;

  guint         show_emoji_icon         : 1;
  guint         editing_canceled        : 1; /* Only used by GtkCellRendererText */
};

struct _EntryIconInfo
{
  GtkWidget *widget;
  gchar *tooltip;
  guint nonactivatable : 1;
  guint in_drag        : 1;

  GdkDragAction actions;
  GdkContentFormats *target_list;
};

enum {
  ACTIVATE,
  ICON_PRESS,
  ICON_RELEASE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_MAX_LENGTH,
  PROP_VISIBILITY,
  PROP_HAS_FRAME,
  PROP_INVISIBLE_CHAR,
  PROP_ACTIVATES_DEFAULT,
  PROP_SCROLL_OFFSET,
  PROP_TRUNCATE_MULTILINE,
  PROP_OVERWRITE_MODE,
  PROP_TEXT_LENGTH,
  PROP_INVISIBLE_CHAR_SET,
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
  PROP_TABS,
  PROP_EXTRA_MENU,
  PROP_SHOW_EMOJI_ICON,
  PROP_ENABLE_EMOJI_COMPLETION,
  PROP_EDITING_CANCELED,
  NUM_PROPERTIES = PROP_EDITING_CANCELED,
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
static void   gtk_entry_size_allocate        (GtkWidget        *widget,
                                              int               width,
                                              int               height,
                                                int               baseline);
static void   gtk_entry_snapshot             (GtkWidget        *widget,
                                              GtkSnapshot      *snapshot);
static gboolean gtk_entry_query_tooltip      (GtkWidget        *widget,
                                              gint              x,
                                              gint              y,
                                              gboolean          keyboard_tip,
                                              GtkTooltip       *tooltip);
static void   gtk_entry_direction_changed    (GtkWidget        *widget,
					      GtkTextDirection  previous_dir);


/* GtkCellEditable method implementations
 */
static void gtk_entry_start_editing (GtkCellEditable *cell_editable,
				     GdkEvent        *event);

/* Default signal handlers
 */
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

static void
gtk_entry_grab_focus (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  gtk_widget_grab_focus (priv->text);
}

static gboolean
gtk_entry_mnemonic_activate (GtkWidget *widget,
                             gboolean   group_cycling)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  gtk_widget_grab_focus (priv->text);

  return TRUE;
}

static void
gtk_entry_class_init (GtkEntryClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  gobject_class->dispose = gtk_entry_dispose;
  gobject_class->finalize = gtk_entry_finalize;
  gobject_class->set_property = gtk_entry_set_property;
  gobject_class->get_property = gtk_entry_get_property;

  widget_class->measure = gtk_entry_measure;
  widget_class->size_allocate = gtk_entry_size_allocate;
  widget_class->snapshot = gtk_entry_snapshot;
  widget_class->query_tooltip = gtk_entry_query_tooltip;
  widget_class->direction_changed = gtk_entry_direction_changed;
  widget_class->grab_focus = gtk_entry_grab_focus;
  widget_class->mnemonic_activate = gtk_entry_mnemonic_activate;
  
  quark_entry_completion = g_quark_from_static_string ("gtk-entry-completion-key");

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

  entry_props[PROP_SCROLL_OFFSET] =
      g_param_spec_int ("scroll-offset",
                        P_("Scroll offset"),
                        P_("Number of pixels of the entry scrolled off the screen to the left"),
                        0, G_MAXINT,
                        0,
                        GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

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

  /**
   * GtkEntry:extra-menu:
   *
   * A menu model whose contents will be appended to
   * the context menu.
   */
  entry_props[PROP_EXTRA_MENU] =
      g_param_spec_object ("extra-menu",
                           P_("Extra menu"),
                           P_("Model menu to append to the context menu"),
                           G_TYPE_MENU_MODEL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  entry_props[PROP_ENABLE_EMOJI_COMPLETION] =
      g_param_spec_boolean ("enable-emoji-completion",
                            P_("Enable Emoji completion"),
                            P_("Whether to suggest Emoji replacements"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, entry_props);
  g_object_class_override_property (gobject_class, PROP_EDITING_CANCELED, "editing-canceled");
  gtk_editable_install_properties (gobject_class, PROP_EDITING_CANCELED + 1);

  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkEntryClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkEntry::icon-press:
   * @entry: The entry on which the signal is emitted
   * @icon_pos: The position of the clicked icon
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
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_ENTRY_ICON_POSITION);

  /**
   * GtkEntry::icon-release:
   * @entry: The entry on which the signal is emitted
   * @icon_pos: The position of the clicked icon
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
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_ENTRY_ICON_POSITION);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_ENTRY_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("entry"));
}

static GtkEditable *
gtk_entry_get_delegate (GtkEditable *editable)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  return GTK_EDITABLE (priv->text);
}

static void
gtk_entry_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = gtk_entry_get_delegate;
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

  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
    return;

  switch (prop_id)
    {
    case PROP_BUFFER:
    case PROP_MAX_LENGTH:
    case PROP_VISIBILITY:
    case PROP_INVISIBLE_CHAR:
    case PROP_INVISIBLE_CHAR_SET:
    case PROP_ACTIVATES_DEFAULT:
    case PROP_TRUNCATE_MULTILINE:
    case PROP_OVERWRITE_MODE:
    case PROP_PLACEHOLDER_TEXT:
    case PROP_IM_MODULE:
    case PROP_INPUT_PURPOSE:
    case PROP_INPUT_HINTS:
    case PROP_ATTRIBUTES:
    case PROP_TABS:
    case PROP_ENABLE_EMOJI_COMPLETION:
      g_object_set_property (G_OBJECT (priv->text), pspec->name, value);
      break;

    case PROP_HAS_FRAME:
      gtk_entry_set_has_frame (entry, g_value_get_boolean (value));
      break;

    case PROP_PROGRESS_FRACTION:
      gtk_entry_set_progress_fraction (entry, g_value_get_double (value));
      break;

    case PROP_PROGRESS_PULSE_STEP:
      gtk_entry_set_progress_pulse_step (entry, g_value_get_double (value));
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

    case PROP_SHOW_EMOJI_ICON:
      set_show_emoji_icon (entry, g_value_get_boolean (value));
      break;

    case PROP_EXTRA_MENU:
      gtk_entry_set_extra_menu (entry, g_value_get_object (value));
      break;

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

  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  switch (prop_id)
    {
    case PROP_BUFFER:
    case PROP_IM_MODULE:
    case PROP_MAX_LENGTH:
    case PROP_VISIBILITY:
    case PROP_INVISIBLE_CHAR:
    case PROP_INVISIBLE_CHAR_SET:
    case PROP_ACTIVATES_DEFAULT:
    case PROP_SCROLL_OFFSET:
    case PROP_TRUNCATE_MULTILINE:
    case PROP_OVERWRITE_MODE:
    case PROP_PLACEHOLDER_TEXT:
    case PROP_INPUT_PURPOSE:
    case PROP_INPUT_HINTS:
    case PROP_ATTRIBUTES:
    case PROP_TABS:
    case PROP_ENABLE_EMOJI_COMPLETION:
      g_object_get_property (G_OBJECT (priv->text), pspec->name, value);
      break;

    case PROP_HAS_FRAME:
      g_value_set_boolean (value, gtk_entry_get_has_frame (entry));
      break;

    case PROP_TEXT_LENGTH:
      g_value_set_uint (value, gtk_entry_get_text_length (entry));
      break;

    case PROP_PROGRESS_FRACTION:
      g_value_set_double (value, gtk_entry_get_progress_fraction (entry));
      break;

    case PROP_PROGRESS_PULSE_STEP:
      g_value_set_double (value, gtk_entry_get_progress_pulse_step (entry));
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

    case PROP_SHOW_EMOJI_ICON:
      g_value_set_boolean (value, priv->show_emoji_icon);
      break;

    case PROP_EXTRA_MENU:
      g_value_set_object (value, gtk_entry_get_extra_menu (entry));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
activate_cb (GtkText *text, GtkEntry  *entry)
{
  g_signal_emit (entry, signals[ACTIVATE], 0);
}

static void
notify_cb (GObject    *object,
           GParamSpec *pspec,
           gpointer    data)
{
  gpointer iface;
  gpointer class;

  /* The editable interface properties are already forwarded by the editable delegate setup */
  iface = g_type_interface_peek (g_type_class_peek (G_OBJECT_TYPE (object)), gtk_editable_get_type ());
  class = g_type_class_peek (GTK_TYPE_ENTRY);
  if (!g_object_interface_find_property (iface, pspec->name) &&
      g_object_class_find_property (class, pspec->name))
    g_object_notify (data, pspec->name);
}

static void
connect_text_signals (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_signal_connect (priv->text, "activate", G_CALLBACK (activate_cb), entry);
  g_signal_connect (priv->text, "notify", G_CALLBACK (notify_cb), entry);
}

static void
disconnect_text_signals (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_signal_handlers_disconnect_by_func (priv->text, activate_cb, entry);
  g_signal_handlers_disconnect_by_func (priv->text, notify_cb, entry);
}

static void
gtk_entry_init (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  priv->text = gtk_text_new ();
  gtk_widget_set_parent (priv->text, GTK_WIDGET (entry));
  gtk_editable_init_delegate (GTK_EDITABLE (entry));
  connect_text_signals (entry);

  priv->editing_canceled = FALSE;
}

static void
gtk_entry_dispose (GObject *object)
{
  GtkEntry *entry = GTK_ENTRY (object);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  gtk_entry_set_completion (entry, NULL);

  if (priv->text)
    {
      disconnect_text_signals (entry);
      gtk_editable_finish_delegate (GTK_EDITABLE (entry));
    }
  g_clear_pointer (&priv->text, gtk_widget_unparent);

  g_clear_pointer (&priv->emoji_chooser, gtk_widget_unparent);

  gtk_entry_set_icon_from_paintable (entry, GTK_ENTRY_ICON_PRIMARY, NULL);
  gtk_entry_set_icon_tooltip_markup (entry, GTK_ENTRY_ICON_PRIMARY, NULL);
  gtk_entry_set_icon_from_paintable (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
  gtk_entry_set_icon_tooltip_markup (entry, GTK_ENTRY_ICON_SECONDARY, NULL);

  G_OBJECT_CLASS (gtk_entry_parent_class)->dispose (object);
}

static void
gtk_entry_finalize (GObject *object)
{
  GtkEntry *entry = GTK_ENTRY (object);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info = NULL;
  gint i;

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

  g_clear_pointer (&priv->progress_widget, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_entry_parent_class)->finalize (object);
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

static GtkEntryIconPosition
get_icon_position_from_controller (GtkEntry           *entry,
                                   GtkEventController *controller)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);

  if (priv->icons[GTK_ENTRY_ICON_PRIMARY] &&
      priv->icons[GTK_ENTRY_ICON_PRIMARY]->widget == widget)
    return GTK_ENTRY_ICON_PRIMARY;
  else if (priv->icons[GTK_ENTRY_ICON_SECONDARY] &&
           priv->icons[GTK_ENTRY_ICON_SECONDARY]->widget == widget)
    return GTK_ENTRY_ICON_SECONDARY;

  g_assert_not_reached ();
  return -1;
}

static void
icon_pressed_cb (GtkGestureClick *gesture,
                 gint                  n_press,
                 gdouble               x,
                 gdouble               y,
                 GtkEntry             *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEntryIconPosition pos;
  EntryIconInfo *icon_info;

  pos = get_icon_position_from_controller (entry, GTK_EVENT_CONTROLLER (gesture));
  icon_info = priv->icons[pos];

  if (!icon_info->nonactivatable)
    g_signal_emit (entry, signals[ICON_PRESS], 0, pos);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
icon_released_cb (GtkGestureClick *gesture,
                  gint             n_press,
                  gdouble          x,
                  gdouble          y,
                  GtkEntry        *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEntryIconPosition pos;
  EntryIconInfo *icon_info;

  pos = get_icon_position_from_controller (entry, GTK_EVENT_CONTROLLER (gesture));
  icon_info = priv->icons[pos];

  if (!icon_info->nonactivatable)
    g_signal_emit (entry, signals[ICON_RELEASE], 0, pos);
}

static void
icon_drag_update_cb (GtkGestureDrag *gesture,
                     gdouble         x,
                     gdouble         y,
                     GtkEntry       *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gdouble start_x, start_y;
  GtkEntryIconPosition pos;
  EntryIconInfo *icon_info;

  gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);
  pos = get_icon_position_from_controller (entry, GTK_EVENT_CONTROLLER (gesture));
  icon_info = priv->icons[pos];

  if (icon_info->target_list != NULL &&
      gtk_drag_check_threshold (icon_info->widget,
                                start_x, start_y,
                                x, y))
    {
      icon_info->in_drag = TRUE;
      gtk_drag_begin (GTK_WIDGET (entry),
                      gtk_gesture_get_device (GTK_GESTURE (gesture)),
                      icon_info->target_list,
                      icon_info->actions,
                      start_x, start_y);
    }
}

static EntryIconInfo*
construct_icon_info (GtkWidget            *widget,
                     GtkEntryIconPosition  icon_pos)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;
  GtkGesture *drag, *press;

  g_return_val_if_fail (priv->icons[icon_pos] == NULL, NULL);

  icon_info = g_slice_new0 (EntryIconInfo);
  priv->icons[icon_pos] = icon_info;

  icon_info->widget = gtk_image_new ();
  gtk_widget_set_cursor_from_name (icon_info->widget, "default");
  if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
    gtk_widget_insert_before (icon_info->widget, widget, priv->text);
  else
    gtk_widget_insert_after (icon_info->widget, widget, priv->text);

  update_icon_style (widget, icon_pos);
  update_node_ordering (entry);

  press = gtk_gesture_click_new ();
  g_signal_connect (press, "pressed", G_CALLBACK (icon_pressed_cb), entry);
  g_signal_connect (press, "released", G_CALLBACK (icon_released_cb), entry);
  gtk_widget_add_controller (icon_info->widget, GTK_EVENT_CONTROLLER (press));

  drag = gtk_gesture_drag_new ();
  g_signal_connect (drag, "drag-update",
                    G_CALLBACK (icon_drag_update_cb), entry);
  gtk_widget_add_controller (icon_info->widget, GTK_EVENT_CONTROLLER (drag));

  gtk_gesture_group (press, drag);

  return icon_info;
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

  gtk_widget_measure (priv->text,
                      orientation,
                      for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);

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
gtk_entry_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  const gboolean is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  gint i;
  GtkAllocation text_alloc;

  text_alloc.x = 0;
  text_alloc.y = 0;
  text_alloc.width = width;
  text_alloc.height = height;

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];
      GtkAllocation icon_alloc;
      int icon_width;

      if (!icon_info)
        continue;

      gtk_widget_measure (icon_info->widget,
                          GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          NULL, &icon_width,
                          NULL, NULL);

      if ((is_rtl  && i == GTK_ENTRY_ICON_PRIMARY) ||
          (!is_rtl && i == GTK_ENTRY_ICON_SECONDARY))
        icon_alloc.x = width - icon_width;
      else
        icon_alloc.x = 0;
      icon_alloc.y = 0;
      icon_alloc.width = icon_width;
      icon_alloc.height = height;

      gtk_widget_size_allocate (icon_info->widget, &icon_alloc, baseline);

      text_alloc.width -= icon_width;

      if ((!is_rtl  && i == GTK_ENTRY_ICON_PRIMARY) ||
          (is_rtl && i == GTK_ENTRY_ICON_SECONDARY))
        text_alloc.x += icon_width;
    }

  gtk_widget_size_allocate (priv->text, &text_alloc, baseline);

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
      progress_alloc.y = height - nat;
      progress_alloc.width = width;
      progress_alloc.height = nat;

      gtk_widget_size_allocate (priv->progress_widget, &progress_alloc, -1);
    }

  /* Do this here instead of gtk_entry_size_allocate() so it works
   * inside spinbuttons, which don't chain up.
   */
  if (gtk_widget_get_realized (widget))
    {
      GtkEntryCompletion *completion;

      completion = gtk_entry_get_completion (entry);
      if (completion)
        _gtk_entry_completion_resize_popup (completion);
    }

  if (priv->emoji_chooser)
    gtk_native_check_resize (GTK_NATIVE (priv->emoji_chooser));
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

  gtk_widget_snapshot_child (widget, priv->text, snapshot);

  /* Draw icons */
  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];

      if (icon_info != NULL)
        gtk_widget_snapshot_child (widget, icon_info->widget, snapshot);
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
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  gtk_text_grab_focus_without_selecting (GTK_TEXT (priv->text));
}

static void
gtk_entry_direction_changed (GtkWidget        *widget,
                             GtkTextDirection  previous_dir)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  update_icon_style (widget, GTK_ENTRY_ICON_PRIMARY);
  update_icon_style (widget, GTK_ENTRY_ICON_SECONDARY);

  update_node_ordering (entry);

  GTK_WIDGET_CLASS (gtk_entry_parent_class)->direction_changed (widget, previous_dir);
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
gtk_cell_editable_entry_key_pressed (GtkEventControllerKey *key,
                                     guint                  keyval,
                                     guint                  keycode,
                                     GdkModifierType        modifiers,
                                     GtkEntry              *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

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
  g_signal_connect (gtk_entry_get_key_controller (GTK_ENTRY (cell_editable)),
                    "key-pressed",
                    G_CALLBACK (gtk_cell_editable_entry_key_pressed),
                    cell_editable);
}

/* Internal functions
 */

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

  gtk_text_reset_im_context (GTK_TEXT (priv->text));
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

  return gtk_text_get_buffer (GTK_TEXT (priv->text));
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

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_buffer (GTK_TEXT (priv->text), buffer);
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

  gtk_text_set_visibility (GTK_TEXT (priv->text), visible);
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

  return gtk_text_get_visibility (GTK_TEXT (priv->text));
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

  gtk_text_set_invisible_char (GTK_TEXT (priv->text), ch);
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

  return gtk_text_get_invisible_char (GTK_TEXT (priv->text));
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

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_unset_invisible_char (GTK_TEXT (priv->text));
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

  gtk_text_set_overwrite_mode (GTK_TEXT (priv->text), overwrite);
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

  return gtk_text_get_overwrite_mode (GTK_TEXT (priv->text));

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
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_max_length (GTK_TEXT (priv->text), max);
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
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return gtk_text_get_max_length (GTK_TEXT (priv->text));
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
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return gtk_text_get_text_length (GTK_TEXT (priv->text));
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
 **/
void
gtk_entry_set_activates_default (GtkEntry *entry,
                                 gboolean  setting)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_activates_default (GTK_TEXT (priv->text), setting);
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

  return gtk_text_get_activates_default (GTK_TEXT (priv->text));
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

  gtk_editable_set_alignment (GTK_EDITABLE (priv->text), xalign);
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

  return gtk_editable_get_alignment (GTK_EDITABLE (priv->text));
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
  graphene_rect_t r;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (icon_area != NULL);

  icon_info = priv->icons[icon_pos];

  if (icon_info &&
      gtk_widget_compute_bounds (icon_info->widget, GTK_WIDGET (entry), &r))
    {
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
  gtk_widget_set_can_target (priv->progress_widget, FALSE);

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
 * Sets text to be displayed in @entry when it is empty.
 * This can be used to give a visual hint of the expected contents of
 * the #GtkEntry.
 **/
void
gtk_entry_set_placeholder_text (GtkEntry    *entry,
                                const gchar *text)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_placeholder_text (GTK_TEXT (priv->text), text);
}

/**
 * gtk_entry_get_placeholder_text:
 * @entry: a #GtkEntry
 *
 * Retrieves the text that will be displayed when @entry is empty and unfocused
 *
 * Returns: (nullable) (transfer none):a pointer to the placeholder text as a string.
 *   This string points to internally allocated storage in the widget and must
 *   not be freed, modified or stored. If no placeholder text has been set,
 *   %NULL will be returned.
 **/
const gchar *
gtk_entry_get_placeholder_text (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return gtk_text_get_placeholder_text (GTK_TEXT (priv->text));
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

  gtk_text_set_input_purpose (GTK_TEXT (priv->text), purpose);
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

  g_return_val_if_fail (GTK_IS_ENTRY (entry), GTK_INPUT_PURPOSE_FREE_FORM);

  return gtk_text_get_input_purpose (GTK_TEXT (priv->text));
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

  gtk_text_set_input_hints (GTK_TEXT (priv->text), hints);
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

  g_return_val_if_fail (GTK_IS_ENTRY (entry), GTK_INPUT_HINT_NONE);

  return gtk_text_get_input_hints (GTK_TEXT (priv->text));
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

  gtk_text_set_attributes (GTK_TEXT (priv->text), attrs);
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

  return gtk_text_get_attributes (GTK_TEXT (priv->text));
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

  gtk_text_set_tabs (GTK_TEXT (priv->text), tabs);
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

  return gtk_text_get_tabs (GTK_TEXT (priv->text));
}

void
gtk_entry_enter_text (GtkEntry *entry,
                      const char *text)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  gtk_text_enter_text (GTK_TEXT (priv->text), text);
}

static void
pick_emoji (GtkEntry *entry,
            int       icon,
            GdkEvent *event,
            gpointer  data)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (gtk_widget_get_ancestor (GTK_WIDGET (entry), GTK_TYPE_EMOJI_CHOOSER) != NULL)
    return;

  if (icon == GTK_ENTRY_ICON_SECONDARY)
    g_signal_emit_by_name (priv->text, "insert-emoji");
}

static void
set_show_emoji_icon (GtkEntry *entry,
                     gboolean  value)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GActionGroup *actions;
  GAction *action;

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

#if 0
  actions = gtk_widget_get_action_group (priv->text, "context");
  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "insert-emoji");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                               priv->show_emoji_icon ||
                               (gtk_entry_get_input_hints (entry) & GTK_INPUT_HINT_NO_EMOJI) == 0);
#endif
}

GtkEventController *
gtk_entry_get_key_controller (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  return gtk_text_get_key_controller (GTK_TEXT (priv->text));
}

GtkText *
gtk_entry_get_text_widget (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  return GTK_TEXT (priv->text);
}

/**
 * gtk_entry_set_extra_menu:
 * @entry: a #GtkEntry
 * @model: (allow-none): a #GMenuModel
 *
 * Sets a menu model to add when constructing
 * the context menu for @entry.
 */
void
gtk_entry_set_extra_menu (GtkEntry   *entry,
                          GMenuModel *model)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_extra_menu (GTK_TEXT (priv->text), model);

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_EXTRA_MENU]);
}

/**
 * gtk_entry_get_extra_menu:
 * @self: a #GtkText
 *
 * Gets the menu model set with gtk_entry_set_extra_menu().
 *
 * Returns: (transfer none): (nullable): the menu model
 */
GMenuModel *
gtk_entry_get_extra_menu (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return gtk_text_get_extra_menu (GTK_TEXT (priv->text));
}
