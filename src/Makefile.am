# src/Makefile.am

BUILT_SOURCES = $(builddir)/hamlibdatetime.h

DISTCLEANFILES = hamlibdatetime.h

RIGSRC = hamlibdatetime.h rig.c serial.c serial.h misc.c misc.h register.c register.h event.c \
	event.h cal.c cal.h conf.c tones.c tones.h rotator.c locator.c rot_reg.c \
	rot_conf.c rot_conf.h rot_settings.c rot_ext.c iofunc.c iofunc.h ext.c \
   	mem.c settings.c parallel.c parallel.h usb_port.c usb_port.h debug.c \
   	network.c network.h cm108.c cm108.h gpio.c gpio.h idx_builtin.h token.h \
   	par_nt.h microham.c microham.h amplifier.c amp_reg.c amp_conf.c \
   	amp_conf.h amp_settings.c extamp.c sleep.c sleep.h sprintflst.c \
   	sprintflst.h cache.c cache.h snapshot_data.c snapshot_data.h

lib_LTLIBRARIES = libhamlib.la
libhamlib_la_SOURCES = $(RIGSRC)
libhamlib_la_LDFLAGS = $(WINLDFLAGS) $(OSXLDFLAGS) -no-undefined -version-info $(ABI_VERSION):$(ABI_REVISION):$(ABI_AGE)

libhamlib_la_LIBADD = $(top_builddir)/lib/libmisc.la $(top_builddir)/security/libsecurity.la \
	$(BACKENDEPS) $(RIG_BACKENDEPS) $(ROT_BACKENDEPS) $(AMP_BACKENDEPS) $(NET_LIBS) $(MATH_LIBS) $(LIBUSB_LIBS) $(INDI_LIBS)

libhamlib_la_DEPENDENCIES = $(top_builddir)/lib/libmisc.la $(top_builddir)/security/libsecurity.la $(BACKENDEPS) $(RIG_BACKENDEPS) $(ROT_BACKENDEPS) $(AMP_BACKENDEPS)

EXTRA_DIST = Android.mk hamlibdatetime.h.in

# If we have  a .git directory then we will  generate the hamlibdate.h
# file and  replace it if it  is different. Fall  back to a copy  of a
# generic hamlibdatetime.h.in in the source tree. Build looks in build
# directory  before  the  source directory  for  the  hamlibdatetime.h
# header.
hamlibdatetime.h: FORCE
	echo "#define HAMLIBDATETIME "\"$$(TZ=UTC git --git-dir=$(top_srcdir)/.git log --no-merges --date='format-local:%b %d %H:%M:%SZ %Y SHA=' --format='%cd' -n 1)$$(git --git-dir=$(top_srcdir)/.git log --no-merges -n 1 | cut -c8-13)\"  
	@if test -x $(top_srcdir)/.git ; then \
		echo "/* This date time is from the last non-merge commit to Hamlib. */" > $(builddir)/$(@F).tmp ;\
		echo "#define HAMLIBDATETIME "\"$$(TZ=UTC git --git-dir=$(top_srcdir)/.git log --no-merges --date='format-local:%b %d %H:%M:%SZ %Y SHA=' --format='%cd' -n 1)$$(git --git-dir=$(top_srcdir)/.git log --no-merges -n 1 | head -n 1 | cut -c8-13)\"  >> $(builddir)/$(@F).tmp ;\
		diff -qN $(builddir)/$(@F).tmp $(builddir)/$(@F) ; test $$? -eq 0 || { echo "Generating SCS header \"$(builddir)/$(@F)\"" ; mv -f $(builddir)/$(@F).tmp $(builddir)/$(@F) ; } ;\
		rm -f $(builddir)/$(@F).tmp ;\
	else \
		test -f $(srcdir)/$(@F) || cp $(srcdir)/$(@F).in $(srcdir)/$(@F) ;\
	fi

FORCE:

# If we are making a  distribution out-of-source and we have generated
# a  hamlibdatetime.h; then  copy it  to  the tests  directory of  the
# source  tarball  so  that  downstream builds  pick  up  the  version
# information we know about.
dist-hook:
	test ./ -ef $(srcdir)/ || test ! -f hamlibdatetime.h || cp -f hamlibdatetime.h $(srcdir)/
