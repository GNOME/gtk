/* This file extracted from the GTK tutorial. */

/* menufactory.h */

#ifndef __MENUFACTORY_H__
#define __MENUFACTORY_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void get_main_menu (GtkWidget **menubar, GtkAcceleratorTable **table);
void menus_create(GtkMenuEntry *entries, int nmenu_entries);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MENUFACTORY_H__ */
