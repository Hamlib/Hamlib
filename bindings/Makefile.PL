use ExtUtils::MakeMaker;
WriteMakefile(
    'NAME'		=> 'Hamlib',
    ($] >= 5.005 ?    ## Add these new keywords supported since 5.005
      (#ABSTRACT_FROM => 'Hamlib.pm', # retrieve abstract from module
       AUTHOR     => 'Stephane Fillod') : ()),
	# should use libtool somehow. Otherwise try -Wl,--rpath 
    #'LIBS'		=> ['-lhamlib'],
    #'DEFINE'		=> '',
    #'INC'		=> '',
    #'OBJECT'		=> 'hamlibperl_wrap.o', # link all the C files too
);
