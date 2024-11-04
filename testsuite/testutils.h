#pragma once

#include <glib.h>

char *          diff_string_with_file                   (const char     *file1,
                                                         const char     *text,
                                                         gssize          len,
                                                         GError        **error);

char *          diff_bytes_with_file                    (const char     *file1,
                                                         GBytes         *bytes,
                                                         GError        **error);
