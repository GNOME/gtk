
IMAGES = @STRIP_BEGIN@ \
	dialog_error.png	\
	dialog_info.png		\
	dialog_question.png	\
	dialog_warning.png	\
	stock_button_apply.png	\
	stock_button_cancel.png	\
	stock_button_close.png	\
	stock_button_no.png	\
	stock_button_ok.png	\
	stock_button_yes.png	\
	stock_close.png		\
	stock_exit.png		\
	stock_help.png		\
	stock_new.png		\
	stock_open.png		\
	stock_save.png		\
@STRIP_END@

VARIABLES = @STRIP_BEGIN@ \
		dialog_error     	$(srcdir)/dialog_error.png    	\
	        dialog_info      	$(srcdir)/dialog_info.png     	\
		dialog_question  	$(srcdir)/dialog_question.png 	\
	 	dialog_warning   	$(srcdir)/dialog_warning.png  	\
		stock_button_apply 	$(srcdir)/stock_button_apply.png  \
		stock_button_cancel     $(srcdir)/stock_button_cancel.png \
		stock_button_close      $(srcdir)/stock_button_close.png  \
		stock_button_no         $(srcdir)/stock_button_no.png  	\
		stock_button_ok     	$(srcdir)/stock_button_ok.png  	\
		stock_button_yes	$(srcdir)/stock_button_yes.png  	\
		stock_close 		$(srcdir)/stock_close.png  	\
		stock_exit		$(srcdir)/stock_exit.png 		\
		stock_help		$(srcdir)/stock_help.png  	\
		stock_new		$(srcdir)/stock_new.png		\
		stock_open		$(srcdir)/stock_open.png  	\
		stock_save		$(srcdir)/stock_save.png		\
@STRIP_END@

noinst_DATA = gtkstockpixbufs.h
CLEANFILES += $(noinst_DATA)

pixbuf_dir = $(top_builddir)/gdk-pixbuf

gtkstockpixbufs.h: $(pixbuf_dir)/make-inline-pixbuf $(IMAGES)
	GDK_PIXBUF_MODULEDIR=$(pixbuf_dir)/.libs \
	   $(pixbuf_dir)/make-inline-pixbuf $(srcdir)/gtkstockpixbufs.h $(VARIABLES)

EXTRA_DIST = $(IMAGES)
