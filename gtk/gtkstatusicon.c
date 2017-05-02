/* gtkstatusicon.c:
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2005 Hans Breuer <hans@breuer.org>
 * Copyright (C) 2005 Novell, Inc.
 * Copyright (C) 2006 Imendio AB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 *	Hans Breuer <hans@breuer.org>
 *	Tor Lillqvist <tml@novell.com>
 *	Mikael Hallendal <micke@imendio.com>
 */

#include "config.h"
#include <string.h>

#include "gtkstatusicon.h"

#include "gtkintl.h"
#include "gtkiconfactory.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtktrayicon.h"

#include "gtkprivate.h"
#include "gtkwidget.h"
#include "gtktooltip.h"

#ifdef GDK_WINDOWING_X11
#include "gdk/x11/gdkx.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "gtkicontheme.h"
#include "gtklabel.h"

#include "win32/gdkwin32.h"
#define WM_GTK_TRAY_NOTIFICATION (WM_USER+1)
#endif

#ifdef GDK_WINDOWING_QUARTZ
#include "gtkicontheme.h"
#include "gtklabel.h"
#endif	

#include "gdkkeysyms.h"

#include "gtkalias.h"

#define BLINK_TIMEOUT 500

enum
{
  PROP_0,
  PROP_PIXBUF,
  PROP_FILE,
  PROP_STOCK,
  PROP_ICON_NAME,
  PROP_GICON,
  PROP_STORAGE_TYPE,
  PROP_SIZE,
  PROP_SCREEN,
  PROP_VISIBLE,
  PROP_ORIENTATION,
  PROP_EMBEDDED,
  PROP_BLINKING,
  PROP_HAS_TOOLTIP,
  PROP_TOOLTIP_TEXT,
  PROP_TOOLTIP_MARKUP,
  PROP_TITLE
};

enum 
{
  ACTIVATE_SIGNAL,
  POPUP_MENU_SIGNAL,
  SIZE_CHANGED_SIGNAL,
  BUTTON_PRESS_EVENT_SIGNAL,
  BUTTON_RELEASE_EVENT_SIGNAL,
  SCROLL_EVENT_SIGNAL,
  QUERY_TOOLTIP_SIGNAL,
  LAST_SIGNAL
};

static guint status_icon_signals [LAST_SIGNAL] = { 0 };

#ifdef GDK_WINDOWING_QUARTZ
#include "gtkstatusicon-quartz.c"
#endif

struct _GtkStatusIconPrivate
{
#ifdef GDK_WINDOWING_X11
  GtkWidget    *tray_icon;
  GtkWidget    *image;
#endif

#ifdef GDK_WINDOWING_WIN32
  GtkWidget     *dummy_widget;
  NOTIFYICONDATAW nid;
  gint          taskbar_top;
  gint		last_click_x, last_click_y;
  GtkOrientation orientation;
  gchar         *tooltip_text;
  gchar         *title;
#endif
	
#ifdef GDK_WINDOWING_QUARTZ
  GtkWidget     *dummy_widget;
  GtkQuartzStatusIcon *status_item;
  gchar         *tooltip_text;
  gchar         *title;
#endif

  gint          size;

  gint          image_width;
  gint          image_height;

  GtkImageType  storage_type;

  union
    {
      GdkPixbuf *pixbuf;
      gchar     *stock_id;
      gchar     *icon_name;
      GIcon     *gicon;
    } image_data;

  GdkPixbuf    *blank_icon;
  guint         blinking_timeout;

  guint         blinking : 1;
  guint         blink_off : 1;
  guint         visible : 1;
};

static GObject* gtk_status_icon_constructor      (GType                  type,
                                                  guint                  n_construct_properties,
                                                  GObjectConstructParam *construct_params);
static void     gtk_status_icon_finalize         (GObject        *object);
static void     gtk_status_icon_set_property     (GObject        *object,
						  guint           prop_id,
						  const GValue   *value,
						  GParamSpec     *pspec);
static void     gtk_status_icon_get_property     (GObject        *object,
						  guint           prop_id,
						  GValue         *value,
					          GParamSpec     *pspec);

#ifdef GDK_WINDOWING_X11
static void     gtk_status_icon_size_allocate    (GtkStatusIcon  *status_icon,
						  GtkAllocation  *allocation);
static void     gtk_status_icon_screen_changed   (GtkStatusIcon  *status_icon,
						  GdkScreen      *old_screen);
static void     gtk_status_icon_embedded_changed (GtkStatusIcon *status_icon);
static void     gtk_status_icon_orientation_changed (GtkStatusIcon *status_icon);
static gboolean gtk_status_icon_scroll           (GtkStatusIcon  *status_icon,
						  GdkEventScroll *event);
static gboolean gtk_status_icon_query_tooltip    (GtkStatusIcon *status_icon,
						  gint           x,
						  gint           y,
						  gboolean       keyboard_tip,
						  GtkTooltip    *tooltip);

static gboolean gtk_status_icon_key_press        (GtkStatusIcon  *status_icon,
						  GdkEventKey    *event);
static void     gtk_status_icon_popup_menu       (GtkStatusIcon  *status_icon);
#endif
static gboolean gtk_status_icon_button_press     (GtkStatusIcon  *status_icon,
						  GdkEventButton *event);
static gboolean gtk_status_icon_button_release   (GtkStatusIcon  *status_icon,
						  GdkEventButton *event);
static void     gtk_status_icon_disable_blinking (GtkStatusIcon  *status_icon);
static void     gtk_status_icon_reset_image_data (GtkStatusIcon  *status_icon);
static void     gtk_status_icon_update_image    (GtkStatusIcon *status_icon);

G_DEFINE_TYPE (GtkStatusIcon, gtk_status_icon, G_TYPE_OBJECT)

