# hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
#
# Makefile - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
# TOP LEVEL Makefile for a set of  API's for communicating
# via serial interface to ham radio equipment via serial interface.
#
#
# Top Level Make file for shared lib  suite.
#
#
#	$Id: Makefile,v 1.2 2000-09-25 00:02:11 javabear Exp $	
#
# See common/Makefile for making libhamlib.so -- FS
#
#

#for time stamping releases
REL_NAME :=	$(shell date +%Y.%m.%d.%s)


all:
	(cd ft747 && $(MAKE) all)
	(cd ft847 && $(MAKE) all)
	(cd common && $(MAKE) all)


# Build all libs, install locally, and make test suite
# but dont run..

.PHONY:	build_all
build_all:
	(cd common && $(MAKE) all)
	(cd ft747 && $(MAKE) build_all)
	(cd ft847 && $(MAKE) build_all)


# Clean

.PHONY:	cleanall
cleanall:
	(cd ft747 && $(MAKE) cleanall)
	(cd ft847 && $(MAKE) cleanall)
	(cd common && $(MAKE) clean)


# Build all libs, install locally and test suite
# and run..

# .PHONY:	verify
# verify:
# 	(cd common && $(MAKE) all)
# 	(cd ft747 && $(MAKE) verify)
# 	(cd ft847 && $(MAKE) verify)



# Basis tar.tgz distribution with embedded date info. Store in
# parent directory.
 
.PHONY: dist
dist:
	make cleanall
	tar -zcvf ../Hamlib.${REL_NAME}.tgz *



