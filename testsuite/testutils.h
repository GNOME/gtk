#pragma once

#include <glib.h>

char * diff_with_file (const char  *file1,
                       const char  *text,
                       gssize       len,
                       GError     **error);