static void
gtk_status_icon_class_init (GtkStatusIconClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *) class;

  gobject_class->constructor  = gtk_status_icon_constructor;
  gobject_class->finalize     = gtk_status_icon_finalize;
  gobject_class->set_property = gtk_status_icon_set_property;
  gobject_class->get_property = gtk_status_icon_get_property;

  class->button_press_event   = NULL;
  class->button_release_event = NULL;
  class->scroll_event         = NULL;
  class->query_tooltip        = NULL;

  g_object_class_install_property (gobject_class,
				   PROP_PIXBUF,
				   g_param_spec_object ("pixbuf",
							P_("Pixbuf"),
							P_("A GdkPixbuf to display"),
							GDK_TYPE_PIXBUF,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_FILE,
				   g_param_spec_string ("file",
							P_("Filename"),
							P_("Filename to load and display"),
							NULL,
							GTK_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
				   PROP_STOCK,
				   g_param_spec_string ("stock",
							P_("Stock ID"),
							P_("Stock ID for a stock image to display"),
							NULL,
							GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        P_("Icon Name"),
                                                        P_("The name of the icon from the icon theme"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkStatusIcon:gicon:
   *
   * The #GIcon displayed in the #GtkStatusIcon. For themed icons,
   * the image will be updated automatically if the theme changes.
   *
   * Since: 2.14
   */
  g_object_class_install_property (gobject_class,
                                   PROP_GICON,
                                   g_param_spec_object ("gicon",
                                                        P_("GIcon"),
                                                        P_("The GIcon being displayed"),
                                                        G_TYPE_ICON,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_STORAGE_TYPE,
				   g_param_spec_enum ("storage-type",
						      P_("Storage type"),
						      P_("The representation being used for image data"),
						      GTK_TYPE_IMAGE_TYPE,
						      GTK_IMAGE_EMPTY,
						      GTK_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
				   PROP_SIZE,
				   g_param_spec_int ("size",
						     P_("Size"),
						     P_("The size of the icon"),
						     0,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
				   PROP_SCREEN,
				   g_param_spec_object ("screen",
 							P_("Screen"),
 							P_("The screen where this status icon will be displayed"),
							GDK_TYPE_SCREEN,
 							GTK_PARAM_READWRITE));

  /**
   * GtkStatusIcon:blinking:
   *
   * Whether or not the status icon is blinking.
   *
   * Deprecated: 2.22: This property will be removed in GTK+ 3
   */
  g_object_class_install_property (gobject_class,
				   PROP_BLINKING,
				   g_param_spec_boolean ("blinking",
							 P_("Blinking"),
							 P_("Whether or not the status icon is blinking"),
							 FALSE,
							 GTK_PARAM_READWRITE | G_PARAM_DEPRECATED));

  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE,
				   g_param_spec_boolean ("visible",
							 P_("Visible"),
							 P_("Whether or not the status icon is visible"),
							 TRUE,
							 GTK_PARAM_READWRITE));


  /**
   * GtkStatusIcon:embedded: 
   *
   * %TRUE if the statusicon is embedded in a notification area.
   *
   * Since: 2.12
   */
  g_object_class_install_property (gobject_class,
				   PROP_EMBEDDED,
				   g_param_spec_boolean ("embedded",
							 P_("Embedded"),
							 P_("Whether or not the status icon is embedded"),
							 FALSE,
							 GTK_PARAM_READABLE));

  /**
   * GtkStatusIcon:orientation:
   *
   * The orientation of the tray in which the statusicon 
   * is embedded. 
   *
   * Since: 2.12
   */
  g_object_class_install_property (gobject_class,
				   PROP_ORIENTATION,
				   g_param_spec_enum ("orientation",
						      P_("Orientation"),
						      P_("The orientation of the tray"),
						      GTK_TYPE_ORIENTATION,
						      GTK_ORIENTATION_HORIZONTAL,
						      GTK_PARAM_READABLE));

/**
 * GtkStatusIcon:has-tooltip:
 *
 * Enables or disables the emission of #GtkStatusIcon::query-tooltip on
 * @status_icon.  A value of %TRUE indicates that @status_icon can have a
 * tooltip, in this case the status icon will be queried using
 * #GtkStatusIcon::query-tooltip to determine whether it will provide a
 * tooltip or not.
 *
 * Note that setting this property to %TRUE for the first time will change
 * the event masks of the windows of this status icon to include leave-notify
 * and motion-notify events. This will not be undone when the property is set
 * to %FALSE again.
 *
 * Whether this property is respected is platform dependent.
 * For plain text tooltips, use #GtkStatusIcon:tooltip-text in preference.
 *
 * Since: 2.16
 */
  g_object_class_install_property (gobject_class,
				   PROP_HAS_TOOLTIP,
				   g_param_spec_boolean ("has-tooltip",
 							 P_("Has tooltip"),
 							 P_("Whether this tray icon has a tooltip"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  /**
   * GtkStatusIcon:tooltip-text:
   *
   * Sets the text of tooltip to be the given string.
   *
   * Also see gtk_tooltip_set_text().
   *
   * This is a convenience property which will take care of getting the
   * tooltip shown if the given string is not %NULL.
   * #GtkStatusIcon:has-tooltip will automatically be set to %TRUE and
   * the default handler for the #GtkStatusIcon::query-tooltip signal
   * will take care of displaying the tooltip.
   *
   * Note that some platforms have limitations on the length of tooltips
   * that they allow on status icons, e.g. Windows only shows the first
   * 64 characters.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TOOLTIP_TEXT,
                                   g_param_spec_string ("tooltip-text",
                                                        P_("Tooltip Text"),
                                                        P_("The contents of the tooltip for this widget"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  /**
   * GtkStatusIcon:tooltip-markup:
   *
   * Sets the text of tooltip to be the given string, which is marked up
   * with the <link linkend="PangoMarkupFormat">Pango text markup 
   * language</link>. Also see gtk_tooltip_set_markup().
   *
   * This is a convenience property which will take care of getting the
   * tooltip shown if the given string is not %NULL.
   * #GtkStatusIcon:has-tooltip will automatically be set to %TRUE and
   * the default handler for the #GtkStatusIcon::query-tooltip signal
   * will take care of displaying the tooltip.
   *
   * On some platforms, embedded markup will be ignored.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
				   PROP_TOOLTIP_MARKUP,
				   g_param_spec_string ("tooltip-markup",
 							P_("Tooltip markup"),
							P_("The contents of the tooltip for this tray icon"),
							NULL,
							GTK_PARAM_READWRITE));


  /**
   * GtkStatusIcon:title:
   *
   * The title of this tray icon. This should be a short, human-readable,
   * localized string describing the tray icon. It may be used by tools
   * like screen readers to render the tray icon.
   *
   * Since: 2.18
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        P_("Title"),
                                                        P_("The title of this tray icon"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkStatusIcon::activate:
   * @status_icon: the object which received the signal
   *
   * Gets emitted when the user activates the status icon. 
   * If and how status icons can activated is platform-dependent.
   *
   * Unlike most G_SIGNAL_ACTION signals, this signal is meant to 
   * be used by applications and should be wrapped by language bindings.
   *
   * Since: 2.10
   */
  status_icon_signals [ACTIVATE_SIGNAL] =
    g_signal_new (I_("activate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkStatusIconClass, activate),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);

  /**
   * GtkStatusIcon::popup-menu:
   * @status_icon: the object which received the signal
   * @button: the button that was pressed, or 0 if the 
   *   signal is not emitted in response to a button press event
   * @activate_time: the timestamp of the event that
   *   triggered the signal emission
   *
   * Gets emitted when the user brings up the context menu
   * of the status icon. Whether status icons can have context 
   * menus and how these are activated is platform-dependent.
   *
   * The @button and @activate_time parameters should be 
   * passed as the last to arguments to gtk_menu_popup().
   *
   * Unlike most G_SIGNAL_ACTION signals, this signal is meant to 
   * be used by applications and should be wrapped by language bindings.
   *
   * Since: 2.10
   */
  status_icon_signals [POPUP_MENU_SIGNAL] =
    g_signal_new (I_("popup-menu"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkStatusIconClass, popup_menu),
		  NULL,
		  NULL,
		  _gtk_marshal_VOID__UINT_UINT,
		  G_TYPE_NONE,
		  2,
		  G_TYPE_UINT,
		  G_TYPE_UINT);

  /**
   * GtkStatusIcon::size-changed:
   * @status_icon: the object which received the signal
   * @size: the new size
   *
   * Gets emitted when the size available for the image
   * changes, e.g. because the notification area got resized.
   *
   * Return value: %TRUE if the icon was updated for the new
   * size. Otherwise, GTK+ will scale the icon as necessary.
   *
   * Since: 2.10
   */
  status_icon_signals [SIZE_CHANGED_SIGNAL] =
    g_signal_new (I_("size-changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkStatusIconClass, size_changed),
		  g_signal_accumulator_true_handled,
		  NULL,
		  _gtk_marshal_BOOLEAN__INT,
		  G_TYPE_BOOLEAN,
		  1,
		  G_TYPE_INT);

  /**
   * GtkStatusIcon::button-press-event:
   * @status_icon: the object which received the signal
   * @event: the #GdkEventButton which triggered this signal
   *
   * The ::button-press-event signal will be emitted when a button
   * (typically from a mouse) is pressed.
   *
   * Whether this event is emitted is platform-dependent.  Use the ::activate
   * and ::popup-menu signals in preference.
   *
   * Return value: %TRUE to stop other handlers from being invoked
   * for the event. %FALSE to propagate the event further.
   *
   * Since: 2.14
   */
  status_icon_signals [BUTTON_PRESS_EVENT_SIGNAL] =
    g_signal_new (I_("button_press_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkStatusIconClass, button_press_event),
		  g_signal_accumulator_true_handled, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkStatusIcon::button-release-event:
   * @status_icon: the object which received the signal
   * @event: the #GdkEventButton which triggered this signal
   *
   * The ::button-release-event signal will be emitted when a button
   * (typically from a mouse) is released.
   *
   * Whether this event is emitted is platform-dependent.  Use the ::activate
   * and ::popup-menu signals in preference.
   *
   * Return value: %TRUE to stop other handlers from being invoked
   * for the event. %FALSE to propagate the event further.
   *
   * Since: 2.14
   */
  status_icon_signals [BUTTON_RELEASE_EVENT_SIGNAL] =
    g_signal_new (I_("button_release_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkStatusIconClass, button_release_event),
		  g_signal_accumulator_true_handled, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkStatusIcon::scroll-event:
   * @status_icon: the object which received the signal.
   * @event: the #GdkEventScroll which triggered this signal
   *
   * The ::scroll-event signal is emitted when a button in the 4 to 7
   * range is pressed. Wheel mice are usually configured to generate 
   * button press events for buttons 4 and 5 when the wheel is turned.
   *
   * Whether this event is emitted is platform-dependent.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event. 
   *   %FALSE to propagate the event further.
   */
  status_icon_signals[SCROLL_EVENT_SIGNAL] =
    g_signal_new (I_("scroll_event"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkStatusIconClass, scroll_event),
		  g_signal_accumulator_true_handled, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkStatusIcon::query-tooltip:
   * @status_icon: the object which received the signal
   * @x: the x coordinate of the cursor position where the request has been
   *     emitted, relative to @status_icon
   * @y: the y coordinate of the cursor position where the request has been
   *     emitted, relative to @status_icon
   * @keyboard_mode: %TRUE if the tooltip was trigged using the keyboard
   * @tooltip: a #GtkTooltip
   *
   * Emitted when the #GtkSettings:gtk-tooltip-timeout has expired with the
   * cursor hovering above @status_icon; or emitted when @status_icon got
   * focus in keyboard mode.
   *
   * Using the given coordinates, the signal handler should determine
   * whether a tooltip should be shown for @status_icon. If this is
   * the case %TRUE should be returned, %FALSE otherwise. Note that if
   * @keyboard_mode is %TRUE, the values of @x and @y are undefined and
   * should not be used.
   *
   * The signal handler is free to manipulate @tooltip with the therefore
   * destined function calls.
   *
   * Whether this signal is emitted is platform-dependent.
   * For plain text tooltips, use #GtkStatusIcon:tooltip-text in preference.
   *
   * Returns: %TRUE if @tooltip should be shown right now, %FALSE otherwise.
   *
   * Since: 2.16
   */
  status_icon_signals [QUERY_TOOLTIP_SIGNAL] =
    g_signal_new (I_("query_tooltip"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkStatusIconClass, query_tooltip),
		  g_signal_accumulator_true_handled, NULL,
		  _gtk_marshal_BOOLEAN__INT_INT_BOOLEAN_OBJECT,
		  G_TYPE_BOOLEAN, 4,
		  G_TYPE_INT,
		  G_TYPE_INT,
		  G_TYPE_BOOLEAN,
		  GTK_TYPE_TOOLTIP);

  g_type_class_add_private (class, sizeof (GtkStatusIconPrivate));
}

#ifdef GDK_WINDOWING_WIN32

static void
build_button_event (GtkStatusIconPrivate *priv,
		    GdkEventButton       *e,
		    guint                 button)
{
  POINT pos;
  GdkRectangle monitor0;

  /* We know that gdk/win32 puts the primary monitor at index 0 */
  gdk_screen_get_monitor_geometry (gdk_screen_get_default (), 0, &monitor0);
  e->window = g_object_ref (gdk_get_default_root_window ());
  e->send_event = TRUE;
  e->time = GetTickCount ();
  GetCursorPos (&pos);
  priv->last_click_x = e->x = pos.x + monitor0.x;
  priv->last_click_y = e->y = pos.y + monitor0.y;
  e->axes = NULL;
  e->state = 0;
  e->button = button;
  e->device = gdk_display_get_default ()->core_pointer;
  e->x_root = e->x;
  e->y_root = e->y;
}

typedef struct
{
  GtkStatusIcon *status_icon;
  GdkEventButton *event;
} ButtonCallbackData;

static gboolean
button_callback (gpointer data)
{
  ButtonCallbackData *bc = (ButtonCallbackData *) data;

  if (bc->event->type == GDK_BUTTON_PRESS)
    gtk_status_icon_button_press (bc->status_icon, bc->event);
  else
    gtk_status_icon_button_release (bc->status_icon, bc->event);

  gdk_event_free ((GdkEvent *) bc->event);
  g_free (data);

  return FALSE;
}

static UINT taskbar_created_msg = 0;
static GSList *status_icons = NULL;
static UINT status_icon_id = 0;

static GtkStatusIcon *
find_status_icon (UINT id)
{
  GSList *rover;

  for (rover = status_icons; rover != NULL; rover = rover->next)
    {
      GtkStatusIcon *status_icon = GTK_STATUS_ICON (rover->data);
      GtkStatusIconPrivate *priv = status_icon->priv;

      if (priv->nid.uID == id)
        return status_icon;
    }

  return NULL;
}

static LRESULT CALLBACK
wndproc (HWND   hwnd,
	 UINT   message,
	 WPARAM wparam,
	 LPARAM lparam)
{
  if (message == taskbar_created_msg)
    {
      GSList *rover;

      for (rover = status_icons; rover != NULL; rover = rover->next)
	{
	  GtkStatusIcon *status_icon = GTK_STATUS_ICON (rover->data);
	  GtkStatusIconPrivate *priv = status_icon->priv;

	  /* taskbar_created_msg is also fired when DPI changes. Try to delete existing icons if possible. */
	  if (!Shell_NotifyIconW (NIM_DELETE, &priv->nid))
	  {
		g_warning (G_STRLOC ": Shell_NotifyIcon(NIM_DELETE) on existing icon failed");
	  }

	  priv->nid.hWnd = hwnd;
	  priv->nid.uID = status_icon_id++;
	  priv->nid.uCallbackMessage = WM_GTK_TRAY_NOTIFICATION;
	  priv->nid.uFlags = NIF_MESSAGE;

	  if (!Shell_NotifyIconW (NIM_ADD, &priv->nid))
	    {
	      g_warning (G_STRLOC ": Shell_NotifyIcon(NIM_ADD) failed");
	      priv->nid.hWnd = NULL;
	      continue;
	    }

	  gtk_status_icon_update_image (status_icon);
	}
      return 0;
    }

  if (message == WM_GTK_TRAY_NOTIFICATION)
    {
      ButtonCallbackData *bc;
      guint button;
      
      switch (lparam)
	{
	case WM_LBUTTONDOWN:
	  button = 1;
	  goto buttondown0;

	case WM_MBUTTONDOWN:
	  button = 2;
	  goto buttondown0;

	case WM_RBUTTONDOWN:
	  button = 3;
	  goto buttondown0;

	case WM_XBUTTONDOWN:
	  if (HIWORD (wparam) == XBUTTON1)
	    button = 4;
	  else
	    button = 5;

	buttondown0:
	  bc = g_new (ButtonCallbackData, 1);
	  bc->event = (GdkEventButton *) gdk_event_new (GDK_BUTTON_PRESS);
	  bc->status_icon = find_status_icon (wparam);
	  build_button_event (bc->status_icon->priv, bc->event, button);
	  g_idle_add (button_callback, bc);
	  break;

	case WM_LBUTTONUP:
	  button = 1;
	  goto buttonup0;

	case WM_MBUTTONUP:
	  button = 2;
	  goto buttonup0;

	case WM_RBUTTONUP:
	  button = 3;
	  goto buttonup0;

	case WM_XBUTTONUP:
	  if (HIWORD (wparam) == XBUTTON1)
	    button = 4;
	  else
	    button = 5;

	buttonup0:
	  bc = g_new (ButtonCallbackData, 1);
	  bc->event = (GdkEventButton *) gdk_event_new (GDK_BUTTON_RELEASE);
	  bc->status_icon = find_status_icon (wparam);
	  build_button_event (bc->status_icon->priv, bc->event, button);
	  g_idle_add (button_callback, bc);
	  break;

	default :
	  break;
	}
	return 0;
    }
  else
    {
      return DefWindowProc (hwnd, message, wparam, lparam);
    }
}

static HWND
create_tray_observer (void)
{
  WNDCLASS    wclass;
  static HWND hwnd = NULL;
  ATOM        klass;
  HINSTANCE   hmodule = GetModuleHandle (NULL);

  if (hwnd)
    return hwnd;

  taskbar_created_msg = RegisterWindowMessage("TaskbarCreated");

  memset (&wclass, 0, sizeof(WNDCLASS));
  wclass.lpszClassName = "gtkstatusicon-observer";
  wclass.lpfnWndProc   = wndproc;
  wclass.hInstance     = hmodule;

  klass = RegisterClass (&wclass);
  if (!klass)
    return NULL;

  hwnd = CreateWindow (MAKEINTRESOURCE (klass),
                       NULL, WS_POPUP,
                       0, 0, 1, 1, NULL, NULL,
                       hmodule, NULL);
  if (!hwnd)
    {
      UnregisterClass (MAKEINTRESOURCE(klass), hmodule);
      return NULL;
    }

  return hwnd;
}

#endif

static void
gtk_status_icon_init (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (status_icon, GTK_TYPE_STATUS_ICON,
				      GtkStatusIconPrivate);
  status_icon->priv = priv;
  
  priv->storage_type = GTK_IMAGE_EMPTY;
  priv->visible      = TRUE;

#ifdef GDK_WINDOWING_X11
  priv->size         = 0;
  priv->image_width  = 0;
  priv->image_height = 0;

  priv->tray_icon = GTK_WIDGET (_gtk_tray_icon_new (NULL));

  gtk_widget_add_events (GTK_WIDGET (priv->tray_icon),
			 GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			 GDK_SCROLL_MASK);

  g_signal_connect_swapped (priv->tray_icon, "key-press-event",
			    G_CALLBACK (gtk_status_icon_key_press), status_icon);
  g_signal_connect_swapped (priv->tray_icon, "popup-menu",
			    G_CALLBACK (gtk_status_icon_popup_menu), status_icon);
  g_signal_connect_swapped (priv->tray_icon, "notify::embedded",
			    G_CALLBACK (gtk_status_icon_embedded_changed), status_icon);
  g_signal_connect_swapped (priv->tray_icon, "notify::orientation",
			    G_CALLBACK (gtk_status_icon_orientation_changed), status_icon);
  g_signal_connect_swapped (priv->tray_icon, "button-press-event",
			    G_CALLBACK (gtk_status_icon_button_press), status_icon);
  g_signal_connect_swapped (priv->tray_icon, "button-release-event",
			    G_CALLBACK (gtk_status_icon_button_release), status_icon);
  g_signal_connect_swapped (priv->tray_icon, "scroll-event",
			    G_CALLBACK (gtk_status_icon_scroll), status_icon);
  g_signal_connect_swapped (priv->tray_icon, "query-tooltip",
			    G_CALLBACK (gtk_status_icon_query_tooltip), status_icon);
  g_signal_connect_swapped (priv->tray_icon, "screen-changed",
		    	    G_CALLBACK (gtk_status_icon_screen_changed), status_icon);
  priv->image = gtk_image_new ();
  gtk_widget_set_can_focus (priv->image, TRUE);
  gtk_container_add (GTK_CONTAINER (priv->tray_icon), priv->image);
  gtk_widget_show (priv->image);

  g_signal_connect_swapped (priv->image, "size-allocate",
			    G_CALLBACK (gtk_status_icon_size_allocate), status_icon);

#endif

#ifdef GDK_WINDOWING_WIN32

  /* Get position and orientation of Windows taskbar. */
  {
    APPBARDATA abd;
    
    abd.cbSize = sizeof (abd);
    SHAppBarMessage (ABM_GETTASKBARPOS, &abd);
    if (abd.rc.bottom - abd.rc.top > abd.rc.right - abd.rc.left)
      priv->orientation = GTK_ORIENTATION_VERTICAL;
    else
      priv->orientation = GTK_ORIENTATION_HORIZONTAL;

    priv->taskbar_top = abd.rc.top;
  }

  priv->last_click_x = priv->last_click_y = 0;

  /* Are the system tray icons always 16 pixels square? */
  priv->size         = 16;
  priv->image_width  = 16;
  priv->image_height = 16;

  priv->dummy_widget = gtk_label_new ("");

  memset (&priv->nid, 0, sizeof (priv->nid));

  priv->nid.hWnd = create_tray_observer ();
  priv->nid.uID = status_icon_id++;
  priv->nid.uCallbackMessage = WM_GTK_TRAY_NOTIFICATION;
  priv->nid.uFlags = NIF_MESSAGE;

  /* To help win7 identify the icon create it with an application "unique" tip */
  if (g_get_prgname ())
  {
    WCHAR *wcs = g_utf8_to_utf16 (g_get_prgname (), -1, NULL, NULL, NULL);

    priv->nid.uFlags |= NIF_TIP;
    wcsncpy (priv->nid.szTip, wcs, G_N_ELEMENTS (priv->nid.szTip) - 1);
    priv->nid.szTip[G_N_ELEMENTS (priv->nid.szTip) - 1] = 0;
    g_free (wcs);
  }

  if (!Shell_NotifyIconW (NIM_ADD, &priv->nid))
    {
      g_warning (G_STRLOC ": Shell_NotifyIcon(NIM_ADD) failed");
      priv->nid.hWnd = NULL;
    }

  status_icons = g_slist_append (status_icons, status_icon);

#endif
	
#ifdef GDK_WINDOWING_QUARTZ
  priv->dummy_widget = gtk_label_new ("");

  QUARTZ_POOL_ALLOC;

  priv->status_item = [[GtkQuartzStatusIcon alloc] initWithStatusIcon:status_icon];

  priv->image_width = priv->image_height = [priv->status_item getHeight];
  priv->size = priv->image_height;

  QUARTZ_POOL_RELEASE;

#endif 
}

static GObject*
gtk_status_icon_constructor (GType                  type,
                             guint                  n_construct_properties,
                             GObjectConstructParam *construct_params)
{
  GObject *object;
#ifdef GDK_WINDOWING_X11
  GtkStatusIcon *status_icon;
  GtkStatusIconPrivate *priv;
#endif

  object = G_OBJECT_CLASS (gtk_status_icon_parent_class)->constructor (type,
                                                                       n_construct_properties,
                                                                       construct_params);

#ifdef GDK_WINDOWING_X11
  status_icon = GTK_STATUS_ICON (object);
  priv = status_icon->priv;
  
  if (priv->visible)
    gtk_widget_show (priv->tray_icon);
#endif

  return object;
}

static void
gtk_status_icon_finalize (GObject *object)
{
  GtkStatusIcon *status_icon = GTK_STATUS_ICON (object);
  GtkStatusIconPrivate *priv = status_icon->priv;

  gtk_status_icon_disable_blinking (status_icon);
  
  gtk_status_icon_reset_image_data (status_icon);

  if (priv->blank_icon)
    g_object_unref (priv->blank_icon);
  priv->blank_icon = NULL;

#ifdef GDK_WINDOWING_X11
  g_signal_handlers_disconnect_by_func (priv->tray_icon,
			                gtk_status_icon_key_press, status_icon);
  g_signal_handlers_disconnect_by_func (priv->tray_icon,
			                gtk_status_icon_popup_menu, status_icon);
  g_signal_handlers_disconnect_by_func (priv->tray_icon,
			                gtk_status_icon_embedded_changed, status_icon);
  g_signal_handlers_disconnect_by_func (priv->tray_icon,
			                gtk_status_icon_orientation_changed, status_icon);
  g_signal_handlers_disconnect_by_func (priv->tray_icon,
			                gtk_status_icon_button_press, status_icon);
  g_signal_handlers_disconnect_by_func (priv->tray_icon,
			                gtk_status_icon_button_release, status_icon);
  g_signal_handlers_disconnect_by_func (priv->tray_icon,
			                gtk_status_icon_scroll, status_icon);
  g_signal_handlers_disconnect_by_func (priv->tray_icon,
			                gtk_status_icon_query_tooltip, status_icon);
  g_signal_handlers_disconnect_by_func (priv->tray_icon,
		    	                gtk_status_icon_screen_changed, status_icon);
  gtk_widget_destroy (priv->image);
  gtk_widget_destroy (priv->tray_icon);
#endif

#ifdef GDK_WINDOWING_WIN32
  if (priv->nid.hWnd != NULL && priv->visible)
    Shell_NotifyIconW (NIM_DELETE, &priv->nid);
  if (priv->nid.hIcon)
    DestroyIcon (priv->nid.hIcon);
  g_free (priv->tooltip_text);

  gtk_widget_destroy (priv->dummy_widget);

  status_icons = g_slist_remove (status_icons, status_icon);
#endif
	
#ifdef GDK_WINDOWING_QUARTZ
  QUARTZ_POOL_ALLOC;
  [priv->status_item release];
  QUARTZ_POOL_RELEASE;
  g_free (priv->tooltip_text);
#endif

  G_OBJECT_CLASS (gtk_status_icon_parent_class)->finalize (object);
}

static void
gtk_status_icon_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GtkStatusIcon *status_icon = GTK_STATUS_ICON (object);

  switch (prop_id)
    {
    case PROP_PIXBUF:
      gtk_status_icon_set_from_pixbuf (status_icon, g_value_get_object (value));
      break;
    case PROP_FILE:
      gtk_status_icon_set_from_file (status_icon, g_value_get_string (value));
      break;
    case PROP_STOCK:
      gtk_status_icon_set_from_stock (status_icon, g_value_get_string (value));
      break;
    case PROP_ICON_NAME:
      gtk_status_icon_set_from_icon_name (status_icon, g_value_get_string (value));
      break;
    case PROP_GICON:
      gtk_status_icon_set_from_gicon (status_icon, g_value_get_object (value));
      break;
    case PROP_SCREEN:
      gtk_status_icon_set_screen (status_icon, g_value_get_object (value));
      break;
    case PROP_BLINKING:
      gtk_status_icon_set_blinking (status_icon, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE:
      gtk_status_icon_set_visible (status_icon, g_value_get_boolean (value));
      break;
    case PROP_HAS_TOOLTIP:
      gtk_status_icon_set_has_tooltip (status_icon, g_value_get_boolean (value));
      break;
    case PROP_TOOLTIP_TEXT:
      gtk_status_icon_set_tooltip_text (status_icon, g_value_get_string (value));
      break;
    case PROP_TOOLTIP_MARKUP:
      gtk_status_icon_set_tooltip_markup (status_icon, g_value_get_string (value));
      break;
    case PROP_TITLE:
      gtk_status_icon_set_title (status_icon, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_status_icon_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  GtkStatusIcon *status_icon = GTK_STATUS_ICON (object);
  GtkStatusIconPrivate *priv = status_icon->priv;

  /* The "getter" functions whine if you try to get the wrong
   * storage type. This function is instead robust against that,
   * so that GUI builders don't have to jump through hoops
   * to avoid g_warning
   */

  switch (prop_id)
    {
    case PROP_PIXBUF:
      if (priv->storage_type != GTK_IMAGE_PIXBUF)
	g_value_set_object (value, NULL);
      else
	g_value_set_object (value, gtk_status_icon_get_pixbuf (status_icon));
      break;
    case PROP_STOCK:
      if (priv->storage_type != GTK_IMAGE_STOCK)
	g_value_set_string (value, NULL);
      else
	g_value_set_string (value, gtk_status_icon_get_stock (status_icon));
      break;
    case PROP_ICON_NAME:
      if (priv->storage_type != GTK_IMAGE_ICON_NAME)
	g_value_set_string (value, NULL);
      else
	g_value_set_string (value, gtk_status_icon_get_icon_name (status_icon));
      break;
    case PROP_GICON:
      if (priv->storage_type != GTK_IMAGE_GICON)
        g_value_set_object (value, NULL);
      else
        g_value_set_object (value, gtk_status_icon_get_gicon (status_icon));
      break;
    case PROP_STORAGE_TYPE:
      g_value_set_enum (value, gtk_status_icon_get_storage_type (status_icon));
      break;
    case PROP_SIZE:
      g_value_set_int (value, gtk_status_icon_get_size (status_icon));
      break;
    case PROP_SCREEN:
      g_value_set_object (value, gtk_status_icon_get_screen (status_icon));
      break;
    case PROP_BLINKING:
      g_value_set_boolean (value, gtk_status_icon_get_blinking (status_icon));
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, gtk_status_icon_get_visible (status_icon));
      break;
    case PROP_EMBEDDED:
      g_value_set_boolean (value, gtk_status_icon_is_embedded (status_icon));
      break;
    case PROP_ORIENTATION:
#ifdef GDK_WINDOWING_X11
      g_value_set_enum (value, _gtk_tray_icon_get_orientation (GTK_TRAY_ICON (status_icon->priv->tray_icon)));
#endif
#ifdef GDK_WINDOWING_WIN32
      g_value_set_enum (value, status_icon->priv->orientation);
#endif
      break;
    case PROP_HAS_TOOLTIP:
      g_value_set_boolean (value, gtk_status_icon_get_has_tooltip (status_icon));
      break;
    case PROP_TOOLTIP_TEXT:
      g_value_set_string (value, gtk_status_icon_get_tooltip_text (status_icon));
      break;
    case PROP_TOOLTIP_MARKUP:
      g_value_set_string (value, gtk_status_icon_get_tooltip_markup (status_icon));
      break;
    case PROP_TITLE:
      g_value_set_string (value, gtk_status_icon_get_title (status_icon));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_status_icon_new:
 * 
 * Creates an empty status icon object.
 * 
 * Return value: a new #GtkStatusIcon
 *
 * Since: 2.10
 **/
GtkStatusIcon *
gtk_status_icon_new (void)
{
  return g_object_new (GTK_TYPE_STATUS_ICON, NULL);
}

/**
 * gtk_status_icon_new_from_pixbuf:
 * @pixbuf: a #GdkPixbuf
 * 
 * Creates a status icon displaying @pixbuf. 
 *
 * The image will be scaled down to fit in the available 
 * space in the notification area, if necessary.
 * 
 * Return value: a new #GtkStatusIcon
 *
 * Since: 2.10
 **/
GtkStatusIcon *
gtk_status_icon_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  return g_object_new (GTK_TYPE_STATUS_ICON,
		       "pixbuf", pixbuf,
		       NULL);
}

/**
 * gtk_status_icon_new_from_file:
 * @filename: a filename
 * 
 * Creates a status icon displaying the file @filename. 
 *
 * The image will be scaled down to fit in the available 
 * space in the notification area, if necessary.
 * 
 * Return value: a new #GtkStatusIcon
 *
 * Since: 2.10
 **/
GtkStatusIcon *
gtk_status_icon_new_from_file (const gchar *filename)
{
  return g_object_new (GTK_TYPE_STATUS_ICON,
		       "file", filename,
		       NULL);
}

/**
 * gtk_status_icon_new_from_stock:
 * @stock_id: a stock icon id
 * 
 * Creates a status icon displaying a stock icon. Sample stock icon
 * names are #GTK_STOCK_OPEN, #GTK_STOCK_QUIT. You can register your 
 * own stock icon names, see gtk_icon_factory_add_default() and 
 * gtk_icon_factory_add(). 
 *
 * Return value: a new #GtkStatusIcon
 *
 * Since: 2.10
 **/
GtkStatusIcon *
gtk_status_icon_new_from_stock (const gchar *stock_id)
{
  return g_object_new (GTK_TYPE_STATUS_ICON,
		       "stock", stock_id,
		       NULL);
}

/**
 * gtk_status_icon_new_from_icon_name:
 * @icon_name: an icon name
 * 
 * Creates a status icon displaying an icon from the current icon theme.
 * If the current icon theme is changed, the icon will be updated 
 * appropriately.
 * 
 * Return value: a new #GtkStatusIcon
 *
 * Since: 2.10
 **/
GtkStatusIcon *
gtk_status_icon_new_from_icon_name (const gchar *icon_name)
{
  return g_object_new (GTK_TYPE_STATUS_ICON,
		       "icon-name", icon_name,
		       NULL);
}

/**
 * gtk_status_icon_new_from_gicon:
 * @icon: a #GIcon
 *
 * Creates a status icon displaying a #GIcon. If the icon is a
 * themed icon, it will be updated when the theme changes.
 *
 * Return value: a new #GtkStatusIcon
 *
 * Since: 2.14
 **/
GtkStatusIcon *
gtk_status_icon_new_from_gicon (GIcon *icon)
{
  return g_object_new (GTK_TYPE_STATUS_ICON,
		       "gicon", icon,
		       NULL);
}

static void
emit_activate_signal (GtkStatusIcon *status_icon)
{
  g_signal_emit (status_icon,
		 status_icon_signals [ACTIVATE_SIGNAL], 0);
}

static void
emit_popup_menu_signal (GtkStatusIcon *status_icon,
			guint          button,
			guint32        activate_time)
{
  g_signal_emit (status_icon,
		 status_icon_signals [POPUP_MENU_SIGNAL], 0,
		 button,
		 activate_time);
}

#ifdef GDK_WINDOWING_X11

static gboolean
emit_size_changed_signal (GtkStatusIcon *status_icon,
			  gint           size)
{
  gboolean handled = FALSE;
  
  g_signal_emit (status_icon,
		 status_icon_signals [SIZE_CHANGED_SIGNAL], 0,
		 size,
		 &handled);

  return handled;
}

#endif

static GdkPixbuf *
gtk_status_icon_blank_icon (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv = status_icon->priv;

  if (priv->blank_icon)
    {
      gint width, height;

      width  = gdk_pixbuf_get_width (priv->blank_icon);
      height = gdk_pixbuf_get_height (priv->blank_icon);


      if (width == priv->image_width && height == priv->image_height)
	return priv->blank_icon;
      else
	{
	  g_object_unref (priv->blank_icon);
	  priv->blank_icon = NULL;
	}
    }

  priv->blank_icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
				     priv->image_width, 
				     priv->image_height);
  if (priv->blank_icon)
    gdk_pixbuf_fill (priv->blank_icon, 0);

  return priv->blank_icon;
}

#ifdef GDK_WINDOWING_X11

static GtkIconSize
find_icon_size (GtkWidget *widget, 
		gint       pixel_size)
{
  GdkScreen *screen;
  GtkSettings *settings;
  GtkIconSize s, size;
  gint w, h, d, dist;

  screen = gtk_widget_get_screen (widget);

  if (!screen)
    return GTK_ICON_SIZE_MENU;

  settings = gtk_settings_get_for_screen (screen);
  
  dist = G_MAXINT;
  size = GTK_ICON_SIZE_MENU;

  for (s = GTK_ICON_SIZE_MENU; s <= GTK_ICON_SIZE_DIALOG; s++)
    {
      if (gtk_icon_size_lookup_for_settings (settings, s, &w, &h) &&
	  w <= pixel_size && h <= pixel_size)
	{
	  d = MAX (pixel_size - w, pixel_size - h);
	  if (d < dist)
	    {
	      dist = d;
	      size = s;
	    }
	}
    }
  
  return size;
}

#endif

static void
gtk_status_icon_update_image (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv = status_icon->priv;
#ifdef GDK_WINDOWING_WIN32
  HICON prev_hicon;
#endif

  if (priv->blink_off)
    {
#ifdef GDK_WINDOWING_X11
      gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image),
				 gtk_status_icon_blank_icon (status_icon));
#endif
#ifdef GDK_WINDOWING_WIN32
      prev_hicon = priv->nid.hIcon;
      priv->nid.hIcon = gdk_win32_pixbuf_to_hicon_libgtk_only (gtk_status_icon_blank_icon (status_icon));
      priv->nid.uFlags |= NIF_ICON;
      if (priv->nid.hWnd != NULL && priv->visible)
	if (!Shell_NotifyIconW (NIM_MODIFY, &priv->nid))
	  g_warning (G_STRLOC ": Shell_NotifyIcon(NIM_MODIFY) failed");
      if (prev_hicon)
	DestroyIcon (prev_hicon);
#endif
#ifdef GDK_WINDOWING_QUARTZ
      QUARTZ_POOL_ALLOC;
      [priv->status_item setImage:gtk_status_icon_blank_icon (status_icon)];
      QUARTZ_POOL_RELEASE;
#endif
      return;
    }

  switch (priv->storage_type)
    {
    case GTK_IMAGE_PIXBUF:
      {
	GdkPixbuf *pixbuf;

	pixbuf = priv->image_data.pixbuf;

	if (pixbuf)
	  {
	    GdkPixbuf *scaled;
	    gint size;
	    gint width;
	    gint height;

	    size = priv->size;

	    width  = gdk_pixbuf_get_width  (pixbuf);
	    height = gdk_pixbuf_get_height (pixbuf);

	    if (width > size || height > size)
	      scaled = gdk_pixbuf_scale_simple (pixbuf,
						MIN (size, width),
						MIN (size, height),
						GDK_INTERP_BILINEAR);
	    else
	      scaled = g_object_ref (pixbuf);

#ifdef GDK_WINDOWING_X11
	    gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), scaled);
#endif
#ifdef GDK_WINDOWING_WIN32
	    prev_hicon = priv->nid.hIcon;
	    priv->nid.hIcon = gdk_win32_pixbuf_to_hicon_libgtk_only (scaled);
	    priv->nid.uFlags |= NIF_ICON;
	    if (priv->nid.hWnd != NULL && priv->visible)
	      if (!Shell_NotifyIconW (NIM_MODIFY, &priv->nid))
		  g_warning (G_STRLOC ": Shell_NotifyIcon(NIM_MODIFY) failed");
	    if (prev_hicon)
	      DestroyIcon (prev_hicon);
#endif
#ifdef GDK_WINDOWING_QUARTZ
      QUARTZ_POOL_ALLOC;
      [priv->status_item setImage:scaled];
      QUARTZ_POOL_RELEASE;
#endif
			
	    g_object_unref (scaled);
	  }
	else
	  {
#ifdef GDK_WINDOWING_X11
	    gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), NULL);
#endif
#ifdef GDK_WINDOWING_WIN32
	    priv->nid.uFlags &= ~NIF_ICON;
	    if (priv->nid.hWnd != NULL && priv->visible)
	      if (!Shell_NotifyIconW (NIM_MODIFY, &priv->nid))
		g_warning (G_STRLOC ": Shell_NotifyIcon(NIM_MODIFY) failed");
#endif
#ifdef GDK_WINDOWING_QUARTZ
      [priv->status_item setImage:NULL];
#endif
	  }
      }
      break;

    case GTK_IMAGE_STOCK:
      {
#ifdef GDK_WINDOWING_X11
	GtkIconSize size = find_icon_size (priv->image, priv->size);
	gtk_image_set_from_stock (GTK_IMAGE (priv->image),
				  priv->image_data.stock_id,
				  size);
#endif
#ifdef GDK_WINDOWING_WIN32
	{
	  GdkPixbuf *pixbuf =
	    gtk_widget_render_icon (priv->dummy_widget,
				    priv->image_data.stock_id,
				    GTK_ICON_SIZE_SMALL_TOOLBAR,
				    NULL);

	  prev_hicon = priv->nid.hIcon;
	  priv->nid.hIcon = gdk_win32_pixbuf_to_hicon_libgtk_only (pixbuf);
	  priv->nid.uFlags |= NIF_ICON;
	  if (priv->nid.hWnd != NULL && priv->visible)
	    if (!Shell_NotifyIconW (NIM_MODIFY, &priv->nid))
	      g_warning (G_STRLOC ": Shell_NotifyIcon(NIM_MODIFY) failed");
	  if (prev_hicon)
	    DestroyIcon (prev_hicon);
	  g_object_unref (pixbuf);
	}
#endif
#ifdef GDK_WINDOWING_QUARTZ
	{
	  GdkPixbuf *pixbuf;

	  pixbuf = gtk_widget_render_icon (priv->dummy_widget,
					   priv->image_data.stock_id,
					   GTK_ICON_SIZE_SMALL_TOOLBAR,
					   NULL);
	  QUARTZ_POOL_ALLOC;
	  [priv->status_item setImage:pixbuf];
	  QUARTZ_POOL_RELEASE;
	  g_object_unref (pixbuf);
	}	
#endif
      }
      break;
      
    case GTK_IMAGE_ICON_NAME:
      {
#ifdef GDK_WINDOWING_X11
	GtkIconSize size = find_icon_size (priv->image, priv->size);
	gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				      priv->image_data.icon_name,
				      size);
#endif
#ifdef GDK_WINDOWING_WIN32
	{
	  GdkPixbuf *pixbuf =
	    gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
				      priv->image_data.icon_name,
				      priv->size,
				      0, NULL);
	  
	  prev_hicon = priv->nid.hIcon;
	  priv->nid.hIcon = gdk_win32_pixbuf_to_hicon_libgtk_only (pixbuf);
	  priv->nid.uFlags |= NIF_ICON;
	  if (priv->nid.hWnd != NULL && priv->visible)
	    if (!Shell_NotifyIconW (NIM_MODIFY, &priv->nid))
	      g_warning (G_STRLOC ": Shell_NotifyIcon(NIM_MODIFY) failed");
	  if (prev_hicon)
	    DestroyIcon (prev_hicon);
	  g_object_unref (pixbuf);
	}
#endif
#ifdef GDK_WINDOWING_QUARTZ
	{
	  GdkPixbuf *pixbuf;

	  pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
					     priv->image_data.icon_name,
					     priv->size,
					     0, NULL);

	  QUARTZ_POOL_ALLOC;
	  [priv->status_item setImage:pixbuf];
	  QUARTZ_POOL_RELEASE;
	  g_object_unref (pixbuf);
	}
#endif
	
      }
      break;

    case GTK_IMAGE_GICON:
      {
#ifdef GDK_WINDOWING_X11
        GtkIconSize size = find_icon_size (priv->image, priv->size);
        gtk_image_set_from_gicon (GTK_IMAGE (priv->image),
                                  priv->image_data.gicon,
                                  size);
#endif
#ifdef GDK_WINDOWING_WIN32
      {
        GtkIconInfo *info =
        gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
                                        priv->image_data.gicon,
                                        priv->size,
                                        0);
        GdkPixbuf *pixbuf = gtk_icon_info_load_icon (info, NULL);

        prev_hicon = priv->nid.hIcon;
        priv->nid.hIcon = gdk_win32_pixbuf_to_hicon_libgtk_only (pixbuf);
        priv->nid.uFlags |= NIF_ICON;
        if (priv->nid.hWnd != NULL && priv->visible)
          if (!Shell_NotifyIconW (NIM_MODIFY, &priv->nid))
            g_warning (G_STRLOC ": Shell_NotifyIcon(NIM_MODIFY) failed");
          if (prev_hicon)
            DestroyIcon (prev_hicon);
          g_object_unref (pixbuf);
      }
#endif
#ifdef GDK_WINDOWING_QUARTZ
      {
        GtkIconInfo *info =
        gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
                                        priv->image_data.gicon,
                                        priv->size,
                                        0);
        GdkPixbuf *pixbuf = gtk_icon_info_load_icon (info, NULL);

        QUARTZ_POOL_ALLOC;
        [priv->status_item setImage:pixbuf];
        QUARTZ_POOL_RELEASE;
        g_object_unref (pixbuf);
      }
#endif

      }
      break;

    case GTK_IMAGE_EMPTY:
#ifdef GDK_WINDOWING_X11
      gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), NULL);
#endif
#ifdef GDK_WINDOWING_WIN32
      priv->nid.uFlags &= ~NIF_ICON;
      if (priv->nid.hWnd != NULL && priv->visible)
	if (!Shell_NotifyIconW (NIM_MODIFY, &priv->nid))
	  g_warning (G_STRLOC ": Shell_NotifyIcon(NIM_MODIFY) failed");
#endif
#ifdef GDK_WINDOWING_QUARTZ
        {
          QUARTZ_POOL_ALLOC;
          [priv->status_item setImage:NULL];
          QUARTZ_POOL_RELEASE;
        }
#endif
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

#ifdef GDK_WINDOWING_X11

static void
gtk_status_icon_size_allocate (GtkStatusIcon *status_icon,
			       GtkAllocation *allocation)
{
  GtkStatusIconPrivate *priv = status_icon->priv;
  GtkOrientation orientation;
  gint size;

  orientation = _gtk_tray_icon_get_orientation (GTK_TRAY_ICON (priv->tray_icon));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    size = allocation->height;
  else
    size = allocation->width;

  priv->image_width = allocation->width - GTK_MISC (priv->image)->xpad * 2;
  priv->image_height = allocation->height - GTK_MISC (priv->image)->ypad * 2;

  if (priv->size - 1 > size || priv->size + 1 < size)
    {
      priv->size = size;

      g_object_notify (G_OBJECT (status_icon), "size");

      if (!emit_size_changed_signal (status_icon, size))
	gtk_status_icon_update_image (status_icon);
    }
}

static void
gtk_status_icon_screen_changed (GtkStatusIcon *status_icon,
				GdkScreen *old_screen)
{
  GtkStatusIconPrivate *priv = status_icon->priv;

  if (gtk_widget_get_screen (priv->tray_icon) != old_screen)
    {
      g_object_notify (G_OBJECT (status_icon), "screen");
    }
}

#endif

#ifdef GDK_WINDOWING_X11

static void
gtk_status_icon_embedded_changed (GtkStatusIcon *status_icon)
{
  g_object_notify (G_OBJECT (status_icon), "embedded");
}

static void
gtk_status_icon_orientation_changed (GtkStatusIcon *status_icon)
{
  g_object_notify (G_OBJECT (status_icon), "orientation");
}

static gboolean
gtk_status_icon_key_press (GtkStatusIcon  *status_icon,
			   GdkEventKey    *event)
{
  guint state, keyval;

  state = event->state & gtk_accelerator_get_default_mod_mask ();
  keyval = event->keyval;
  if (state == 0 &&
      (keyval == GDK_Return ||
       keyval == GDK_KP_Enter ||
       keyval == GDK_ISO_Enter ||
       keyval == GDK_space ||
       keyval == GDK_KP_Space))
    {
      emit_activate_signal (status_icon);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_status_icon_popup_menu (GtkStatusIcon  *status_icon)
{
  emit_popup_menu_signal (status_icon, 0, gtk_get_current_event_time ());
}

#endif

static gboolean
gtk_status_icon_button_press (GtkStatusIcon  *status_icon,
			      GdkEventButton *event)
{
  gboolean handled = FALSE;

  g_signal_emit (status_icon,
		 status_icon_signals [BUTTON_PRESS_EVENT_SIGNAL], 0,
		 event, &handled);
  if (handled)
    return TRUE;

  if (_gtk_button_event_triggers_context_menu (event))
    {
      emit_popup_menu_signal (status_icon, event->button, event->time);
      return TRUE;
    }
  else if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {
      emit_activate_signal (status_icon);
      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_status_icon_button_release (GtkStatusIcon  *status_icon,
				GdkEventButton *event)
{
  gboolean handled = FALSE;
  g_signal_emit (status_icon,
		 status_icon_signals [BUTTON_RELEASE_EVENT_SIGNAL], 0,
		 event, &handled);
  return handled;
}

#ifdef GDK_WINDOWING_X11
static gboolean
gtk_status_icon_scroll (GtkStatusIcon  *status_icon,
			GdkEventScroll *event)
{
  gboolean handled = FALSE;
  g_signal_emit (status_icon,
		 status_icon_signals [SCROLL_EVENT_SIGNAL], 0,
		 event, &handled);
  return handled;
}

static gboolean
gtk_status_icon_query_tooltip (GtkStatusIcon *status_icon,
			       gint           x,
			       gint           y,
			       gboolean       keyboard_tip,
			       GtkTooltip    *tooltip)
{
  gboolean handled = FALSE;
  g_signal_emit (status_icon,
		 status_icon_signals [QUERY_TOOLTIP_SIGNAL], 0,
		 x, y, keyboard_tip, tooltip, &handled);
  return handled;
}
#endif /* GDK_WINDOWING_X11 */

static void
gtk_status_icon_reset_image_data (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv = status_icon->priv;

  switch (priv->storage_type)
  {
    case GTK_IMAGE_PIXBUF:
      if (priv->image_data.pixbuf)
	g_object_unref (priv->image_data.pixbuf);
      priv->image_data.pixbuf = NULL;
      g_object_notify (G_OBJECT (status_icon), "pixbuf");
      break;

    case GTK_IMAGE_STOCK:
      g_free (priv->image_data.stock_id);
      priv->image_data.stock_id = NULL;

      g_object_notify (G_OBJECT (status_icon), "stock");
      break;
      
    case GTK_IMAGE_ICON_NAME:
      g_free (priv->image_data.icon_name);
      priv->image_data.icon_name = NULL;

      g_object_notify (G_OBJECT (status_icon), "icon-name");
      break;

    case GTK_IMAGE_GICON:
      if (priv->image_data.gicon)
        g_object_unref (priv->image_data.gicon);
      priv->image_data.gicon = NULL;

      g_object_notify (G_OBJECT (status_icon), "gicon");
      break;

    case GTK_IMAGE_EMPTY:
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  priv->storage_type = GTK_IMAGE_EMPTY;
  g_object_notify (G_OBJECT (status_icon), "storage-type");
}

static void
gtk_status_icon_set_image (GtkStatusIcon *status_icon,
    			   GtkImageType   storage_type,
			   gpointer       data)
{
  GtkStatusIconPrivate *priv = status_icon->priv;

  g_object_freeze_notify (G_OBJECT (status_icon));

  gtk_status_icon_reset_image_data (status_icon);

  priv->storage_type = storage_type;
  g_object_notify (G_OBJECT (status_icon), "storage-type");

  switch (storage_type) 
    {
    case GTK_IMAGE_PIXBUF:
      priv->image_data.pixbuf = (GdkPixbuf *)data;
      g_object_notify (G_OBJECT (status_icon), "pixbuf");
      break;
    case GTK_IMAGE_STOCK:
      priv->image_data.stock_id = g_strdup ((const gchar *)data);
      g_object_notify (G_OBJECT (status_icon), "stock");
      break;
    case GTK_IMAGE_ICON_NAME:
      priv->image_data.icon_name = g_strdup ((const gchar *)data);
      g_object_notify (G_OBJECT (status_icon), "icon-name");
      break;
    case GTK_IMAGE_GICON:
      priv->image_data.gicon = (GIcon *)data;
      g_object_notify (G_OBJECT (status_icon), "gicon");
      break;
    default:
      g_warning ("Image type %u not handled by GtkStatusIcon", storage_type);
    }

  g_object_thaw_notify (G_OBJECT (status_icon));

  gtk_status_icon_update_image (status_icon);
}

/**
 * gtk_status_icon_set_from_pixbuf:
 * @status_icon: a #GtkStatusIcon
 * @pixbuf: (allow-none): a #GdkPixbuf or %NULL
 *
 * Makes @status_icon display @pixbuf.
 * See gtk_status_icon_new_from_pixbuf() for details.
 *
 * Since: 2.10
 **/
void
gtk_status_icon_set_from_pixbuf (GtkStatusIcon *status_icon,
				 GdkPixbuf     *pixbuf)
{
  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  if (pixbuf)
    g_object_ref (pixbuf);

  gtk_status_icon_set_image (status_icon, GTK_IMAGE_PIXBUF,
      			     (gpointer) pixbuf);
}

/**
 * gtk_status_icon_set_from_file:
 * @status_icon: a #GtkStatusIcon
 * @filename: a filename
 * 
 * Makes @status_icon display the file @filename.
 * See gtk_status_icon_new_from_file() for details.
 *
 * Since: 2.10 
 **/
void
gtk_status_icon_set_from_file (GtkStatusIcon *status_icon,
 			       const gchar   *filename)
{
  GdkPixbuf *pixbuf;
  
  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));
  g_return_if_fail (filename != NULL);
  
  pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
  
  gtk_status_icon_set_from_pixbuf (status_icon, pixbuf);
  
  if (pixbuf)
    g_object_unref (pixbuf);
}

/**
 * gtk_status_icon_set_from_stock:
 * @status_icon: a #GtkStatusIcon
 * @stock_id: a stock icon id
 * 
 * Makes @status_icon display the stock icon with the id @stock_id.
 * See gtk_status_icon_new_from_stock() for details.
 *
 * Since: 2.10 
 **/
void
gtk_status_icon_set_from_stock (GtkStatusIcon *status_icon,
				const gchar   *stock_id)
{
  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));
  g_return_if_fail (stock_id != NULL);

  gtk_status_icon_set_image (status_icon, GTK_IMAGE_STOCK,
      			     (gpointer) stock_id);
}

/**
 * gtk_status_icon_set_from_icon_name:
 * @status_icon: a #GtkStatusIcon
 * @icon_name: an icon name
 * 
 * Makes @status_icon display the icon named @icon_name from the 
 * current icon theme.
 * See gtk_status_icon_new_from_icon_name() for details.
 *
 * Since: 2.10 
 **/
void
gtk_status_icon_set_from_icon_name (GtkStatusIcon *status_icon,
				    const gchar   *icon_name)
{
  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));
  g_return_if_fail (icon_name != NULL);

  gtk_status_icon_set_image (status_icon, GTK_IMAGE_ICON_NAME,
      			     (gpointer) icon_name);
}

/**
 * gtk_status_icon_set_from_gicon:
 * @status_icon: a #GtkStatusIcon
 * @icon: a GIcon
 *
 * Makes @status_icon display the #GIcon.
 * See gtk_status_icon_new_from_gicon() for details.
 *
 * Since: 2.14
 **/
void
gtk_status_icon_set_from_gicon (GtkStatusIcon *status_icon,
                                GIcon         *icon)
{
  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));
  g_return_if_fail (icon != NULL);

  if (icon)
    g_object_ref (icon);

  gtk_status_icon_set_image (status_icon, GTK_IMAGE_GICON,
                             (gpointer) icon);
}

/**
 * gtk_status_icon_get_storage_type:
 * @status_icon: a #GtkStatusIcon
 * 
 * Gets the type of representation being used by the #GtkStatusIcon
 * to store image data. If the #GtkStatusIcon has no image data,
 * the return value will be %GTK_IMAGE_EMPTY. 
 * 
 * Return value: the image representation being used
 *
 * Since: 2.10
 **/
GtkImageType
gtk_status_icon_get_storage_type (GtkStatusIcon *status_icon)
{
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), GTK_IMAGE_EMPTY);

  return status_icon->priv->storage_type;
}
/**
 * gtk_status_icon_get_pixbuf:
 * @status_icon: a #GtkStatusIcon
 * 
 * Gets the #GdkPixbuf being displayed by the #GtkStatusIcon.
 * The storage type of the status icon must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_PIXBUF (see gtk_status_icon_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned pixbuf.
 * 
 * Return value: (transfer none): the displayed pixbuf,
 *     or %NULL if the image is empty.
 *
 * Since: 2.10
 **/
GdkPixbuf *
gtk_status_icon_get_pixbuf (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv;

  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), NULL);

  priv = status_icon->priv;

  g_return_val_if_fail (priv->storage_type == GTK_IMAGE_PIXBUF ||
			priv->storage_type == GTK_IMAGE_EMPTY, NULL);

  if (priv->storage_type == GTK_IMAGE_EMPTY)
    priv->image_data.pixbuf = NULL;

  return priv->image_data.pixbuf;
}

