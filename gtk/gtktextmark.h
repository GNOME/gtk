#ifndef GTK_TEXT_MARK_H
#define GTK_TEXT_MARK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* The GtkTextMark data type */

typedef struct _GtkTextMark GtkTextMark;

void gtk_text_mark_set_visible (GtkTextMark *mark,
                                 gboolean setting);

gboolean gtk_text_mark_is_visible (GtkTextMark *mark);
/* Temporarily commented out until memory management behavior is figured out */
/* char *   gtk_text_mark_get_name   (GtkTextMark *mark); */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


