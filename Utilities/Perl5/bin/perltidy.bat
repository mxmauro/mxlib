@rem = '--*-Perl-*--
@set "ErrorLevel="
@if "%OS%" == "Windows_NT" @goto WinNT
@perl -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
@set ErrorLevel=%ErrorLevel%
@goto endofperl
:WinNT
@perl -x -S %0 %*
@set ErrorLevel=%ErrorLevel%
@if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" @goto endofperl
@if %ErrorLevel% == 9009 @echo You do not have Perl in your PATH.
@goto endofperl
@rem ';
#!/usr/bin/perl
#line 30
package main;

use Perl::Tidy;

my $arg_string = undef;

# give Macs a chance to provide command line parameters
if ( $^O =~ /Mac/ ) {
    $arg_string = MacPerl::Ask(
        'Please enter @ARGV (-h for help)',
        defined $ARGV[0] ? "\"$ARGV[0]\"" : ""
    );
}

# Exit codes returned by perltidy:
#    0 = no errors
#    1 = perltidy could not run to completion due to errors
#    2 = perltidy ran to completion with error messages
exit Perl::Tidy::perltidy( argv => $arg_string );

__END__

=head1 NAME

perltidy - a perl script indenter and reformatter

=head1 SYNOPSIS

    perltidy [ options ] file1 file2 file3 ...
            (output goes to file1.tdy, file2.tdy, file3.tdy, ...)
    perltidy [ options ] file1 -o outfile
    perltidy [ options ] file1 -st >outfile
    perltidy [ options ] <infile >outfile

=head1 DESCRIPTION

Perltidy reads a perl script and writes an indented, reformatted script.  The
formatting process involves converting the script into a string of tokens,
removing any non-essential whitespace, and then rewriting the string of tokens
with whitespace using whatever rules are specified, or defaults.  This happens
in a series of operations which can be controlled with the parameters described
in this document.

Perltidy is a commandline frontend to the module Perl::Tidy.  For documentation
describing how to call the Perl::Tidy module from other applications see the
separate documentation for Perl::Tidy.  It is the file Perl::Tidy.pod in the source distribution.

Many users will find enough information in L<"EXAMPLES"> to get
started.  New users may benefit from the short tutorial
which can be found at
http://perltidy.sourceforge.net/tutorial.html

A convenient aid to systematically defining a set of style parameters
can be found at
http://perltidy.sourceforge.net/stylekey.html

Perltidy can produce output on either of two modes, depending on the
existence of an B<-html> flag.  Without this flag, the output is passed
through a formatter.  The default formatting tries to follow the
recommendations in perlstyle(1), but it can be controlled in detail with
numerous input parameters, which are described in L<"FORMATTING OPTIONS">.

When the B<-html> flag is given, the output is passed through an HTML
formatter which is described in L<"HTML OPTIONS">.

=head1 EXAMPLES

  perltidy somefile.pl

This will produce a file F<somefile.pl.tdy> containing the script reformatted
using the default options, which approximate the style suggested in
perlstyle(1).  The source file F<somefile.pl> is unchanged.

  perltidy *.pl

Execute perltidy on all F<.pl> files in the current directory with the
default options.  The output will be in files with an appended F<.tdy>
extension.  For any file with an error, there will be a file with extension
F<.ERR>.

  perltidy -b file1.pl file2.pl

Modify F<file1.pl> and F<file2.pl> in place, and backup the originals to
F<file1.pl.bak> and F<file2.pl.bak>.  If F<file1.pl.bak> and/or F<file2.pl.bak>
already exist, they will be overwritten.

  perltidy -b -bext='/' file1.pl file2.pl

Same as the previous example except that the backup files F<file1.pl.bak> and F<file2.pl.bak> will be deleted if there are no errors.

  perltidy -gnu somefile.pl

Execute perltidy on file F<somefile.pl> with a style which approximates the
GNU Coding Standards for C programs.  The output will be F<somefile.pl.tdy>.

  perltidy -i=3 somefile.pl

Execute perltidy on file F<somefile.pl>, with 3 columns for each level of
indentation (B<-i=3>) instead of the default 4 columns.  There will not be any
tabs in the reformatted script, except for any which already exist in comments,
pod documents, quotes, and here documents.  Output will be F<somefile.pl.tdy>.

  perltidy -i=3 -et=8 somefile.pl

Same as the previous example, except that leading whitespace will
be entabbed with one tab character per 8 spaces.

  perltidy -ce -l=72 somefile.pl

Execute perltidy on file F<somefile.pl> with all defaults except use "cuddled
elses" (B<-ce>) and a maximum line length of 72 columns (B<-l=72>) instead of
the default 80 columns.

  perltidy -g somefile.pl

Execute perltidy on file F<somefile.pl> and save a log file F<somefile.pl.LOG>
which shows the nesting of braces, parentheses, and square brackets at
the start of every line.

  perltidy -html somefile.pl

This will produce a file F<somefile.pl.html> containing the script with
html markup.  The output file will contain an embedded style sheet in
the <HEAD> section which may be edited to change the appearance.

  perltidy -html -css=mystyle.css somefile.pl

This will produce a file F<somefile.pl.html> containing the script with
html markup.  This output file will contain a link to a separate style
sheet file F<mystyle.css>.  If the file F<mystyle.css> does not exist,
it will be created.  If it exists, it will not be overwritten.

  perltidy -html -pre somefile.pl

Write an html snippet with only the PRE section to F<somefile.pl.html>.
This is useful when code snippets are being formatted for inclusion in a
larger web page.  No style sheet will be written in this case.

  perltidy -html -ss >mystyle.css

Write a style sheet to F<mystyle.css> and exit.

  perltidy -html -frm mymodule.pm

Write html with a frame holding a table of contents and the source code.  The
output files will be F<mymodule.pm.html> (the frame), F<mymodule.pm.toc.html>
(the table of contents), and F<mymodule.pm.src.html> (the source code).

=head1 OPTIONS - OVERVIEW

The entire command line is scanned for options, and they are processed
before any files are processed.  As a result, it does not matter
whether flags are before or after any filenames.  However, the relative
order of parameters is important, with later parameters overriding the
values of earlier parameters.

For each parameter, there is a long name and a short name.  The short
names are convenient for keyboard input, while the long names are
self-documenting and therefore useful in scripts.  It is customary to
use two leading dashes for long names, but one may be used.

Most parameters which serve as on/off flags can be negated with a
leading "n" (for the short name) or a leading "no" or "no-" (for the
long name).  For example, the flag to outdent long quotes is B<-olq>
or B<--outdent-long-quotes>.  The flag to skip this is B<-nolq>
or B<--nooutdent-long-quotes> or B<--no-outdent-long-quotes>.

Options may not be bundled together.  In other words, options B<-q> and
B<-g> may NOT be entered as B<-qg>.

Option names may be terminated early as long as they are uniquely identified.
For example, instead of B<--dump-token-types>, it would be sufficient to enter
B<--dump-tok>, or even B<--dump-t>, to uniquely identify this command.

=head2 I/O Control

The following parameters concern the files which are read and written.

=over 4

=item B<-h>,    B<--help>

Show summary of usage and exit.

=item	B<-o>=filename,    B<--outfile>=filename

Name of the output file (only if a single input file is being
processed).  If no output file is specified, and output is not
redirected to the standard output (see B<-st>), the output will go to
F<filename.tdy>. [Note: - does not redirect to standard output. Use
B<-st> instead.]

=item	B<-st>,    B<--standard-output>

Perltidy must be able to operate on an arbitrarily large number of files
in a single run, with each output being directed to a different output
file.  Obviously this would conflict with outputting to the single
standard output device, so a special flag, B<-st>, is required to
request outputting to the standard output.  For example,

  perltidy somefile.pl -st >somefile.new.pl

This option may only be used if there is just a single input file.
The default is B<-nst> or B<--nostandard-output>.

=item	B<-se>,    B<--standard-error-output>

If perltidy detects an error when processing file F<somefile.pl>, its
default behavior is to write error messages to file F<somefile.pl.ERR>.
Use B<-se> to cause all error messages to be sent to the standard error
output stream instead.  This directive may be negated with B<-nse>.
Thus, you may place B<-se> in a F<.perltidyrc> and override it when
desired with B<-nse> on the command line.

=item	B<-oext>=ext,    B<--output-file-extension>=ext

Change the extension of the output file to be F<ext> instead of the
default F<tdy> (or F<html> in case the -B<-html> option is used).
See L<"Specifying File Extensions">.

=item	B<-opath>=path,    B<--output-path>=path

When perltidy creates a filename for an output file, by default it merely
appends an extension to the path and basename of the input file.  This
parameter causes the path to be changed to F<path> instead.

The path should end in a valid path separator character, but perltidy will try
to add one if it is missing.

For example

 perltidy somefile.pl -opath=/tmp/

will produce F</tmp/somefile.pl.tdy>.  Otherwise, F<somefile.pl.tdy> will
appear in whatever directory contains F<somefile.pl>.

If the path contains spaces, it should be placed in quotes.

This parameter will be ignored if output is being directed to standard output,
or if it is being specified explicitly with the B<-o=s> parameter.

=item	B<-b>,    B<--backup-and-modify-in-place>

Modify the input file or files in-place and save the original with the
extension F<.bak>.  Any existing F<.bak> file will be deleted.  See next
item for changing the default backup extension, and for eliminating the
backup file altogether.

B<Please Note>: Writing back to the input file increases the risk of data loss
or corruption in the event of a software or hardware malfunction. Before using
the B<-b> parameter please be sure to have backups and verify that it works
correctly in your environment and operating system.

A B<-b> flag will be ignored if input is from standard input or goes to
standard output, or if the B<-html> flag is set.

In particular, if you want to use both the B<-b> flag and the B<-pbp>
(--perl-best-practices) flag, then you must put a B<-nst> flag after the
B<-pbp> flag because it contains a B<-st> flag as one of its components,
which means that output will go to the standard output stream.

=item	B<-bext>=ext,    B<--backup-file-extension>=ext

This parameter serves two purposes: (1) to change the extension of the backup
file to be something other than the default F<.bak>, and (2) to indicate
that no backup file should be saved.

To change the default extension to something other than F<.bak> see
L<"Specifying File Extensions">.

A backup file of the source is always written, but you can request that it
be deleted at the end of processing if there were no errors.  This is risky
unless the source code is being maintained with a source code control
system.

To indicate that the backup should be deleted include one forward slash,
B</>, in the extension.  If any text remains after the slash is removed
it will be used to define the backup file extension (which is always
created and only deleted if there were no errors).

Here are some examples:

  Parameter           Extension          Backup File Treatment
  <-bext=bak>         F<.bak>            Keep (same as the default behavior)
  <-bext='/'>         F<.bak>            Delete if no errors
  <-bext='/backup'>   F<.backup>         Delete if no errors
  <-bext='original/'> F<.original>       Delete if no errors

=item B<-bm=s>,  B<--backup-method=s>

This parameter should not normally be used but is available in the event that
problems arise as a transition is made from an older implementation of the
backup logic to a newer implementation.  The newer implementation is the
default and is specified with B<-bm='copy'>. The older implementation is
specified with B<-bm='move'>.  The difference is that the older implementation
made the backup by moving the input file to the backup file, and the newer
implementation makes the backup by copying the input file.  The newer
implementation preserves the file system B<inode> value. This may avoid
problems with other software running simultaneously.  This change was made
as part of issue B<git #103> at github.

=item B<-w>,    B<--warning-output>

Setting B<-w> causes any non-critical warning
messages to be reported as errors.  These include messages
about possible pod problems, possibly bad starting indentation level,
and cautions about indirect object usage.  The default, B<-nw> or
B<--nowarning-output>, is not to include these warnings.

=item B<-q>,    B<--quiet>

Deactivate error messages (for running under an editor).

For example, if you use a vi-style editor, such as vim, you may execute
perltidy as a filter from within the editor using something like

 :n1,n2!perltidy -q

where C<n1,n2> represents the selected text.  Without the B<-q> flag,
any error message may mess up your screen, so be prepared to use your
"undo" key.

=item B<-log>,    B<--logfile>

Save the F<.LOG> file, which has many useful diagnostics.  Perltidy always
creates a F<.LOG> file, but by default it is deleted unless a program bug is
suspected.  Setting the B<-log> flag forces the log file to be saved.

=item B<-g=n>, B<--logfile-gap=n>

Set maximum interval between input code lines in the logfile.  This purpose of
this flag is to assist in debugging nesting errors.  The value of C<n> is
optional.  If you set the flag B<-g> without the value of C<n>, it will be
taken to be 1, meaning that every line will be written to the log file.  This
can be helpful if you are looking for a brace, paren, or bracket nesting error.

Setting B<-g> also causes the logfile to be saved, so it is not necessary to
also include B<-log>.

If no B<-g> flag is given, a value of 50 will be used, meaning that at least
every 50th line will be recorded in the logfile.  This helps prevent
excessively long log files.

Setting a negative value of C<n> is the same as not setting B<-g> at all.

=item B<-npro>  B<--noprofile>

Ignore any F<.perltidyrc> command file.  Normally, perltidy looks first in
your current directory for a F<.perltidyrc> file of parameters.  (The format
is described below).  If it finds one, it applies those options to the
initial default values, and then it applies any that have been defined
on the command line.  If no F<.perltidyrc> file is found, it looks for one
in your home directory.

If you set the B<-npro> flag, perltidy will not look for this file.

=item B<-pro=filename> or  B<--profile=filename>

To simplify testing and switching .perltidyrc files, this command may be
used to specify a configuration file which will override the default
name of .perltidyrc.  There must not be a space on either side of the
'=' sign.  For example, the line

   perltidy -pro=testcfg

would cause file F<testcfg> to be used instead of the
default F<.perltidyrc>.

A pathname begins with three dots, e.g. ".../.perltidyrc", indicates that
the file should be searched for starting in the current directory and
working upwards. This makes it easier to have multiple projects each with
their own .perltidyrc in their root directories.

=item B<-opt>,   B<--show-options>

Write a list of all options used to the F<.LOG> file.
Please see B<--dump-options> for a simpler way to do this.

=item B<-f>,   B<--force-read-binary>

Force perltidy to process binary files.  To avoid producing excessive
error messages, perltidy skips files identified by the system as non-text.
However, valid perl scripts containing binary data may sometimes be identified
as non-text, and this flag forces perltidy to process them.

=item B<-ast>,   B<--assert-tidy>

This flag asserts that the input and output code streams are identical, or in
other words that the input code is already 'tidy' according to the formatting
parameters.  If this is not the case, an error message noting this is produced.
This error message will cause the process to return a non-zero exit code.
The test for this is made by comparing an MD5 hash value for the input and
output code streams. This flag has no other effect on the functioning of
perltidy.  This might be useful for certain code maintenance operations.
Note: you will not see this message if you have error messages turned off with the
-quiet flag.

=item B<-asu>,   B<--assert-untidy>

This flag asserts that the input and output code streams are different, or in
other words that the input code is 'untidy' according to the formatting
parameters.  If this is not the case, an error message noting this is produced.
This flag has no other effect on the functioning of perltidy.

=back

=head1 FORMATTING OPTIONS

=head2 Basic Options

=over 4

=item B<--notidy>

This flag disables all formatting and causes the input to be copied unchanged
to the output except for possible changes in line ending characters and any
pre- and post-filters.  This can be useful in conjunction with a hierarchical
set of F<.perltidyrc> files to avoid unwanted code tidying.  See also
L<"Skipping Selected Sections of Code"> for a way to avoid tidying specific
sections of code.

=item B<-i=n>,  B<--indent-columns=n>

Use n columns per indentation level (default n=4).

=item B<-l=n>, B<--maximum-line-length=n>

The default maximum line length is n=80 characters.  Perltidy will try
to find line break points to keep lines below this length. However, long
quotes and side comments may cause lines to exceed this length.

The default length of 80 comes from the past when this was the standard CRT
screen width.  Many programmers prefer to increase this to something like 120.

Setting B<-l=0> is equivalent to setting B<-l=(a very large number)>.  But this is
not recommended because, for example, a very long list will be formatted in a
single long line.

=item B<-vmll>, B<--variable-maximum-line-length>

A problem arises using a fixed maximum line length with very deeply nested code
and data structures because eventually the amount of leading whitespace used
for indicating indentation takes up most or all of the available line width,
leaving little or no space for the actual code or data.  One solution is to use
a very long line length.  Another solution is to use the B<-vmll> flag, which
basically tells perltidy to ignore leading whitespace when measuring the line
length.

To be precise, when the B<-vmll> parameter is set, the maximum line length of a
line of code will be M+L*I, where

      M is the value of --maximum-line-length=M (-l=M), default 80,
      I is the value of --indent-columns=I (-i=I), default 4,
      L is the indentation level of the line of code

When this flag is set, the choice of breakpoints for a block of code should be
essentially independent of its nesting depth.  However, the absolute line
lengths, including leading whitespace, can still be arbitrarily large.  This
problem can be avoided by including the next parameter.

The default is not to do this (B<-nvmll>).

=item B<-wc=n>, B<--whitespace-cycle=n>

This flag also addresses problems with very deeply nested code and data
structures.  When the nesting depth exceeds the value B<n> the leading
whitespace will be reduced and start at a depth of 1 again.  The result is that
blocks of code will shift back to the left rather than moving arbitrarily far
to the right.  This occurs cyclically to any depth.

For example if one level of indentation equals 4 spaces (B<-i=4>, the default),
and one uses B<-wc=15>, then if the leading whitespace on a line exceeds about
4*15=60 spaces it will be reduced back to 4*1=4 spaces and continue increasing
from there.  If the whitespace never exceeds this limit the formatting remains
unchanged.

The combination of B<-vmll> and B<-wc=n> provides a solution to the problem of
displaying arbitrarily deep data structures and code in a finite window,
although B<-wc=n> may of course be used without B<-vmll>.

The default is not to use this, which can also be indicated using B<-wc=0>.

=item B<Tabs>

Using tab characters will almost certainly lead to future portability
and maintenance problems, so the default and recommendation is not to
use them.  For those who prefer tabs, however, there are two different
options.

Except for possibly introducing tab indentation characters, as outlined
below, perltidy does not introduce any tab characters into your file,
and it removes any tabs from the code (unless requested not to do so
with B<-fws>).  If you have any tabs in your comments, quotes, or
here-documents, they will remain.

=over 4

=item B<-et=n>,   B<--entab-leading-whitespace>

This flag causes each B<n> leading space characters produced by the
formatting process to be replaced by one tab character.  The
formatting process itself works with space characters. The B<-et=n> parameter is applied
as a last step, after formatting is complete, to convert leading spaces into tabs.
Before starting to use tabs, it is essential to first get the indentation
controls set as desired without tabs, particularly the two parameters B<--indent-columns=n> (or B<-i=n>) and B<--continuation-indentation=n> (or B<-ci=n>).

The value of the integer B<n> can be any value but can be coordinated with the
number of spaces used for indentation. For example, B<-et=4 -ci=4 -i=4> will
produce one tab for each indentation level and and one for each continuation
indentation level.  You may want to coordinate the value of B<n> with what your
display software assumes for the spacing of a tab.

=item B<-t>,   B<--tabs>

This flag causes one leading tab character to be inserted for each level
of indentation.  Certain other features are incompatible with this
option, and if these options are also given, then a warning message will
be issued and this flag will be unset.  One example is the B<-lp>
option. This flag is retained for backwards compatibility, but
if you use tabs, the B<-et=n> flag is recommended.  If both B<-t> and
B<-et=n> are set, the B<-et=n> is used.

=item B<-dt=n>,   B<--default-tabsize=n>

If the first line of code passed to perltidy contains leading tabs but no
tab scheme is specified for the output stream then perltidy must guess how many
spaces correspond to each leading tab.  This number of spaces B<n>
corresponding to each leading tab of the input stream may be specified with
B<-dt=n>.  The default is B<n=8>.

This flag has no effect if a tab scheme is specified for the output stream,
because then the input stream is assumed to use the same tab scheme and
indentation spaces as for the output stream (any other assumption would lead to
unstable editing).

=back

=item B<-io>,   B<--indent-only>

This flag is used to deactivate all whitespace and line break changes
within non-blank lines of code.
When it is in effect, the only change to the script will be
to the indentation and to the number of blank lines.
And any flags controlling whitespace and newlines will be ignored.  You
might want to use this if you are perfectly happy with your whitespace
and line breaks, and merely want perltidy to handle the indentation.
(This also speeds up perltidy by well over a factor of two, so it might be
useful when perltidy is merely being used to help find a brace error in
a large script).

Setting this flag is equivalent to setting B<--freeze-newlines> and
B<--freeze-whitespace>.

If you also want to keep your existing blank lines exactly
as they are, you can add B<--freeze-blank-lines>.

With this option perltidy is still free to modify the indenting (and
outdenting) of code and comments as it normally would.  If you also want to
prevent long comment lines from being outdented, you can add either B<-noll> or
B<-l=0>.

Setting this flag will prevent perltidy from doing any special operations on
closing side comments.  You may still delete all side comments however when
this flag is in effect.


=item B<-enc=s>,  B<--character-encoding=s>

This flag indicates if the input data stream uses a character encoding.
Perltidy does not look for the encoding directives in the source stream, such
as B<use utf8>, and instead relies on this flag to determine the encoding.
(This is because perltidy often works on snippets of code rather than complete
files, so it cannot rely on B<use utf8> directives).  Consequently perltidy is
likely to encounter problems formatting a file which is only partially encoded.

The possible values for B<s> are:

 -enc=none if no encoding is used, or
 -enc=utf8 for encoding in utf8
 -enc=guess if perltidy should guess between these two possibilities.

The value B<none> causes the stream to be processed without special encoding
assumptions.  This is appropriate for files which are written in single-byte
character encodings such as latin-1.

The value B<utf8> causes the stream to be read and written as
UTF-8.  If the input stream cannot be decoded with this encoding then
processing is not done.

The value B<guess> tells perltidy to guess between either utf8 encoding or no
encoding (meaning one character per byte).  The B<guess> option uses the
Encode::Guess module which has been found to be reliable at detecting
if a file is encoded in utf8 or not.

The current default is B<guess>.

The abbreviations B<-utf8> or B<-UTF8> are equivalent to B<-enc=utf8>, and the
abbreviation B<-guess> is equivalent to B<-enc=guess>.  So to process a file
named B<file.pl> which is encoded in UTF-8 you can use:

   perltidy -utf8 file.pl

or

   perltidy -guess file.pl

or simply

   perltidy file.pl

since B<-guess> is the default.

To process files with an encoding other than UTF-8, it would be necessary to
write a short program which calls the Perl::Tidy module with some pre- and
post-processing to handle decoding and encoding.

=item B<-eos=s>,   B<--encode-output-strings=s>

This flag was added to resolve an issue involving the interface between
Perl::Tidy and calling programs, and in particular B<Code::TidyAll (tidyall)>.

If you only run the B<perltidy> binary this flag has no effect.  If you run a
program which calls the Perl::Tidy module and receives a string in return, then
the meaning of the flag is as follows:

=over 4

=item *

The setting B<-eos> means Perl::Tidy should encode any string which it decodes.
This is the default because it makes perltidy behave well as a filter,
and is the correct setting for most programs.

=item *

The setting B<-neos> means that a string should remain decoded if it was
decoded by Perl::Tidy.  This is only appropriate if the calling program will
handle any needed encoding before outputting the string.

=back

The default was changed from B<-neos> to B<-eos> in versions after 20220217.
If this change causes a program to start running incorrectly on encoded files,
an emergency fix might be to set B<-neos>.  Additional information can be found
in the man pages for the B<Perl::Tidy> module and also in
L<https://github.com/perltidy/perltidy/blob/master/docs/eos_flag.md>.

=item B<-gcs>,  B<--use-unicode-gcstring>

This flag controls whether or not perltidy may use module Unicode::GCString to
obtain accurate display widths of wide characters.  The default
is B<--nouse-unicode-gcstring>.

If this flag is set, and text is encoded, perltidy will look for the module
Unicode::GCString and, if found, will use it to obtain character display
widths.  This can improve displayed vertical alignment for files with wide
characters.  It is a nice feature but it is off by default to avoid conflicting
formatting when there are multiple developers.  Perltidy installation does not
require Unicode::GCString, so users wanting to use this feature need set this
flag and also to install Unicode::GCString separately.

If this flag is set and perltidy does not find module Unicode::GCString,
a warning message will be produced and processing will continue but without
the potential benefit provided by the module.

Also note that actual vertical alignment depends upon the fonts used by the
text display software, so vertical alignment may not be optimal even when
Unicode::GCString is used.

=item B<-ole=s>,  B<--output-line-ending=s>

where s=C<win>, C<dos>, C<unix>, or C<mac>.  This flag tells perltidy
to output line endings for a specific system.  Normally,
perltidy writes files with the line separator character of the host
system.  The C<win> and C<dos> flags have an identical result.

=item B<-ple>,  B<--preserve-line-endings>