/**
 * gtk_status_icon_get_stock:
 * @status_icon: a #GtkStatusIcon
 * 
 * Gets the id of the stock icon being displayed by the #GtkStatusIcon.
 * The storage type of the status icon must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_STOCK (see gtk_status_icon_get_storage_type()).
 * The returned string is owned by the #GtkStatusIcon and should not
 * be freed or modified.
 * 
 * Return value: stock id of the displayed stock icon,
 *   or %NULL if the image is empty.
 *
 * Since: 2.10
 **/
const gchar *
gtk_status_icon_get_stock (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv;

  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), NULL);

  priv = status_icon->priv;

  g_return_val_if_fail (priv->storage_type == GTK_IMAGE_STOCK ||
			priv->storage_type == GTK_IMAGE_EMPTY, NULL);
  
  if (priv->storage_type == GTK_IMAGE_EMPTY)
    priv->image_data.stock_id = NULL;

  return priv->image_data.stock_id;
}

/**
 * gtk_status_icon_get_icon_name:
 * @status_icon: a #GtkStatusIcon
 * 
 * Gets the name of the icon being displayed by the #GtkStatusIcon.
 * The storage type of the status icon must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_ICON_NAME (see gtk_status_icon_get_storage_type()).
 * The returned string is owned by the #GtkStatusIcon and should not
 * be freed or modified.
 * 
 * Return value: name of the displayed icon, or %NULL if the image is empty.
 *
 * Since: 2.10
 **/
