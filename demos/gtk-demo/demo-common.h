#ifndef __DEMO_COMMON_H__
#define __DEMO_COMMON_H__

G_BEGIN_DECLS

gchar *demo_find_file (const gchar  *base,
		       GError      **err);

GtkWidget *get_cached_widget	  (GtkWidget *widget,
				   gchar     *key);
void cache_widget		  (GtkWidget *widget,
				   gchar     *key);
gpointer get_cached_pointer	  (GtkWidget *widget,
				   gchar     *key);
void  cache_pointer		  (GtkWidget *widget,
				   gchar     *key,
				   gpointer  data);
void remove_cached_widget	  (GtkWidget *widget, 
				   gchar *key);
G_END_DECLS

#endif /* __DEMO_COMMON_H__ */
