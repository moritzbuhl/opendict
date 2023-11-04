DICT(1) - General Commands Manual

# NAME

**dict** - look up dictionary entries

# SYNOPSIS

**dict**
**-D**&nbsp;*dictionary*
\[**-Vdem**]
*word*&nbsp;\[*...*]

# DESCRIPTION

The
**dict**
utility looks up words in a dictionary of the dictd format.
By default, a prefix match strategy selects all entries that match
*words*.

The options are as follows:

**-D** *dictionary*

> Use the specified
> *dictionary*
> to match and define words.
> See
> *FILES*
> for the naming of the index, dictionary, and parent directory.

**-d**

> Define the given
> *words*
> by looking up all matching entries in the
> *dictionary*.
> If not specified, the
> **-m**
> option is used.

**-e**

> Use an exact match strategy to look up entries that match
> *words*.
> If not specified, a prefix match strategy is used.

**-m**

> Match
> *words*
> in the index of the dictionary.
> This option is used by default.

**-V**

> Do not validate the index for correctness before matching words to
> reduce the overhead per
> **dict**
> invocation.
> By default
> **dict**
> validates the index for correctness before looking up words.

# ENVIRONMENT

`DICT_PATH`

> Specifies the location of the available dictionaries.
> Defaults to
> */usr/local/freedict*.

# FILES

*/usr/local/freedict/foo-bar/*

> Directory containing index and database of the 'foo-bar' dictionary.

*/usr/local/freedict/foo-bar/foo-bar.index*

> Index file with alphabetically sorted words of 'foo' and references
> to the definitions in 'bar'.

*/usr/local/freedict/foo-bar/foo-bar.dict.dz*

> Database file containing definitions of 'bar'.
> A
> gzip(1)
> file with an additional random access header.

# EXAMPLES

Match all index entries for the English word 'ham' in the 'eng-fra'
dictionary with an exact match:

	$ dict -D eng-fra -em ham

Define all words starting with 'manual' in the 'eng-deu' dictionary:

	$ dict -D eng-deu -d manual

# SEE ALSO

gzip(1)