const gchar *
gtk_status_icon_get_icon_name (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), NULL);

  priv = status_icon->priv;

  g_return_val_if_fail (priv->storage_type == GTK_IMAGE_ICON_NAME ||
			priv->storage_type == GTK_IMAGE_EMPTY, NULL);

  if (priv->storage_type == GTK_IMAGE_EMPTY)
    priv->image_data.icon_name = NULL;

  return priv->image_data.icon_name;
}

/**
 * gtk_status_icon_get_gicon:
 * @status_icon: a #GtkStatusIcon
 *
 * Retrieves the #GIcon being displayed by the #GtkStatusIcon.
 * The storage type of the status icon must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_GICON (see gtk_status_icon_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned #GIcon.
 *
 * If this function fails, @icon is left unchanged;
 *
 * Returns: (transfer none): the displayed icon, or %NULL if the image is empty
 *
 * Since: 2.14
 **/
GIcon *
gtk_status_icon_get_gicon (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv;

  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), NULL);

  priv = status_icon->priv;

  g_return_val_if_fail (priv->storage_type == GTK_IMAGE_GICON ||
                        priv->storage_type == GTK_IMAGE_EMPTY, NULL);

  if (priv->storage_type == GTK_IMAGE_EMPTY)
    priv->image_data.gicon = NULL;

  return priv->image_data.gicon;
}

