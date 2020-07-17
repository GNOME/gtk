#include <gtk/gtk.h>
#include <ctype.h>

#define GTK_TYPE_MATCH_OBJECT (gtk_match_object_get_type ())
GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkMatchObject, gtk_match_object, GTK, MATCH_OBJECT, GObject)

struct _GtkMatchObject
{
  GObject parent_instance;
  char *string;
  int start;
  int end;
  int score;
};

enum {
  PROP_STRING = 1,
  PROP_START,
  PROP_END,
  PROP_SCORE
};

G_DEFINE_TYPE (GtkMatchObject, gtk_match_object, G_TYPE_OBJECT);

static void
gtk_match_object_init (GtkMatchObject *object)
{
  object->start = -1;
  object->end = -1;
  object->score = 0;
}

static void
gtk_match_object_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkMatchObject *self = GTK_MATCH_OBJECT (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_value_set_string (value, self->string);
      break;

    case PROP_START:
      g_value_set_int (value, self->start);
      break;

    case PROP_END:
      g_value_set_int (value, self->end);
      break;

    case PROP_SCORE:
      g_value_set_int (value, self->score);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_match_object_finalize (GObject *object)
{
  GtkMatchObject *self = GTK_MATCH_OBJECT (object);

  g_free (self->string);

  G_OBJECT_CLASS (gtk_match_object_parent_class)->finalize (object);
}

static void
gtk_match_object_class_init (GtkMatchObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GParamSpec *pspec;

  object_class->finalize = gtk_match_object_finalize;
  object_class->get_property = gtk_match_object_get_property;

  pspec = g_param_spec_string ("string", "String", "String",
                               NULL,
                               G_PARAM_READABLE |
                               G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STRING, pspec);

  pspec = g_param_spec_int ("start", "Start", "Match Start",
                            -1, G_MAXINT, -1,
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_START, pspec);

  pspec = g_param_spec_int ("end", "End", "Match End",
                            -1, G_MAXINT, -1,
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_END, pspec);

  pspec = g_param_spec_int ("score", "Score", "Score",
                            0, G_MAXINT, 0,
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SCORE, pspec);
}

static GtkMatchObject *
gtk_match_object_new (const char *string)
{
  GtkMatchObject *obj;

  obj = g_object_new (GTK_TYPE_MATCH_OBJECT, NULL);
  obj->string = g_strdup (string);

  return obj;
}

static void
gtk_match_object_set_match (GtkMatchObject *obj,
                            int             start,
                            int             end)
{
  g_object_freeze_notify (G_OBJECT (obj));

  if (obj->start != start)
    {
      obj->start = start;
      g_object_notify (G_OBJECT (obj), "start");
    }

  if (obj->end != end)
    {
      obj->end = end;
      g_object_notify (G_OBJECT (obj), "end");
    }

  g_object_thaw_notify (G_OBJECT (obj));
}

static void
gtk_match_object_set_score (GtkMatchObject *obj,
                            int             score)
{
  if (obj->score == score)
    return;

  obj->score = score;

  g_object_notify (G_OBJECT (obj), "score");
}

static const char *
gtk_match_object_get_string (GtkMatchObject *obj)
{
  return obj->string;
}

static int
gtk_match_object_get_start (GtkMatchObject *obj)
{
  return obj->start;
}

static int
gtk_match_object_get_end (GtkMatchObject *obj)
{
  return obj->end;
}

static int
gtk_match_object_get_score (GtkMatchObject *obj)
{
  return obj->score;
}

static GListModel *
load_words (const char *file)
{
  char *contents;
  gsize size;
  char **lines;
  GError *error = NULL;
  GListStore *store;
  int i;

  if (!g_file_get_contents (file, &contents, &size, &error))
    {
      g_printerr ("%s", error->message);
      exit (1);
    }

  store = g_list_store_new (GTK_TYPE_MATCH_OBJECT);

  lines = g_strsplit (contents, "\n", -1);

  for (i = 0; lines[i]; i++)
    {
      char *s = g_strstrip (lines[i]);

       if (s[0] != '\0')
         {
           GtkMatchObject *obj = gtk_match_object_new (s);
           g_list_store_append (store, obj);
           g_object_unref (obj);
         }
    }

  g_strfreev (lines);
  g_free (contents);

  return G_LIST_MODEL (store);
}

static void
setup_item (GtkSignalListItemFactory *factory,
            GtkListItem              *list_item,
            gpointer                  data)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_item (GtkSignalListItemFactory *factory,
           GtkListItem              *list_item,
           gpointer                  data)
{
  GtkMatchObject *obj;
  GtkWidget *label;
  PangoAttrList *attrs;
  PangoAttribute *attr;
  const char *str;
  int score;
  char *text;

  obj = GTK_MATCH_OBJECT (gtk_list_item_get_item (list_item));
  label = gtk_list_item_get_child (list_item);

  str = gtk_match_object_get_string (obj);
  score = gtk_match_object_get_score (obj);

  text = g_strdup_printf ("%s - %d", str, score);
  gtk_label_set_label (GTK_LABEL (label), text);
  g_free (text);
  attrs = pango_attr_list_new ();
  attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  attr->start_index = gtk_match_object_get_start (obj);
  attr->end_index = gtk_match_object_get_end (obj);
  pango_attr_list_insert (attrs, attr);
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);
}

