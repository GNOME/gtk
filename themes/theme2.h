#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "gtkthemes.h"
#include "gtkprivate.h"
#include "gtkbutton.h"
#include "gtkcheckbutton.h"
#include "gtkwindow.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include <gdk_imlib.h>

typedef struct _themebuttonborder
{
   char           *filename;
   GdkImlibImage  *image;
   GdkImlibBorder  border;
} ThemeButtonBorder;

typedef struct _themebuttonbackground
{
   char           *filename;
   GdkImlibColor   color;
   GdkImlibImage  *image;
   GdkImlibBorder  border;
   char            scale_to_fit;
   char            tile_relative_to_parent;
} ThemeButtonBackground;

typedef struct _themebuttondecoration
{
   char           *filename;
   GdkImlibImage  *image;
   char            simplegeom;
   int             xrel,xabs,yrel,yabs;
   int             x2rel,x2abs,y2rel,y2abs;
} ThemeButtonDecoration;

typedef struct _themebuttonconfig
{
   /* Border between outside of button and any children (eg label) */
   GdkImlibBorder         button_padding;
   int                    min_w,min_h;
   ThemeButtonBorder      border;
   ThemeButtonBackground  background;
   int                    number_of_decorations;
   ThemeButtonDecoration  *decoration;
} ThemeButtonConfig;

typedef struct _themewindowconfig
{
   /* Border between outside of button and any children (eg label) */
   GdkImlibBorder         window_padding;
   int                    min_w,min_h;
   ThemeButtonBorder      border;
   ThemeButtonBackground  background;
   int                    number_of_decorations;
   ThemeButtonDecoration  *decoration;
} ThemeWindowConfig;

typedef struct _themeconfig
{
   ThemeButtonConfig buttonconfig[3][5][2];
   ThemeWindowConfig windowconfig;
} ThemeConfig;
