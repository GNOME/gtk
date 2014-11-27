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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <math.h>
#include <string.h>

#include "gtklabel.h"
#include "gtklabelprivate.h"
#include "gtkaccellabel.h"
#include "gtkbindings.h"
#include "gtkbuildable.h"
#include "gtkclipboard.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkdnd.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenuitem.h"
#include "gtkmenushellprivate.h"
#include "gtknotebook.h"
#include "gtkpango.h"
#include "gtkprivate.h"
#include "gtkseparatormenuitem.h"
#include "gtkshow.h"
#include "gtkstylecontextprivate.h"
#include "gtktextutil.h"
#include "gtktooltip.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindow.h"

#include "a11y/gtklabelaccessibleprivate.h"

/* this is in case rint() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

/**
 * SECTION:gtklabel
 * @Short_description: A widget that displays a small to medium amount of text
 * @Title: GtkLabel
 *
 * The #GtkLabel widget displays a small amount of text. As the name
 * implies, most labels are used to label another widget such as a
 * #GtkButton, a #GtkMenuItem, or a #GtkComboBox.
 *
 * # GtkLabel as GtkBuildable
 *
 * The GtkLabel implementation of the GtkBuildable interface supports a
 * custom <attributes> element, which supports any number of <attribute>
 * elements. The <attribute> element has attributes named “name“, “value“,
 * “start“ and “end“ and allows you to specify #PangoAttribute values for
 * this label.
 *
 * An example of a UI definition fragment specifying Pango attributes:
 * |[
 * <object class="GtkLabel">
 *   <attributes>
 *     <attribute name="weight" value="PANGO_WEIGHT_BOLD"/>
 *     <attribute name="background" value="red" start="5" end="10"/>"
 *   </attributes>
 * </object>
 * ]|
 *
 * The start and end attributes specify the range of characters to which the
 * Pango attribute applies. If start and end are not specified, the attribute is
 * applied to the whole text. Note that specifying ranges does not make much
 * sense with translatable attributes. Use markup embedded in the translatable
 * content instead.
 *
 * # Mnemonics
 *
 * Labels may contain “mnemonics”. Mnemonics are
 * underlined characters in the label, used for keyboard navigation.
 * Mnemonics are created by providing a string with an underscore before
 * the mnemonic character, such as `"_File"`, to the
 * functions gtk_label_new_with_mnemonic() or
 * gtk_label_set_text_with_mnemonic().
 *
 * Mnemonics automatically activate any activatable widget the label is
 * inside, such as a #GtkButton; if the label is not inside the
 * mnemonic’s target widget, you have to tell the label about the target
 * using gtk_label_set_mnemonic_widget(). Here’s a simple example where
 * the label is inside a button:
 *
 * |[<!-- language="C" -->
 *   // Pressing Alt+H will activate this button
 *   button = gtk_button_new ();
 *   label = gtk_label_new_with_mnemonic ("_Hello");
 *   gtk_container_add (GTK_CONTAINER (button), label);
 * ]|
 *
 * There’s a convenience function to create buttons with a mnemonic label
 * already inside:
 *
 * |[<!-- language="C" -->
 *   // Pressing Alt+H will activate this button
 *   button = gtk_button_new_with_mnemonic ("_Hello");
 * ]|
 *
 * To create a mnemonic for a widget alongside the label, such as a
 * #GtkEntry, you have to point the label at the entry with
 * gtk_label_set_mnemonic_widget():
 *
 * |[<!-- language="C" -->
 *   // Pressing Alt+H will focus the entry
 *   entry = gtk_entry_new ();
 *   label = gtk_label_new_with_mnemonic ("_Hello");
 *   gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
 * ]|
 *
 * # Markup (styled text)
 *
 * To make it easy to format text in a label (changing colors,
 * fonts, etc.), label text can be provided in a simple
 * [markup format][PangoMarkupFormat].
 *
 * Here’s how to create a label with a small font:
 * |[<!-- language="C" -->
 *   label = gtk_label_new (NULL);
 *   gtk_label_set_markup (GTK_LABEL (label), "<small>Small text</small>");
 * ]|
 *
 * (See [complete documentation][PangoMarkupFormat] of available
 * tags in the Pango manual.)
 *
 * The markup passed to gtk_label_set_markup() must be valid; for example,
 * literal <, > and & characters must be escaped as &lt;, &gt;, and &amp;.
 * If you pass text obtained from the user, file, or a network to
 * gtk_label_set_markup(), you’ll want to escape it with
 * g_markup_escape_text() or g_markup_printf_escaped().
 *
 * Markup strings are just a convenient way to set the #PangoAttrList on
 * a label; gtk_label_set_attributes() may be a simpler way to set
 * attributes in some cases. Be careful though; #PangoAttrList tends to
 * cause internationalization problems, unless you’re applying attributes
 * to the entire string (i.e. unless you set the range of each attribute
 * to [0, %G_MAXINT)). The reason is that specifying the start_index and
 * end_index for a #PangoAttribute requires knowledge of the exact string
 * being displayed, so translations will cause problems.
 *
 * # Selectable labels
 *
 * Labels can be made selectable with gtk_label_set_selectable().
 * Selectable labels allow the user to copy the label contents to
 * the clipboard. Only labels that contain useful-to-copy information
 * — such as error messages — should be made selectable.
 *
 * # Text layout # {#label-text-layout}
 *
 * A label can contain any number of paragraphs, but will have
 * performance problems if it contains more than a small number.
 * Paragraphs are separated by newlines or other paragraph separators
 * understood by Pango.
 *
 * Labels can automatically wrap text if you call
 * gtk_label_set_line_wrap().
 *
 * gtk_label_set_justify() sets how the lines in a label align
 * with one another. If you want to set how the label as a whole
 * aligns in its available space, see the #GtkWidget::halign and
 * #GtkWidget:valign properties.
 *
 * The #GtkLabel:width-chars and #GtkLabel:max-width-chars properties
 * can be used to control the size allocation of ellipsized or wrapped
 * labels. For ellipsizing labels, if either is specified (and less
 * than the actual text size), it is used as the minimum width, and the actual
 * text size is used as the natural width of the label. For wrapping labels,
 * width-chars is used as the minimum width, if specified, and max-width-chars
 * is used as the natural width. Even if max-width-chars specified, wrapping
 * labels will be rewrapped to use all of the available width.
 *
 * Note that the interpretation of #GtkLabel:width-chars and
 * #GtkLabel:max-width-chars has changed a bit with the introduction of
 * [width-for-height geometry management.][geometry-management]
 *
 * # Links
 *
 * Since 2.18, GTK+ supports markup for clickable hyperlinks in addition
 * to regular Pango markup. The markup for links is borrowed from HTML,
 * using the `<a>` with “href“ and “title“ attributes. GTK+ renders links
 * similar to the way they appear in web browsers, with colored, underlined
 * text. The “title“ attribute is displayed as a tooltip on the link.
 *
 * An example looks like this:
 *
 * |[<!-- language="C" -->
 * const gchar *text =
 * "Go to the"
 * "<a href=\"http://www.gtk.org title="&lt;i&gt;Our&lt;/i&gt; website\">"
 * "GTK+ website</a> for more...";
 * gtk_label_set_markup (label, text);
 * ]|
 *
 * It is possible to implement custom handling for links and their tooltips with
 * the #GtkLabel::activate-link signal and the gtk_label_get_current_uri() function.
 */

struct _GtkLabelPrivate
{
  GtkLabelSelectionInfo *select_info;
  GtkWidget *mnemonic_widget;
  GtkWindow *mnemonic_window;

  PangoAttrList *attrs;
  PangoAttrList *markup_attrs;
  PangoLayout   *layout;

  GtkGesture    *drag_gesture;
  GtkGesture    *multipress_gesture;

  gchar   *label;
  gchar   *text;

  gdouble  angle;
  gfloat   xalign;
  gfloat   yalign;

  guint    mnemonics_visible  : 1;
  guint    jtype              : 2;
  guint    wrap               : 1;
  guint    use_underline      : 1;
  guint    use_markup         : 1;
  guint    ellipsize          : 3;
  guint    single_line_mode   : 1;
  guint    have_transform     : 1;
  guint    in_click           : 1;
  guint    wrap_mode          : 3;
  guint    pattern_set        : 1;
  guint    track_links        : 1;

  guint    mnemonic_keyval;

  gint     width_chars;
  gint     max_width_chars;
  gint     lines;
};

/* Notes about the handling of links:
 *
 * Links share the GtkLabelSelectionInfo struct with selectable labels.
 * There are some new fields for links. The links field contains the list
 * of GtkLabelLink structs that describe the links which are embedded in
 * the label. The active_link field points to the link under the mouse
 * pointer. For keyboard navigation, the “focus” link is determined by
 * finding the link which contains the selection_anchor position.
 * The link_clicked field is used with button press and release events
 * to ensure that pressing inside a link and releasing outside of it
 * does not activate the link.
 *
 * Links are rendered with the #GTK_STATE_FLAG_LINK/#GTK_STATE_FLAG_VISITED
 * state flags. When the mouse pointer is over a link, the pointer is changed
 * to indicate the link.
 *
 * Labels with links accept keyboard focus, and it is possible to move
 * the focus between the embedded links using Tab/Shift-Tab. The focus
 * is indicated by a focus rectangle that is drawn around the link text.
 * Pressing Enter activates the focussed link, and there is a suitable
 * context menu for links that can be opened with the Menu key. Pressing
 * Control-C copies the link URI to the clipboard.
 *
 * In selectable labels with links, link functionality is only available
 * when the selection is empty.
 */
typedef struct
{
  gchar *uri;
  gchar *title;     /* the title attribute, used as tooltip */
  gboolean visited; /* get set when the link is activated; this flag
                     * gets preserved over later set_markup() calls
                     */
  gint start;       /* position of the link in the PangoLayout */
  gint end;
} GtkLabelLink;

struct _GtkLabelSelectionInfo
{
  GdkWindow *window;
  gint selection_anchor;
  gint selection_end;
  GtkWidget *popup_menu;

  GList *links;
  GtkLabelLink *active_link;

  gint drag_start_x;
  gint drag_start_y;

  guint in_drag      : 1;
  guint select_words : 1;
  guint selectable   : 1;
  guint link_clicked : 1;
};

enum {
  MOVE_CURSOR,
  COPY_CLIPBOARD,
  POPULATE_POPUP,
  ACTIVATE_LINK,
  ACTIVATE_CURRENT_LINK,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_ATTRIBUTES,
  PROP_USE_MARKUP,
  PROP_USE_UNDERLINE,
  PROP_JUSTIFY,
  PROP_PATTERN,
  PROP_WRAP,
  PROP_WRAP_MODE,
  PROP_SELECTABLE,
  PROP_MNEMONIC_KEYVAL,
  PROP_MNEMONIC_WIDGET,
  PROP_CURSOR_POSITION,
  PROP_SELECTION_BOUND,
  PROP_ELLIPSIZE,
  PROP_WIDTH_CHARS,
  PROP_SINGLE_LINE_MODE,
  PROP_ANGLE,
  PROP_MAX_WIDTH_CHARS,
  PROP_TRACK_VISITED_LINKS,
  PROP_LINES,
  PROP_XALIGN,
  PROP_YALIGN
};

/* When rotating ellipsizable text we want the natural size to request 
 * more to ensure the label wont ever ellipsize in an allocation of full natural size.
 * */
#define ROTATION_ELLIPSIZE_PADDING 2

static guint signals[LAST_SIGNAL] = { 0 };

static void gtk_label_set_property      (GObject          *object,
					 guint             prop_id,
					 const GValue     *value,
					 GParamSpec       *pspec);
static void gtk_label_get_property      (GObject          *object,
					 guint             prop_id,
					 GValue           *value,
					 GParamSpec       *pspec);
static void gtk_label_finalize          (GObject          *object);
static void gtk_label_destroy           (GtkWidget        *widget);
static void gtk_label_size_allocate     (GtkWidget        *widget,
                                         GtkAllocation    *allocation);
static void gtk_label_state_flags_changed   (GtkWidget        *widget,
                                             GtkStateFlags     prev_state);
static void gtk_label_style_updated     (GtkWidget        *widget);
static gboolean gtk_label_draw          (GtkWidget        *widget,
                                         cairo_t          *cr);
static gboolean gtk_label_focus         (GtkWidget         *widget,
                                         GtkDirectionType   direction);

static void gtk_label_realize           (GtkWidget        *widget);
static void gtk_label_unrealize         (GtkWidget        *widget);
static void gtk_label_map               (GtkWidget        *widget);
static void gtk_label_unmap             (GtkWidget        *widget);

static gboolean gtk_label_motion            (GtkWidget        *widget,
					     GdkEventMotion   *event);
static gboolean gtk_label_leave_notify      (GtkWidget        *widget,
                                             GdkEventCrossing *event);

static void     gtk_label_grab_focus        (GtkWidget        *widget);

static gboolean gtk_label_query_tooltip     (GtkWidget        *widget,
                                             gint              x,
                                             gint              y,
                                             gboolean          keyboard_tip,
                                             GtkTooltip       *tooltip);

static void gtk_label_set_text_internal          (GtkLabel      *label,
						  gchar         *str);
static void gtk_label_set_label_internal         (GtkLabel      *label,
						  gchar         *str);
static void gtk_label_set_use_markup_internal    (GtkLabel      *label,
						  gboolean       val);
static void gtk_label_set_use_underline_internal (GtkLabel      *label,
						  gboolean       val);
static void gtk_label_set_uline_text_internal    (GtkLabel      *label,
						  const gchar   *str);
static void gtk_label_set_pattern_internal       (GtkLabel      *label,
				                  const gchar   *pattern,
                                                  gboolean       is_mnemonic);
static void gtk_label_set_markup_internal        (GtkLabel      *label,
						  const gchar   *str,
						  gboolean       with_uline);
static void gtk_label_recalculate                (GtkLabel      *label);
static void gtk_label_hierarchy_changed          (GtkWidget     *widget,
						  GtkWidget     *old_toplevel);
static void gtk_label_screen_changed             (GtkWidget     *widget,
						  GdkScreen     *old_screen);
static gboolean gtk_label_popup_menu             (GtkWidget     *widget);

static void gtk_label_create_window       (GtkLabel *label);
static void gtk_label_destroy_window      (GtkLabel *label);
static void gtk_label_ensure_select_info  (GtkLabel *label);
static void gtk_label_clear_select_info   (GtkLabel *label);
static void gtk_label_update_cursor       (GtkLabel *label);
static void gtk_label_clear_layout        (GtkLabel *label);
static void gtk_label_ensure_layout       (GtkLabel *label);
static void gtk_label_select_region_index (GtkLabel *label,
                                           gint      anchor_index,
                                           gint      end_index);


static gboolean gtk_label_mnemonic_activate (GtkWidget         *widget,
					     gboolean           group_cycling);
static void     gtk_label_setup_mnemonic    (GtkLabel          *label,
					     guint              last_key);
static void     gtk_label_drag_data_get     (GtkWidget         *widget,
					     GdkDragContext    *context,
					     GtkSelectionData  *selection_data,
					     guint              info,
					     guint              time);

static void     gtk_label_buildable_interface_init     (GtkBuildableIface *iface);
static gboolean gtk_label_buildable_custom_tag_start   (GtkBuildable     *buildable,
							GtkBuilder       *builder,
							GObject          *child,
							const gchar      *tagname,
							GMarkupParser    *parser,
							gpointer         *data);

static void     gtk_label_buildable_custom_finished    (GtkBuildable     *buildable,
							GtkBuilder       *builder,
							GObject          *child,
							const gchar      *tagname,
							gpointer          user_data);


static void connect_mnemonics_visible_notify    (GtkLabel   *label);
static gboolean      separate_uline_pattern     (const gchar  *str,
                                                 guint        *accel_key,
                                                 gchar       **new_str,
                                                 gchar       **pattern);


/* For selectable labels: */
static void gtk_label_move_cursor        (GtkLabel        *label,
					  GtkMovementStep  step,
					  gint             count,
					  gboolean         extend_selection);
static void gtk_label_copy_clipboard     (GtkLabel        *label);
static void gtk_label_select_all         (GtkLabel        *label);
static void gtk_label_do_popup           (GtkLabel        *label,
					  GdkEventButton  *event);
static gint gtk_label_move_forward_word  (GtkLabel        *label,
					  gint             start);
static gint gtk_label_move_backward_word (GtkLabel        *label,
					  gint             start);

/* For links: */
static void          gtk_label_clear_links      (GtkLabel  *label);
static gboolean      gtk_label_activate_link    (GtkLabel    *label,
                                                 const gchar *uri);
static void          gtk_label_activate_current_link (GtkLabel *label);
static GtkLabelLink *gtk_label_get_current_link (GtkLabel  *label);
static void          emit_activate_link         (GtkLabel     *label,
                                                 GtkLabelLink *link);

/* Event controller callbacks */
static void   gtk_label_multipress_gesture_pressed  (GtkGestureMultiPress *gesture,
                                                     gint                  n_press,
                                                     gdouble               x,
                                                     gdouble               y,
                                                     GtkLabel             *label);
static void   gtk_label_multipress_gesture_released (GtkGestureMultiPress *gesture,
                                                     gint                  n_press,
                                                     gdouble               x,
                                                     gdouble               y,
                                                     GtkLabel             *label);
static void   gtk_label_drag_gesture_begin          (GtkGestureDrag *gesture,
                                                     gdouble         start_x,
                                                     gdouble         start_y,
                                                     GtkLabel       *label);
static void   gtk_label_drag_gesture_update         (GtkGestureDrag *gesture,
                                                     gdouble         offset_x,
                                                     gdouble         offset_y,
                                                     GtkLabel       *label);

static GtkSizeRequestMode gtk_label_get_request_mode                (GtkWidget           *widget);
static void               gtk_label_get_preferred_width             (GtkWidget           *widget,
                                                                     gint                *minimum_size,
                                                                     gint                *natural_size);
static void               gtk_label_get_preferred_height            (GtkWidget           *widget,
                                                                     gint                *minimum_size,
                                                                     gint                *natural_size);
static void               gtk_label_get_preferred_width_for_height  (GtkWidget           *widget,
                                                                     gint                 height,
                                                                     gint                *minimum_width,
                                                                     gint                *natural_width);
static void               gtk_label_get_preferred_height_for_width  (GtkWidget           *widget,
                                                                     gint                 width,
                                                                     gint                *minimum_height,
                                                                     gint                *natural_height);
static void    gtk_label_get_preferred_height_and_baseline_for_width (GtkWidget          *widget,
								      gint                width,
								      gint               *minimum_height,
								      gint               *natural_height,
								      gint               *minimum_baseline,
								      gint               *natural_baseline);

static GtkBuildableIface *buildable_parent_iface = NULL;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
G_DEFINE_TYPE_WITH_CODE (GtkLabel, gtk_label, GTK_TYPE_MISC,
                         G_ADD_PRIVATE (GtkLabel)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_label_buildable_interface_init))
