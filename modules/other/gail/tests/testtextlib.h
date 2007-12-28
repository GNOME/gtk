#include <stdio.h>
#include <gtk/gtk.h>
#include <atk/atk.h>
#include "testlib.h"

void add_handlers (AtkObject *obj);
gint setup_gui (AtkObject *obj, TLruntest test);
void runtest(AtkObject *obj, gint win_num);
void _run_offset_test(AtkObject *obj, char * type, gint param_int1, AtkTextBoundary boundary);
void _notify_caret_handler (GObject *obj, int position);
void _notify_text_insert_handler (GObject *obj,
  int start_offset, int end_offset);
void _notify_text_delete_handler (GObject *obj,
  int start_offset, int end_offset);                                 

