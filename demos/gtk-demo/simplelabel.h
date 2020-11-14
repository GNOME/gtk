#pragma once

#include <gtk/gtk.h>

#define SIMPLE_TYPE_LABEL (simple_label_get_type ())
G_DECLARE_FINAL_TYPE (SimpleLabel, simple_label, SIMPLE, LABEL, GtkWidget)

GtkWidget * simple_label_new           (void);

void        simple_label_set_text      (SimpleLabel *self,
                                        const char  *text);
void        simple_label_set_min_chars (SimpleLabel *self,
                                        int          min_chars);
void        simple_label_set_nat_chars (SimpleLabel *self,
                                        int          nat_chars);