G_GNUC_END_IGNORE_DEPRECATIONS

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
gtk_label_class_init (GtkLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkBindingSet *binding_set;

  gobject_class->set_property = gtk_label_set_property;
  gobject_class->get_property = gtk_label_get_property;
  gobject_class->finalize = gtk_label_finalize;

  widget_class->destroy = gtk_label_destroy;
  widget_class->size_allocate = gtk_label_size_allocate;
  widget_class->state_flags_changed = gtk_label_state_flags_changed;
  widget_class->style_updated = gtk_label_style_updated;
  widget_class->query_tooltip = gtk_label_query_tooltip;
  widget_class->draw = gtk_label_draw;
  widget_class->realize = gtk_label_realize;
  widget_class->unrealize = gtk_label_unrealize;
  widget_class->map = gtk_label_map;
  widget_class->unmap = gtk_label_unmap;
  widget_class->motion_notify_event = gtk_label_motion;
  widget_class->leave_notify_event = gtk_label_leave_notify;
  widget_class->hierarchy_changed = gtk_label_hierarchy_changed;
  widget_class->screen_changed = gtk_label_screen_changed;
  widget_class->mnemonic_activate = gtk_label_mnemonic_activate;
  widget_class->drag_data_get = gtk_label_drag_data_get;
  widget_class->grab_focus = gtk_label_grab_focus;
  widget_class->popup_menu = gtk_label_popup_menu;
  widget_class->focus = gtk_label_focus;
  widget_class->get_request_mode = gtk_label_get_request_mode;
  widget_class->get_preferred_width = gtk_label_get_preferred_width;
  widget_class->get_preferred_height = gtk_label_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_label_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_label_get_preferred_height_for_width;
  widget_class->get_preferred_height_and_baseline_for_width = gtk_label_get_preferred_height_and_baseline_for_width;

  class->move_cursor = gtk_label_move_cursor;
  class->copy_clipboard = gtk_label_copy_clipboard;
  class->activate_link = gtk_label_activate_link;

  /**
   * GtkLabel::move-cursor:
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
		  G_STRUCT_OFFSET (GtkLabelClass, move_cursor),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM_INT_BOOLEAN,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_MOVEMENT_STEP,
		  G_TYPE_INT,
		  G_TYPE_BOOLEAN);

   /**
   * GtkLabel::copy-clipboard:
   * @label: the object which received the signal
   *
   * The ::copy-clipboard signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to copy the selection to the clipboard.
   *
   * The default binding for this signal is Ctrl-c.
   */ 
  signals[COPY_CLIPBOARD] =
    g_signal_new (I_("copy-clipboard"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkLabelClass, copy_clipboard),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  /**
   * GtkLabel::populate-popup:
   * @label: The label on which the signal is emitted
   * @menu: the menu that is being populated
   *
   * The ::populate-popup signal gets emitted before showing the
   * context menu of the label. Note that only selectable labels
   * have context menus.
   *
   * If you need to add items to the context menu, connect
   * to this signal and append your menuitems to the @menu.
   */
  signals[POPULATE_POPUP] =
    g_signal_new (I_("populate-popup"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkLabelClass, populate_popup),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_MENU);

    /**
     * GtkLabel::activate-current-link:
     * @label: The label on which the signal was emitted
     *
     * A [keybinding signal][GtkBindingSignal]
     * which gets emitted when the user activates a link in the label.
     *
     * Applications may also emit the signal with g_signal_emit_by_name()
     * if they need to control activation of URIs programmatically.
     *
     * The default bindings for this signal are all forms of the Enter key.
     *
     * Since: 2.18
     */
    signals[ACTIVATE_CURRENT_LINK] =
      g_signal_new_class_handler ("activate-current-link",
                                  G_TYPE_FROM_CLASS (gobject_class),
                                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                  G_CALLBACK (gtk_label_activate_current_link),
                                  NULL, NULL,
                                  _gtk_marshal_VOID__VOID,
                                  G_TYPE_NONE, 0);

    /**
     * GtkLabel::activate-link:
     * @label: The label on which the signal was emitted
     * @uri: the URI that is activated
     *
     * The signal which gets emitted to activate a URI.
     * Applications may connect to it to override the default behaviour,
     * which is to call gtk_show_uri().
     *
     * Returns: %TRUE if the link has been activated
     *
     * Since: 2.18
     */
    signals[ACTIVATE_LINK] =
      g_signal_new ("activate-link",
                    G_TYPE_FROM_CLASS (gobject_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkLabelClass, activate_link),
                    _gtk_boolean_handled_accumulator, NULL,
                    _gtk_marshal_BOOLEAN__STRING,
                    G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        P_("Label"),
                                                        P_("The text of the label"),
                                                        "",
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_ATTRIBUTES,
				   g_param_spec_boxed ("attributes",
						       P_("Attributes"),
						       P_("A list of style attributes to apply to the text of the label"),
						       PANGO_TYPE_ATTR_LIST,
						       GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_USE_MARKUP,
                                   g_param_spec_boolean ("use-markup",
							 P_("Use markup"),
							 P_("The text of the label includes XML markup. See pango_parse_markup()"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
                                   PROP_USE_UNDERLINE,
                                   g_param_spec_boolean ("use-underline",
							 P_("Use underline"),
							 P_("If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
				   PROP_JUSTIFY,
                                   g_param_spec_enum ("justify",
                                                      P_("Justification"),
                                                      P_("The alignment of the lines in the text of the label relative to each other. This does NOT affect the alignment of the label within its allocation. See GtkLabel:xalign for that"),
						      GTK_TYPE_JUSTIFICATION,
						      GTK_JUSTIFY_LEFT,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkLabel:xalign:
   *
   * The xalign property determines the horizontal aligment of the label text
   * inside the labels size allocation. Compare this to #GtkWidget:halign,
   * which determines how the labels size allocation is positioned in the
   * space available for the label.
   *
   * Since: 3.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_XALIGN,
                                   g_param_spec_float ("xalign",
                                                       P_("X align"),
                                                       P_("The horizontal alignment, from 0 (left) to 1 (right). Reversed for RTL layouts."),
                                                       0.0, 1.0, 0.5,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkLabel:yalign:
   *
   * The yalign property determines the vertical aligment of the label text
   * inside the labels size allocation. Compare this to #GtkWidget:valign,
   * which determines how the labels size allocation is positioned in the
   * space available for the label.
   *
   * Since: 3.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_YALIGN,
                                   g_param_spec_float ("yalign",
                                                       P_("Y align"),
                                                       P_("The vertical alignment, from 0 (top) to 1 (bottom)"),
                                                       0.0, 1.0, 0.5,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_PATTERN,
                                   g_param_spec_string ("pattern",
                                                        P_("Pattern"),
                                                        P_("A string with _ characters in positions correspond to characters in the text to underline"),
                                                        NULL,
                                                        GTK_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_WRAP,
                                   g_param_spec_boolean ("wrap",
                                                        P_("Line wrap"),
                                                        P_("If set, wrap lines if the text becomes too wide"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkLabel:wrap-mode:
   *
   * If line wrapping is on (see the #GtkLabel:wrap property) this controls 
   * how the line wrapping is done. The default is %PANGO_WRAP_WORD, which 
   * means wrap on word boundaries.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
                                   PROP_WRAP_MODE,
                                   g_param_spec_enum ("wrap-mode",
						      P_("Line wrap mode"),
						      P_("If wrap is set, controls how linewrapping is done"),
						      PANGO_TYPE_WRAP_MODE,
						      PANGO_WRAP_WORD,
						      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTABLE,
                                   g_param_spec_boolean ("selectable",
                                                        P_("Selectable"),
                                                        P_("Whether the label text can be selected with the mouse"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
                                   PROP_MNEMONIC_KEYVAL,
                                   g_param_spec_uint ("mnemonic-keyval",
						      P_("Mnemonic key"),
						      P_("The mnemonic accelerator key for this label"),
						      0,
						      G_MAXUINT,
						      GDK_KEY_VoidSymbol,
						      GTK_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_MNEMONIC_WIDGET,
                                   g_param_spec_object ("mnemonic-widget",
							P_("Mnemonic widget"),
							P_("The widget to be activated when the label's mnemonic "
							  "key is pressed"),
							GTK_TYPE_WIDGET,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_CURSOR_POSITION,
                                   g_param_spec_int ("cursor-position",
                                                     P_("Cursor Position"),
                                                     P_("The current position of the insertion cursor in chars"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READABLE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTION_BOUND,
                                   g_param_spec_int ("selection-bound",
                                                     P_("Selection Bound"),
                                                     P_("The position of the opposite end of the selection from the cursor in chars"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READABLE));
  
  /**
   * GtkLabel:ellipsize:
   *
   * The preferred place to ellipsize the string, if the label does 
   * not have enough room to display the entire string, specified as a 
   * #PangoEllipsizeMode. 
   *
   * Note that setting this property to a value other than 
   * %PANGO_ELLIPSIZE_NONE has the side-effect that the label requests 
   * only enough space to display the ellipsis "...". In particular, this 
   * means that ellipsizing labels do not work well in notebook tabs, unless 
   * the #GtkNotebook tab-expand child property is set to %TRUE. Other ways
   * to set a label's width are gtk_widget_set_size_request() and
   * gtk_label_set_width_chars().
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_ELLIPSIZE,
                                   g_param_spec_enum ("ellipsize",
                                                      P_("Ellipsize"),
                                                      P_("The preferred place to ellipsize the string, if the label does not have enough room to display the entire string"),
						      PANGO_TYPE_ELLIPSIZE_MODE,
						      PANGO_ELLIPSIZE_NONE,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkLabel:width-chars:
   *
   * The desired width of the label, in characters. If this property is set to
   * -1, the width will be calculated automatically.
   *
   * See the section on [text layout][label-text-layout]
   * for details of how #GtkLabel:width-chars and #GtkLabel:max-width-chars
   * determine the width of ellipsized and wrapped labels.
   *
   * Since: 2.6
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_WIDTH_CHARS,
                                   g_param_spec_int ("width-chars",
                                                     P_("Width In Characters"),
                                                     P_("The desired width of the label, in characters"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  /**
   * GtkLabel:single-line-mode:
   * 
   * Whether the label is in single line mode. In single line mode,
   * the height of the label does not depend on the actual text, it
   * is always set to ascent + descent of the font. This can be an
   * advantage in situations where resizing the label because of text 
   * changes would be distracting, e.g. in a statusbar.
   *
   * Since: 2.6
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SINGLE_LINE_MODE,
                                   g_param_spec_boolean ("single-line-mode",
                                                        P_("Single Line Mode"),
                                                        P_("Whether the label is in single line mode"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkLabel:angle:
   * 
   * The angle that the baseline of the label makes with the horizontal,
   * in degrees, measured counterclockwise. An angle of 90 reads from
   * from bottom to top, an angle of 270, from top to bottom. Ignored
   * if the label is selectable, wrapped, or ellipsized.
   *
   * Since: 2.6
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ANGLE,
                                   g_param_spec_double ("angle",
							P_("Angle"),
							P_("Angle at which the label is rotated"),
							0.0,
							360.0,
							0.0, 
							GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  /**
   * GtkLabel:max-width-chars:
   * 
   * The desired maximum width of the label, in characters. If this property 
   * is set to -1, the width will be calculated automatically.
   *
   * See the section on [text layout][label-text-layout]
   * for details of how #GtkLabel:width-chars and #GtkLabel:max-width-chars
   * determine the width of ellipsized and wrapped labels.
   *
   * Since: 2.6
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MAX_WIDTH_CHARS,
                                   g_param_spec_int ("max-width-chars",
                                                     P_("Maximum Width In Characters"),
                                                     P_("The desired maximum width of the label, in characters"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkLabel:track-visited-links:
   *
   * Set this property to %TRUE to make the label track which links
   * have been visited. It will then apply the #GTK_STATE_FLAG_VISITED
   * when rendering this link, in addition to #GTK_STATE_FLAG_LINK.
   *
   * Since: 2.18
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TRACK_VISITED_LINKS,
                                   g_param_spec_boolean ("track-visited-links",
                                                         P_("Track visited links"),
                                                         P_("Whether visited links should be tracked"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkLabel:lines:
   *
   * The number of lines to which an ellipsized, wrapping label
   * should be limited. This property has no effect if the
   * label is not wrapping or ellipsized. Set this property to
   * -1 if you don't want to limit the number of lines.
   *
   * Since: 3.10
   */
  g_object_class_install_property (gobject_class,
                                   PROP_LINES,
                                   g_param_spec_int ("lines",
                                                     P_("Number of lines"),
                                                     P_("The desired number of lines, when ellipsizing a wrapping label"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
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
  
  add_move_binding (binding_set, GDK_KEY_f, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_LOGICAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_KEY_b, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_LOGICAL_POSITIONS, -1);
  
  add_move_binding (binding_set, GDK_KEY_Right, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_KEY_Left, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (binding_set, GDK_KEY_KP_Right, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_KEY_KP_Left, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, -1);

  /* select all */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, GTK_MOVEMENT_PARAGRAPH_ENDS,
				G_TYPE_INT, -1,
				G_TYPE_BOOLEAN, FALSE);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, GTK_MOVEMENT_PARAGRAPH_ENDS,
				G_TYPE_INT, 1,
				G_TYPE_BOOLEAN, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_slash, GDK_CONTROL_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, GTK_MOVEMENT_PARAGRAPH_ENDS,
				G_TYPE_INT, -1,
				G_TYPE_BOOLEAN, FALSE);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_slash, GDK_CONTROL_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, GTK_MOVEMENT_PARAGRAPH_ENDS,
				G_TYPE_INT, 1,
				G_TYPE_BOOLEAN, TRUE);

  /* unselect all */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, GTK_MOVEMENT_PARAGRAPH_ENDS,
				G_TYPE_INT, 0,
				G_TYPE_BOOLEAN, FALSE);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_backslash, GDK_CONTROL_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, GTK_MOVEMENT_PARAGRAPH_ENDS,
				G_TYPE_INT, 0,
				G_TYPE_BOOLEAN, FALSE);

  add_move_binding (binding_set, GDK_KEY_f, GDK_MOD1_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_KEY_b, GDK_MOD1_MASK,
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

  /* copy */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_c, GDK_CONTROL_MASK,
				"copy-clipboard", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, 0,
				"activate-current-link", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_ISO_Enter, 0,
				"activate-current-link", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter, 0,
				"activate-current-link", 0);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_LABEL_ACCESSIBLE);
}

static void 
gtk_label_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkLabel *label = GTK_LABEL (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_label_set_label (label, g_value_get_string (value));
      break;
    case PROP_ATTRIBUTES:
      gtk_label_set_attributes (label, g_value_get_boxed (value));
      break;
    case PROP_USE_MARKUP:
      gtk_label_set_use_markup (label, g_value_get_boolean (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_label_set_use_underline (label, g_value_get_boolean (value));
      break;
    case PROP_JUSTIFY:
      gtk_label_set_justify (label, g_value_get_enum (value));
      break;
    case PROP_PATTERN:
      gtk_label_set_pattern (label, g_value_get_string (value));
      break;
    case PROP_WRAP:
      gtk_label_set_line_wrap (label, g_value_get_boolean (value));
      break;	  
    case PROP_WRAP_MODE:
      gtk_label_set_line_wrap_mode (label, g_value_get_enum (value));
      break;	  
    case PROP_SELECTABLE:
      gtk_label_set_selectable (label, g_value_get_boolean (value));
      break;	  
    case PROP_MNEMONIC_WIDGET:
      gtk_label_set_mnemonic_widget (label, (GtkWidget*) g_value_get_object (value));
      break;
    case PROP_ELLIPSIZE:
      gtk_label_set_ellipsize (label, g_value_get_enum (value));
      break;
    case PROP_WIDTH_CHARS:
      gtk_label_set_width_chars (label, g_value_get_int (value));
      break;
    case PROP_SINGLE_LINE_MODE:
      gtk_label_set_single_line_mode (label, g_value_get_boolean (value));
      break;	  
    case PROP_ANGLE:
      gtk_label_set_angle (label, g_value_get_double (value));
      break;
    case PROP_MAX_WIDTH_CHARS:
      gtk_label_set_max_width_chars (label, g_value_get_int (value));
      break;
    case PROP_TRACK_VISITED_LINKS:
      gtk_label_set_track_visited_links (label, g_value_get_boolean (value));
      break;
    case PROP_LINES:
      gtk_label_set_lines (label, g_value_get_int (value));
      break;
    case PROP_XALIGN:
      gtk_label_set_xalign (label, g_value_get_float (value));
      break;
    case PROP_YALIGN:
      gtk_label_set_yalign (label, g_value_get_float (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_label_get_property (GObject     *object,
			guint        prop_id,
			GValue      *value,
			GParamSpec  *pspec)
{
  GtkLabel *label = GTK_LABEL (object);
  GtkLabelPrivate *priv = label->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, priv->label);
      break;
    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, priv->attrs);
      break;
    case PROP_USE_MARKUP:
      g_value_set_boolean (value, priv->use_markup);
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, priv->use_underline);
      break;
    case PROP_JUSTIFY:
      g_value_set_enum (value, priv->jtype);
      break;
    case PROP_WRAP:
      g_value_set_boolean (value, priv->wrap);
      break;
    case PROP_WRAP_MODE:
      g_value_set_enum (value, priv->wrap_mode);
      break;
    case PROP_SELECTABLE:
      g_value_set_boolean (value, gtk_label_get_selectable (label));
      break;
    case PROP_MNEMONIC_KEYVAL:
      g_value_set_uint (value, priv->mnemonic_keyval);
      break;
    case PROP_MNEMONIC_WIDGET:
      g_value_set_object (value, (GObject*) priv->mnemonic_widget);
      break;
    case PROP_CURSOR_POSITION:
      g_value_set_int (value, _gtk_label_get_cursor_position (label));
      break;
    case PROP_SELECTION_BOUND:
      g_value_set_int (value, _gtk_label_get_selection_bound (label));
      break;
    case PROP_ELLIPSIZE:
      g_value_set_enum (value, priv->ellipsize);
      break;
    case PROP_WIDTH_CHARS:
      g_value_set_int (value, gtk_label_get_width_chars (label));
      break;
    case PROP_SINGLE_LINE_MODE:
      g_value_set_boolean (value, gtk_label_get_single_line_mode (label));
      break;
    case PROP_ANGLE:
      g_value_set_double (value, gtk_label_get_angle (label));
      break;
    case PROP_MAX_WIDTH_CHARS:
      g_value_set_int (value, gtk_label_get_max_width_chars (label));
      break;
    case PROP_TRACK_VISITED_LINKS:
      g_value_set_boolean (value, gtk_label_get_track_visited_links (label));
      break;
    case PROP_LINES:
      g_value_set_int (value, gtk_label_get_lines (label));
      break;
    case PROP_XALIGN:
      g_value_set_float (value, gtk_label_get_xalign (label));
      break;
    case PROP_YALIGN:
      g_value_set_float (value, gtk_label_get_yalign (label));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_label_init (GtkLabel *label)
{
  GtkLabelPrivate *priv;
  GtkStyleContext *context;

  label->priv = gtk_label_get_instance_private (label);
  priv = label->priv;

  gtk_widget_set_has_window (GTK_WIDGET (label), FALSE);

  priv->width_chars = -1;
  priv->max_width_chars = -1;
  priv->label = NULL;
  priv->lines = -1;

  priv->xalign = 0.5;
  priv->yalign = 0.5;

  priv->jtype = GTK_JUSTIFY_LEFT;
  priv->wrap = FALSE;
  priv->wrap_mode = PANGO_WRAP_WORD;
  priv->ellipsize = PANGO_ELLIPSIZE_NONE;

  priv->use_underline = FALSE;
  priv->use_markup = FALSE;
  priv->pattern_set = FALSE;
  priv->track_links = TRUE;

  priv->mnemonic_keyval = GDK_KEY_VoidSymbol;
  priv->layout = NULL;
  priv->text = NULL;
  priv->attrs = NULL;

  priv->mnemonic_widget = NULL;
  priv->mnemonic_window = NULL;

  priv->mnemonics_visible = TRUE;

  gtk_label_set_text (label, "");

  context = gtk_widget_get_style_context (GTK_WIDGET (label));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_LABEL);

  priv->drag_gesture = gtk_gesture_drag_new (GTK_WIDGET (label));
  g_signal_connect (priv->drag_gesture, "drag-begin",
                    G_CALLBACK (gtk_label_drag_gesture_begin), label);
  g_signal_connect (priv->drag_gesture, "drag-update",
                    G_CALLBACK (gtk_label_drag_gesture_update), label);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (priv->drag_gesture), TRUE);

  priv->multipress_gesture = gtk_gesture_multi_press_new (GTK_WIDGET (label));
  g_signal_connect (priv->multipress_gesture, "pressed",
                    G_CALLBACK (gtk_label_multipress_gesture_pressed), label);
  g_signal_connect (priv->multipress_gesture, "released",
                    G_CALLBACK (gtk_label_multipress_gesture_released), label);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->multipress_gesture), 0);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (priv->multipress_gesture), TRUE);
}


static void
gtk_label_buildable_interface_init (GtkBuildableIface *iface)
{
  buildable_parent_iface = g_type_interface_peek_parent (iface);

  iface->custom_tag_start = gtk_label_buildable_custom_tag_start;
  iface->custom_finished = gtk_label_buildable_custom_finished;
}

typedef struct {
  GtkBuilder    *builder;
  GObject       *object;
  PangoAttrList *attrs;
} PangoParserData;

static PangoAttribute *
attribute_from_text (GtkBuilder   *builder,
		     const gchar  *name, 
		     const gchar  *value,
		     GError      **error)
{
  PangoAttribute *attribute = NULL;
  PangoAttrType   type;
  PangoLanguage  *language;
  PangoFontDescription *font_desc;
  GdkColor       *color;
  GValue          val = G_VALUE_INIT;

  if (!gtk_builder_value_from_string_type (builder, PANGO_TYPE_ATTR_TYPE, name, &val, error))
    return NULL;

  type = g_value_get_enum (&val);
  g_value_unset (&val);

  switch (type)
    {
      /* PangoAttrLanguage */
    case PANGO_ATTR_LANGUAGE:
      if ((language = pango_language_from_string (value)))
	{
	  attribute = pango_attr_language_new (language);
	  g_value_init (&val, G_TYPE_INT);
	}
      break;
      /* PangoAttrInt */
    case PANGO_ATTR_STYLE:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_STYLE, value, &val, error))
	attribute = pango_attr_style_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_WEIGHT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_WEIGHT, value, &val, error))
	attribute = pango_attr_weight_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_VARIANT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_VARIANT, value, &val, error))
	attribute = pango_attr_variant_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_STRETCH:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_STRETCH, value, &val, error))
	attribute = pango_attr_stretch_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_UNDERLINE:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_UNDERLINE, value, &val, NULL))
	attribute = pango_attr_underline_new (g_value_get_enum (&val));
      else
        {
          /* XXX: allow boolean for backwards compat, so ignore error */
          /* Deprecate this somehow */
          g_value_unset (&val);
          if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
            attribute = pango_attr_underline_new (g_value_get_boolean (&val));
        }
      break;
    case PANGO_ATTR_STRIKETHROUGH:	
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
	attribute = pango_attr_strikethrough_new (g_value_get_boolean (&val));
      break;
    case PANGO_ATTR_GRAVITY:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_GRAVITY, value, &val, error))
	attribute = pango_attr_gravity_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_GRAVITY_HINT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_GRAVITY_HINT, 
					      value, &val, error))
	attribute = pango_attr_gravity_hint_new (g_value_get_enum (&val));
      break;
      /* PangoAttrString */	  
    case PANGO_ATTR_FAMILY:
      attribute = pango_attr_family_new (value);
      g_value_init (&val, G_TYPE_INT);
      break;

      /* PangoAttrSize */	  
    case PANGO_ATTR_SIZE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, 
					      value, &val, error))
	attribute = pango_attr_size_new (g_value_get_int (&val));
      break;
    case PANGO_ATTR_ABSOLUTE_SIZE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, 
					      value, &val, error))
	attribute = pango_attr_size_new_absolute (g_value_get_int (&val));
      break;
    
      /* PangoAttrFontDesc */
    case PANGO_ATTR_FONT_DESC:
      if ((font_desc = pango_font_description_from_string (value)))
	{
	  attribute = pango_attr_font_desc_new (font_desc);
	  pango_font_description_free (font_desc);
	  g_value_init (&val, G_TYPE_INT);
	}
      break;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

      /* PangoAttrColor */
    case PANGO_ATTR_FOREGROUND:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_COLOR,
					      value, &val, error))
	{
	  color = g_value_get_boxed (&val);
	  attribute = pango_attr_foreground_new (color->red, color->green, color->blue);
	}
      break;
    case PANGO_ATTR_BACKGROUND: 
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_COLOR, 
					      value, &val, error))
	{
	  color = g_value_get_boxed (&val);
	  attribute = pango_attr_background_new (color->red, color->green, color->blue);
	}
      break;
    case PANGO_ATTR_UNDERLINE_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_COLOR, 
					      value, &val, error))
	{
	  color = g_value_get_boxed (&val);
	  attribute = pango_attr_underline_color_new (color->red, color->green, color->blue);
	}
      break;
    case PANGO_ATTR_STRIKETHROUGH_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_COLOR, 
					      value, &val, error))
	{
	  color = g_value_get_boxed (&val);
	  attribute = pango_attr_strikethrough_color_new (color->red, color->green, color->blue);
	}
      break;

G_GNUC_END_IGNORE_DEPRECATIONS
      
      /* PangoAttrShape */
    case PANGO_ATTR_SHAPE:
      /* Unsupported for now */
      break;
      /* PangoAttrFloat */
    case PANGO_ATTR_SCALE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_DOUBLE, 
					      value, &val, error))
	attribute = pango_attr_scale_new (g_value_get_double (&val));
      break;

    case PANGO_ATTR_INVALID:
    case PANGO_ATTR_LETTER_SPACING:
    case PANGO_ATTR_RISE:
    case PANGO_ATTR_FALLBACK:
    default:
      break;
    }

  g_value_unset (&val);

  return attribute;
}


