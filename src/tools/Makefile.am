## Avoid parallel build races when generating sources
.NOTPARALLEL:

CC = $(CC_FOR_BUILD)
AM_CPPFLAGS = -I$(top_srcdir)/src $(GLIB_FOR_BUILD_CFLAGS) $(CPPFLAGS_FOR_BUILD)
AM_CFLAGS = $(CFLAGS_FOR_BUILD)
AM_LDFLAGS = $(GLIB_FOR_BUILD_LDFLAGS) $(LDFLAGS_FOR_BUILD)

noinst_PROGRAMS = gendec genglsl genmach

gendec_SOURCES = gendec.c gendec.h insparse.c actparse.c
gendec_LDADD = @GLIB_FOR_BUILD_LIBS@ @GTK_LIBS@ $(INTLLIBS)

genmach_SOURCES = genmach.c genmach.h mdparse.c
genmach_LDADD = @GLIB_FOR_BUILD_LIBS@ @GTK_LIBS@ $(INTLLIBS)

genglsl_SOURCES = genglsl.c
genglsl_LDADD = @GLIB_FOR_BUILD_LIBS@ @GTK_LIBS@ $(INTLLIBS)