/**
 * gtk_status_icon_get_size:
 * @status_icon: a #GtkStatusIcon
 * 
 * Gets the size in pixels that is available for the image. 
 * Stock icons and named icons adapt their size automatically
 * if the size of the notification area changes. For other
 * storage types, the size-changed signal can be used to
 * react to size changes.
 *
 * Note that the returned size is only meaningful while the 
 * status icon is embedded (see gtk_status_icon_is_embedded()).
 * 
 * Return value: the size that is available for the image
 *
 * Since: 2.10
 **/
gint
gtk_status_icon_get_size (GtkStatusIcon *status_icon)
{
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), 0);

  return status_icon->priv->size;
}

/**
 * gtk_status_icon_set_screen:
 * @status_icon: a #GtkStatusIcon
 * @screen: a #GdkScreen
 *
 * Sets the #GdkScreen where @status_icon is displayed; if
 * the icon is already mapped, it will be unmapped, and
 * then remapped on the new screen.
 *
 * Since: 2.12
 */
void
gtk_status_icon_set_screen (GtkStatusIcon *status_icon,
                            GdkScreen     *screen)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));

#ifdef GDK_WINDOWING_X11
  gtk_window_set_screen (GTK_WINDOW (status_icon->priv->tray_icon), screen);
#endif
}

