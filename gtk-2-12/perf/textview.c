#include <gtk/gtk.h>
#include "widgets.h"

GtkWidget *
text_view_new (void)
{
  GtkWidget *sw;
  GtkWidget *text_view;
  GtkTextBuffer *buffer;

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

  text_view = gtk_text_view_new ();
  gtk_widget_set_size_request (text_view, 400, 300);
  gtk_container_add (GTK_CONTAINER (sw), text_view);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

  gtk_text_buffer_set_text (buffer,
			    "In felaweshipe, and pilgrimes were they alle,\n"
			    "That toward Caunterbury wolden ryde.\n"
			    "The chambres and the stables weren wyde,\n"
			    "And wel we weren esed atte beste;\n"
			    "And shortly, whan the sonne was to reste,\n"
			    "\n"
			    "So hadde I spoken with hem everychon \n"
			    "That I was of hir felaweshipe anon, \n"
			    "And made forward erly for to ryse \n"
			    "To take our wey, ther as I yow devyse. \n"
			    "   But nathelees, whil I have tyme and space, \n"
			    " \n"
			    "Er that I ferther in this tale pace, \n"
			    "Me thynketh it acordaunt to resoun \n"
			    "To telle yow al the condicioun \n"
			    "Of ech of hem, so as it semed me, \n"
			    "And whiche they weren, and of what degree, \n"
			    " \n"
			    "And eek in what array that they were inne; \n"
			    "And at a knyght than wol I first bigynne. \n"
			    "   A knyght ther was, and that a worthy man, \n"
			    "That fro the tyme that he first bigan \n"
			    "To riden out, he loved chivalrie, \n"
			    " \n"
			    "Trouthe and honour, fredom and curteisie. \n"
			    "Ful worthy was he in his lordes werre, \n"
			    " \n"
			    "And therto hadde he riden, no man ferre, \n"
			    "As wel in Cristendom as in Hethenesse, \n"
			    "And evere honoured for his worthynesse. \n"
			    " \n"
			    "   At Alisaundre he was, whan it was wonne; \n"
			    "Ful ofte tyme he hadde the bord bigonne \n"
			    "Aboven alle nacions in Pruce; \n"
			    "In Lettow hadde he reysed, and in Ruce, \n"
			    "No cristen man so ofte of his degree. \n",
			    -1);

  return sw;
}