static void
pango_start_element (GMarkupParseContext *context,
		     const gchar         *element_name,
		     const gchar        **names,
		     const gchar        **values,
		     gpointer             user_data,
		     GError             **error)
{
  PangoParserData *data = (PangoParserData*)user_data;
  GValue val = G_VALUE_INIT;
  guint i;
  gint line_number, char_number;

  if (strcmp (element_name, "attribute") == 0)
    {
      PangoAttribute *attr = NULL;
      const gchar *name = NULL;
      const gchar *value = NULL;
      const gchar *start = NULL;
      const gchar *end = NULL;
      guint start_val = 0;
      guint end_val   = G_MAXUINT;

      for (i = 0; names[i]; i++)
	{
	  if (strcmp (names[i], "name") == 0)
	    name = values[i];
	  else if (strcmp (names[i], "value") == 0)
	    value = values[i];
	  else if (strcmp (names[i], "start") == 0)
	    start = values[i];
	  else if (strcmp (names[i], "end") == 0)
	    end = values[i];
	  else
	    {
	      g_markup_parse_context_get_position (context,
						   &line_number,
						   &char_number);
	      g_set_error (error,
			   GTK_BUILDER_ERROR,
			   GTK_BUILDER_ERROR_INVALID_ATTRIBUTE,
			   "%s:%d:%d '%s' is not a valid attribute of <%s>",
			   "<input>",
			   line_number, char_number, names[i], "attribute");
	      return;
	    }
	}

      if (!name || !value)
	{
	  g_markup_parse_context_get_position (context,
					       &line_number,
					       &char_number);
	  g_set_error (error,
		       GTK_BUILDER_ERROR,
		       GTK_BUILDER_ERROR_MISSING_ATTRIBUTE,
		       "%s:%d:%d <%s> requires attribute \"%s\"",
		       "<input>",
		       line_number, char_number, "attribute",
		       name ? "value" : "name");
	  return;
	}

      if (start)
	{
	  if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_UINT, 
						   start, &val, error))
	    return;
	  start_val = g_value_get_uint (&val);
	  g_value_unset (&val);
	}

      if (end)
	{
	  if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_UINT, 
						   end, &val, error))
	    return;
	  end_val = g_value_get_uint (&val);
	  g_value_unset (&val);
	}

      attr = attribute_from_text (data->builder, name, value, error);

      if (attr)
	{
          attr->start_index = start_val;
          attr->end_index   = end_val;

	  if (!data->attrs)
	    data->attrs = pango_attr_list_new ();

	  pango_attr_list_insert (data->attrs, attr);
	}
    }
  else if (strcmp (element_name, "attributes") == 0)
    ;
  else
    g_warning ("Unsupported tag for GtkLabel: %s\n", element_name);
}

static const GMarkupParser pango_parser =
  {
    pango_start_element,
  };

static gboolean
gtk_label_buildable_custom_tag_start (GtkBuildable     *buildable,
				      GtkBuilder       *builder,
				      GObject          *child,
				      const gchar      *tagname,
				      GMarkupParser    *parser,
				      gpointer         *data)
{
  if (buildable_parent_iface->custom_tag_start (buildable, builder, child, 
						tagname, parser, data))
    return TRUE;

  if (strcmp (tagname, "attributes") == 0)
    {
      PangoParserData *parser_data;

      parser_data = g_slice_new0 (PangoParserData);
      parser_data->builder = g_object_ref (builder);
      parser_data->object = g_object_ref (buildable);
      *parser = pango_parser;
      *data = parser_data;
      return TRUE;
    }
  return FALSE;
}

static void
gtk_label_buildable_custom_finished (GtkBuildable *buildable,
				     GtkBuilder   *builder,
				     GObject      *child,
				     const gchar  *tagname,
				     gpointer      user_data)
{
  PangoParserData *data;

  buildable_parent_iface->custom_finished (buildable, builder, child, 
					   tagname, user_data);

  if (strcmp (tagname, "attributes") == 0)
    {
      data = (PangoParserData*)user_data;

      if (data->attrs)
	{
	  gtk_label_set_attributes (GTK_LABEL (buildable), data->attrs);
	  pango_attr_list_unref (data->attrs);
	}

      g_object_unref (data->object);
      g_object_unref (data->builder);
      g_slice_free (PangoParserData, data);
    }
}


/**
 * gtk_label_new:
 * @str: (allow-none): The text of the label
 *
 * Creates a new label with the given text inside it. You can
 * pass %NULL to get an empty label widget.
 *
 * Returns: the new #GtkLabel
 **/
GtkWidget*
gtk_label_new (const gchar *str)
{
  GtkLabel *label;
  
  label = g_object_new (GTK_TYPE_LABEL, NULL);

  if (str && *str)
    gtk_label_set_text (label, str);
  
  return GTK_WIDGET (label);
}

/**
 * gtk_label_new_with_mnemonic:
 * @str: (allow-none): The text of the label, with an underscore in front of the
 *       mnemonic character
 *
 * Creates a new #GtkLabel, containing the text in @str.
 *
 * If characters in @str are preceded by an underscore, they are
 * underlined. If you need a literal underscore character in a label, use
 * '__' (two underscores). The first underlined character represents a 
 * keyboard accelerator called a mnemonic. The mnemonic key can be used 
 * to activate another widget, chosen automatically, or explicitly using
 * gtk_label_set_mnemonic_widget().
 * 
 * If gtk_label_set_mnemonic_widget() is not called, then the first 
 * activatable ancestor of the #GtkLabel will be chosen as the mnemonic 
 * widget. For instance, if the label is inside a button or menu item, 
 * the button or menu item will automatically become the mnemonic widget 
 * and be activated by the mnemonic.
 *
 * Returns: the new #GtkLabel
 **/
GtkWidget*
gtk_label_new_with_mnemonic (const gchar *str)
{
  GtkLabel *label;
  
  label = g_object_new (GTK_TYPE_LABEL, NULL);

  if (str && *str)
    gtk_label_set_text_with_mnemonic (label, str);
  
  return GTK_WIDGET (label);
}

static gboolean
gtk_label_mnemonic_activate (GtkWidget *widget,
			     gboolean   group_cycling)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;
  GtkWidget *parent;

  if (priv->mnemonic_widget)
    return gtk_widget_mnemonic_activate (priv->mnemonic_widget, group_cycling);

  /* Try to find the widget to activate by traversing the
   * widget's ancestry.
   */
  parent = gtk_widget_get_parent (widget);

  if (GTK_IS_NOTEBOOK (parent))
    return FALSE;
  
  while (parent)
    {
      if (gtk_widget_get_can_focus (parent) ||
	  (!group_cycling && GTK_WIDGET_GET_CLASS (parent)->activate_signal) ||
          GTK_IS_NOTEBOOK (gtk_widget_get_parent (parent)) ||
	  GTK_IS_MENU_ITEM (parent))
	return gtk_widget_mnemonic_activate (parent, group_cycling);
      parent = gtk_widget_get_parent (parent);
    }

  /* barf if there was nothing to activate */
  g_warning ("Couldn't find a target for a mnemonic activation.");
  gtk_widget_error_bell (widget);

  return FALSE;
}

static void
gtk_label_setup_mnemonic (GtkLabel *label,
			  guint     last_key)
{
  GtkLabelPrivate *priv = label->priv;
  GtkWidget *widget = GTK_WIDGET (label);
  GtkWidget *toplevel;
  GtkWidget *mnemonic_menu;
  
  mnemonic_menu = g_object_get_data (G_OBJECT (label), "gtk-mnemonic-menu");
  
  if (last_key != GDK_KEY_VoidSymbol)
    {
      if (priv->mnemonic_window)
	{
	  gtk_window_remove_mnemonic  (priv->mnemonic_window,
				       last_key,
				       widget);
	  priv->mnemonic_window = NULL;
	}
      if (mnemonic_menu)
	{
	  _gtk_menu_shell_remove_mnemonic (GTK_MENU_SHELL (mnemonic_menu),
					   last_key,
					   widget);
	  mnemonic_menu = NULL;
	}
    }
  
  if (priv->mnemonic_keyval == GDK_KEY_VoidSymbol)
      goto done;

  connect_mnemonics_visible_notify (GTK_LABEL (widget));

  toplevel = gtk_widget_get_toplevel (widget);
  if (gtk_widget_is_toplevel (toplevel))
    {
      GtkWidget *menu_shell;
      
      menu_shell = gtk_widget_get_ancestor (widget,
					    GTK_TYPE_MENU_SHELL);

      if (menu_shell)
	{
	  _gtk_menu_shell_add_mnemonic (GTK_MENU_SHELL (menu_shell),
					priv->mnemonic_keyval,
					widget);
	  mnemonic_menu = menu_shell;
	}
      
      if (!GTK_IS_MENU (menu_shell))
	{
	  gtk_window_add_mnemonic (GTK_WINDOW (toplevel),
				   priv->mnemonic_keyval,
				   widget);
	  priv->mnemonic_window = GTK_WINDOW (toplevel);
	}
    }
  
 done:
  g_object_set_data (G_OBJECT (label), I_("gtk-mnemonic-menu"), mnemonic_menu);
}

static void
gtk_label_hierarchy_changed (GtkWidget *widget,
			     GtkWidget *old_toplevel)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;

  gtk_label_setup_mnemonic (label, priv->mnemonic_keyval);
}

static void
label_shortcut_setting_apply (GtkLabel *label)
{
  gtk_label_recalculate (label);
  if (GTK_IS_ACCEL_LABEL (label))
    gtk_accel_label_refetch (GTK_ACCEL_LABEL (label));
}

static void
label_shortcut_setting_traverse_container (GtkWidget *widget,
                                           gpointer   data)
{
  if (GTK_IS_LABEL (widget))
    label_shortcut_setting_apply (GTK_LABEL (widget));
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
                          label_shortcut_setting_traverse_container, data);
}

static void
label_shortcut_setting_changed (GtkSettings *settings)
{
  GList *list, *l;

  list = gtk_window_list_toplevels ();

  for (l = list; l ; l = l->next)
    {
      GtkWidget *widget = l->data;

      if (gtk_widget_get_settings (widget) == settings)
        gtk_container_forall (GTK_CONTAINER (widget),
                              label_shortcut_setting_traverse_container, NULL);
    }

  g_list_free (list);
}

static void
mnemonics_visible_apply (GtkWidget *widget,
                         gboolean   mnemonics_visible)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;

  mnemonics_visible = mnemonics_visible != FALSE;

  if (priv->mnemonics_visible != mnemonics_visible)
    {
      priv->mnemonics_visible = mnemonics_visible;

      gtk_label_recalculate (label);
    }
}

static void
label_mnemonics_visible_traverse_container (GtkWidget *widget,
                                            gpointer   data)
{
  gboolean mnemonics_visible = GPOINTER_TO_INT (data);

  _gtk_label_mnemonics_visible_apply_recursively (widget, mnemonics_visible);
}

void
_gtk_label_mnemonics_visible_apply_recursively (GtkWidget *widget,
                                                gboolean   mnemonics_visible)
{
  if (GTK_IS_LABEL (widget))
    mnemonics_visible_apply (widget, mnemonics_visible);
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
                          label_mnemonics_visible_traverse_container,
                          GINT_TO_POINTER (mnemonics_visible));
}

static void
label_mnemonics_visible_changed (GtkWindow  *window,
                                 GParamSpec *pspec,
                                 gpointer    data)
{
  gboolean mnemonics_visible;

  g_object_get (window, "mnemonics-visible", &mnemonics_visible, NULL);

  gtk_container_forall (GTK_CONTAINER (window),
                        label_mnemonics_visible_traverse_container,
                        GINT_TO_POINTER (mnemonics_visible));
}

static void
gtk_label_screen_changed (GtkWidget *widget,
			  GdkScreen *old_screen)
{
  GtkSettings *settings;
  gboolean shortcuts_connected;

  /* The PangoContext is replaced when the screen changes, so clear the layouts */
  gtk_label_clear_layout (GTK_LABEL (widget));

  if (!gtk_widget_has_screen (widget))
    return;

  settings = gtk_widget_get_settings (widget);

  shortcuts_connected =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (settings),
                                        "gtk-label-shortcuts-connected"));

  if (! shortcuts_connected)
    {
      g_signal_connect (settings, "notify::gtk-enable-mnemonics",
                        G_CALLBACK (label_shortcut_setting_changed),
                        NULL);
      g_signal_connect (settings, "notify::gtk-enable-accels",
                        G_CALLBACK (label_shortcut_setting_changed),
                        NULL);

      g_object_set_data (G_OBJECT (settings), "gtk-label-shortcuts-connected",
                         GINT_TO_POINTER (TRUE));
    }

  label_shortcut_setting_apply (GTK_LABEL (widget));
}


static void
label_mnemonic_widget_weak_notify (gpointer      data,
				   GObject      *where_the_object_was)
{
  GtkLabel *label = data;
  GtkLabelPrivate *priv = label->priv;

  priv->mnemonic_widget = NULL;
  g_object_notify (G_OBJECT (label), "mnemonic-widget");
}

/**
 * gtk_label_set_mnemonic_widget:
 * @label: a #GtkLabel
 * @widget: (allow-none): the target #GtkWidget
 *
 * If the label has been set so that it has an mnemonic key (using
 * i.e. gtk_label_set_markup_with_mnemonic(),
 * gtk_label_set_text_with_mnemonic(), gtk_label_new_with_mnemonic()
 * or the “use_underline” property) the label can be associated with a
 * widget that is the target of the mnemonic. When the label is inside
 * a widget (like a #GtkButton or a #GtkNotebook tab) it is
 * automatically associated with the correct widget, but sometimes
 * (i.e. when the target is a #GtkEntry next to the label) you need to
 * set it explicitly using this function.
 *
 * The target widget will be accelerated by emitting the 
 * GtkWidget::mnemonic-activate signal on it. The default handler for 
 * this signal will activate the widget if there are no mnemonic collisions 
 * and toggle focus between the colliding widgets otherwise.
 **/
void
gtk_label_set_mnemonic_widget (GtkLabel  *label,
			       GtkWidget *widget)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  if (widget)
    g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->mnemonic_widget)
    {
      gtk_widget_remove_mnemonic_label (priv->mnemonic_widget, GTK_WIDGET (label));
      g_object_weak_unref (G_OBJECT (priv->mnemonic_widget),
			   label_mnemonic_widget_weak_notify,
			   label);
    }
  priv->mnemonic_widget = widget;
  if (priv->mnemonic_widget)
    {
      g_object_weak_ref (G_OBJECT (priv->mnemonic_widget),
		         label_mnemonic_widget_weak_notify,
		         label);
      gtk_widget_add_mnemonic_label (priv->mnemonic_widget, GTK_WIDGET (label));
    }
  
  g_object_notify (G_OBJECT (label), "mnemonic-widget");
}

/**
 * gtk_label_get_mnemonic_widget:
 * @label: a #GtkLabel
 *
 * Retrieves the target of the mnemonic (keyboard shortcut) of this
 * label. See gtk_label_set_mnemonic_widget().
 *
 * Returns: (transfer none): the target of the label’s mnemonic,
 *     or %NULL if none has been set and the default algorithm will be used.
 **/
GtkWidget *
gtk_label_get_mnemonic_widget (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->priv->mnemonic_widget;
}

/**
 * gtk_label_get_mnemonic_keyval:
 * @label: a #GtkLabel
 *
 * If the label has been set so that it has an mnemonic key this function
 * returns the keyval used for the mnemonic accelerator. If there is no
 * mnemonic set up it returns #GDK_KEY_VoidSymbol.
 *
 * Returns: GDK keyval usable for accelerators, or #GDK_KEY_VoidSymbol
 **/
guint
gtk_label_get_mnemonic_keyval (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), GDK_KEY_VoidSymbol);

  return label->priv->mnemonic_keyval;
}

static void
gtk_label_set_text_internal (GtkLabel *label,
                             gchar    *str)
{
  GtkLabelPrivate *priv = label->priv;

  if (g_strcmp0 (priv->text, str) == 0)
    {
      g_free (str);
      return;
    }

  _gtk_label_accessible_text_deleted (label);
  g_free (priv->text);
  priv->text = str;

  _gtk_label_accessible_text_inserted (label);

  gtk_label_select_region_index (label, 0, 0);
}

static void
gtk_label_set_label_internal (GtkLabel *label,
			      gchar    *str)
{
  GtkLabelPrivate *priv = label->priv;

  g_free (priv->label);

  priv->label = str;

  g_object_notify (G_OBJECT (label), "label");
}

static void
gtk_label_set_use_markup_internal (GtkLabel *label,
				   gboolean  val)
{
  GtkLabelPrivate *priv = label->priv;

  val = val != FALSE;
  if (priv->use_markup != val)
    {
      priv->use_markup = val;

      g_object_notify (G_OBJECT (label), "use-markup");
    }
}

static void
gtk_label_set_use_underline_internal (GtkLabel *label,
				      gboolean val)
{
  GtkLabelPrivate *priv = label->priv;

  val = val != FALSE;
  if (priv->use_underline != val)
    {
      priv->use_underline = val;

      g_object_notify (G_OBJECT (label), "use-underline");
    }
}

static gboolean
my_pango_attr_list_merge_filter (PangoAttribute *attribute,
                                 gpointer        list)
{
  pango_attr_list_change (list, pango_attribute_copy (attribute));
  return FALSE;
}

static void
my_pango_attr_list_merge (PangoAttrList *into,
                          PangoAttrList *from)
{
  pango_attr_list_filter (from, my_pango_attr_list_merge_filter, into);
}

/* Calculates text, attrs and mnemonic_keyval from
 * label, use_underline and use_markup
 */
static void
gtk_label_recalculate (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;
  guint keyval = priv->mnemonic_keyval;

  gtk_label_clear_links (label);

  if (priv->use_markup)
    gtk_label_set_markup_internal (label, priv->label, priv->use_underline);
  else if (priv->use_underline)
    gtk_label_set_uline_text_internal (label, priv->label);
  else
    {
      if (!priv->pattern_set)
        {
          if (priv->markup_attrs)
            pango_attr_list_unref (priv->markup_attrs);
          priv->markup_attrs = NULL;
        }
      gtk_label_set_text_internal (label, g_strdup (priv->label));
    }

  if (!priv->use_underline)
    priv->mnemonic_keyval = GDK_KEY_VoidSymbol;

  if (keyval != priv->mnemonic_keyval)
    {
      gtk_label_setup_mnemonic (label, keyval);
      g_object_notify (G_OBJECT (label), "mnemonic-keyval");
    }

  gtk_label_clear_layout (label);
  gtk_label_clear_select_info (label);
  gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * gtk_label_set_text:
 * @label: a #GtkLabel
 * @str: The text you want to set
 *
 * Sets the text within the #GtkLabel widget. It overwrites any text that
 * was there before.  
 *
 * This will also clear any previously set mnemonic accelerators.
 **/
void
gtk_label_set_text (GtkLabel    *label,
		    const gchar *str)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  g_object_freeze_notify (G_OBJECT (label));

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, FALSE);
  gtk_label_set_use_underline_internal (label, FALSE);
  
  gtk_label_recalculate (label);

  g_object_thaw_notify (G_OBJECT (label));
}

/**
 * gtk_label_set_attributes:
 * @label: a #GtkLabel
 * @attrs: a #PangoAttrList
 * 
 * Sets a #PangoAttrList; the attributes in the list are applied to the
 * label text. 
 *
 * The attributes set with this function will be applied
 * and merged with any other attributes previously effected by way
 * of the #GtkLabel:use-underline or #GtkLabel:use-markup properties.
 * While it is not recommended to mix markup strings with manually set
 * attributes, if you must; know that the attributes will be applied
 * to the label after the markup string is parsed.
 **/
void
gtk_label_set_attributes (GtkLabel         *label,
                          PangoAttrList    *attrs)
{
  GtkLabelPrivate *priv = label->priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  if (attrs)
    pango_attr_list_ref (attrs);

  if (priv->attrs)
    pango_attr_list_unref (priv->attrs);
  priv->attrs = attrs;

  g_object_notify (G_OBJECT (label), "attributes");

  gtk_label_clear_layout (label);
  gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * gtk_label_get_attributes:
 * @label: a #GtkLabel
 *
 * Gets the attribute list that was set on the label using
 * gtk_label_set_attributes(), if any. This function does
 * not reflect attributes that come from the labels markup
 * (see gtk_label_set_markup()). If you want to get the
 * effective attributes for the label, use
 * pango_layout_get_attribute (gtk_label_get_layout (label)).
 *
 * Returns: (transfer none): the attribute list, or %NULL
 *     if none was set.
 **/
PangoAttrList *
gtk_label_get_attributes (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->priv->attrs;
}

/**
 * gtk_label_set_label:
 * @label: a #GtkLabel
 * @str: the new text to set for the label
 *
 * Sets the text of the label. The label is interpreted as
 * including embedded underlines and/or Pango markup depending
 * on the values of the #GtkLabel:use-underline" and
 * #GtkLabel:use-markup properties.
 **/
void
gtk_label_set_label (GtkLabel    *label,
		     const gchar *str)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  g_object_freeze_notify (G_OBJECT (label));

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_recalculate (label);

  g_object_thaw_notify (G_OBJECT (label));
}

/**
 * gtk_label_get_label:
 * @label: a #GtkLabel
 *
 * Fetches the text from a label widget including any embedded
 * underlines indicating mnemonics and Pango markup. (See
 * gtk_label_get_text()).
 *
 * Returns: the text of the label widget. This string is
 *   owned by the widget and must not be modified or freed.
 **/
const gchar *
gtk_label_get_label (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->priv->label;
}

typedef struct
{
  GtkLabel *label;
  GList *links;
  GString *new_str;
  gsize text_len;
} UriParserData;

static void
start_element_handler (GMarkupParseContext  *context,
                       const gchar          *element_name,
                       const gchar         **attribute_names,
                       const gchar         **attribute_values,
                       gpointer              user_data,
                       GError              **error)
{
  GtkLabelPrivate *priv;
  UriParserData *pdata = user_data;

  if (strcmp (element_name, "a") == 0)
    {
      GtkLabelLink *link;
      const gchar *uri = NULL;
      const gchar *title = NULL;
      gboolean visited = FALSE;
      gint line_number;
      gint char_number;
      gint i;

      g_markup_parse_context_get_position (context, &line_number, &char_number);

      for (i = 0; attribute_names[i] != NULL; i++)
        {
          const gchar *attr = attribute_names[i];

          if (strcmp (attr, "href") == 0)
            uri = attribute_values[i];
          else if (strcmp (attr, "title") == 0)
            title = attribute_values[i];
          else
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                           "Attribute '%s' is not allowed on the <a> tag "
                           "on line %d char %d",
                            attr, line_number, char_number);
              return;
            }
        }

      if (uri == NULL)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Attribute 'href' was missing on the <a> tag "
                       "on line %d char %d",
                       line_number, char_number);
          return;
        }

      visited = FALSE;
      priv = pdata->label->priv;
      if (priv->track_links && priv->select_info)
        {
          GList *l;
          for (l = priv->select_info->links; l; l = l->next)
            {
              link = l->data;
              if (strcmp (uri, link->uri) == 0)
                {
                  visited = link->visited;
                  break;
                }
            }
        }

      link = g_new0 (GtkLabelLink, 1);
      link->uri = g_strdup (uri);
      link->title = g_strdup (title);
      link->visited = visited;
      link->start = pdata->text_len;
      pdata->links = g_list_prepend (pdata->links, link);
    }
  else
    {
      gint i;

      g_string_append_c (pdata->new_str, '<');
      g_string_append (pdata->new_str, element_name);

      for (i = 0; attribute_names[i] != NULL; i++)
        {
          const gchar *attr  = attribute_names[i];
          const gchar *value = attribute_values[i];
          gchar *newvalue;

          newvalue = g_markup_escape_text (value, -1);

          g_string_append_c (pdata->new_str, ' ');
          g_string_append (pdata->new_str, attr);
          g_string_append (pdata->new_str, "=\"");
          g_string_append (pdata->new_str, newvalue);
          g_string_append_c (pdata->new_str, '\"');

          g_free (newvalue);
        }
      g_string_append_c (pdata->new_str, '>');
    }
}

static void
end_element_handler (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     gpointer              user_data,
                     GError              **error)
{
  UriParserData *pdata = user_data;

  if (!strcmp (element_name, "a"))
    {
      GtkLabelLink *link = pdata->links->data;
      link->end = pdata->text_len;
    }
  else
    {
      g_string_append (pdata->new_str, "</");
      g_string_append (pdata->new_str, element_name);
      g_string_append_c (pdata->new_str, '>');
    }
}

