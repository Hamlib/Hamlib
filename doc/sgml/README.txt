README.txt - the companion answer file for the Hamlib SGML source 
distribution.  Copyright (C) 2001 by Nathan Bargmann n0nb@arrl.net
under the terms of the GNU General Public License version 2.0.

Notes for hamlib-doc for version 1.1.0 (ALPHA)

GENERAL:

      This is the initial release of hamlib-doc, version 0.1.1.  Covered
      topics include licensing of Hamlib, where and how to get the latest
      stable and CVS versions, introductory material on building, and
      API reference.
      
      Generated formats are HTML, PS and PDF at this time.

BUILDING:

      The source files are SGML marked up text that validates against the
      Docbook 3.1 DTD.  Build environment is Debian GNU/Linux Woody a.ka. 
      3.0 and required tools are Jade and nsgmls and the Cynus stylesheets.
      If you plan to use a Debian system I strongly recommend doing it on
      a Woody box as I had a number of issues with the output generated
      by the tools in the Potato release.
      
      The proper DTD is required.  Installing the sgml-tools task from 
      dselect or apt-get will get all the DTDs and catalogs you'd ever 
      need installed.
      
  PDF:
      The PDF is built with the Cygnus db2pdf script, however, there is an
      included Docbook stylesheet, hamlib-doc.dsl.  Invoke the db2pdf script
      in the same directory as the .sgml files with -d option:

	  db2pdf -d hamlib-doc hamlib-doc.sgml
	  
      and you'll wind up with a nice .pdf file.

  HTML:
      To generate the HTML files I use a similar script called db2html that
      is invoked as above.  The script will create and place the HTML and
      other files in a subdirectory.

  PS:
      Same as above except the script in Woody won't accept the -d option for
      a custom DSSSL file.

  SGML:
      Any editor may be used to edit the SGML source files.  After editing,
      the SGML needs to be validated to ensure proper element placement and 
      typos in the SGML structure.  An excellent tool to do this is nsgmls.  
      You can validate the files from the directory where the files are 
      stored:

	nsgmls -sv hamlib.sgml	    # validates the sgml

      If there are no validation issues, nsgmls will return a prompt with
      no output except its version.

BUGS:

      Bugs?  What bugs?  All bugs were taken out and shot!

      I wish...

      To enable links in the reference section I used brute force and
      marked up hyperlinks into the text flow.  The .pdf has the hyperlinks
      in the text, but they're not annotated in any special way except
      beginning with http:.

      Probably more as I'm too tired to look for them...
      
TODO:

      Sections covering Hamlib usage in a program, writing a backend,
      and Hamlib internals.

      Document top-level structures used by the API.

      Sync with CVS version.
      
MISC:

      I appreciate all feedback.  Assistance won't be ignored either!
      Write me at n0nb@arrl.net

      73, de Nate >>

      
