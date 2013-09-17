#
# Copyright 2001 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

MTSRC = module_4937.c module_4702.c

GRIO_SOURCES = 		\
	i2cio.cc i2cio.h		\
	i2cio_pp.cc i2cio_pp.h		\
	i2c.cc i2c.h			\
	microtune_4937.cc microtune_4937.h	\
	microtune_4702.cc microtune_4702.h	\
	microtune_eval_board.cc microtune_eval_board.h \
	microtune_eval_board_defs.h

noinst_LTLIBRARIES = libhamlib-microtune.la
libhamlib_microtune_la_SOURCES =  $(MTSRC) $(GRIO_SOURCES) microtune.cc microtune.h
hamlib_microtune_la_LIBADD = -lstdc++