static void
text_handler (GMarkupParseContext  *context,
              const gchar          *text,
              gsize                 text_len,
              gpointer              user_data,
              GError              **error)
{
  UriParserData *pdata = user_data;
  gchar *newtext;

  newtext = g_markup_escape_text (text, text_len);
  g_string_append (pdata->new_str, newtext);
  pdata->text_len += text_len;
  g_free (newtext);
}

static const GMarkupParser markup_parser =
{
  start_element_handler,
  end_element_handler,
  text_handler,
  NULL,
  NULL
};

static gboolean
xml_isspace (gchar c)
{
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static void
link_free (GtkLabelLink *link)
{
  g_free (link->uri);
  g_free (link->title);
  g_free (link);
}


static gboolean
parse_uri_markup (GtkLabel     *label,
                  const gchar  *str,
                  gchar       **new_str,
                  GList       **links,
                  GError      **error)
{
  GMarkupParseContext *context = NULL;
  const gchar *p, *end;
  gboolean needs_root = TRUE;
  gsize length;
  UriParserData pdata;

  length = strlen (str);
  p = str;
  end = str + length;

  pdata.label = label;
  pdata.links = NULL;
  pdata.new_str = g_string_sized_new (length);
  pdata.text_len = 0;

  while (p != end && xml_isspace (*p))
    p++;

  if (end - p >= 8 && strncmp (p, "<markup>", 8) == 0)
    needs_root = FALSE;

  context = g_markup_parse_context_new (&markup_parser, 0, &pdata, NULL);

  if (needs_root)
    {
      if (!g_markup_parse_context_parse (context, "<markup>", -1, error))
        goto failed;
    }

  if (!g_markup_parse_context_parse (context, str, length, error))
    goto failed;

  if (needs_root)
    {
      if (!g_markup_parse_context_parse (context, "</markup>", -1, error))
        goto failed;
    }

  if (!g_markup_parse_context_end_parse (context, error))
    goto failed;

  g_markup_parse_context_free (context);

  *new_str = g_string_free (pdata.new_str, FALSE);
  *links = pdata.links;

  return TRUE;

failed:
  g_markup_parse_context_free (context);
  g_string_free (pdata.new_str, TRUE);
  g_list_free_full (pdata.links, (GDestroyNotify) link_free);

  return FALSE;
}

static void
gtk_label_ensure_has_tooltip (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;
  GList *l;
  gboolean has_tooltip = FALSE;

  for (l = priv->select_info->links; l; l = l->next)
    {
      GtkLabelLink *link = l->data;
      if (link->title)
        {
          has_tooltip = TRUE;
          break;
        }
    }

  gtk_widget_set_has_tooltip (GTK_WIDGET (label), has_tooltip);
}

static void
gtk_label_set_markup_internal (GtkLabel    *label,
                               const gchar *str,
                               gboolean     with_uline)
{
  GtkLabelPrivate *priv = label->priv;
  gchar *text = NULL;
  GError *error = NULL;
  PangoAttrList *attrs = NULL;
  gunichar accel_char = 0;
  gchar *str_for_display = NULL;
  gchar *str_for_accel = NULL;
  GList *links = NULL;

  if (!parse_uri_markup (label, str, &str_for_display, &links, &error))
    {
      g_warning ("Failed to set text from markup due to error parsing markup: %s",
                 error->message);
      g_error_free (error);
      return;
    }

  str_for_accel = g_strdup (str_for_display);

  if (links)
    {
      gtk_label_ensure_select_info (label);
      priv->select_info->links = g_list_reverse (links);
      _gtk_label_accessible_update_links (label);
      gtk_label_ensure_has_tooltip (label);
    }

  if (with_uline)
    {
      gboolean enable_mnemonics = TRUE;
      gboolean auto_mnemonics = TRUE;

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (label)),
                    "gtk-enable-mnemonics", &enable_mnemonics,
                    NULL);

      if (!(enable_mnemonics && priv->mnemonics_visible &&
            (!auto_mnemonics ||
             (gtk_widget_is_sensitive (GTK_WIDGET (label)) &&
              (!priv->mnemonic_widget ||
               gtk_widget_is_sensitive (priv->mnemonic_widget))))))
        {
          gchar *tmp;
          gchar *pattern;
          guint key;

          if (separate_uline_pattern (str_for_display, &key, &tmp, &pattern))
            {
              g_free (str_for_display);
              str_for_display = tmp;
              g_free (pattern);
            }
        }
    }

  /* Extract the text to display */
  if (!pango_parse_markup (str_for_display,
                           -1,
                           with_uline ? '_' : 0,
                           &attrs,
                           &text,
                           NULL,
                           &error))
    {
      g_warning ("Failed to set text from markup due to error parsing markup: %s",
                 error->message);
      g_free (str_for_display);
      g_free (str_for_accel);
      g_error_free (error);
      return;
    }

  /* Extract the accelerator character */
  if (with_uline && !pango_parse_markup (str_for_accel,
					 -1,
					 '_',
					 NULL,
					 NULL,
					 &accel_char,
					 &error))
    {
      g_warning ("Failed to set text from markup due to error parsing markup: %s",
                 error->message);
      g_free (str_for_display);
      g_free (str_for_accel);
      g_error_free (error);
      return;
    }

  g_free (str_for_display);
  g_free (str_for_accel);

  if (text)
    gtk_label_set_text_internal (label, text);

  if (attrs)
    {
      if (priv->markup_attrs)
	pango_attr_list_unref (priv->markup_attrs);
      priv->markup_attrs = attrs;
    }

  if (accel_char != 0)
    priv->mnemonic_keyval = gdk_keyval_to_lower (gdk_unicode_to_keyval (accel_char));
  else
    priv->mnemonic_keyval = GDK_KEY_VoidSymbol;
}

/**
 * gtk_label_set_markup:
 * @label: a #GtkLabel
 * @str: a markup string (see [Pango markup format][PangoMarkupFormat])
 *
 * Parses @str which is marked up with the
 * [Pango text markup language][PangoMarkupFormat], setting the
 * label’s text and attribute list based on the parse results. If the @str is
 * external data, you may need to escape it with g_markup_escape_text() or
 * g_markup_printf_escaped():
 * |[<!-- language="C" -->
 * const char *format = "<span style=\"italic\">\%s</span>";
 * char *markup;
 *
 * markup = g_markup_printf_escaped (format, str);
 * gtk_label_set_markup (GTK_LABEL (label), markup);
 * g_free (markup);
 * ]|
 **/
void
gtk_label_set_markup (GtkLabel    *label,
                      const gchar *str)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  g_object_freeze_notify (G_OBJECT (label));

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, TRUE);
  gtk_label_set_use_underline_internal (label, FALSE);

  gtk_label_recalculate (label);

  g_object_thaw_notify (G_OBJECT (label));
}

/**
 * gtk_label_set_markup_with_mnemonic:
 * @label: a #GtkLabel
 * @str: a markup string (see
 *     [Pango markup format][PangoMarkupFormat])
 *
 * Parses @str which is marked up with the
 * [Pango text markup language][PangoMarkupFormat],
 * setting the label’s text and attribute list based on the parse results.
 * If characters in @str are preceded by an underscore, they are underlined
 * indicating that they represent a keyboard accelerator called a mnemonic.
 *
 * The mnemonic key can be used to activate another widget, chosen
 * automatically, or explicitly using gtk_label_set_mnemonic_widget().
 */
void
gtk_label_set_markup_with_mnemonic (GtkLabel    *label,
                                    const gchar *str)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  g_object_freeze_notify (G_OBJECT (label));

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, TRUE);
  gtk_label_set_use_underline_internal (label, TRUE);

  gtk_label_recalculate (label);

  g_object_thaw_notify (G_OBJECT (label));
}

/**
 * gtk_label_get_text:
 * @label: a #GtkLabel
 * 
 * Fetches the text from a label widget, as displayed on the
 * screen. This does not include any embedded underlines
 * indicating mnemonics or Pango markup. (See gtk_label_get_label())
 * 
 * Returns: the text in the label widget. This is the internal
 *   string used by the label, and must not be modified.
 **/
const gchar *
gtk_label_get_text (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->priv->text;
}

static PangoAttrList *
gtk_label_pattern_to_attrs (GtkLabel      *label,
			    const gchar   *pattern)
{
  GtkLabelPrivate *priv = label->priv;
  const char *start;
  const char *p = priv->text;
  const char *q = pattern;
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();

  while (1)
    {
      while (*p && *q && *q != '_')
	{
	  p = g_utf8_next_char (p);
	  q++;
	}
      start = p;
      while (*p && *q && *q == '_')
	{
	  p = g_utf8_next_char (p);
	  q++;
	}
      
      if (p > start)
	{
	  PangoAttribute *attr = pango_attr_underline_new (PANGO_UNDERLINE_LOW);
	  attr->start_index = start - priv->text;
	  attr->end_index = p - priv->text;
	  
	  pango_attr_list_insert (attrs, attr);
	}
      else
	break;
    }

  return attrs;
}

static void
gtk_label_set_pattern_internal (GtkLabel    *label,
				const gchar *pattern,
                                gboolean     is_mnemonic)
{
  GtkLabelPrivate *priv = label->priv;
  PangoAttrList *attrs;
  gboolean enable_mnemonics = TRUE;
  gboolean auto_mnemonics = TRUE;

  if (priv->pattern_set)
    return;

  if (is_mnemonic)
    {
      g_object_get (gtk_widget_get_settings (GTK_WIDGET (label)),
                    "gtk-enable-mnemonics", &enable_mnemonics,
                    NULL);

      if (enable_mnemonics && priv->mnemonics_visible && pattern &&
          (!auto_mnemonics ||
           (gtk_widget_is_sensitive (GTK_WIDGET (label)) &&
            (!priv->mnemonic_widget ||
             gtk_widget_is_sensitive (priv->mnemonic_widget)))))
        attrs = gtk_label_pattern_to_attrs (label, pattern);
      else
        attrs = NULL;
    }
  else
    attrs = gtk_label_pattern_to_attrs (label, pattern);

  if (priv->markup_attrs)
    pango_attr_list_unref (priv->markup_attrs);
  priv->markup_attrs = attrs;
}

/**
 * gtk_label_set_pattern:
 * @label: The #GtkLabel you want to set the pattern to.
 * @pattern: The pattern as described above.
 *
 * The pattern of underlines you want under the existing text within the
 * #GtkLabel widget.  For example if the current text of the label says
 * “FooBarBaz” passing a pattern of “___   ___” will underline
 * “Foo” and “Baz” but not “Bar”.
 */
void
gtk_label_set_pattern (GtkLabel	   *label,
		       const gchar *pattern)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  priv->pattern_set = FALSE;

  if (pattern)
    {
      gtk_label_set_pattern_internal (label, pattern, FALSE);
      priv->pattern_set = TRUE;
    }
  else
    gtk_label_recalculate (label);

  gtk_label_clear_layout (label);
  gtk_widget_queue_resize (GTK_WIDGET (label));
}


/**
 * gtk_label_set_justify:
 * @label: a #GtkLabel
 * @jtype: a #GtkJustification
 *
 * Sets the alignment of the lines in the text of the label relative to
 * each other. %GTK_JUSTIFY_LEFT is the default value when the widget is
 * first created with gtk_label_new(). If you instead want to set the
 * alignment of the label as a whole, use gtk_widget_set_halign() instead.
 * gtk_label_set_justify() has no effect on labels containing only a
 * single line.
 */
void
gtk_label_set_justify (GtkLabel        *label,
		       GtkJustification jtype)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (jtype >= GTK_JUSTIFY_LEFT && jtype <= GTK_JUSTIFY_FILL);

  priv = label->priv;

  if ((GtkJustification) priv->jtype != jtype)
    {
      priv->jtype = jtype;

      /* No real need to be this drastic, but easier than duplicating the code */
      gtk_label_clear_layout (label);
      
      g_object_notify (G_OBJECT (label), "justify");
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_justify:
 * @label: a #GtkLabel
 *
 * Returns the justification of the label. See gtk_label_set_justify().
 *
 * Returns: #GtkJustification
 **/
GtkJustification
gtk_label_get_justify (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), 0);

  return label->priv->jtype;
}

/**
 * gtk_label_set_ellipsize:
 * @label: a #GtkLabel
 * @mode: a #PangoEllipsizeMode
 *
 * Sets the mode used to ellipsize (add an ellipsis: "...") to the text 
 * if there is not enough space to render the entire string.
 *
 * Since: 2.6
 **/
void
gtk_label_set_ellipsize (GtkLabel          *label,
			 PangoEllipsizeMode mode)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (mode >= PANGO_ELLIPSIZE_NONE && mode <= PANGO_ELLIPSIZE_END);

  priv = label->priv;

  if ((PangoEllipsizeMode) priv->ellipsize != mode)
    {
      priv->ellipsize = mode;

      /* No real need to be this drastic, but easier than duplicating the code */
      gtk_label_clear_layout (label);
      
      g_object_notify (G_OBJECT (label), "ellipsize");
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_ellipsize:
 * @label: a #GtkLabel
 *
 * Returns the ellipsizing position of the label. See gtk_label_set_ellipsize().
 *
 * Returns: #PangoEllipsizeMode
 *
 * Since: 2.6
 **/
PangoEllipsizeMode
gtk_label_get_ellipsize (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), PANGO_ELLIPSIZE_NONE);

  return label->priv->ellipsize;
}

/**
 * gtk_label_set_width_chars:
 * @label: a #GtkLabel
 * @n_chars: the new desired width, in characters.
 * 
 * Sets the desired width in characters of @label to @n_chars.
 * 
 * Since: 2.6
 **/
void
gtk_label_set_width_chars (GtkLabel *label,
			   gint      n_chars)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  if (priv->width_chars != n_chars)
    {
      priv->width_chars = n_chars;
      g_object_notify (G_OBJECT (label), "width-chars");
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_width_chars:
 * @label: a #GtkLabel
 * 
 * Retrieves the desired width of @label, in characters. See
 * gtk_label_set_width_chars().
 * 
 * Returns: the width of the label in characters.
 * 
 * Since: 2.6
 **/
gint
gtk_label_get_width_chars (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), -1);

  return label->priv->width_chars;
}

/**
 * gtk_label_set_max_width_chars:
 * @label: a #GtkLabel
 * @n_chars: the new desired maximum width, in characters.
 * 
 * Sets the desired maximum width in characters of @label to @n_chars.
 * 
 * Since: 2.6
 **/
void
gtk_label_set_max_width_chars (GtkLabel *label,
			       gint      n_chars)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  if (priv->max_width_chars != n_chars)
    {
      priv->max_width_chars = n_chars;

      g_object_notify (G_OBJECT (label), "max-width-chars");
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_max_width_chars:
 * @label: a #GtkLabel
 * 
 * Retrieves the desired maximum width of @label, in characters. See
 * gtk_label_set_width_chars().
 * 
 * Returns: the maximum width of the label in characters.
 * 
 * Since: 2.6
 **/
gint
gtk_label_get_max_width_chars (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), -1);

  return label->priv->max_width_chars;
}

/**
 * gtk_label_set_line_wrap:
 * @label: a #GtkLabel
 * @wrap: the setting
 *
 * Toggles line wrapping within the #GtkLabel widget. %TRUE makes it break
 * lines if text exceeds the widget’s size. %FALSE lets the text get cut off
 * by the edge of the widget if it exceeds the widget size.
 *
 * Note that setting line wrapping to %TRUE does not make the label
 * wrap at its parent container’s width, because GTK+ widgets
 * conceptually can’t make their requisition depend on the parent
 * container’s size. For a label that wraps at a specific position,
 * set the label’s width using gtk_widget_set_size_request().
 **/
void
gtk_label_set_line_wrap (GtkLabel *label,
			 gboolean  wrap)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  wrap = wrap != FALSE;

  if (priv->wrap != wrap)
    {
      priv->wrap = wrap;

      gtk_label_clear_layout (label);
      gtk_widget_queue_resize (GTK_WIDGET (label));
      g_object_notify (G_OBJECT (label), "wrap");
    }
}

/**
 * gtk_label_get_line_wrap:
 * @label: a #GtkLabel
 *
 * Returns whether lines in the label are automatically wrapped. 
 * See gtk_label_set_line_wrap().
 *
 * Returns: %TRUE if the lines of the label are automatically wrapped.
 */
gboolean
gtk_label_get_line_wrap (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->priv->wrap;
}

/**
 * gtk_label_set_line_wrap_mode:
 * @label: a #GtkLabel
 * @wrap_mode: the line wrapping mode
 *
 * If line wrapping is on (see gtk_label_set_line_wrap()) this controls how
 * the line wrapping is done. The default is %PANGO_WRAP_WORD which means
 * wrap on word boundaries.
 *
 * Since: 2.10
 **/
void
gtk_label_set_line_wrap_mode (GtkLabel *label,
			      PangoWrapMode wrap_mode)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  if (priv->wrap_mode != wrap_mode)
    {
      priv->wrap_mode = wrap_mode;
      g_object_notify (G_OBJECT (label), "wrap-mode");
      
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_line_wrap_mode:
 * @label: a #GtkLabel
 *
 * Returns line wrap mode used by the label. See gtk_label_set_line_wrap_mode().
 *
 * Returns: %TRUE if the lines of the label are automatically wrapped.
 *
 * Since: 2.10
 */
PangoWrapMode
gtk_label_get_line_wrap_mode (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->priv->wrap_mode;
}

static void
gtk_label_destroy (GtkWidget *widget)
{
  GtkLabel *label = GTK_LABEL (widget);

  gtk_label_set_mnemonic_widget (label, NULL);

  GTK_WIDGET_CLASS (gtk_label_parent_class)->destroy (widget);
}

static void
gtk_label_finalize (GObject *object)
{
  GtkLabel *label = GTK_LABEL (object);
  GtkLabelPrivate *priv = label->priv;

  g_free (priv->label);
  g_free (priv->text);

  if (priv->layout)
    g_object_unref (priv->layout);

  if (priv->attrs)
    pango_attr_list_unref (priv->attrs);

  if (priv->markup_attrs)
    pango_attr_list_unref (priv->markup_attrs);

  gtk_label_clear_links (label);
  g_free (priv->select_info);

  g_object_unref (priv->drag_gesture);
  g_object_unref (priv->multipress_gesture);

  G_OBJECT_CLASS (gtk_label_parent_class)->finalize (object);
}

static void
gtk_label_clear_layout (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;

  if (priv->layout)
    {
      g_object_unref (priv->layout);
      priv->layout = NULL;
    }
}

/**
 * gtk_label_get_measuring_layout:
 * @label: the label
 * @existing_layout: %NULL or an existing layout already in use.
 * @width: the width to measure with in pango units, or -1 for infinite
 *
 * Gets a layout that can be used for measuring sizes. The returned
 * layout will be identical to the label’s layout except for the
 * layout’s width, which will be set to @width. Do not modify the returned
 * layout.
 *
 * Returns: a new reference to a pango layout
 **/
static PangoLayout *
gtk_label_get_measuring_layout (GtkLabel *   label,
                                PangoLayout *existing_layout,
                                int          width)
{
  GtkLabelPrivate *priv = label->priv;
  PangoRectangle rect;
  PangoLayout *copy;

  if (existing_layout != NULL)
    {
      if (existing_layout != priv->layout)
        {
          pango_layout_set_width (existing_layout, width);
          return existing_layout;
        }

      g_object_unref (existing_layout);
    }

  gtk_label_ensure_layout (label);

  if (pango_layout_get_width (priv->layout) == width)
    {
      g_object_ref (priv->layout);
      return priv->layout;
    }

  /* We can use the label's own layout if we're not allocated a size yet,
   * because we don't need it to be properly setup at that point.
   * This way we can make use of caching upon the label's creation.
   */
  if (gtk_widget_get_allocated_width (GTK_WIDGET (label)) <= 1)
    {
      g_object_ref (priv->layout);
      pango_layout_set_width (priv->layout, width);
      return priv->layout;
    }

  /* oftentimes we want to measure a width that is far wider than the current width,
   * even though the layout would not change if we made it wider. In that case, we
   * can just return the current layout, because for measuring purposes, it will be
   * identical.
   */
  pango_layout_get_extents (priv->layout, NULL, &rect);
  if ((width == -1 || rect.width <= width) &&
      !pango_layout_is_wrapped (priv->layout) &&
      !pango_layout_is_ellipsized (priv->layout))
    {
      g_object_ref (priv->layout);
      return priv->layout;
    }

  copy = pango_layout_copy (priv->layout);
  pango_layout_set_width (copy, width);
  return copy;
}

static void
gtk_label_update_layout_width (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;
  GtkWidget *widget = GTK_WIDGET (label);

  g_assert (priv->layout);

  if (priv->ellipsize || priv->wrap)
    {
      GtkBorder border;
      PangoRectangle logical;
      gint width, height;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      _gtk_misc_get_padding_and_border (GTK_MISC (label), &border);
G_GNUC_END_IGNORE_DEPRECATIONS

      width = gtk_widget_get_allocated_width (GTK_WIDGET (label)) - border.left - border.right;
      height = gtk_widget_get_allocated_height (GTK_WIDGET (label)) - border.top - border.bottom;

      if (priv->have_transform)
        {
          PangoContext *context = gtk_widget_get_pango_context (widget);
          const PangoMatrix *matrix = pango_context_get_matrix (context);
          const gdouble dx = matrix->xx; /* cos (M_PI * angle / 180) */
          const gdouble dy = matrix->xy; /* sin (M_PI * angle / 180) */

          pango_layout_set_width (priv->layout, -1);
          pango_layout_get_pixel_extents (priv->layout, NULL, &logical);

          if (fabs (dy) < 0.01)
            {
              if (logical.width > width)
                pango_layout_set_width (priv->layout, width * PANGO_SCALE);
            }
          else if (fabs (dx) < 0.01)
            {
              if (logical.width > height)
                pango_layout_set_width (priv->layout, height * PANGO_SCALE);
            }
          else
            {
              gdouble x0, y0, x1, y1, length;
              gboolean vertical;
              gint cy;

              x0 = width / 2;
              y0 = dx ? x0 * dy / dx : G_MAXDOUBLE;
              vertical = fabs (y0) > height / 2;

              if (vertical)
                {
                  y0 = height/2;
                  x0 = dy ? y0 * dx / dy : G_MAXDOUBLE;
                }

              length = 2 * sqrt (x0 * x0 + y0 * y0);
              pango_layout_set_width (priv->layout, rint (length * PANGO_SCALE));
              pango_layout_get_pixel_size (priv->layout, NULL, &cy);

              x1 = +dy * cy/2;
              y1 = -dx * cy/2;

              if (vertical)
                {
                  y0 = height/2 + y1 - y0;
                  x0 = -y0 * dx/dy;
                }
              else
                {
                  x0 = width/2 + x1 - x0;
                  y0 = -x0 * dy/dx;
                }

              length = length - sqrt (x0 * x0 + y0 * y0) * 2;
              pango_layout_set_width (priv->layout, rint (length * PANGO_SCALE));
            }
        }
      else
        {
          pango_layout_set_width (priv->layout, width * PANGO_SCALE);
        }
    }
  else
    {
      pango_layout_set_width (priv->layout, -1);
    }
}

static void
gtk_label_update_layout_attributes (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;
  GtkWidget *widget = GTK_WIDGET (label);
  GtkStyleContext *context;
  PangoAttrList *attrs;

  if (priv->layout == NULL)
    return;

  context = gtk_widget_get_style_context (widget);

  if (priv->select_info && priv->select_info->links)
    {
      GdkRGBA link_color;
      PangoAttribute *attribute;
      GList *list;

      attrs = pango_attr_list_new ();

      gtk_style_context_save (context);

      for (list = priv->select_info->links; list; list = list->next)
        {
          GtkLabelLink *link = list->data;
          GtkStateFlags state;

          attribute = pango_attr_underline_new (TRUE);
          attribute->start_index = link->start;
          attribute->end_index = link->end;
          pango_attr_list_insert (attrs, attribute);

          state = gtk_widget_get_state_flags (widget);
          if (link->visited)
            state |= GTK_STATE_FLAG_VISITED;
          else
            state |= GTK_STATE_FLAG_LINK;

          gtk_style_context_set_state (context, state);
          gtk_style_context_get_color (context, state, &link_color);

          attribute = pango_attr_foreground_new (link_color.red * 65535,
                                                 link_color.green * 65535,
                                                 link_color.blue * 65535);
          attribute->start_index = link->start;
          attribute->end_index = link->end;
          pango_attr_list_insert (attrs, attribute);
        }

      gtk_style_context_restore (context);
    }
  else if (priv->markup_attrs && priv->attrs)
    attrs = pango_attr_list_new ();
  else
    attrs = NULL;

  if (priv->markup_attrs)
    {
      if (attrs)
        my_pango_attr_list_merge (attrs, priv->markup_attrs);
      else
        attrs = pango_attr_list_ref (priv->markup_attrs);
    }

  if (priv->attrs)
    {
      if (attrs)
        my_pango_attr_list_merge (attrs, priv->attrs);
      else
        attrs = pango_attr_list_ref (priv->attrs);
    }

  pango_layout_set_attributes (priv->layout, attrs);

  if (attrs)
    pango_attr_list_unref (attrs);
}

static void
gtk_label_ensure_layout (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;
  GtkWidget *widget;
  gboolean rtl;

  widget = GTK_WIDGET (label);

  rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  if (!priv->layout)
    {
      PangoAlignment align = PANGO_ALIGN_LEFT; /* Quiet gcc */
      gdouble angle = gtk_label_get_angle (label);

      if (angle != 0.0 && !priv->select_info)
	{
          PangoMatrix matrix = PANGO_MATRIX_INIT;

	  /* We rotate the standard singleton PangoContext for the widget,
	   * depending on the fact that it's meant pretty much exclusively
	   * for our use.
	   */
	  pango_matrix_rotate (&matrix, angle);

	  pango_context_set_matrix (gtk_widget_get_pango_context (widget), &matrix);

	  priv->have_transform = TRUE;
	}
      else
	{
	  if (priv->have_transform)
	    pango_context_set_matrix (gtk_widget_get_pango_context (widget), NULL);

	  priv->have_transform = FALSE;
	}

      priv->layout = gtk_widget_create_pango_layout (widget, priv->text);

      gtk_label_update_layout_attributes (label);

      switch (priv->jtype)
	{
	case GTK_JUSTIFY_LEFT:
	  align = rtl ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT;
	  break;
	case GTK_JUSTIFY_RIGHT:
	  align = rtl ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT;
	  break;
	case GTK_JUSTIFY_CENTER:
	  align = PANGO_ALIGN_CENTER;
	  break;
	case GTK_JUSTIFY_FILL:
	  align = rtl ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT;
	  pango_layout_set_justify (priv->layout, TRUE);
	  break;
	default:
	  g_assert_not_reached();
	}

      pango_layout_set_alignment (priv->layout, align);
      pango_layout_set_ellipsize (priv->layout, priv->ellipsize);
      pango_layout_set_wrap (priv->layout, priv->wrap_mode);
      pango_layout_set_single_paragraph_mode (priv->layout, priv->single_line_mode);
      if (priv->lines > 0)
        pango_layout_set_height (priv->layout, - priv->lines);

      gtk_label_update_layout_width (label);
    }
}

static GtkSizeRequestMode
gtk_label_get_request_mode (GtkWidget *widget)
{
  GtkLabel *label = GTK_LABEL (widget);
  gdouble   angle = gtk_label_get_angle (label);

  if (label->priv->wrap)
    return (angle == 90 || angle == 270) ?
      GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT : 
      GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;

    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}


static void
get_size_for_allocation (GtkLabel        *label,
                         GtkOrientation   orientation,
                         gint             allocation,
                         gint            *minimum_size,
                         gint            *natural_size,
			 gint            *minimum_baseline,
                         gint            *natural_baseline)
{
  PangoLayout *layout;
  gint text_height, baseline;

  layout = gtk_label_get_measuring_layout (label, NULL, allocation * PANGO_SCALE);

  pango_layout_get_pixel_size (layout, NULL, &text_height);

  if (minimum_size)
    *minimum_size = text_height;

  if (natural_size)
    *natural_size = text_height;

  if (minimum_baseline || natural_baseline)
    {
      baseline = pango_layout_get_baseline (layout) / PANGO_SCALE;
      if (minimum_baseline)
	*minimum_baseline = baseline;

      if (natural_baseline)
	*natural_baseline = baseline;
    }

  g_object_unref (layout);
}

static gint
get_char_pixels (GtkWidget   *label,
                 PangoLayout *layout)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  gint char_width, digit_width;

  context = pango_layout_get_context (layout);
  metrics = pango_context_get_metrics (context,
                                       pango_context_get_font_description (context),
                                       pango_context_get_language (context));
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
  pango_font_metrics_unref (metrics);

  return MAX (char_width, digit_width);;
}

