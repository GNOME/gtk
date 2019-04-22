/* GDK - The GIMP Drawing Kit
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/*
 * Private uninstalled header defining things local to X windowing code
 */

#ifndef __GDK_PRIVATE_X11_H__
#define __GDK_PRIVATE_X11_H__

#include "gdkcursor.h"
#include "gdkinternals.h"
#include "gdkx.h"
#include "gdksurface-x11.h"
#include "gdkscreen-x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>

#include <cairo-xlib.h>

typedef enum {
  GDK_FILTER_CONTINUE,
  GDK_FILTER_TRANSLATE,
  GDK_FILTER_REMOVE
} GdkFilterReturn;

typedef GdkFilterReturn (*GdkFilterFunc) (const XEvent *xevent,
                                          GdkEvent     *event,
                                          gpointer      data);


void _gdk_x11_error_handler_push (void);
void _gdk_x11_error_handler_pop  (void);

void          gdk_display_setup_window_visual            (GdkDisplay     *display,
                                                          gint            depth,
                                                          Visual         *visual,
                                                          Colormap        colormap,
                                                          gboolean        rgba);
int           gdk_x11_display_get_window_depth           (GdkX11Display  *display);
Visual *      gdk_x11_display_get_window_visual          (GdkX11Display  *display);
Colormap      gdk_x11_display_get_window_colormap        (GdkX11Display  *display);

void _gdk_x11_display_add_window    (GdkDisplay *display,
                                     XID        *xid,
                                     GdkSurface  *window);
void _gdk_x11_display_remove_window (GdkDisplay *display,
                                     XID         xid);

gint _gdk_x11_display_send_xevent (GdkDisplay *display,
                                   Window      window,
                                   gboolean    propagate,
                                   glong       event_mask,
                                   XEvent     *event_send);

cairo_region_t* _gdk_x11_xwindow_get_shape  (Display *xdisplay,
                                             Window   window,
                                             gint     scale,
                                             gint     shape_type);

void     _gdk_x11_region_get_xrectangles   (const cairo_region_t  *region,
                                            gint                   x_offset,
                                            gint                   y_offset,
                                            gint                   scale,
                                            XRectangle           **rects,
                                            gint                  *n_rects);

gboolean _gdk_x11_moveresize_handle_event   (const XEvent *event);
gboolean _gdk_x11_moveresize_configure_done (GdkDisplay *display,
                                             GdkSurface  *window);

void     _gdk_x11_keymap_state_changed   (GdkDisplay      *display,
                                          const XEvent    *event);
void     _gdk_x11_keymap_keys_changed    (GdkDisplay      *display);
void     _gdk_x11_keymap_add_virt_mods   (GdkKeymap       *keymap,
                                          GdkModifierType *modifiers);

void _gdk_x11_surfaceing_init    (void);

void _gdk_x11_surface_grab_check_unmap   (GdkSurface *window,
                                          gulong     serial);
void _gdk_x11_surface_grab_check_destroy (GdkSurface *window);

gboolean _gdk_x11_display_is_root_window (GdkDisplay *display,
                                          Window      xroot_window);

void _gdk_x11_display_update_grab_info        (GdkDisplay *display,
                                               GdkDevice  *device,
                                               gint        status);
void _gdk_x11_display_update_grab_info_ungrab (GdkDisplay *display,
                                               GdkDevice  *device,
                                               guint32     time,
                                               gulong      serial);
void _gdk_x11_display_queue_events            (GdkDisplay *display);


GdkAppLaunchContext *_gdk_x11_display_get_app_launch_context (GdkDisplay *display);

gint        _gdk_x11_display_text_property_to_utf8_list (GdkDisplay     *display,
                                                         GdkAtom         encoding,
                                                         gint            format,
                                                         const guchar   *text,
                                                         gint            length,
                                                         gchar        ***list);
gchar *     _gdk_x11_display_utf8_to_string_target      (GdkDisplay     *displayt,
                                                         const gchar    *str);

void _gdk_x11_device_check_extension_events   (GdkDevice  *device);

GdkX11DeviceManagerXI2 *_gdk_x11_device_manager_new (GdkDisplay *display);

guchar * _gdk_x11_device_xi2_translate_event_mask (GdkX11DeviceManagerXI2 *device_manager_xi2,
                                                   GdkEventMask            event_mask,
                                                   gint                   *len);
guint    _gdk_x11_device_xi2_translate_state      (XIModifierState *mods_state,
                                                   XIButtonState   *buttons_state,
                                                   XIGroupState    *group_state);
gint     _gdk_x11_device_xi2_get_id               (GdkX11DeviceXI2 *device);
void     _gdk_device_xi2_unset_scroll_valuators   (GdkX11DeviceXI2 *device);


GdkDevice * _gdk_x11_device_manager_xi2_lookup    (GdkX11DeviceManagerXI2 *device_manager_xi2,
                                                   gint                    device_id);
void     _gdk_x11_device_xi2_add_scroll_valuator  (GdkX11DeviceXI2    *device,
                                                   guint               n_valuator,
                                                   GdkScrollDirection  direction,
                                                   gdouble             increment);
gboolean  _gdk_x11_device_xi2_get_scroll_delta    (GdkX11DeviceXI2    *device,
                                                   guint               n_valuator,
                                                   gdouble             valuator_value,
                                                   GdkScrollDirection *direction_ret,
                                                   gdouble            *delta_ret);
