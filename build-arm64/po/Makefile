# Makefile for program source directory in GNU NLS utilities package.
# Copyright (C) 1995, 1996, 1997 by Ulrich Drepper <drepper@gnu.ai.mit.edu>
#
# This file file be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU Public License
# but which still want to provide support for the GNU gettext functionality.
# Please note that the actual code is *not* freely available.
#
# - Modified by Owen Taylor <otaylor@redhat.com> to use GETTEXT_PACKAGE
#   instead of PACKAGE and to look for po2tbl in ./ not in intl/
#
# - Modified by jacob berkman <jacob@ximian.com> to install
#   Makefile.in.in and po2tbl.sed.in for use with glib-gettextize

GETTEXT_PACKAGE = lxdream
PACKAGE = lxdream
VERSION = 0.1

SHELL = /bin/sh


srcdir = ../../po
top_srcdir = ../..
VPATH = ../../po

prefix = /usr/local
exec_prefix = ${prefix}
datarootdir = ${prefix}/share
datadir = ${datarootdir}
libdir = ${exec_prefix}/lib
localedir = $(libdir)/locale
gnulocaledir = $(datadir)/locale
gettextsrcdir = $(datadir)/glib-2.0/gettext/po
subdir = po

INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
MKINSTALLDIRS = mkdir -p

CC = gcc
GENCAT = @GENCAT@
GMSGFMT = 
MSGFMT = 
MSGFMT_OPTS = 
XGETTEXT = :
MSGMERGE = msgmerge

DEFS = -DHAVE_CONFIG_H
CFLAGS = -g -fexceptions -D_POSIX_C_SOURCE=200809L -g -fexceptions -D_POSIX_C_SOURCE=200809L -O2 -fomit-frame-pointer -DGL_SILENCE_DEPRECATION -std=c99
CPPFLAGS =  -D_DARWIN_C_SOURCE

INCLUDES = -I.. -I$(top_srcdir)/intl

COMPILE = $(CC) -c $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(XCFLAGS)

SOURCES = 
POFILES =  de.po es.po it.po nl.po pt_BR.po
GMOFILES =  de.gmo es.gmo it.gmo nl.gmo pt_BR.gmo
DISTFILES = LINGUAS ChangeLog Makefile.in.in POTFILES.in $(GETTEXT_PACKAGE).pot \
$(POFILES) $(GMOFILES) $(SOURCES)