static void
text_changed (GObject    *object,
              GParamSpec *pspec,
              GListModel *model)
{
  const char *text;
  guint i, n;
  int len;

  text = gtk_editable_get_text (GTK_EDITABLE (object));
  len = strlen (text);

  n = g_list_model_get_n_items (model);
  for (i = 0; i < n; i++)
    {
      GtkMatchObject *obj = GTK_MATCH_OBJECT (g_list_model_get_item (model, i));
      char *p;
      int start, end;
      int score;

#ifdef G_OS_WIN32
      p = strstr (obj->string, text);
#else
      p = strcasestr (obj->string, text);
#endif
      if (p)
        {
          start = p - obj->string;
          end = start + len;

          /* prefer matches close to the start */
          score = 100 - start;

          /* prefer exact matches */
          if (strncmp (p, text, len) == 0)
            score += 10;

          /* prefer words */
          if ((start == 0 || isspace (p[-1])) &&
              (p[len] == '\0' || isspace (p[len]) || ispunct (p[len])))
            score += 20;
        }
      else
        {
          start = end = -1;
          score = 0;
        }

      gtk_match_object_set_match (obj, start, end);
      gtk_match_object_set_score (obj, score);
      g_object_unref (obj);
    }

  g_list_model_items_changed (model, 0, n, n);
}

static gboolean
filter_func (gpointer item, gpointer data)
{
  return GTK_MATCH_OBJECT (item)->score > 0;
}

static int
compare_func (gconstpointer a,
              gconstpointer b,
              gpointer      data)
{
  const GtkMatchObject *ao = a;
  const GtkMatchObject *bo = b;
  return bo->score - ao->score;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *entry;
  GListModel *words;
  GListModel *sorted;
  GListModel *filtered;
  GtkExpression *expression;
  GtkListItemFactory *factory;
  GtkFilter *filter;
  GtkSorter *sorter;

  words = load_words (argv[1]);

  sorter = gtk_custom_sorter_new (compare_func, NULL, NULL);
  sorted = G_LIST_MODEL (gtk_sort_list_model_new (words, sorter));
  g_object_unref (sorter);

  filter = gtk_custom_filter_new (filter_func, NULL, NULL);
  filtered = G_LIST_MODEL (gtk_filter_list_model_new (sorted, filter));
  g_object_unref (filter);

  gtk_init ();

  window = gtk_window_new ();

  entry = gtk_suggestion_entry_new ();
  gtk_suggestion_entry_set_model (GTK_SUGGESTION_ENTRY (entry), filtered);

  gtk_suggestion_entry_set_use_filter (GTK_SUGGESTION_ENTRY (entry), FALSE);

  expression = gtk_property_expression_new (GTK_TYPE_MATCH_OBJECT, NULL, "string");
  gtk_suggestion_entry_set_expression (GTK_SUGGESTION_ENTRY (entry), expression);
  gtk_expression_unref (expression);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);
  gtk_suggestion_entry_set_factory (GTK_SUGGESTION_ENTRY (entry), factory);
  g_object_unref (factory);

  g_signal_connect (entry, "notify::text", G_CALLBACK (text_changed), words);

  gtk_widget_set_valign (entry, GTK_ALIGN_CENTER);

  gtk_window_set_child (GTK_WINDOW (window), entry);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  g_object_unref (words);
  g_object_unref (sorted);
  g_object_unref (filtered);

  return 0;
}
