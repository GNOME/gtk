#pragma once

#include <glib.h>
#include <gsk/gsk.h>

char *          diff_bytes                              (const char     *file,
                                                         GBytes         *input1,
                                                         GBytes         *input2);
char *          diff_string_with_file                   (const char     *file1,
                                                         const char     *text,
                                                         gssize          len,
                                                         GError        **error);

char *          diff_bytes_with_file                    (const char     *file1,
                                                         GBytes         *bytes,
                                                         GError        **error);
char *          diff_node_with_file                     (const char     *file1,
                                                         GskRenderNode  *node,
                                                         GError        **error);
