#pragma once

#include <gtk/gtklayoutmanager.h>

G_BEGIN_DECLS

#define GTK_TYPE_LEGACY_LAYOUT (gtk_legacy_layout_get_type ())

typedef GtkSizeRequestMode (* GtkLegacyRequestModeFunc) (GtkWidget *widget);

typedef void (* GtkLegacyMeasureFunc) (GtkWidget      *widget,
                                       GtkOrientation  orientation,
                                       int             for_size,
                                       int            *minimum,
                                       int            *natural,
                                       int            *minimum_baseline,
                                       int            *natural_baseline);

typedef void (* GtkLegacyAllocateFunc) (GtkWidget *widget,
                                        int        width,
                                        int        height,
                                        int        baseline);

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkLegacyLayout, gtk_legacy_layout, GTK, LEGACY_LAYOUT, GtkLayoutManager)

GDK_AVAILABLE_IN_ALL
GtkLayoutManager *
gtk_legacy_layout_new (GtkLegacyRequestModeFunc request_mode,
                       GtkLegacyMeasureFunc measure,
                       GtkLegacyAllocateFunc allocate);

G_END_DECLS