POTFILES = \
	../../src/aica/aica.c \
	../../src/aica/aica.h \
	../../src/aica/armcore.c \
	../../src/aica/armcore.h \
	../../src/aica/armdasm.c \
	../../src/aica/armdasm.h \
	../../src/aica/armmem.c \
	../../src/aica/audio.c \
	../../src/aica/audio.h \
	../../src/asic.c \
	../../src/asic.h \
	../../src/bios.c \
	../../src/bootstrap.c \
	../../src/bootstrap.h \
	../../src/clock.h \
	../../src/cocoaui/cocoa_cfg.m \
	../../src/cocoaui/cocoa_ctrl.m \
	../../src/cocoaui/cocoa_gd.m \
	../../src/cocoaui/cocoa_prefs.m \
	../../src/cocoaui/cocoa_win.m \
	../../src/cocoaui/cocoaui.h \
	../../src/cocoaui/cocoaui.m \
	../../src/cocoaui/paths_osx.m \
	../../src/config.c \
	../../src/config.h \
	../../src/cpu.c \
	../../src/cpu.h \
	../../src/dckeysyms.h \
	../../src/dcload.c \
	../../src/display.c \
	../../src/display.h \
	../../src/dream.h \
	../../src/dreamcast.c \
	../../src/dreamcast.h \
	../../src/drivers/audio_alsa.c \
	../../src/drivers/audio_esd.c \
	../../src/drivers/audio_null.c \
	../../src/drivers/audio_osx.m \
	../../src/drivers/audio_pulse.c \
	../../src/drivers/audio_sdl.c \
	../../src/drivers/cdrom/cd_cdi.c \
	../../src/drivers/cdrom/cd_gdi.c \
	../../src/drivers/cdrom/cd_linux.c \
	../../src/drivers/cdrom/cd_mmc.c \
	../../src/drivers/cdrom/cd_none.c \
	../../src/drivers/cdrom/cd_nrg.c \
	../../src/drivers/cdrom/cd_osx.c \
	../../src/drivers/cdrom/cdimpl.h \
	../../src/drivers/cdrom/cdrom.c \
	../../src/drivers/cdrom/cdrom.h \
	../../src/drivers/cdrom/defs.h \
	../../src/drivers/cdrom/drive.c \
	../../src/drivers/cdrom/drive.h \
	../../src/drivers/cdrom/ecc.h \
	../../src/drivers/cdrom/edc_crctable.h \
	../../src/drivers/cdrom/edc_ecc.c \
	../../src/drivers/cdrom/edc_encoder.h \
	../../src/drivers/cdrom/edc_l2sq.h \
	../../src/drivers/cdrom/edc_scramble.h \
	../../src/drivers/cdrom/isofs.c \
	../../src/drivers/cdrom/isofs.h \
	../../src/drivers/cdrom/isomem.c \
	../../src/drivers/cdrom/sector.c \
	../../src/drivers/cdrom/sector.h \
	../../src/drivers/dummy.c \
	../../src/drivers/gl_fbo.c \
	../../src/drivers/gl_vbo.c \
	../../src/drivers/input_lirc.c \
	../../src/drivers/io_glib.c \
	../../src/drivers/io_osx.m \
	../../src/drivers/joy_linux.c \
	../../src/drivers/joy_linux.h \
	../../src/drivers/osx_iokit.h \
	../../src/drivers/osx_iokit.m \
	../../src/drivers/serial_unix.c \
	../../src/drivers/video_gdk.c \
	../../src/drivers/video_gl.c \
	../../src/drivers/video_gl.h \
	../../src/drivers/video_glx.c \
	../../src/drivers/video_glx.h \
	../../src/drivers/video_gtk.c \
	../../src/drivers/video_nsgl.h \
	../../src/drivers/video_nsgl.m \
	../../src/drivers/video_null.c \
	../../src/drivers/video_osx.m \
	../../src/elf.h \
	../../src/eventq.c \
	../../src/eventq.h \
	../../src/gdbserver.c \
	../../src/gdlist.c \
	../../src/gdlist.h \
	../../src/gdrom/gdrom.c \
	../../src/gdrom/gdrom.h \
	../../src/gdrom/ide.c \
	../../src/gdrom/ide.h \
	../../src/gdrom/packet.h \
	../../src/gettext.h \
	../../src/gtkui/gtk_cfg.c \
	../../src/gtkui/gtk_ctrl.c \
	../../src/gtkui/gtk_debug.c \
	../../src/gtkui/gtk_dump.c \
	../../src/gtkui/gtk_gd.c \
	../../src/gtkui/gtk_mmio.c \
	../../src/gtkui/gtk_win.c \
	../../src/gtkui/gtkcb.c \
	../../src/gtkui/gtkui.c \
	../../src/gtkui/gtkui.h \
	../../src/gui.h \
	../../src/hook.h \
	../../src/hotkeys.c \
	../../src/hotkeys.h \
	../../src/ioutil.c \
	../../src/ioutil.h \
	../../src/loader.c \
	../../src/loader.h \
	../../src/lxdream.h \
	../../src/lxpaths.c \
	../../src/lxpaths.h \
	../../src/main.c \
	../../src/maple/controller.c \
	../../src/maple/kbd.c \
	../../src/maple/lightgun.c \
	../../src/maple/maple.c \
	../../src/maple/maple.h \
	../../src/maple/mouse.c \
	../../src/maple/vmu.c \
	../../src/mem.c \
	../../src/mem.h \
	../../src/mmio.h \
	../../src/paths_unix.c \
	../../src/plugin.c \
	../../src/plugin.h \
	../../src/pvr2/gl_sl.c \
	../../src/pvr2/glrender.c \
	../../src/pvr2/glutil.c \
	../../src/pvr2/glutil.h \
	../../src/pvr2/pvr2.c \
	../../src/pvr2/pvr2.h \
	../../src/pvr2/pvr2mem.c \
	../../src/pvr2/pvr2mmio.h \
	../../src/pvr2/rendsave.c \
	../../src/pvr2/rendsort.c \
	../../src/pvr2/scene.c \
	../../src/pvr2/scene.h \
	../../src/pvr2/tacore.c \
	../../src/pvr2/texcache.c \
	../../src/pvr2/tileiter.h \
	../../src/pvr2/yuv.c \
	../../src/sdram.c \
	../../src/serial.h \
	../../src/sh4/cache.c \
	../../src/sh4/dmac.c \
	../../src/sh4/dmac.h \
	../../src/sh4/intc.c \
	../../src/sh4/intc.h \
	../../src/sh4/mmu.c \
	../../src/sh4/mmu.h \
	../../src/sh4/mmux86.c \
	../../src/sh4/pmm.c \
	../../src/sh4/scif.c \
	../../src/sh4/sh4.c \
	../../src/sh4/sh4.h \
	../../src/sh4/sh4core.h \
	../../src/sh4/sh4dasm.h \
	../../src/sh4/sh4mem.c \
	../../src/sh4/sh4mmio.c \
	../../src/sh4/sh4mmio.h \
	../../src/sh4/sh4stat.h \
	../../src/sh4/sh4trans.c \
	../../src/sh4/sh4trans.h \
	../../src/sh4/shadow.c \
	../../src/sh4/timer.c \
	../../src/syscall.c \
	../../src/syscall.h \
	../../src/test/testsh4x86.c \
	../../src/test/testxlt.c \
	../../src/tools/actparse.c \
	../../src/tools/gendec.c \
	../../src/tools/gendec.h \
	../../src/tools/genglsl.c \
	../../src/tools/genmach.c \
	../../src/tools/genmach.h \
	../../src/tools/insparse.c \
	../../src/tools/mdparse.c \
	../../src/util.c \
	../../src/vmu/vmulist.c \
	../../src/vmu/vmulist.h \
	../../src/vmu/vmuvol.c \
	../../src/vmu/vmuvol.h \
	../../src/watch.c \
	../../src/x86dasm/ansidecl.h \
	../../src/x86dasm/bfd.h \
	../../src/x86dasm/dis-asm.h \
	../../src/x86dasm/dis-buf.c \
	../../src/x86dasm/dis-init.c \
	../../src/x86dasm/i386-dis.c \
	../../src/x86dasm/symcat.h \
	../../src/x86dasm/sysdep.h \
	../../src/x86dasm/x86dasm.c \
	../../src/x86dasm/x86dasm.h \
	../../src/xlat/x86/amd64abi.h \
	../../src/xlat/x86/ia32abi.h \
	../../src/xlat/x86/x86op.h \
	../../src/xlat/xltcache.c \
	../../src/xlat/xltcache.h