/**
 * gtk_status_icon_get_screen:
 * @status_icon: a #GtkStatusIcon
 *
 * Returns the #GdkScreen associated with @status_icon.
 *
 * Return value: (transfer none): a #GdkScreen.
 *
 * Since: 2.12
 */
GdkScreen *
gtk_status_icon_get_screen (GtkStatusIcon *status_icon)
{
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), NULL);

#ifdef GDK_WINDOWING_X11
  return gtk_window_get_screen (GTK_WINDOW (status_icon->priv->tray_icon));
#else
  return gdk_screen_get_default ();
#endif
}

/**
 * gtk_status_icon_set_tooltip:
 * @status_icon: a #GtkStatusIcon
 * @tooltip_text: (allow-none): the tooltip text, or %NULL
 *
 * Sets the tooltip of the status icon.
 *
 * Since: 2.10
 *
 * Deprecated: 2.16: Use gtk_status_icon_set_tooltip_text() instead.
 */
void
gtk_status_icon_set_tooltip (GtkStatusIcon *status_icon,
			     const gchar   *tooltip_text)
{
  gtk_status_icon_set_tooltip_text (status_icon, tooltip_text);
}

static gboolean
gtk_status_icon_blinker (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv = status_icon->priv;
  
  priv->blink_off = !priv->blink_off;

  gtk_status_icon_update_image (status_icon);

  return TRUE;
}

