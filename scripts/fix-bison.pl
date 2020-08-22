#!/usr/bin/perl
#
# Format output code generated by bison
#
# Usage:
#  bison -b foo_parser -p foo_parser_ -d -v foo_parser.y
#  perl fix-bison.pl foo_parser.tab.c > $tmp
#  mv $tmp foo_parser.tab.c
#
# Copyright (C) 2004-2014, David Beckett http://www.dajobe.org/
# Copyright (C) 2004, University of Bristol, UK http://www.bristol.ac.uk/
#

my $seen_yyerrlab1=0;
my $syntax_error_has_default=0;
my $line_offset=1; # #line directives always refer to the NEXT line
while(<>) {
  # Remove code that causes a warning
  if(/Suppress GCC warning that yyerrlab1/) {
    do {
      $_=<>;
      $line_offset--; # skipped a line
    } while(!/^\#endif/);
    $line_offset--; # skipped a line
    next;
  }

  $seen_yyerrlab1=1 if /goto yyerrlab1/;

  s/^yyerrlab1:// unless $seen_yyerrlab1;

  # If using Bison 3.7+, api.header.include has a default value and it will
  # generate C code that #includes the header. But we can't tell it the right
  # final header name in the .y file or we lose support for Bison <3.4. So we
  # fix up any mention of the header file happens to occur, instead.
  s/_parser\.tab\.h/_parser.h/g;

  # Do not use macro name for a temporary variable
  s/unsigned int yylineno = /unsigned int yylineno_tmp = /;
  s/yyrule - 1, yylineno\)/yyrule - 1, yylineno_tmp\)/;

  # Do not (re)define prototypes that the system did better
  if(m%^void \*malloc\s*\(%) {
    $line_offset--; # skipped a line
    next;
  }
  if(m%^void free\s*\(%) {
    $line_offset--; # skipped a line
    next; 
  }

  # syntax error handler will have a default case already in Bison 3.0.5+
  $syntax_error_has_default=1 if /default: \/\* Avoid compiler warnings. \*\//;

  if(m%^\# undef YYCASE_$% and $syntax_error_has_default==0) {
    # Add a default value for yyformat on Bison <3.0.5, for coverity CID 10838
    my $line=$_;
    print qq{      default: yyformat = YY_("syntax error");\n};
    $line_offset++; # extra line
    print $line;
    next;
  }

  if(m%yysyntax_error_status = YYSYNTAX_ERROR%) {
    # Set yytoken to non-negative value for coverity CID 29259
    my $line=$_;
    print qq{if(yytoken < 0) yytoken = YYUNDEFTOK;\n};
    $line_offset++; # extra line
    print $line;
    next;
  }

  # Suppress warnings about empty declarations
  s/(^static int .*_init_globals.*);$/$1/;

  # Remove always false condition
  if(m%if \(/\*CONSTCOND\*/ 0\)%) {
    $line_offset--; # skipped a line
    $_ = <>;
    $line_offset--; # skipped a line
    next;
  }

  # Remove always false condition; this macro is #defined to 0
  if(m%if \(yytable_value_is_error \(yyn\)\)%) {
    $line_offset--; # skipped a line
    $_ = <>;
    $line_offset--; # skipped a line
    next;
  }



  # Fixup pending filename renaming, see above.
  # Fix line numbers.
  my $line=$. +$line_offset;
  s/^(\#line) \d+ (.*)\.tab\.c/$1 $line $2.c/;

  print;
}
