/*
 * Copyright (C) 2009 Carlos Garnacho  <carlosg@gnome.org>
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#ifndef __TEST_COORDINATE_WIDGET_H__
#define __TEST_COORDINATE_WIDGET_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TEST_TYPE_COORDINATE_WIDGET         (test_coordinate_widget_get_type ())
#define TEST_COORDINATE_WIDGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TEST_TYPE_COORDINATE_WIDGET, TestCoordinateWidget))
#define TEST_COORDINATE_WIDGET_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), TEST_TYPE_COORDINATE_WIDGET, TestCoordinateWidgetClass))
#define TEST_IS_COORDINATE_WIDGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TEST_TYPE_COORDINATE_WIDGET))
#define TEST_IS_COORDINATE_WIDGET_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), TEST_TYPE_COORDINATE_WIDGET))
#define TEST_COORDINATE_WIDGET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), TEST_TYPE_COORDINATE_WIDGET, TestCoordinateWidgetClass))

typedef struct TestCoordinateWidget TestCoordinateWidget;
typedef struct TestCoordinateWidgetClass TestCoordinateWidgetClass;

struct TestCoordinateWidget
{
  GtkDrawingArea parent_instance;
};

struct TestCoordinateWidgetClass
{
  GtkDrawingAreaClass parent_class;
};


GType       test_coordinate_widget_get_type (void) G_GNUC_CONST;

GtkWidget * test_coordinate_widget_new      (void);


G_END_DECLS

#endif /* __TEST_COORDINATE_WIDGET_H__ */
