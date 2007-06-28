/* GTK - The GIMP Toolkit
 * Copyright (C) 2007 Mathias Hasselmann <mathias.hasselmann@gmx.de>
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

#ifndef __GTK_EXTENDED_LAYOUT_H__
#define __GTK_EXTENDED_LAYOUT_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_EXTENDED_LAYOUT            (gtk_extended_layout_get_type ())
#define GTK_EXTENDED_LAYOUT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_EXTENDED_LAYOUT, GtkExtendedLayout))
#define GTK_EXTENDED_LAYOUT_CLASS(klass)    ((GtkExtendedLayoutIface*)g_type_interface_peek ((klass), GTK_TYPE_EXTENDED_LAYOUT))
#define GTK_IS_EXTENDED_LAYOUT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_EXTENDED_LAYOUT))
#define GTK_EXTENDED_LAYOUT_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GTK_TYPE_EXTENDED_LAYOUT, GtkExtendedLayoutIface))

#define GTK_EXTENDED_LAYOUT_HAS_WIDTH_FOR_HEIGHT(obj) \
  (gtk_extended_layout_get_features (GTK_EXTENDED_LAYOUT ((obj))) & \
   GTK_EXTENDED_LAYOUT_WIDTH_FOR_HEIGHT)
#define GTK_EXTENDED_LAYOUT_HAS_HEIGHT_FOR_WIDTH(obj) \
  (gtk_extended_layout_get_features (GTK_EXTENDED_LAYOUT ((obj))) & \
   GTK_EXTENDED_LAYOUT_HEIGHT_FOR_WIDTH)
#define GTK_EXTENDED_LAYOUT_HAS_NATURAL_SIZE(obj) \
  (gtk_extended_layout_get_features (GTK_EXTENDED_LAYOUT ((obj))) & \
   GTK_EXTENDED_LAYOUT_NATURAL_SIZE)
#define GTK_EXTENDED_LAYOUT_HAS_BASELINES(obj) \
  (gtk_extended_layout_get_features (GTK_EXTENDED_LAYOUT ((obj))) & \
   GTK_EXTENDED_LAYOUT_BASELINES)
#define GTK_EXTENDED_LAYOUT_HAS_PADDING(obj) \
  (gtk_extended_layout_get_features (GTK_EXTENDED_LAYOUT ((obj))) & \
   GTK_EXTENDED_LAYOUT_PADDING)

typedef struct _GtkExtendedLayout           GtkExtendedLayout;
typedef struct _GtkExtendedLayoutIface      GtkExtendedLayoutIface;

/*< flags >*/
typedef enum {
  GTK_EXTENDED_LAYOUT_NONE             = 0,
  GTK_EXTENDED_LAYOUT_WIDTH_FOR_HEIGHT = 1 << 0, 
  GTK_EXTENDED_LAYOUT_HEIGHT_FOR_WIDTH = 1 << 1,
  GTK_EXTENDED_LAYOUT_NATURAL_SIZE     = 1 << 2,
  GTK_EXTENDED_LAYOUT_BASELINES        = 1 << 3,
  GTK_EXTENDED_LAYOUT_PADDING          = 1 << 4
} GtkExtendedLayoutFeatures;

struct _GtkExtendedLayoutIface
{
  GTypeInterface g_iface;

  /* virtual table */

  GtkExtendedLayoutFeatures (*get_features)         (GtkExtendedLayout  *layout);
  gint                      (*get_height_for_width) (GtkExtendedLayout  *layout,
                                                     gint                width);
  gint                      (*get_width_for_height) (GtkExtendedLayout  *layout,
                                                     gint                height);
  void                      (*get_natural_size)     (GtkExtendedLayout  *layout,
                                                     GtkRequisition     *requisition);
  gint                      (*get_baselines)        (GtkExtendedLayout  *layout,
                                                     gint              **baselines);
  void                      (*get_padding)          (GtkExtendedLayout  *layout,
                                                     GtkBorder          *padding);
};


GType                     gtk_extended_layout_get_type             (void) G_GNUC_CONST;
GtkExtendedLayoutFeatures gtk_extended_layout_get_features         (GtkExtendedLayout  *layout);
gint                      gtk_extended_layout_get_height_for_width (GtkExtendedLayout  *layout,
                                                                    gint                width);
gint                      gtk_extended_layout_get_width_for_height (GtkExtendedLayout  *layout,
                                                                    gint                height);
void                      gtk_extended_layout_get_natural_size     (GtkExtendedLayout  *layout,
                                                                    GtkRequisition     *requisition);
gint                      gtk_extended_layout_get_baselines        (GtkExtendedLayout  *layout,
                                                                    gint              **baselines);
gint                      gtk_extended_layout_get_single_baseline  (GtkExtendedLayout  *layout,
                                                                    GtkBaselinePolicy   policy);
void                      gtk_extended_layout_get_padding          (GtkExtendedLayout  *layout,
                                                                    GtkBorder          *padding);

G_END_DECLS

#endif /* __GTK_EXTENDED_LAYOUT_H__ */
