/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkQueryTips: Query onscreen widgets for their tooltips
 * Copyright (C) 1998 Tim Janik
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
#ifndef __GTKTIPSQUERY_H__
#define __GTKTIPSQUERY_H__


#include <gtk/gtklabel.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


/* --- type macros --- */
#define GTK_TIPS_QUERY(obj)		(GTK_CHECK_CAST (obj, gtk_tips_query_get_type (), GtkTipsQuery))
#define GTK_TIPS_QUERY_CLASS(klass)	(GTK_CHECK_CLASS_CAST (klass, gtk_tips_query_get_type (), GtkTipsQueryClass))
#define GTK_IS_TIPS_QUERY(obj)		(GTK_CHECK_TYPE (obj, gtk_tips_query_get_type ()))


/* --- typedefs --- */
typedef	struct	_GtkTipsQuery		GtkTipsQuery;
typedef	struct	_GtkTipsQueryClass	GtkTipsQueryClass;


/* --- structures --- */
struct	_GtkTipsQuery
{
  GtkLabel	label;

  guint		emit_always : 1;
  guint		in_query : 1;
  gchar		*label_inactive;
  gchar		*label_no_tip;

  GtkWidget	*caller;
  GtkWidget	*last_crossed;

  GdkCursor	*query_cursor;
};

struct	_GtkTipsQueryClass
{
  GtkLabelClass			parent_class;

  void	(*start_query)		(GtkTipsQuery	*tips_query);
  void	(*stop_query)		(GtkTipsQuery	*tips_query);
  void	(*widget_entered)	(GtkTipsQuery	*tips_query,
				 GtkWidget	*widget,
				 const gchar	*tip_text,
				 const gchar	*tip_private);
  gint	(*widget_selected)	(GtkTipsQuery	*tips_query,
				 GtkWidget	*widget,
				 const gchar	*tip_text,
				 const gchar	*tip_private,
				 GdkEventButton	*event);
};


/* --- prototypes --- */
GtkType		gtk_tips_query_get_type		(void);
GtkWidget*	gtk_tips_query_new		(void);
void		gtk_tips_query_start_query	(GtkTipsQuery	*tips_query);
void		gtk_tips_query_stop_query	(GtkTipsQuery	*tips_query);
void		gtk_tips_query_set_caller	(GtkTipsQuery	*tips_query,
						 GtkWidget	*caller);
void		gtk_tips_query_set_labels 	(GtkTipsQuery   *tips_query,
						 const gchar    *label_inactive,
						 const gchar    *label_no_tip);
     


#ifdef __cplusplus
#pragma {
}
#endif /* __cplusplus */


#endif	/* __GTKTIPSQUERY_H__ */
