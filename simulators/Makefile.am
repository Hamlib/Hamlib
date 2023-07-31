# Current Autotools documentation suggests that DejaGNU is obsolete
# and replaced by Autotest.  TODO: implement Autotest

# AUTOMAKE_OPTIONS = dejagnu
# DEJATOOL = testfreq testbcd testloc rigctl

DISTCLEANFILES = 

bin_PROGRAMS = 

check_PROGRAMS = simelecraft simicgeneric simkenwood simyaesu simic9100 simic9700 simft991 simftdx1200 simftdx3000 simjupiter simpowersdr simid5100 simft736 simftdx5000 simtmd700 simrotorez simspid simft817 simts590 simft847 simic7300 simic7000 simic7100 simic7200 simatd578 simic905 simts450 simic7600 simic7610 simic705

simelecraft_SOURCES = simelecraft.c 
simkenwood_SOURCES = simkenwood.c 
simyaesu_SOURCES = simyaesu.c 
simid5100_SOURCES = simid5100.c

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

EXTRA_DIST = 

# Support 'make check' target for simple tests
#check_SCRIPTS = 

#TESTS = $(check_SCRIPTS)


CLEANFILES = simelelecraft simicgeneric simkenwood simyaesu