static void
gtk_label_get_preferred_layout_size (GtkLabel *label,
                                     PangoRectangle *smallest,
                                     PangoRectangle *widest)
{
  GtkLabelPrivate *priv = label->priv;
  PangoLayout *layout;
  gint char_pixels;

  /* "width-chars" Hard-coded minimum width:
   *    - minimum size should be MAX (width-chars, strlen ("..."));
   *    - natural size should be MAX (width-chars, strlen (priv->text));
   *
   * "max-width-chars" User specified maximum size requisition
   *    - minimum size should be MAX (width-chars, 0)
   *    - natural size should be MIN (max-width-chars, strlen (priv->text))
   *
   *    For ellipsizing labels; if max-width-chars is specified: either it is used as 
   *    a minimum size or the label text as a minimum size (natural size still overflows).
   *
   *    For wrapping labels; A reasonable minimum size is useful to naturally layout
   *    interfaces automatically. In this case if no "width-chars" is specified, the minimum
   *    width will default to the wrap guess that gtk_label_ensure_layout() does.
   */

  /* Start off with the pixel extents of an as-wide-as-possible layout */
  layout = gtk_label_get_measuring_layout (label, NULL, -1);

  if (priv->width_chars > -1 || priv->max_width_chars > -1)
    char_pixels = get_char_pixels (GTK_WIDGET (label), layout);
  else
    char_pixels = 0;
      
  pango_layout_get_extents (layout, NULL, widest);
  widest->width = MAX (widest->width, char_pixels * priv->width_chars);
  widest->x = widest->y = 0;

  if (priv->ellipsize || priv->wrap)
    {
      /* a layout with width 0 will be as small as humanly possible */
      layout = gtk_label_get_measuring_layout (label,
                                               layout,
                                               priv->width_chars > -1 ? char_pixels * priv->width_chars
                                                                      : 0);

      pango_layout_get_extents (layout, NULL, smallest);
      smallest->width = MAX (smallest->width, char_pixels * priv->width_chars);
      smallest->x = smallest->y = 0;

      if (priv->max_width_chars > -1 && widest->width > char_pixels * priv->max_width_chars)
        {
          layout = gtk_label_get_measuring_layout (label,
                                                   layout,
                                                   MAX (smallest->width, char_pixels * priv->max_width_chars));
          pango_layout_get_extents (layout, NULL, widest);
          widest->width = MAX (widest->width, char_pixels * priv->width_chars);
          widest->x = widest->y = 0;
        }
    }
  else
    {
      *smallest = *widest;
    }

  if (widest->width < smallest->width)
    *smallest = *widest;

  g_object_unref (layout);
}

static void
gtk_label_get_preferred_size (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              gint           *minimum_size,
                              gint           *natural_size,
			      gint           *minimum_baseline,
			      gint           *natural_baseline)
{
  GtkLabel      *label = GTK_LABEL (widget);
  GtkLabelPrivate  *priv = label->priv;
  PangoRectangle widest_rect;
  PangoRectangle smallest_rect;
  GtkBorder border;

  if (minimum_baseline)
    *minimum_baseline = -1;

  if (natural_baseline)
    *natural_baseline = -1;

  gtk_label_get_preferred_layout_size (label, &smallest_rect, &widest_rect);

  /* Now that we have minimum and natural sizes in pango extents, apply a possible transform */
  if (priv->have_transform)
    {
      PangoContext *context;
      const PangoMatrix *matrix;

      context = pango_layout_get_context (priv->layout);
      matrix = pango_context_get_matrix (context);

      pango_matrix_transform_rectangle (matrix, &widest_rect);
      pango_matrix_transform_rectangle (matrix, &smallest_rect);

      /* Bump the size in case of ellipsize to ensure pango has
       * enough space in the angles (note, we could alternatively set the
       * layout to not ellipsize when we know we have been allocated our
       * full size, or it may be that pango needs a fix here).
       */
      if (priv->ellipsize && priv->angle != 0 && priv->angle != 90 && 
          priv->angle != 180 && priv->angle != 270 && priv->angle != 360)
        {
          /* For some reason we only need this at about 110 degrees, and only
           * when gaining in height
           */
          widest_rect.height += ROTATION_ELLIPSIZE_PADDING * 2 * PANGO_SCALE;
          widest_rect.width  += ROTATION_ELLIPSIZE_PADDING * 2 * PANGO_SCALE;
          smallest_rect.height += ROTATION_ELLIPSIZE_PADDING * 2 * PANGO_SCALE;
          smallest_rect.width  += ROTATION_ELLIPSIZE_PADDING * 2 * PANGO_SCALE;
        }
    }

  widest_rect.width  = PANGO_PIXELS_CEIL (widest_rect.width);
  widest_rect.height = PANGO_PIXELS_CEIL (widest_rect.height);

  smallest_rect.width  = PANGO_PIXELS_CEIL (smallest_rect.width);
  smallest_rect.height = PANGO_PIXELS_CEIL (smallest_rect.height);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _gtk_misc_get_padding_and_border (GTK_MISC (label), &border);
G_GNUC_END_IGNORE_DEPRECATIONS

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* Note, we cant use get_size_for_allocation() when rotating
       * ellipsized labels.
       */
      if (!(priv->ellipsize && priv->have_transform) &&
          (priv->angle == 90 || priv->angle == 270))
        {
          /* Doing a h4w request on a rotated label here, return the
           * required width for the minimum height.
           */
          get_size_for_allocation (label,
                                   GTK_ORIENTATION_VERTICAL,
                                   smallest_rect.height,
                                   minimum_size, natural_size,
				   NULL, NULL);

        }
      else
        {
          /* Normal desired width */
          *minimum_size = smallest_rect.width;
          *natural_size = widest_rect.width;
        }

      *minimum_size += border.left + border.right;
      *natural_size += border.left + border.right;
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      /* Note, we cant use get_size_for_allocation() when rotating
       * ellipsized labels.
       */
      if (!(priv->ellipsize && priv->have_transform) &&
          (priv->angle == 0 || priv->angle == 180 || priv->angle == 360))
        {
          /* Doing a w4h request on a label here, return the required
           * height for the minimum width.
           */
          get_size_for_allocation (label,
                                   GTK_ORIENTATION_HORIZONTAL,
                                   widest_rect.width,
                                   minimum_size, natural_size,
				   minimum_baseline, natural_baseline);

	  if (priv->angle == 180)
	    {
	      if (minimum_baseline)
		*minimum_baseline = *minimum_size - *minimum_baseline;
	      if (natural_baseline)
		*natural_baseline = *natural_size - *natural_baseline;
	    }
        }
      else
        {
          /* A vertically rotated label does w4h, so return the base
           * desired height (text length)
           */
          *minimum_size = MIN (smallest_rect.height, widest_rect.height);
          *natural_size = MAX (smallest_rect.height, widest_rect.height);
        }

      *minimum_size += border.top + border.bottom;
      *natural_size += border.top + border.bottom;

      if (minimum_baseline && *minimum_baseline != -1)
	*minimum_baseline += border.top;

      if (natural_baseline && *natural_baseline != -1)
	*natural_baseline += border.top;
    }
}

static void
gtk_label_get_preferred_width (GtkWidget *widget,
                               gint      *minimum_size,
                               gint      *natural_size)
{
  gtk_label_get_preferred_size (widget, GTK_ORIENTATION_HORIZONTAL, minimum_size, natural_size, NULL, NULL);
}

static void
gtk_label_get_preferred_height (GtkWidget *widget,
                                gint      *minimum_size,
                                gint      *natural_size)
{
  gtk_label_get_preferred_size (widget, GTK_ORIENTATION_VERTICAL, minimum_size, natural_size, NULL, NULL);
}

static void
gtk_label_get_preferred_width_for_height (GtkWidget *widget,
                                          gint       height,
                                          gint      *minimum_width,
                                          gint      *natural_width)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;

  if (priv->wrap && (priv->angle == 90 || priv->angle == 270))
    {
      GtkBorder border;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      _gtk_misc_get_padding_and_border (GTK_MISC (label), &border);
G_GNUC_END_IGNORE_DEPRECATIONS

      if (priv->wrap)
        gtk_label_clear_layout (label);

      get_size_for_allocation (label, GTK_ORIENTATION_VERTICAL,
                               MAX (1, height - border.top - border.bottom),
                               minimum_width, natural_width,
			       NULL, NULL);

      if (minimum_width)
        *minimum_width += border.right + border.left;

      if (natural_width)
        *natural_width += border.right + border.left;
    }
  else
    GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, minimum_width, natural_width);
}

static void
gtk_label_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
						       gint       width,
						       gint      *minimum_height,
						       gint      *natural_height,
						       gint      *minimum_baseline,
						       gint      *natural_baseline)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;

  if (width != -1 && priv->wrap && (priv->angle == 0 || priv->angle == 180 || priv->angle == 360))
    {
      GtkBorder border;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      _gtk_misc_get_padding_and_border (GTK_MISC (label), &border);
G_GNUC_END_IGNORE_DEPRECATIONS

      if (priv->wrap)
        gtk_label_clear_layout (label);

      get_size_for_allocation (label, GTK_ORIENTATION_HORIZONTAL,
                               MAX (1, width - border.left - border.right),
                               minimum_height, natural_height,
			       minimum_baseline, natural_baseline);

      if (minimum_baseline && *minimum_baseline != -1)
	*minimum_baseline += border.top;
      if (natural_baseline && *natural_baseline != -1)
	*natural_baseline += border.top;

      if (minimum_height)
        *minimum_height += border.top + border.bottom;

      if (natural_height)
        *natural_height += border.top + border.bottom;
    }
  else
    gtk_label_get_preferred_size (widget, GTK_ORIENTATION_VERTICAL, minimum_height, natural_height, minimum_baseline, natural_baseline);
}

static void
gtk_label_get_preferred_height_for_width (GtkWidget *widget,
                                          gint       width,
                                          gint      *minimum_height,
                                          gint      *natural_height)
{
  gtk_label_get_preferred_height_and_baseline_for_width (widget, width,
                                                         minimum_height, natural_height,
                                                         NULL, NULL);
}

static void
get_layout_location (GtkLabel  *label,
                     gint      *xp,
                     gint      *yp)
{
  GtkAllocation allocation;
  GtkWidget *widget;
  GtkLabelPrivate *priv;
  GtkBorder border;
  gint req_width, x, y;
  gint req_height;
  gfloat xalign, yalign;
  PangoRectangle logical;
  gint baseline, layout_baseline, baseline_offset;

  widget = GTK_WIDGET (label);
  priv   = label->priv;

  xalign = priv->xalign;
  yalign = priv->yalign;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _gtk_misc_get_padding_and_border (GTK_MISC (label), &border);
G_GNUC_END_IGNORE_DEPRECATIONS

  if (gtk_widget_get_direction (widget) != GTK_TEXT_DIR_LTR)
    xalign = 1.0 - xalign;

  pango_layout_get_extents (priv->layout, NULL, &logical);

  if (priv->have_transform)
    {
      PangoContext *context = gtk_widget_get_pango_context (widget);
      const PangoMatrix *matrix = pango_context_get_matrix (context);
      pango_matrix_transform_rectangle (matrix, &logical);
    }

  pango_extents_to_pixels (&logical, NULL);

  req_width  = logical.width;
  req_height = logical.height;

  req_width  += border.left + border.right;
  req_height += border.top + border.bottom;

  gtk_widget_get_allocation (widget, &allocation);

  x = floor (allocation.x + border.left + xalign * (allocation.width - req_width) - logical.x);

  baseline_offset = 0;
  baseline = gtk_widget_get_allocated_baseline (widget);
  if (baseline != -1 && !priv->have_transform)
    {
      layout_baseline = pango_layout_get_baseline (priv->layout) / PANGO_SCALE;
      baseline_offset = baseline - layout_baseline;
      yalign = 0.0; /* Can't support yalign while baseline aligning */
    }

  /* bgo#315462 - For single-line labels, *do* align the requisition with
   * respect to the allocation, even if we are under-allocated.  For multi-line
   * labels, always show the top of the text when they are under-allocated.  The
   * rationale is this:
   *
   * - Single-line labels appear in GtkButtons, and it is very easy to get them
   *   to be smaller than their requisition.  The button may clip the label, but
   *   the label will still be able to show most of itself and the focus
   *   rectangle.  Also, it is fairly easy to read a single line of clipped text.
   *
   * - Multi-line labels should not be clipped to showing "something in the
   *   middle".  You want to read the first line, at least, to get some context.
   */
  if (pango_layout_get_line_count (priv->layout) == 1)
    y = floor (allocation.y + border.top + (allocation.height - req_height) * yalign) - logical.y + baseline_offset;
  else
    y = floor (allocation.y + border.top + MAX ((allocation.height - req_height) * yalign, 0)) - logical.y + baseline_offset;

  if (xp)
    *xp = x;

  if (yp)
    *yp = y;
}

static void
gtk_label_get_ink_rect (GtkLabel     *label,
                        GdkRectangle *rect)
{
  GtkLabelPrivate *priv = label->priv;
  GtkStyleContext *context;
  PangoRectangle ink_rect;
  GtkBorder extents;
  int x, y;

  gtk_label_ensure_layout (label);
  get_layout_location (label, &x, &y);
  pango_layout_get_pixel_extents (priv->layout, &ink_rect, NULL);
  context = gtk_widget_get_style_context (GTK_WIDGET (label));
  _gtk_css_shadows_value_get_extents (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_TEXT_SHADOW), &extents);

  rect->x = x + ink_rect.x - extents.left;
  rect->width = ink_rect.width + extents.left + extents.right;
  rect->y = y + ink_rect.y - extents.top;
  rect->height = ink_rect.height + extents.top + extents.bottom;
}

static void
gtk_label_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;
  GdkRectangle clip_rect;

  GTK_WIDGET_CLASS (gtk_label_parent_class)->size_allocate (widget, allocation);

  if (priv->layout)
    gtk_label_update_layout_width (label);

  if (priv->select_info && priv->select_info->window)
    {
      gdk_window_move_resize (priv->select_info->window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }

  gtk_label_get_ink_rect (label, &clip_rect);
  _gtk_widget_set_simple_clip (widget, &clip_rect);
}

static void
gtk_label_update_cursor (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;
  GtkWidget *widget;

  if (!priv->select_info)
    return;

  widget = GTK_WIDGET (label);

  if (gtk_widget_get_realized (widget))
    {
      GdkDisplay *display;
      GdkCursor *cursor;

      if (gtk_widget_is_sensitive (widget))
        {
          display = gtk_widget_get_display (widget);

          if (priv->select_info->active_link)
            cursor = gdk_cursor_new_for_display (display, GDK_HAND2);
          else if (priv->select_info->selectable)
            cursor = gdk_cursor_new_for_display (display, GDK_XTERM);
          else
            cursor = NULL;
        }
      else
        cursor = NULL;

      gdk_window_set_cursor (priv->select_info->window, cursor);

      if (cursor)
        g_object_unref (cursor);
    }
}

static void
gtk_label_state_flags_changed (GtkWidget     *widget,
                               GtkStateFlags  prev_state)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;

  if (priv->select_info)
    {
      if (!gtk_widget_is_sensitive (widget))
        gtk_label_select_region (label, 0, 0);

      gtk_label_update_cursor (label);
    }

  if (GTK_WIDGET_CLASS (gtk_label_parent_class)->state_flags_changed)
    GTK_WIDGET_CLASS (gtk_label_parent_class)->state_flags_changed (widget, prev_state);
}

static void 
gtk_label_style_updated (GtkWidget *widget)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;

  GTK_WIDGET_CLASS (gtk_label_parent_class)->style_updated (widget);

  if (priv->select_info && priv->select_info->links)
    gtk_label_update_layout_attributes (label);
}

static PangoDirection
get_cursor_direction (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;
  GSList *l;

  g_assert (priv->select_info);

  gtk_label_ensure_layout (label);

  for (l = pango_layout_get_lines_readonly (priv->layout); l; l = l->next)
    {
      PangoLayoutLine *line = l->data;

      /* If priv->select_info->selection_end is at the very end of
       * the line, we don't know if the cursor is on this line or
       * the next without looking ahead at the next line. (End
       * of paragraph is different from line break.) But it's
       * definitely in this paragraph, which is good enough
       * to figure out the resolved direction.
       */
       if (line->start_index + line->length >= priv->select_info->selection_end)
	return line->resolved_dir;
    }

  return PANGO_DIRECTION_LTR;
}

static GtkLabelLink *
gtk_label_get_focus_link (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;
  GtkLabelSelectionInfo *info = priv->select_info;
  GList *l;

  if (!info)
    return NULL;

  if (info->selection_anchor != info->selection_end)
    return NULL;

  for (l = info->links; l; l = l->next)
    {
      GtkLabelLink *link = l->data;
      if (link->start <= info->selection_anchor &&
          info->selection_anchor <= link->end)
        return link;
    }

  return NULL;
}

