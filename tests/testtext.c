#include <gtk/gtk.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

static gint
blink_timeout(gpointer data)
{
  GtkTextTag *tag;
  static gboolean flip = FALSE;
  
  tag = GTK_TEXT_TAG(data);

  gtk_object_set(GTK_OBJECT(tag),
                 "foreground", flip ? "blue" : "purple",
                 NULL);

  flip = !flip;

  return TRUE;
}

static gint
tag_event_handler(GtkTextTag *tag, GtkWidget *widget, GdkEvent *event,
                  const GtkTextIter *iter, gpointer user_data)
{
  gint char_index;

  char_index = gtk_text_iter_get_char_index(iter);
  
  switch (event->type)
    {
    case GDK_MOTION_NOTIFY:
      printf("Motion event at char %d tag `%s'\n",
             char_index, tag->name);
      break;
        
    case GDK_BUTTON_PRESS:
      printf("Button press at char %d tag `%s'\n",
             char_index, tag->name);
      break;
        
    case GDK_2BUTTON_PRESS:
      printf("Double click at char %d tag `%s'\n",
             char_index, tag->name);
      break;
        
    case GDK_3BUTTON_PRESS:
      printf("Triple click at char %d tag `%s'\n",
             char_index, tag->name);
      break;
        
    case GDK_BUTTON_RELEASE:
      printf("Button release at char %d tag `%s'\n",
             char_index, tag->name);
      break;
        
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_PROPERTY_NOTIFY:
    case GDK_SELECTION_CLEAR:
    case GDK_SELECTION_REQUEST:
    case GDK_SELECTION_NOTIFY:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
    default:
      break;
    }

  return FALSE;
}

static void
setup_tag(GtkTextTag *tag)
{

  gtk_signal_connect(GTK_OBJECT(tag),
                     "event",
                     GTK_SIGNAL_FUNC(tag_event_handler),
                     NULL);
}

static gint
delete_event_cb(GtkWidget *window, GdkEventAny *event, gpointer data)
{
  gtk_main_quit();
  return TRUE;
}

static void
create_window(GtkTextBuffer *buffer)
{
  GtkWidget *window;
  GtkWidget *sw;
  GtkWidget *tkxt;
  
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect(GTK_OBJECT(window), "delete_event",
                     GTK_SIGNAL_FUNC(delete_event_cb), NULL);
  
  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);

  tkxt = gtk_text_view_new_with_buffer(buffer);

  gtk_container_add(GTK_CONTAINER(window), sw);
  gtk_container_add(GTK_CONTAINER(sw), tkxt);

  gtk_window_set_default_size(GTK_WINDOW(window),
                              500, 500);

  gtk_widget_grab_focus(tkxt);
  
  gtk_widget_show_all(window);
}

static char  *book_closed_xpm[] = {
"16 16 6 1",
"       c None s None",
".      c black",
"X      c red",
"o      c yellow",
"O      c #808080",
"#      c white",
"                ",
"       ..       ",
"     ..XX.      ",
"   ..XXXXX.     ",
" ..XXXXXXXX.    ",
".ooXXXXXXXXX.   ",
"..ooXXXXXXXXX.  ",
".X.ooXXXXXXXXX. ",
".XX.ooXXXXXX..  ",
" .XX.ooXXX..#O  ",
"  .XX.oo..##OO. ",
"   .XX..##OO..  ",
"    .X.#OO..    ",
"     ..O..      ",
"      ..        ",
"                "};

