#pragma once

#include <gtk/gtk.h>

GdkTexture *load_heif_image (const char  *resource_path,
                             GString     *details,
                             GError     **error);
