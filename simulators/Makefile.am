# Current Autotools documentation suggests that DejaGNU is obsolete
# and replaced by Autotest.  TODO: implement Autotest

# AUTOMAKE_OPTIONS = dejagnu
# DEJATOOL = testfreq testbcd testloc rigctl

DISTCLEANFILES = 

bin_PROGRAMS = 

check_PROGRAMS = \
	simatd578 \
	simeasycomm \
	simelecraft \
	simelecraftk4 \
	simflex \
	simft1000 \
	simft450 \
	simft710 \
	simft736 \
	simft747gx \
	simft817 \
	simft818 \
	simft847 \
	simft897 \
	simft990 \
	simft991 \
	simftdx101 \
	simftdx1200 \
	simftdx3000 \
	simftdx5000 \
	simic2730 \
	simic275 \
	simic7000 \
	simic705 \
	simic7100 \
	simic7200 \
	simic7300 \
	simic7600 \
	simic7610 \
	simic7700 \
	simic7851 \
	simic905 \
	simic910 \
	simic9100 \
	simic9700 \
	simicgeneric \
	simicr8600 \
	simid5100 \
	simjupiter \
	simkenwood \
	simmicom \
	simorion \
	simpmr171 \
	simpowersdr \
	simpstrotator \
	simqrplabs \
	simrotorez \
	simspid \
	simtmd700 \
	simtmd710 \
	simtrusdx \
	simts450 \
	simts590 \
	simts890 \
	simts950 \
	simts990 \
	simxiegug90 \
	simxiegux108g \
	simxiegux6100 \
	simyaesu

# include generated include files ahead of any in sources
#rigctl_CPPFLAGS = -I$(top_builddir)/tests -I$(top_builddir)/src -I$(srcdir) $(AM_CPPFLAGS)

# all the programs need this
LDADD = $(top_builddir)/src/libhamlib.la $(top_builddir)/lib/libmisc.la $(DL_LIBS)

simelecraft_CFLAGS = $(AM_CFLAGS) $(LIBXML2_CFLAGS) -I$(top_builddir)/src
simkenwood_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src
simyaesu_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src
simid5100_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src

simelecraft_LDADD = $(PTHREAD_LIBS) $(READLINE_LIBS) $(LDADD)
simkenwood_LDADD = $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
simyaesu_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
simid5100_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)

# Linker options
simelecraft_LDFLAGS = $(WINEXELDFLAGS)
simkenwood_LDFLAGS = $(WINEXELDFLAGS)
simyaesu_LDFLAGS = $(WINEXELDFLAGS)
simid5100_LDFLAGS = $(WINEXELDFLAGS)

EXTRA_DIST = sim.h simft990.dat

# Support 'make check' target for simple tests
#check_SCRIPTS = 

#TESTS = $(check_SCRIPTS)

$(top_builddir)/src/libhamlib.la:
	$(MAKE) -C $(top_builddir)/src/ libhamlib.la

$(top_builddir)/lib/libmisc.la:
	$(MAKE) -C $(top_builddir)/lib/ libmisc.la