int
main(int argc, char** argv)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter, iter2;
  gchar *str;
  GtkTextTag *tag;
  GdkColor color;
  GdkColor color2;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  int i;
  
  gtk_init(&argc, &argv);
  
  buffer = gtk_text_buffer_new(NULL);

  if (argv[1])
    {
      FILE* f;

      f = fopen(argv[1], "r");

      if (f == NULL)
        {
          fprintf(stderr, "Failed to open %s: %s\n",
                  argv[1], g_strerror(errno));
          exit(1);
        }

      while (TRUE)
        {
          gchar buf[2048];
          gchar *s;

          s = fgets(buf, 2048, f);

          if (s == NULL)
            break;
          
          gtk_text_buffer_insert_after_line(buffer, -1, buf, -1);
        }
    }
  else
    {      
      tag = gtk_text_buffer_create_tag(buffer, "fg_blue");

      /*       gtk_timeout_add(1000, blink_timeout, tag); */
      
      setup_tag(tag);
      
      color.red = color.green = 0;
      color.blue = 0xffff;
      color2.red = 0xfff;
      color2.blue = 0x0;
      color2.green = 0;
      gtk_object_set(GTK_OBJECT(tag),
                     "foreground_gdk", &color,
                     "background_gdk", &color2,
                     "font", "-*-courier-bold-r-*-*-30-*-*-*-*-*-*-*",
                     NULL);

      tag = gtk_text_buffer_create_tag(buffer, "fg_red");

      setup_tag(tag);
      
      color.blue = color.green = 0;
      color.red = 0xffff;
      gtk_object_set(GTK_OBJECT(tag),
                     "offset", -4,
                     "foreground_gdk", &color,
                     NULL);

      tag = gtk_text_buffer_create_tag(buffer, "bg_green");

      setup_tag(tag);
      
      color.blue = color.red = 0;
      color.green = 0xffff;
      gtk_object_set(GTK_OBJECT(tag),
                     "background_gdk", &color,
                     "font", "-*-courier-bold-r-*-*-10-*-*-*-*-*-*-*",
                     NULL);

      tag = gtk_text_buffer_create_tag(buffer, "overstrike");

      setup_tag(tag);
      
      gtk_object_set(GTK_OBJECT(tag),
                     "overstrike", TRUE,
                     NULL);


      tag = gtk_text_buffer_create_tag(buffer, "underline");

      setup_tag(tag);
      
      gtk_object_set(GTK_OBJECT(tag),
                     "underline", TRUE,
                     NULL);


      pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL,
                                                      gtk_widget_get_default_colormap(),
                                                      &mask,
                                                      NULL, book_closed_xpm);

      g_assert(pixmap != NULL);
      
      i = 0;
      while (i < 1000)
        {
          gtk_text_buffer_get_iter_at_char(buffer, &iter, 0);
          
          gtk_text_buffer_insert_pixmap (buffer, &iter, pixmap, mask);
          
          str = g_strdup_printf("%d Hello World! blah blah blah blah blah blah blah blah blah blah blah blah\nwoo woo woo woo woo woo woo woo woo woo woo woo woo woo woo\n",
                                i);
      
          gtk_text_buffer_insert(buffer, &iter, str, -1);

          g_free(str);
      
          gtk_text_buffer_get_iter_at_line_char(buffer, &iter, 0, 5);
          
          gtk_text_buffer_insert(buffer, &iter,
                                  "(Hello World!)\nfoo foo Hello this is some text we are using to text word wrap. It has punctuation! gee; blah - hmm, great.\nnew line with a significant quantity of text on it. This line really does contain some text. More text! More text! More text!\n"
                                  /* This is UTF8 stuff, Emacs doesn't
                                     really know how to display it */
                                  "Spanish (Español) ¡Hola! / French (Français) Bonjour, Salut / German (Deutsch Süd) Grüß Gott (testing Latin-1 chars encoded in UTF8)\nThai (we can't display this, just making sure we don't crash)  (ภาษาไทย)  สวัสดีครับ, สวัสดีค่ะ\n", -1);
          
#if 1
          gtk_text_buffer_get_iter_at_line_char(buffer, &iter, 0, 6);
          gtk_text_buffer_get_iter_at_line_char(buffer, &iter2, 0, 13);

          gtk_text_buffer_apply_tag(buffer, "fg_blue", &iter, &iter2);

          gtk_text_buffer_get_iter_at_line_char(buffer, &iter, 1, 10);
          gtk_text_buffer_get_iter_at_line_char(buffer, &iter2, 1, 16);

          gtk_text_buffer_apply_tag(buffer, "underline", &iter, &iter2);

          gtk_text_buffer_get_iter_at_line_char(buffer, &iter, 1, 14);
          gtk_text_buffer_get_iter_at_line_char(buffer, &iter2, 1, 24);

          gtk_text_buffer_apply_tag(buffer, "overstrike", &iter, &iter2);
          
          gtk_text_buffer_get_iter_at_line_char(buffer, &iter, 0, 9);
          gtk_text_buffer_get_iter_at_line_char(buffer, &iter2, 0, 16);

          gtk_text_buffer_apply_tag(buffer, "bg_green", &iter, &iter2);
  
          gtk_text_buffer_get_iter_at_line_char(buffer, &iter, 4, 2);
          gtk_text_buffer_get_iter_at_line_char(buffer, &iter2, 4, 10);

          gtk_text_buffer_apply_tag(buffer, "bg_green", &iter, &iter2);

          gtk_text_buffer_get_iter_at_line_char(buffer, &iter, 4, 8);
          gtk_text_buffer_get_iter_at_line_char(buffer, &iter2, 4, 15);

          gtk_text_buffer_apply_tag(buffer, "fg_red", &iter, &iter2);
#endif
          ++i;
        }

      gdk_pixmap_unref(pixmap);
      if (mask)
        gdk_bitmap_unref(mask);
    }

  
#if 0  
  str = gtk_text_buffer_get_chars_from_line(buffer, 0, 0, -1, TRUE);

  printf("Got: `%s'\n", str);

  g_free(str);

  gtk_text_buffer_spew(buffer);

#endif
  printf("%d lines %d chars\n",
         gtk_text_buffer_get_line_count(buffer),
         gtk_text_buffer_get_char_count(buffer));  

  /*   create_window(buffer); */
  create_window(buffer);
  
  gtk_main();

  return 0;
}