void     _gdk_device_xi2_reset_scroll_valuators   (GdkX11DeviceXI2    *device);

gdouble  gdk_x11_device_xi2_get_last_axis_value (GdkX11DeviceXI2 *device,
                                                 gint             n_axis);

void     gdk_x11_device_xi2_store_axes          (GdkX11DeviceXI2 *device,
                                                 gdouble         *axes,
                                                 gint             n_axes);

GdkAtom _gdk_x11_display_manager_atom_intern   (GdkDisplayManager *manager,
                                                const gchar       *atom_name,
                                                gboolean           copy_name);
gchar * _gdk_x11_display_manager_get_atom_name (GdkDisplayManager *manager,
                                                GdkAtom            atom);

gboolean   _gdk_x11_display_supports_cursor_alpha   (GdkDisplay    *display);
gboolean   _gdk_x11_display_supports_cursor_color   (GdkDisplay    *display);
void       _gdk_x11_display_get_default_cursor_size (GdkDisplay *display,
                                                     guint      *width,
                                                     guint      *height);
void       _gdk_x11_display_get_maximal_cursor_size (GdkDisplay *display,
                                                     guint      *width,
                                                     guint      *height);

GdkSurface * _gdk_x11_display_create_surface (GdkDisplay     *display,
                                              GdkSurfaceType  surface_type,
                                              GdkSurface     *parent,
                                              int             x,
                                              int             y,
                                              int             width,
                                              int             height);
GList *    gdk_x11_display_get_toplevel_windows     (GdkDisplay *display);

void _gdk_x11_precache_atoms (GdkDisplay          *display,
                              const gchar * const *atom_names,
                              gint                 n_atoms);

Atom _gdk_x11_get_xatom_for_display_printf         (GdkDisplay    *display,
                                                    const gchar   *format,
                                                    ...) G_GNUC_PRINTF (2, 3);

GdkDrag        *gdk_x11_drag_find                       (GdkDisplay             *display,
                                                         Window                  source_xid,
                                                         Window                  dest_xid);
void            gdk_x11_drag_handle_status              (GdkDisplay             *display,
                                                         const XEvent           *xevent);
void            gdk_x11_drag_handle_finished            (GdkDisplay             *display,
                                                         const XEvent           *xevent);
void            gdk_x11_drop_read_actions               (GdkDrop                *drop);
gboolean        gdk_x11_drop_filter                     (GdkSurface             *surface,
                                                         const XEvent           *xevent);

typedef struct _GdkSurfaceCache GdkSurfaceCache;

GdkSurfaceCache *
gdk_surface_cache_get (GdkDisplay *display);

GdkFilterReturn
gdk_surface_cache_filter (const XEvent *xevent,
                          GdkEvent     *event,
                          gpointer      data);
GdkFilterReturn
gdk_surface_cache_shape_filter (const XEvent *xevent,
                                GdkEvent     *event,
                                gpointer      data);

void _gdk_x11_cursor_display_finalize (GdkDisplay *display);

void _gdk_x11_surface_register_dnd (GdkSurface *window);

GdkDrag        * _gdk_x11_surface_drag_begin (GdkSurface          *window,
                                              GdkDevice          *device,
                                              GdkContentProvider *content,
                                              GdkDragAction       actions,
                                              gint                dx,
                                              gint                dy);

GdkGrabStatus _gdk_x11_convert_grab_status (gint status);

cairo_surface_t * _gdk_x11_display_create_bitmap_surface (GdkDisplay *display,
                                                          int         width,
                                                          int         height);

extern const gint        _gdk_x11_event_mask_table[];
extern const gint        _gdk_x11_event_mask_table_size;

#define GDK_SCREEN_DISPLAY(screen)    (GDK_X11_SCREEN (screen)->display)
#define GDK_SCREEN_XROOTWIN(screen)   (GDK_X11_SCREEN (screen)->xroot_window)
#define GDK_DISPLAY_XROOTWIN(display) (GDK_SCREEN_XROOTWIN (GDK_X11_DISPLAY (display)->screen))
#define GDK_SURFACE_SCREEN(win)        (GDK_X11_DISPLAY (gdk_surface_get_display (win))->screen)
#define GDK_SURFACE_DISPLAY(win)       (gdk_surface_get_display (win))
#define GDK_SURFACE_XROOTWIN(win)      (GDK_X11_SCREEN (GDK_SURFACE_SCREEN (win))->xroot_window)

/* override some macros from gdkx.h with direct-access variants */
#undef GDK_DISPLAY_XDISPLAY
#undef GDK_SURFACE_XDISPLAY
#undef GDK_SURFACE_XID
#undef GDK_SCREEN_XDISPLAY

#define GDK_DISPLAY_XDISPLAY(display) (GDK_X11_DISPLAY(display)->xdisplay)
#define GDK_SURFACE_XDISPLAY(win)      (GDK_X11_SCREEN (GDK_SURFACE_SCREEN (win))->xdisplay)
#define GDK_SURFACE_XID(win)           (GDK_X11_SURFACE (win)->xid)
#define GDK_SCREEN_XDISPLAY(screen)   (GDK_X11_SCREEN (screen)->xdisplay)

#endif /* __GDK_PRIVATE_X11_H__ */
