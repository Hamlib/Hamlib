#------------------------------------------------------------------------
# SC_PATH_PERLINC --
#
#	Locate the perl include files 
#
# Arguments:
#	none
#
# Results:
#
#	Adds the following arguments to configure:
#		--with-perl-inc=...
#
#	Defines the following vars:
#		PERL_INC_DIR	Full path to the directory containing
#				the perl include files
#------------------------------------------------------------------------

AC_DEFUN([SC_PATH_PERLINC], [

	# we reset no_perl in case something fails here
	no_perl=true

    AC_ARG_WITH(perl-inc, [  --with-perl-inc         directory containing perl includes], with_perl_inc=${withval})
	AC_MSG_CHECKING([for perl headers])
	AC_CACHE_VAL(ac_cv_c_perl_inc,[

	    # First check to see if --with-perl-inc was specified.
	    if test x"${with_perl_inc}" != x ; then
		if test -f "${with_perl_inc}/perl.h" ; then
		    ac_cv_c_perl_inc=`(cd ${with_perl_inc}; pwd)`
		else
		    AC_MSG_ERROR([${with_perl_inc} directory doesn't contain perl.h])
		fi
	    fi

	    # then check for a private Perl installation
	    if test x"${ac_cv_c_perl_inc}" = x ; then
	    	eval `perl -V:archlibexp`
		if test -f "${archlibexp}/CORE/perl.h" ; then
		    ac_cv_c_perl_inc=`(cd ${archlibexp}/CORE; pwd)`
		else
		    AC_MSG_WARN([${with_perl_inc} directory doesn't contain perl.h])
		fi
	    fi
	])

	if test x"${ac_cv_c_perl_inc}" = x ; then
	    PERL_INC_DIR=
	    AC_MSG_WARN(Can't find Perl header files)
	else
	    no_perl=
	    PERL_INC_DIR=${ac_cv_c_perl_inc}
	    AC_MSG_RESULT(found $PERL_INC_DIR)
	fi
    AC_SUBST(PERL_INC_DIR)
])

