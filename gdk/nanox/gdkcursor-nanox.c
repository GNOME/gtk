#include "gdk.h"
#include "gdkprivate-nanox.h"

GdkCursor*
gdk_cursor_new (GdkCursorType cursor_type) {
		g_message("unimplemented %s", __FUNCTION__);
	return NULL;
}

GdkCursor*
gdk_cursor_new_from_pixmap (GdkPixmap *source,
			    GdkPixmap *mask,
			    GdkColor  *fg,
			    GdkColor  *bg,
			    gint       x,
			    gint       y)
{
		g_message("unimplemented %s", __FUNCTION__);
		return NULL;
}

void
_gdk_cursor_destroy (GdkCursor *cursor)
{
}