static gboolean
gtk_label_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;
  GtkLabelSelectionInfo *info = priv->select_info;
  GtkAllocation allocation;
  GtkStyleContext *context;
  GtkStateFlags state;
  gint x, y;

  gtk_label_ensure_layout (label);

  context = gtk_widget_get_style_context (widget);
  gtk_widget_get_allocation (widget, &allocation);

  gtk_render_background (context, cr,
                         0, 0,
                         allocation.width, allocation.height);
  gtk_render_frame (context, cr,
                    0, 0,
                    allocation.width, allocation.height);

  if (priv->text && (*priv->text != '\0'))
    {
      get_layout_location (label, &x, &y);

      cairo_translate (cr, -allocation.x, -allocation.y);

      gtk_render_layout (context, cr,
                         x, y,
                         priv->layout);

      state = gtk_widget_get_state_flags (widget);

      if (info &&
          (info->selection_anchor != info->selection_end))
        {
          gint range[2];
          cairo_region_t *clip;

          range[0] = info->selection_anchor;
          range[1] = info->selection_end;

          if (range[0] > range[1])
            {
              gint tmp = range[0];
              range[0] = range[1];
              range[1] = tmp;
            }

          clip = gdk_pango_layout_get_clip_region (priv->layout,
                                                   x, y,
                                                   range,
                                                   1);

          cairo_save (cr);
          gtk_style_context_save (context);

          gdk_cairo_region (cr, clip);
          cairo_clip (cr);

          gtk_style_context_set_state (context, state | GTK_STATE_FLAG_SELECTED);

          gtk_render_background (context, cr,
                                 allocation.x, allocation.y,
                                 allocation.width, allocation.height);

          gtk_render_layout (context, cr,
                             x, y,
                             priv->layout);

          gtk_style_context_restore (context);
          cairo_restore (cr);
          cairo_region_destroy (clip);
        }
      else if (info)
        {
          GtkLabelLink *focus_link;
          GtkLabelLink *active_link;
          gint range[2];
          cairo_region_t *clip;
          GdkRectangle rect;

          if (info->selectable &&
              gtk_widget_has_focus (widget) &&
              gtk_widget_is_drawable (widget))
            {
              PangoDirection cursor_direction;

              cursor_direction = get_cursor_direction (label);
              gtk_render_insertion_cursor (context, cr,
                                           x, y,
                                           priv->layout, priv->select_info->selection_end,
                                           cursor_direction);
            }

          focus_link = gtk_label_get_focus_link (label);
          active_link = info->active_link;

          if (active_link)
            {
              range[0] = active_link->start;
              range[1] = active_link->end;

              cairo_save (cr);
              gtk_style_context_save (context);

              clip = gdk_pango_layout_get_clip_region (priv->layout,
                                                       x, y,
                                                       range,
                                                       1);
              gdk_cairo_region (cr, clip);
              cairo_clip (cr);
              cairo_region_destroy (clip);

              if (info->link_clicked)
                state |= GTK_STATE_FLAG_ACTIVE;
              else
                state |= GTK_STATE_FLAG_PRELIGHT;

              if (active_link->visited)
                state |= GTK_STATE_FLAG_VISITED;
              else
                state |= GTK_STATE_FLAG_LINK;

              gtk_style_context_set_state (context, state);

              gtk_render_background (context, cr,
                                     allocation.x, allocation.y,
                                     allocation.width, allocation.height);

              gtk_render_layout (context, cr,
                                 x, y,
                                 priv->layout);

              gtk_style_context_restore (context);
              cairo_restore (cr);
            }

          if (focus_link && gtk_widget_has_visible_focus (widget))
            {
              range[0] = focus_link->start;
              range[1] = focus_link->end;

              clip = gdk_pango_layout_get_clip_region (priv->layout,
                                                       x, y,
                                                       range,
                                                       1);
              cairo_region_get_extents (clip, &rect);

              gtk_render_focus (context, cr,
                                rect.x, rect.y,
                                rect.width, rect.height);

              cairo_region_destroy (clip);
            }
        }
    }

  return FALSE;
}

static gboolean
separate_uline_pattern (const gchar  *str,
                        guint        *accel_key,
                        gchar       **new_str,
                        gchar       **pattern)
{
  gboolean underscore;
  const gchar *src;
  gchar *dest;
  gchar *pattern_dest;

  *accel_key = GDK_KEY_VoidSymbol;
  *new_str = g_new (gchar, strlen (str) + 1);
  *pattern = g_new (gchar, g_utf8_strlen (str, -1) + 1);

  underscore = FALSE;

  src = str;
  dest = *new_str;
  pattern_dest = *pattern;

  while (*src)
    {
      gunichar c;
      const gchar *next_src;

      c = g_utf8_get_char (src);
      if (c == (gunichar)-1)
	{
	  g_warning ("Invalid input string");
	  g_free (*new_str);
	  g_free (*pattern);

	  return FALSE;
	}
      next_src = g_utf8_next_char (src);

      if (underscore)
	{
	  if (c == '_')
	    *pattern_dest++ = ' ';
	  else
	    {
	      *pattern_dest++ = '_';
	      if (*accel_key == GDK_KEY_VoidSymbol)
		*accel_key = gdk_keyval_to_lower (gdk_unicode_to_keyval (c));
	    }

	  while (src < next_src)
	    *dest++ = *src++;

	  underscore = FALSE;
	}
      else
	{
	  if (c == '_')
	    {
	      underscore = TRUE;
	      src = next_src;
	    }
	  else
	    {
	      while (src < next_src)
		*dest++ = *src++;

	      *pattern_dest++ = ' ';
	    }
	}
    }

  *dest = 0;
  *pattern_dest = 0;

  return TRUE;
}

static void
gtk_label_set_uline_text_internal (GtkLabel    *label,
				   const gchar *str)
{
  GtkLabelPrivate *priv = label->priv;
  guint accel_key = GDK_KEY_VoidSymbol;
  gchar *new_str;
  gchar *pattern;

  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);

  /* Split text into the base text and a separate pattern
   * of underscores.
   */
  if (!separate_uline_pattern (str, &accel_key, &new_str, &pattern))
    return;

  gtk_label_set_text_internal (label, new_str);
  gtk_label_set_pattern_internal (label, pattern, TRUE);
  priv->mnemonic_keyval = accel_key;

  g_free (pattern);
}

/**
 * gtk_label_set_text_with_mnemonic:
 * @label: a #GtkLabel
 * @str: a string
 * 
 * Sets the label’s text from the string @str.
 * If characters in @str are preceded by an underscore, they are underlined
 * indicating that they represent a keyboard accelerator called a mnemonic.
 * The mnemonic key can be used to activate another widget, chosen 
 * automatically, or explicitly using gtk_label_set_mnemonic_widget().
 **/
void
gtk_label_set_text_with_mnemonic (GtkLabel    *label,
				  const gchar *str)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);

  g_object_freeze_notify (G_OBJECT (label));

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, FALSE);
  gtk_label_set_use_underline_internal (label, TRUE);
  
  gtk_label_recalculate (label);

  g_object_thaw_notify (G_OBJECT (label));
}

static void
gtk_label_realize (GtkWidget *widget)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;

  GTK_WIDGET_CLASS (gtk_label_parent_class)->realize (widget);

  if (priv->select_info)
    gtk_label_create_window (label);
}

static void
gtk_label_unrealize (GtkWidget *widget)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;

  if (priv->select_info)
    gtk_label_destroy_window (label);

  GTK_WIDGET_CLASS (gtk_label_parent_class)->unrealize (widget);
}

static void
gtk_label_map (GtkWidget *widget)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;

  GTK_WIDGET_CLASS (gtk_label_parent_class)->map (widget);

  if (priv->select_info)
    gdk_window_show (priv->select_info->window);
}

static void
gtk_label_unmap (GtkWidget *widget)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;

  if (priv->select_info)
    gdk_window_hide (priv->select_info->window);

  GTK_WIDGET_CLASS (gtk_label_parent_class)->unmap (widget);
}

static void
window_to_layout_coords (GtkLabel *label,
                         gint     *x,
                         gint     *y)
{
  GtkAllocation allocation;
  gint lx, ly;
  GtkWidget *widget;

  widget = GTK_WIDGET (label);
  
  /* get layout location in widget->window coords */
  get_layout_location (label, &lx, &ly);

  gtk_widget_get_allocation (widget, &allocation);

  if (x)
    {
      *x += allocation.x; /* go to widget->window */
      *x -= lx;                   /* go to layout */
    }

  if (y)
    {
      *y += allocation.y; /* go to widget->window */
      *y -= ly;                   /* go to layout */
    }
}

#if 0
static void
layout_to_window_coords (GtkLabel *label,
                         gint     *x,
                         gint     *y)
{
  gint lx, ly;
  GtkWidget *widget;

  widget = GTK_WIDGET (label);
  
  /* get layout location in widget->window coords */
  get_layout_location (label, &lx, &ly);
  
  if (x)
    {
      *x += lx;                   /* go to widget->window */
      *x -= widget->allocation.x; /* go to selection window */
    }

  if (y)
    {
      *y += ly;                   /* go to widget->window */
      *y -= widget->allocation.y; /* go to selection window */
    }
}
#endif

static gboolean
get_layout_index (GtkLabel *label,
                  gint      x,
                  gint      y,
                  gint     *index)
{
  GtkLabelPrivate *priv = label->priv;
  gint trailing = 0;
  const gchar *cluster;
  const gchar *cluster_end;
  gboolean inside;

  *index = 0;

  gtk_label_ensure_layout (label);

  window_to_layout_coords (label, &x, &y);

  x *= PANGO_SCALE;
  y *= PANGO_SCALE;

  inside = pango_layout_xy_to_index (priv->layout,
                                     x, y,
                                     index, &trailing);

  cluster = priv->text + *index;
  cluster_end = cluster;
  while (trailing)
    {
      cluster_end = g_utf8_next_char (cluster_end);
      --trailing;
    }

  *index += (cluster_end - cluster);

  return inside;
}

static gboolean
range_is_in_ellipsis_full (GtkLabel *label,
                           gint      range_start,
                           gint      range_end,
                           gint     *ellipsis_start,
                           gint     *ellipsis_end)
{
  GtkLabelPrivate *priv = label->priv;
  PangoLayoutIter *iter;
  gboolean in_ellipsis;

  if (!priv->ellipsize)
    return FALSE;

  gtk_label_ensure_layout (label);

  if (!pango_layout_is_ellipsized (priv->layout))
    return FALSE;

  iter = pango_layout_get_iter (priv->layout);

  in_ellipsis = FALSE;

  do {
    PangoLayoutRun *run;

    run = pango_layout_iter_get_run_readonly (iter);
    if (run)
      {
        PangoItem *item;

        item = ((PangoGlyphItem*)run)->item;

        if (item->offset <= range_start && range_end <= item->offset + item->length)
          {
            if (item->analysis.flags & PANGO_ANALYSIS_FLAG_IS_ELLIPSIS)
              {
                if (ellipsis_start)
                  *ellipsis_start = item->offset;
                if (ellipsis_end)
                  *ellipsis_end = item->offset + item->length;
                in_ellipsis = TRUE;
              }
            break;
          }
        else if (item->offset + item->length >= range_end)
          break;
      }
  } while (pango_layout_iter_next_run (iter));

  pango_layout_iter_free (iter);

  return in_ellipsis;
}

static gboolean
range_is_in_ellipsis (GtkLabel *label,
                      gint      range_start,
                      gint      range_end)
{
  return range_is_in_ellipsis_full (label, range_start, range_end, NULL, NULL);
}

static void
gtk_label_select_word (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;
  gint min, max;

  gint start_index = gtk_label_move_backward_word (label, priv->select_info->selection_end);
  gint end_index = gtk_label_move_forward_word (label, priv->select_info->selection_end);

  min = MIN (priv->select_info->selection_anchor,
	     priv->select_info->selection_end);
  max = MAX (priv->select_info->selection_anchor,
	     priv->select_info->selection_end);

  min = MIN (min, start_index);
  max = MAX (max, end_index);

  gtk_label_select_region_index (label, min, max);
}

static void
gtk_label_grab_focus (GtkWidget *widget)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;
  gboolean select_on_focus;
  GtkLabelLink *link;
  GList *l;

  if (priv->select_info == NULL)
    return;

  GTK_WIDGET_CLASS (gtk_label_parent_class)->grab_focus (widget);

  if (priv->select_info->selectable)
    {
      g_object_get (gtk_widget_get_settings (widget),
                    "gtk-label-select-on-focus",
                    &select_on_focus,
                    NULL);

      if (select_on_focus && !priv->in_click)
        gtk_label_select_region (label, 0, -1);
    }
  else
    {
      if (priv->select_info->links && !priv->in_click)
        {
          for (l = priv->select_info->links; l; l = l->next)
            {
              link = l->data;
              if (!range_is_in_ellipsis (label, link->start, link->end))
                {
                  priv->select_info->selection_anchor = link->start;
                  priv->select_info->selection_end = link->start;
                  _gtk_label_accessible_focus_link_changed (label);
                  break;
                }
            }
        }
    }
}

static gboolean
gtk_label_focus (GtkWidget        *widget,
                 GtkDirectionType  direction)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;
  GtkLabelSelectionInfo *info = priv->select_info;
  GtkLabelLink *focus_link;
  GList *l;

  if (!gtk_widget_is_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      if (info)
        {
          focus_link = gtk_label_get_focus_link (label);
          if (focus_link && direction == GTK_DIR_TAB_BACKWARD)
            {
              for (l = g_list_last (info->links); l; l = l->prev)
                {
                  focus_link = l->data;
                  if (!range_is_in_ellipsis (label, focus_link->start, focus_link->end))
                    {
                      info->selection_anchor = focus_link->start;
                      info->selection_end = focus_link->start;
                      _gtk_label_accessible_focus_link_changed (label);
                    }
                }
            }
        }

      return TRUE;
    }

  if (!info)
    return FALSE;

  if (info->selectable)
    {
      gint index;

      if (info->selection_anchor != info->selection_end)
        goto out;

      index = info->selection_anchor;

      if (direction == GTK_DIR_TAB_FORWARD)
        for (l = info->links; l; l = l->next)
          {
            GtkLabelLink *link = l->data;

            if (link->start > index)
              {
                if (!range_is_in_ellipsis (label, link->start, link->end))
                  {
                    gtk_label_select_region_index (label, link->start, link->start);
                    _gtk_label_accessible_focus_link_changed (label);
                    return TRUE;
                  }
              }
          }
      else if (direction == GTK_DIR_TAB_BACKWARD)
        for (l = g_list_last (info->links); l; l = l->prev)
          {
            GtkLabelLink *link = l->data;

            if (link->end < index)
              {
                if (!range_is_in_ellipsis (label, link->start, link->end))
                  {
                    gtk_label_select_region_index (label, link->start, link->start);
                    _gtk_label_accessible_focus_link_changed (label);
                    return TRUE;
                  }
              }
          }

      goto out;
    }
  else
    {
      focus_link = gtk_label_get_focus_link (label);
      switch (direction)
        {
        case GTK_DIR_TAB_FORWARD:
          if (focus_link)
            {
              l = g_list_find (info->links, focus_link);
              l = l->next;
            }
          else
            l = info->links;
          for (; l; l = l->next)
            {
              GtkLabelLink *link = l->data;
              if (!range_is_in_ellipsis (label, link->start, link->end))
                break;
            }
          break;

        case GTK_DIR_TAB_BACKWARD:
          if (focus_link)
            {
              l = g_list_find (info->links, focus_link);
              l = l->prev;
            }
          else
            l = g_list_last (info->links);
          for (; l; l = l->prev)
            {
              GtkLabelLink *link = l->data;
              if (!range_is_in_ellipsis (label, link->start, link->end))
                break;
            }
          break;

        default:
          goto out;
        }

      if (l)
        {
          focus_link = l->data;
          info->selection_anchor = focus_link->start;
          info->selection_end = focus_link->start;
          _gtk_label_accessible_focus_link_changed (label);
          gtk_widget_queue_draw (widget);

          return TRUE;
        }
    }

out:

  return FALSE;
}

static void
gtk_label_multipress_gesture_pressed (GtkGestureMultiPress *gesture,
                                      gint                  n_press,
                                      gdouble               widget_x,
                                      gdouble               widget_y,
                                      GtkLabel             *label)
{
  GtkLabelPrivate *priv = label->priv;
  GtkLabelSelectionInfo *info = priv->select_info;
  GtkWidget *widget = GTK_WIDGET (label);
  GdkEventSequence *sequence;
  const GdkEvent *event;
  guint button;

  if (info == NULL)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  if (info->active_link)
    {
      if (gdk_event_triggers_context_menu (event))
        {
          info->link_clicked = 1;
          gtk_label_do_popup (label, (GdkEventButton*) event);
          return;
        }
      else if (button == GDK_BUTTON_PRIMARY)
        {
          info->link_clicked = 1;
          gtk_widget_queue_draw (widget);
          if (!info->selectable)
            return;
        }
    }

  if (!info->selectable)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  info->in_drag = FALSE;
  info->select_words = FALSE;

  if (gdk_event_triggers_context_menu ((GdkEvent *) event))
    gtk_label_do_popup (label, (GdkEventButton*) event);
  else if (button == GDK_BUTTON_PRIMARY)
    {
      if (!gtk_widget_has_focus (widget))
        {
          priv->in_click = TRUE;
          gtk_widget_grab_focus (widget);
          priv->in_click = FALSE;
        }

      if (n_press == 3)
        gtk_label_select_region_index (label, 0, strlen (priv->text));
      else if (n_press == 2)
        {
          info->select_words = TRUE;
          gtk_label_select_word (label);
        }
    }

  if (n_press >= 3)
    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
}

static void
gtk_label_multipress_gesture_released (GtkGestureMultiPress *gesture,
                                       gint                  n_press,
                                       gdouble               x,
                                       gdouble               y,
                                       GtkLabel             *label)
{
  GtkLabelPrivate *priv = label->priv;
  GtkLabelSelectionInfo *info = priv->select_info;
  GdkEventSequence *sequence;
  gint index;

  if (info == NULL)
    return;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (!gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
    return;

  if (n_press != 1)
    return;

  if (info->in_drag)
    {
      info->in_drag = 0;
      get_layout_index (label, x, y, &index);
      gtk_label_select_region_index (label, index, index);
    }
  else if (info->active_link &&
           info->selection_anchor == info->selection_end &&
           info->link_clicked)
    {
      emit_activate_link (label, info->active_link);
      info->link_clicked = 0;
    }
}

static void
connect_mnemonics_visible_notify (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;
  GtkWidget *toplevel;
  gboolean connected;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (label));

  if (!GTK_IS_WINDOW (toplevel))
    return;

  /* always set up this widgets initial value */
  priv->mnemonics_visible =
    gtk_window_get_mnemonics_visible (GTK_WINDOW (toplevel));

  connected =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (toplevel),
                                        "gtk-label-mnemonics-visible-connected"));

  if (!connected)
    {
      g_signal_connect (toplevel,
                        "notify::mnemonics-visible",
                        G_CALLBACK (label_mnemonics_visible_changed),
                        label);
      g_object_set_data (G_OBJECT (toplevel),
                         "gtk-label-mnemonics-visible-connected",
                         GINT_TO_POINTER (1));
    }
}

static void
drag_begin_cb (GtkWidget      *widget,
               GdkDragContext *context,
               gpointer        data)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;
  cairo_surface_t *surface = NULL;

  g_signal_handlers_disconnect_by_func (widget, drag_begin_cb, NULL);

  if ((priv->select_info->selection_anchor !=
       priv->select_info->selection_end) &&
      priv->text)
    {
      gint start, end;
      gint len;

      start = MIN (priv->select_info->selection_anchor,
                   priv->select_info->selection_end);
      end = MAX (priv->select_info->selection_anchor,
                 priv->select_info->selection_end);

      len = strlen (priv->text);

      if (end > len)
        end = len;

      if (start > len)
        start = len;

      surface = _gtk_text_util_create_drag_icon (widget,
                                                 priv->text + start,
                                                 end - start);
    }

  if (surface)
    {
      gtk_drag_set_icon_surface (context, surface);
      cairo_surface_destroy (surface);
    }
  else
    {
      gtk_drag_set_icon_default (context);
    }
}

static void
gtk_label_drag_gesture_begin (GtkGestureDrag *gesture,
                              gdouble         start_x,
                              gdouble         start_y,
                              GtkLabel       *label)
{
  GtkLabelPrivate *priv = label->priv;
  GtkLabelSelectionInfo *info = priv->select_info;
  GdkModifierType state_mask;
  GdkEventSequence *sequence;
  const GdkEvent *event;
  gint min, max, index;

  if (!info)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  get_layout_index (label, start_x, start_y, &index);
  min = MIN (info->selection_anchor, info->selection_end);
  max = MAX (info->selection_anchor, info->selection_end);

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  gdk_event_get_state (event, &state_mask);

  if ((info->selection_anchor != info->selection_end) &&
      (state_mask & GDK_SHIFT_MASK))
    {
      if (index > min && index < max)
        {
          /* truncate selection, but keep it as big as possible */
          if (index - min > max - index)
            max = index;
          else
            min = index;
        }
      else
        {
          /* extend (same as motion) */
          min = MIN (min, index);
          max = MAX (max, index);
        }

      /* ensure the anchor is opposite index */
      if (index == min)
        {
          gint tmp = min;
          min = max;
          max = tmp;
        }

      gtk_label_select_region_index (label, min, max);
    }
  else
    {
      if (min < max && min <= index && index <= max)
        {
          info->in_drag = TRUE;
          info->drag_start_x = start_x;
          info->drag_start_y = start_y;
        }
      else
        /* start a replacement */
        gtk_label_select_region_index (label, index, index);
    }
}

static void
gtk_label_drag_gesture_update (GtkGestureDrag *gesture,
                               gdouble         offset_x,
                               gdouble         offset_y,
                               GtkLabel       *label)
{
  GtkLabelPrivate *priv = label->priv;
  GtkLabelSelectionInfo *info = priv->select_info;
  GtkWidget *widget = GTK_WIDGET (label);
  GdkEventSequence *sequence;
  gdouble x, y;
  gint index;

  if (info == NULL || !info->selectable)
    return;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  gtk_gesture_get_point (GTK_GESTURE (gesture), sequence, &x, &y);

  if (info->in_drag)
    {
      if (gtk_drag_check_threshold (widget,
				    info->drag_start_x,
				    info->drag_start_y,
				    x, y))
	{
	  GtkTargetList *target_list = gtk_target_list_new (NULL, 0);
          const GdkEvent *event;

          event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
	  gtk_target_list_add_text_targets (target_list, 0);

          g_signal_connect (widget, "drag-begin",
                            G_CALLBACK (drag_begin_cb), NULL);
	  gtk_drag_begin_with_coordinates (widget, target_list,
                                           GDK_ACTION_COPY,
                                           1, (GdkEvent*) event,
                                           info->drag_start_x,
                                           info->drag_start_y);

	  info->in_drag = FALSE;

	  gtk_target_list_unref (target_list);
	}
    }
  else
    {
      get_layout_index (label, x, y, &index);

      if (index != info->selection_anchor)
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

      if (info->select_words)
        {
          gint min, max;
          gint old_min, old_max;
          gint anchor, end;

          min = gtk_label_move_backward_word (label, index);
          max = gtk_label_move_forward_word (label, index);

          anchor = info->selection_anchor;
          end = info->selection_end;

          old_min = MIN (anchor, end);
          old_max = MAX (anchor, end);

          if (min < old_min)
            {
              anchor = min;
              end = old_max;
            }
          else if (old_max < max)
            {
              anchor = max;
              end = old_min;
            }
          else if (anchor == old_min)
            {
              if (anchor != min)
                anchor = max;
            }
          else
            {
              if (anchor != max)
                anchor = min;
            }

          gtk_label_select_region_index (label, anchor, end);
        }
      else
        gtk_label_select_region_index (label, info->selection_anchor, index);
    }
}

