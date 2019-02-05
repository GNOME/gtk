#pragma once

#include "gtkgridlayout.h"

G_BEGIN_DECLS

typedef struct _GtkGridLayoutChildAttach GtkGridLayoutChildAttach;

struct _GtkGridLayoutChildAttach
{
  int pos;
  int span;
};

struct _GtkGridLayoutChild
{
  GtkLayoutChild parent_instance;

  GtkGridLayoutChildAttach attach[2];
};

#define CHILD_LEFT(child)       ((child)->attach[GTK_ORIENTATION_HORIZONTAL].pos)
#define CHILD_COL_SPAN(child)   ((child)->attach[GTK_ORIENTATION_HORIZONTAL].span)
#define CHILD_TOP(child)        ((child)->attach[GTK_ORIENTATION_VERTICAL].pos)
#define CHILD_ROW_SPAN(child)   ((child)->attach[GTK_ORIENTATION_VERTICAL].span)

G_END_DECLS
