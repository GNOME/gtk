/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_PROGRESS_BAR_H__
#define __GTK_PROGRESS_BAR_H__


#include <gdk/gdk.h>
#include <gtk/gtkprogress.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_PROGRESS_BAR            (gtk_progress_bar_get_type ())
#define GTK_PROGRESS_BAR(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_PROGRESS_BAR, GtkProgressBar))
#define GTK_PROGRESS_BAR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_PROGRESS_BAR, GtkProgressBarClass))
#define GTK_IS_PROGRESS_BAR(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_PROGRESS_BAR))
#define GTK_IS_PROGRESS_BAR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PROGRESS_BAR))
#define GTK_PROGRESS_BAR_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_PROGRESS_BAR, GtkProgressBarClass))


typedef struct _GtkProgressBar       GtkProgressBar;
typedef struct _GtkProgressBarClass  GtkProgressBarClass;

typedef enum
{
  GTK_PROGRESS_CONTINUOUS,
  GTK_PROGRESS_DISCRETE
} GtkProgressBarStyle;

typedef enum
{
  GTK_PROGRESS_LEFT_TO_RIGHT,
  GTK_PROGRESS_RIGHT_TO_LEFT,
  GTK_PROGRESS_BOTTOM_TO_TOP,
  GTK_PROGRESS_TOP_TO_BOTTOM
} GtkProgressBarOrientation;

struct _GtkProgressBar
{
  GtkProgress progress;

  GtkProgressBarStyle bar_style;
  GtkProgressBarOrientation orientation;

  guint blocks;
  gint  in_block;

  gint  activity_pos;
  guint activity_step;
  guint activity_blocks;

  gfloat pulse_fraction;
  
  guint activity_dir : 1;
};

struct _GtkProgressBarClass
{
  GtkProgressClass parent_class;
};


GtkType    gtk_progress_bar_get_type             (void) G_GNUC_CONST;
GtkWidget* gtk_progress_bar_new                  (void);

/*
 * GtkProgress/GtkProgressBar had serious problems in GTK 1.2.
 * 
 *  - Only 3 or 4 functions are really needed for 95% of progress  
 *    interfaces; GtkProgress[Bar] had about 25 functions, and 
 *    didn't even include these 3 or 4.
 *  - In activity mode, the API involves setting the adjustment 
 *    to any random value, just to have the side effect of 
 *    calling the progress bar update function - the adjustment
 *    is totally ignored in activity mode
 *  - You set the activity step as a pixel value, which means to 
 *    set the activity step you basically need to connect to 
 *    size_allocate
 *  - There are ctree_set_expander_style()-functions, to randomly 
 *    change look-and-feel for no good reason
 *  - The split between GtkProgress and GtkProgressBar makes no sense 
 *    to me whatsoever.
 *  
 * This was a big wart on GTK and made people waste lots of time,
 * both learning and using the interface.
 * 
 * So, I have added what I feel is the correct API, and marked all the
 * rest deprecated. However, the changes are 100% backward-compatible and
 * should break no existing code.
 *
 * The following 9 functions are the new programming interface. 
 */
void       gtk_progress_bar_pulse                (GtkProgressBar *pbar);
void       gtk_progress_bar_set_text             (GtkProgressBar *pbar,
                                                  const gchar    *text);
void       gtk_progress_bar_set_fraction         (GtkProgressBar *pbar,
                                                  gfloat          fraction);

void       gtk_progress_bar_set_pulse_step       (GtkProgressBar *pbar,
                                                  gfloat          fraction);
void       gtk_progress_bar_set_orientation      (GtkProgressBar *pbar,
						  GtkProgressBarOrientation orientation);

G_CONST_RETURN gchar* gtk_progress_bar_get_text       (GtkProgressBar *pbar);
gfloat                gtk_progress_bar_get_fraction   (GtkProgressBar *pbar);
gfloat                gtk_progress_bar_get_pulse_step (GtkProgressBar *pbar);

GtkProgressBarOrientation gtk_progress_bar_get_orientation (GtkProgressBar *pbar);


#ifndef GTK_DISABLE_DEPRECATED

/* Everything below here is deprecated */
GtkWidget* gtk_progress_bar_new_with_adjustment  (GtkAdjustment  *adjustment);
void       gtk_progress_bar_set_bar_style        (GtkProgressBar *pbar,
						  GtkProgressBarStyle style);
void       gtk_progress_bar_set_discrete_blocks  (GtkProgressBar *pbar,
						  guint           blocks);
/* set_activity_step() is not only deprecated, it doesn't even work.
 * (Of course, it wasn't usable anyway, you had to set it from a size_allocate
 * handler or something)
 */
void       gtk_progress_bar_set_activity_step    (GtkProgressBar *pbar,
                                                  guint           step);
void       gtk_progress_bar_set_activity_blocks  (GtkProgressBar *pbar,
						  guint           blocks);
void       gtk_progress_bar_update               (GtkProgressBar *pbar,
						  gfloat          percentage);

#endif /* GTK_DISABLE_DEPRECATED */

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_PROGRESS_BAR_H__ */
