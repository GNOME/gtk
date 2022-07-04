#pragma once

#include <gtk/gtk.h>


#define SAMPLE_CHOOSER_TYPE (sample_chooser_get_type ())
#define SAMPLE_CHOOSER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SAMPLE_CHOOSER_TYPE, SampleChooser))


typedef struct _SampleChooser         SampleChooser;
typedef struct _SampleChooserClass    SampleChooserClass;


GType           sample_chooser_get_type (void);
SampleChooser * sample_chooser_new      (void);