static gboolean
gtk_label_motion (GtkWidget      *widget,
                  GdkEventMotion *event)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;
  GtkLabelSelectionInfo *info = priv->select_info;
  gint index;

  if (info == NULL)
    return FALSE;

  if (info->links && !info->in_drag)
    {
      GList *l;
      GtkLabelLink *link;
      gboolean found = FALSE;

      if (info->selection_anchor == info->selection_end)
        {
          if (get_layout_index (label, event->x, event->y, &index))
            {
              for (l = info->links; l != NULL; l = l->next)
                {
                  link = l->data;
                  if (index >= link->start && index <= link->end)
                    {
                      if (!range_is_in_ellipsis (label, link->start, link->end))
                        found = TRUE;
                      break;
                    }
                }
            }
        }

      if (found)
        {
          if (info->active_link != link)
            {
              info->link_clicked = 0;
              info->active_link = link;
              gtk_label_update_cursor (label);
              gtk_widget_queue_draw (widget);
            }
        }
      else
        {
          if (info->active_link != NULL)
            {
              info->link_clicked = 0;
              info->active_link = NULL;
              gtk_label_update_cursor (label);
              gtk_widget_queue_draw (widget);
            }
        }
    }

  return GTK_WIDGET_CLASS (gtk_label_parent_class)->motion_notify_event (widget, event);
}

static gboolean
gtk_label_leave_notify (GtkWidget        *widget,
                        GdkEventCrossing *event)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;

  if (priv->select_info)
    {
      priv->select_info->active_link = NULL;
      gtk_label_update_cursor (label);
      gtk_widget_queue_draw (widget);
    }

  if (GTK_WIDGET_CLASS (gtk_label_parent_class)->leave_notify_event)
    return GTK_WIDGET_CLASS (gtk_label_parent_class)->leave_notify_event (widget, event);

 return FALSE;
}

static void
gtk_label_create_window (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;
  GtkAllocation allocation;
  GtkWidget *widget;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_assert (priv->select_info);
  widget = GTK_WIDGET (label);
  g_assert (gtk_widget_get_realized (widget));

  if (priv->select_info->window)
    return;

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.event_mask = gtk_widget_get_events (widget) |
    GDK_BUTTON_PRESS_MASK        |
    GDK_BUTTON_RELEASE_MASK      |
    GDK_LEAVE_NOTIFY_MASK        |
    GDK_BUTTON_MOTION_MASK       |
    GDK_POINTER_MOTION_MASK      |
    GDK_POINTER_MOTION_HINT_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;
  if (gtk_widget_is_sensitive (widget) && priv->select_info && priv->select_info->selectable)
    {
      attributes.cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
						      GDK_XTERM);
      attributes_mask |= GDK_WA_CURSOR;
    }


  priv->select_info->window = gdk_window_new (gtk_widget_get_window (widget),
                                               &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->select_info->window);

  if (attributes_mask & GDK_WA_CURSOR)
    g_object_unref (attributes.cursor);
}

static void
gtk_label_destroy_window (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;

  g_assert (priv->select_info);

  if (priv->select_info->window == NULL)
    return;

  gtk_widget_unregister_window (GTK_WIDGET (label), priv->select_info->window);
  gdk_window_destroy (priv->select_info->window);
  priv->select_info->window = NULL;
}

static void
gtk_label_ensure_select_info (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;

  if (priv->select_info == NULL)
    {
      priv->select_info = g_new0 (GtkLabelSelectionInfo, 1);

      gtk_widget_set_can_focus (GTK_WIDGET (label), TRUE);

      if (gtk_widget_get_realized (GTK_WIDGET (label)))
	gtk_label_create_window (label);

      if (gtk_widget_get_mapped (GTK_WIDGET (label)))
        gdk_window_show (priv->select_info->window);
    }
}

static void
gtk_label_clear_select_info (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;

  if (priv->select_info == NULL)
    return;

  if (!priv->select_info->selectable && !priv->select_info->links)
    {
      gtk_label_destroy_window (label);

      g_free (priv->select_info);
      priv->select_info = NULL;

      gtk_widget_set_can_focus (GTK_WIDGET (label), FALSE);
    }
}

/**
 * gtk_label_set_selectable:
 * @label: a #GtkLabel
 * @setting: %TRUE to allow selecting text in the label
 *
 * Selectable labels allow the user to select text from the label, for
 * copy-and-paste.
 **/
void
gtk_label_set_selectable (GtkLabel *label,
                          gboolean  setting)
{
  GtkLabelPrivate *priv;
  gboolean old_setting;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  setting = setting != FALSE;
  old_setting = priv->select_info && priv->select_info->selectable;

  if (setting)
    {
      gtk_label_ensure_select_info (label);
      priv->select_info->selectable = TRUE;
      gtk_label_update_cursor (label);
    }
  else
    {
      if (old_setting)
        {
          /* unselect, to give up the selection */
          gtk_label_select_region (label, 0, 0);

          priv->select_info->selectable = FALSE;
          gtk_label_clear_select_info (label);
          gtk_label_update_cursor (label);
        }
    }
  if (setting != old_setting)
    {
      g_object_freeze_notify (G_OBJECT (label));
      g_object_notify (G_OBJECT (label), "selectable");
      g_object_notify (G_OBJECT (label), "cursor-position");
      g_object_notify (G_OBJECT (label), "selection-bound");
      g_object_thaw_notify (G_OBJECT (label));
      gtk_widget_queue_draw (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_selectable:
 * @label: a #GtkLabel
 * 
 * Gets the value set by gtk_label_set_selectable().
 * 
 * Returns: %TRUE if the user can copy text from the label
 **/
gboolean
gtk_label_get_selectable (GtkLabel *label)
{
  GtkLabelPrivate *priv;

  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  priv = label->priv;

  return priv->select_info && priv->select_info->selectable;
}

/**
 * gtk_label_set_angle:
 * @label: a #GtkLabel
 * @angle: the angle that the baseline of the label makes with
 *   the horizontal, in degrees, measured counterclockwise
 * 
 * Sets the angle of rotation for the label. An angle of 90 reads from
 * from bottom to top, an angle of 270, from top to bottom. The angle
 * setting for the label is ignored if the label is selectable,
 * wrapped, or ellipsized.
 *
 * Since: 2.6
 **/
void
gtk_label_set_angle (GtkLabel *label,
		     gdouble   angle)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  /* Canonicalize to [0,360]. We don't canonicalize 360 to 0, because
   * double property ranges are inclusive, and changing 360 to 0 would
   * make a property editor behave strangely.
   */
  if (angle < 0 || angle > 360.0)
    angle = angle - 360. * floor (angle / 360.);

  if (priv->angle != angle)
    {
      priv->angle = angle;
      
      gtk_label_clear_layout (label);
      gtk_widget_queue_resize (GTK_WIDGET (label));

      g_object_notify (G_OBJECT (label), "angle");
    }
}

/**
 * gtk_label_get_angle:
 * @label: a #GtkLabel
 * 
 * Gets the angle of rotation for the label. See
 * gtk_label_set_angle().
 * 
 * Returns: the angle of rotation for the label
 *
 * Since: 2.6
 **/
gdouble
gtk_label_get_angle  (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), 0.0);
  
  return label->priv->angle;
}

static void
gtk_label_set_selection_text (GtkLabel         *label,
			      GtkSelectionData *selection_data)
{
  GtkLabelPrivate *priv = label->priv;

  if (priv->select_info &&
      (priv->select_info->selection_anchor !=
       priv->select_info->selection_end) &&
      priv->text)
    {
      gint start, end;
      gint len;

      start = MIN (priv->select_info->selection_anchor,
                   priv->select_info->selection_end);
      end = MAX (priv->select_info->selection_anchor,
                 priv->select_info->selection_end);

      len = strlen (priv->text);

      if (end > len)
        end = len;

      if (start > len)
        start = len;

      gtk_selection_data_set_text (selection_data,
				   priv->text + start,
				   end - start);
    }
}

static void
gtk_label_drag_data_get (GtkWidget        *widget,
			 GdkDragContext   *context,
			 GtkSelectionData *selection_data,
			 guint             info,
			 guint             time)
{
  gtk_label_set_selection_text (GTK_LABEL (widget), selection_data);
}

static void
get_text_callback (GtkClipboard     *clipboard,
                   GtkSelectionData *selection_data,
                   guint             info,
                   gpointer          user_data_or_owner)
{
  gtk_label_set_selection_text (GTK_LABEL (user_data_or_owner), selection_data);
}

static void
clear_text_callback (GtkClipboard     *clipboard,
                     gpointer          user_data_or_owner)
{
  GtkLabel *label;
  GtkLabelPrivate *priv;

  label = GTK_LABEL (user_data_or_owner);
  priv = label->priv;

  if (priv->select_info)
    {
      priv->select_info->selection_anchor = priv->select_info->selection_end;

      gtk_widget_queue_draw (GTK_WIDGET (label));
    }
}

static void
gtk_label_select_region_index (GtkLabel *label,
                               gint      anchor_index,
                               gint      end_index)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  if (priv->select_info && priv->select_info->selectable)
    {
      GtkClipboard *clipboard;
      gint s, e;

      /* Ensure that we treat an ellipsized region like a single
       * character with respect to selection.
       */
      if (anchor_index < end_index)
        {
          if (range_is_in_ellipsis_full (label, anchor_index, anchor_index + 1, &s, &e))
            {
              if (priv->select_info->selection_anchor == s)
                anchor_index = e;
              else
                anchor_index = s;
            }
          if (range_is_in_ellipsis_full (label, end_index - 1, end_index, &s, &e))
            {
              if (priv->select_info->selection_end == e)
                end_index = s;
              else
                end_index = e;
            }
        }
      else if (end_index < anchor_index)
        {
          if (range_is_in_ellipsis_full (label, end_index, end_index + 1, &s, &e))
            {
              if (priv->select_info->selection_end == s)
                end_index = e;
              else
                end_index = s;
            }
          if (range_is_in_ellipsis_full (label, anchor_index - 1, anchor_index, &s, &e))
            {
              if (priv->select_info->selection_anchor == e)
                anchor_index = s;
              else
                anchor_index = e;
            }
        }
      else
        {
          if (range_is_in_ellipsis_full (label, anchor_index, anchor_index, &s, &e))
            {
              if (priv->select_info->selection_anchor == s)
                anchor_index = e;
              else if (priv->select_info->selection_anchor == e)
                anchor_index = s;
              else if (anchor_index - s < e - anchor_index)
                anchor_index = s;
              else
                anchor_index = e;
              end_index = anchor_index;
            }
        }

      if (priv->select_info->selection_anchor == anchor_index &&
          priv->select_info->selection_end == end_index)
        return;

      g_object_freeze_notify (G_OBJECT (label));

      if (priv->select_info->selection_anchor != anchor_index)
        g_object_notify (G_OBJECT (label), "selection-bound");
      if (priv->select_info->selection_end != end_index)
        g_object_notify (G_OBJECT (label), "cursor-position");

      priv->select_info->selection_anchor = anchor_index;
      priv->select_info->selection_end = end_index;

      if (gtk_widget_has_screen (GTK_WIDGET (label)))
        clipboard = gtk_widget_get_clipboard (GTK_WIDGET (label),
                                              GDK_SELECTION_PRIMARY);
      else
        clipboard = NULL;

      if (anchor_index != end_index)
        {
          GtkTargetList *list;
          GtkTargetEntry *targets;
          gint n_targets;

          list = gtk_target_list_new (NULL, 0);
          gtk_target_list_add_text_targets (list, 0);
          targets = gtk_target_table_new_from_list (list, &n_targets);

          if (clipboard)
            gtk_clipboard_set_with_owner (clipboard,
                                          targets, n_targets,
                                          get_text_callback,
                                          clear_text_callback,
                                          G_OBJECT (label));

          gtk_target_table_free (targets, n_targets);
          gtk_target_list_unref (list);
        }
      else
        {
          if (clipboard &&
              gtk_clipboard_get_owner (clipboard) == G_OBJECT (label))
            gtk_clipboard_clear (clipboard);
        }

      gtk_widget_queue_draw (GTK_WIDGET (label));

      g_object_thaw_notify (G_OBJECT (label));
    }
}

/**
 * gtk_label_select_region:
 * @label: a #GtkLabel
 * @start_offset: start offset (in characters not bytes)
 * @end_offset: end offset (in characters not bytes)
 *
 * Selects a range of characters in the label, if the label is selectable.
 * See gtk_label_set_selectable(). If the label is not selectable,
 * this function has no effect. If @start_offset or
 * @end_offset are -1, then the end of the label will be substituted.
 **/
void
gtk_label_select_region  (GtkLabel *label,
                          gint      start_offset,
                          gint      end_offset)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  if (priv->text && priv->select_info)
    {
      if (start_offset < 0)
        start_offset = g_utf8_strlen (priv->text, -1);
      
      if (end_offset < 0)
        end_offset = g_utf8_strlen (priv->text, -1);
      
      gtk_label_select_region_index (label,
                                     g_utf8_offset_to_pointer (priv->text, start_offset) - priv->text,
                                     g_utf8_offset_to_pointer (priv->text, end_offset) - priv->text);
    }
}

/**
 * gtk_label_get_selection_bounds:
 * @label: a #GtkLabel
 * @start: (out): return location for start of selection, as a character offset
 * @end: (out): return location for end of selection, as a character offset
 * 
 * Gets the selected range of characters in the label, returning %TRUE
 * if there’s a selection.
 * 
 * Returns: %TRUE if selection is non-empty
 **/
gboolean
gtk_label_get_selection_bounds (GtkLabel  *label,
                                gint      *start,
                                gint      *end)
{
  GtkLabelPrivate *priv;

  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  priv = label->priv;

  if (priv->select_info == NULL)
    {
      /* not a selectable label */
      if (start)
        *start = 0;
      if (end)
        *end = 0;

      return FALSE;
    }
  else
    {
      gint start_index, end_index;
      gint start_offset, end_offset;
      gint len;
      
      start_index = MIN (priv->select_info->selection_anchor,
                   priv->select_info->selection_end);
      end_index = MAX (priv->select_info->selection_anchor,
                 priv->select_info->selection_end);

      len = strlen (priv->text);

      if (end_index > len)
        end_index = len;

      if (start_index > len)
        start_index = len;
      
      start_offset = g_utf8_strlen (priv->text, start_index);
      end_offset = g_utf8_strlen (priv->text, end_index);

      if (start_offset > end_offset)
        {
          gint tmp = start_offset;
          start_offset = end_offset;
          end_offset = tmp;
        }
      
      if (start)
        *start = start_offset;

      if (end)
        *end = end_offset;

      return start_offset != end_offset;
    }
}


/**
 * gtk_label_get_layout:
 * @label: a #GtkLabel
 * 
 * Gets the #PangoLayout used to display the label.
 * The layout is useful to e.g. convert text positions to
 * pixel positions, in combination with gtk_label_get_layout_offsets().
 * The returned layout is owned by the @label so need not be
 * freed by the caller. The @label is free to recreate its layout at
 * any time, so it should be considered read-only.
 *
 * Returns: (transfer none): the #PangoLayout for this label
 **/
PangoLayout*
gtk_label_get_layout (GtkLabel *label)
{
  GtkLabelPrivate *priv;

  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  priv = label->priv;

  gtk_label_ensure_layout (label);

  return priv->layout;
}

/**
 * gtk_label_get_layout_offsets:
 * @label: a #GtkLabel
 * @x: (out) (allow-none): location to store X offset of layout, or %NULL
 * @y: (out) (allow-none): location to store Y offset of layout, or %NULL
 *
 * Obtains the coordinates where the label will draw the #PangoLayout
 * representing the text in the label; useful to convert mouse events
 * into coordinates inside the #PangoLayout, e.g. to take some action
 * if some part of the label is clicked. Of course you will need to
 * create a #GtkEventBox to receive the events, and pack the label
 * inside it, since labels are a #GTK_NO_WINDOW widget. Remember
 * when using the #PangoLayout functions you need to convert to
 * and from pixels using PANGO_PIXELS() or #PANGO_SCALE.
 **/
void
gtk_label_get_layout_offsets (GtkLabel *label,
                              gint     *x,
                              gint     *y)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_ensure_layout (label);

  get_layout_location (label, x, y);
}

/**
 * gtk_label_set_use_markup:
 * @label: a #GtkLabel
 * @setting: %TRUE if the label’s text should be parsed for markup.
 *
 * Sets whether the text of the label contains markup in
 * [Pango’s text markup language][PangoMarkupFormat].
 * See gtk_label_set_markup().
 **/
void
gtk_label_set_use_markup (GtkLabel *label,
			  gboolean  setting)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  g_object_freeze_notify (G_OBJECT (label));

  gtk_label_set_use_markup_internal (label, setting);
  gtk_label_recalculate (label);

  g_object_thaw_notify (G_OBJECT (label));
}

/**
 * gtk_label_get_use_markup:
 * @label: a #GtkLabel
 *
 * Returns whether the label’s text is interpreted as marked up with
 * the [Pango text markup language][PangoMarkupFormat].
 * See gtk_label_set_use_markup ().
 *
 * Returns: %TRUE if the label’s text will be parsed for markup.
 **/
gboolean
gtk_label_get_use_markup (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->priv->use_markup;
}

/**
 * gtk_label_set_use_underline:
 * @label: a #GtkLabel
 * @setting: %TRUE if underlines in the text indicate mnemonics
 *
 * If true, an underline in the text indicates the next character should be
 * used for the mnemonic accelerator key.
 */
void
gtk_label_set_use_underline (GtkLabel *label,
			     gboolean  setting)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  g_object_freeze_notify (G_OBJECT (label));

  gtk_label_set_use_underline_internal (label, setting);
  gtk_label_recalculate (label);

  g_object_thaw_notify (G_OBJECT (label));
}

/**
 * gtk_label_get_use_underline:
 * @label: a #GtkLabel
 *
 * Returns whether an embedded underline in the label indicates a
 * mnemonic. See gtk_label_set_use_underline().
 *
 * Returns: %TRUE whether an embedded underline in the label indicates
 *               the mnemonic accelerator keys.
 **/
gboolean
gtk_label_get_use_underline (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->priv->use_underline;
}

/**
 * gtk_label_set_single_line_mode:
 * @label: a #GtkLabel
 * @single_line_mode: %TRUE if the label should be in single line mode
 *
 * Sets whether the label is in single line mode.
 *
 * Since: 2.6
 */
void
gtk_label_set_single_line_mode (GtkLabel *label,
                                gboolean single_line_mode)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  single_line_mode = single_line_mode != FALSE;

  if (priv->single_line_mode != single_line_mode)
    {
      priv->single_line_mode = single_line_mode;

      gtk_label_clear_layout (label);
      gtk_widget_queue_resize (GTK_WIDGET (label));

      g_object_notify (G_OBJECT (label), "single-line-mode");
    }
}

/**
 * gtk_label_get_single_line_mode:
 * @label: a #GtkLabel
 *
 * Returns whether the label is in single line mode.
 *
 * Returns: %TRUE when the label is in single line mode.
 *
 * Since: 2.6
 **/
gboolean
gtk_label_get_single_line_mode  (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->priv->single_line_mode;
}

/* Compute the X position for an offset that corresponds to the "more important
 * cursor position for that offset. We use this when trying to guess to which
 * end of the selection we should go to when the user hits the left or
 * right arrow key.
 */
static void
get_better_cursor (GtkLabel *label,
		   gint      index,
		   gint      *x,
		   gint      *y)
{
  GtkLabelPrivate *priv = label->priv;
  GdkKeymap *keymap = gdk_keymap_get_for_display (gtk_widget_get_display (GTK_WIDGET (label)));
  PangoDirection keymap_direction = gdk_keymap_get_direction (keymap);
  PangoDirection cursor_direction = get_cursor_direction (label);
  gboolean split_cursor;
  PangoRectangle strong_pos, weak_pos;
  
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (label)),
		"gtk-split-cursor", &split_cursor,
		NULL);

  gtk_label_ensure_layout (label);
  
  pango_layout_get_cursor_pos (priv->layout, index,
			       &strong_pos, &weak_pos);

  if (split_cursor)
    {
      *x = strong_pos.x / PANGO_SCALE;
      *y = strong_pos.y / PANGO_SCALE;
    }
  else
    {
      if (keymap_direction == cursor_direction)
	{
	  *x = strong_pos.x / PANGO_SCALE;
	  *y = strong_pos.y / PANGO_SCALE;
	}
      else
	{
	  *x = weak_pos.x / PANGO_SCALE;
	  *y = weak_pos.y / PANGO_SCALE;
	}
    }
}


static gint
gtk_label_move_logically (GtkLabel *label,
			  gint      start,
			  gint      count)
{
  GtkLabelPrivate *priv = label->priv;
  gint offset = g_utf8_pointer_to_offset (priv->text,
					  priv->text + start);

  if (priv->text)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;
      gint length;

      gtk_label_ensure_layout (label);
      
      length = g_utf8_strlen (priv->text, -1);

      pango_layout_get_log_attrs (priv->layout, &log_attrs, &n_attrs);

      while (count > 0 && offset < length)
	{
	  do
	    offset++;
	  while (offset < length && !log_attrs[offset].is_cursor_position);
	  
	  count--;
	}
      while (count < 0 && offset > 0)
	{
	  do
	    offset--;
	  while (offset > 0 && !log_attrs[offset].is_cursor_position);
	  
	  count++;
	}
      
      g_free (log_attrs);
    }

  return g_utf8_offset_to_pointer (priv->text, offset) - priv->text;
}

static gint
gtk_label_move_visually (GtkLabel *label,
			 gint      start,
			 gint      count)
{
  GtkLabelPrivate *priv = label->priv;
  gint index;

  index = start;
  
  while (count != 0)
    {
      int new_index, new_trailing;
      gboolean split_cursor;
      gboolean strong;

      gtk_label_ensure_layout (label);

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (label)),
		    "gtk-split-cursor", &split_cursor,
		    NULL);

      if (split_cursor)
	strong = TRUE;
      else
	{
	  GdkKeymap *keymap = gdk_keymap_get_for_display (gtk_widget_get_display (GTK_WIDGET (label)));
	  PangoDirection keymap_direction = gdk_keymap_get_direction (keymap);

	  strong = keymap_direction == get_cursor_direction (label);
	}
      
      if (count > 0)
	{
	  pango_layout_move_cursor_visually (priv->layout, strong, index, 0, 1, &new_index, &new_trailing);
	  count--;
	}
      else
	{
	  pango_layout_move_cursor_visually (priv->layout, strong, index, 0, -1, &new_index, &new_trailing);
	  count++;
	}

      if (new_index < 0 || new_index == G_MAXINT)
	break;

      index = new_index;
      
      while (new_trailing--)
	index = g_utf8_next_char (priv->text + new_index) - priv->text;
    }
  
  return index;
}

