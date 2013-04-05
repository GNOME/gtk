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

#ifndef __GDK_PRIVATE_BROADWAY_H__
#define __GDK_PRIVATE_BROADWAY_H__

#include <gdk/gdkcursor.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkinternals.h>
#include "gdkwindow-broadway.h"
#include "gdkdisplay-broadway.h"

#include "gdkbroadwaycursor.h"
#include "gdkbroadwayvisual.h"
#include "gdkbroadwaywindow.h"

void _gdk_broadway_resync_windows (void);
void _gdk_broadway_windowing_init (void);

gchar * _gdk_broadway_display_manager_get_atom_name (GdkDisplayManager *manager,
						     GdkAtom atom);
void _gdk_broadway_display_manager_add_display (GdkDisplayManager *manager,
						GdkDisplay        *display);
void _gdk_broadway_display_manager_remove_display (GdkDisplayManager *manager,
						   GdkDisplay        *display);
GdkAtom _gdk_broadway_display_manager_atom_intern_static_string (GdkDisplayManager *manager,
								 const gchar *atom_name);
GdkAtom _gdk_broadway_display_manager_atom_intern (GdkDisplayManager *manager,
						   const gchar *atom_name, 
						   gboolean     only_if_exists);


void     _gdk_broadway_window_register_dnd (GdkWindow      *window);
GdkDragContext * _gdk_broadway_window_drag_begin (GdkWindow *window,
						  GdkDevice *device,
						  GList     *targets);
gboolean _gdk_broadway_window_queue_antiexpose  (GdkWindow *window,
						 cairo_region_t *area);
void     _gdk_broadway_window_translate         (GdkWindow *window,
						 cairo_region_t *area,
						 gint       dx,
						 gint       dy);
gboolean _gdk_broadway_window_get_property (GdkWindow   *window,
					    GdkAtom      property,
					    GdkAtom      type,
					    gulong       offset,
					    gulong       length,
					    gint         pdelete,
					    GdkAtom     *actual_property_type,
					    gint        *actual_format_type,
					    gint        *actual_length,
					    guchar     **data);
void _gdk_broadway_window_change_property (GdkWindow    *window,
					   GdkAtom       property,
					   GdkAtom       type,
					   gint          format,
					   GdkPropMode   mode,
					   const guchar *data,
					   gint          nelements);
void _gdk_broadway_window_delete_property (GdkWindow *window,
					   GdkAtom    property);
gboolean _gdk_broadway_moveresize_handle_event   (GdkDisplay *display,
						  BroadwayInputMsg *msg);
gboolean _gdk_broadway_moveresize_configure_done (GdkDisplay *display,
						  GdkWindow  *window);


void     _gdk_broadway_selection_window_destroyed (GdkWindow *window);
void     _gdk_broadway_window_grab_check_destroy (GdkWindow *window);
void     _gdk_broadway_window_grab_check_unmap (GdkWindow *window,
						gulong     serial);

void _gdk_keymap_keys_changed     (GdkDisplay      *display);
gint _gdk_broadway_get_group_for_state (GdkDisplay      *display,
					GdkModifierType  state);
void _gdk_keymap_add_virtual_modifiers_compat (GdkKeymap       *keymap,
                                               GdkModifierType *modifiers);
gboolean _gdk_keymap_key_is_modifier   (GdkKeymap       *keymap,
					guint            keycode);

void _gdk_broadway_initialize_locale (void);

void _gdk_broadway_screen_events_init   (GdkScreen *screen);
GdkVisual *_gdk_broadway_screen_get_system_visual (GdkScreen * screen);
gint _gdk_broadway_screen_visual_get_best_depth (GdkScreen * screen);
GdkVisualType _gdk_broadway_screen_visual_get_best_type (GdkScreen * screen);
GdkVisual *_gdk_broadway_screen_get_system_visual (GdkScreen * screen);
GdkVisual*_gdk_broadway_screen_visual_get_best (GdkScreen * screen);
GdkVisual*_gdk_broadway_screen_visual_get_best_with_depth (GdkScreen * screen,
							   gint depth);
GdkVisual*_gdk_broadway_screen_visual_get_best_with_type (GdkScreen * screen,
							  GdkVisualType visual_type);
GdkVisual*_gdk_broadway_screen_visual_get_best_with_both (GdkScreen * screen,
							  gint          depth,
							  GdkVisualType visual_type);
void _gdk_broadway_screen_query_depths  (GdkScreen * screen,
					 gint **depths,
					 gint  *count);
void _gdk_broadway_screen_query_visual_types (GdkScreen * screen,
					      GdkVisualType **visual_types,
					      gint           *count);
GList *_gdk_broadway_screen_list_visuals (GdkScreen *screen);
void _gdk_broadway_screen_size_changed (GdkScreen *screen, 
					BroadwayInputScreenResizeNotify *msg);

void _gdk_broadway_events_got_input      (BroadwayInputMsg *message);

