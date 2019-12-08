
#include "action-holder.h"

struct _ActionHolder {
  GObject instance;

  GActionGroup *group;
  char *name;
};

G_DEFINE_TYPE (ActionHolder, action_holder, G_TYPE_OBJECT)

static void
action_holder_init (ActionHolder *holder)
{
}

static void
action_holder_finalize (GObject *object)
{
  ActionHolder *holder = ACTION_HOLDER (object);

  g_object_unref (holder->group);
  g_free (holder->name);

  G_OBJECT_CLASS (action_holder_parent_class)->finalize (object);
}

static void
action_holder_class_init (ActionHolderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = action_holder_finalize;
}

ActionHolder *
action_holder_new (GActionGroup *group,
                   const char   *name)
{
  ActionHolder *holder;

  holder = g_object_new (ACTION_TYPE_HOLDER, NULL);

  holder->group = g_object_ref (group);
  holder->name = g_strdup (name);

  return holder;
}

GActionGroup *
action_holder_get_group (ActionHolder *holder)
{
  return holder->group;
}

const char *
action_holder_get_name (ActionHolder *holder)
{
  return holder->name;
}