This flag tells perltidy to write its output files with the same line
endings as the input file, if possible.  It should work for
B<dos>, B<unix>, and B<mac> line endings.  It will only work if perltidy
input comes from a filename (rather than stdin, for example).  If
perltidy has trouble determining the input file line ending, it will
revert to the default behavior of using the line ending of the host system.

=item B<-atnl>,  B<--add-terminal-newline>

This flag, which is enabled by default, allows perltidy to terminate the last
line of the output stream with a newline character, regardless of whether or
not the input stream was terminated with a newline character.  If this flag is
negated, with B<-natnl>, then perltidy will add a terminal newline to the the
output stream only if the input stream is terminated with a newline.

Negating this flag may be useful for manipulating one-line scripts intended for
use on a command line.

=item B<-it=n>,   B<--iterations=n>

This flag causes perltidy to do B<n> complete iterations.  The reason for this
flag is that code beautification is an iterative process and in some
cases the output from perltidy can be different if it is applied a second time.
For most purposes the default of B<n=1> should be satisfactory.  However B<n=2>
can be useful when a major style change is being made, or when code is being
beautified on check-in to a source code control system.  It has been found to
be extremely rare for the output to change after 2 iterations.  If a value
B<n> is greater than 2 is input then a convergence test will be used to stop
the iterations as soon as possible, almost always after 2 iterations.  See
the next item for a simplified iteration control.

This flag has no effect when perltidy is used to generate html.

=item B<-conv>,   B<--converge>

This flag is equivalent to B<-it=4> and is included to simplify iteration
control.  For all practical purposes one either does or does not want to be
sure that the output is converged, and there is no penalty to using a large
iteration limit since perltidy will check for convergence and stop iterating as
soon as possible.  The default is B<-nconv> (no convergence check).  Using
B<-conv> will approximately double run time since typically one extra iteration
is required to verify convergence.  No extra iterations are required if no new
line breaks are made, and two extra iterations are occasionally needed when
reformatting complex code structures, such as deeply nested ternary statements.

=back

=head2 Code Indentation Control

=over 4

=item B<-ci=n>, B<--continuation-indentation=n>

Continuation indentation is extra indentation spaces applied when
a long line is broken.  The default is n=2, illustrated here:

 my $level =   # -ci=2
   ( $max_index_to_go >= 0 ) ? $levels_to_go[0] : $last_output_level;

The same example, with n=0, is a little harder to read:

 my $level =   # -ci=0
 ( $max_index_to_go >= 0 ) ? $levels_to_go[0] : $last_output_level;

The value given to B<-ci> is also used by some commands when a small
space is required.  Examples are commands for outdenting labels,
B<-ola>, and control keywords, B<-okw>.

When default values are not used, it is recommended that either

(1) the value B<n> given with B<-ci=n> be no more than about one-half of the
number of spaces assigned to a full indentation level on the B<-i=n> command, or

(2) the flag B<-extended-continuation-indentation> is used (see next section).

=item B<-xci>, B<--extended-continuation-indentation>

This flag allows perltidy to use some improvements which have been made to its
indentation model. One of the things it does is "extend" continuation
indentation deeper into structures, hence the name.  The improved indentation
is particularly noticeable when the flags B<-ci=n> and B<-i=n> use the same value of
B<n>. There are no significant disadvantages to using this flag, but to avoid
disturbing existing formatting the default is not to use it, B<-nxci>.

Please see the section L<"B<-pbp>, B<--perl-best-practices>"> for an example of
how this flag can improve the formatting of ternary statements.  It can also
improve indentation of some multi-line qw lists as shown below.

            # perltidy
            foreach $color (
                qw(
                AntiqueWhite3 Bisque1 Bisque2 Bisque3 Bisque4
                SlateBlue3 RoyalBlue1 SteelBlue2 DeepSkyBlue3
                ),
                qw(
                LightBlue1 DarkSlateGray1 Aquamarine2 DarkSeaGreen2
                SeaGreen1 Yellow1 IndianRed1 IndianRed2 Tan1 Tan4
                )
              )

            # perltidy -xci
            foreach $color (
                qw(
                    AntiqueWhite3 Bisque1 Bisque2 Bisque3 Bisque4
                    SlateBlue3 RoyalBlue1 SteelBlue2 DeepSkyBlue3
                ),
                qw(
                    LightBlue1 DarkSlateGray1 Aquamarine2 DarkSeaGreen2
                    SeaGreen1 Yellow1 IndianRed1 IndianRed2 Tan1 Tan4
                )
              )

=item B<-sil=n> B<--starting-indentation-level=n>

By default, perltidy examines the input file and tries to determine the
starting indentation level.  While it is often zero, it may not be
zero for a code snippet being sent from an editing session.

To guess the starting indentation level perltidy simply assumes that
indentation scheme used to create the code snippet is the same as is being used
for the current perltidy process.  This is the only sensible guess that can be
made.  It should be correct if this is true, but otherwise it probably won't.
For example, if the input script was written with -i=2 and the current perltidy
flags have -i=4, the wrong initial indentation will be guessed for a code
snippet which has non-zero initial indentation. Likewise, if an entabbing
scheme is used in the input script and not in the current process then the
guessed indentation will be wrong.

If the default method does not work correctly, or you want to change the
starting level, use B<-sil=n>, to force the starting level to be n.

=item B<List indentation> using B<--line-up-parentheses>, B<-lp> or B<--extended--line-up-parentheses> , B<-xlp>

These flags provide an alternative indentation method for list data.  The
original flag for this is B<-lp>, but it has some limitations (explained below)
which are avoided with the newer B<-xlp> flag.  So B<-xlp> is probably the better
choice for new work, but the B<-lp> flag is retained to minimize changes to
existing formatting.
If you enter both B<-lp> and B<-xlp>, then B<-xlp> will be used.


In the default indentation method perltidy indents lists with 4 spaces, or
whatever value is specified with B<-i=n>.  Here is a small list formatted in
this way:

    # perltidy (default)
    @month_of_year = (
        'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
        'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'
    );

The B<-lp> or B<-xlp> flags add extra indentation to cause the data to begin
past the opening parentheses of a sub call or list, or opening square bracket
of an anonymous array, or opening curly brace of an anonymous hash.  With this
option, the above list would become:

    # perltidy -lp or -xlp
    @month_of_year = (
                       'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
                       'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'
    );

If the available line length (see B<-l=n> ) does not permit this much
space, perltidy will use less.   For alternate placement of the
closing paren, see the next section.

These flags have no effect on code BLOCKS, such as if/then/else blocks,
which always use whatever is specified with B<-i=n>.

Some limitations on these flags are:

=over 4

=item *

A limitation on B<-lp>, but not B<-xlp>, occurs in situations where perltidy
does not have complete freedom to choose line breaks. Then it may temporarily revert
to its default indentation method.  This can occur for example if there are
blank lines, block comments, multi-line quotes, or side comments between the
opening and closing parens, braces, or brackets.  It will also occur if a
multi-line anonymous sub occurs within a container since that will impose
specific line breaks (such as line breaks after statements).

=item *

For both the B<-lp> and B<-xlp> flags, any parameter which significantly
restricts the ability of perltidy to choose newlines will conflict with these
flags and will cause them to be deactivated.  These include B<-io>, B<-fnl>,
B<-nanl>, and B<-ndnl>.

=item *

The B<-lp> and B<-xlp> options may not be used together with the B<-t> tabs option.
They may, however, be used with the B<-et=n> tab method

=back

There are some potential disadvantages of this indentation method compared to
the default method that should be noted:

=over 4

=item *

The available line length can quickly be used up if variable names are
long.  This can cause deeply nested code to quickly reach the line length
limit, and become badly formatted, much sooner than would occur with the
default indentation method.

=item *

Since the indentation depends on the lengths of variable names, small
changes in variable names can cause changes in indentation over many lines in a
file.  This means that minor name changes can produce significant file
differences.  This can be annoying and does not occur with the default
indentation method.

=back

Some things that can be done to minimize these problems are:

=over 4

=item *

Increase B<--maximum-line-length=n> above the default B<n=80> characters if
necessary.

=item *

If you use B<-xlp> then long side comments can limit the indentation over
multiple lines.  Consider adding the flag B<--ignore-side-comment-lengths> to
prevent this, or minimizing the use of side comments.

=item *

Apply this style in a limited way.  By default, it applies to all list
containers (not just lists in parentheses).  The next section describes how to
limit this style to, for example, just function calls.  The default indentation
method will be applied elsewhere.

=back

=item B<-lpil=s>, B<--line-up-parentheses-inclusion-list> and B<-lpxl=s>,  B<--line-up-parentheses-exclusion-list>

The following discussion is written for B<-lp> but applies equally to the newer B<-xlp> version.
By default, the B<-lp> flag applies to as many containers as possible.
The set of containers to which the B<-lp> style applies can be reduced by
either one of these two flags:

Use B<-lpil=s> to specify the containers to which B<-lp> applies, or

use B<-lpxl=s> to specify the containers to which B<-lp> does NOT apply.

Only one of these two flags may be used.  Both flags can achieve the same
result, but the B<-lpil=s> flag is much easier to describe and use and is
recommended.  The B<-lpxl=s> flag was the original implementation and is
only retained for backwards compatibility.

This list B<s> for these parameters is a string with space-separated items.
Each item consists of up to three pieces of information in this order: (1) an
optional letter code (2) a required container type, and (3) an optional numeric
code.

The only required piece of information is a container type, which is one of
'(', '[', or '{'.  For example the string

  -lpil='('

means use -lp formatting only on lists within parentheses, not lists in square-brackets or braces.
The same thing could alternatively be specified with

  -lpxl = '[ {'

which says to exclude lists within square-brackets and braces.  So what remains is lists within parentheses.

A second optional item of information which can be given for parentheses is an alphanumeric
letter which is used to limit the selection further depending on the type of
token immediately before the paren.  The possible letters are currently 'k',
'K', 'f', 'F', 'w', and 'W', with these meanings for matching whatever precedes an opening paren:

 'k' matches if the previous nonblank token is a perl built-in keyword (such as 'if', 'while'),
 'K' matches if 'k' does not, meaning that the previous token is not a keyword.
 'f' matches if the previous token is a function other than a keyword.
 'F' matches if 'f' does not.
 'w' matches if either 'k' or 'f' match.
 'W' matches if 'w' does not.

For example:

  -lpil = 'f('

means only apply -lp to function calls, and

  -lpil = 'w('

means only apply -lp to parenthesized lists which follow a function or a keyword.

This last example could alternatively be written using the B<-lpxl=s> flag as

  -lpxl = '[ { W('

which says exclude B<-lp> for lists within square-brackets, braces, and parens NOT preceded by
a keyword or function.   Clearly, the B<-lpil=s> method is easier to understand.

An optional numeric code may follow any of the container types to further refine the selection based
on container contents.  The numeric codes are:

  '0' or blank: no check on contents is made
  '1' exclude B<-lp> unless the contents is a simple list without sublists
  '2' exclude B<-lp> unless the contents is a simple list without sublists, without
      code blocks, and without ternary operators

For example,

  -lpil = 'f(2'

means only apply -lp to function call lists which do not contain any sublists,
code blocks or ternary expressions.

=item B<-cti=n>, B<--closing-token-indentation>

The B<-cti=n> flag controls the indentation of a line beginning with
a C<)>, C<]>, or a non-block C<}>.  Such a line receives:

 -cti = 0 no extra indentation (default)
 -cti = 1 extra indentation such that the closing token
        aligns with its opening token.
 -cti = 2 one extra indentation level if the line looks like:
        );  or  ];  or  };
 -cti = 3 one extra indentation level always

The flags B<-cti=1> and B<-cti=2> work well with the B<-lp> flag (previous
section).

    # perltidy -lp -cti=1
    @month_of_year = (
                       'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
                       'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'
                     );

    # perltidy -lp -cti=2
    @month_of_year = (
                       'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
                       'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'
                       );

These flags are merely hints to the formatter and they may not always be
followed.  In particular, if -lp is not being used, the indentation for
B<cti=1> is constrained to be no more than one indentation level.

If desired, this control can be applied independently to each of the
closing container token types.  In fact, B<-cti=n> is merely an
abbreviation for B<-cpi=n -csbi=n -cbi=n>, where:
B<-cpi> or B<--closing-paren-indentation> controls B<)>'s,
B<-csbi> or B<--closing-square-bracket-indentation> controls B<]>'s,
B<-cbi> or B<--closing-brace-indentation> controls non-block B<}>'s.

=item B<-icp>, B<--indent-closing-paren>

The B<-icp> flag is equivalent to
B<-cti=2>, described in the previous section.  The B<-nicp> flag is
equivalent B<-cti=0>.  They are included for backwards compatibility.

=item B<-icb>, B<--indent-closing-brace>

The B<-icb> option gives one extra level of indentation to a brace which
terminates a code block .  For example,

        if ($task) {
            yyy();
            }    # -icb
        else {
            zzz();
            }

The default is not to do this, indicated by B<-nicb>.


=item B<-nib>, B<--non-indenting-braces>

Normally, lines of code contained within a pair of block braces receive one
additional level of indentation.  This flag, which is enabled by default,
causes perltidy to look for
opening block braces which are followed by a special side comment. This special
side comment is B<#<<<> by default.  If found, the code between this opening brace and its
corresponding closing brace will not be given the normal extra indentation
level.  For example:

            { #<<<   a closure to contain lexical vars

            my $var;  # this line does not get one level of indentation
            ...

            }

            # this line does not 'see' $var;

This can be useful, for example, when combining code from different files.
Different sections of code can be placed within braces to keep their lexical
variables from being visible to the end of the file.  To keep the new braces
from causing all of their contained code to be indented if you run perltidy,
and possibly introducing new line breaks in long lines, you can mark the
opening braces with this special side comment.

Only the opening brace needs to be marked, since perltidy knows where the
closing brace is.  Braces contained within marked braces may also be marked
as non-indenting.

If your code happens to have some opening braces followed by '#<<<', and you
don't want this behavior, you can use B<-nnib> to deactivate it.  To make it
easy to remember, the default string is the same as the string for starting a
B<format-skipping> section. There is no confusion because in that case it is
for a block comment rather than a side-comment.

The special side comment can be changed with the next parameter.


=item B<-nibp=s>, B<--non-indenting-brace-prefix=s>

The B<-nibp=string> parameter may be used to change the marker for
non-indenting braces.  The default is equivalent to -nibp='#<<<'.  The string
that you enter must begin with a # and should be in quotes as necessary to get
past the command shell of your system.  This string is the leading text of a
regex pattern that is constructed by appending pre-pending a '^' and appending
a'\s', so you must also include backslashes for characters to be taken
literally rather than as patterns.

For example, to match the side comment '#++', the parameter would be

  -nibp='#\+\+'


=item B<-olq>, B<--outdent-long-quotes>

When B<-olq> is set, lines which is a quoted string longer than the
value B<maximum-line-length> will have their indentation removed to make
them more readable.  This is the default.  To prevent such out-denting,
use B<-nolq> or B<--nooutdent-long-lines>.

=item B<-oll>, B<--outdent-long-lines>

This command is equivalent to B<--outdent-long-quotes> and
B<--outdent-long-comments>, and it is included for compatibility with previous
versions of perltidy.  The negation of this also works, B<-noll> or
B<--nooutdent-long-lines>, and is equivalent to setting B<-nolq> and B<-nolc>.

=item B<Outdenting Labels:> B<-ola>,  B<--outdent-labels>

This command will cause labels to be outdented by 2 spaces (or whatever B<-ci>
has been set to), if possible.  This is the default.  For example:

        my $i;
      LOOP: while ( $i = <FOTOS> ) {
            chomp($i);
            next unless $i;
            fixit($i);
        }

Use B<-nola> to not outdent labels.  To control line breaks after labels see L<"-bal=n, --break-after-labels=n">.

=item B<Outdenting Keywords>

=over 4

=item B<-okw>,  B<--outdent-keywords>

The command B<-okw> will cause certain leading control keywords to
be outdented by 2 spaces (or whatever B<-ci> has been set to), if
possible.  By default, these keywords are C<redo>, C<next>, C<last>,
C<goto>, and C<return>.  The intention is to make these control keywords
easier to see.  To change this list of keywords being outdented, see
the next section.

For example, using C<perltidy -okw> on the previous example gives:

        my $i;
      LOOP: while ( $i = <FOTOS> ) {
            chomp($i);
          next unless $i;
            fixit($i);
        }

The default is not to do this.

=item B<Specifying Outdented Keywords:> B<-okwl=string>,  B<--outdent-keyword-list=string>

This command can be used to change the keywords which are outdented with
the B<-okw> command.  The parameter B<string> is a required list of perl
keywords, which should be placed in quotes if there are more than one.
By itself, it does not cause any outdenting to occur, so the B<-okw>
command is still required.

For example, the commands C<-okwl="next last redo goto" -okw> will cause
those four keywords to be outdented.  It is probably simplest to place
any B<-okwl> command in a F<.perltidyrc> file.

=back

=back

=head2 Whitespace Control

Whitespace refers to the blank space between variables, operators,
and other code tokens.

=over 4

=item B<-fws>,  B<--freeze-whitespace>

This flag causes your original whitespace to remain unchanged, and
causes the rest of the whitespace commands in this section, the
Code Indentation section, and
the Comment Control section to be ignored.

=item B<Tightness of curly braces, parentheses, and square brackets>

Here the term "tightness" will mean the closeness with which
pairs of enclosing tokens, such as parentheses, contain the quantities
within.  A numerical value of 0, 1, or 2 defines the tightness, with
0 being least tight and 2 being most tight.  Spaces within containers
are always symmetric, so if there is a space after a C<(> then there
will be a space before the corresponding C<)>.

