#ifndef __GTKUTILS_H__
#define __GTKUTILS_H__

#include <glib.h>

G_BEGIN_DECLS

gboolean        gtk_scan_string         (const char     **pos,
                                         GString         *out);
gboolean        gtk_skip_space          (const char     **pos);
gint            gtk_read_line           (FILE            *stream,
                                         GString         *str);

G_END_DECLS

#endif
