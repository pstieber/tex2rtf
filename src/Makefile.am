## Process this file with automake to produce Makefile.in

bin_PROGRAMS = tex2rtf_gui tex2rtf

tex2rtf_gui_SOURCES = \
htmlutil.cpp \
maths.cpp \
readshg.cpp \
rtfutils.cpp \
table.cpp \
tex2any.cpp \
tex2rtf.cpp \
texutils.cpp

tex2rtf_gui_LDADD = @WX_LIBS@

tex2rtf_SOURCES = \
htmlutil.cpp \
maths.cpp \
readshg.cpp \
rtfutils.cpp \
table.cpp \
tex2any.cpp \
tex2rtf.cpp \
texutils.cpp

tex2rtf_CPPFLAGS=-DNO_GUI

noinst_HEADERS = \
bmputils.h \
readshg.h \
rtfutils.h \
symbols.h \
table.h \
tex2any.h \
tex2rtf.h

tex2rtf_LDADD = @WX_LIBS@
