# $(gtkconfigdir)/gtkrc.ru
#
# This file defines the fontsets for Russian language (ru) using
# the KOI8-R charset encoding.
#
# 1999, Pablo Saratxaga <srtxg@chanae.alphanet.ch>
#
# Changelog:
# 1999-06-23: changed -cronyx-helvetica-...koi8-r to -*-helvetica-...koi8-r
#	everybody doesn't has cronyx fonts (I haven't :) )

style "gtk-default-ru" {
       fontset = "-adobe-helvetica-medium-r-normal--12-*-*-*-*-*-iso8859-*,\
                  -*-arial-medium-r-normal--12-*-*-*-*-*-iso8859-1,\"
                  -cronyx-helvetica-medium-r-normal--12-*-*-*-*-*-koi8-r,\"
                  -*-arial-medium-r-normal--12-*-*-*-*-*-koi8-r"
}
class "GtkWidget" style "gtk-default-ru"

