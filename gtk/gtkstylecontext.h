/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GTK_STYLE_CONTEXT_H__
#define __GTK_STYLE_CONTEXT_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkborder.h>
#include <gtk/gtkcsssection.h>
#include <gtk/gtkstyleprovider.h>
#include <gtk/gtktypes.h>
#include <atk/atk.h>

G_BEGIN_DECLS

#define GTK_TYPE_STYLE_CONTEXT         (gtk_style_context_get_type ())
#define GTK_STYLE_CONTEXT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_STYLE_CONTEXT, GtkStyleContext))
#define GTK_STYLE_CONTEXT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_STYLE_CONTEXT, GtkStyleContextClass))
#define GTK_IS_STYLE_CONTEXT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_STYLE_CONTEXT))
#define GTK_IS_STYLE_CONTEXT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_STYLE_CONTEXT))
#define GTK_STYLE_CONTEXT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_STYLE_CONTEXT, GtkStyleContextClass))

typedef struct _GtkStyleContextClass GtkStyleContextClass;
typedef struct _GtkStyleContextPrivate GtkStyleContextPrivate;

struct _GtkStyleContext
{
  GObject parent_object;
  GtkStyleContextPrivate *priv;
};

struct _GtkStyleContextClass
{
  GObjectClass parent_class;

