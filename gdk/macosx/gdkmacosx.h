


#ifndef __GDK_MACOSX_H__
#define __GDK_MACOSX_H__

#include <gdk/gdk.h>
#include <Cocoa/Cocoa.h>

#define GDK_WINDOW_NSVIEW(win) (GDK_WINDOW_IMPL_MACOSX(win)->v)


GdkWindow *gdk_nsview_table_lookup_for_display(GdkDisplay     *display,
											   GdkNativeWindow anid);
void _gdk_nsview_table_insert (GdkDisplay     *display,
								   GdkNativeWindow anid,
								   GdkWindow      *window);


inline static GdkRectangle ns_to_gdkrect(NSRect r)
{
	GdkRectangle dest;
	dest.x = r.origin.x;
	dest.y = r.origin.y;
	dest.width = r.size.width;
	dest.height = r.size.height;

	return dest;
}

inline static GdkModifierType ns_to_gdk_modifier(int modifier)
{
	GdkModifierType dest = 0;

	if (modifier & NSAlphaShiftKeyMask) {
		dest |= GDK_LOCK_MASK;
	}
	if (modifier & NSShiftKeyMask) {
		dest |= GDK_SHIFT_MASK;
	}
	if (modifier & NSControlKeyMask) {
		dest |= GDK_CONTROL_MASK;
	}
	if (modifier & NSAlternateKeyMask) {
		dest |= GDK_MOD1_MASK;
	}
	if (modifier & NSCommandKeyMask) {
		dest != GDK_MOD2_MASK;
	}

	return dest;
}

#endif