CATALOGS = 
CATOBJEXT = NONE
INSTOBJEXT = 

.SUFFIXES:
.SUFFIXES: .c .o .po .pox .gmo .mo .msg .cat

.c.o:
	$(COMPILE) $<

.po.pox:
	$(MAKE) $(GETTEXT_PACKAGE).pot
	$(MSGMERGE) $< $(srcdir)/$(GETTEXT_PACKAGE).pot -o $*.pox

.po.mo:
	$(MSGFMT) -o $@ $<

.po.gmo:
	$(AM_V_GEN) file=$(srcdir)/`echo $* | sed 's,.*/,,'`.gmo \
	  && rm -f $$file && $(GMSGFMT) $(MSGFMT_OPTS) -o $$file $<

.po.cat:
	sed -f ../intl/po2msg.sed < $< > $*.msg \
	  && rm -f $@ && $(GENCAT) $@ $*.msg


all: all-yes

all-yes: $(CATALOGS)
all-no:

$(srcdir)/$(GETTEXT_PACKAGE).pot: $(POTFILES)
	$(XGETTEXT) --default-domain=$(GETTEXT_PACKAGE) --from-code=UTF-8 \
	  --msgid-bugs-address='http://bugzilla.gnome.org/enter_bug.cgi?product=glib&keywords=I18N+L10N&component=general' \
	  --add-comments --keyword=_ --keyword=N_ \
	  --keyword=C_:1c,2 \
	  --keyword=NC_:1c,2 \
	  --keyword=g_dcgettext:2 \
	  --keyword=g_dngettext:2,3 \
	  --keyword=g_dpgettext2:2c,3 \
	  --flag=N_:1:pass-c-format \
	  --flag=C_:2:pass-c-format \
	  --flag=NC_:2:pass-c-format \
	  --flag=g_dngettext:2:pass-c-format \
          --flag=g_strdup_printf:1:c-format \
          --flag=g_string_printf:2:c-format \
          --flag=g_string_append_printf:2:c-format \
          --flag=g_error_new:3:c-format \
          --flag=g_set_error:4:c-format \
          --flag=g_markup_printf_escaped:1:c-format \
          --flag=g_log:3:c-format \
          --flag=g_print:1:c-format \
          --flag=g_printerr:1:c-format \
          --flag=g_printf:1:c-format \
          --flag=g_fprintf:2:c-format \
          --flag=g_sprintf:2:c-format \
          --flag=g_snprintf:3:c-format \
          --flag=g_scanner_error:2:c-format \
          --flag=g_scanner_warn:2:c-format \
	  $(POTFILES) \
	&& test ! -f $(GETTEXT_PACKAGE).po \
	   || ( rm -f $(srcdir)/$(GETTEXT_PACKAGE).pot \
		&& mv $(GETTEXT_PACKAGE).po $(srcdir)/$(GETTEXT_PACKAGE).pot )

