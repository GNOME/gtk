#include "config.h"

#include "gtkanimationmanagerprivate.h"

struct _GtkAnimationManager
{
  GObject parent_instance;

  GdkFrameClock *frame_clock;

  gint64 last_frame_time;
};

G_DEFINE_TYPE (GtkAnimationManager, gtk_animation_manager, G_TYPE_OBJECT)

static void
gtk_animation_manager_class_init (GtkAnimationManagerClass *klass)
{
}

static void
gtk_animation_manager_init (GtkAnimationManager *self)
{
}

GtkAnimationManager *
gtk_animation_manager_new (void)
{
  return g_object_new (GTK_TYPE_ANIMATION_MANAGER, NULL);
}

void
gtk_animation_manager_set_frame_clock (GtkAnimationManager *self,
                                       GdkFrameClock       *frame_clock)
{

}
