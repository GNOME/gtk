#pragma once

#include <gtk/gtk.h>


#define RANGE_EDIT_TYPE (range_edit_get_type ())
#define RANGE_EDIT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RANGE_EDIT_TYPE, RangeEdit))


typedef struct _RangeEdit         RangeEdit;
typedef struct _RangeEditClass    RangeEditClass;


GType       range_edit_get_type (void);
RangeEdit * range_edit_new      (void);
