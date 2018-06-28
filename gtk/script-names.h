#ifndef SCRIPT_NAMES_H
#define SCRIPT_NAMES_H

#include <glib.h>

G_BEGIN_DECLS

const char * get_script_name (GUnicodeScript script);
const char * get_script_name_for_tag (guint32 tag);

G_END_DECLS

#endif
