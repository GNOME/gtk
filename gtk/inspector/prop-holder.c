#include "prop-holder.h"

struct _PropHolder {
  GObject instance;

  GObject *object;
  char *property;
};

G_DEFINE_TYPE (PropHolder, prop_holder, G_TYPE_OBJECT)

static void
prop_holder_init (PropHolder *holder)
{
}

static void
prop_holder_finalize (GObject *object)
{
  PropHolder *holder = PROP_HOLDER (object);

  g_object_unref (holder->object);
  g_free (holder->property);

  G_OBJECT_CLASS (prop_holder_parent_class)->finalize (object);
}

static void
prop_holder_class_init (PropHolderClass *class)
{
  G_OBJECT_CLASS (class)->finalize = prop_holder_finalize;
}

PropHolder *
prop_holder_new (GObject *object,
                 const char *property)
{
  PropHolder *holder;

  holder = g_object_new (PROP_TYPE_HOLDER, NULL);
  holder->object = g_object_ref (object);
  holder->property = g_strdup (property);

  return holder;
}

GObject *
prop_holder_get_object (PropHolder *holder)
{
  return holder->object;
}

const char *
prop_holder_get_property (PropHolder *holder)
{
  return holder->property;
}
