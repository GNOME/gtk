#ifndef GTK_TEXT_TAG_TABLE_H
#define GTK_TEXT_TAG_TABLE_H

#include <gtk/gtktexttag.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtktexttag.h>

typedef void (* GtkTextTagTableForeach) (GtkTextTag *tag, gpointer data);

#define GTK_TYPE_TEXT_TAG_TABLE            (gtk_text_tag_table_get_type ())
#define GTK_TEXT_TAG_TABLE(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_TEXT_TAG_TABLE, GtkTextTagTable))
#define GTK_TEXT_TAG_TABLE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_TAG_TABLE, GtkTextTagTableClass))
#define GTK_IS_TEXT_TAG_TABLE(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_TEXT_TAG_TABLE))
#define GTK_IS_TEXT_TAG_TABLE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_TAG_TABLE))
#define GTK_TEXT_TAG_TABLE_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_TEXT_TAG_TABLE, GtkTextTagTableClass))

typedef struct _GtkTextTagTableClass GtkTextTagTableClass;

struct _GtkTextTagTable {
  GtkObject parent_instance;

  GHashTable *hash;
  GSList *anonymous;
  gint anon_count;
};

struct _GtkTextTagTableClass {
  GtkObjectClass parent_class;

  void (* tag_changed) (GtkTextTagTable *table, GtkTextTag *tag, gboolean size_changed);
  void (* tag_added) (GtkTextTagTable *table, GtkTextTag *tag);
  void (* tag_removed) (GtkTextTagTable *table, GtkTextTag *tag);
};

GtkType          gtk_text_tag_table_get_type (void) G_GNUC_CONST;

GtkTextTagTable *gtk_text_tag_table_new      (void);
void             gtk_text_tag_table_add      (GtkTextTagTable        *table,
                                              GtkTextTag             *tag);
void             gtk_text_tag_table_remove   (GtkTextTagTable        *table,
                                              GtkTextTag             *tag);
GtkTextTag      *gtk_text_tag_table_lookup   (GtkTextTagTable        *table,
                                              const gchar            *name);
void             gtk_text_tag_table_foreach  (GtkTextTagTable        *table,
                                              GtkTextTagTableForeach  func,
                                              gpointer                data);
guint            gtk_text_tag_table_size     (GtkTextTagTable        *table);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