static void
gtk_status_icon_enable_blinking (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv = status_icon->priv;
  
  if (!priv->blinking_timeout)
    {
      gtk_status_icon_blinker (status_icon);

      priv->blinking_timeout =
	gdk_threads_add_timeout (BLINK_TIMEOUT, 
		       (GSourceFunc) gtk_status_icon_blinker, 
		       status_icon);
    }
}

static void
gtk_status_icon_disable_blinking (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv = status_icon->priv;
  
  if (priv->blinking_timeout)
    {
      g_source_remove (priv->blinking_timeout);
      priv->blinking_timeout = 0;
      priv->blink_off = FALSE;

      gtk_status_icon_update_image (status_icon);
    }
}

/**
 * gtk_status_icon_set_visible:
 * @status_icon: a #GtkStatusIcon
 * @visible: %TRUE to show the status icon, %FALSE to hide it
 * 
 * Shows or hides a status icon.
 *
 * Since: 2.10
 **/
void
gtk_status_icon_set_visible (GtkStatusIcon *status_icon,
			     gboolean       visible)
{
  GtkStatusIconPrivate *priv;

  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));

  priv = status_icon->priv;

  visible = visible != FALSE;

  if (priv->visible != visible)
    {
      priv->visible = visible;

#ifdef GDK_WINDOWING_X11
      if (visible)
	gtk_widget_show (priv->tray_icon);
      else if (gtk_widget_get_realized (priv->tray_icon))
        {
	  gtk_widget_hide (priv->tray_icon);
	  gtk_widget_unrealize (priv->tray_icon);
        }
#endif
#ifdef GDK_WINDOWING_WIN32
      if (priv->nid.hWnd != NULL)
	{
	  if (visible)
	    Shell_NotifyIconW (NIM_ADD, &priv->nid);
	  else
	    Shell_NotifyIconW (NIM_DELETE, &priv->nid);
	}
#endif
#ifdef GDK_WINDOWING_QUARTZ
      QUARTZ_POOL_ALLOC;
      [priv->status_item setVisible:visible];
      QUARTZ_POOL_RELEASE;
#endif
      g_object_notify (G_OBJECT (status_icon), "visible");
    }
}

/**
 * gtk_status_icon_get_visible:
 * @status_icon: a #GtkStatusIcon
 * 
 * Returns whether the status icon is visible or not. 
 * Note that being visible does not guarantee that 
 * the user can actually see the icon, see also 
 * gtk_status_icon_is_embedded().
 * 
 * Return value: %TRUE if the status icon is visible
 *
 * Since: 2.10
 **/
gboolean
gtk_status_icon_get_visible (GtkStatusIcon *status_icon)
{
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), FALSE);

  return status_icon->priv->visible;
}

/**
 * gtk_status_icon_set_blinking:
 * @status_icon: a #GtkStatusIcon
 * @blinking: %TRUE to turn blinking on, %FALSE to turn it off
 * 
 * Makes the status icon start or stop blinking. 
 * Note that blinking user interface elements may be problematic
 * for some users, and thus may be turned off, in which case
 * this setting has no effect.
 *
 * Since: 2.10
 *
 * Deprecated: 2.22: This function will be removed in GTK+ 3
 **/
void
gtk_status_icon_set_blinking (GtkStatusIcon *status_icon,
			      gboolean       blinking)
{
  GtkStatusIconPrivate *priv;

  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));

  priv = status_icon->priv;

  blinking = blinking != FALSE;

  if (priv->blinking != blinking)
    {
      priv->blinking = blinking;

      if (blinking)
	gtk_status_icon_enable_blinking (status_icon);
      else
	gtk_status_icon_disable_blinking (status_icon);

      g_object_notify (G_OBJECT (status_icon), "blinking");
    }
}

/**
 * gtk_status_icon_get_blinking:
 * @status_icon: a #GtkStatusIcon
 * 
 * Returns whether the icon is blinking, see 
 * gtk_status_icon_set_blinking().
 * 
 * Return value: %TRUE if the icon is blinking
 *
 * Since: 2.10
 *
 * Deprecated: 2.22: This function will be removed in GTK+ 3
 **/
gboolean
gtk_status_icon_get_blinking (GtkStatusIcon *status_icon)
{
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), FALSE);

  return status_icon->priv->blinking;
}

/**
 * gtk_status_icon_is_embedded:
 * @status_icon: a #GtkStatusIcon
 * 
 * Returns whether the status icon is embedded in a notification
 * area. 
 * 
 * Return value: %TRUE if the status icon is embedded in
 *   a notification area.
 *
 * Since: 2.10
 **/
gboolean
gtk_status_icon_is_embedded (GtkStatusIcon *status_icon)
{
#ifdef GDK_WINDOWING_X11
  GtkPlug *plug;
#endif

  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), FALSE);

#ifdef GDK_WINDOWING_X11
  plug = GTK_PLUG (status_icon->priv->tray_icon);

  if (plug->socket_window)
    return TRUE;
  else
    return FALSE;
#endif
#ifdef GDK_WINDOWING_WIN32
  return TRUE;
#endif
#ifdef GDK_WINDOWING_QUARTZ
  return TRUE;
#endif
}

/**
 * gtk_status_icon_position_menu:
 * @menu: the #GtkMenu
 * @x: return location for the x position
 * @y: return location for the y position
 * @push_in: whether the first menu item should be offset (pushed in) to be
 *           aligned with the menu popup position (only useful for GtkOptionMenu).
 * @user_data: the status icon to position the menu on
 *
 * Menu positioning function to use with gtk_menu_popup()
 * to position @menu aligned to the status icon @user_data.
 * 
 * Since: 2.10
 **/
void
gtk_status_icon_position_menu (GtkMenu  *menu,
			       gint     *x,
			       gint     *y,
			       gboolean *push_in,
			       gpointer  user_data)
{
#ifdef GDK_WINDOWING_X11
  GtkStatusIcon *status_icon;
  GtkStatusIconPrivate *priv;
  GtkTrayIcon *tray_icon;
  GtkWidget *widget;
  GdkScreen *screen;
  GtkTextDirection direction;
  GtkRequisition menu_req;
  GdkRectangle monitor;
  gint monitor_num, height, width, xoffset, yoffset;
  
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_STATUS_ICON (user_data));

  status_icon = GTK_STATUS_ICON (user_data);
  priv = status_icon->priv;
  tray_icon = GTK_TRAY_ICON (priv->tray_icon);
  widget = priv->tray_icon;

  direction = gtk_widget_get_direction (widget);

  screen = gtk_widget_get_screen (widget);
  gtk_menu_set_screen (menu, screen);

  monitor_num = gdk_screen_get_monitor_at_window (screen, widget->window);
  if (monitor_num < 0)
    monitor_num = 0;
  gtk_menu_set_monitor (menu, monitor_num);

  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  gdk_window_get_origin (widget->window, x, y);
  
  gtk_widget_size_request (GTK_WIDGET (menu), &menu_req);

  if (_gtk_tray_icon_get_orientation (tray_icon) == GTK_ORIENTATION_VERTICAL)
    {
      width = 0;
      height = widget->allocation.height;
      xoffset = widget->allocation.width;
      yoffset = 0;
    }
  else
    {
      width = widget->allocation.width;
      height = 0;
      xoffset = 0;
      yoffset = widget->allocation.height;
    }

  if (direction == GTK_TEXT_DIR_RTL)
    {
      if ((*x - (menu_req.width - width)) >= monitor.x)
        *x -= menu_req.width - width;
      else if ((*x + xoffset + menu_req.width) < (monitor.x + monitor.width))
        *x += xoffset;
      else if ((monitor.x + monitor.width - (*x + xoffset)) < *x)
        *x -= menu_req.width - width;
      else
        *x += xoffset;
    }
  else
    {
      if ((*x + xoffset + menu_req.width) < (monitor.x + monitor.width))
        *x += xoffset;
      else if ((*x - (menu_req.width - width)) >= monitor.x)
        *x -= menu_req.width - width;
      else if ((monitor.x + monitor.width - (*x + xoffset)) > *x)
        *x += xoffset;
      else 
        *x -= menu_req.width - width;
    }

  if ((*y + yoffset + menu_req.height) < (monitor.y + monitor.height))
    *y += yoffset;
  else if ((*y - (menu_req.height - height)) >= monitor.y)
    *y -= menu_req.height - height;
  else if (monitor.y + monitor.height - (*y + yoffset) > *y)
    *y += yoffset;
  else 
    *y -= menu_req.height - height;

  *push_in = FALSE;
#endif /* GDK_WINDOWING_X11 */

#ifdef GDK_WINDOWING_WIN32
  GtkStatusIcon *status_icon;
  GtkStatusIconPrivate *priv;
  GtkRequisition menu_req;
  
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_STATUS_ICON (user_data));

  status_icon = GTK_STATUS_ICON (user_data);
  priv = status_icon->priv;

  gtk_widget_size_request (GTK_WIDGET (menu), &menu_req);

  *x = priv->last_click_x;
  *y = priv->taskbar_top - menu_req.height;

  *push_in = TRUE;
#endif
}

/**
 * gtk_status_icon_get_geometry:
 * @status_icon: a #GtkStatusIcon
 * @screen: (out) (transfer none) (allow-none): return location for the screen, or %NULL if the
 *          information is not needed
 * @area: (out) (allow-none): return location for the area occupied by the status
 *        icon, or %NULL
 * @orientation: (out) (allow-none): return location for the orientation of the panel
 *    in which the status icon is embedded, or %NULL. A panel
 *    at the top or bottom of the screen is horizontal, a panel
 *    at the left or right is vertical.
 *
 * Obtains information about the location of the status icon
 * on screen. This information can be used to e.g. position 
 * popups like notification bubbles. 
 *
 * See gtk_status_icon_position_menu() for a more convenient 
 * alternative for positioning menus.
 *
 * Note that some platforms do not allow GTK+ to provide 
 * this information, and even on platforms that do allow it,
 * the information is not reliable unless the status icon
 * is embedded in a notification area, see
 * gtk_status_icon_is_embedded().
 *
 * Return value: %TRUE if the location information has 
 *               been filled in
 *
 * Since: 2.10
 */
gboolean  
gtk_status_icon_get_geometry (GtkStatusIcon    *status_icon,
			      GdkScreen       **screen,
			      GdkRectangle     *area,
			      GtkOrientation   *orientation)
{
#ifdef GDK_WINDOWING_X11   
  GtkWidget *widget;
  GtkStatusIconPrivate *priv;
  gint x, y;

  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), FALSE);

  priv = status_icon->priv;
  widget = priv->tray_icon;

  if (screen)
    *screen = gtk_widget_get_screen (widget);

  if (area)
    {
      gdk_window_get_origin (widget->window, &x, &y);
      area->x = x;
      area->y = y;
      area->width = widget->allocation.width;
      area->height = widget->allocation.height;
    }

  if (orientation)
    *orientation = _gtk_tray_icon_get_orientation (GTK_TRAY_ICON (widget));

  return TRUE;
#else
  return FALSE;
#endif /* GDK_WINDOWING_X11 */
}

