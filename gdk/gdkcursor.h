#ifndef __GDK_CURSOR_H__
#define __GDK_CURSOR_H__

#include <gdk/gdktypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GDK_TYPE_CURSOR (gdk_cursor_get_type ())

/* Cursor types.
 */
typedef enum
{
#include <gdk/gdkcursors.h>
  GDK_LAST_CURSOR,
  GDK_CURSOR_IS_PIXMAP = -1
} GdkCursorType;

struct _GdkCursor
{
  GdkCursorType type;
  guint ref_count;
};

/* Cursors
 */

GType      gdk_cursor_get_type           (void);

GdkCursor* gdk_cursor_new		 (GdkCursorType	   cursor_type);
GdkCursor* gdk_cursor_new_from_pixmap	 (GdkPixmap	  *source,
					  GdkPixmap	  *mask,
					  GdkColor	  *fg,
					  GdkColor	  *bg,
					  gint		   x,
					  gint		   y);
GdkCursor* gdk_cursor_ref                (GdkCursor      *cursor);
void       gdk_cursor_unref              (GdkCursor      *cursor);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_CURSOR_H__ */
