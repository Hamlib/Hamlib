# Builds Hamlib-pl.mk and is invoked from Makefile

use ExtUtils::MakeMaker;
WriteMakefile(
	'NAME'		=> 'Hamlib',
	# Add these new keywords supported since 5.005
	($] >= 5.005 ?
	(
	#ABSTRACT_FROM	=> 'Hamlib.pm', # retrieve abstract from module
	AUTHOR		=> 'Stephane Fillod and the Hamlib Group') : ()),
	
	# should use libtool somehow. Otherwise try -Wl,--rpath 
	#'LIBS'		=> ['-lhamlib'],
	#'DEFINE'	=> '',
	#'INC'		=> '',
	#'OBJECT'	=> 'hamlibperl_wrap.o', # link all the C files too
);