The B<-pt=n> or B<--paren-tightness=n> parameter controls the space within
parens.  The example below shows the effect of the three possible
values, 0, 1, and 2:

 if ( ( my $len_tab = length( $tabstr ) ) > 0 ) {  # -pt=0
 if ( ( my $len_tab = length($tabstr) ) > 0 ) {    # -pt=1 (default)
 if ((my $len_tab = length($tabstr)) > 0) {        # -pt=2

When n is 0, there is always a space to the right of a '(' and to the left
of a ')'.  For n=2 there is never a space.  For n=1, the default, there
is a space unless the quantity within the parens is a single token, such
as an identifier or quoted string.

Likewise, the parameter B<-sbt=n> or B<--square-bracket-tightness=n>
controls the space within square brackets, as illustrated below.

 $width = $col[ $j + $k ] - $col[ $j ];  # -sbt=0
 $width = $col[ $j + $k ] - $col[$j];    # -sbt=1 (default)
 $width = $col[$j + $k] - $col[$j];      # -sbt=2

Curly braces which do not contain code blocks are controlled by
the parameter B<-bt=n> or B<--brace-tightness=n>.

 $obj->{ $parsed_sql->{ 'table' }[0] };    # -bt=0
 $obj->{ $parsed_sql->{'table'}[0] };      # -bt=1 (default)
 $obj->{$parsed_sql->{'table'}[0]};        # -bt=2

And finally, curly braces which contain blocks of code are controlled by the
parameter B<-bbt=n> or B<--block-brace-tightness=n> as illustrated in the
example below.

 %bf = map { $_ => -M $_ } grep { /\.deb$/ } dirents '.'; # -bbt=0 (default)
 %bf = map { $_ => -M $_ } grep {/\.deb$/} dirents '.';   # -bbt=1
 %bf = map {$_ => -M $_} grep {/\.deb$/} dirents '.';     # -bbt=2

To simplify input in the case that all of the tightness flags have the same
value <n>, the parameter <-act=n> or B<--all-containers-tightness=n> is an
abbreviation for the combination <-pt=n -sbt=n -bt=n -bbt=n>.

=item B<-xbt>,   B<--extended-block-tightness>

The previous section described two controls for spacing within curly braces,
namely B<-block-brace-tightness=n> for code block braces and
B<-brace-tightness=n> for all other braces.

There is a little fuzziness in this division of brace types though because the
curly braces considered by perltidy to contain code blocks for formatting
purposes, such as highlighting code structure, exclude some of the small code
blocks used by Perl mainly for isolating terms.  These include curly braces
following a keyword where an indirect object might occur, or curly braces
following a type symbol.  For example, perltidy does not mark the following
braces as code block braces:

    print {*STDERR} $message;
    return ${$foo};

Consequently, the spacing within these small braced containers by default
follows the flag B<--brace-tightness=n> rather than
B<--block-brace-tightness=n>, as one might expect.

If desired, small blocks such as these can be made to instead follow the
spacing defined by the B<--block-brace-tightness=n> flag by setting
B<--extended-block-tightness>.  The specific types of small blocks to which
this parameter applies is controlled by a companion control parameter,
described in the next section.

Note that if the two flags B<-bbt=n> and B<-bt=n> have the same value
B<n> then there would be no reason to set this flag.

=item B<-xbtl=s>,   B<--extended-block-tightness-list=s>

The previous parameter B<-xbt> can be made to apply to curly braces preceded
by any of the keywords

    print printf exec system say

and/or the special symbols

    $ @ % & * $#

The parameter string B<s> may contain a selection of these keywords and symbols
to indicate the brace types to which B<-xbt> applies.  For convenience, all of
the keywords can be selected with 'k', and all of the special symbols
can be selected with 't'.  The default is equivalent to B<-xbtl='k'>, which
selects all of the keywords.

Examples:

   -xbtl='k'          # selects just the keywords [DEFAULT]
   -xbtl="t"          # selects just the special type symbols
   -xbtl="k t"        # selects all keywords and symbols, or more simply
   -xbtl="kt"         # selects all keywords and symbols
   -xbtl="print say"  # selects just keywords B<print> and B<say>:

Here are some formatting examples using the default values of B<-bt=n> and
B<-bbt=n>. Note that in these examples B<$ref> is in block braces but B<$key>
is not.

    # default formatting
    print {*STDERR} $message;
    my $val = ${$ref}{$key};

    # perltidy -xbt           or
    # perltidy -xbt -xbtl=k
    print { *STDERR } $message;
    my $val = ${$ref}{$key};

    # perltidy -xbt -xbtl=t
    print {*STDERR} $message;
    my $val = ${ $ref }{$key};

    # perltidy -xbt -xbtl=kt
    print { *STDERR } $message;
    my $val = ${ $ref }{$key};

Finally, note that this parameter merely changes the way that the parameter
B<--extended-block-tightness> works. It has no effect unless
B<--extended-block-tightness> is actually set.

=item B<-tso>,   B<--tight-secret-operators>

The flag B<-tso> causes certain perl token sequences (secret operators)
which might be considered to be a single operator to be formatted "tightly"
(without spaces).  The operators currently modified by this flag are:

     0+  +0  ()x!! ~~<>  ,=>   =( )=

For example the sequence B<0 +>,  which converts a string to a number,
would be formatted without a space: B<0+> when the B<-tso> flag is set.  This
flag is off by default.

=item B<-sts>,   B<--space-terminal-semicolon>

Some programmers prefer a space before all terminal semicolons.  The
default is for no such space, and is indicated with B<-nsts> or
B<--nospace-terminal-semicolon>.

	$i = 1 ;     #  -sts
	$i = 1;      #  -nsts   (default)

=item B<-sfs>,   B<--space-for-semicolon>

Semicolons within B<for> loops may sometimes be hard to see,
particularly when commas are also present.  This option places spaces on
both sides of these special semicolons, and is the default.  Use
B<-nsfs> or B<--nospace-for-semicolon> to deactivate it.

 for ( @a = @$ap, $u = shift @a ; @a ; $u = $v ) {  # -sfs (default)
 for ( @a = @$ap, $u = shift @a; @a; $u = $v ) {    # -nsfs

=item B<-asc>,  B<--add-semicolons>

Setting B<-asc> allows perltidy to add any missing optional semicolon at the end
of a line which is followed by a closing curly brace on the next line.  This
is the default, and may be deactivated with B<-nasc> or B<--noadd-semicolons>.

=item B<-dsm>,  B<--delete-semicolons>

Setting B<-dsm> allows perltidy to delete extra semicolons which are
simply empty statements.  This is the default, and may be deactivated
with B<-ndsm> or B<--nodelete-semicolons>.  (Such semicolons are not
deleted, however, if they would promote a side comment to a block
comment).

=item B<-aws>,  B<--add-whitespace>

Setting this option allows perltidy to add certain whitespace to improve
code readability.  This is the default. If you do not want any
whitespace added, but are willing to have some whitespace deleted, use
B<-naws>.  (Use B<-fws> to leave whitespace completely unchanged).

=item B<-dws>,  B<--delete-old-whitespace>

Setting this option allows perltidy to remove some old whitespace
between characters, if necessary.  This is the default.  If you
do not want any old whitespace removed, use B<-ndws> or
B<--nodelete-old-whitespace>.

=item B<Detailed whitespace controls around tokens>

For those who want more detailed control over the whitespace around
tokens, there are four parameters which can directly modify the default
whitespace rules built into perltidy for any token.  They are:

B<-wls=s> or B<--want-left-space=s>,

B<-nwls=s> or B<--nowant-left-space=s>,

B<-wrs=s> or B<--want-right-space=s>,

B<-nwrs=s> or B<--nowant-right-space=s>.

These parameters are each followed by a quoted string, B<s>, containing a
list of token types.  No more than one of each of these parameters
should be specified, because repeating a command-line parameter
always overwrites the previous one before perltidy ever sees it.

To illustrate how these are used, suppose it is desired that there be no
space on either side of the token types B<= + - / *>.  The following two
parameters would specify this desire:

  -nwls="= + - / *"    -nwrs="= + - / *"

(Note that the token types are in quotes, and that they are separated by
spaces).  With these modified whitespace rules, the following line of math:

  $root = -$b + sqrt( $b * $b - 4. * $a * $c ) / ( 2. * $a );

becomes this:

  $root=-$b+sqrt( $b*$b-4.*$a*$c )/( 2.*$a );

These parameters should be considered to be hints to perltidy rather
than fixed rules, because perltidy must try to resolve conflicts that
arise between them and all of the other rules that it uses.  One
conflict that can arise is if, between two tokens, the left token wants
a space and the right one doesn't.  In this case, the token not wanting
a space takes priority.

It is necessary to have a list of all token types in order to create
this type of input.  Such a list can be obtained by the command
B<--dump-token-types>.  Also try the B<-D> flag on a short snippet of code
and look at the .DEBUG file to see the tokenization.

B<WARNING> Be sure to put these tokens in quotes to avoid having them
misinterpreted by your command shell.

=item B<Note1: Perltidy does always follow whitespace controls>

The various parameters controlling whitespace within a program are requests which perltidy follows as well as possible, but there are a number of situations where changing whitespace could change program behavior and is not done.  Some of these are obvious; for example, we should not remove the space between the two plus symbols in '$x+ +$y' to avoid creating a '++' operator. Some are more subtle and involve the whitespace around bareword symbols and locations of possible filehandles.  For example, consider the problem of formatting the following subroutine:

   sub print_div {
      my ($x,$y)=@_;
      print $x/$y;
   }

Suppose the user requests that / signs have a space to the left but not to the right. Perltidy will refuse to do this, but if this were done the result would be

   sub print_div {
       my ($x,$y)=@_;
       print $x /$y;
   }

If formatted in this way, the program will not run (at least with recent versions of perl) because the $x is taken to be a filehandle and / is assumed to start a quote. In a complex program, there might happen to be a / which terminates the multiline quote without a syntax error, allowing the program to run, but not as intended.

Related issues arise with other binary operator symbols, such as + and -, and in older versions of perl there could be problems with ternary operators.  So to avoid changing program behavior, perltidy has the simple rule that whitespace around possible filehandles is left unchanged.  Likewise, whitespace around barewords is left unchanged.  The reason is that if the barewords are defined in other modules, or in code that has not even been written yet, perltidy will not have seen their prototypes and must treat them cautiously.

In perltidy this is implemented in the tokenizer by marking token following a
B<print> keyword as a special type B<Z>.  When formatting is being done,
whitespace following this token type is generally left unchanged as a precaution
against changing program behavior.  This is excessively conservative but simple
and easy to implement.  Keywords which are treated similarly to B<print> include
B<printf>, B<sort>, B<exec>, B<system>.  Changes in spacing around parameters
following these keywords may have to be made manually.  For example, the space,
or lack of space, after the parameter $foo in the following line will be
unchanged in formatting.

   system($foo );
   system($foo);

To find if a token is of type B<Z> you can use B<perltidy -DEBUG>. For the
first line above the result is

   1: system($foo );
   1: kkkkkk{ZZZZb};

which shows that B<system> is type B<k> (keyword) and $foo is type B<Z>.

=item B<Note2: Perltidy's whitespace rules are not perfect>

Despite these precautions, it is still possible to introduce syntax errors with
some asymmetric whitespace rules, particularly when call parameters are not
placed in containing parens or braces.  For example, the following two lines will
be parsed by perl without a syntax error:

  # original programming, syntax ok
  my @newkeys = map $_-$nrecs+@data, @oldkeys;

  # perltidy default, syntax ok
  my @newkeys = map $_ - $nrecs + @data, @oldkeys;

But the following will give a syntax error:

  # perltidy -nwrs='-'
  my @newkeys = map $_ -$nrecs + @data, @oldkeys;

For another example, the following two lines will be parsed without syntax error:

  # original programming, syntax ok
  for my $severity ( reverse $SEVERITY_LOWEST+1 .. $SEVERITY_HIGHEST ) { ...  }

  # perltidy default, syntax ok
  for my $severity ( reverse $SEVERITY_LOWEST + 1 .. $SEVERITY_HIGHEST ) { ... }

But the following will give a syntax error:

  # perltidy -nwrs='+', syntax error:
  for my $severity ( reverse $SEVERITY_LOWEST +1 .. $SEVERITY_HIGHEST ) { ... }

To avoid subtle parsing problems like this, it is best to avoid spacing a
binary operator asymmetrically with a space on the left but not on the right.

=item B<Space between specific keywords and opening paren>

When an opening paren follows a Perl keyword, no space is introduced after the
keyword, unless it is (by default) one of these:

   my local our and or xor eq ne if else elsif until unless
   while for foreach return switch case given when

These defaults can be modified with two commands:

B<-sak=s>  or B<--space-after-keyword=s>  adds keywords.

B<-nsak=s>  or B<--nospace-after-keyword=s>  removes keywords.

where B<s> is a list of keywords (in quotes if necessary).  For example,

  my ( $a, $b, $c ) = @_;    # default
  my( $a, $b, $c ) = @_;     # -nsak="my local our"

The abbreviation B<-nsak='*'> is equivalent to including all of the
keywords in the above list.

When both B<-nsak=s> and B<-sak=s> commands are included, the B<-nsak=s>
command is executed first.  For example, to have space after only the
keywords (my, local, our) you could use B<-nsak="*" -sak="my local our">.

To put a space after all keywords, see the next item.

=item B<Space between all keywords and opening parens>

When an opening paren follows a function or keyword, no space is introduced
after the keyword except for the keywords noted in the previous item.  To
always put a space between a function or keyword and its opening paren,
use the command:

B<-skp>  or B<--space-keyword-paren>

You may also want to use the flag B<-sfp> (next item) too.

=item B<Space between all function names and opening parens>

When an opening paren follows a function the default and recommended formatting
is not to introduce a space.  To cause a space to be introduced use:

B<-sfp>  or B<--space-function-paren>

  myfunc( $a, $b, $c );    # default
  myfunc ( $a, $b, $c );   # -sfp

You will probably also want to use the flag B<-skp> (previous item) too.

The parameter is not recommended because spacing a function paren can make a
program vulnerable to parsing problems by Perl.  For example, the following
two-line program will run as written but will have a syntax error if
reformatted with -sfp:

  if ( -e filename() ) { print "I'm here\n"; }
  sub filename { return $0 }

In this particular case the syntax error can be removed if the line order is
reversed, so that Perl parses 'sub filename' first.

=item B<-fpva>  or B<--function-paren-vertical-alignment>

A side-effect of using the B<-sfp> flag is that the parens may become vertically
aligned. For example,

    # perltidy -sfp
    myfun     ( $aaa, $b, $cc );
    mylongfun ( $a, $b, $c );

This is the default behavior.  To prevent this alignment use B<-nfpva>:

    # perltidy -sfp -nfpva
    myfun ( $aaa, $b, $cc );
    mylongfun ( $a, $b, $c );

=item B<-spp=n>  or B<--space-prototype-paren=n>

This flag can be used to control whether a function prototype is preceded by a space.  For example, the following prototype does not have a space.

      sub usage();

This integer B<n> may have the value 0, 1, or 2 as follows:

    -spp=0 means no space before the paren
    -spp=1 means follow the example of the source code [DEFAULT]
    -spp=2 means always put a space before the paren

The default is B<-spp=1>, meaning that a space will be used if and only if there is one in the source code.  Given the above line of code, the result of
applying the different options would be:

        sub usage();    # n=0 [no space]
        sub usage();    # n=1 [default; follows input]
        sub usage ();   # n=2 [space]

=item B<-kpit=n> or B<--keyword-paren-inner-tightness=n>

The space inside of an opening paren, which itself follows a certain keyword,
can be controlled by this parameter.  The space on the inside of the
corresponding closing paren will be treated in the same (balanced) manner.
This parameter has precedence over any other paren spacing rules.  The values
of B<n> are as follows:

   -kpit=0 means always put a space (not tight)
   -kpit=1 means ignore this parameter [default]
   -kpit=2 means never put a space (tight)

To illustrate, the following snippet is shown formatted in three ways:

    if ( seek( DATA, 0, 0 ) ) { ... }    # perltidy (default)
    if (seek(DATA, 0, 0)) { ... }        # perltidy -pt=2
    if ( seek(DATA, 0, 0) ) { ... }      # perltidy -pt=2 -kpit=0

In the second case the -pt=2 parameter makes all of the parens tight. In the
third case the -kpit=0 flag causes the space within the 'if' parens to have a
space, since 'if' is one of the keywords to which the -kpit flag applies by
default.  The remaining parens are still tight because of the -pt=2 parameter.

The set of keywords to which this parameter applies are by default are:

   if elsif unless while until for foreach

These can be changed with the parameter B<-kpitl=s> described in the next section.


=item B<-kpitl=string> or B<--keyword-paren-inner-tightness=string>

This command can be used to change the keywords to which the the B<-kpit=n>
command applies.  The parameter B<string> is a required list either keywords or
functions, which should be placed in quotes if there are more than one.  By
itself, this parameter does not cause any change in spacing, so the B<-kpit=n>
command is still required.

For example, the commands C<-kpitl="if else while" -kpit=2> will cause the just
the spaces inside parens following  'if', 'else', and 'while' keywords to
follow the tightness value indicated by the B<-kpit=2> flag.

=item B<-lop>  or B<--logical-padding>

In the following example some extra space has been inserted on the second
line between the two open parens. This extra space is called "logical padding"
and is intended to help align similar things vertically in some logical
or ternary expressions.

    # perltidy [default formatting]
    $same =
      (      ( $aP eq $bP )
          && ( $aS eq $bS )
          && ( $aT eq $bT )
          && ( $a->{'title'} eq $b->{'title'} )
          && ( $a->{'href'} eq $b->{'href'} ) );

Note that this is considered to be a different operation from "vertical
alignment" because space at just one line is being adjusted, whereas in
"vertical alignment" the spaces at all lines are being adjusted. So it sort of
a local version of vertical alignment.

Here is an example involving a ternary operator:

    # perltidy [default formatting]
    $bits =
        $top > 0xffff ? 32
      : $top > 0xff   ? 16
      : $top > 1      ? 8
      :                 1;

This behavior is controlled with the flag B<--logical-padding>, which is set
'on' by default.  If it is not desired it can be turned off using
B<--nological-padding> or B<-nlop>.  The above two examples become, with
B<-nlop>:

    # perltidy -nlop
    $same =
      ( ( $aP eq $bP )
          && ( $aS eq $bS )
          && ( $aT eq $bT )
          && ( $a->{'title'} eq $b->{'title'} )
          && ( $a->{'href'} eq $b->{'href'} ) );

    # perltidy -nlop
    $bits =
      $top > 0xffff ? 32
      : $top > 0xff ? 16
      : $top > 1    ? 8
      :               1;


=item B<Trimming whitespace around C<qw> quotes>

B<-tqw> or B<--trim-qw> provide the default behavior of trimming
spaces around multi-line C<qw> quotes and indenting them appropriately.

B<-ntqw> or B<--notrim-qw> cause leading and trailing whitespace around
multi-line C<qw> quotes to be left unchanged.  This option will not
normally be necessary, but was added for testing purposes, because in
some versions of perl, trimming C<qw> quotes changes the syntax tree.

=item B<-sbq=n>  or B<--space-backslash-quote=n>

lines like

       $str1=\"string1";
       $str2=\'string2';

can confuse syntax highlighters unless a space is included between the backslash and the single or double quotation mark.

this can be controlled with the value of B<n> as follows:

    -sbq=0 means no space between the backslash and quote
    -sbq=1 means follow the example of the source code
    -sbq=2 means always put a space between the backslash and quote

The default is B<-sbq=1>, meaning that a space will be used if there is one in the source code.

=item B<Trimming trailing whitespace from lines of POD>

B<-trp> or B<--trim-pod> will remove trailing whitespace from lines of POD.
The default is not to do this.

=back

=head2 Comment Controls

Perltidy has a number of ways to control the appearance of both block comments
and side comments.  The term B<block comment> here refers to a full-line
comment, whereas B<side comment> will refer to a comment which appears on a
line to the right of some code.

=over 4

=item B<-ibc>,  B<--indent-block-comments>

Block comments normally look best when they are indented to the same
level as the code which follows them.  This is the default behavior, but
you may use B<-nibc> to keep block comments left-justified.  Here is an
example:

             # this comment is indented      (-ibc, default)
	     if ($task) { yyy(); }

The alternative is B<-nibc>:

 # this comment is not indented              (-nibc)
	     if ($task) { yyy(); }

See also the next item, B<-isbc>, as well as B<-sbc>, for other ways to
have some indented and some outdented block comments.

=item B<-isbc>,  B<--indent-spaced-block-comments>

If there is no leading space on the line, then the comment will not be
indented, and otherwise it may be.

If both B<-ibc> and B<-isbc> are set, then B<-isbc> takes priority.

=item B<-olc>, B<--outdent-long-comments>

When B<-olc> is set, lines which are full-line (block) comments longer
than the value B<maximum-line-length> will have their indentation
removed.  This is the default; use B<-nolc> to prevent outdenting.

=item B<-msc=n>,  B<--minimum-space-to-comment=n>

Side comments look best when lined up several spaces to the right of
code.  Perltidy will try to keep comments at least n spaces to the
right.  The default is n=4 spaces.

=item B<-fpsc=n>,  B<--fixed-position-side-comment=n>

This parameter tells perltidy to line up side comments in column number B<n>
whenever possible.  The default, n=0, will not do this.

=item B<-iscl>,  B<--ignore-side-comment-lengths>

This parameter causes perltidy to ignore the length of side comments when
setting line breaks.  The default, B<-niscl>, is to include the length of
side comments when breaking lines to stay within the length prescribed
by the B<-l=n> maximum line length parameter.  For example, the following
long single line would remain intact with -l=80 and -iscl:

     perltidy -l=80 -iscl
        $vmsfile =~ s/;[\d\-]*$//; # Clip off version number; we can use a newer version as well

whereas without the -iscl flag the line will be broken:

     perltidy -l=80
        $vmsfile =~ s/;[\d\-]*$//
          ;    # Clip off version number; we can use a newer version as well

=item B<-ipc>,  B<--ignore-perlcritic-comments>

Perltidy, by default, will look for side comments beginning with
B<## no critic> and ignore their lengths when making line break decisions,
even if the user has not set B<-iscl>.  The reason is that an unwanted line
break can make these special comments ineffective in controlling B<perlcritic>.

Setting B<--ignore-perlcritic-comments> tells perltidy not to look for these 
B<## no critic> comments.

=item B<-hsc>, B<--hanging-side-comments>

By default, perltidy tries to identify and align "hanging side
comments", which are something like this:

        my $IGNORE = 0;    # This is a side comment
                           # This is a hanging side comment
                           # And so is this

A comment is considered to be a hanging side comment if (1) it immediately
follows a line with a side comment, or another hanging side comment, and
(2) there is some leading whitespace on the line.
To deactivate this feature, use B<-nhsc> or B<--nohanging-side-comments>.
If block comments are preceded by a blank line, or have no leading
whitespace, they will not be mistaken as hanging side comments.

=item B<Closing Side Comments>

A closing side comment is a special comment which perltidy can
automatically create and place after the closing brace of a code block.
They can be useful for code maintenance and debugging.  The command
B<-csc> (or B<--closing-side-comments>) adds or updates closing side
comments.  For example, here is a small code snippet

        sub message {
            if ( !defined( $_[0] ) ) {
                print("Hello, World\n");
            }
            else {
                print( $_[0], "\n" );
            }
        }

And here is the result of processing with C<perltidy -csc>:

        sub message {
            if ( !defined( $_[0] ) ) {
                print("Hello, World\n");
            }
            else {
                print( $_[0], "\n" );
            }
        } ## end sub message

A closing side comment was added for C<sub message> in this case, but not
for the C<if> and C<else> blocks, because they were below the 6 line
cutoff limit for adding closing side comments.  This limit may be
changed with the B<-csci> command, described below.

The command B<-dcsc> (or B<--delete-closing-side-comments>) reverses this
process and removes these comments.

Several commands are available to modify the behavior of these two basic
commands, B<-csc> and B<-dcsc>:

=over 4

=item B<-csci=n>, or B<--closing-side-comment-interval=n>

where C<n> is the minimum number of lines that a block must have in
order for a closing side comment to be added.  The default value is
C<n=6>.  To illustrate:

        # perltidy -csci=2 -csc
        sub message {
            if ( !defined( $_[0] ) ) {
                print("Hello, World\n");
            } ## end if ( !defined( $_[0] ))
            else {
                print( $_[0], "\n" );
            } ## end else [ if ( !defined( $_[0] ))
        } ## end sub message

Now the C<if> and C<else> blocks are commented.  However, now this has
become very cluttered.

=item B<-cscp=string>, or B<--closing-side-comment-prefix=string>

where string is the prefix used before the name of the block type.  The
default prefix, shown above, is C<## end>.  This string will be added to
closing side comments, and it will also be used to recognize them in
order to update, delete, and format them.  Any comment identified as a
closing side comment will be placed just a single space to the right of
its closing brace.

=item B<-cscl=string>, or B<--closing-side-comment-list>

where C<string> is a list of block types to be tagged with closing side
comments.  By default, all code block types preceded by a keyword or
label (such as C<if>, C<sub>, and so on) will be tagged.  The B<-cscl>
command changes the default list to be any selected block types; see
L<"Specifying Block Types">.
For example, the following command
requests that only C<sub>'s, labels, C<BEGIN>, and C<END> blocks be
affected by any B<-csc> or B<-dcsc> operation:

   -cscl="sub : BEGIN END"

=item B<-csct=n>, or B<--closing-side-comment-maximum-text=n>

The text appended to certain block types, such as an C<if> block, is
whatever lies between the keyword introducing the block, such as C<if>,
and the opening brace.  Since this might be too much text for a side
comment, there needs to be a limit, and that is the purpose of this
parameter.  The default value is C<n=20>, meaning that no additional
tokens will be appended to this text after its length reaches 20
characters.  Omitted text is indicated with C<...>.  (Tokens, including
sub names, are never truncated, however, so actual lengths may exceed
this).  To illustrate, in the above example, the appended text of the
first block is C< ( !defined( $_[0] )...>.  The existing limit of
C<n=20> caused this text to be truncated, as indicated by the C<...>.  See
the next flag for additional control of the abbreviated text.

=item B<-cscb>, or B<--closing-side-comments-balanced>

As discussed in the previous item, when the
closing-side-comment-maximum-text limit is exceeded the comment text must
be truncated.  Older versions of perltidy terminated with three dots, and this
can still be achieved with -ncscb:

  perltidy -csc -ncscb
  } ## end foreach my $foo (sort { $b cmp $a ...

However this causes a problem with editors which cannot recognize
comments or are not configured to do so because they cannot "bounce" around in
the text correctly.  The B<-cscb> flag has been added to
help them by appending appropriate balancing structure:

  perltidy -csc -cscb
  } ## end foreach my $foo (sort { $b cmp $a ... })

The default is B<-cscb>.

=item B<-csce=n>, or B<--closing-side-comment-else-flag=n>

The default, B<n=0>, places the text of the opening C<if> statement after any
terminal C<else>.

If B<n=2> is used, then each C<elsif> is also given the text of the opening
C<if> statement.  Also, an C<else> will include the text of a preceding
C<elsif> statement.  Note that this may result some long closing
side comments.

If B<n=1> is used, the results will be the same as B<n=2> whenever the
resulting line length is less than the maximum allowed.

=item B<-cscb>, or B<--closing-side-comments-balanced>

When using closing-side-comments, and the closing-side-comment-maximum-text
limit is exceeded, then the comment text must be abbreviated.
It is terminated with three dots if the B<-cscb> flag is negated:

  perltidy -csc -ncscb
  } ## end foreach my $foo (sort { $b cmp $a ...

This causes a problem with older editors which do not recognize comments
because they cannot "bounce" around in the text correctly.  The B<-cscb>
flag tries to help them by appending appropriate terminal balancing structures:

  perltidy -csc -cscb
  } ## end foreach my $foo (sort { $b cmp $a ... })

The default is B<-cscb>.


=item B<-cscw>, or B<--closing-side-comment-warnings>

This parameter is intended to help make the initial transition to the use of
closing side comments.
It causes two
things to happen if a closing side comment replaces an existing, different
closing side comment:  first, an error message will be issued, and second, the
original side comment will be placed alone on a new specially marked comment
line for later attention.

The intent is to avoid clobbering existing hand-written side comments
which happen to match the pattern of closing side comments. This flag
should only be needed on the first run with B<-csc>.

=back

B<Important Notes on Closing Side Comments:>

=over 4

=item *

Closing side comments are only placed on lines terminated with a closing
brace.  Certain closing styles, such as the use of cuddled elses
(B<-ce>), preclude the generation of some closing side comments.

=item *

Please note that adding or deleting of closing side comments takes
place only through the commands B<-csc> or B<-dcsc>.  The other commands,
if used, merely modify the behavior of these two commands.

=item *

It is recommended that the B<-cscw> flag be used along with B<-csc> on
the first use of perltidy on a given file.  This will prevent loss of
any existing side comment data which happens to have the csc prefix.

=item *

Once you use B<-csc>, you should continue to use it so that any
closing side comments remain correct as code changes.  Otherwise, these
comments will become incorrect as the code is updated.

=item *

If you edit the closing side comments generated by perltidy, you must also
change the prefix to be different from the closing side comment prefix.
Otherwise, your edits will be lost when you rerun perltidy with B<-csc>.   For
example, you could simply change C<## end> to be C<## End>, since the test is
case sensitive.  You may also want to use the B<-ssc> flag to keep these
modified closing side comments spaced the same as actual closing side comments.

=item *

Temporarily generating closing side comments is a useful technique for
exploring and/or debugging a perl script, especially one written by someone
else.  You can always remove them with B<-dcsc>.

=back

=item B<Static Block Comments>

Static block comments are block comments with a special leading pattern,
C<##> by default, which will be treated slightly differently from other
block comments.  They effectively behave as if they had glue along their
left and top edges, because they stick to the left edge and previous line
when there is no blank spaces in those places.  This option is
particularly useful for controlling how commented code is displayed.

=over 4

=item B<-sbc>, B<--static-block-comments>

When B<-sbc> is used, a block comment with a special leading pattern, C<##> by
default, will be treated specially.

Comments so identified  are treated as follows:

=over 4

=item *

If there is no leading space on the line, then the comment will not
be indented, and otherwise it may be,

=item *

no new blank line will be
inserted before such a comment, and

=item *

such a comment will never become
a hanging side comment.

=back

For example, assuming C<@month_of_year> is
left-adjusted:

    @month_of_year = (    # -sbc (default)
        'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct',
    ##  'Dec', 'Nov'
        'Nov', 'Dec');

Without this convention, the above code would become

    @month_of_year = (   # -nsbc
        'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct',

        ##  'Dec', 'Nov'
        'Nov', 'Dec'
    );

which is not as clear.
The default is to use B<-sbc>.  This may be deactivated with B<-nsbc>.

=item B<-sbcp=string>, B<--static-block-comment-prefix=string>

This parameter defines the prefix used to identify static block comments
when the B<-sbc> parameter is set.  The default prefix is C<##>,
corresponding to C<-sbcp=##>.  The prefix is actually part of a perl
pattern used to match lines and it must either begin with C<#> or C<^#>.
In the first case a prefix ^\s* will be added to match any leading
whitespace, while in the second case the pattern will match only
comments with no leading whitespace.  For example, to
identify all comments as static block comments, one would use C<-sbcp=#>.
To identify all left-adjusted comments as static block comments, use C<-sbcp='^#'>.

Please note that B<-sbcp> merely defines the pattern used to identify static
block comments; it will not be used unless the switch B<-sbc> is set.  Also,
please be aware that since this string is used in a perl regular expression
which identifies these comments, it must enable a valid regular expression to
be formed.

A pattern which can be useful is:

    -sbcp=^#{2,}[^\s#]

This pattern requires a static block comment to have at least one character
which is neither a # nor a space.  It allows a line containing only '#'
characters to be rejected as a static block comment.  Such lines are often used
at the start and end of header information in subroutines and should not be
separated from the intervening comments, which typically begin with just a
single '#'.

=item B<-osbc>, B<--outdent-static-block-comments>

The command B<-osbc> will cause static block comments to be outdented by 2
spaces (or whatever B<-ci=n> has been set to), if possible.

=back

=item B<Static Side Comments>

Static side comments are side comments with a special leading pattern.
This option can be useful for controlling how commented code is displayed
when it is a side comment.

=over 4

=item B<-ssc>, B<--static-side-comments>

When B<-ssc> is used, a side comment with a static leading pattern, which is
C<##> by default, will be spaced only a single space from previous
character, and it will not be vertically aligned with other side comments.

The default is B<-nssc>.

=item B<-sscp=string>, B<--static-side-comment-prefix=string>

This parameter defines the prefix used to identify static side comments
when the B<-ssc> parameter is set.  The default prefix is C<##>,
corresponding to C<-sscp=##>.

Please note that B<-sscp> merely defines the pattern used to identify
static side comments; it will not be used unless the switch B<-ssc> is
set.  Also, note that this string is used in a perl regular expression
which identifies these comments, so it must enable a valid regular
expression to be formed.

=back

=back

=head2 Skipping Selected Sections of Code

Selected lines of code may be passed verbatim to the output without any
formatting by marking the starting and ending lines with special comments.
There are two options for doing this.  The first option is called
B<--format-skipping> or B<-fs>, and the second option is called
B<--code-skipping> or B<-cs>.

In both cases the lines of code will be output without any changes.
The difference is that in B<--format-skipping>
perltidy will still parse the marked lines of code and check for errors,
whereas in B<--code-skipping> perltidy will simply pass the lines to the output without any checking.

Both of these features are enabled by default and are invoked with special
comment markers.  B<--format-skipping> uses starting and ending markers '#<<<'
and '#>>>', like this:

 #<<<  format skipping: do not let perltidy change my nice formatting
    my @list = (1,
                1, 1,
                1, 2, 1,
                1, 3, 3, 1,
                1, 4, 6, 4, 1,);
 #>>>

B<--code-skipping> uses starting and ending markers '#<<V' and '#>>V', like
this:

 #<<V  code skipping: perltidy will pass this verbatim without error checking

    token ident_digit {
        [ [ <?word> | _ | <?digit> ] <?ident_digit>
        |   <''>
        ]
    };

 #>>V

Additional text may appear on the special comment lines provided that it
is separated from the marker by at least one space, as in the above examples.

Any number of code-skipping or format-skipping sections may appear in a file.
If an opening code-skipping or format-skipping comment is not followed by a
corresponding closing comment, then skipping continues to the end of the file.
If a closing code-skipping or format-skipping comment appears in a file but
does not follow a corresponding opening comment, then it is treated as an
ordinary comment without any special meaning.

It is recommended to use B<--code-skipping> only if you need to hide a block of
an extended syntax which would produce errors if parsed by perltidy, and use
B<--format-skipping> otherwise.  This is because the B<--format-skipping>
option provides the benefits of error checking, and there are essentially no
limitations on which lines to which it can be applied.  The B<--code-skipping>
option, on the other hand, does not do error checking and its use is more
restrictive because the code which remains, after skipping the marked lines,
must be syntactically correct code with balanced containers.

These features should be used sparingly to avoid littering code with markers,
but they can be helpful for working around occasional problems.

Note that it may be possible to avoid the use of B<--format-skipping> for the
specific case of a comma-separated list of values, as in the above example, by
simply inserting a blank or comment somewhere between the opening and closing
parens.  See the section L<"Controlling List Formatting">.

The following sections describe the available controls for these options.  They
should not normally be needed.

=over 4

=item B<-fs>,  B<--format-skipping>

As explained above, this flag, which is enabled by default, causes any code
between special beginning and ending comment markers to be passed to the output
without formatting.  The code between the comments is still checked for errors
however.  The default beginning marker is #<<< and the default ending marker is
#>>>.

Format skipping begins when a format skipping beginning comment is seen and
continues until a format-skipping ending comment is found.

This feature can be disabled with B<-nfs>.   This should not normally be necessary.

=item B<-fsb=string>,  B<--format-skipping-begin=string>

This and the next parameter allow the special beginning and ending comments to
be changed.  However, it is recommended that they only be changed if there is a
conflict between the default values and some other use.  If they are used, it
is recommended that they only be entered in a B<.perltidyrc> file, rather than
on a command line.  This is because properly escaping these parameters on a
command line can be difficult.

If changed comment markers do not appear to be working, use the B<-log> flag and
examine the F<.LOG> file to see if and where they are being detected.

The B<-fsb=string> parameter may be used to change the beginning marker for
format skipping.  The default is equivalent to -fsb='#<<<'.  The string that
you enter must begin with a # and should be in quotes as necessary to get past
the command shell of your system.  It is actually the leading text of a pattern
that is constructed by appending a '\s', so you must also include backslashes
for characters to be taken literally rather than as patterns.

Some examples show how example strings become patterns:

 -fsb='#\{\{\{' becomes /^#\{\{\{\s/  which matches  #{{{ but not #{{{{
 -fsb='#\*\*'   becomes /^#\*\*\s/    which matches  #** but not #***
 -fsb='#\*{2,}' becomes /^#\*{2,}\s/  which matches  #** and #*****

=item B<-fse=string>,  B<--format-skipping-end=string>

The B<-fse=string> is the corresponding parameter used to change the
ending marker for format skipping.  The default is equivalent to
-fse='#<<<'.

The beginning and ending strings may be the same, but it is preferable
to make them different for clarity.

=item B<-cs>,  B<--code-skipping>

As explained above, this flag, which is enabled by default, causes any code
between special beginning and ending comment markers to be directly passed to
the output without any error checking or formatting.  Essentially, perltidy
treats it as if it were a block of arbitrary text.  The default beginning
marker is #<<V and the default ending marker is #>>V.

This feature can be disabled with B<-ncs>.   This should not normally be
necessary.

=item B<-csb=string>,  B<--code-skipping-begin=string>

This may be used to change the beginning comment for a B<--code-skipping> section, and its use is similar to the B<-fsb=string>.
The default is equivalent to -csb='#<<V'.

=item B<-cse=string>,  B<--code-skipping-end=string>

This may be used to change the ending comment for a B<--code-skipping> section, and its use is similar to the B<-fse=string>.
The default is equivalent to -cse='#>>V'.

=back

=head2 Line Break Control

The parameters in this and the next sections control breaks after
non-blank lines of code.  Blank lines are controlled
separately by parameters in the section L<"Blank Line Control">.

=over 4

=item B<-dnl>,  B<--delete-old-newlines>

By default, perltidy first deletes all old line break locations, and then it
looks for good break points to match the desired line length.  Use B<-ndnl>
or  B<--nodelete-old-newlines> to force perltidy to retain all old line break
points.

=item B<-anl>,  B<--add-newlines>

By default, perltidy will add line breaks when necessary to create
continuations of long lines and to improve the script appearance.  Use
B<-nanl> or B<--noadd-newlines> to prevent any new line breaks.

This flag does not prevent perltidy from eliminating existing line
breaks; see B<--freeze-newlines> to completely prevent changes to line
break points.

=item B<-fnl>,  B<--freeze-newlines>

If you do not want any changes to the line breaks within
lines of code in your script, set
B<-fnl>, and they will remain fixed, and the rest of the commands in
this section and sections
L<"Controlling List Formatting">,
L<"Retaining or Ignoring Existing Line Breaks">.
You may want to use B<-noll> with this.

Note: If you also want to keep your blank lines exactly
as they are, you can use the B<-fbl> flag which is described
in the section L<"Blank Line Control">.

=back

=head2 Controlling Breaks at Braces, Parens, and Square Brackets

=over 4

=item B<-ce>,   B<--cuddled-else>

Enable the "cuddled else" style, in which C<else> and C<elsif> are
follow immediately after the curly brace closing the previous block.
The default is not to use cuddled elses, and is indicated with the flag
B<-nce> or B<--nocuddled-else>.  Here is a comparison of the
alternatives:

  # -ce
  if ($task) {
      yyy();
  } else {
      zzz();
  }

  # -nce (default)
  if ($task) {
	yyy();
  }
  else {
	zzz();
  }

In this example the keyword B<else> is placed on the same line which begins with
the preceding closing block brace and is followed by its own opening block brace
on the same line.  Other keywords and function names which are formatted with
this "cuddled" style are B<elsif>, B<continue>, B<catch>, B<finally>.

Other block types can be formatted by specifying their names on a
separate parameter B<-cbl>, described in a later section.

Cuddling between a pair of code blocks requires that the closing brace of the
first block start a new line.  If this block is entirely on one line in the
input file, it is necessary to decide if it should be broken to allow cuddling.
This decision is controlled by the flag B<-cbo=n> discussed below.  The default
and recommended value of B<-cbo=1> bases this decision on the first block in
the chain.  If it spans multiple lines then cuddling is made and continues
along the chain, regardless of the sizes of subsequent blocks. Otherwise, short
lines remain intact.

So for example, the B<-ce> flag would not have any effect if the above snippet
is rewritten as

  if ($task) { yyy() }
  else {    zzz() }

If the first block spans multiple lines, then cuddling can be done and will
continue for the subsequent blocks in the chain, as illustrated in the previous
snippet.

If there are blank lines between cuddled blocks they will be eliminated.  If
there are comments after the closing brace where cuddling would occur then
cuddling will be prevented.  If this occurs, cuddling will restart later in the
chain if possible.

=item B<-cb>,   B<--cuddled-blocks>

This flag is equivalent to B<-ce>.


=item B<-cbl>,    B<--cuddled-block-list>

The built-in default cuddled block types are B<else, elsif, continue, catch, finally>.

Additional block types to which the B<-cuddled-blocks> style applies can be defined by
this parameter.  This parameter is a character string, giving a list of
block types separated by commas or spaces.  For example, to cuddle code blocks
of type sort, map and grep, in addition to the default types, the string could
be set to

  -cbl="sort map grep"

or equivalently

  -cbl=sort,map,grep

Note however that these particular block types are typically short so there might not be much
opportunity for the cuddled format style.

Using commas avoids the need to protect spaces with quotes.

As a diagnostic check, the flag B<--dump-cuddled-block-list> or B<-dcbl> can be
used to view the hash of values that are generated by this flag.

Finally, note that the B<-cbl> flag by itself merely specifies which blocks are formatted
with the cuddled format. It has no effect unless this formatting style is activated with
B<-ce>.

=item B<-cblx>,    B<--cuddled-block-list-exclusive>

When cuddled else formatting is selected with B<-ce>, setting this flag causes
perltidy to ignore its built-in defaults and rely exclusively on the block types
specified on the B<-cbl> flag described in the previous section.  For example,
to avoid using cuddled B<catch> and B<finally>, which are among the defaults, the
following set of parameters could be used:

  perltidy -ce -cbl='else elsif continue' -cblx


=item B<-cbo=n>, B<--cuddled-break-option=n>

Cuddled formatting is only possible between a pair of code blocks if the
closing brace of the first block starts a new line. If a block is encountered
which is entirely on a single line, and cuddled formatting is selected, it is
necessary to make a decision as to whether or not to "break" the block, meaning
to cause it to span multiple lines.  This parameter controls that decision. The
options are:

   cbo=0  Never force a short block to break.
   cbo=1  If the first of a pair of blocks is broken in the input file,
          then break the second [DEFAULT].
   cbo=2  Break open all blocks for maximal cuddled formatting.

The default and recommended value is B<cbo=1>.  With this value, if the starting
block of a chain spans multiple lines, then a cascade of breaks will occur for
remaining blocks causing the entire chain to be cuddled.

The option B<cbo=0> can produce erratic cuddling if there are numerous one-line
blocks.

The option B<cbo=2> produces maximal cuddling but will not allow any short blocks.

=item B<-bl>, B<--opening-brace-on-new-line>, or B<--brace-left>

Use the flag B<-bl> to place an opening block brace on a new line:

  if ( $input_file eq '-' )
  {
      ...
  }

By default it applies to all structural blocks except B<sort map grep eval> and
anonymous subs.

The default is B<-nbl> which places an opening brace on the same line as
the keyword introducing it if possible.  For example,

  # default
  if ( $input_file eq '-' ) {
     ...
  }

When B<-bl> is set, the blocks to which this applies can be controlled with the
parameters B<--brace-left-list> and B<-brace-left-exclusion-list> described in the next sections.

=item B<-bll=s>, B<--brace-left-list=s>

Use this parameter to change the types of block braces for which the
B<-bl> flag applies; see L<"Specifying Block Types">.  For example,
B<-bll='if elsif else sub'> would apply it to only C<if/elsif/else>
and named sub blocks.  The default is all blocks, B<-bll='*'>.

=item B<-blxl=s>, B<--brace-left-exclusion-list=s>

Use this parameter to exclude types of block braces for which the
B<-bl> flag applies; see L<"Specifying Block Types">.  For example,
the default settings B<-bll='*'> and B<-blxl='sort map grep eval asub'>
mean all blocks except B<sort map grep eval> and anonymous sub blocks.

Note that the lists B<-bll=s> and B<-blxl=s> control the behavior of the
B<-bl> flag but have no effect unless the B<-bl> flag is set.

=item B<-sbl>,    B<--opening-sub-brace-on-new-line>

The flag B<-sbl> provides a shortcut way to turn on B<-bl> just for named
subs.  The same effect can be achieved by turning on B<-bl>
with the block list set as B<-bll='sub'>.

For example,

 perltidy -sbl

produces this result:

 sub message
 {
    if (!defined($_[0])) {
        print("Hello, World\n");
    }
    else {
        print($_[0], "\n");
    }
 }

This flag is negated with B<-nsbl>, which is the default.

=item B<-asbl>,    B<--opening-anonymous-sub-brace-on-new-line>

The flag B<-asbl> is like the B<-sbl> flag except that it applies
to anonymous sub's instead of named subs. For example

 perltidy -asbl

produces this result:

 $a = sub
 {
     if ( !defined( $_[0] ) ) {
         print("Hello, World\n");
     }
     else {
         print( $_[0], "\n" );
     }
 };

This flag is negated with B<-nasbl>, and the default is B<-nasbl>.

=item B<-bli>,    B<--brace-left-and-indent>

The flag B<-bli> is similar to the B<-bl> flag but in addition it causes one
unit of continuation indentation ( see B<-ci> ) to be placed before
an opening and closing block braces.

For example, perltidy -bli gives

        if ( $input_file eq '-' )
          {
            important_function();
          }

By default, this extra indentation occurs for block types:
B<if>, B<elsif>, B<else>, B<unless>, B<while>, B<for>, B<foreach>, B<do>, and
also B<named subs> and blocks preceded by a B<label>.  The next item shows how to
change this.

B<Note>: The B<-bli> flag is similar to the B<-bl> flag, with the difference being
that braces get indented.  But these two flags are implemented independently,
and have different default settings for historical reasons.  If desired, a
mixture of effects can be achieved if desired by turning them both on with
different B<-list> settings.  In the event that both settings are selected for
a certain block type, the B<-bli> style has priority.

=item B<-blil=s>,    B<--brace-left-and-indent-list=s>

Use this parameter to change the types of block braces for which the
B<-bli> flag applies; see L<"Specifying Block Types">.

The default is B<-blil='if else elsif unless while for foreach do : sub'>.

=item B<-blixl=s>, B<--brace-left-and-indent-exclusion-list=s>

Use this parameter to exclude types of block braces for which the B<-bli> flag
applies; see L<"Specifying Block Types">.

This might be useful in conjunction with selecting all blocks B<-blil='*'>.
The default setting is B<-blixl=' '>, which does not exclude any blocks.

Note that the two parameters B<-blil> and B<-blixl> control the behavior of
the B<-bli> flag but have no effect unless the B<-bli> flag is set.

=item B<-bar>,    B<--opening-brace-always-on-right>

The default style, B<-nbl> places the opening code block brace on a new
line if it does not fit on the same line as the opening keyword, like
this:

        if ( $bigwasteofspace1 && $bigwasteofspace2
          || $bigwasteofspace3 && $bigwasteofspace4 )
        {
            big_waste_of_time();
        }

To force the opening brace to always be on the right, use the B<-bar>
flag.  In this case, the above example becomes

        if ( $bigwasteofspace1 && $bigwasteofspace2
          || $bigwasteofspace3 && $bigwasteofspace4 ) {
            big_waste_of_time();
        }

A conflict occurs if both B<-bl> and B<-bar> are specified.

=item B<-cpb>, B<--cuddled-paren-brace>

A related parameter, B<--cuddled-paren-brace>, causes perltidy to join
two lines which otherwise would be

      )
    {

to be

    ) {

For example:

    # default
    foreach my $dir (
        '05_lexer', '07_token', '08_regression', '11_util',
        '13_data',  '15_transform'
      )
    {
        ...
    }

    # perltidy -cpb
    foreach my $dir (
        '05_lexer', '07_token', '08_regression', '11_util',
        '13_data',  '15_transform'
    ) {
        ...;
    }

=item B<-otr>,  B<--opening-token-right> and related flags

The B<-otr> flag is a hint that perltidy should not place a break between a
comma and an opening token.  For example:

    # default formatting
    push @{ $self->{$module}{$key} },
      {
        accno       => $ref->{accno},
        description => $ref->{description}
      };

    # perltidy -otr
    push @{ $self->{$module}{$key} }, {
        accno       => $ref->{accno},
        description => $ref->{description}
      };

The flag B<-otr> is actually an abbreviation for three other flags
which can be used to control parens, hash braces, and square brackets
separately if desired:

  -opr  or --opening-paren-right
  -ohbr or --opening-hash-brace-right
  -osbr or --opening-square-bracket-right

=item B<-bbhb=n>,  B<--break-before-hash-brace=n> and related flags

When a list of items spans multiple lines, the default formatting is to place
the opening brace (or other container token) at the end of the starting line,
like this:

    $romanNumerals = {
        one   => 'I',
        two   => 'II',
        three => 'III',
        four  => 'IV',
    };

This flag can change the default behavior to cause a line break to be placed
before the opening brace according to the value given to the integer B<n>:

  -bbhb=0 never break [default]
  -bbhb=1 stable: break if the input script had a break
  -bbhb=2 break if list is 'complex' (see note below)
  -bbhb=3 always break

For example,

    # perltidy -bbhb=3
    $romanNumerals =
      {
        one   => 'I',
        two   => 'II',
        three => 'III',
        four  => 'IV',
      };

There are several points to note about this flag:

=over 4

=item *

This parameter only applies if the opening brace is preceded by an '='
or '=>'.

=item *

This parameter only applies if the contents of the container looks like a list.
The contents need to contain some commas or '=>'s at the next interior level to
be considered a list.

=item *

For the B<n=2> option, a list is considered 'complex' if it is part of a nested list
structure which spans multiple lines in the input file.

=item *

If multiple opening tokens have been 'welded' together with the B<-wn> parameter, then
this parameter has no effect.

=item *

The indentation of the braces will normally be one level of continuation
indentation by default.  This can be changed with the parameter
B<-bbhbi=n> in the next section.

=item *

Similar flags for controlling parens and square brackets are given in the subsequent section.

=back

=item B<-bbhbi=n>,  B<--break-before-hash-brace-and-indent=n>

This flag is a companion to B<-bbhb=n> for controlling the indentation of an opening hash brace
which is placed on a new line by that parameter.  The indentation is as follows:

  -bbhbi=0 one continuation level [default]
  -bbhbi=1 outdent by one continuation level
  -bbhbi=2 indent one full indentation level

For example:

    # perltidy -bbhb=3 -bbhbi=1
    $romanNumerals =
    {
        one   => 'I',
        two   => 'II',
        three => 'III',
        four  => 'IV',
    };

    # perltidy -bbhb=3 -bbhbi=2
    $romanNumerals =
        {
        one   => 'I',
        two   => 'II',
        three => 'III',
        four  => 'IV',
        };

Note that this parameter has no effect unless B<-bbhb=n> is also set.

=item B<-bbsb=n>,  B<--break-before-square-bracket=n>

This flag is similar to the flag described above, except it applies to lists contained within square brackets.

  -bbsb=0 never break [default]
  -bbsb=1 stable: break if the input script had a break
  -bbsb=2 break if list is 'complex' (part of nested list structure)
  -bbsb=3 always break

=item B<-bbsbi=n>,  B<--break-before-square-bracket-and-indent=n>

This flag is a companion to B<-bbsb=n> for controlling the indentation of an opening square bracket
which is placed on a new line by that parameter.  The indentation is as follows:

  -bbsbi=0 one continuation level [default]
  -bbsbi=1 outdent by one continuation level
  -bbsbi=2 indent one full indentation level

=item B<-bbp=n>,  B<--break-before-paren=n>

This flag is similar to B<-bbhb=n>, described above, except it applies to lists contained within parens.

  -bbp=0 never break [default]
  -bbp=1 stable: break if the input script had a break
  -bpb=2 break if list is 'complex' (part of nested list structure)
  -bbp=3 always break

=item B<-bbpi=n>,  B<--break-before-paren-and-indent=n>

This flag is a companion to B<-bbp=n> for controlling the indentation of an
opening paren which is placed on a new line by that parameter.  The indentation
is as follows:

  -bbpi=0 one continuation level [default]
  -bbpi=1 outdent by one continuation level
  -bbpi=2 indent one full indentation level

=item B<-bfvt=n>,  B<--brace-follower-vertical-tightness=n>

Some types of closing block braces, such as B<eval>, may be followed by
additional code.  A line break may be inserted between such a closing
brace and the following code depending on the parameter B<n> and
the length of the trailing code, as follows:

If the trailing code fits on a single line, then

  -bfvt=0 Follow the input style regarding break/no-break
  -bfvt=1 Follow the input style regarding break/no-break [Default]
  -bfvt=2 Do not insert a line break

If the trailing code requires multiple lines, then

  -bfvt=0 Insert a line break
  -bfvt=1 Insert a line break except for a cuddled block chain [Default]
  -bfvt=2 Do not insert a line break

So the most compact code is achieved with B<-bfvt=2>.

Example (non-cuddled, multiple lines ):

    # -bfvt=0 or -bvft=1 [DEFAULT]
    eval {
        ( $line, $cond ) = $self->_normalize_if_elif($line);
        1;
    }
      or die sprintf "Error at line %d\nLine %d: %s\n%s",
      ( $line_info->start_line_num() ) x 2, $line, $@;

    # -bfvt=2
    eval {
        ( $line, $cond ) = $self->_normalize_if_elif($line);
        1;
    } or die sprintf "Error at line %d\nLine %d: %s\n%s",
      ( $line_info->start_line_num() ) x 2, $line, $@;

Example (cuddled, multiple lines):

    # -bfvt=0
    eval {
        #STUFF;
        1;    # return true
    }
      or do {
        ##handle error
      };

    # -bfvt=1 [DEFAULT] or -bfvt=2
    eval {
        #STUFF;
        1;    # return true
    } or do {
        ##handle error
    };

=back

=head2 Welding

=over 4

=item B<-wn>,  B<--weld-nested-containers>

The B<-wn> flag causes closely nested pairs of opening and closing container
symbols (curly braces, brackets, or parens) to be "welded" together, meaning
that they are treated as if combined into a single unit, with the indentation
of the innermost code reduced to be as if there were just a single container
symbol.

For example:

	# default formatting
        do {
            {
                next if $x == $y;
            }
        } until $x++ > $z;

	# perltidy -wn
        do { {
            next if $x == $y;
        } } until $x++ > $z;

When this flag is set perltidy makes a preliminary pass through the file and
identifies all nested pairs of containers.  To qualify as a nested pair, the
closing container symbols must be immediately adjacent and the opening symbols
must either (1) be adjacent as in the above example, or (2) have an anonymous
sub declaration following an outer opening container symbol which is not a
code block brace, or (3) have an outer opening paren separated from the inner
opening symbol by any single non-container symbol or something that looks like
a function evaluation, as illustrated in the next examples. An additional
option (4) which can be turned on with the flag B<--weld-fat-comma> is when the opening container symbols are separated by a hash key and fat comma (=>).

Any container symbol may serve as both the inner container of one pair and as
the outer container of an adjacent pair. Consequently, any number of adjacent
opening or closing symbols may join together in weld.  For example, here are
three levels of wrapped function calls:

	# default formatting
        my (@date_time) = Localtime(
            Date_to_Time(
                Add_Delta_DHMS(
                    $year, $month,  $day, $hour, $minute, $second,
                    '0',   $offset, '0',  '0'
                )
            )
        );

        # perltidy -wn
        my (@date_time) = Localtime( Date_to_Time( Add_Delta_DHMS(
            $year, $month,  $day, $hour, $minute, $second,
            '0',   $offset, '0',  '0'
        ) ) );

Notice how the indentation of the inner lines are reduced by two levels in this
case.  This example also shows the typical result of this formatting, namely it
is a sandwich consisting of an initial opening layer, a central section of any
complexity forming the "meat" of the sandwich, and a final closing layer.  This
predictable structure helps keep the compacted structure readable.

The inner sandwich layer is required to be at least one line thick.  If this
cannot be achieved, welding does not occur.  This constraint can cause
formatting to take a couple of iterations to stabilize when it is first applied
to a script. The B<-conv> flag can be used to insure that the final format is
achieved in a single run.

Here is an example illustrating a welded container within a welded containers:

	# default formatting
        $x->badd(
            bmul(
                $class->new(
                    abs(
                        $sx * int( $xr->numify() ) & $sy * int( $yr->numify() )
                    )
                ),
                $m
            )
        );

	# perltidy -wn
        $x->badd( bmul(
            $class->new( abs(
                $sx * int( $xr->numify() ) & $sy * int( $yr->numify() )
            ) ),
            $m
        ) );

The welded closing tokens are by default on a separate line but this can be
modified with the B<-vtc=n> flag (described in the next section).  For example,
the same example adding B<-vtc=2> is

	# perltidy -wn -vtc=2
        $x->badd( bmul(
            $class->new( abs(
                $sx * int( $xr->numify() ) & $sy * int( $yr->numify() ) ) ),
            $m ) );

This format option is quite general but there are some limitations.

One limitation is that any line length limit still applies and can cause long
welded sections to be broken into multiple lines.

Another limitation is that an opening symbol which delimits quoted text cannot
be included in a welded pair.  This is because quote delimiters are treated
specially in perltidy.

Finally, the stacking of containers defined by this flag have priority over
any other container stacking flags.  This is because any welding is done first.

=item B<-wfc>,  B<--weld-fat-comma >

When the B<-wfc> flag is set, along with B<-wn>, perltidy is allowed to weld
an opening paren to an inner opening container when they are separated by a hash key and fat comma (=>). for example

    # perltidy -wn -wfc
    elf->call_method( method_name_foo => {
        some_arg1       => $foo,
        some_other_arg3 => $bar->{'baz'},
    } );

This option is off by default.

=item B<-wnxl=s>,  B<--weld-nested-exclusion-list>

The B<-wnxl=s> flag provides some control over the types of containers which
can be welded.  The B<-wn> flag by default is "greedy" in welding adjacent
containers.  If it welds more types of containers than desired, this flag
provides a capability to reduce the amount of welding by specifying a list
of things which should B<not> be welded.

The logic in perltidy to apply this is straightforward.  As each container
token is being considered for joining a weld, any exclusion rules are consulted
and used to reject the weld if necessary.

This list is a string with space-separated items.  Each item consists of up to
three pieces of information: (1) an optional position, (2) an optional
preceding type, and (3) a container type.

The only required piece of information is a container type, which is one of
'(', '[', '{' or 'q'.  The first three of these are container tokens and the
last represents a quoted list.  For example the string

  -wnxl='[ { q'

means do B<NOT> include square-brackets, braces, or quotes in any welds.  The only unspecified
container is '(', so this string means that only welds involving parens will be made.

To illustrate, following welded snippet consists of a chain of three welded
containers with types '(' '[' and 'q':

    # perltidy -wn
    skip_symbols( [ qw(
        Perl_dump_fds
        Perl_ErrorNo
        Perl_GetVars
        PL_sys_intern
    ) ] );

Even though the qw term uses parens as the quote delimiter, it has a special
type 'q' here. If it appears in a weld it always appears at the end of the
welded chain.

Any of the container types '[', '{', and '(' may be prefixed with a position
indicator which is either '^', to indicate the first token of a welded
sequence, or '.', to indicate an interior token of a welded sequence.  (Since
a quoted string 'q' always ends a chain it does need a position indicator).

For example, if we do not want a sequence of welded containers to start with a
square bracket we could use

  -wnxl='^['

In the above snippet, there is a square bracket but it does not start the chain,
so the formatting would be unchanged if it were formatted with this restriction.

A third optional item of information which can be given is an alphanumeric
letter which is used to limit the selection further depending on the type of
token immediately before the container.  If given, it goes just before the
container symbol.  The possible letters are currently 'k', 'K', 'f', 'F',
'w', and 'W', with these meanings:

 'k' matches if the previous nonblank token is a perl built-in keyword (such as 'if', 'while'),
 'K' matches if 'k' does not, meaning that the previous token is not a keyword.
 'f' matches if the previous token is a function other than a keyword.
 'F' matches if 'f' does not.
 'w' matches if either 'k' or 'f' match.
 'W' matches if 'w' does not.

For example, compare

        # perltidy -wn
        if ( defined( $_Cgi_Query{
            $Config{'methods'}{'authentication'}{'remote'}{'cgi'}{'username'}
        } ) )

with

        # perltidy -wn -wnxl='^K( {'
        if ( defined(
            $_Cgi_Query{ $Config{'methods'}{'authentication'}{'remote'}{'cgi'}
                  {'username'} }
        ) )

The first case does maximum welding. In the second case the leading paren is
retained by the rule (it would have been rejected if preceded by a non-keyword)
but the curly brace is rejected by the rule.

Here are some additional example strings and their meanings:

    '^('   - the weld must not start with a paren
    '.('   - the second and later tokens may not be parens
    '.w('  - the second and later tokens may not keyword or function call parens
    '('    - no parens in a weld
    '^K('  - exclude a leading paren preceded by a non-keyword
    '.k('  - exclude a secondary paren preceded by a keyword
    '[ {'  - exclude all brackets and braces
    '[ ( ^K{' - exclude everything except nested structures like do {{  ... }}


=item B<Vertical tightness> of non-block curly braces, parentheses, and square brackets.

These parameters control what shall be called vertical tightness.  Here are the
main points:

=over 4

=item *

Opening tokens (except for block braces) are controlled by B<-vt=n>, or
B<--vertical-tightness=n>, where

 -vt=0 always break a line after opening token (default).
 -vt=1 do not break unless this would produce more than one
         step in indentation in a line.
 -vt=2 never break a line after opening token

=item *

You must also use the B<-lp> flag when you use the B<-vt> flag; the
reason is explained below.

=item *

Closing tokens (except for block braces) are controlled by B<-vtc=n>, or
B<--vertical-tightness-closing=n>, where

 -vtc=0 always break a line before a closing token (default),
 -vtc=1 do not break before a closing token which is followed
        by a semicolon or another closing token, and is not in
        a list environment.
 -vtc=2 never break before a closing token.
 -vtc=3 Like -vtc=1 except always break before a closing token
        if the corresponding opening token follows an = or =>.

The rules for B<-vtc=1> and B<-vtc=3> are designed to maintain a reasonable
balance between tightness and readability in complex lists.

=item *

Different controls may be applied to different token types,
and it is also possible to control block braces; see below.

=item *

Finally, please note that these vertical tightness flags are merely
hints to the formatter, and it cannot always follow them.  Things which
make it difficult or impossible include comments, blank lines, blocks of
code within a list, and possibly the lack of the B<-lp> parameter.
Also, these flags may be ignored for very small lists (2 or 3 lines in
length).

=back

Here are some examples:

    # perltidy -lp -vt=0 -vtc=0
    %romanNumerals = (
                       one   => 'I',
                       two   => 'II',
                       three => 'III',
                       four  => 'IV',
    );

    # perltidy -lp -vt=1 -vtc=0
    %romanNumerals = ( one   => 'I',
                       two   => 'II',
                       three => 'III',
                       four  => 'IV',
    );

    # perltidy -lp -vt=1 -vtc=1
    %romanNumerals = ( one   => 'I',
                       two   => 'II',
                       three => 'III',
                       four  => 'IV', );

    # perltidy -vtc=3
    my_function(
        one   => 'I',
        two   => 'II',
        three => 'III',
        four  => 'IV', );

    # perltidy -vtc=3
    %romanNumerals = (
        one   => 'I',
        two   => 'II',
        three => 'III',
        four  => 'IV',
    );

In the last example for B<-vtc=3>, the opening paren is preceded by an equals
so the closing paren is placed on a new line.

The difference between B<-vt=1> and B<-vt=2> is shown here:

    # perltidy -lp -vt=1
    $init->add(
                mysprintf( "(void)find_threadsv(%s);",
                           cstring( $threadsv_names[ $op->targ ] )
                )
    );

    # perltidy -lp -vt=2
    $init->add( mysprintf( "(void)find_threadsv(%s);",
                           cstring( $threadsv_names[ $op->targ ] )
                )
    );

With B<-vt=1>, the line ending in C<add(> does not combine with the next
line because the next line is not balanced.  This can help with
readability, but B<-vt=2> can be used to ignore this rule.

The tightest, and least readable, code is produced with both C<-vt=2> and
C<-vtc=2>:

    # perltidy -lp -vt=2 -vtc=2
    $init->add( mysprintf( "(void)find_threadsv(%s);",
                           cstring( $threadsv_names[ $op->targ ] ) ) );

Notice how the code in all of these examples collapses vertically as
B<-vt> increases, but the indentation remains unchanged.  This is
because perltidy implements the B<-vt> parameter by first formatting as
if B<-vt=0>, and then simply overwriting one output line on top of the
next, if possible, to achieve the desired vertical tightness.  The
B<-lp> indentation style has been designed to allow this vertical
collapse to occur, which is why it is required for the B<-vt> parameter.

The B<-vt=n> and B<-vtc=n> parameters apply to each type of container
token.  If desired, vertical tightness controls can be applied
independently to each of the closing container token types.

The parameters for controlling parentheses are B<-pvt=n> or
B<--paren-vertical-tightness=n>, and B<-pvtc=n> or
B<--paren-vertical-tightness-closing=n>.

Likewise, the parameters for square brackets are B<-sbvt=n> or
B<--square-bracket-vertical-tightness=n>, and B<-sbvtc=n> or
B<--square-bracket-vertical-tightness-closing=n>.

Finally, the parameters for controlling non-code block braces are
B<-bvt=n> or B<--brace-vertical-tightness=n>, and B<-bvtc=n> or
B<--brace-vertical-tightness-closing=n>.

In fact, the parameter B<-vt=n> is actually just an abbreviation for
B<-pvt=n -bvt=n sbvt=n>, and likewise B<-vtc=n> is an abbreviation
for B<-pvtc=n -bvtc=n -sbvtc=n>.

=item B<-bbvt=n> or B<--block-brace-vertical-tightness=n>

The B<-bbvt=n> flag is just like the B<-vt=n> flag but applies
to opening code block braces.

 -bbvt=0 break after opening block brace (default).
 -bbvt=1 do not break unless this would produce more than one
         step in indentation in a line.
 -bbvt=2 do not break after opening block brace.

It is necessary to also use either B<-bl> or B<-bli> for this to work,
because, as with other vertical tightness controls, it is implemented by
simply overwriting a line ending with an opening block brace with the
subsequent line.  For example:

    # perltidy -bli -bbvt=0
    if ( open( FILE, "< $File" ) )
      {
        while ( $File = <FILE> )
          {
            $In .= $File;
            $count++;
          }
        close(FILE);
      }

    # perltidy -bli -bbvt=1
    if ( open( FILE, "< $File" ) )
      { while ( $File = <FILE> )
          { $In .= $File;
            $count++;
          }
        close(FILE);
      }

By default this applies to blocks associated with keywords B<if>,
B<elsif>, B<else>, B<unless>, B<for>, B<foreach>, B<sub>, B<while>,
B<until>, and also with a preceding label.  This can be changed with
the parameter B<-bbvtl=string>, or
B<--block-brace-vertical-tightness-list=string>, where B<string> is a
space-separated list of block types.  For more information on the
possible values of this string, see L<"Specifying Block Types">

For example, if we want to just apply this style to C<if>,
C<elsif>, and C<else> blocks, we could use
C<perltidy -bli -bbvt=1 -bbvtl='if elsif else'>.

There is no vertical tightness control for closing block braces; with
one exception they will be placed on separate lines.
The exception is that a cascade of closing block braces may
be stacked on a single line.  See B<-scbb>.

=item B<-sot>,  B<--stack-opening-tokens> and related flags

The B<-sot> flag tells perltidy to "stack" opening tokens
when possible to avoid lines with isolated opening tokens.

For example:

    # default
    $opt_c = Text::CSV_XS->new(
        {
            binary       => 1,
            sep_char     => $opt_c,
            always_quote => 1,
        }
    );

    # -sot
    $opt_c = Text::CSV_XS->new( {
            binary       => 1,
            sep_char     => $opt_c,
            always_quote => 1,
        }
    );

For detailed control of individual closing tokens the following
controls can be used:

  -sop  or --stack-opening-paren
  -sohb or --stack-opening-hash-brace
  -sosb or --stack-opening-square-bracket
  -sobb or --stack-opening-block-brace

The flag B<-sot> is an abbreviation for B<-sop -sohb -sosb>.

The flag B<-sobb> is an abbreviation for B<-bbvt=2 -bbvtl='*'>.  This
will case a cascade of opening block braces to appear on a single line,
although this an uncommon occurrence except in test scripts.

=item B<-sct>,  B<--stack-closing-tokens> and related flags

The B<-sct> flag tells perltidy to "stack" closing tokens
when possible to avoid lines with isolated closing tokens.

For example:

    # default
    $opt_c = Text::CSV_XS->new(
        {
            binary       => 1,
            sep_char     => $opt_c,
            always_quote => 1,
        }
    );

    # -sct
    $opt_c = Text::CSV_XS->new(
        {
            binary       => 1,
            sep_char     => $opt_c,
            always_quote => 1,
        } );

The B<-sct> flag is somewhat similar to the B<-vtc> flags, and in some
cases it can give a similar result.  The difference is that the B<-vtc>
flags try to avoid lines with leading opening tokens by "hiding" them at
the end of a previous line, whereas the B<-sct> flag merely tries to
reduce the number of lines with isolated closing tokens by stacking them
but does not try to hide them.  For example:

    # -vtc=2
    $opt_c = Text::CSV_XS->new(
        {
            binary       => 1,
            sep_char     => $opt_c,
            always_quote => 1, } );

For detailed control of the stacking of individual closing tokens the
following controls can be used:

  -scp  or --stack-closing-paren
  -schb or --stack-closing-hash-brace
  -scsb or --stack-closing-square-bracket
  -scbb or --stack-closing-block-brace

The flag B<-sct> is an abbreviation for stacking the non-block closing
tokens, B<-scp -schb -scsb>.

Stacking of closing block braces, B<-scbb>, causes a cascade of isolated
closing block braces to be combined into a single line as in the following
example:

    # -scbb:
    for $w1 (@w1) {
        for $w2 (@w2) {
            for $w3 (@w3) {
                for $w4 (@w4) {
                    push( @lines, "$w1 $w2 $w3 $w4\n" );
                } } } }

To simplify input even further for the case in which both opening and closing
non-block containers are stacked, the flag B<-sac> or B<--stack-all-containers>
is an abbreviation for B<-sot -sct>.

Please note that if both opening and closing tokens are to be stacked, then the
newer flag B<-weld-nested-containers> may be preferable because it insures that
stacking is always done symmetrically.  It also removes an extra level of
unnecessary indentation within welded containers.  It is able to do this
because it works on formatting globally rather than locally, as the B<-sot> and
B<-sct> flags do.

=back

=head2 Breaking Before or After Operators

Four command line parameters provide some control over whether
a line break should be before or after specific token types.
Two parameters give detailed control:

B<-wba=s> or B<--want-break-after=s>, and

B<-wbb=s> or B<--want-break-before=s>.

These parameters are each followed by a quoted string, B<s>, containing
a list of token types (separated only by spaces).  No more than one of each
of these parameters should be specified, because repeating a
command-line parameter always overwrites the previous one before
perltidy ever sees it.

By default, perltidy breaks B<after> these token types:
  % + - * / x != == >= <= =~ !~ < >  | &
  = **= += *= &= <<= &&= -= /= |= >>= ||= //= .= %= ^= x=

And perltidy breaks B<before> these token types by default:
  . << >> -> && || //

To illustrate, to cause a break after a concatenation operator, C<'.'>,
rather than before it, the command line would be

  -wba="."

As another example, the following command would cause a break before
math operators C<'+'>, C<'-'>, C<'/'>, and C<'*'>:

  -wbb="+ - / *"

These commands should work well for most of the token types that perltidy uses
(use B<--dump-token-types> for a list).  Also try the B<-D> flag on a short
snippet of code and look at the .DEBUG file to see the tokenization.  However,
for a few token types there may be conflicts with hardwired logic which cause
unexpected results.  One example is curly braces, which should be controlled
with the parameter B<bl> provided for that purpose.

B<WARNING> Be sure to put these tokens in quotes to avoid having them
misinterpreted by your command shell.

Two additional parameters are available which, though they provide no further
capability, can simplify input are:

B<-baao> or B<--break-after-all-operators>,

B<-bbao> or B<--break-before-all-operators>.

The -baao sets the default to be to break after all of the following operators:

    % + - * / x != == >= <= =~ !~ < > | &
    = **= += *= &= <<= &&= -= /= |= >>= ||= //= .= %= ^= x=
    . : ? && || and or err xor

and the B<-bbao> flag sets the default to break before all of these operators.
These can be used to define an initial break preference which can be fine-tuned
with the B<-wba> and B<-wbb> flags.  For example, to break before all operators
except an B<=> one could use --bbao -wba='=' rather than listing every
single perl operator except B<=> on a -wbb flag.

=over 4

=item B<-bal=n, --break-after-labels=n>

This flag controls whether or not a line break occurs after a label. There
are three possible values for B<n>:

  -bal=0  break if there is a break in the input [DEFAULT]
  -bal=1  always break after a label
  -bal=2  never break after a label

For example,

      # perltidy -bal=1
      RETURN:
        return;

      # perltidy -bal=2
      RETURN: return;

=back

=head2 Controlling List Formatting

Perltidy attempts to format lists of comma-separated values in tables which
look good.  Its default algorithms usually work well, but sometimes they don't.
In this case, there are several methods available to control list formatting.

A very simple way to prevent perltidy from changing the line breaks
within a comma-separated list of values is to insert a blank line,
comment, or side-comment anywhere between the opening and closing
parens (or braces or brackets).   This causes perltidy to skip
over its list formatting logic.  (The reason is that any of
these items put a constraint on line breaks, and perltidy
needs complete control over line breaks within a container to
adjust a list layout).  For example, let us consider

    my @list = (1,
                1, 1,
                1, 2, 1,
                1, 3, 3, 1,
                1, 4, 6, 4, 1,);

The default formatting, which allows a maximum line length of 80,
will flatten this down to one line:

    # perltidy (default)
    my @list = ( 1, 1, 1, 1, 2, 1, 1, 3, 3, 1, 1, 4, 6, 4, 1, );

This formatting loses the nice structure.  If we place a side comment anywhere
between the opening and closing parens, the original line break points are
retained.  For example,

    my @list = (
        1,    # a side comment forces the original line breakpoints to be kept
        1, 1,
        1, 2, 1,
        1, 3, 3, 1,
        1, 4, 6, 4, 1,
    );

The side comment can be a single hash symbol without any text.
We could achieve the same result with a blank line or full comment
anywhere between the opening and closing parens.  Vertical alignment
of the list items will still occur if possible.

For another possibility see
the -fs flag in L<"Skipping Selected Sections of Code">.

=over 4

=item B<-boc>,  B<--break-at-old-comma-breakpoints>

The B<-boc> flag is another way to prevent comma-separated lists from being
reformatted.  Using B<-boc> on the above example, plus additional flags to retain
the original style, yields

    # perltidy -boc -lp -pt=2 -vt=1 -vtc=1
    my @list = (1,
                1, 1,
                1, 2, 1,
                1, 3, 3, 1,
                1, 4, 6, 4, 1,);

A disadvantage of this flag compared to the methods discussed above is that all
tables in the file must already be nicely formatted.

=item B<-mft=n>,  B<--maximum-fields-per-table=n>

If B<n> is a positive number, and the computed number of fields for any table
exceeds B<n>, then it will be reduced to B<n>.  This parameter might be used on
a small section of code to force a list to have a particular number of fields
per line, and then either the B<-boc> flag could be used to retain this
formatting, or a single comment could be introduced somewhere to freeze the
formatting in future applications of perltidy. For example

    # perltidy -mft=2
    @month_of_year = (
        'Jan', 'Feb',
        'Mar', 'Apr',
        'May', 'Jun',
        'Jul', 'Aug',
        'Sep', 'Oct',
        'Nov', 'Dec'
    );

The default value is B<n=0>, which does not place a limit on the
number of fields in a table.

=item B<-cab=n>,  B<--comma-arrow-breakpoints=n>

A comma which follows a comma arrow, '=>', is given special
consideration.  In a long list, it is common to break at all such
commas.  This parameter can be used to control how perltidy breaks at
these commas.  (However, it will have no effect if old comma breaks are
being forced because B<-boc> is used).  The possible values of B<n> are:

 n=0 break at all commas after =>
 n=1 stable: break at all commas after => if container is open,
     EXCEPT FOR one-line containers
 n=2 break at all commas after =>, BUT try to form the maximum
     one-line container lengths
 n=3 do not treat commas after => specially at all
 n=4 break everything: like n=0 but ALSO break a short container with
     a => not followed by a comma when -vt=0 is used
 n=5 stable: like n=1 but ALSO break at open one-line containers when
     -vt=0 is used (default)

For example, given the following single line, perltidy by default will
not add any line breaks because it would break the existing one-line
container:

    bless { B => $B, Root => $Root } => $package;

Using B<-cab=0> will force a break after each comma-arrow item:

    # perltidy -cab=0:
    bless {
        B    => $B,
        Root => $Root
    } => $package;

If perltidy is subsequently run with this container broken, then by
default it will break after each '=>' because the container is now
broken.  To reform a one-line container, the parameter B<-cab=2> could
be used.

The flag B<-cab=3> can be used to prevent these commas from being
treated specially.  In this case, an item such as "01" => 31 is
treated as a single item in a table.  The number of fields in this table
will be determined by the same rules that are used for any other table.
Here is an example.

    # perltidy -cab=3
    my %last_day = (
        "01" => 31, "02" => 29, "03" => 31, "04" => 30,
        "05" => 31, "06" => 30, "07" => 31, "08" => 31,
        "09" => 30, "10" => 31, "11" => 30, "12" => 31
    );

=back

=head2 Adding and Deleting Commas

=over 4

=item B<-drc>,  B<--delete-repeated-commas>

Repeated commas in a list are undesirable and can be removed with this flag.
For example, given this list with a repeated comma

      ignoreSpec( $file, "file",, \%spec, \%Rspec );

we can remove it with -drc

      # perltidy -drc:
      ignoreSpec( $file, "file", \%spec, \%Rspec );

Since the default is not to add or delete commas, this feature is off by default and must be requested.


=item B<--want-trailing-commas=s> or B<-wtc=s>, B<--add-trailing-commas> or B<-atc>, and B<--delete-trailing-commas> or B<-dtc>

A trailing comma is a comma following the last item of a list. Perl allows
trailing commas but they are not required.  By default, perltidy does not add
or delete trailing commas, but it is possible to manipulate them with the
following set of three related parameters:

  --want-trailing-commas=s, -wtc=s - defines where trailing commas are wanted
  --add-trailing-commas,    -atc   - gives permission to add trailing commas to match the style wanted
  --delete-trailing-commas, -dtc   - gives permission to delete trailing commas which do not match the style wanted

The parameter B<--want-trailing-commas=s>, or B<-wtc=s>, defines a preferred style.  The string B<s> indicates which lists should get trailing commas, as follows:

  s=0 : no list should have a trailing comma
  s=1 or * : every list should have a trailing comma
  s=m a multi-line list should have a trailing commas
  s=b trailing commas should be 'bare' (comma followed by newline)
  s=h lists of key=>value pairs, with about one one '=>' and one ',' per line,
      with a bare trailing comma
  s=i lists with about one comma per line, with a bare trailing comma
  s=' ' or -wtc not defined : leave trailing commas unchanged [DEFAULT].

This parameter by itself only indicates the where trailing commas are
wanted.  Perltidy only adds these trailing commas if the flag B<--add-trailing-commas>, or B<-atc> is set.  And perltidy only removes unwanted trailing commas
if the flag B<--delete-trailing-commas>, or B<-dtc> is set.

Here are some example parameter combinations and their meanings

  -wtc=0 -dtc   : delete all trailing commas
  -wtc=1 -atc   : all lists get trailing commas
  -wtc=m -atc   : all multi-line lists get trailing commas, but
                  single line lists remain unchanged.
  -wtc=m -dtc   : multi-line lists remain unchanged, but
                  any trailing commas on single line lists are removed.
  -wtc=m -atc -dtc  : all multi-line lists get trailing commas, and
                      any trailing commas on single line lists are removed.

For example, given the following input without a trailing comma

    bless {
        B    => $B,
        Root => $Root
    } => $package;

we can add a trailing comma after the variable C<$Root> using

    # perltidy -wtc=m -atc
    bless {
        B    => $B,
        Root => $Root,
    } => $package;

This could also be achieved in this case with B<-wtc=b> instead of B<-wtc=m>
because the trailing comma here is bare (separated from its closing brace by a
newline).  And it could also be achieved with B<-wtc=h> because this particular
list is a list of key=>value pairs.

The above styles should cover the main of situations of interest, but it is
possible to apply a different style to each type of container token by
including an opening token ahead of the style character in the above table.
For example

    -wtc='(m [b'

means that lists within parens should have multi-line trailing commas, and that
lists within square brackets have bare trailing commas. Since there is no
specification for curly braces in this example, their trailing commas would
remain unchanged.

For parentheses, an additional item of information which can be given is an
alphanumeric letter which is used to limit the selection further depending on
the type of token immediately before the opening paren.  The possible letters
are currently 'k', 'K', 'f', 'F', 'w', and 'W', with these meanings for
matching whatever precedes an opening paren:

 'k' matches if the previous nonblank token is a perl built-in keyword (such as 'if', 'while'),
 'K' matches if 'k' does not, meaning that the previous token is not a keyword.
 'f' matches if the previous token is a function other than a keyword.
 'F' matches if 'f' does not.
 'w' matches if either 'k' or 'f' match.
 'W' matches if 'w' does not.

These are the same codes used for B<--line-up-parentheses-inclusion-list>.
For example,

  -wtc = 'w(m'

means that trailing commas are wanted for multi-line parenthesized lists following a function call or keyword.

Here are some points to note regarding adding and deleting trailing commas:

=over 4

=item *

For the implementation of these parameters, a B<list> is basically taken to be
a container of items (parens, square brackets, or braces), which is not a code
block, with one or more commas.  These parameters only apply to something that
fits this definition of a list.

Note that a paren-less list of parameters is not a list by this definition, so
these parameters have no effect on a peren-less list.

Another consequence is that if the only comma in a list is deleted, then it
cannot later be added back with these parameters because the container no
longer fits this definition of a list.  For example, given

    my ( $self, ) = @_;

and if we remove the comma with

    # perltidy -wtc=m -dtc
    my ( $self ) = @_;

then we cannot use these trailing comma controls to add this comma back.

=item *

By B<multiline> list is meant a list for which the first comma and trailing comma
are on different lines.

=item *

A B<bare> trailing comma is a comma which is at the end of a line. That is,
the closing container token follows on a different line.  So a list with a
bare trailing comma is a special case of a multi-line list.

=item *

The decision regarding whether or not a list is multi-line or bare is
made based on the B<input> stream.  In some cases it may take an iteration
or two to reach a final state.

=item *

When using these parameters for the first time it is a good idea to practice
on some test scripts and verify that the results are as expected.

=item *

Since the default behavior is not to add or delete commas, these parameters
can be useful on a temporary basis for reformatting a script.

=back

=item B<-dwic>,  B<--delete-weld-interfering-commas>

If the closing tokens of two nested containers are separated by a comma, then
welding requested with B<--weld-nested-containers> cannot occur.  Any commas in
this situation are optional trailing commas and can be removed with B<-dwic>.
For example, a comma in this script prevents welding:

    # perltidy -wn
    skip_symbols(
        [ qw(
            Perl_dump_fds
            Perl_ErrorNo
            Perl_GetVars
            PL_sys_intern
        ) ],
    );

Using B<-dwic> removes the comma and allows welding:

    # perltidy -wn -dwic
    skip_symbols( [ qw(
        Perl_dump_fds
        Perl_ErrorNo
        Perl_GetVars
        PL_sys_intern
    ) ] );

Since the default is not to add or delete commas, this feature is off by default.
Here are some points to note about the B<-dwic> parameter

=over 4

=item *

This operation is not reversible, so please check results of using this parameter carefully.

=item *

Removing this type of isolated trailing comma is necessary for welding to be
possible, but not sufficient.  So welding will not always occur where these
commas are removed.

=back

=back

=head2 Retaining or Ignoring Existing Line Breaks

Several additional parameters are available for controlling the extent
to which line breaks in the input script influence the output script.
In most cases, the default parameter values are set so that, if a choice
is possible, the output style follows the input style.  For example, if
a short logical container is broken in the input script, then the
default behavior is for it to remain broken in the output script.

Most of the parameters in this section would only be required for a
one-time conversion of a script from short container lengths to longer
container lengths.  The opposite effect, of converting long container
lengths to shorter lengths, can be obtained by temporarily using a short
maximum line length.

=over 4

=item B<-bol>,  B<--break-at-old-logical-breakpoints>

By default, if a logical expression is broken at a C<&&>, C<||>, C<and>,
or C<or>, then the container will remain broken.  Also, breaks
at internal keywords C<if> and C<unless> will normally be retained.
To prevent this, and thus form longer lines, use B<-nbol>.

Please note that this flag does not duplicate old logical breakpoints.  They
are merely used as a hint with this flag that a statement should remain
broken.  Without this flag, perltidy will normally try to combine relatively
short expressions into a single line.

For example, given this snippet:

    return unless $cmd = $cmd || ($dot
        && $Last_Shell) || &prompt('|');

    # perltidy -bol [default]
    return
      unless $cmd = $cmd
      || ( $dot
        && $Last_Shell )
      || &prompt('|');

    # perltidy -nbol
    return unless $cmd = $cmd || ( $dot && $Last_Shell ) || &prompt('|');

=item B<-bom>,  B<--break-at-old-method-breakpoints>

By default, a method call arrow C<-E<gt>> is considered a candidate for
a breakpoint, but method chains will fill to the line width before a break is
considered.  With B<-bom>, breaks before the arrow are preserved, so if you
have pre-formatted a method chain:

  my $q = $rs
    ->related_resultset('CDs')
    ->related_resultset('Tracks')
    ->search({
      'track.id' => {-ident => 'none_search.id'},
    })->as_query;

It will B<keep> these breaks, rather than become this:

  my $q = $rs->related_resultset('CDs')->related_resultset('Tracks')->search({
      'track.id' => {-ident => 'none_search.id'},
    })->as_query;

This flag will also look for and keep a 'cuddled' style of calls,
in which lines begin with a closing paren followed by a call arrow,
as in this example:

  # perltidy -bom -wn
  my $q = $rs->related_resultset(
      'CDs'
  )->related_resultset(
      'Tracks'
  )->search( {
      'track.id' => { -ident => 'none_search.id' },
  } )->as_query;

You may want to include the B<-weld-nested-containers> flag in this case to keep
nested braces and parens together, as in the last line.

=item B<-bos>,  B<--break-at-old-semicolon-breakpoints>

Semicolons are normally placed at the end of a statement.  This means that formatted lines do not normally begin with semicolons.  If the input stream has some lines which begin with semicolons, these can be retained by setting this flag.  For example, consider
the following two-line input snippet:

  $z = sqrt($x**2 + $y**2)
  ;

The default formatting will be:

  $z = sqrt( $x**2 + $y**2 );

The result using B<perltidy -bos> keeps the isolated semicolon:

  $z = sqrt( $x**2 + $y**2 )
    ;

The default is not to do this, B<-nbos>.


=item B<-bok>,  B<--break-at-old-keyword-breakpoints>

By default, perltidy will retain a breakpoint before keywords which may
return lists, such as C<sort> and <map>.  This allows chains of these
operators to be displayed one per line.  Use B<-nbok> to prevent
retaining these breakpoints.

=item B<-bot>,  B<--break-at-old-ternary-breakpoints>

By default, if a conditional (ternary) operator is broken at a C<:>,
then it will remain broken.  To prevent this, and thereby
form longer lines, use B<-nbot>.

=item B<-boa>,  B<--break-at-old-attribute-breakpoints>

By default, if an attribute list is broken at a C<:> in the source file, then
it will remain broken.  For example, given the following code, the line breaks
at the ':'s will be retained:

                    my @field
                      : field
                      : Default(1)
                      : Get('Name' => 'foo') : Set('Name');

If the attributes are on a single line in the source code then they will remain
on a single line if possible.

To prevent this, and thereby always form longer lines, use B<-nboa>.

=item B<Keeping old breakpoints at specific token types>

It is possible to override the choice of line breaks made by perltidy, and
force it to follow certain line breaks in the input stream, with these two
parameters:

B<-kbb=s> or B<--keep-old-breakpoints-before=s>, and

B<-kba=s> or B<--keep-old-breakpoints-after=s>

These parameters are each followed by a quoted string, B<s>, containing
a list of token types (separated only by spaces).  No more than one of each
of these parameters should be specified, because repeating a
command-line parameter always overwrites the previous one before
perltidy ever sees it.

For example, -kbb='=>' means that if an input line begins with a '=>' then the
output script should also have a line break before that token.

For example, given the script:

    method 'foo'
      => [ Int, Int ]
      => sub {
        my ( $self, $x, $y ) = ( shift, @_ );
        ...;
      };

    # perltidy [default]
    method 'foo' => [ Int, Int ] => sub {
        my ( $self, $x, $y ) = ( shift, @_ );
        ...;
    };

    # perltidy -kbb='=>'
    method 'foo'
      => [ Int, Int ]
      => sub {
        my ( $self, $x, $y ) = ( shift, @_ );
        ...;
      };

For the container tokens '{', '[' and '(' and, their closing counterparts, use the token symbol. Thus,
the command to keep a break after all opening parens is:

   perltidy -kba='('

It is possible to be more specific in matching parentheses by preceding them
with a letter.  The possible letters are 'k', 'K', 'f', 'F', 'w', and 'W', with
these meanings (these are the same as used in the
B<--weld-nested-exclusion-list> and B<--line-up-parentheses-exclusion-list>
parameters):

 'k' matches if the previous nonblank token is a perl built-in keyword (such as 'if', 'while'),
 'K' matches if 'k' does not, meaning that the previous token is not a keyword.
 'f' matches if the previous token is a function other than a keyword.
 'F' matches if 'f' does not.
 'w' matches if either 'k' or 'f' match.
 'W' matches if 'w' does not.

So for example the the following parameter will keep breaks after opening function call
parens:

   perltidy -kba='f('

B<NOTE>: A request to break before an opening container, such as B<-kbb='('>,
will be silently ignored because it can lead to formatting instability.
Likewise, a request to break after a closing container, such as B<-kba>=')',
will also be silently ignored.

=item B<-iob>,  B<--ignore-old-breakpoints>

Use this flag to tell perltidy to ignore existing line breaks to the
maximum extent possible.  This will tend to produce the longest possible
containers, regardless of type, which do not exceed the line length
limit.  But please note that this parameter has priority over all
other parameters requesting that certain old breakpoints be kept.

To illustrate, consider the following input text:

    has subcmds => (
        is => 'ro',
        default => sub { [] },
    );

The default formatting will keep the container broken, giving

    # perltidy [default]
    has subcmds => (
        is      => 'ro',
        default => sub { [] },
    );

If old breakpoints are ignored, the list will be flattened:

    # perltidy -iob
    has subcmds => ( is => 'ro', default => sub { [] }, );

Besides flattening lists, this parameter also applies to lines broken
at certain logical breakpoints such as 'if' and 'or'.

Even if this is parameter is not used globally, it provides a convenient way to
flatten selected lists from within an editor.

=item B<-kis>,  B<--keep-interior-semicolons>

Use the B<-kis> flag to prevent breaking at a semicolon if
there was no break there in the input file.  Normally
perltidy places a newline after each semicolon which
terminates a statement unless several statements are
contained within a one-line brace block.  To illustrate,
consider the following input lines:

    dbmclose(%verb_delim); undef %verb_delim;
    dbmclose(%expanded); undef %expanded;

The default is to break after each statement, giving

    dbmclose(%verb_delim);
    undef %verb_delim;
    dbmclose(%expanded);
    undef %expanded;

With B<perltidy -kis> the multiple statements are retained:

    dbmclose(%verb_delim); undef %verb_delim;
    dbmclose(%expanded);   undef %expanded;

The statements are still subject to the specified value
of B<maximum-line-length> and will be broken if this
maximum is exceeded.

=back

=head2 Blank Line Control

Blank lines can improve the readability of a script if they are carefully
placed.  Perltidy has several commands for controlling the insertion,
retention, and removal of blank lines.

=over 4

=item B<-fbl>,  B<--freeze-blank-lines>

Set B<-fbl> if you want to the blank lines in your script to
remain exactly as they are.  The rest of the parameters in
this section may then be ignored.  (Note: setting the B<-fbl> flag
is equivalent to setting B<-mbl=0> and B<-kbl=2>).

=item B<-bbc>,  B<--blanks-before-comments>

A blank line will be introduced before a full-line comment.  This is the
default.  Use B<-nbbc> or  B<--noblanks-before-comments> to prevent
such blank lines from being introduced.

=item B<-blbs=n>,  B<--blank-lines-before-subs=n>

The parameter B<-blbs=n> requests that least B<n> blank lines precede a sub
definition which does not follow a comment and which is more than one-line
long.  The default is <-blbs=1>.  B<BEGIN> and B<END> blocks are included.

The requested number of blanks statement will be inserted regardless of the
value of B<--maximum-consecutive-blank-lines=n> (B<-mbl=n>) with the exception
that if B<-mbl=0> then no blanks will be output.

This parameter interacts with the value B<k> of the parameter B<--maximum-consecutive-blank-lines=k> (B<-mbl=k>) as follows:

1. If B<-mbl=0> then no blanks will be output.  This allows all blanks to be suppressed with a single parameter.  Otherwise,

2. If the number of old blank lines in the script is less than B<n> then
additional blanks will be inserted to make the total B<n> regardless of the
value of B<-mbl=k>.

3. If the number of old blank lines in the script equals or exceeds B<n> then
this parameter has no effect, however the total will not exceed
value specified on the B<-mbl=k> flag.


=item B<-blbp=n>,  B<--blank-lines-before-packages=n>

The parameter B<-blbp=n> requests that least B<n> blank lines precede a package
which does not follow a comment.  The default is B<-blbp=1>.

This parameter interacts with the value B<k> of the parameter
B<--maximum-consecutive-blank-lines=k> (B<-mbl=k>) in the same way as described
for the previous item B<-blbs=n>.


=item B<-bbs>,  B<--blanks-before-subs>

For compatibility with previous versions, B<-bbs> or B<--blanks-before-subs>
is equivalent to F<-blbp=1> and F<-blbs=1>.

Likewise, B<-nbbs> or B<--noblanks-before-subs>
is equivalent to F<-blbp=0> and F<-blbs=0>.

=item B<-bbb>,  B<--blanks-before-blocks>

A blank line will be introduced before blocks of coding delimited by
B<for>, B<foreach>, B<while>, B<until>, and B<if>, B<unless>, in the following
circumstances:

=over 4

=item *

The block is not preceded by a comment.

=item *

The block is not a one-line block.

=item *

The number of consecutive non-blank lines at the current indentation depth is at least B<-lbl>
(see next section).

=back

This is the default.  The intention of this option is to introduce
some space within dense coding.
This is negated with B<-nbbb> or  B<--noblanks-before-blocks>.

=item B<-lbl=n> B<--long-block-line-count=n>

This controls how often perltidy is allowed to add blank lines before
certain block types (see previous section).  The default is 8.  Entering
a value of B<0> is equivalent to entering a very large number.

=item B<-blao=i> or B<--blank-lines-after-opening-block=i>

This control places a minimum of B<i> blank lines B<after> a line which B<ends>
with an opening block brace of a specified type.  By default, this only applies
to the block of a named B<sub>, but this can be changed (see B<-blaol> below).
The default is not to do this (B<i=0>).

Please see the note below on using the B<-blao> and B<-blbc> options.

=item B<-blbc=i> or B<--blank-lines-before-closing-block=i>

This control places a minimum of B<i> blank lines B<before> a line which
B<begins> with a closing block brace of a specified type.  By default, this
only applies to the block of a named B<sub>, but this can be changed (see
B<-blbcl> below).  The default is not to do this (B<i=0>).

=item B<-blaol=s> or B<--blank-lines-after-opening-block-list=s>

The parameter B<s> is a list of block type keywords to which the flag B<-blao>
should apply.  The section L<"Specifying Block Types"> explains how to list
block types.

=item B<-blbcl=s> or B<--blank-lines-before-closing-block-list=s>

This parameter is a list of block type keywords to which the flag B<-blbc>
should apply.  The section L<"Specifying Block Types"> explains how to list
block types.

=item B<Note on using the> B<-blao> and B<-blbc> options.

These blank line controls introduce a certain minimum number of blank lines in
the text, but the final number of blank lines may be greater, depending on
values of the other blank line controls and the number of old blank lines.  A
consequence is that introducing blank lines with these and other controls
cannot be exactly undone, so some experimentation with these controls is
recommended before using them.

For example, suppose that for some reason we decide to introduce one blank
space at the beginning and ending of all blocks.  We could do
this using

  perltidy -blao=2 -blbc=2 -blaol='*' -blbcl='*' filename

Now suppose the script continues to be developed, but at some later date we
decide we don't want these spaces after all. We might expect that running with
the flags B<-blao=0> and B<-blbc=0> will undo them.  However, by default
perltidy retains single blank lines, so the blank lines remain.

We can easily fix this by telling perltidy to ignore old blank lines by
including the added parameter B<-kbl=0> and rerunning. Then the unwanted blank
lines will be gone.  However, this will cause all old blank lines to be
ignored, perhaps even some that were added by hand to improve formatting. So
please be cautious when using these parameters.

=item B<-mbl=n> B<--maximum-consecutive-blank-lines=n>

This parameter specifies the maximum number of consecutive blank lines which
will be output within code sections of a script.  The default is n=1.  If the
input file has more than n consecutive blank lines, the number will be reduced
to n except as noted above for the B<-blbp> and B<-blbs> parameters.  If B<n=0>
then no blank lines will be output (unless all old blank lines are retained
with the B<-kbl=2> flag of the next section).

This flag obviously does not apply to pod sections,
here-documents, and quotes.

=item B<-kbl=n>,  B<--keep-old-blank-lines=n>

The B<-kbl=n> flag gives you control over how your existing blank lines are
treated.

The possible values of B<n> are:

 n=0 ignore all old blank lines
 n=1 stable: keep old blanks, but limited by the value of the B<-mbl=n> flag
 n=2 keep all old blank lines, regardless of the value of the B<-mbl=n> flag

The default is B<n=1>.

=item B<-sob>,  B<--swallow-optional-blank-lines>

This is equivalent to B<kbl=0> and is included for compatibility with
previous versions.

=item B<-nsob>,  B<--noswallow-optional-blank-lines>

This is equivalent to B<kbl=1> and is included for compatibility with
previous versions.

=back

B<Controls for blank lines around lines of consecutive keywords>

The parameters in this section provide some control over the placement of blank
lines within and around groups of statements beginning with selected keywords.
These blank lines are called here B<keyword group blanks>, and all of the
parameters begin with B<--keyword-group-blanks*>, or B<-kgb*> for short.  The
default settings do not employ these controls but they can be enabled with the
following parameters:

B<-kgbl=s> or B<--keyword-group-blanks-list=s>; B<s> is a quoted string of keywords

B<-kgbs=s> or B<--keyword-group-blanks-size=s>; B<s> gives the number of keywords required to form a group.

B<-kgbb=n> or B<--keyword-group-blanks-before=n>; B<n> = (0, 1, or 2) controls a leading blank

B<-kgba=n> or B<--keyword-group-blanks-after=n>; B<n> = (0, 1, or 2) controls a trailing blank

B<-kgbi> or B<--keyword-group-blanks-inside> is a switch for adding blanks between subgroups

B<-kgbd> or B<--keyword-group-blanks-delete> is a switch for removing initial blank lines between keywords

B<-kgbr=n> or B<--keyword-group-blanks-repeat-count=n> can limit the number of times this logic is applied

In addition, the following abbreviations are available to for simplified usage:

B<-kgb> or B<--keyword-group-blanks> is short for B<-kgbb=2 -kgba=2 kgbi>

B<-nkgb> or B<--nokeyword-group-blanks>, is short for B<-kgbb=1 -kgba=1 nkgbi>

Before describing the meaning of the parameters in detail let us look at an
example which is formatted with default parameter settings.

        print "Entering test 2\n";
        use Test;
        use Encode qw(from_to encode decode
          encode_utf8 decode_utf8
          find_encoding is_utf8);
        use charnames qw(greek);
        my @encodings     = grep( /iso-?8859/, Encode::encodings() );
        my @character_set = ( '0' .. '9', 'A' .. 'Z', 'a' .. 'z' );
        my @source        = qw(ascii iso8859-1 cp1250);
        my @destiny       = qw(cp1047 cp37 posix-bc);
        my @ebcdic_sets   = qw(cp1047 cp37 posix-bc);
        my $str           = join( '', map( chr($_), 0x20 .. 0x7E ) );
        return unless ($str);

using B<perltidy -kgb> gives:

        print "Entering test 2\n";
                                      <----------this blank controlled by -kgbb
        use Test;
        use Encode qw(from_to encode decode
          encode_utf8 decode_utf8
          find_encoding is_utf8);
        use charnames qw(greek);
                                      <---------this blank controlled by -kgbi
        my @encodings     = grep( /iso-?8859/, Encode::encodings() );
        my @character_set = ( '0' .. '9', 'A' .. 'Z', 'a' .. 'z' );
        my @source        = qw(ascii iso8859-1 cp1250);
        my @destiny       = qw(cp1047 cp37 posix-bc);
        my @ebcdic_sets   = qw(cp1047 cp37 posix-bc);
        my $str           = join( '', map( chr($_), 0x20 .. 0x7E ) );
                                      <----------this blank controlled by -kgba
        return unless ($str);

Blank lines have been introduced around the B<my> and B<use> sequences.  What
happened is that the default keyword list includes B<my> and B<use> but not
B<print> and B<return>.  So a continuous sequence of nine B<my> and B<use>
statements was located.  This number exceeds the default threshold of five, so
blanks were placed before and after the entire group.  Then, since there was
also a subsequence of six B<my> lines, a blank line was introduced to separate
them.

Finer control over blank placement can be achieved by using the individual
parameters rather than the B<-kgb> flag.  The individual controls are as follows.

B<-kgbl=s> or B<--keyword-group-blanks-list=s>, where B<s> is a quoted string,
defines the set of keywords which will be formed into groups.  The string is a
space separated list of keywords.  The default set is B<s="use require local
our my">, but any list of keywords may be used. Comment lines may also be included in a keyword group, even though they are not keywords.  To include ordinary block comments, include the symbol B<BC>. To include static block comments (which normally begin with '##'), include the symbol B<SBC>.

B<-kgbs=s> or B<--keyword-group-blanks-size=s>, where B<s> is a string
describing the number of consecutive keyword statements forming a group (Note:
statements separated by blank lines in the input file are considered
consecutive for purposes of this count).  If B<s> is an integer then it is the
minimum number required for a group.  A maximum value may also be given with
the format B<s=min.max>, where B<min> is the minimum number and B<max> is the
maximum number, and the min and max values are separated by one or more dots.
No groups will be found if the maximum is less than the minimum.  The maximum
is unlimited if not given.  The default is B<s=5>.  Some examples:

    s      min   max         number for group
    3      3     unlimited   3 or more
    1.1    1     1           1
    1..3   1     3           1 to 3
    1.0    1     0           (no match)

There is no really good default value for this parameter.  If it is set too
small, then an excessive number of blank lines may be generated.  However, some
users may prefer reducing the value somewhat below the default, perhaps to
B<s=3>.

B<-kgbb=n> or B<--keyword-group-blanks-before=n> specifies whether
a blank should appear before the first line of the group, as follows:

   n=0 => (delete) an existing blank line will be removed
   n=1 => (stable) no change to the input file is made  [DEFAULT]
   n=2 => (insert) a blank line is introduced if possible

B<-kgba=n> or B<--keyword-group-blanks-after=n> likewise specifies
whether a blank should appear after the last line of the group, using the same
scheme (0=delete, 1=stable, 2=insert).

B<-kgbi> or B<--keyword-group-blanks-inside> controls
the insertion of blank lines between the first and last statement of the entire
group.  If there is a continuous run of a single statement type with more than
the minimum threshold number (as specified with B<-kgbs=s>) then this
switch causes a blank line be inserted between this
subgroup and the others. In the example above this happened between the
B<use> and B<my> statements.

B<-kgbd> or B<--keyword-group-blanks-delete> controls the deletion of any
blank lines that exist in the the group when it is first scanned.  When
statements are initially scanned, any existing blank lines are included in the
collection.  Any such original blank lines will be deleted before any other
insertions are made when the parameter B<-kgbd> is set.  The default is not to
do this, B<-nkgbd>.

B<-kgbr=n> or B<--keyword-group-blanks-repeat-count=n> specifies B<n>, the
maximum number of times this logic will be applied to any file.  The special
value B<n=0> is the same as n=infinity which means it will be applied to an
entire script [Default].  A value B<n=1> could be used to make it apply just
one time for example.  This might be useful for adjusting just the B<use>
statements in the top part of a module for example.

B<-kgb> or B<--keyword-group-blanks> is an abbreviation equivalent to setting
B<-kgbb=1 -kgba=1 -kgbi>.  This turns on keyword group formatting with a set of
default values.

B<-nkgb> or B<--nokeyword-group-blanks> is equivalent to B<-kgbb=0 -kgba
nkgbi>.  This flag turns off keyword group blank lines and is the default
setting.

Here are a few notes about the functioning of this technique.

=over 4

=item *

These parameters are probably more useful as part of a major code reformatting
operation rather than as a routine formatting operation.

In particular, note that deleting old blank lines with B<-kgbd> is an
irreversible operation so it should be applied with care.  Existing blank lines
may be serving an important role in controlling vertical alignment.

=item *

Conflicts which arise among these B<kgb*> parameters and other blank line
controls are generally resolved by producing the maximum number of blank lines
implied by any parameter.

For example, if the flags B<--freeze-blank-lines>, or
B<--keep-old-blank-lines=2>, are set, then they have priority over any blank
line deletion implied by the B<-kgb> flags of this section, so no blank lines
will be deleted.

For another example, if a keyword group ends at a B<sub> and the flag B<kgba=0> requests no blank line there, but we also have B<--blank-lines-before-subs=2>, then two blank lines will still be introduced before the sub.

=item *

The introduction of blank lines does not occur if it would conflict with other
input controls or code validity. For example, a blank line will not be placed
within a here-doc or within a section of code marked with format skipping
comments.  And in general, a blank line will only be introduced at the end of a
group if the next statement is a line of code.

=item *

The count which is used to determine the group size is not the number of lines
but rather the total number of keywords which are found.  Individual statements
with a certain leading keyword may continue on multiple lines, but if any of
these lines is nested more than one level deep then that group will be ended.

=item *

The search for groups of lines with similar leading keywords is based on the
input source, not the final formatted source.  Consequently, if the source code
is badly formatted, it would be best to make a first formatting pass without
these options.

=back

=head2 Styles

A style refers to a convenient collection of existing parameters.

=over 4

=item B<-gnu>, B<--gnu-style>

B<-gnu> gives an approximation to the GNU Coding Standards (which do
not apply to perl) as they are sometimes implemented.  At present, this
style overrides the default style with the following parameters:

    -lp -bl -noll -pt=2 -bt=2 -sbt=2 -icp

To use this style with B<-xlp> instead of B<-lp> use B<-gnu -xlp>.

=item B<-pbp>, B<--perl-best-practices>

B<-pbp> is an abbreviation for the parameters in the book B<Perl Best Practices>
by Damian Conway:

    -l=78 -i=4 -ci=4 -st -se -vt=2 -cti=0 -pt=1 -bt=1 -sbt=1 -bbt=1 -nsfs -nolq
    -wbb="% + - * / x != == >= <= =~ !~ < > | & =
          **= += *= &= <<= &&= -= /= |= >>= ||= //= .= %= ^= x="

Please note that this parameter set includes -st and -se flags, which make
perltidy act as a filter on one file only.  These can be overridden by placing
B<-nst> and/or B<-nse> after the -pbp parameter.

Also note that the value of continuation indentation, -ci=4, is equal to the
value of the full indentation, -i=4.  It is recommended that the either (1) the
parameter B<-ci=2> be used instead, or (2) the flag B<-xci> be set.  This will
help show structure, particularly when there are ternary statements. The
following snippet illustrates these options.

    # perltidy -pbp
    $self->{_text} = (
         !$section        ? ''
        : $type eq 'item' ? "the $section entry"
        :                   "the section on $section"
        )
        . (
        $page
        ? ( $section ? ' in ' : '' ) . "the $page$page_ext manpage"
        : ' elsewhere in this document'
        );

    # perltidy -pbp -ci=2
    $self->{_text} = (
         !$section        ? ''
        : $type eq 'item' ? "the $section entry"
        :                   "the section on $section"
      )
      . (
        $page
        ? ( $section ? ' in ' : '' ) . "the $page$page_ext manpage"
        : ' elsewhere in this document'
      );

    # perltidy -pbp -xci
    $self->{_text} = (
         !$section        ? ''
        : $type eq 'item' ? "the $section entry"
        :                   "the section on $section"
        )
        . ( $page
            ? ( $section ? ' in ' : '' ) . "the $page$page_ext manpage"
            : ' elsewhere in this document'
        );

The B<-xci> flag was developed after the B<-pbp> parameters were published so you need
to include it separately.

=back

=head2 One-Line Blocks

A one-line block is a block of code where the contents within the curly braces
is short enough to fit on a single line. For example,

    if ( -e $file ) { print "'$file' exists\n" }

The alternative, a block which spans multiple lines, is said to be a broken
block.  With few exceptions, perltidy retains existing one-line blocks, if it
is possible within the line-length constraint, but it does not attempt to form
new ones.  In other words, perltidy will try to follow the input file regarding
broken and unbroken blocks.

The main exception to this rule is that perltidy will attempt to form new
one-line blocks following the keywords C<map>, C<eval>, and C<sort>, C<eval>,
because these code blocks are often small and most clearly displayed in a
single line. This behavior can be controlled with the flag
B<--one-line-block-exclusion-list> described below.

When the B<cuddled-else> style is used, the default treatment of one-line blocks
may interfere with the cuddled style.  In this case, the default behavior may 
be changed with the flag B<--cuddled-break-option=n> described elsehwere.

When an existing one-line block is longer than the maximum line length, and
must therefore be broken into multiple lines, perltidy checks for and adds any
optional terminating semicolon (unless the B<-nasc> option is used) if the
block is a code block.

=over 4

=item B<-olbxl=s>, B<--one-line-block-exclusion-list=s>

As noted above, perltidy will, by default, attempt to create new one-line
blocks for certain block types.  This flag allows the user to prevent this behavior for the block types listed in the string B<s>.  The list B<s> may
include any of the words C<sort>, C<map>, C<grep>, C<eval>,  or it may be C<*>
to indicate all of these.

So for example to prevent multi-line B<eval> blocks from becoming one-line
blocks, the command would be B<-olbxl='eval'>.  In this case, existing one-line B<eval> blocks will remain on one-line if possible, and existing multi-line
B<eval> blocks will remain multi-line blocks.

=item B<-olbn=n>, B<--one-line-block-nesting=n>

Nested one-line blocks are lines with code blocks which themselves contain code
blocks.  For example, the following line is a nested one-line block.

         foreach (@list) { if ($_ eq $asked_for) { last } ++$found }

The default behavior is to break such lines into multiple lines, but this
behavior can be controlled with this flag.  The values of n are:

  n=0 break nested one-line blocks into multiple lines [DEFAULT]
  n=1 stable: keep existing nested-one line blocks intact

For the above example, the default formatting (B<-olbn=0>) is

    foreach (@list) {
        if ( $_ eq $asked_for ) { last }
        ++$found;
    }

If the parameter B<-olbn=1> is given, then the line will be left intact if it
is a single line in the source, or it will be broken into multiple lines if it
is broken in multiple lines in the source.

=item B<-olbs=n>, B<--one-line-block-semicolons=n>

This flag controls the placement of semicolons at the end of one-line blocks.
Semicolons are optional before a closing block brace, and frequently they are
omitted at the end of a one-line block containing just a single statement.
By default, perltidy follows the input file regarding these semicolons,
but this behavior can be controlled by this flag.  The values of n are:

  n=0 remove terminal semicolons in one-line blocks having a single statement
  n=1 stable; keep input file placement of terminal semicolons [DEFAULT ]
  n=2 add terminal semicolons in all one-line blocks

Note that the B<n=2> option has no effect if adding semicolons is prohibited
with the B<-nasc> flag.  Also not that while B<n=2> adds missing semicolons to
all one-line blocks, regardless of complexity, the B<n=0> option only removes
ending semicolons which terminate one-line blocks containing just one
semicolon.  So these two options are not exact inverses.

=item B<Forming new one-line blocks>

Sometimes it might be desirable to convert a script to have one-line blocks
whenever possible.  Although there is currently no flag for this, a simple
workaround is to execute perltidy twice, once with the flag B<-noadd-newlines>
and then once again with normal parameters, like this:

     cat infile | perltidy -nanl | perltidy >outfile

When executed on this snippet

    if ( $? == -1 ) {
        die "failed to execute: $!\n";
    }
    if ( $? == -1 ) {
        print "Had enough.\n";
        die "failed to execute: $!\n";
    }

the result is

    if ( $? == -1 ) { die "failed to execute: $!\n"; }
    if ( $? == -1 ) {
        print "Had enough.\n";
        die "failed to execute: $!\n";
    }

This shows that blocks with a single statement become one-line blocks.

=item B<Breaking existing one-line blocks>

There is no automatic way to break existing long one-line blocks into multiple
lines, but this can be accomplished by processing a script, or section of a
script, with a short value of the parameter B<maximum-line-length=n>.  Then,
when the script is reformatted again with the normal parameters, the blocks
which were broken will remain broken (with the exceptions noted above).

Another trick for doing this for certain block types is to format one time with
the B<-cuddled-else> flag and B<--cuddled-break-option=2>. Then format again
with the normal parameters.  This will break any one-line blocks which are
involved in a cuddled-else style.

=back

=head2 Controlling Vertical Alignment

Vertical alignment refers to lining up certain symbols in a list of consecutive
similar lines to improve readability.  For example, the "fat commas" are
aligned in the following statement:

        $data = $pkg->new(
            PeerAddr => join( ".", @port[ 0 .. 3 ] ),
            PeerPort => $port[4] * 256 + $port[5],
            Proto    => 'tcp'
        );

Vertical alignment can be completely turned off using the B<-novalign> flag
mentioned below.  However, vertical alignment can be forced to
stop and restart by selectively introducing blank lines.  For example, a blank
has been inserted in the following code to keep somewhat similar things
aligned.

    %option_range = (
        'format'             => [ 'tidy', 'html', 'user' ],
        'output-line-ending' => [ 'dos',  'win',  'mac', 'unix' ],
        'character-encoding' => [ 'none', 'utf8' ],

        'block-brace-tightness'    => [ 0, 2 ],
        'brace-tightness'          => [ 0, 2 ],
        'paren-tightness'          => [ 0, 2 ],
        'square-bracket-tightness' => [ 0, 2 ],
    );

Vertical alignment is implemented by locally increasing an existing blank space
to produce alignment with an adjacent line.  It cannot occur if there is no
blank space to increase.  So if a particular space is removed by one of the
existing controls then vertical alignment cannot occur. Likewise, if a space is
added with one of the controls, then vertical alignment might occur.

For example,

        # perltidy -nwls='=>'
        $data = $pkg->new(
            PeerAddr=> join( ".", @port[ 0 .. 3 ] ),
            PeerPort=> $port[4] * 256 + $port[5],
            Proto=> 'tcp'
        );

=over 4

=item B<Completely turning off vertical alignment with -novalign>

The default is to use vertical alignment, but vertical alignment can be
completely turned of with the B<-novalign> flag.

A lower level of control of vertical alignment is possible with three parameters
B<-vc>, B<-vsc>, and B<-vbc>. These independently control alignment
of code, side comments and block comments.  They are described in the
next section.

The parameter B<-valign> is in fact an alias for B<-vc -vsc -vbc>, and its
negative B<-novalign> is an alias for B<-nvc -nvsc -nvbc>.

=item B<Controlling code alignment with --valign-code or -vc>

The B<-vc> flag enables alignment of code symbols such as B<=>.  The default is B<-vc>.
For detailed control of which symbols to align, see the B<-valign-exclude-list> parameter
below.

=item B<Controlling side comment alignment with --valign-side-comments or -vsc>

The B<-vsc> flag enables alignment of side comments and is enabled by default.  If side
comment alignment is disabled with B<-nvsc> they will appear at a fixed space from the
preceding code token.  The default is B<-vsc>

=item B<Controlling block comment alignment with --valign-block-comments or -vbc>

When B<-vbc> is enabled, block comments can become aligned for example if one
comment of a consecutive sequence of comments becomes outdented due a length in
excess of the maximum line length.  If this occurs, the entire group of
comments will remain aligned and be outdented by the same amount.  This coordinated
alignment will not occur if B<-nvbc> is set.  The default is B<-vbc>.

=item B<Finer alignment control with --valign-exclusion-list=s or -vxl=s and --valign-inclusion-list=s or -vil=s>

More detailed control of alignment types is available with these two
parameters.  Most of the vertical alignments in typical programs occur at one
of the tokens ',', '=', and '=>', but many other alignments are possible and are given in the following list:

  = **= += *= &= <<= &&= -= /= |= >>= ||= //= .= %= ^= x=
  { ( ? : , ; => && || ~~ !~~ =~ !~ // <=> -> q
  if unless and or err for foreach while until

These alignment types correspond to perl symbols, operators and keywords except
for 'q', which refers to the special case of alignment in a 'use' statement of
qw quotes and empty parens. 

They are all enabled by default, but they can be selectively disabled by including one or more of these tokens in the space-separated list B<valign-exclusion-list=s>.
For example, the following would prevent alignment at B<=> and B<if>:

  --valign-exclusion-list='= if'

If it is simpler to specify only the token types which are to be aligned, then
include the types which are to be aligned in the list of B<--valign-inclusion-list>.
In that case you may leave the B<valign-exclusion-list> undefined, or use the special symbol B<*> for the exclusion list.
For example, the following parameters enable alignment only at commas and 'fat commas':

  --valign-inclusion-list=', =>'
  --valign-exclusion-list='*'     ( this is optional and may be omitted )

These parameter lists should consist of space-separated tokens from the above
list of possible alignment tokens, or a '*'.  If an unrecognized token
appears, it is simply ignored. And if a specific token is entered in both lists by
mistake then the exclusion list has priority.

The default values of these parameters enable all alignments and are equivalent to

  --valign-exclusion-list=' '
  --valign-inclusion-list='*'

To illustrate, consider the following snippet with default formatting

    # perltidy
    $co_description = ($color) ? 'bold cyan'  : '';           # description
    $co_prompt      = ($color) ? 'bold green' : '';           # prompt
    $co_unused      = ($color) ? 'on_green'   : 'reverse';    # unused

To exclude all alignments except the equals (i.e., include only equals) we could use:

    # perltidy -vil='='
    $co_description = ($color) ? 'bold cyan' : '';          # description
    $co_prompt      = ($color) ? 'bold green' : '';         # prompt
    $co_unused      = ($color) ? 'on_green' : 'reverse';    # unused

To exclude only the equals we could use:

    # perltidy -vxl='='
    $co_description = ($color) ? 'bold cyan' : '';     # description
    $co_prompt = ($color) ? 'bold green' : '';         # prompt
    $co_unused = ($color) ? 'on_green' : 'reverse';    # unused

Notice in this last example that although only the equals alignment was
excluded, the ternary alignments were also lost.  This happens because the
vertical aligner sweeps from left-to-right and usually stops if an important
alignment cannot be made for some reason.

But also notice that side comments remain aligned because their alignment is
controlled separately with the parameter B<--valign-side_comments> described above.

=item B<Aligning postfix unless and if with --valign-if-unless or -viu>

By default, postfix B<if> terms align and postfix B<unless> terms align,
but separately.  For example,

    # perltidy [DEFAULT]
    print "Tried to add: @ResolveRPM\n" if ( @ResolveRPM and !$Quiet );
    print "Would need: @DepList\n"      if ( @DepList    and !$Quiet );
    print "RPM Output:\n"                 unless $Quiet;
    print join( "\n", @RPMOutput ) . "\n" unless $Quiet;

The B<-viu> flag causes a postfix B<unless> to be treated as if it were a
postfix B<if> for purposes of alignment.  Thus

    # perltidy -viu
    print "Tried to add: @ResolveRPM\n"   if ( @ResolveRPM and !$Quiet );
    print "Would need: @DepList\n"        if ( @DepList    and !$Quiet );
    print "RPM Output:\n"                 unless $Quiet;
    print join( "\n", @RPMOutput ) . "\n" unless $Quiet;

=back

=head2 Extended Syntax

This section describes some parameters for dealing with extended syntax.

For another method of handling extended syntax see the section L<"Skipping Selected Sections of Code">.

Also note that the module F<Perl::Tidy> supplies a pre-filter and post-filter capability. This requires calling the module from a separate program rather than through the binary F<perltidy>. 

=over 4

=item B<-xs>,   B<--extended-syntax>

This flag allows perltidy to handle certain common extensions
to the standard syntax without complaint.

For example, without this flag a structure such as the following would generate
a syntax error:

    Method deposit( Num $amount) {
        $self->balance( $self->balance + $amount );
    }

This flag is enabled by default but it can be deactivated with B<-nxs>.
Probably the only reason to deactivate this flag is to generate more diagnostic
messages when debugging a script.

=item B<-sal=s>,   B<--sub-alias-list=s>

This flag causes one or more words to be treated the same as if they were the keyword B<sub>.  The string B<s> contains one or more alias words, separated by spaces or commas.

For example,

        perltidy -sal='method fun _sub M4'

will cause the perltidy to treat the words 'method', 'fun', '_sub' and 'M4' the same as if they were 'sub'.  Note that if the alias words are separated by spaces then the string of words should be placed in quotes.

Note that several other parameters accept a list of keywords, including 'sub' (see L<"Specifying Block Types">).
You do not need to include any sub aliases in these lists. Just include keyword 'sub' if you wish, and all aliases are automatically included.

=item B<-gal=s>,   B<--grep-alias-list=s>

This flag allows a code block following an external 'list operator' function to be formatted as if it followed one of the built-in keywords B<grep>,  B<map> or B<sort>.  The string B<s> contains the names of one or more such list operators, separated by spaces or commas.

By 'list operator' is meant a function which is invoked in the form

      word {BLOCK} @list

Perltidy tries to keep code blocks for these functions intact, since they are usually short, and does not automatically break after the closing brace since a list may follow. It also does some special handling of continuation indentation.

For example, the code block arguments to functions 'My_grep' and 'My_map' can be given formatting like 'grep' with

        perltidy -gal='My_grep My_map'

By default, the following list operators in List::Util are automatically included:

      all any first none notall reduce reductions

Any operators specified with B<--grep-alias-list> are added to this list.
The next parameter can be used to remove words from this default list.

=item B<-gaxl=s>,   B<--grep-alias-exclusion-list=s>

The B<-gaxl=s> flag provides a method for removing any of the default list operators given above
by listing them in the string B<s>.  To remove all of the default operators use B<-gaxl='*'>.

=item B<-uf=s>,   B<--use-feature=s>

This flag tells perltidy to allow the syntax associated a pragma in string
B<s>. Currently only the recognized values for the string are B<s='class'> or
string B<s=' '>.  The default is B<--use-feature='class'>.  This enables
perltidy to recognized the special words B<class>, B<method>, B<field>, and
B<ADJUST>.  If this causes a conflict with other uses of these words, the
default can be turned off with B<--use-feature=' '>.

=back

=head2 Other Controls

=over 4

=item B<Deleting selected text>

Perltidy can selectively delete comments and/or pod documentation.  The
command B<-dac> or  B<--delete-all-comments> will delete all comments
B<and> all pod documentation, leaving just code and any leading system
control lines.

The command B<-dp> or B<--delete-pod> will remove all pod documentation
(but not comments).

Two commands which remove comments (but not pod) are: B<-dbc> or
B<--delete-block-comments> and B<-dsc> or  B<--delete-side-comments>.
(Hanging side comments will be deleted with side comments here.)

When side comments are deleted, any special control side comments for
non-indenting braces will be retained unless they are deactivated with
a B<-nnib> flag.

The negatives of these commands also work, and are the defaults.  When
block comments are deleted, any leading 'hash-bang' will be retained.
Also, if the B<-x> flag is used, any system commands before a leading
hash-bang will be retained (even if they are in the form of comments).

=item B<Writing selected text to a file>

When perltidy writes a formatted text file, it has the ability to also
send selected text to a file with a F<.TEE> extension.  This text can
include comments and pod documentation.

The command B<-tac> or  B<--tee-all-comments> will write all comments
B<and> all pod documentation.

The command B<-tp> or B<--tee-pod> will write all pod documentation (but
not comments).

The commands which write comments (but not pod) are: B<-tbc> or
B<--tee-block-comments> and B<-tsc> or  B<--tee-side-comments>.
(Hanging side comments will be written with side comments here.)

The negatives of these commands also work, and are the defaults.

=item B<Using a F<.perltidyrc> command file>

If you use perltidy frequently, you probably won't be happy until you
create a F<.perltidyrc> file to avoid typing commonly-used parameters.
Perltidy will first look in your current directory for a command file
named F<.perltidyrc>.  If it does not find one, it will continue looking
for one in other standard locations.

These other locations are system-dependent, and may be displayed with
the command C<perltidy -dpro>.  Under Unix systems, it will first look
for an environment variable B<PERLTIDY>.  Then it will look for a
F<.perltidyrc> file in the home directory, and then for a system-wide
file F</usr/local/etc/perltidyrc>, and then it will look for
F</etc/perltidyrc>.  Note that these last two system-wide files do not
have a leading dot.  Further system-dependent information will be found
in the INSTALL file distributed with perltidy.

Under Windows, perltidy will also search for a configuration file named F<perltidy.ini> since Windows does not allow files with a leading period (.).
Use C<perltidy -dpro> to see the possible locations for your system.
An example might be F<C:\Documents and Settings\All Users\perltidy.ini>.

Another option is the use of the PERLTIDY environment variable.
The method for setting environment variables depends upon the version of
Windows that you are using.

Under Windows NT / 2000 / XP the PERLTIDY environment variable can be placed in
either the user section or the system section.  The later makes the
configuration file common to all users on the machine.  Be sure to enter the
full path of the configuration file in the value of the environment variable.
Ex.  PERLTIDY=C:\Documents and Settings\perltidy.ini

The configuration file is free format, and simply a list of parameters, just as
they would be entered on a command line.  Any number of lines may be used, with
any number of parameters per line, although it may be easiest to read with one
parameter per line.  Comment text begins with a #, and there must
also be a space before the # for side comments.  It is a good idea to
put complex parameters in either single or double quotes.

Here is an example of a F<.perltidyrc> file:

  # This is a simple of a .perltidyrc configuration file
  # This implements a highly spaced style
  -se    # errors to standard error output
  -w     # show all warnings
  -bl	 # braces on new lines
  -pt=0  # parens not tight at all
  -bt=0  # braces not tight
  -sbt=0 # square brackets not tight

The parameters in the F<.perltidyrc> file are installed first, so any
parameters given on the command line will have priority over them.

To avoid confusion, perltidy ignores any command in the .perltidyrc
file which would cause some kind of dump and an exit.  These are:

 -h -v -ddf -dln -dop -dsn -dtt -dwls -dwrs -ss

There are several options may be helpful in debugging a F<.perltidyrc>
file:

=over 4

=item *

A very helpful command is B<--dump-profile> or B<-dpro>.  It writes a
list of all configuration filenames tested to standard output, and
if a file is found, it dumps the content to standard output before
exiting.  So, to find out where perltidy looks for its configuration
files, and which one if any it selects, just enter

  perltidy -dpro

=item *

It may be simplest to develop and test configuration files with
alternative names, and invoke them with B<-pro=filename> on the command
line.  Then rename the desired file to F<.perltidyrc> when finished.

=item *

The parameters in the F<.perltidyrc> file can be switched off with
the B<-npro> option.

=item *

The commands B<--dump-options>, B<--dump-defaults>, B<--dump-long-names>,
and B<--dump-short-names>, all described below, may all be helpful.

=back

=item B<Creating a new abbreviation>

A special notation is available for use in a F<.perltidyrc> file
for creating an abbreviation for a group
of options.  This can be used to create a
shorthand for one or more styles which are frequently, but not always,
used.  The notation is to group the options within curly braces which
are preceded by the name of the alias (without leading dashes), like this:

	newword {
	-opt1
	-opt2
	}

where B<newword> is the abbreviation, and B<opt1>, etc, are existing parameters
I<or other abbreviations>.  The main syntax requirement is that the new
abbreviation along with its opening curly brace must begin on a new line.
Space before and after the curly braces is optional.

For a specific example, the following line

        oneliner { --maximum-line-length=0 --noadd-newlines --noadd-terminal-newline}

or equivalently with abbreviations

	oneliner { -l=0 -nanl -natnl }

could be placed in a F<.perltidyrc> file to temporarily override the maximum
line length with a large value, to temporarily prevent new line breaks from
being added, and to prevent an extra newline character from being added the
file.  All other settings in the F<.perltidyrc> file still apply.  Thus it
provides a way to format a long 'one liner' when perltidy is invoked with

	perltidy --oneliner ...

(Either C<-oneliner> or C<--oneliner> may be used).

=item Skipping leading non-perl commands with B<-x> or B<--look-for-hash-bang>

If your script has leading lines of system commands or other text which
are not valid perl code, and which are separated from the start of the
perl code by a "hash-bang" line, ( a line of the form C<#!...perl> ),
you must use the B<-x> flag to tell perltidy not to parse and format any
lines before the "hash-bang" line.  This option also invokes perl with a
-x flag when checking the syntax.  This option was originally added to
allow perltidy to parse interactive VMS scripts, but it should be used
for any script which is normally invoked with C<perl -x>.

Please note: do not use this flag unless you are sure your script needs it.
Parsing errors can occur if it does not have a hash-bang, or, for example, if
the actual first hash-bang is in a here-doc. In that case a parsing error will
occur because the tokenization will begin in the middle of the here-doc.

=item B<Making a file unreadable>

The goal of perltidy is to improve the readability of files, but there
are two commands which have the opposite effect, B<--mangle> and
B<--extrude>.  They are actually
merely aliases for combinations of other parameters.  Both of these
strip all possible whitespace, but leave comments and pod documents,
so that they are essentially reversible.  The
difference between these is that B<--mangle> puts the fewest possible
line breaks in a script while B<--extrude> puts the maximum possible.
Note that these options do not provided any meaningful obfuscation, because
perltidy can be used to reformat the files.  They were originally
developed to help test the tokenization logic of perltidy, but they
have other uses.
One use for B<--mangle> is the following:

  perltidy --mangle myfile.pl -st | perltidy -o myfile.pl.new

This will form the maximum possible number of one-line blocks (see next
section), and can sometimes help clean up a badly formatted script.

A similar technique can be used with B<--extrude> instead of B<--mangle>
to make the minimum number of one-line blocks.

Another use for B<--mangle> is to combine it with B<-dac> to reduce
the file size of a perl script.

=item B<Debugging>

The following flags are available for debugging:

B<--dump-cuddled-block-list> or B<-dcbl> will dump to standard output the
internal hash of cuddled block types created by a B<-cuddled-block-list> input
string.

B<--dump-defaults> or B<-ddf> will write the default option set to standard output and quit

B<--dump-profile> or B<-dpro>  will write the name of the current
configuration file and its contents to standard output and quit.

B<--dump-options> or B<-dop>  will write current option set to standard
output and quit.

B<--dump-long-names> or B<-dln>  will write all command line long names (passed
to Get_options) to standard output and quit.

B<--dump-short-names>  or B<-dsn> will write all command line short names
to standard output and quit.

B<--dump-token-types> or B<-dtt>  will write a list of all token types
to standard output and quit.

B<--dump-want-left-space> or B<-dwls>  will write the hash %want_left_space
to standard output and quit.  See the section on controlling whitespace
around tokens.

B<--dump-want-right-space> or B<-dwrs>  will write the hash %want_right_space
to standard output and quit.  See the section on controlling whitespace
around tokens.

B<--no-memoize> or B<-nmem>  will turn of memoizing.
Memoization can reduce run time when running perltidy repeatedly in a
single process.  It is on by default but can be deactivated for
testing with B<-nmem>.

B<--no-timestamp> or B<-nts> will eliminate any time stamps in output files to prevent
differences in dates from causing test installation scripts to fail. There are just
a couple of places where timestamps normally occur. One is in the headers of
html files, and another is when the B<-cscw> option is selected. The default is
to allow timestamps (B<--timestamp> or B<-ts>).

B<--file-size-order> or B<-fso> will cause files to be processed in order of
increasing size, when multiple files are being processed.  This is useful
during program development, when large numbers of files with varying sizes are
processed, because it can reduce virtual memory usage.

B<--maximum-file-size-mb=n> or B<-maxfs=n> specifies the maximum file size in
megabytes that perltidy will attempt to format. This parameter is provided to
avoid causing system problems by accidentally attempting to format an extremely
large data file. Most perl scripts are less than about 2 MB in size. The
integer B<n> has a default value of 10, so perltidy will skip formatting files
which have a size greater than 10 MB.  The command to increase the limit to 20
MB for example would be

  perltidy -maxfs=20

This only applies to files specified by filename on the command line.

B<--maximum-level-errors=n> or B<-maxle=n> specifies the maximum number of
indentation level errors are allowed before perltidy skips formatting and just
outputs a file verbatim.  The default is B<n=1>.  This means that if the final
indentation of a script differs from the starting indentation by more than 1
levels, the file will be output verbatim.  To avoid formatting if there are any
indentation level errors use -maxle=0. To skip this check you can either set n
equal to a large number, such as B<n=100>, or set B<n=-1>.

For example, the following script has level error of 3 and will be output verbatim

    Input and default output:
    {{{


    perltidy -maxle=100
    {
        {
            {

B<--maximum-unexpected-errors=n> or B<-maxue=n> specifies the maximum number of
unexpected tokenization errors are allowed before formatting is skipped and a
script is output verbatim.  The intention is to avoid accidentally formatting
a non-perl script, such as an html file for example.  This check can be turned
off by setting B<n=0>.

A recommended value is B<n=3>.  However, the default is B<n=0> (skip this check)
to avoid causing problems with scripts which have extended syntaxes.

B<-DEBUG>  will write a file with extension F<.DEBUG> for each input file
showing the tokenization of all lines of code.

=item B<Making a table of information on code blocks>

A table listing information about the blocks of code in a file can be made with
B<--dump-block-summary>, or B<-dbs>.  This causes perltidy to read and parse
the file, write a table of comma-separated values for selected code blocks to
the standard output, and then exit.  This parameter must be on the command
line, not in a F<.perlticyrc> file, and it requires a single file name on the
command line.  For example

   perltidy -dbs somefile.pl >blocks.csv

produces an output file F<blocks.csv> whose lines hold these
parameters:

    filename     - the name of the file
    line         - the line number of the opening brace of this block
    line_count   - the number of lines between opening and closing braces
    code_lines   - the number of lines excluding blanks, comments, and pod
    type         - the block type (sub, for, foreach, ...)
    name         - the block name if applicable (sub name, label, asub name)
    depth        - the nesting depth of the opening block brace
    max_change   - the change in depth to the most deeply nested code block
    block_count  - the total number of code blocks nested in this block
    mccabe_count - the McCabe complexity measure of this code block

This feature was developed to help identify complex sections of code as an aid
in refactoring.  The McCabe complexity measure follows the definition used by
Perl::Critic.  By default the table contains these values for subroutines, but
the user may request them for any or all blocks of code or packages.  For
blocks which are loops nested within loops, a postfix '+' to the C<type> is
added to indicate possible code complexity.  Although the table does not
otherwise indicate which blocks are nested in other blocks, this can be
determined by computing and comparing the block ending line numbers.

By default the table lists subroutines with more than 20 C<code_lines>, but
this can be changed with the following two parameters:

B<--dump-block-minimum-lines=n>, or B<-dbl=n>, where B<n> is the minimum
number of C<code_lines> to be included. The default is B<-n=20>.  Note that
C<code_lines> is the number of lines excluding and comments, blanks and pod.

B<--dump-block-types=s>, or B<-dbt=s>, where string B<s> is a list of block
types to be included.  The type of a block is either the name of the perl
builtin keyword for that block (such as B<sub if elsif else for foreach ..>) or
the word immediately before the opening brace.  In addition, there are
a few symbols for special block types, as follows:

   if elsif else for foreach ... any keyword introducing a block
   sub  - any sub or anynomous sub
   asub - any anonymous sub
   *    - any block except nameless blocks
   +    - any nested inner block loop
   package - any package or class
   closure - any nameless block

In addition, specific block loop types which are nested in other loops can be
selected by adding a B<+> after the block name. (Nested loops are sometimes
good candidates for restructuring).

The default is B<-dbt='sub'>.

In the following examples a table C<block.csv> is created for a file
C<somefile.pl>:

=over 4

=item *
This selects both C<subs> and C<packages> which have 20 or more lines of code.
This can be useful in code which contains multiple packages.

    perltidy -dbs -dbt='sub package' somefile.pl >blocks.csv

=item *
This selects block types C<sub for foreach while> with 10 or more code lines.

    perltidy -dbs -dbl=10 -dbt='sub for foreach while' somefile.pl >blocks.csv

=item *
This selects blocks with 2 or more code lines which are type C<sub> or which
are inner loops.

    perltidy -dbs -dbl=2 -dbt='sub +' somefile.pl >blocks.csv

=item *
This selects every block and package.

    perltidy -dbs -dbl=1 -dbt='* closure' somefile.pl >blocks.csv

=back


=item B<Working with MakeMaker, AutoLoader and SelfLoader>

The first $VERSION line of a file which might be eval'd by MakeMaker
is passed through unchanged except for indentation.
Use B<--nopass-version-line>, or B<-npvl>, to deactivate this feature.

If the AutoLoader module is used, perltidy will continue formatting
code after seeing an __END__ line.
Use B<--nolook-for-autoloader>, or B<-nlal>, to deactivate this feature.

Likewise, if the SelfLoader module is used, perltidy will continue formatting
code after seeing a __DATA__ line.
Use B<--nolook-for-selfloader>, or B<-nlsl>, to deactivate this feature.

=item B<Working around problems with older version of Perl>

Perltidy contains a number of rules which help avoid known subtleties
and problems with older versions of perl, and these rules always
take priority over whatever formatting flags have been set.  For example,
perltidy will usually avoid starting a new line with a bareword, because
this might cause problems if C<use strict> is active.

There is no way to override these rules.

=back

=head1 HTML OPTIONS

=over 4

=item  The B<-html> master switch

The flag B<-html> causes perltidy to write an html file with extension
F<.html>.  So, for example, the following command

	perltidy -html somefile.pl

will produce a syntax-colored html file named F<somefile.pl.html>
which may be viewed with a browser.

B<Please Note>: In this case, perltidy does not do any formatting to the
input file, and it does not write a formatted file with extension
F<.tdy>.  This means that two perltidy runs are required to create a
fully reformatted, html copy of a script.

=item  The B<-pre> flag for code snippets

When the B<-pre> flag is given, only the pre-formatted section, within
the <PRE> and </PRE> tags, will be output.  This simplifies inclusion
of the output in other files.  The default is to output a complete
web page.

=item  The B<-nnn> flag for line numbering

When the B<-nnn> flag is given, the output lines will be numbered.

=item  The B<-toc>, or B<--html-table-of-contents> flag

By default, a table of contents to packages and subroutines will be
written at the start of html output.  Use B<-ntoc> to prevent this.
This might be useful, for example, for a pod document which contains a
number of unrelated code snippets.  This flag only influences the code
table of contents; it has no effect on any table of contents produced by
pod2html (see next item).

=item  The B<-pod>, or B<--pod2html> flag

There are two options for formatting pod documentation.  The default is
to pass the pod through the Pod::Html module (which forms the basis of
the pod2html utility).  Any code sections are formatted by perltidy, and
the results then merged.  Note: perltidy creates a temporary file when
Pod::Html is used; see L<"FILES">.  Also, Pod::Html creates temporary
files for its cache.

NOTE: Perltidy counts the number of C<=cut> lines, and either moves the
pod text to the top of the html file if there is one C<=cut>, or leaves
the pod text in its original order (interleaved with code) otherwise.

Most of the flags accepted by pod2html may be included in the perltidy
command line, and they will be passed to pod2html.  In some cases,
the flags have a prefix C<pod> to emphasize that they are for the
pod2html, and this prefix will be removed before they are passed to
pod2html.  The flags which have the additional C<pod> prefix are:

   --[no]podheader --[no]podindex --[no]podrecurse --[no]podquiet
   --[no]podverbose --podflush

The flags which are unchanged from their use in pod2html are:

   --backlink=s --cachedir=s --htmlroot=s --libpods=s --title=s
   --podpath=s --podroot=s

where 's' is an appropriate character string.  Not all of these flags are
available in older versions of Pod::Html.  See your Pod::Html documentation for
more information.

The alternative, indicated with B<-npod>, is not to use Pod::Html, but
rather to format pod text in italics (or whatever the stylesheet
indicates), without special html markup.  This is useful, for example,
if pod is being used as an alternative way to write comments.

=item  The B<-frm>, or B<--frames> flag

By default, a single html output file is produced.  This can be changed
with the B<-frm> option, which creates a frame holding a table of
contents in the left panel and the source code in the right side. This
simplifies code browsing.  Assume, for example, that the input file is
F<MyModule.pm>.  Then, for default file extension choices, these three
files will be created:

 MyModule.pm.html      - the frame
 MyModule.pm.toc.html  - the table of contents
 MyModule.pm.src.html  - the formatted source code

Obviously this file naming scheme requires that output be directed to a real
file (as opposed to, say, standard output).  If this is not the
case, or if the file extension is unknown, the B<-frm> option will be
ignored.

=item  The B<-text=s>, or B<--html-toc-extension> flag

Use this flag to specify the extra file extension of the table of contents file
when html frames are used.  The default is "toc".
See L<"Specifying File Extensions">.

=item  The B<-sext=s>, or B<--html-src-extension> flag

Use this flag to specify the extra file extension of the content file when html
frames are used.  The default is "src".
See L<"Specifying File Extensions">.

=item  The B<-hent>, or B<--html-entities> flag

This flag controls the use of Html::Entities for html formatting.  By
default, the module Html::Entities is used to encode special symbols.
This may not be the right thing for some browser/language
combinations.  Use --nohtml-entities or -nhent to prevent this.

=item  B<Style Sheets>

Style sheets make it very convenient to control and adjust the
appearance of html pages.  The default behavior is to write a page of
html with an embedded style sheet.

An alternative to an embedded style sheet is to create a page with a
link to an external style sheet.  This is indicated with the
B<-css=filename>,  where the external style sheet is F<filename>.  The
external style sheet F<filename> will be created if and only if it does
not exist.  This option is useful for controlling multiple pages from a
single style sheet.

To cause perltidy to write a style sheet to standard output and exit,
use the B<-ss>, or B<--stylesheet>, flag.  This is useful if the style
sheet could not be written for some reason, such as if the B<-pre> flag
was used.  Thus, for example,

  perltidy -html -ss >mystyle.css

will write a style sheet with the default properties to file
F<mystyle.css>.

The use of style sheets is encouraged, but a web page without a style
sheets can be created with the flag B<-nss>.  Use this option if you
must to be sure that older browsers (roughly speaking, versions prior to
4.0 of Netscape Navigator and Internet Explorer) can display the
syntax-coloring of the html files.

=item  B<Controlling HTML properties>

Note: It is usually more convenient to accept the default properties
and then edit the stylesheet which is produced.  However, this section
shows how to control the properties with flags to perltidy.

Syntax colors may be changed from their default values by flags of the either
the long form, B<-html-color-xxxxxx=n>, or more conveniently the short form,
B<-hcx=n>, where B<xxxxxx> is one of the following words, and B<x> is the
corresponding abbreviation:

      Token Type             xxxxxx           x
      ----------             --------         --
      comment                comment          c
      number                 numeric          n
      identifier             identifier       i
      bareword, function     bareword         w
      keyword                keyword          k
      quite, pattern         quote            q
      here doc text          here-doc-text    h
      here doc target        here-doc-target  hh
      punctuation            punctuation      pu
      parentheses            paren            p
      structural braces      structure        s
      semicolon              semicolon        sc
      colon                  colon            co
      comma                  comma            cm
      label                  label            j
      sub definition name    subroutine       m
      pod text               pod-text         pd

A default set of colors has been defined, but they may be changed by providing
values to any of the following parameters, where B<n> is either a 6 digit
hex RGB color value or an ascii name for a color, such as 'red'.

To illustrate, the following command will produce an html
file F<somefile.pl.html> with "aqua" keywords:

	perltidy -html -hck=00ffff somefile.pl

and this should be equivalent for most browsers:

	perltidy -html -hck=aqua somefile.pl

Perltidy merely writes any non-hex names that it sees in the html file.
The following 16 color names are defined in the HTML 3.2 standard:

	black   => 000000,
	silver  => c0c0c0,
	gray    => 808080,
	white   => ffffff,
	maroon  => 800000,
	red     => ff0000,
	purple  => 800080,
	fuchsia => ff00ff,
	green   => 008000,
	lime    => 00ff00,
	olive   => 808000,
	yellow  => ffff00
	navy    => 000080,
	blue    => 0000ff,
	teal    => 008080,
	aqua    => 00ffff,

Many more names are supported in specific browsers, but it is safest
to use the hex codes for other colors.  Helpful color tables can be
located with an internet search for "HTML color tables".

Besides color, two other character attributes may be set: bold, and italics.
To set a token type to use bold, use the flag
B<--html-bold-xxxxxx> or B<-hbx>, where B<xxxxxx> or B<x> are the long
or short names from the above table.  Conversely, to set a token type to
NOT use bold, use B<--nohtml-bold-xxxxxx> or B<-nhbx>.

Likewise, to set a token type to use an italic font, use the flag
B<--html-italic-xxxxxx> or B<-hix>, where again B<xxxxxx> or B<x> are the
long or short names from the above table.  And to set a token type to
NOT use italics, use B<--nohtml-italic-xxxxxx> or B<-nhix>.

For example, to use bold braces and lime color, non-bold, italics keywords the
following command would be used:

	perltidy -html -hbs -hck=00FF00 -nhbk -hik somefile.pl

The background color can be specified with B<--html-color-background=n>,
or B<-hcbg=n> for short, where n is a 6 character hex RGB value.  The
default color of text is the value given to B<punctuation>, which is
black as a default.

Here are some notes and hints:

1. If you find a preferred set of these parameters, you may want
to create a F<.perltidyrc> file containing them.  See the perltidy man
page for an explanation.

2. Rather than specifying values for these parameters, it is probably
easier to accept the defaults and then edit a style sheet.  The style
sheet contains comments which should make this easy.

3. The syntax-colored html files can be very large, so it may be best to
split large files into smaller pieces to improve download times.

=back

=head1 SOME COMMON INPUT CONVENTIONS

=head2 Specifying Block Types

Several parameters which refer to code block types may be customized by also
specifying an associated list of block types.  The type of a block is the name
of the keyword which introduces that block, such as B<if>, B<else>, or B<sub>.
An exception is a labeled block, which has no keyword, and should be specified
with just a colon.  To specify all blocks use B<'*'>.

The keyword B<sub> indicates a named sub.  For anonymous subs, use the special
keyword B<asub>.

For example, the following parameter specifies C<sub>, labels, C<BEGIN>, and
C<END> blocks:

   -cscl="sub : BEGIN END"

(the meaning of the -cscl parameter is described above.)  Note that
quotes are required around the list of block types because of the
spaces.  For another example, the following list specifies all block types
for vertical tightness:

   -bbvtl='*'

=head2 Specifying File Extensions

Several parameters allow default file extensions to be overridden.  For
example, a backup file extension may be specified with B<-bext=ext>,
where B<ext> is some new extension.  In order to provides the user some
flexibility, the following convention is used in all cases to decide if
a leading '.' should be used.  If the extension C<ext> begins with
C<A-Z>, C<a-z>, or C<0-9>, then it will be appended to the filename with
an intermediate '.' (or perhaps a '_' on VMS systems).  Otherwise, it
will be appended directly.

For example, suppose the file is F<somefile.pl>.  For C<-bext=old>, a '.' is
added to give F<somefile.pl.old>.  For C<-bext=.old>, no additional '.' is
added, so again the backup file is F<somefile.pl.old>.  For C<-bext=~>, then no
dot is added, and the backup file will be F<somefile.pl~>  .

=head1 SWITCHES WHICH MAY BE NEGATED

The following list shows all short parameter names which allow a prefix
'n' to produce the negated form:

 D      anl    asbl   asc    ast    asu    atc    atnl   aws    b
 baa    baao   bar    bbao   bbb    bbc    bbs    bl     bli    boa
 boc    bok    bol    bom    bos    bot    cblx   ce     conv   cpb
 cs     csc    cscb   cscw   dac    dbc    dbs    dcbl   dcsc   ddf
 dln    dnl    dop    dp     dpro   drc    dsc    dsm    dsn    dtc
 dtt    dwic   dwls   dwrs   dws    eos    f      fpva   frm    fs
 fso    gcs    hbc    hbcm   hbco   hbh    hbhh   hbi    hbj    hbk
 hbm    hbn    hbp    hbpd   hbpu   hbq    hbs    hbsc   hbv    hbw
 hent   hic    hicm   hico   hih    hihh   hii    hij    hik    him
 hin    hip    hipd   hipu   hiq    his    hisc   hiv    hiw    hsc
 html   ibc    icb    icp    iob    ipc    isbc   iscl   kgb    kgbd
 kgbi   kis    lal    log    lop    lp     lsl    mem    nib    ohbr
 okw    ola    olc    oll    olq    opr    opt    osbc   osbr   otr
 ple    pod    pvl    q      sac    sbc    sbl    scbb   schb   scp
 scsb   sct    se     sfp    sfs    skp    sob    sobb   sohb   sop
 sosb   sot    ssc    st     sts    t      tac    tbc    toc    tp
 tqw    trp    ts     tsc    tso    vbc    vc     viu    vmll   vsc
 w      wfc    wn     x      xbt    xci    xlp    xs

Equivalently, the prefix 'no' or 'no-' on the corresponding long names may be
used.

=head1 LIMITATIONS

=over 4

=item  B<Parsing Limitations>

Perltidy should work properly on most perl scripts.  It does a lot of
self-checking, but still, it is possible that an error could be
introduced and go undetected.  Therefore, it is essential to make
careful backups and to test reformatted scripts.

The main current limitation is that perltidy does not scan modules
included with 'use' statements.  This makes it necessary to guess the
context of any bare words introduced by such modules.  Perltidy has good
guessing algorithms, but they are not infallible.  When it must guess,
it leaves a message in the log file.

If you encounter a bug, please report it.

=item  B<What perltidy does not parse and format>

Perltidy indents but does not reformat comments and C<qw> quotes.
Perltidy does not in any way modify the contents of here documents or
quoted text, even if they contain source code.  (You could, however,
reformat them separately).  Perltidy does not format 'format' sections
in any way.  And, of course, it does not modify pod documents.

=back

=head1 FILES

=over 4

=item B<Temporary files>

Under the -html option with the default --pod2html flag, a temporary file is
required to pass text to Pod::Html.  Unix systems will try to use the POSIX
tmpnam() function.  Otherwise the file F<perltidy.TMP> will be temporarily
created in the current working directory.

=item B<Special files when standard input is used>

When standard input is used, the log file, if saved, is F<perltidy.LOG>,
and any errors are written to F<perltidy.ERR> unless the B<-se> flag is
set.  These are saved in the current working directory.

=item B<Files overwritten>

The following file extensions are used by perltidy, and files with these
extensions may be overwritten or deleted: F<.ERR>, F<.LOG>, F<.TEE>,
and/or F<.tdy>, F<.html>, and F<.bak>, depending on the run type and
settings.

=item  B<Files extensions limitations>

Perltidy does not operate on files for which the run could produce a file with
a duplicated file extension.  These extensions include F<.LOG>, F<.ERR>,
F<.TEE>, and perhaps F<.tdy> and F<.bak>, depending on the run type.  The
purpose of this rule is to prevent generating confusing filenames such as
F<somefile.tdy.tdy.tdy>.

=back

=head1 ERROR HANDLING

An exit value of 0, 1, or 2 is returned by perltidy to indicate the status of the result.

A exit value of 0 indicates that perltidy ran to completion with no error messages.

A non-zero exit value indicates some kind of problem was detected.

An exit value of 1 indicates that perltidy terminated prematurely, usually due
to some kind of errors in the input parameters.  This can happen for example if
a parameter is misspelled or given an invalid value.  Error messages in the
standard error output will indicate the cause of any problem.  If perltidy
terminates prematurely then no output files will be produced.

An exit value of 2 indicates that perltidy was able to run to completion but
there there are (1) warning messages in the standard error output related to
parameter errors or problems and/or (2) warning messages in the perltidy error
file(s) relating to possible syntax errors in one or more of the source
script(s) being tidied.  When multiple files are being processed, an error
detected in any single file will produce this type of exit condition.

=head1 SEE ALSO

perlstyle(1), Perl::Tidy(3)

=head1 INSTALLATION

The perltidy binary uses the Perl::Tidy module and is installed when that module is installed.  The module name is case-sensitive.  For example, the basic command for installing with cpanm is 'cpanm Perl::Tidy'.

=head1 VERSION

This man page documents perltidy version 20230701

=head1 BUG REPORTS

The source code repository is at L<https://github.com/perltidy/perltidy>.

To report a new bug or problem, use the "issues" link on this page.

=head1 COPYRIGHT

Copyright (c) 2000-2023 by Steve Hancock

=head1 LICENSE

This package is free software; you can redistribute it and/or modify it
under the terms of the "GNU General Public License".

Please refer to the file "COPYING" for details.

=head1 DISCLAIMER

This package is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the "GNU General Public License" for more details.
__END__
:endofperl
@set "ErrorLevel=" & @goto _undefined_label_ 2>NUL || @"%COMSPEC%" /d/c @exit %ErrorLevel%
