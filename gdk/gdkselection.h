#ifndef __GDK_SELECTION_H__
#define __GDK_SELECTION_H__

#include <gdk/gdktypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Predefined atoms relating to selections. In general, one will need to use
 * gdk_intern_atom
 */
#define GDK_SELECTION_PRIMARY 		_GDK_MAKE_ATOM (1)
#define GDK_SELECTION_SECONDARY 	_GDK_MAKE_ATOM (2)
#define GDK_SELECTION_CLIPBOARD 	_GDK_MAKE_ATOM (69)
#define GDK_TARGET_BITMAP 		_GDK_MAKE_ATOM (5)
#define GDK_TARGET_COLORMAP 		_GDK_MAKE_ATOM (7)
#define GDK_TARGET_DRAWABLE 		_GDK_MAKE_ATOM (17)
#define GDK_TARGET_PIXMAP 		_GDK_MAKE_ATOM (20)
#define GDK_TARGET_STRING 		_GDK_MAKE_ATOM (31)
#define GDK_SELECTION_TYPE_ATOM 	_GDK_MAKE_ATOM (4)
#define GDK_SELECTION_TYPE_BITMAP 	_GDK_MAKE_ATOM (5)
#define GDK_SELECTION_TYPE_COLORMAP 	_GDK_MAKE_ATOM (7)
#define GDK_SELECTION_TYPE_DRAWABLE 	_GDK_MAKE_ATOM (17)
#define GDK_SELECTION_TYPE_INTEGER 	_GDK_MAKE_ATOM (19)
#define GDK_SELECTION_TYPE_PIXMAP 	_GDK_MAKE_ATOM (20)
#define GDK_SELECTION_TYPE_WINDOW 	_GDK_MAKE_ATOM (33)
#define GDK_SELECTION_TYPE_STRING 	_GDK_MAKE_ATOM (31)

#ifndef GDK_DISABLE_DEPRECATED

typedef GdkAtom GdkSelection;
typedef GdkAtom GdkTarget;
typedef GdkAtom GdkSelectionType;

#endif /* GDK_DISABLE_DEPRECATED */

/* Selections
 */

#ifndef GDK_MULTIHEAD_SAFE
gboolean   gdk_selection_owner_set (GdkWindow	 *owner,
				    GdkAtom	  selection,
				    guint32	  time_,
				    gboolean      send_event);
GdkWindow* gdk_selection_owner_get (GdkAtom	  selection);
#endif/* GDK_MULTIHEAD_SAFE */

gboolean   gdk_selection_owner_set_for_display (GdkDisplay *display,
						GdkWindow  *owner,
						GdkAtom     selection,
						guint32     time_,
						gboolean    send_event);
GdkWindow *gdk_selection_owner_get_for_display (GdkDisplay *display,
						GdkAtom     selection);

void	   gdk_selection_convert   (GdkWindow	 *requestor,
				    GdkAtom	  selection,
				    GdkAtom	  target,
				    guint32	  time_);
gboolean   gdk_selection_property_get (GdkWindow  *requestor,
				       guchar	 **data,
				       GdkAtom	  *prop_type,
				       gint	  *prop_format);

#ifndef GDK_MULTIHEAD_SAFE
void	   gdk_selection_send_notify (guint32	    requestor,
				      GdkAtom	    selection,
				      GdkAtom	    target,
				      GdkAtom	    property,
				      guint32	    time_);
#endif /* GDK_MULTIHEAD_SAFE */

void       gdk_selection_send_notify_for_display (GdkDisplay *display,
						  guint32     requestor,
						  GdkAtom     selection,
						  GdkAtom     target,
						  GdkAtom     property,
						  guint32     time_);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_SELECTION_H__ */
