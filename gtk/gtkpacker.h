/* GTK - The GIMP Toolkit 
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald 
 * 
 * GtkPacker Widget 
 * Copyright (C) 1998 Shawn T. Amundson, James S. Mitchell, Michael L. Staiger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_PACKER_H__
#define __GTK_PACKER_H__

#include <gtk/gtkcontainer.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define GTK_TYPE_PACKER            (gtk_packer_get_type ())
#define GTK_PACKER(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_PACKER, GtkPacker))
#define GTK_PACKER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_PACKER, GtkPackerClass))
#define GTK_IS_PACKER(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_PACKER))
#define GTK_IS_PACKER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PACKER))



typedef struct _GtkPacker           GtkPacker;
typedef struct _GtkPackerClass      GtkPackerClass;
typedef struct _GtkPackerChild      GtkPackerChild;

typedef enum
{
  G_NV (GTK_PACK_EXPAND,	pack-expand,	1 << 0),
  G_NV (GTK_FILL_X,		fill-x,		1 << 1),
  G_NV (GTK_FILL_Y,		fill-y,		1 << 2)
} G_FLAGS (GtkPackerOptions);

typedef enum
{
  G_SV (GTK_SIDE_TOP,		top),
  G_SV (GTK_SIDE_BOTTOM,	bottom),
  G_SV (GTK_SIDE_LEFT,		left),
  G_SV (GTK_SIDE_RIGHT,		right)
} G_ENUM (GtkSideType);

typedef enum
{
  G_SV (GTK_ANCHOR_CENTER,	center),
  G_SV (GTK_ANCHOR_NORTH,	north),
  G_SV (GTK_ANCHOR_NORTH_WEST,	north-west),
  G_SV (GTK_ANCHOR_NORTH_EAST,	north-east),
  G_SV (GTK_ANCHOR_SOUTH,	south),
  G_SV (GTK_ANCHOR_SOUTH_WEST,	south-west),
  G_SV (GTK_ANCHOR_SOUTH_EAST,	south-east),
  G_SV (GTK_ANCHOR_WEST,	west),
  G_SV (GTK_ANCHOR_EAST,	east),
  GTK_ANCHOR_N		=	GTK_ANCHOR_NORTH,
  GTK_ANCHOR_NW		=	GTK_ANCHOR_NORTH_WEST,
  GTK_ANCHOR_NE		=	GTK_ANCHOR_NORTH_EAST,
  GTK_ANCHOR_S		=	GTK_ANCHOR_SOUTH,
  GTK_ANCHOR_SW		=	GTK_ANCHOR_SOUTH_WEST,
  GTK_ANCHOR_SE		=	GTK_ANCHOR_SOUTH_EAST,
  GTK_ANCHOR_W		=	GTK_ANCHOR_WEST,
  GTK_ANCHOR_E		=	GTK_ANCHOR_EAST
} G_ENUM (GtkAnchorType);

struct _GtkPackerChild
{
  GtkWidget *widget;
  
  GtkAnchorType anchor;
  GtkSideType side;
  GtkPackerOptions options;
  
  guint use_default : 1;
  
  guint border_width;
  gint  padX;
  gint  padY;
  gint  iPadX;
  gint  iPadY;
};

struct _GtkPacker
{
  GtkContainer parent;
  
  GList *children;
  
  guint spacing;
  
  guint default_border_width;
  gint  default_padX;
  gint  default_padY;
  gint  default_iPadX;
  gint  default_iPadY;
};

struct _GtkPackerClass
{
  GtkContainerClass parent_class;
};


GtkType    gtk_packer_get_type		       (void);
GtkWidget* gtk_packer_new        	       (void);
void       gtk_packer_add_defaults	       (GtkPacker       *packer, 
						GtkWidget       *child,
						GtkSideType      side,
						GtkAnchorType    anchor,
						GtkPackerOptions options);
void       gtk_packer_add		       (GtkPacker       *packer, 
						GtkWidget       *child,
						GtkSideType      side,
						GtkAnchorType    anchor,
						GtkPackerOptions options,
						guint		 border_width, 
						gint		 padX, 
						gint		 padY,
						gint		 ipadX,
						gint		 ipadY);
void       gtk_packer_configure		       (GtkPacker	*packer, 
						GtkWidget       *child,
						GtkSideType      side,
						GtkAnchorType    anchor,
						GtkPackerOptions options,
						guint		 border_width, 
						gint		 padX, 
						gint		 padY,
						gint		 ipadX,
						gint		 ipadY);
void       gtk_packer_set_spacing	       (GtkPacker	*packer,
						guint		 spacing);
void       gtk_packer_set_default_border_width (GtkPacker	*packer,
						guint		 border);
void       gtk_packer_set_default_pad	       (GtkPacker	*packer,
						gint             padX,
						gint             padY);
void       gtk_packer_set_default_ipad	       (GtkPacker	*packer,
						gint             iPadX,
						gint             iPadY);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_PACKER_H__ */
