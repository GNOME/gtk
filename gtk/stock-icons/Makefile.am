BUILT_SOURCES=gtkstockpixbufs.h

IMAGES= dialog_error.png  dialog_info.png  dialog_question.png  dialog_warning.png  stock_button_apply.png  stock_button_cancel.png  stock_button_close.png  stock_button_no.png  stock_button_ok.png  stock_button_yes.png  stock_close.png  stock_exit.png  stock_help.png  stock_new.png  stock_open.png  stock_save.png

VARIABLES=	dialog_error     	dialog_error.png    	\
	        dialog_info      	dialog_info.png     	\
		dialog_question  	dialog_question.png 	\
	 	dialog_warning   	dialog_warning.png  	\
		stock_button_apply 	stock_button_apply.png  \
		stock_button_cancel     stock_button_cancel.png \
		stock_button_close      stock_button_close.png  \
		stock_button_no         stock_button_no.png  	\
		stock_button_ok     	stock_button_ok.png  	\
		stock_button_yes	stock_button_yes.png  	\
		stock_close 		stock_close.png  	\
		stock_exit		stock_exit.png 		\
		stock_help		stock_help.png  	\
		stock_new		stock_new.png		\
		stock_open		stock_open.png  	\
		stock_save		stock_save.png

gtkstockpixbufs.h: $(top_builddir)/gdk-pixbuf/make-inline-pixbuf $(IMAGES)
	(topdir=`cd $(top_builddir) && pwd` ; curdir=`pwd` ; \
         cd $(top_srcdir)/gtk/stock-icons &&                 \
            GDK_PIXBUF_MODULEDIR=$$topdir/gdk-pixbuf/.libs $$topdir/gdk-pixbuf/make-inline-pixbuf $$curdir/gtkstockpixbufs.h $(VARIABLES))

EXTRA_DIST = $(IMAGES)

# For some reason, rebuilding of this file doesn't happen in the make dist'ed version, so force it.

all-am: gtkstockpixbufs.h