install: install-exec install-data
install-exec:
install-data: install-data-yes
install-data-no: all
install-data-yes: all
	$(MKINSTALLDIRS) $(DESTDIR)$(datadir); \
	catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  case "$$cat" in \
	    *.gmo) destdir=$(gnulocaledir);; \
	    *)     destdir=$(localedir);; \
	  esac; \
	  lang=`echo $$cat | sed 's/\$(CATOBJEXT)$$//'`; \
	  dir=$(DESTDIR)$$destdir/$$lang/LC_MESSAGES; \
	  $(MKINSTALLDIRS) $$dir; \
	  if test -r $$cat; then \
	    $(INSTALL_DATA) $$cat $$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT); \
	    echo "installing $$cat as $$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT)"; \
	  else \
	    $(INSTALL_DATA) $(srcdir)/$$cat $$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT); \
	    echo "installing $(srcdir)/$$cat as" \
		 "$$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT)"; \
	  fi; \
	  if test -r $$cat.m; then \
	    $(INSTALL_DATA) $$cat.m $$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT).m; \
	    echo "installing $$cat.m as $$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT).m"; \
	  else \
	    if test -r $(srcdir)/$$cat.m ; then \
	      $(INSTALL_DATA) $(srcdir)/$$cat.m \
		$$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT).m; \
	      echo "installing $(srcdir)/$$cat as" \
		   "$$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT).m"; \
	    else \
	      true; \
	    fi; \
	  fi; \
	done
	if test "$(PACKAGE)" = "glib"; then \
	  $(MKINSTALLDIRS) $(DESTDIR)$(gettextsrcdir); \
	  $(INSTALL_DATA) $(srcdir)/Makefile.in.in \
			  $(DESTDIR)$(gettextsrcdir)/Makefile.in.in; \
	else \
	  : ; \
	fi

