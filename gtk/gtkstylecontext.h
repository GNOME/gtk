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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_STYLE_CONTEXT_H__
#define __GTK_STYLE_CONTEXT_H__

#include <glib-object.h>
#include <gtk/gtkstyleprovider.h>
#include <gtk/gtkwidgetpath.h>
#include <gtk/gtkborder.h>

G_BEGIN_DECLS

#define GTK_TYPE_STYLE_CONTEXT         (gtk_style_context_get_type ())
#define GTK_STYLE_CONTEXT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_STYLE_CONTEXT, GtkStyleContext))
#define GTK_STYLE_CONTEXT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_STYLE_CONTEXT, GtkStyleContextClass))
#define GTK_IS_STYLE_CONTEXT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_STYLE_CONTEXT))
#define GTK_IS_STYLE_CONTEXT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_STYLE_CONTEXT))
#define GTK_STYLE_CONTEXT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_STYLE_CONTEXT, GtkStyleContextClass))

typedef struct _GtkStyleContext GtkStyleContext;
typedef struct _GtkStyleContextClass GtkStyleContextClass;

struct _GtkStyleContext
{
  GObject parent_object;
  gpointer priv;
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
 * A property holding the rendered element's padding as a #GtkBorder. The
 * padding is defined as the spacing between the inner part of the element border
 * and its child. It's the innermost spacing property of the padding/border/margin
 * series.
 */
#define GTK_STYLE_PROPERTY_PADDING "padding"

/**
 * GTK_STYLE_PROPERTY_BORDER_WIDTH:
 *
 * A property holding the rendered element's border width in pixels as
 * a #GtkBorder. The border is the intermediary spacing property of the
 * padding/border/margin series.
 *
 * gtk_render_frame() uses this property to find out the frame line width,
 * so #GtkWidget<!-- -->s rendering frames may need to add up this padding when
 * requesting size
 */
#define GTK_STYLE_PROPERTY_BORDER_WIDTH "border-width"

/**
 * GTK_STYLE_PROPERTY_MARGIN:
 *
 * A property holding the rendered element's margin as a #GtkBorder. The
 * margin is defined as the spacing between the border of the element
 * and its surrounding elements. It is external to #GtkWidget<!-- -->s's
 * size allocations, and the most external spacing property of the
 * padding/border/margin series.
 */
#define GTK_STYLE_PROPERTY_MARGIN "margin"

/**
 * GTK_STYLE_PROPERTY_BORDER_RADIUS:
 *
 * A property holding the rendered element's border radius in pixels as a #gint.
 */
#define GTK_STYLE_PROPERTY_BORDER_RADIUS "border-radius"

/**
 * GTK_STYLE_PROPERTY_BORDER_STYLE:
 *
 * A property holding the element's border style as a #GtkBorderStyle.
 */
#define GTK_STYLE_PROPERTY_BORDER_STYLE "border-style"

/**
 * GTK_STYLE_PROPERTY_BORDER_COLOR:
 *
 * A property holding the element's border color as a #GdkRGBA.
 */
#define GTK_STYLE_PROPERTY_BORDER_COLOR "border-color"

/**
 * GTK_STYLE_PROPERTY_BACKGROUND_IMAGE:
 *
 * A property holding the element's background as a #cairo_pattern_t.
 */
#define GTK_STYLE_PROPERTY_BACKGROUND_IMAGE "background-image"


/* Predefined set of CSS classes */

/**
 * GTK_STYLE_CLASS_CELL:
 *
 * A CSS class to match content rendered in cell views.
 */
#define GTK_STYLE_CLASS_CELL "cell"

/**
 * GTK_STYLE_CLASS_ENTRY:
 *
 * A CSS class to match text entries.
 */
#define GTK_STYLE_CLASS_ENTRY "entry"

/**
 * GTK_STYLE_CLASS_BUTTON:
 *
 * A CSS class to match buttons.
 */
#define GTK_STYLE_CLASS_BUTTON "button"

/**
 * GTK_STYLE_CLASS_CALENDAR:
 *
 * A CSS class to match calendars.
 */
#define GTK_STYLE_CLASS_CALENDAR "calendar"

/**
 * GTK_STYLE_CLASS_SLIDER:
 *
 * A CSS class to match sliders.
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
 * A CSS class to match popup menus.
 */
#define GTK_STYLE_CLASS_MENU "menu"

/**
 * GTK_STYLE_CLASS_MENUBAR:
 *
 * A CSS class to menubars.
 */
#define GTK_STYLE_CLASS_MENUBAR "menubar"

/**
 * GTK_STYLE_CLASS_MENUITEM:
 *
 * A CSS class to match menu items.
 */
#define GTK_STYLE_CLASS_MENUITEM "menuitem"

/**
 * GTK_STYLE_CLASS_TOOLBAR:
 *
 * A CSS class to match toolbars.
 */
#define GTK_STYLE_CLASS_TOOLBAR "toolbar"

/**
 * GTK_STYLE_CLASS_RADIO:
 *
 * A CSS class to match radio buttons.
 */
#define GTK_STYLE_CLASS_RADIO "radio"

/**
 * GTK_STYLE_CLASS_CHECK:
 *
 * A CSS class to match check boxes.
 */
#define GTK_STYLE_CLASS_CHECK "check"

/**
 * GTK_STYLE_CLASS_DEFAULT:
 *
 * A CSS class to match the default widget.
 */
#define GTK_STYLE_CLASS_DEFAULT "default"

/**
 * GTK_STYLE_CLASS_TROUGH:
 *
 * A CSS class to match troughs, as in scrollbars and progressbars.
 */
#define GTK_STYLE_CLASS_TROUGH "trough"

/**
 * GTK_STYLE_CLASS_SCROLLBAR:
 *
 * A CSS class to match scrollbars.
 */
#define GTK_STYLE_CLASS_SCROLLBAR "scrollbar"

/**
 * GTK_STYLE_CLASS_SCALE:
 *
 * A CSS class to match scale widgets.
 */
#define GTK_STYLE_CLASS_SCALE "scale"

/**
 * GTK_STYLE_CLASS_HEADER:
 *
 * A CSS class to match a header element.
 */
#define GTK_STYLE_CLASS_HEADER "header"

/**
 * GTK_STYLE_CLASS_ACCELERATOR:
 *
 * A CSS class to match an accelerator.
 */
#define GTK_STYLE_CLASS_ACCELERATOR "accelerator"

/**
 * GTK_STYLE_CLASS_GRIP:
 *
 * A widget class defining a resize grip
 */
#define GTK_STYLE_CLASS_GRIP "grip"

/**
 * GTK_STYLE_CLASS_DOCK:
 *
 * A widget class defining a dock area
 */
#define GTK_STYLE_CLASS_DOCK "dock"

/**
 * GTK_STYLE_CLASS_PROGRESSBAR:
 *
 * A widget class defining a resize grip
 */
#define GTK_STYLE_CLASS_PROGRESSBAR "progressbar"

/**
 * GTK_STYLE_CLASS_SPINNER:
 *
 * A widget class defining a spinner
 */
#define GTK_STYLE_CLASS_SPINNER "spinner"

/**
 * GTK_STYLE_CLASS_MARK:
 *
 * A widget class defining marks in a widget, such as in scales
 */
#define GTK_STYLE_CLASS_MARK "mark"

/**
 * GTK_STYLE_CLASS_EXPANDER:
 *
 * A widget class defining an expander, such as those in treeviews
 */
#define GTK_STYLE_CLASS_EXPANDER "expander"

/**
 * GTK_STYLE_CLASS_SPINBUTTON:
 *
 * A widget class defining an spinbutton
 */
#define GTK_STYLE_CLASS_SPINBUTTON "spinbutton"

/**
 * GTK_STYLE_CLASS_NOTEBOOK:
 *
 * A widget class defining a notebook
 */
#define GTK_STYLE_CLASS_NOTEBOOK "notebook"

/**
 * GTK_STYLE_CLASS_VIEW:
 *
 * A widget class defining a view, such as iconviews or treeviews
 */
#define GTK_STYLE_CLASS_VIEW "view"

/**
 * GTK_STYLE_CLASS_HIGHLIGHT:
 *
 * A CSS class defining a highlighted area, such as headings in
 * assistants.
 */
#define GTK_STYLE_CLASS_HIGHLIGHT "highlight"

/**
 * GTK_STYLE_CLASS_FRAME:
 *
 * A CSS class defining a frame delimiting content, such as GtkFrame
 * or the scrolled window frame around the scrollable area.
 */
#define GTK_STYLE_CLASS_FRAME "frame"

/**
 * GTK_STYLE_CLASS_DND:
 *
 * A CSS class for a drag-and-drop indicator
 */
#define GTK_STYLE_CLASS_DND "dnd"

/**
 * GTK_STYLE_CLASS_PANE_SEPARATOR:
 *
 * A CSS class for a pane separator, such as those in #GtkPaned.
 */
#define GTK_STYLE_CLASS_PANE_SEPARATOR "pane-separator"

/**
 * GTK_STYLE_CLASS_INFO:
 *
 * A widget class for an area displaying an informational message,
 * such as those in infobars
 */
#define GTK_STYLE_CLASS_INFO "info"

/**
 * GTK_STYLE_CLASS_WARNING:
 *
 * A widget class for an area displaying a warning message,
 * such as those in infobars
 */
#define GTK_STYLE_CLASS_WARNING "warning"

/**
 * GTK_STYLE_CLASS_QUESTION:
 *
 * A widget class for an area displaying a question to the user,
 * such as those in infobars
 */
#define GTK_STYLE_CLASS_QUESTION "question"

/**
 * GTK_STYLE_CLASS_ERROR:
 *
 * A widget class for an area displaying an error message,
 * such as those in infobars
 */
#define GTK_STYLE_CLASS_ERROR "error"

/**
 * GTK_STYLE_CLASS_HORIZONTAL:
 *
 * A widget class for horizontally layered widgets.
 */
#define GTK_STYLE_CLASS_HORIZONTAL "horizontal"

/**
 * GTK_STYLE_CLASS_VERTICAL:
 *
 * A widget class for vertically layered widgets.
 */
#define GTK_STYLE_CLASS_VERTICAL "vertical"


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


GType gtk_style_context_get_type (void) G_GNUC_CONST;

GtkStyleContext * gtk_style_context_new (void);

void gtk_style_context_add_provider_for_screen    (GdkScreen        *screen,
                                                   GtkStyleProvider *provider,
                                                   guint             priority);
void gtk_style_context_remove_provider_for_screen (GdkScreen        *screen,
                                                   GtkStyleProvider *provider);

void gtk_style_context_add_provider    (GtkStyleContext  *context,
                                        GtkStyleProvider *provider,
                                        guint             priority);

void gtk_style_context_remove_provider (GtkStyleContext  *context,
                                        GtkStyleProvider *provider);

void gtk_style_context_save    (GtkStyleContext *context);
void gtk_style_context_restore (GtkStyleContext *context);

void gtk_style_context_get_property (GtkStyleContext *context,
                                     const gchar     *property,
                                     GtkStateFlags    state,
                                     GValue          *value);
void gtk_style_context_get_valist   (GtkStyleContext *context,
                                     GtkStateFlags    state,
                                     va_list          args);
void gtk_style_context_get          (GtkStyleContext *context,
                                     GtkStateFlags    state,
                                     ...) G_GNUC_NULL_TERMINATED;

void          gtk_style_context_set_state    (GtkStyleContext *context,
                                              GtkStateFlags    flags);
GtkStateFlags gtk_style_context_get_state    (GtkStyleContext *context);

gboolean      gtk_style_context_state_is_running (GtkStyleContext *context,
                                                  GtkStateType     state,
                                                  gdouble         *progress);

void          gtk_style_context_set_path     (GtkStyleContext *context,
                                              GtkWidgetPath   *path);
G_CONST_RETURN GtkWidgetPath * gtk_style_context_get_path (GtkStyleContext *context);

GList *  gtk_style_context_list_classes (GtkStyleContext *context);

void     gtk_style_context_add_class    (GtkStyleContext *context,
                                         const gchar     *class_name);
void     gtk_style_context_remove_class (GtkStyleContext *context,
                                         const gchar     *class_name);
gboolean gtk_style_context_has_class    (GtkStyleContext *context,
                                         const gchar     *class_name);

GList *  gtk_style_context_list_regions (GtkStyleContext *context);

void     gtk_style_context_add_region    (GtkStyleContext    *context,
                                          const gchar        *region_name,
                                          GtkRegionFlags      flags);
void     gtk_style_context_remove_region (GtkStyleContext    *context,
                                          const gchar        *region_name);
gboolean gtk_style_context_has_region    (GtkStyleContext    *context,
                                          const gchar        *region_name,
                                          GtkRegionFlags     *flags_return);

void gtk_style_context_get_style_property (GtkStyleContext *context,
                                           const gchar     *property_name,
                                           GValue          *value);
void gtk_style_context_get_style_valist   (GtkStyleContext *context,
                                           va_list          args);
void gtk_style_context_get_style          (GtkStyleContext *context,
                                           ...);

GtkIconSet * gtk_style_context_lookup_icon_set (GtkStyleContext *context,
                                                const gchar     *stock_id);
GdkPixbuf  * gtk_icon_set_render_icon_pixbuf   (GtkIconSet      *icon_set,
                                                GtkStyleContext *context,
                                                GtkIconSize      size);

void        gtk_style_context_set_screen (GtkStyleContext *context,
                                          GdkScreen       *screen);
GdkScreen * gtk_style_context_get_screen (GtkStyleContext *context);

void             gtk_style_context_set_direction (GtkStyleContext  *context,
                                                  GtkTextDirection  direction);
GtkTextDirection gtk_style_context_get_direction (GtkStyleContext  *context);

void             gtk_style_context_set_junction_sides (GtkStyleContext  *context,
                                                       GtkJunctionSides  sides);
GtkJunctionSides gtk_style_context_get_junction_sides (GtkStyleContext  *context);

gboolean gtk_style_context_lookup_color (GtkStyleContext *context,
                                         const gchar     *color_name,
                                         GdkRGBA         *color);

void  gtk_style_context_notify_state_change (GtkStyleContext *context,
                                             GdkWindow       *window,
                                             gpointer         region_id,
                                             GtkStateType     state,
                                             gboolean         state_value);
void  gtk_style_context_cancel_animations   (GtkStyleContext *context,
                                             gpointer         region_id);
void  gtk_style_context_scroll_animations   (GtkStyleContext *context,
                                             GdkWindow       *window,
                                             gint             dx,
                                             gint             dy);

void gtk_style_context_push_animatable_region (GtkStyleContext *context,
                                               gpointer         region_id);
void gtk_style_context_pop_animatable_region  (GtkStyleContext *context);

/* Some helper functions to retrieve most common properties */
void gtk_style_context_get_color            (GtkStyleContext *context,
                                             GtkStateFlags    state,
                                             GdkRGBA         *color);
void gtk_style_context_get_background_color (GtkStyleContext *context,
                                             GtkStateFlags    state,
                                             GdkRGBA         *color);
void gtk_style_context_get_border_color     (GtkStyleContext *context,
                                             GtkStateFlags    state,
                                             GdkRGBA         *color);
const PangoFontDescription *
     gtk_style_context_get_font             (GtkStyleContext *context,
                                             GtkStateFlags    state);
void gtk_style_context_get_border           (GtkStyleContext *context,
                                             GtkStateFlags    state,
                                             GtkBorder       *border);
void gtk_style_context_get_padding          (GtkStyleContext *context,
                                             GtkStateFlags    state,
                                             GtkBorder       *padding);
void gtk_style_context_get_margin           (GtkStyleContext *context,
                                             GtkStateFlags    state,
                                             GtkBorder       *margin);

void gtk_style_context_invalidate           (GtkStyleContext *context);
void gtk_style_context_reset_widgets        (GdkScreen       *screen);

void gtk_style_context_set_background       (GtkStyleContext *context,
                                             GdkWindow       *window);

/* Paint methods */
void        gtk_render_check       (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_render_option      (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_render_arrow       (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              angle,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              size);
void        gtk_render_background  (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_render_frame       (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_render_expander    (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_render_focus       (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_render_layout      (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    PangoLayout         *layout);
void        gtk_render_line        (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x0,
                                    gdouble              y0,
                                    gdouble              x1,
                                    gdouble              y1);
void        gtk_render_slider      (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height,
                                    GtkOrientation       orientation);
void        gtk_render_frame_gap   (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height,
                                    GtkPositionType      gap_side,
                                    gdouble              xy0_gap,
                                    gdouble              xy1_gap);
void        gtk_render_extension   (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height,
                                    GtkPositionType      gap_side);
void        gtk_render_handle      (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_render_activity    (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
GdkPixbuf * gtk_render_icon_pixbuf (GtkStyleContext     *context,
                                    const GtkIconSource *source,
                                    GtkIconSize          size);

G_END_DECLS

#endif /* __GTK_STYLE_CONTEXT_H__ */
