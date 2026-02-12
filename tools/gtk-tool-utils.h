#pragma once

#include <gdk/gdk.h>

GdkTexture * load_image_file (const char *filename);


gboolean        find_format_by_name      (const char      *name,
                                          GdkMemoryFormat *format);
char **         get_format_names         (void);
GdkColorState * find_color_state_by_name (const char      *name);

char **         get_color_state_names    (void);
char *          get_color_state_name     (GdkColorState   *color_state);
char *          get_color_state_cicp     (GdkColorState   *color_state);

GdkColorState  *parse_cicp_tuple         (const char      *cicp_tuple,
                                          GError         **error);
