use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'		=> 'Hamlib',
    'VERSION_FROM'	=> 'Hamlib.pm', # finds $VERSION
    'PREREQ_PM'		=> {}, # e.g., Module::Name => 1.1
    ($] >= 5.005 ?    ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM => 'Hamlib.pm', # retrieve abstract from module
       AUTHOR     => 'S. Fillod <fillods@users.sf.net>') : ()),
	# should use libtool somehow. Otherwise try -Wl,--rpath 
    #'LIBS'		=> ['-L../../src/.libs -lhamlib'],
    'DEFINE'		=> '',
    #'INC'		=> '-I.',
	'dist'      => {'COMPRESS'=>'gzip -9f', 'SUFFIX' => 'gz',
	                'ZIP'=>'/usr/bin/zip','ZIPFLAGS'=>'-rl'},
	# Un-comment this if you add C files to link with later:
    # 'OBJECT'		=> '$(O_FILES)', # link all the C files too
);