static gint
gtk_label_move_forward_word (GtkLabel *label,
			     gint      start)
{
  GtkLabelPrivate *priv = label->priv;
  gint new_pos = g_utf8_pointer_to_offset (priv->text,
					   priv->text + start);
  gint length;

  length = g_utf8_strlen (priv->text, -1);
  if (new_pos < length)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;

      gtk_label_ensure_layout (label);

      pango_layout_get_log_attrs (priv->layout, &log_attrs, &n_attrs);

      /* Find the next word end */
      new_pos++;
      while (new_pos < n_attrs && !log_attrs[new_pos].is_word_end)
	new_pos++;

      g_free (log_attrs);
    }

  return g_utf8_offset_to_pointer (priv->text, new_pos) - priv->text;
}


static gint
gtk_label_move_backward_word (GtkLabel *label,
			      gint      start)
{
  GtkLabelPrivate *priv = label->priv;
  gint new_pos = g_utf8_pointer_to_offset (priv->text,
					   priv->text + start);

  if (new_pos > 0)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;

      gtk_label_ensure_layout (label);

      pango_layout_get_log_attrs (priv->layout, &log_attrs, &n_attrs);

      new_pos -= 1;

      /* Find the previous word beginning */
      while (new_pos > 0 && !log_attrs[new_pos].is_word_start)
	new_pos--;

      g_free (log_attrs);
    }

  return g_utf8_offset_to_pointer (priv->text, new_pos) - priv->text;
}

static void
gtk_label_move_cursor (GtkLabel       *label,
                       GtkMovementStep step,
                       gint            count,
                       gboolean        extend_selection)
{
  GtkLabelPrivate *priv = label->priv;
  gint old_pos;
  gint new_pos;

  if (priv->select_info == NULL)
    return;

  old_pos = new_pos = priv->select_info->selection_end;

  if (priv->select_info->selection_end != priv->select_info->selection_anchor &&
      !extend_selection)
    {
      /* If we have a current selection and aren't extending it, move to the
       * start/or end of the selection as appropriate
       */
      switch (step)
        {
        case GTK_MOVEMENT_VISUAL_POSITIONS:
          {
            gint end_x, end_y;
            gint anchor_x, anchor_y;
            gboolean end_is_left;

            get_better_cursor (label, priv->select_info->selection_end, &end_x, &end_y);
            get_better_cursor (label, priv->select_info->selection_anchor, &anchor_x, &anchor_y);

            end_is_left = (end_y < anchor_y) || (end_y == anchor_y && end_x < anchor_x);

            if (count < 0)
              new_pos = end_is_left ? priv->select_info->selection_end : priv->select_info->selection_anchor;
            else
              new_pos = !end_is_left ? priv->select_info->selection_end : priv->select_info->selection_anchor;
            break;
          }
        case GTK_MOVEMENT_LOGICAL_POSITIONS:
        case GTK_MOVEMENT_WORDS:
          if (count < 0)
            new_pos = MIN (priv->select_info->selection_end, priv->select_info->selection_anchor);
          else
            new_pos = MAX (priv->select_info->selection_end, priv->select_info->selection_anchor);
          break;
        case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
        case GTK_MOVEMENT_PARAGRAPH_ENDS:
        case GTK_MOVEMENT_BUFFER_ENDS:
          /* FIXME: Can do better here */
          new_pos = count < 0 ? 0 : strlen (priv->text);
          break;
        case GTK_MOVEMENT_DISPLAY_LINES:
        case GTK_MOVEMENT_PARAGRAPHS:
        case GTK_MOVEMENT_PAGES:
        case GTK_MOVEMENT_HORIZONTAL_PAGES:
          break;
        }
    }
  else
    {
      switch (step)
        {
        case GTK_MOVEMENT_LOGICAL_POSITIONS:
          new_pos = gtk_label_move_logically (label, new_pos, count);
          break;
        case GTK_MOVEMENT_VISUAL_POSITIONS:
          new_pos = gtk_label_move_visually (label, new_pos, count);
          if (new_pos == old_pos)
            {
              if (!extend_selection)
                {
                  if (!gtk_widget_keynav_failed (GTK_WIDGET (label),
                                                 count > 0 ?
                                                 GTK_DIR_RIGHT : GTK_DIR_LEFT))
                    {
                      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (label));

                      if (toplevel)
                        gtk_widget_child_focus (toplevel,
                                                count > 0 ?
                                                GTK_DIR_RIGHT : GTK_DIR_LEFT);
                    }
                }
              else
                {
                  gtk_widget_error_bell (GTK_WIDGET (label));
                }
            }
          break;
        case GTK_MOVEMENT_WORDS:
          while (count > 0)
            {
              new_pos = gtk_label_move_forward_word (label, new_pos);
              count--;
            }
          while (count < 0)
            {
              new_pos = gtk_label_move_backward_word (label, new_pos);
              count++;
            }
          if (new_pos == old_pos)
            gtk_widget_error_bell (GTK_WIDGET (label));
          break;
        case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
        case GTK_MOVEMENT_PARAGRAPH_ENDS:
        case GTK_MOVEMENT_BUFFER_ENDS:
          /* FIXME: Can do better here */
          new_pos = count < 0 ? 0 : strlen (priv->text);
          if (new_pos == old_pos)
            gtk_widget_error_bell (GTK_WIDGET (label));
          break;
        case GTK_MOVEMENT_DISPLAY_LINES:
        case GTK_MOVEMENT_PARAGRAPHS:
        case GTK_MOVEMENT_PAGES:
        case GTK_MOVEMENT_HORIZONTAL_PAGES:
          break;
        }
    }

  if (extend_selection)
    gtk_label_select_region_index (label,
                                   priv->select_info->selection_anchor,
                                   new_pos);
  else
    gtk_label_select_region_index (label, new_pos, new_pos);
}

static void
gtk_label_copy_clipboard (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;

  if (priv->text && priv->select_info)
    {
      gint start, end;
      gint len;
      GtkClipboard *clipboard;

      start = MIN (priv->select_info->selection_anchor,
                   priv->select_info->selection_end);
      end = MAX (priv->select_info->selection_anchor,
                 priv->select_info->selection_end);

      len = strlen (priv->text);

      if (end > len)
        end = len;

      if (start > len)
        start = len;

      clipboard = gtk_widget_get_clipboard (GTK_WIDGET (label), GDK_SELECTION_CLIPBOARD);

      if (start != end)
	gtk_clipboard_set_text (clipboard, priv->text + start, end - start);
      else
        {
          GtkLabelLink *link;

          link = gtk_label_get_focus_link (label);
          if (link)
            gtk_clipboard_set_text (clipboard, link->uri, -1);
        }
    }
}

static void
gtk_label_select_all (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;

  gtk_label_select_region_index (label, 0, strlen (priv->text));
}

/* Quick hack of a popup menu
 */
static void
activate_cb (GtkWidget *menuitem,
	     GtkLabel  *label)
{
  const gchar *signal = g_object_get_data (G_OBJECT (menuitem), "gtk-signal");
  g_signal_emit_by_name (label, signal);
}

static void
append_action_signal (GtkLabel     *label,
		      GtkWidget    *menu,
		      const gchar  *text,
		      const gchar  *signal,
                      gboolean      sensitive)
{
  GtkWidget *menuitem = gtk_menu_item_new_with_mnemonic (text);

  g_object_set_data (G_OBJECT (menuitem), I_("gtk-signal"), (char *)signal);
  g_signal_connect (menuitem, "activate",
		    G_CALLBACK (activate_cb), label);

  gtk_widget_set_sensitive (menuitem, sensitive);
  
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}

static void
popup_menu_detach (GtkWidget *attach_widget,
		   GtkMenu   *menu)
{
  GtkLabel *label = GTK_LABEL (attach_widget);
  GtkLabelPrivate *priv = label->priv;

  if (priv->select_info)
    priv->select_info->popup_menu = NULL;
}

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer	user_data)
{
  GtkLabel *label;
  GtkWidget *widget;
  GtkAllocation allocation;
  GtkRequisition req;
  GdkScreen *screen;

  label = GTK_LABEL (user_data);
  widget = GTK_WIDGET (label);

  g_return_if_fail (gtk_widget_get_realized (widget));

  screen = gtk_widget_get_screen (widget);
  gdk_window_get_origin (gtk_widget_get_window (widget), x, y);

  gtk_widget_get_allocation (widget, &allocation);

  *x += allocation.x;
  *y += allocation.y;

  gtk_widget_get_preferred_size (GTK_WIDGET (menu),
                                 &req, NULL);

  gtk_widget_get_allocation (widget, &allocation);

  *x += allocation.width / 2;
  *y += allocation.height;

  *x = CLAMP (*x, 0, MAX (0, gdk_screen_get_width (screen) - req.width));
  *y = CLAMP (*y, 0, MAX (0, gdk_screen_get_height (screen) - req.height));
}

static void
open_link_activate_cb (GtkMenuItem *menuitem,
                       GtkLabel    *label)
{
  GtkLabelLink *link;

  link = g_object_get_data (G_OBJECT (menuitem), "link");
  emit_activate_link (label, link);
}

static void
copy_link_activate_cb (GtkMenuItem *menuitem,
                       GtkLabel    *label)
{
  GtkLabelLink *link;
  GtkClipboard *clipboard;

  link = g_object_get_data (G_OBJECT (menuitem), "link");
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (label), GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, link->uri, -1);
}

static gboolean
gtk_label_popup_menu (GtkWidget *widget)
{
  gtk_label_do_popup (GTK_LABEL (widget), NULL);

  return TRUE;
}

static void
gtk_label_do_popup (GtkLabel       *label,
                    GdkEventButton *event)
{
  GtkLabelPrivate *priv = label->priv;
  GtkWidget *menuitem;
  GtkWidget *menu;
  gboolean have_selection;
  GtkLabelLink *link;

  if (!priv->select_info)
    return;

  if (priv->select_info->popup_menu)
    gtk_widget_destroy (priv->select_info->popup_menu);

  priv->select_info->popup_menu = menu = gtk_menu_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (menu),
                               GTK_STYLE_CLASS_CONTEXT_MENU);

  gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (label), popup_menu_detach);

  have_selection =
    priv->select_info->selection_anchor != priv->select_info->selection_end;

  if (event)
    {
      if (priv->select_info->link_clicked)
        link = priv->select_info->active_link;
      else
        link = NULL;
    }
  else
    link = gtk_label_get_focus_link (label);

  if (!have_selection && link)
    {
      /* Open Link */
      menuitem = gtk_menu_item_new_with_mnemonic (_("_Open Link"));
      g_object_set_data (G_OBJECT (menuitem), "link", link);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      g_signal_connect (G_OBJECT (menuitem), "activate",
                        G_CALLBACK (open_link_activate_cb), label);

      /* Copy Link Address */
      menuitem = gtk_menu_item_new_with_mnemonic (_("Copy _Link Address"));
      g_object_set_data (G_OBJECT (menuitem), "link", link);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      g_signal_connect (G_OBJECT (menuitem), "activate",
                        G_CALLBACK (copy_link_activate_cb), label);
    }
  else
    {
      append_action_signal (label, menu, _("Cu_t"), "cut-clipboard", FALSE);
      append_action_signal (label, menu, _("_Copy"), "copy-clipboard", have_selection);
      append_action_signal (label, menu, _("_Paste"), "paste-clipboard", FALSE);
  
      menuitem = gtk_menu_item_new_with_mnemonic (_("_Delete"));
      gtk_widget_set_sensitive (menuitem, FALSE);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      menuitem = gtk_separator_menu_item_new ();
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      menuitem = gtk_menu_item_new_with_mnemonic (_("Select _All"));
      g_signal_connect_swapped (menuitem, "activate",
			        G_CALLBACK (gtk_label_select_all), label);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    }

  g_signal_emit (label, signals[POPULATE_POPUP], 0, menu);

  if (event)
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                    NULL, NULL,
                    event->button, event->time);
  else
    {
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                      popup_position_func, label,
                      0, gtk_get_current_event_time ());
      gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
    }
}

static void
gtk_label_clear_links (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;

  if (!priv->select_info)
    return;

  g_list_free_full (priv->select_info->links, (GDestroyNotify) link_free);
  priv->select_info->links = NULL;
  priv->select_info->active_link = NULL;

  _gtk_label_accessible_update_links (label);
}

static gboolean
gtk_label_activate_link (GtkLabel    *label,
                         const gchar *uri)
{
  GtkWidget *widget = GTK_WIDGET (label);
  GError *error = NULL;

  if (!gtk_show_uri (gtk_widget_get_screen (widget),
                     uri, gtk_get_current_event_time (), &error))
    {
      g_warning ("Unable to show '%s': %s", uri, error->message);
      g_error_free (error);
    }

  return TRUE;
}

static void
emit_activate_link (GtkLabel     *label,
                    GtkLabelLink *link)
{
  GtkLabelPrivate *priv = label->priv;
  gboolean handled;

  g_signal_emit (label, signals[ACTIVATE_LINK], 0, link->uri, &handled);
  if (handled && priv->track_links && !link->visited)
    {
      link->visited = TRUE;
      /* FIXME: shouldn't have to redo everything here */
      gtk_label_clear_layout (label);
    }
}

static void
gtk_label_activate_current_link (GtkLabel *label)
{
  GtkLabelLink *link;
  GtkWidget *widget = GTK_WIDGET (label);

  link = gtk_label_get_focus_link (label);

  if (link)
    {
      emit_activate_link (label, link);
    }
  else
    {
      GtkWidget *toplevel;
      GtkWindow *window;
      GtkWidget *default_widget, *focus_widget;

      toplevel = gtk_widget_get_toplevel (widget);
      if (GTK_IS_WINDOW (toplevel))
        {
          window = GTK_WINDOW (toplevel);

          if (window)
            {
              default_widget = gtk_window_get_default_widget (window);
              focus_widget = gtk_window_get_focus (window);

              if (default_widget != widget &&
                  !(widget == focus_widget && (!default_widget || !gtk_widget_is_sensitive (default_widget))))
                gtk_window_activate_default (window);
            }
        }
    }
}

static GtkLabelLink *
gtk_label_get_current_link (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;
  GtkLabelLink *link;

  if (!priv->select_info)
    return NULL;

  if (priv->select_info->link_clicked)
    link = priv->select_info->active_link;
  else
    link = gtk_label_get_focus_link (label);

  return link;
}

/**
 * gtk_label_get_current_uri:
 * @label: a #GtkLabel
 *
 * Returns the URI for the currently active link in the label.
 * The active link is the one under the mouse pointer or, in a
 * selectable label, the link in which the text cursor is currently
 * positioned.
 *
 * This function is intended for use in a #GtkLabel::activate-link handler
 * or for use in a #GtkWidget::query-tooltip handler.
 *
 * Returns: the currently active URI. The string is owned by GTK+ and must
 *   not be freed or modified.
 *
 * Since: 2.18
 */
const gchar *
gtk_label_get_current_uri (GtkLabel *label)
{
  GtkLabelLink *link;

  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  link = gtk_label_get_current_link (label);

  if (link)
    return link->uri;

  return NULL;
}

/**
 * gtk_label_set_track_visited_links:
 * @label: a #GtkLabel
 * @track_links: %TRUE to track visited links
 *
 * Sets whether the label should keep track of clicked
 * links (and use a different color for them).
 *
 * Since: 2.18
 */
void
gtk_label_set_track_visited_links (GtkLabel *label,
                                   gboolean  track_links)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  track_links = track_links != FALSE;

  if (priv->track_links != track_links)
    {
      priv->track_links = track_links;

      /* FIXME: shouldn't have to redo everything here */
      gtk_label_recalculate (label);

      g_object_notify (G_OBJECT (label), "track-visited-links");
    }
}

/**
 * gtk_label_get_track_visited_links:
 * @label: a #GtkLabel
 *
 * Returns whether the label is currently keeping track
 * of clicked links.
 *
 * Returns: %TRUE if clicked links are remembered
 *
 * Since: 2.18
 */
gboolean
gtk_label_get_track_visited_links (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->priv->track_links;
}

static gboolean
gtk_label_query_tooltip (GtkWidget  *widget,
                         gint        x,
                         gint        y,
                         gboolean    keyboard_tip,
                         GtkTooltip *tooltip)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv = label->priv;
  GtkLabelSelectionInfo *info = priv->select_info;
  gint index = -1;
  GList *l;

  if (info && info->links)
    {
      if (keyboard_tip)
        {
          if (info->selection_anchor == info->selection_end)
            index = info->selection_anchor;
        }
      else
        {
          if (!get_layout_index (label, x, y, &index))
            index = -1;
        }

      if (index != -1)
        {
          for (l = info->links; l != NULL; l = l->next)
            {
              GtkLabelLink *link = l->data;
              if (index >= link->start && index <= link->end)
                {
                  if (link->title)
                    {
                      gtk_tooltip_set_markup (tooltip, link->title);
                      return TRUE;
                    }
                  break;
                }
            }
        }
    }

  return GTK_WIDGET_CLASS (gtk_label_parent_class)->query_tooltip (widget,
                                                                   x, y,
                                                                   keyboard_tip,
                                                                   tooltip);
}

gint
_gtk_label_get_cursor_position (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;

  if (priv->select_info && priv->select_info->selectable)
    return g_utf8_pointer_to_offset (priv->text,
                                     priv->text + priv->select_info->selection_end);

  return 0;
}

gint
_gtk_label_get_selection_bound (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;

  if (priv->select_info && priv->select_info->selectable)
    return g_utf8_pointer_to_offset (priv->text,
                                     priv->text + priv->select_info->selection_anchor);

  return 0;
}

/**
 * gtk_label_set_lines:
 * @label: a #GtkLabel
 * @lines: the desired number of lines, or -1
 *
 * Sets the number of lines to which an ellipsized, wrapping label
 * should be limited. This has no effect if the label is not wrapping
 * or ellipsized. Set this to -1 if you don’t want to limit the
 * number of lines.
 *
 * Since: 3.10
 */
void
gtk_label_set_lines (GtkLabel *label,
                     gint      lines)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = label->priv;

  if (priv->lines != lines)
    {
      priv->lines = lines;
      gtk_label_clear_layout (label);
      g_object_notify (G_OBJECT (label), "lines");
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_lines:
 * @label: a #GtkLabel
 *
 * Gets the number of lines to which an ellipsized, wrapping
 * label should be limited. See gtk_label_set_lines().
 *
 * Returns: The number of lines
 *
 * Since: 3.10
 */
gint
gtk_label_get_lines (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), -1);

  return label->priv->lines;
}

gint
_gtk_label_get_n_links (GtkLabel *label)
{
  GtkLabelPrivate *priv = label->priv;

  if (priv->select_info)
    return g_list_length (priv->select_info->links);

  return 0;
}

const gchar *
_gtk_label_get_link_uri (GtkLabel *label,
                         gint      idx)
{
  GtkLabelPrivate *priv = label->priv;
  gint i;
  GList *l;
  GtkLabelLink *link;

  if (priv->select_info)
    for (l = priv->select_info->links, i = 0; l; l = l->next, i++)
      {
        if (i == idx)
          {
            link = l->data;
            return link->uri;
          }
      }

  return NULL;
}

void
_gtk_label_get_link_extent (GtkLabel *label,
                            gint      idx,
                            gint     *start,
                            gint     *end)
{
  GtkLabelPrivate *priv = label->priv;
  gint i;
  GList *l;
  GtkLabelLink *link;

  if (priv->select_info)
    for (l = priv->select_info->links, i = 0; l; l = l->next, i++)
      {
        if (i == idx)
          {
            link = l->data;
            *start = link->start;
            *end = link->end;
            return;
          }
      }

  *start = -1;
  *end = -1;
}

gint
_gtk_label_get_link_at (GtkLabel *label, 
                        gint      pos)
{
  GtkLabelPrivate *priv = label->priv;
  gint i;
  GList *l;
  GtkLabelLink *link;

  if (priv->select_info)
    for (l = priv->select_info->links, i = 0; l; l = l->next, i++)
      {
        link = l->data;
        if (link->start <= pos && pos < link->end)
          return i;
      }

  return -1;
}

void
_gtk_label_activate_link (GtkLabel *label, 
                          gint      idx)
{
  GtkLabelPrivate *priv = label->priv;
  gint i;
  GList *l;
  GtkLabelLink *link;

  if (priv->select_info)
    for (l = priv->select_info->links, i = 0; l; l = l->next, i++)
      {
        if (i == idx)
          {
            link = l->data;
            emit_activate_link (label, link);
            return;
          }
      }
}

gboolean
_gtk_label_get_link_visited (GtkLabel *label,
                             gint      idx)
{
  GtkLabelPrivate *priv = label->priv;
  gint i;
  GList *l;
  GtkLabelLink *link;

  if (priv->select_info)
    for (l = priv->select_info->links, i = 0; l; l = l->next, i++)
      {
        if (i == idx)
          {
            link = l->data;
            return link->visited;
          }
      }

  return FALSE;
}

gboolean
_gtk_label_get_link_focused (GtkLabel *label,
                             gint      idx)
{
  GtkLabelPrivate *priv = label->priv;
  gint i;
  GList *l;
  GtkLabelLink *link;
  GtkLabelSelectionInfo *info = priv->select_info;

  if (!info)
    return FALSE;

  if (info->selection_anchor != info->selection_end)
    return FALSE;

  for (l = info->links, i = 0; l; l = l->next, i++)
    {
      if (i == idx)
        {
          link = l->data;
          if (link->start <= info->selection_anchor &&
              info->selection_anchor <= link->end)
            return TRUE;
        }
    }

  return FALSE;
}

/**
 * gtk_label_set_xalign:
 * @label: a #GtkLabel
 * @xalign: the new xalign value, between 0 and 1
 *
 * Sets the #GtkLabel:xalign property for @label.
 *
 * Since: 3.16
 */
void
gtk_label_set_xalign (GtkLabel *label,
                      gfloat    xalign)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  xalign = CLAMP (xalign, 0.0, 1.0); 

  if (label->priv->xalign == xalign)
    return;

  label->priv->xalign = xalign;

  gtk_widget_queue_draw (GTK_WIDGET (label));
  g_object_notify (G_OBJECT (label), "xalign");
}

/**
 * gtk_label_get_xalign:
 * @label: a #GtkLabel
 *
 * Gets the #GtkLabel:xalign property for @label.
 *
 * Returns: the xalign property
 *
 * Since: 3.16
 */
gfloat
gtk_label_get_xalign (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), 0.5);

  return label->priv->xalign;
}

/**
 * gtk_label_set_yalign:
 * @label: a #GtkLabel
 * @xalign: the new yalign value, between 0 and 1
 *
 * Sets the #GtkLabel:yalign property for @label.
 *
 * Since: 3.16
 */
void
gtk_label_set_yalign (GtkLabel *label,
                      gfloat    yalign)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  yalign = CLAMP (yalign, 0.0, 1.0); 

  if (label->priv->yalign == yalign)
    return;

  label->priv->yalign = yalign;

  gtk_widget_queue_draw (GTK_WIDGET (label));
  g_object_notify (G_OBJECT (label), "yalign");
}

/**
 * gtk_label_get_yalign:
 * @label: a #GtkLabel
 *
 * Gets the #GtkLabel:yalign property for @label.
 *
 * Returns: the yalign property
 *
 * Since: 3.16
 */
gfloat
gtk_label_get_yalign (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), 0.5);

  return label->priv->yalign;
}