# Define this as empty until I found a useful application.
installcheck:

uninstall:
	catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed 's/\$(CATOBJEXT)$$//'`; \
	  rm -f $(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE)$(INSTOBJEXT); \
	  rm -f $(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE)$(INSTOBJEXT).m; \
	  rm -f $(DESTDIR)$(gnulocaledir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE)$(INSTOBJEXT); \
	  rm -f $(DESTDIR)$(gnulocaledir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE)$(INSTOBJEXT).m; \
	done
	if test "$(PACKAGE)" = "glib"; then \
	  rm -f $(DESTDIR)$(gettextsrcdir)/Makefile.in.in; \
	fi

check: all

dvi info tags TAGS ID:

mostlyclean:
	rm -f core core.* *.pox $(GETTEXT_PACKAGE).po *.old.po cat-id-tbl.tmp
	rm -fr *.o

clean: mostlyclean

distclean: clean
	rm -f Makefile Makefile.in POTFILES *.mo *.msg *.cat *.cat.m

maintainer-clean: distclean
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."
	rm -f $(GMOFILES)

distdir = ../$(GETTEXT_PACKAGE)-$(VERSION)/$(subdir)
dist distdir: $(DISTFILES)
	dists="$(DISTFILES)"; \
	for file in $$dists; do \
	  ln $(srcdir)/$$file $(distdir) 2> /dev/null \
	    || cp -p $(srcdir)/$$file $(distdir); \
	done

update-po: Makefile
	$(MAKE) $(GETTEXT_PACKAGE).pot
	tmpdir=`pwd`; \
	cd $(srcdir); \
	catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed 's/\$(CATOBJEXT)$$//'`; \
	  echo "$$lang:"; \
	  if $(MSGMERGE) $$lang.po $(GETTEXT_PACKAGE).pot -o $$tmpdir/$$lang.new.po; then \
	    if cmp $$lang.po $$tmpdir/$$lang.new.po >/dev/null 2>&1; then \
	      rm -f $$tmpdir/$$lang.new.po; \
            else \
	      if mv -f $$tmpdir/$$lang.new.po $$lang.po; then \
	        :; \
	      else \
	        echo "msgmerge for $$lang.po failed: cannot move $$tmpdir/$$lang.new.po to $$lang.po" 1>&2; \
	        rm -f $$tmpdir/$$lang.new.po; \
	        exit 1; \
	      fi; \
	    fi; \
	  else \
	    echo "msgmerge for $$cat failed!"; \
	    rm -f $$tmpdir/$$lang.new.po; \
	  fi; \
	done

# POTFILES is created from POTFILES.in by stripping comments, empty lines
# and Intltool tags (enclosed in square brackets), and appending a full
# relative path to them
POTFILES: POTFILES.in
	( if test 'x$(srcdir)' != 'x.'; then \
	    posrcprefix='$(top_srcdir)/'; \
	  else \
	    posrcprefix="../"; \
	  fi; \
	  rm -f $@-t $@ \
	    && (sed -e '/^#/d' 						\
		    -e "s/^\[.*\] +//" 					\
		    -e '/^[ 	]*$$/d' 				\
		    -e "s@.*@	$$posrcprefix& \\\\@" < $(srcdir)/$@.in	\
		| sed -e '$$s/\\$$//') > $@-t \
	    && chmod a-w $@-t \
	    && mv $@-t $@ )

Makefile: Makefile.in.in ../config.status POTFILES
	cd .. \
	  && $(SHELL) ./config.status $(subdir)/$@.in

# Tell versions [3.59,3.63) of GNU make not to export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
