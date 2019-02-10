#include "config.h"

#include "gtkdoubleselection.h"
#include "gtkselectionmodel.h"

struct _GtkDoubleSelection
{
  GObject parent_instance;

  GListModel *model;
  guint selected1;
  guint selected2;
};

struct _GtkDoubleSelectionClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,

  /* selectionmodel */
  PROP_MODEL,
  N_PROPS = PROP_MODEL
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_double_selection_get_item_type (GListModel *list)
{
  GtkDoubleSelection *self = GTK_DOUBLE_SELECTION (list);

  return g_list_model_get_item_type (self->model);
}

static guint
gtk_double_selection_get_n_items (GListModel *list)
{
  GtkDoubleSelection *self = GTK_DOUBLE_SELECTION (list);

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_double_selection_get_item (GListModel *list,
                               guint       position)
{
  GtkDoubleSelection *self = GTK_DOUBLE_SELECTION (list);

  return g_list_model_get_item (self->model, position);
}

static void
gtk_double_selection_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_double_selection_get_item_type;
  iface->get_n_items = gtk_double_selection_get_n_items;
  iface->get_item = gtk_double_selection_get_item;
}

static gboolean
gtk_double_selection_is_selected (GtkSelectionModel *model,
                                  guint              position)
{
  GtkDoubleSelection *self = GTK_DOUBLE_SELECTION (model);

  return self->selected1 == position || self->selected2 == position;
}

static gboolean
gtk_double_selection_select_item (GtkSelectionModel *model,
                                  guint              position,
                                  gboolean           exclusive)
{
  GtkDoubleSelection *self = GTK_DOUBLE_SELECTION (model);
  guint pos;
  guint n_items;

  if (position == self->selected1 || position == self->selected2)
    {
      /* no change */
    }
  else if (position < (self->selected1 + self->selected2) / 2)
    {
      pos = MIN (self->selected1, position);
      n_items = MAX (self->selected1, position) - pos + 1;
      self->selected1 = position;
      gtk_selection_model_selection_changed (model, pos, n_items);
    }
  else
    {
      pos = MIN (self->selected2, position);
      n_items = MAX (self->selected2, position) - pos + 1;
      self->selected2 = position;
      gtk_selection_model_selection_changed (model, pos, n_items);
    }

  return TRUE;
}

static gboolean
gtk_double_selection_query_range (GtkSelectionModel *model,
                                  guint             *position,
                                  guint             *n_items)
{
  GtkDoubleSelection *self = GTK_DOUBLE_SELECTION (model);
  guint size;

  size = g_list_model_get_n_items (self->model);

  if (*position < self->selected1)
    {
      *position = 0;
      *n_items = MIN (self->selected1, size);
      return FALSE;
    }
  else if (*position == self->selected1)
    {
      if (self->selected2 == self->selected1 + 1)
        {
          *position = self->selected1;
          *n_items = 2;
          return TRUE;
        }
      else
        {
          *position = self->selected1;
          *n_items = 1;
          return TRUE;
        }
    }
  else if (*position > self->selected1 && *position < self->selected2)
    {
      *position = self->selected1 + 1;
      *n_items = MIN(self->selected2, size) - *position;
      return FALSE;
    }
  else if (*position == self->selected2)
    {
      if (self->selected2 == self->selected1 + 1)
        {
          *position = self->selected1;
          *n_items = 2;
          return TRUE;
        }
      else
        {
          *position = self->selected2;
          *n_items = 1;
          return TRUE;
        }
    }
  else if (*position > self->selected2)
    {
      *position = self->selected2 + 1;
      *n_items = size - *position;
      return FALSE;
    }

  return FALSE;
}

static void
gtk_double_selection_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_double_selection_is_selected; 
  iface->select_item = gtk_double_selection_select_item; 
  iface->query_range = gtk_double_selection_query_range;
}

G_DEFINE_TYPE_EXTENDED (GtkDoubleSelection, gtk_double_selection, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                               gtk_double_selection_list_model_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL,
                                               gtk_double_selection_selection_model_init))

static void
gtk_double_selection_items_changed_cb (GListModel         *model,
                                       guint               position,
                                       guint               removed,
                                       guint               added,
                                       GtkDoubleSelection *self)
{
  /* FIXME: maintain selection here */
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static void
gtk_double_selection_clear_model (GtkDoubleSelection *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, 
                                        gtk_double_selection_items_changed_cb,
                                        self);
  g_clear_object (&self->model);
}

static void
gtk_double_selection_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)

{
  GtkDoubleSelection *self = GTK_DOUBLE_SELECTION (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_double_selection_clear_model (self);
      self->model = g_value_dup_object (value);
      if (self->model)
        g_signal_connect (self->model, "items-changed",
                          G_CALLBACK (gtk_double_selection_items_changed_cb), self);
      /* FIXME */
      g_assert (g_list_model_get_n_items (self->model) >= 2);
      self->selected1 = 0;
      self->selected2 = 1;
        
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_double_selection_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkDoubleSelection *self = GTK_DOUBLE_SELECTION (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_double_selection_dispose (GObject *object)
{
  GtkDoubleSelection *self = GTK_DOUBLE_SELECTION (object);

  gtk_double_selection_clear_model (self);

  G_OBJECT_CLASS (gtk_double_selection_parent_class)->dispose (object);
}

static void
gtk_double_selection_class_init (GtkDoubleSelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_double_selection_get_property;
  gobject_class->set_property = gtk_double_selection_set_property;
  gobject_class->dispose = gtk_double_selection_dispose;

  g_object_class_override_property (gobject_class, PROP_MODEL, "model");

  //g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_double_selection_init (GtkDoubleSelection *self)
{
}

/**
 * gtk_double_selection_new:
 * @model: (transfer none): the #GListModel to manage
 *
 * Creates a new selection to handle @model.
 *
 * Returns: (transfer full) (type GtkDoubleSelection): a new #GtkDoubleSelection
 **/
GtkDoubleSelection *
gtk_double_selection_new (GListModel *model)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  return g_object_new (GTK_TYPE_DOUBLE_SELECTION,
                       "model", model,
                       NULL);
}