  void (* changed) (GtkStyleContext *context);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

/* Default set of properties that GtkStyleContext may contain */

/**
 * GTK_STYLE_PROPERTY_BACKGROUND_COLOR:
 *
 * A property holding the background color of rendered elements as a #GdkRGBA.
 */
#define GTK_STYLE_PROPERTY_BACKGROUND_COLOR "background-color"

/**
 * GTK_STYLE_PROPERTY_COLOR:
 *
 * A property holding the foreground color of rendered elements as a #GdkRGBA.
 */
#define GTK_STYLE_PROPERTY_COLOR "color"

/**
 * GTK_STYLE_PROPERTY_FONT:
 *
 * A property holding the font properties used when rendering text
 * as a #PangoFontDescription.
 */
#define GTK_STYLE_PROPERTY_FONT "font"

/**
 * GTK_STYLE_PROPERTY_PADDING:
 *
 * A property holding the rendered element’s padding as a #GtkBorder. The
 * padding is defined as the spacing between the inner part of the element border
 * and its child. It’s the innermost spacing property of the padding/border/margin
 * series.
 */
#define GTK_STYLE_PROPERTY_PADDING "padding"

/**
 * GTK_STYLE_PROPERTY_BORDER_WIDTH:
 *
 * A property holding the rendered element’s border width in pixels as
 * a #GtkBorder. The border is the intermediary spacing property of the
 * padding/border/margin series.
 *
 * gtk_render_frame() uses this property to find out the frame line width,
 * so #GtkWidgets rendering frames may need to add up this padding when
 * requesting size
 */
#define GTK_STYLE_PROPERTY_BORDER_WIDTH "border-width"

/**
 * GTK_STYLE_PROPERTY_MARGIN:
 *
 * A property holding the rendered element’s margin as a #GtkBorder. The
 * margin is defined as the spacing between the border of the element
 * and its surrounding elements. It is external to #GtkWidget's
 * size allocations, and the most external spacing property of the
 * padding/border/margin series.
 */
#define GTK_STYLE_PROPERTY_MARGIN "margin"

/**
 * GTK_STYLE_PROPERTY_BORDER_RADIUS:
 *
 * A property holding the rendered element’s border radius in pixels as a #gint.
 */
#define GTK_STYLE_PROPERTY_BORDER_RADIUS "border-radius"

/**
 * GTK_STYLE_PROPERTY_BORDER_STYLE:
 *
 * A property holding the element’s border style as a #GtkBorderStyle.
 */
#define GTK_STYLE_PROPERTY_BORDER_STYLE "border-style"

/**
 * GTK_STYLE_PROPERTY_BORDER_COLOR:
 *
 * A property holding the element’s border color as a #GdkRGBA.
 */
#define GTK_STYLE_PROPERTY_BORDER_COLOR "border-color"

/**
 * GTK_STYLE_PROPERTY_BACKGROUND_IMAGE:
 *
 * A property holding the element’s background as a #cairo_pattern_t.
 */
#define GTK_STYLE_PROPERTY_BACKGROUND_IMAGE "background-image"

/* Predefined set of CSS classes */

/**
 * GTK_STYLE_CLASS_CELL:
 *
 * A CSS class to match content rendered in cell views.
 *
 * This is used by cell renderers, e.g. in #GtkIconView
 * and #GtkTreeView.
 */
#define GTK_STYLE_CLASS_CELL "cell"

/**
 * GTK_STYLE_CLASS_DIM_LABEL:
 *
 * A CSS class to match dimmed labels.
 *
 * This should be used for toning down right aligned labels as
 * compared to the entry value.
 */
#define GTK_STYLE_CLASS_DIM_LABEL "dim-label"

/**
 * GTK_STYLE_CLASS_ENTRY:
 *
 * A CSS class to match text entries.
 *
 * This is used by #GtkEntry.
 */
#define GTK_STYLE_CLASS_ENTRY "entry"

/**
 * GTK_STYLE_CLASS_COMBOBOX_ENTRY:
 *
 * A CSS class to match combobox entries.
 *
 * This is used by #GtkComboBox.
 */
#define GTK_STYLE_CLASS_COMBOBOX_ENTRY "combobox-entry"

/**
 * GTK_STYLE_CLASS_BUTTON:
 *
 * A CSS class to match buttons.
 *
 * This is used by #GtkButton and its subclasses, as well
 * as various other widget pieces that appear like buttons,
 * e.g. the arrows in a #GtkCalendar.
 */
#define GTK_STYLE_CLASS_BUTTON "button"

/**
 * GTK_STYLE_CLASS_LIST:
 *
 * A CSS class to match lists.
 *
 * This is used by #GtkListBox.
 */
#define GTK_STYLE_CLASS_LIST "list"

/**
 * GTK_STYLE_CLASS_LIST_ROW:
 *
 * A CSS class to match list rowss.
 *
 * This is used by #GtkListBoxRow.
 */
#define GTK_STYLE_CLASS_LIST_ROW "list-row"

/**
 * GTK_STYLE_CLASS_CALENDAR:
 *
 * A CSS class to match calendars.
 *
 * This is not used by GTK+ itself, currently.
 */
#define GTK_STYLE_CLASS_CALENDAR "calendar"

/**
 * GTK_STYLE_CLASS_SLIDER:
 *
 * A CSS class to match sliders.
 *
 * This is used by #GtkSwitch and #GtkRange and its subclasses.
 */
#define GTK_STYLE_CLASS_SLIDER "slider"

/**
 * GTK_STYLE_CLASS_BACKGROUND:
 *
 * A CSS class to match the window background.
 */
#define GTK_STYLE_CLASS_BACKGROUND "background"

/**
 * GTK_STYLE_CLASS_RUBBERBAND:
 *
 * A CSS class to match the rubberband selection rectangle.
 *
 * This is used in #GtkIconView and #GtkTreeView.
 */
#define GTK_STYLE_CLASS_RUBBERBAND "rubberband"

/**
 * GTK_STYLE_CLASS_TOOLTIP:
 *
 * A CSS class to match tooltip windows.
 */
#define GTK_STYLE_CLASS_TOOLTIP "tooltip"

/**
 * GTK_STYLE_CLASS_MENU:
 *
 * A CSS class to match menus.
 *
 * This is used in #GtkMenu.
 */
#define GTK_STYLE_CLASS_MENU "menu"

/**
 * GTK_STYLE_CLASS_CONTEXT_MENU:
 *
 * A CSS class to match context menus.
 *
 * This style class is useful when you want to prevent
 * a context menu from inheriting e.g. font changes from
 * the widget it is attached to.
 */
#define GTK_STYLE_CLASS_CONTEXT_MENU "context-menu"

/**
 * GTK_STYLE_CLASS_MENUBAR:
 *
 * A CSS class to menubars.
 *
 * This is used in #GtkMenuBar.
 */
#define GTK_STYLE_CLASS_MENUBAR "menubar"

/**
 * GTK_STYLE_CLASS_MENUITEM:
 *
 * A CSS class to match menu items.
 *
 * This is used in #GtkMenuItem and its subclasses.
 */
#define GTK_STYLE_CLASS_MENUITEM "menuitem"

/**
 * GTK_STYLE_CLASS_TOOLBAR:
 *
 * A CSS class to match toolbars.
 *
 * This is used in #GtkToolbar.
 */
#define GTK_STYLE_CLASS_TOOLBAR "toolbar"

/**
 * GTK_STYLE_CLASS_PRIMARY_TOOLBAR:
 *
 * A CSS class to match primary toolbars.
 *
 * This should be used for the “main” toolbar of an application,
 * right below its menubar.
 */
#define GTK_STYLE_CLASS_PRIMARY_TOOLBAR "primary-toolbar"

/**
 * GTK_STYLE_CLASS_INLINE_TOOLBAR:
 *
 * A CSS class to match inline toolbars.
 *
 * This should be used for toolbars that are used to hold
 * actions below lists, as seen e.g. in the left pane of the
 * file chooser.
 */
#define GTK_STYLE_CLASS_INLINE_TOOLBAR "inline-toolbar"

/**
 * GTK_STYLE_CLASS_RADIO:
 *
 * A CSS class to match radio buttons.
 *
 * This is used in #GtkRadioButton, #GtkRadioMenuItem and
 * #GtkCellRendererToggle.
 */
#define GTK_STYLE_CLASS_RADIO "radio"

/**
 * GTK_STYLE_CLASS_CHECK:
 *
 * A CSS class to match check boxes.
 *
 * This is used in #GtkCheckButton, #GtkCheckMenuItem and
 * #GtkCellRendererToggle.
 */
#define GTK_STYLE_CLASS_CHECK "check"

/**
 * GTK_STYLE_CLASS_DEFAULT:
 *
 * A CSS class to match the default widget.
 *
 * This is used by #GtkButton.
 */
#define GTK_STYLE_CLASS_DEFAULT "default"

/**
 * GTK_STYLE_CLASS_TROUGH:
 *
 * A CSS class to match troughs, as in scrollbars and progressbars.
 *
 * This is used in #GtkRange and its subclasses, #GtkProgressBar
 * and #GtkSwitch.
 */
#define GTK_STYLE_CLASS_TROUGH "trough"

/**
 * GTK_STYLE_CLASS_SCROLLBAR:
 *
 * A CSS class to match scrollbars.
 */
#define GTK_STYLE_CLASS_SCROLLBAR "scrollbar"

/**
 * GTK_STYLE_CLASS_SCROLLBARS_JUNCTION:
 *
 * A CSS class to match the junction area between an horizontal
 * and vertical scrollbar, when they’re both shown.
 * 
 * This is used in #GtkScrolledWindow.
 */
#define GTK_STYLE_CLASS_SCROLLBARS_JUNCTION "scrollbars-junction"

/**
 * GTK_STYLE_CLASS_SCALE:
 *
 * A CSS class to match scale widgets.
 *
 * This is used in #GtkScale.
 */
#define GTK_STYLE_CLASS_SCALE "scale"

/**
 * GTK_STYLE_CLASS_SCALE_HAS_MARKS_ABOVE:
 *
 * A CSS class to match scale widgets with marks attached,
 * all the marks are above for horizontal #GtkScale.
 * left for vertical #GtkScale.
 */
#define GTK_STYLE_CLASS_SCALE_HAS_MARKS_ABOVE "scale-has-marks-above"

/**
 * GTK_STYLE_CLASS_SCALE_HAS_MARKS_BELOW:
 *
 * A CSS class to match scale widgets with marks attached,
 * all the marks are below for horizontal #GtkScale,
 * right for vertical #GtkScale.
 */
#define GTK_STYLE_CLASS_SCALE_HAS_MARKS_BELOW "scale-has-marks-below"

/**
 * GTK_STYLE_CLASS_HEADER:
 *
 * A CSS class to match a header element.
 *
 * This is used for the header in #GtkCalendar.
 */
#define GTK_STYLE_CLASS_HEADER "header"

/**
 * GTK_STYLE_CLASS_ACCELERATOR:
 *
 * A CSS class to match an accelerator.
 *
 * This is used for the accelerator in #GtkAccelLabel.
 */
#define GTK_STYLE_CLASS_ACCELERATOR "accelerator"

/**
 * GTK_STYLE_CLASS_RAISED:
 *
 * A CSS class to match a raised control, such as a raised
 * button on a toolbar.
 *
 * This should be used in conjunction with #GTK_STYLE_CLASS_PRIMARY_TOOLBAR.
 */
#define GTK_STYLE_CLASS_RAISED "raised"

/**
 * GTK_STYLE_CLASS_LINKED:
 *
 * A CSS class to match a linked area, such as a box containing buttons
 * belonging to the same control.
 */
#define GTK_STYLE_CLASS_LINKED "linked"

/**
 * GTK_STYLE_CLASS_GRIP:
 *
 * A CSS class defining a resize grip.
 *
 * This is used for the resize grip in #GtkWindow.
 */
#define GTK_STYLE_CLASS_GRIP "grip"

/**
 * GTK_STYLE_CLASS_DOCK:
 *
 * A CSS class defining a dock area.
 *
 * This is used by #GtkHandleBox.
 */
#define GTK_STYLE_CLASS_DOCK "dock"

/**
 * GTK_STYLE_CLASS_PROGRESSBAR:
 *
 * A CSS class to use when rendering activity as a progressbar.
 *
 * This is used in #GtkProgressBar and when drawing progress
 * inside a #GtkEntry or in #GtkCellRendererProgress.
 */
#define GTK_STYLE_CLASS_PROGRESSBAR "progressbar"

/**
 * GTK_STYLE_CLASS_SPINNER:
 *
 * A CSS class to use when rendering activity as a “spinner”.
 *
 * This is used by #GtkSpinner and #GtkCellRendererSpinner.
 */
#define GTK_STYLE_CLASS_SPINNER "spinner"

/**
 * GTK_STYLE_CLASS_MARK:
 *
 * A CSS class defining marks in a widget, such as in scales.
 *
 * Used in #GtkScale.
 */
#define GTK_STYLE_CLASS_MARK "mark"

/**
 * GTK_STYLE_CLASS_EXPANDER:
 *
 * A CSS class defining an expander, such as those in treeviews.
 *
 * Used for drawing expanders in #GtkTreeView, GtkExpander and
 * #GtkToolItemGroup.
 */
#define GTK_STYLE_CLASS_EXPANDER "expander"

/**
 * GTK_STYLE_CLASS_SPINBUTTON:
 *
 * A CSS class defining an spinbutton.
 *
 * This is used in #GtkSpinButton.
 */
#define GTK_STYLE_CLASS_SPINBUTTON "spinbutton"

/**
 * GTK_STYLE_CLASS_NOTEBOOK:
 *
 * A CSS class defining a notebook.
 *
 * Used in #GtkNotebook.
 */
#define GTK_STYLE_CLASS_NOTEBOOK "notebook"

/**
 * GTK_STYLE_CLASS_VIEW:
 *
 * A CSS class defining a view, such as iconviews or treeviews.
 *
 * This is used in #GtkTreeView, #GtkIconView, #GtkTextView,
 * as well as #GtkCalendar.
 */
#define GTK_STYLE_CLASS_VIEW "view"

/**
 * GTK_STYLE_CLASS_SIDEBAR:
 *
 * A CSS class defining a sidebar, such as the left side in
 * a file chooser.
 *
 * This is used in #GtkFileChooser and in #GtkAssistant.
 */
#define GTK_STYLE_CLASS_SIDEBAR "sidebar"

/**
 * GTK_STYLE_CLASS_IMAGE:
 *
 * A CSS class defining an image, such as the icon in an entry.
 *
 * This is used when rendering icons in #GtkEntry.
 */
#define GTK_STYLE_CLASS_IMAGE "image"

/**
 * GTK_STYLE_CLASS_HIGHLIGHT:
 *
 * A CSS class defining a highlighted area, such as headings in
 * assistants and calendars.
 *
 * This is used in #GtkAssistant and #GtkCalendar.
 */
#define GTK_STYLE_CLASS_HIGHLIGHT "highlight"

/**
 * GTK_STYLE_CLASS_FRAME:
 *
 * A CSS class defining a frame delimiting content, such as
 * #GtkFrame or the scrolled window frame around the
 * scrollable area.
 *
 * This is used in #GtkFrame and #GtkScrollbar.
 */
#define GTK_STYLE_CLASS_FRAME "frame"

/**
 * GTK_STYLE_CLASS_DND:
 *
 * A CSS class for a drag-and-drop indicator.
 *
 * This is used when drawing an outline around a potential
 * drop target during DND.
 */
#define GTK_STYLE_CLASS_DND "dnd"

/**
 * GTK_STYLE_CLASS_PANE_SEPARATOR:
 *
 * A CSS class for a pane separator, such as those in #GtkPaned.
 *
 * Used in #GtkPaned.
 */
#define GTK_STYLE_CLASS_PANE_SEPARATOR "pane-separator"

/**
 * GTK_STYLE_CLASS_SEPARATOR:
 *
 * A CSS class for a separator.
 *
 * This is used in #GtkSeparator, #GtkSeparatorMenuItem,
 * #GtkSeparatorToolItem, and when drawing separators in #GtkTreeView.
 */
#define GTK_STYLE_CLASS_SEPARATOR "separator"

/**
 * GTK_STYLE_CLASS_INFO:
 *
 * A CSS class for an area displaying an informational message,
 * such as those in infobars.
 *
 * This is used by #GtkInfoBar.
 */
#define GTK_STYLE_CLASS_INFO "info"

/**
 * GTK_STYLE_CLASS_WARNING:
 *
 * A CSS class for an area displaying a warning message,
 * such as those in infobars.
 *
 * This is used by #GtkInfoBar.
 */
#define GTK_STYLE_CLASS_WARNING "warning"

/**
 * GTK_STYLE_CLASS_QUESTION:
 *
 * A CSS class for an area displaying a question to the user,
 * such as those in infobars.
 *
 * This is used by #GtkInfoBar.
 */
#define GTK_STYLE_CLASS_QUESTION "question"

/**
 * GTK_STYLE_CLASS_ERROR:
 *
 * A CSS class for an area displaying an error message,
 * such as those in infobars.
 *
 * This is used by #GtkInfoBar.
 */
#define GTK_STYLE_CLASS_ERROR "error"

/**
 * GTK_STYLE_CLASS_HORIZONTAL:
 *
 * A CSS class for horizontally layered widgets.
 *
 * This is used by widgets implementing #GtkOrientable.
 */
#define GTK_STYLE_CLASS_HORIZONTAL "horizontal"

/**
 * GTK_STYLE_CLASS_VERTICAL:
 *
 * A CSS class for vertically layered widgets.
 *
 * This is used by widgets implementing #GtkOrientable.
 */
#define GTK_STYLE_CLASS_VERTICAL "vertical"

/**
 * GTK_STYLE_CLASS_TOP:
 *
 * A CSS class to indicate an area at the top of a widget.
 *
 * This is used by widgets that can render an area in different
 * positions, such as tabs in a #GtkNotebook.
 */
#define GTK_STYLE_CLASS_TOP "top"

/**
 * GTK_STYLE_CLASS_BOTTOM:
 *
 * A CSS class to indicate an area at the bottom of a widget.
 *
 * This is used by widgets that can render an area in different
 * positions, such as tabs in a #GtkNotebook.
 */
#define GTK_STYLE_CLASS_BOTTOM "bottom"

/**
 * GTK_STYLE_CLASS_LEFT:
 *
 * A CSS class to indicate an area at the left of a widget.
 *
 * This is used by widgets that can render an area in different
 * positions, such as tabs in a #GtkNotebook.
 */
#define GTK_STYLE_CLASS_LEFT "left"

/**
 * GTK_STYLE_CLASS_RIGHT:
 *
 * A CSS class to indicate an area at the right of a widget.
 *
 * This is used by widgets that can render an area in different
 * positions, such as tabs in a #GtkNotebook.
 */
#define GTK_STYLE_CLASS_RIGHT "right"

/**
 * GTK_STYLE_CLASS_PULSE:
 *
 * A CSS class to use when rendering a pulse in an indeterminate progress bar.
 *
 * This is used by #GtkProgressBar and #GtkEntry.
 */
#define GTK_STYLE_CLASS_PULSE "pulse"

/**
 * GTK_STYLE_CLASS_ARROW:
 *
 * A CSS class used when rendering an arrow element.
 *
 * Note that #gtk_render_arrow automatically adds this style class
 * to the style context when rendering an arrow element.
 */
#define GTK_STYLE_CLASS_ARROW "arrow"

/**
 * GTK_STYLE_CLASS_OSD:
 *
 * A CSS class used when rendering an OSD (On Screen Display) element,
 * on top of another container.
 */
#define GTK_STYLE_CLASS_OSD "osd"

/**
 * GTK_STYLE_CLASS_LEVEL_BAR:
 *
 * A CSS class used when rendering a level indicator, such
 * as a battery charge level, or a password strength.
 *
 * This is used by #GtkLevelBar.
 */
#define GTK_STYLE_CLASS_LEVEL_BAR "level-bar"

/**
 * GTK_STYLE_CLASS_CURSOR_HANDLE:
 *
 * A CSS class used when rendering a drag handle for
 * text selection.
 */
#define GTK_STYLE_CLASS_CURSOR_HANDLE "cursor-handle"

/**
 * GTK_STYLE_CLASS_INSERTION_CURSOR:
 *
 * A CSS class used when rendering a drag handle for
 * the insertion cursor position.
 */
#define GTK_STYLE_CLASS_INSERTION_CURSOR "insertion-cursor"

/**
 * GTK_STYLE_CLASS_TITLEBAR:
 *
 * A CSS class used when rendering a titlebar in a toplevel
 * window.
 */
#define GTK_STYLE_CLASS_TITLEBAR "titlebar"

/**
 * GTK_STYLE_CLASS_NEEDS_ATTENTION:
 *
 * A CSS class used when an element needs the user attention,
 * for instance a button in a stack switcher corresponding to
 * a hidden page that changed state.
*
 * Since: 3.12
 */
#define GTK_STYLE_CLASS_NEEDS_ATTENTION "needs-attention"

/**
 * GTK_STYLE_CLASS_SUGGESTED_ACTION:
 *
 * A CSS class used when an action (usually a button) is the
 * primary suggested action in a specific context.
 *
 * Since: 3.12
 */
#define GTK_STYLE_CLASS_SUGGESTED_ACTION "suggested-action"

/**
 * GTK_STYLE_CLASS_DESTRUCTIVE_ACTION:
 *
 * A CSS class used when an action (usually a button) is
 * one that is expected to remove or destroy something visible
 * to the user.
 *
 * Since: 3.12
 */
#define GTK_STYLE_CLASS_DESTRUCTIVE_ACTION "destructive-action"

/**
 * GTK_STYLE_CLASS_POPOVER:
 *
 * A CSS class that matches popovers. Used by #GtkPopover.
 *
 * Since: 3.14
 */
#define GTK_STYLE_CLASS_POPOVER "popover"

/* Predefined set of widget regions */

/**
 * GTK_STYLE_REGION_ROW:
 *
 * A widget region name to define a treeview row.
 */
#define GTK_STYLE_REGION_ROW "row"

/**
 * GTK_STYLE_REGION_COLUMN:
 *
 * A widget region name to define a treeview column.
 */
#define GTK_STYLE_REGION_COLUMN "column"

/**
 * GTK_STYLE_REGION_COLUMN_HEADER:
 *
 * A widget region name to define a treeview column header.
 */
#define GTK_STYLE_REGION_COLUMN_HEADER "column-header"

/**
 * GTK_STYLE_REGION_TAB:
 *
 * A widget region name to define a notebook tab.
 */
#define GTK_STYLE_REGION_TAB "tab"

GDK_AVAILABLE_IN_ALL
GType gtk_style_context_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkStyleContext * gtk_style_context_new (void);

GDK_AVAILABLE_IN_ALL
void gtk_style_context_add_provider_for_screen    (GdkScreen        *screen,
                                                   GtkStyleProvider *provider,
                                                   guint             priority);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_remove_provider_for_screen (GdkScreen        *screen,
                                                   GtkStyleProvider *provider);

GDK_AVAILABLE_IN_ALL
void gtk_style_context_add_provider    (GtkStyleContext  *context,
                                        GtkStyleProvider *provider,
                                        guint             priority);

GDK_AVAILABLE_IN_ALL
void gtk_style_context_remove_provider (GtkStyleContext  *context,
                                        GtkStyleProvider *provider);

GDK_AVAILABLE_IN_ALL
void gtk_style_context_save    (GtkStyleContext *context);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_restore (GtkStyleContext *context);

GDK_AVAILABLE_IN_ALL
GtkCssSection * gtk_style_context_get_section (GtkStyleContext *context,
                                               const gchar     *property);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_get_property (GtkStyleContext *context,
                                     const gchar     *property,
                                     GtkStateFlags    state,
                                     GValue          *value);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_get_valist   (GtkStyleContext *context,
                                     GtkStateFlags    state,
                                     va_list          args);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_get          (GtkStyleContext *context,
                                     GtkStateFlags    state,
                                     ...) G_GNUC_NULL_TERMINATED;

GDK_AVAILABLE_IN_ALL
void          gtk_style_context_set_state    (GtkStyleContext *context,
                                              GtkStateFlags    flags);
GDK_AVAILABLE_IN_ALL
GtkStateFlags gtk_style_context_get_state    (GtkStyleContext *context);

GDK_AVAILABLE_IN_3_10
void          gtk_style_context_set_scale    (GtkStyleContext *context,
                                              gint             scale);
GDK_AVAILABLE_IN_3_10
gint          gtk_style_context_get_scale    (GtkStyleContext *context);

GDK_DEPRECATED_IN_3_6
gboolean      gtk_style_context_state_is_running (GtkStyleContext *context,
                                                  GtkStateType     state,
                                                  gdouble         *progress);

GDK_AVAILABLE_IN_ALL
void          gtk_style_context_set_path     (GtkStyleContext *context,
                                              GtkWidgetPath   *path);
GDK_AVAILABLE_IN_ALL
const GtkWidgetPath * gtk_style_context_get_path (GtkStyleContext *context);
GDK_AVAILABLE_IN_3_4
void          gtk_style_context_set_parent   (GtkStyleContext *context,
                                              GtkStyleContext *parent);
GDK_AVAILABLE_IN_ALL
GtkStyleContext *gtk_style_context_get_parent (GtkStyleContext *context);

GDK_AVAILABLE_IN_ALL
GList *  gtk_style_context_list_classes (GtkStyleContext *context);

GDK_AVAILABLE_IN_ALL
void     gtk_style_context_add_class    (GtkStyleContext *context,
                                         const gchar     *class_name);
GDK_AVAILABLE_IN_ALL
void     gtk_style_context_remove_class (GtkStyleContext *context,
                                         const gchar     *class_name);
GDK_AVAILABLE_IN_ALL
gboolean gtk_style_context_has_class    (GtkStyleContext *context,
                                         const gchar     *class_name);

GDK_AVAILABLE_IN_ALL
GList *  gtk_style_context_list_regions (GtkStyleContext *context);

GDK_AVAILABLE_IN_ALL
void     gtk_style_context_add_region    (GtkStyleContext    *context,
                                          const gchar        *region_name,
                                          GtkRegionFlags      flags);
GDK_AVAILABLE_IN_ALL
void     gtk_style_context_remove_region (GtkStyleContext    *context,
                                          const gchar        *region_name);
GDK_AVAILABLE_IN_ALL
gboolean gtk_style_context_has_region    (GtkStyleContext    *context,
                                          const gchar        *region_name,
                                          GtkRegionFlags     *flags_return);

GDK_AVAILABLE_IN_ALL
void gtk_style_context_get_style_property (GtkStyleContext *context,
                                           const gchar     *property_name,
                                           GValue          *value);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_get_style_valist   (GtkStyleContext *context,
                                           va_list          args);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_get_style          (GtkStyleContext *context,
                                           ...);

GDK_DEPRECATED_IN_3_10_FOR(gtk_icon_theme_lookup_icon)
GtkIconSet * gtk_style_context_lookup_icon_set (GtkStyleContext *context,
                                                const gchar     *stock_id);
GDK_DEPRECATED_IN_3_10
GdkPixbuf  * gtk_icon_set_render_icon_pixbuf   (GtkIconSet      *icon_set,
                                                GtkStyleContext *context,
                                                GtkIconSize      size);
GDK_DEPRECATED_IN_3_10
cairo_surface_t  *
gtk_icon_set_render_icon_surface               (GtkIconSet      *icon_set,
						GtkStyleContext *context,
						GtkIconSize      size,
						int              scale,
						GdkWindow       *for_window);

GDK_AVAILABLE_IN_ALL
void        gtk_style_context_set_screen (GtkStyleContext *context,
                                          GdkScreen       *screen);
GDK_AVAILABLE_IN_ALL
GdkScreen * gtk_style_context_get_screen (GtkStyleContext *context);

GDK_AVAILABLE_IN_3_8
void           gtk_style_context_set_frame_clock (GtkStyleContext *context,
                                                  GdkFrameClock   *frame_clock);
GDK_AVAILABLE_IN_3_8
GdkFrameClock *gtk_style_context_get_frame_clock (GtkStyleContext *context);

/**
 * GTK_STYLE_CLASS_READ_ONLY:
 *
 * A CSS class used to indicate a read-only state.
 */
#define GTK_STYLE_CLASS_READ_ONLY "read-only"

GDK_DEPRECATED_IN_3_8_FOR(gtk_style_context_set_state)
void             gtk_style_context_set_direction (GtkStyleContext  *context,
                                                  GtkTextDirection  direction);
GDK_DEPRECATED_IN_3_8_FOR(gtk_style_context_get_state)
GtkTextDirection gtk_style_context_get_direction (GtkStyleContext  *context);

GDK_AVAILABLE_IN_ALL
void             gtk_style_context_set_junction_sides (GtkStyleContext  *context,
                                                       GtkJunctionSides  sides);
GDK_AVAILABLE_IN_ALL
GtkJunctionSides gtk_style_context_get_junction_sides (GtkStyleContext  *context);

GDK_AVAILABLE_IN_ALL
gboolean gtk_style_context_lookup_color (GtkStyleContext *context,
                                         const gchar     *color_name,
                                         GdkRGBA         *color);

GDK_DEPRECATED_IN_3_6
void  gtk_style_context_notify_state_change (GtkStyleContext *context,
                                             GdkWindow       *window,
                                             gpointer         region_id,
                                             GtkStateType     state,
                                             gboolean         state_value);
GDK_DEPRECATED_IN_3_6
void  gtk_style_context_cancel_animations   (GtkStyleContext *context,
                                             gpointer         region_id);
GDK_DEPRECATED_IN_3_6
void  gtk_style_context_scroll_animations   (GtkStyleContext *context,
                                             GdkWindow       *window,
                                             gint             dx,
                                             gint             dy);

GDK_DEPRECATED_IN_3_6
void gtk_style_context_push_animatable_region (GtkStyleContext *context,
                                               gpointer         region_id);
GDK_DEPRECATED_IN_3_6
void gtk_style_context_pop_animatable_region  (GtkStyleContext *context);

/* Some helper functions to retrieve most common properties */
GDK_AVAILABLE_IN_ALL
void gtk_style_context_get_color            (GtkStyleContext *context,
                                             GtkStateFlags    state,
                                             GdkRGBA         *color);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_get_background_color (GtkStyleContext *context,
                                             GtkStateFlags    state,
                                             GdkRGBA         *color);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_get_border_color     (GtkStyleContext *context,
                                             GtkStateFlags    state,
                                             GdkRGBA         *color);

GDK_DEPRECATED_IN_3_8_FOR(gtk_style_context_get)
const PangoFontDescription *
     gtk_style_context_get_font             (GtkStyleContext *context,
                                             GtkStateFlags    state);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_get_border           (GtkStyleContext *context,
                                             GtkStateFlags    state,
                                             GtkBorder       *border);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_get_padding          (GtkStyleContext *context,
                                             GtkStateFlags    state,
                                             GtkBorder       *padding);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_get_margin           (GtkStyleContext *context,
                                             GtkStateFlags    state,
                                             GtkBorder       *margin);

GDK_DEPRECATED_IN_3_12
void gtk_style_context_invalidate           (GtkStyleContext *context);
GDK_AVAILABLE_IN_ALL
void gtk_style_context_reset_widgets        (GdkScreen       *screen);

GDK_AVAILABLE_IN_ALL
void gtk_style_context_set_background       (GtkStyleContext *context,
                                             GdkWindow       *window);

/* Paint methods */
GDK_AVAILABLE_IN_ALL
void        gtk_render_check       (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
GDK_AVAILABLE_IN_ALL
void        gtk_render_option      (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
GDK_AVAILABLE_IN_ALL
void        gtk_render_arrow       (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              angle,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              size);
GDK_AVAILABLE_IN_ALL
void        gtk_render_background  (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
GDK_AVAILABLE_IN_ALL
void        gtk_render_frame       (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
GDK_AVAILABLE_IN_ALL
void        gtk_render_expander    (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
GDK_AVAILABLE_IN_ALL
void        gtk_render_focus       (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
GDK_AVAILABLE_IN_ALL
void        gtk_render_layout      (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    PangoLayout         *layout);
GDK_AVAILABLE_IN_ALL
void        gtk_render_line        (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x0,
                                    gdouble              y0,
                                    gdouble              x1,
                                    gdouble              y1);
GDK_AVAILABLE_IN_ALL
void        gtk_render_slider      (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height,
                                    GtkOrientation       orientation);
GDK_AVAILABLE_IN_ALL
void        gtk_render_frame_gap   (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height,
                                    GtkPositionType      gap_side,
                                    gdouble              xy0_gap,
                                    gdouble              xy1_gap);
GDK_AVAILABLE_IN_ALL
void        gtk_render_extension   (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height,
                                    GtkPositionType      gap_side);
GDK_AVAILABLE_IN_ALL
void        gtk_render_handle      (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
GDK_AVAILABLE_IN_ALL
void        gtk_render_activity    (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
GDK_DEPRECATED_IN_3_10_FOR(gtk_icon_theme_load_icon)
GdkPixbuf * gtk_render_icon_pixbuf (GtkStyleContext     *context,
                                    const GtkIconSource *source,
                                    GtkIconSize          size);
GDK_AVAILABLE_IN_3_2
void        gtk_render_icon        (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    GdkPixbuf           *pixbuf,
                                    gdouble              x,
                                    gdouble              y);
GDK_AVAILABLE_IN_3_10
void        gtk_render_icon_surface (GtkStyleContext    *context,
				     cairo_t            *cr,
				     cairo_surface_t    *surface,
				     gdouble             x,
				     gdouble             y);
GDK_AVAILABLE_IN_3_4
void        gtk_render_insertion_cursor
                                   (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    PangoLayout         *layout,
                                    int                  index,
                                    PangoDirection       direction);
GDK_DEPRECATED_IN_3_4
void   gtk_draw_insertion_cursor    (GtkWidget          *widget,
                                     cairo_t            *cr,
                                     const GdkRectangle *location,
                                     gboolean            is_primary,
                                     GtkTextDirection    direction,
                                     gboolean            draw_arrow);

/* Accessibility support */
AtkAttributeSet *_gtk_style_context_get_attributes (AtkAttributeSet *attributes,
                                                    GtkStyleContext *context,
                                                    GtkStateFlags    flags);

G_END_DECLS

#endif /* __GTK_STYLE_CONTEXT_H__ */
