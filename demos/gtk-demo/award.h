#ifndef __AWARD_H__
#define __AWARD_H__

#include <gtk/gtk.h>

#define GTK_TYPE_AWARD (gtk_award_get_type ())

G_DECLARE_FINAL_TYPE (GtkAward, gtk_award, GTK, AWARD, GObject)

GListModel *    gtk_award_get_list                      (void);

const char *    gtk_award_get_name                      (GtkAward               *award);
const char *    gtk_award_get_title                     (GtkAward               *award);
GDateTime *     gtk_award_get_granted                   (GtkAward               *award);

void            award                                   (const char             *name);

#endif /* __AWARD_H__ */