/**
 * gtk_status_icon_set_has_tooltip:
 * @status_icon: a #GtkStatusIcon
 * @has_tooltip: whether or not @status_icon has a tooltip
 *
 * Sets the has-tooltip property on @status_icon to @has_tooltip.
 * See #GtkStatusIcon:has-tooltip for more information.
 *
 * Since: 2.16
 */
void
gtk_status_icon_set_has_tooltip (GtkStatusIcon *status_icon,
				 gboolean       has_tooltip)
{
  GtkStatusIconPrivate *priv;

  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));

  priv = status_icon->priv;

#ifdef GDK_WINDOWING_X11
  gtk_widget_set_has_tooltip (priv->tray_icon, has_tooltip);
#endif
#ifdef GDK_WINDOWING_WIN32
  if (!has_tooltip && priv->tooltip_text)
    gtk_status_icon_set_tooltip_text (status_icon, NULL);
#endif
#ifdef GDK_WINDOWING_QUARTZ
  if (!has_tooltip && priv->tooltip_text)
    gtk_status_icon_set_tooltip_text (status_icon, NULL);
#endif
}

/**
 * gtk_status_icon_get_has_tooltip:
 * @status_icon: a #GtkStatusIcon
 *
 * Returns the current value of the has-tooltip property.
 * See #GtkStatusIcon:has-tooltip for more information.
 *
 * Return value: current value of has-tooltip on @status_icon.
 *
 * Since: 2.16
 */
gboolean
gtk_status_icon_get_has_tooltip (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv;
  gboolean has_tooltip = FALSE;

  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), FALSE);

  priv = status_icon->priv;

#ifdef GDK_WINDOWING_X11
  has_tooltip = gtk_widget_get_has_tooltip (priv->tray_icon);
#endif
#ifdef GDK_WINDOWING_WIN32
  has_tooltip = (priv->tooltip_text != NULL);
#endif
#ifdef GDK_WINDOWING_QUARTZ
  has_tooltip = (priv->tooltip_text != NULL);
#endif

  return has_tooltip;
}

/**
 * gtk_status_icon_set_tooltip_text:
 * @status_icon: a #GtkStatusIcon
 * @text: the contents of the tooltip for @status_icon
 *
 * Sets @text as the contents of the tooltip.
 *
 * This function will take care of setting #GtkStatusIcon:has-tooltip to
 * %TRUE and of the default handler for the #GtkStatusIcon::query-tooltip
 * signal.
 *
 * See also the #GtkStatusIcon:tooltip-text property and
 * gtk_tooltip_set_text().
 *
 * Since: 2.16
 */
void
gtk_status_icon_set_tooltip_text (GtkStatusIcon *status_icon,
				  const gchar   *text)
{
  GtkStatusIconPrivate *priv;

  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));

  priv = status_icon->priv;

#ifdef GDK_WINDOWING_X11

  gtk_widget_set_tooltip_text (priv->tray_icon, text);

#endif
#ifdef GDK_WINDOWING_WIN32
  if (text == NULL)
    priv->nid.uFlags &= ~NIF_TIP;
  else
    {
      WCHAR *wcs = g_utf8_to_utf16 (text, -1, NULL, NULL, NULL);

      priv->nid.uFlags |= NIF_TIP;
      wcsncpy (priv->nid.szTip, wcs, G_N_ELEMENTS (priv->nid.szTip) - 1);
      priv->nid.szTip[G_N_ELEMENTS (priv->nid.szTip) - 1] = 0;
      g_free (wcs);
    }
  if (priv->nid.hWnd != NULL && priv->visible)
    if (!Shell_NotifyIconW (NIM_MODIFY, &priv->nid))
      g_warning (G_STRLOC ": Shell_NotifyIconW(NIM_MODIFY) failed");

  g_free (priv->tooltip_text);
  priv->tooltip_text = g_strdup (text);
#endif
#ifdef GDK_WINDOWING_QUARTZ
  QUARTZ_POOL_ALLOC;
  [priv->status_item setToolTip:text];
  QUARTZ_POOL_RELEASE;

  g_free (priv->tooltip_text);
  priv->tooltip_text = g_strdup (text);
#endif
}

/**
 * gtk_status_icon_get_tooltip_text:
 * @status_icon: a #GtkStatusIcon
 *
 * Gets the contents of the tooltip for @status_icon.
 *
 * Return value: the tooltip text, or %NULL. You should free the
 *   returned string with g_free() when done.
 *
 * Since: 2.16
 */
gchar *
gtk_status_icon_get_tooltip_text (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv;
  gchar *tooltip_text = NULL;

  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), NULL);

  priv = status_icon->priv;

#ifdef GDK_WINDOWING_X11
  tooltip_text = gtk_widget_get_tooltip_text (priv->tray_icon);
#endif
#ifdef GDK_WINDOWING_WIN32
  if (priv->tooltip_text)
    tooltip_text = g_strdup (priv->tooltip_text);
#endif
#ifdef GDK_WINDOWING_QUARTZ
  if (priv->tooltip_text)
    tooltip_text = g_strdup (priv->tooltip_text);
#endif

  return tooltip_text;
}

/**
 * gtk_status_icon_set_tooltip_markup:
 * @status_icon: a #GtkStatusIcon
 * @markup: (allow-none): the contents of the tooltip for @status_icon, or %NULL
 *
 * Sets @markup as the contents of the tooltip, which is marked up with
 *  the <link linkend="PangoMarkupFormat">Pango text markup language</link>.
 *
 * This function will take care of setting #GtkStatusIcon:has-tooltip to %TRUE
 * and of the default handler for the #GtkStatusIcon::query-tooltip signal.
 *
 * See also the #GtkStatusIcon:tooltip-markup property and
 * gtk_tooltip_set_markup().
 *
 * Since: 2.16
 */
void
gtk_status_icon_set_tooltip_markup (GtkStatusIcon *status_icon,
				    const gchar   *markup)
{
  GtkStatusIconPrivate *priv;
#ifndef GDK_WINDOWING_X11
  gchar *text = NULL;
#endif

  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));

  priv = status_icon->priv;

#ifdef GDK_WINDOWING_X11
  gtk_widget_set_tooltip_markup (priv->tray_icon, markup);
#endif
#ifdef GDK_WINDOWING_WIN32
  if (markup)
    pango_parse_markup (markup, -1, 0, NULL, &text, NULL, NULL);
  gtk_status_icon_set_tooltip_text (status_icon, text);
  g_free (text);
#endif
#ifdef GDK_WINDOWING_QUARTZ
  if (markup)
    pango_parse_markup (markup, -1, 0, NULL, &text, NULL, NULL);
  gtk_status_icon_set_tooltip_text (status_icon, text);
  g_free (text);
#endif
}

/**
 * gtk_status_icon_get_tooltip_markup:
 * @status_icon: a #GtkStatusIcon
 *
 * Gets the contents of the tooltip for @status_icon.
 *
 * Return value: the tooltip text, or %NULL. You should free the
 *   returned string with g_free() when done.
 *
 * Since: 2.16
 */
gchar *
gtk_status_icon_get_tooltip_markup (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv;
  gchar *markup = NULL;

  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), NULL);

  priv = status_icon->priv;

#ifdef GDK_WINDOWING_X11
  markup = gtk_widget_get_tooltip_markup (priv->tray_icon);
#endif
#ifdef GDK_WINDOWING_WIN32
  if (priv->tooltip_text)
    markup = g_markup_escape_text (priv->tooltip_text, -1);
#endif
#ifdef GDK_WINDOWING_QUARTZ
  if (priv->tooltip_text)
    markup = g_markup_escape_text (priv->tooltip_text, -1);
#endif

  return markup;
}

/**
 * gtk_status_icon_get_x11_window_id:
 * @status_icon: a #GtkStatusIcon
 *
 * This function is only useful on the X11/freedesktop.org platform.
 * It returns a window ID for the widget in the underlying
 * status icon implementation.  This is useful for the Galago 
 * notification service, which can send a window ID in the protocol 
 * in order for the server to position notification windows 
 * pointing to a status icon reliably.
 *
 * This function is not intended for other use cases which are
 * more likely to be met by one of the non-X11 specific methods, such
 * as gtk_status_icon_position_menu().
 *
 * Return value: An 32 bit unsigned integer identifier for the 
 * underlying X11 Window
 *
 * Since: 2.14
 */
guint32
gtk_status_icon_get_x11_window_id (GtkStatusIcon *status_icon)
{
#ifdef GDK_WINDOWING_X11
  gtk_widget_realize (GTK_WIDGET (status_icon->priv->tray_icon));
  return GDK_WINDOW_XID (GTK_WIDGET (status_icon->priv->tray_icon)->window);
#else
  return 0;
#endif
}

/**
 * gtk_status_icon_set_title:
 * @status_icon: a #GtkStatusIcon
 * @title: the title 
 *
 * Sets the title of this tray icon.
 * This should be a short, human-readable, localized string 
 * describing the tray icon. It may be used by tools like screen
 * readers to render the tray icon.
 *
 * Since: 2.18
 */
void
gtk_status_icon_set_title (GtkStatusIcon *status_icon,
                           const gchar   *title)
{
  GtkStatusIconPrivate *priv;

  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));

  priv = status_icon->priv;

#ifdef GDK_WINDOWING_X11
  gtk_window_set_title (GTK_WINDOW (priv->tray_icon), title);
#endif
#ifdef GDK_WINDOWING_QUARTZ
  g_free (priv->title);
  priv->title = g_strdup (title);
#endif
#ifdef GDK_WINDOWING_WIN32
  g_free (priv->title);
  priv->title = g_strdup (title);
#endif

  g_object_notify (G_OBJECT (status_icon), "title");
}

/**
 * gtk_status_icon_get_title:
 * @status_icon: a #GtkStatusIcon
 *
 * Gets the title of this tray icon. See gtk_status_icon_set_title().
 *
 * Returns: the title of the status icon
 *
 * Since: 2.18
 */
const gchar *
gtk_status_icon_get_title (GtkStatusIcon *status_icon)
{
  GtkStatusIconPrivate *priv;

  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), NULL);

  priv = status_icon->priv;

#ifdef GDK_WINDOWING_X11
  return gtk_window_get_title (GTK_WINDOW (priv->tray_icon));
#endif
#ifdef GDK_WINDOWING_QUARTZ
  return priv->title;
#endif
#ifdef GDK_WINDOWING_WIN32
  return priv->title;
#endif
}


/**
 * gtk_status_icon_set_name:
 * @status_icon: a #GtkStatusIcon
 * @name: the name
 *
 * Sets the name of this tray icon.
 * This should be a string identifying this icon. It is may be
 * used for sorting the icons in the tray and will not be shown to
 * the user.
 *
 * Since: 2.20
 */
void
gtk_status_icon_set_name (GtkStatusIcon *status_icon,
                          const gchar   *name)
{
  GtkStatusIconPrivate *priv;

  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));

  priv = status_icon->priv;

#ifdef GDK_WINDOWING_X11
  if (gtk_widget_get_realized (priv->tray_icon))
    {
      /* gtk_window_set_wmclass() only operates on non-realized windows,
       * so temporarily unrealize the tray here
       */
      gtk_widget_hide (priv->tray_icon);
      gtk_widget_unrealize (priv->tray_icon);
      gtk_window_set_wmclass (GTK_WINDOW (priv->tray_icon), name, name);
      gtk_widget_show (priv->tray_icon);
    }
  else
    gtk_window_set_wmclass (GTK_WINDOW (priv->tray_icon), name, name);
#endif
}


#define __GTK_STATUS_ICON_C__
#include "gtkaliasdef.c"
