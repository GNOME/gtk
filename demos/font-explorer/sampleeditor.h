#pragma once

#include <gtk/gtk.h>


#define SAMPLE_EDITOR_TYPE (sample_editor_get_type ())
#define SAMPLE_EDITOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SAMPLE_EDITOR_TYPE, SampleEditor))


typedef struct _SampleEditor         SampleEditor;
typedef struct _SampleEditorClass    SampleEditorClass;


GType           sample_editor_get_type     (void);
