Regexgen
==========

Regexgen enumerates all strings matching a given input regular expressions (up to a finite limit).

Installation/Usage
-------

You can compile the program with gcc and Qt. On a standard Linux KDE system this is a single call to:

    g++ regexgen.cpp -o regexgen -I/usr/include/qt4/QtCore -I/usr/include/qt4/ -lQtCore

My homepage also provides a [linux 64 binary](http://www.benibela.de/tools_en.html#regexgen).

---

The program then reads an arbitrary input regex from stdin, and prints all possible matches.

In case there are infinite many matches, you can set, when it should abort, with the command line arguments. (e.g. to treat `x+` as `x{1,5}`)

Use --help to print a list of allowed arguments.

Examples:
-------

Input: 

    [ab]{2}
    
Output:

    aa
    ba
    ab
    bb

Input (famous regex, matching all non-prime numbers. The abort criteria is given as command line argument.):

    ^1?$|^(11+?)\1+$
    
Output:

    1
    1111
    111111
    11111111
    1111111111
    111111111111
    111111
    111111111
    111111111111
    111111111111111
    111111111111111111
    11111111
    111111111111
    1111111111111111
    11111111111111111111
    111111111111111111111111
    1111111111
    111111111111111
    11111111111111111111
    1111111111111111111111111
    111111111111111111111111111111
    111111111111
    111111111111111111
    111111111111111111111111
    111111111111111111111111111111
    111111111111111111111111111111111111


Details
------

It works as follow:

1. All ? {} + * | () operators are expanded (to a maximal limit), so that only character classes and backreferences remain.

   e.g. `[a-c]+|t*|([x-z]){2}foo\1|(a|b)(t|u)`   becomes `[a-c]|[a-c][a-c]|[a-c][a-c][a-c]|[a-c][a-c][a-c][a-c]||t|tt|tt|ttt|ttt|([x-z][x-z])foo\1|at|au|bt|bu`

   (the | in latter expression are just notation, the program keeps each alternative  subregex in a list)

2. Backreferences to multiple characters are replaced by backreferences to single characters.

  e.g. the expression above becomes `[a-c]|[a-c][a-c]|[a-c][a-c][a-c]|[a-c][a-c][a-c][a-c]||t|tt|tt|ttt|ttt|([x-z])([x-z])foo\1\2|at|au|bt|bu`

  Now each alternative subregex matches a fixed length string.

3. For each of the alternatives, all combinations of picking characters from the classes are printed:

   e.g. the expression above becomes `a|b|c|aa|ba|..|cc|aaa|baa|...|ccc|aaaa|...|cccc||t|tt|tt|ttt|ttt|xxfooxx|yxfooyx|...|zzfoozz|at|au|bt|bu`


