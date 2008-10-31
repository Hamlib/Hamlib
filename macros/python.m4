dnl
dnl Copyright 2003 Free Software Foundation, Inc.
dnl 
dnl This file is part of Hamlib, actually cloned from GNU Radio
dnl 
dnl GNU Radio is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2, or (at your option)
dnl any later version.
dnl 
dnl GNU Radio is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl 
dnl You should have received a copy of the GNU General Public License
dnl along with GNU Radio; see the file COPYING.  If not, write to
dnl the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
dnl Boston, MA 02111-1307, USA.
dnl 

# PYTHON_DEVEL()
#
# Checks for Python and tries to get the include path to 'Python.h'.
# It provides the $(PYTHON_CPPFLAGS) output variable.
AC_DEFUN([PYTHON_DEVEL],[
	AM_PATH_PYTHON([1.5],, [:])

	# Check for Python include path
	AC_MSG_CHECKING([for Python include path])
	if test "$PYTHON" = : -o -z "$PYTHON" ; then
		AC_MSG_WARN([cannot find Python path])
	else
	python_path=${PYTHON%/bin*}
	for i in "$python_path/include/python$PYTHON_VERSION/" "$python_path/include/python/" "$python_path/" ; do
		python_path=`find $i -type f -name Python.h -print`
		if test -n "$python_path" ; then
			break
		fi
	done
	for i in $python_path ; do
		python_path=${python_path%/Python.h}
		break
	done
	AC_MSG_RESULT([$python_path])
	if test -z "$python_path" ; then
		AC_MSG_WARN([cannot find Python include path])
	else
		AC_SUBST(PYTHON_CPPFLAGS,[-I$python_path])

		# Check for Python headers usability
		python_save_CPPFLAGS=$CPPFLAGS
		CPPFLAGS="$CPPFLAGS $PYTHON_CPPFLAGS"
		AC_CHECK_HEADERS([Python.h],, [cf_with_python_devel=no])
		CPPFLAGS="$python_save_CPPFLAGS"

	if test "$cf_with_python_devel" != "no" ; then

	# Check for Python library path
	AC_MSG_CHECKING([for Python library path])
	python_path=`echo $PYTHON | sed "s,/bin.*$,,"`
	for i in "$python_path/lib/python$PYTHON_VERSION/config/" "$python_path/lib/python$PYTHON_VERSION/" "$python_path/lib/python/config/" "$python_path/lib/python/" "$python_path/" ; do
		python_path=`find $i -type f -name libpython$PYTHON_VERSION.* -print | sed "1q"`
		if test -n "$python_path" ; then
			break
		fi
	done
	python_path=`echo $python_path | sed "s,/libpython.*$,,"`
	AC_MSG_RESULT([$python_path])
	if test -z "$python_path" ; then
		AC_MSG_WARN([cannot find Python library path])
	fi
	AC_SUBST([PYTHON_LDFLAGS],["-L$python_path -lpython$PYTHON_VERSION"])
	#
	python_site=`echo $python_path | sed "s/config/site-packages/"`
	AC_SUBST([PYTHON_SITE_PKG],[$python_site])
	#
	# libraries which must be linked in when embedding
	#
	AC_MSG_CHECKING(python extra libraries)
	PYTHON_EXTRA_LIBS=`$PYTHON -c "import distutils.sysconfig; \
                conf = distutils.sysconfig.get_config_var; \
                print conf('LOCALMODLIBS')+' '+conf('LIBS')"
	AC_MSG_RESULT($PYTHON_EXTRA_LIBS)`
	AC_SUBST(PYTHON_EXTRA_LIBS)


		# Check for Python library usability
		python_save_LDFLAGS=$LDFLAGS
		LDFLAGS="$LDFLAGS $PYTHON_LDFLAGS"
		AC_CHECK_FUNC(PyArg_Parse, 
			[cf_with_python_devel=yes], 
			[cf_with_python_devel=no])

		LDFLAGS="$python_save_LDFLAGS"

	fi


	fi
	fi


])
