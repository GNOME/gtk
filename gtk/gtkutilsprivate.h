#ifndef __GTKUTILS_H__
#define __GTKUTILS_H__

#include <gio/gio.h>

G_BEGIN_DECLS

gboolean        gtk_scan_string         (const char     **pos,
                                         GString         *out);
gboolean        gtk_skip_space          (const char     **pos);
gint            gtk_read_line           (FILE            *stream,
                                         GString         *str);
char *          gtk_trim_string         (const char      *str);
char **         gtk_split_file_list     (const char      *str);
GBytes         *gtk_file_load_bytes     (GFile           *file,
                                         GCancellable    *cancellable,
                                         GError         **error);

G_END_DECLS

#endif