void _gdk_broadway_screen_init_root_window (GdkScreen *screen);
void _gdk_broadway_screen_init_visuals (GdkScreen *screen);
void _gdk_broadway_display_init_dnd (GdkDisplay *display);
GdkDisplay * _gdk_broadway_display_open (const gchar *display_name);
void _gdk_broadway_display_queue_events (GdkDisplay *display);
GdkDragProtocol _gdk_broadway_window_get_drag_protocol (GdkWindow *window,
							GdkWindow **target);
GdkCursor*_gdk_broadway_display_get_cursor_for_type (GdkDisplay    *display,
						     GdkCursorType  cursor_type);
GdkCursor*_gdk_broadway_display_get_cursor_for_name (GdkDisplay  *display,
						     const gchar *name);
GdkCursor *_gdk_broadway_display_get_cursor_for_pixbuf (GdkDisplay *display,
							GdkPixbuf  *pixbuf,
							gint        x,
							gint        y);
gboolean _gdk_broadway_display_supports_cursor_alpha (GdkDisplay *display);
gboolean _gdk_broadway_display_supports_cursor_color (GdkDisplay *display);
void _gdk_broadway_display_get_default_cursor_size (GdkDisplay *display,
						    guint       *width,
						    guint       *height);
void _gdk_broadway_display_get_maximal_cursor_size (GdkDisplay *display,
						    guint       *width,
						    guint       *height);
void       _gdk_broadway_display_before_process_all_updates (GdkDisplay *display);
void       _gdk_broadway_display_after_process_all_updates  (GdkDisplay *display);
void       _gdk_broadway_display_create_window_impl     (GdkDisplay    *display,
							 GdkWindow     *window,
							 GdkWindow     *real_parent,
							 GdkScreen     *screen,
							 GdkEventMask   event_mask,
							 GdkWindowAttr *attributes,
							 gint           attributes_mask);
gboolean _gdk_broadway_display_set_selection_owner (GdkDisplay *display,
						    GdkWindow  *owner,
						    GdkAtom     selection,
						    guint32     time,
						    gboolean    send_event);
GdkWindow * _gdk_broadway_display_get_selection_owner (GdkDisplay *display,
						       GdkAtom     selection);
gint _gdk_broadway_display_get_selection_property (GdkDisplay *display,
						   GdkWindow  *requestor,
						   guchar    **data,
						   GdkAtom    *ret_type,
						   gint       *ret_format);
void _gdk_broadway_display_send_selection_notify (GdkDisplay       *display,
						  GdkWindow       *requestor,
						  GdkAtom          selection,
						  GdkAtom          target,
						  GdkAtom          property, 
						  guint32          time);
void _gdk_broadway_display_convert_selection (GdkDisplay *display,
					      GdkWindow *requestor,
					      GdkAtom    selection,
					      GdkAtom    target,
					      guint32    time);
gint _gdk_broadway_display_text_property_to_utf8_list (GdkDisplay    *display,
						       GdkAtom        encoding,
						       gint           format,
						       const guchar  *text,
						       gint           length,
						       gchar       ***list);
gchar *_gdk_broadway_display_utf8_to_string_target (GdkDisplay  *display,
						    const gchar *str);
GdkKeymap* _gdk_broadway_display_get_keymap (GdkDisplay *display);
void _gdk_broadway_display_consume_all_input (GdkDisplay *display);
BroadwayInputMsg * _gdk_broadway_display_block_for_input (GdkDisplay *display,
							  char op,
							  guint32 serial,
							  gboolean remove);

/* Window methods - testing */
void     _gdk_broadway_window_sync_rendering    (GdkWindow       *window);
gboolean _gdk_broadway_window_simulate_key      (GdkWindow       *window,
						 gint             x,
						 gint             y,
						 guint            keyval,
						 GdkModifierType  modifiers,
						 GdkEventType     key_pressrelease);
gboolean _gdk_broadway_window_simulate_button   (GdkWindow       *window,
						 gint             x,
						 gint             y,
						 guint            button,
						 GdkModifierType  modifiers,
						 GdkEventType     button_pressrelease);
void _gdk_broadway_window_resize_surface        (GdkWindow *window);

void _gdk_broadway_cursor_update_theme (GdkCursor *cursor);
void _gdk_broadway_cursor_display_finalize (GdkDisplay *display);

#define GDK_SCREEN_DISPLAY(screen)    (GDK_BROADWAY_SCREEN (screen)->display)
#define GDK_WINDOW_SCREEN(win)	      (GDK_WINDOW_IMPL_BROADWAY (((GdkWindow *)win)->impl)->screen)
#define GDK_WINDOW_DISPLAY(win)       (GDK_BROADWAY_SCREEN (GDK_WINDOW_SCREEN (win))->display)
#define GDK_WINDOW_IS_BROADWAY(win)   (GDK_IS_WINDOW_IMPL_BROADWAY (((GdkWindow *)win)->impl))

#endif /* __GDK_PRIVATE_BROADWAY_H__ */
