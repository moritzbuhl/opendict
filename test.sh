#!/bin/sh
set -e

function cleanup {
  rm "$tmp"
}
tmp=$(mktemp)
trap cleanup EXIT

if [ "$(uname)" = Linux ]; then
	ncpu=$(grep siblings /proc/cpuinfo  | tail -1 | cut -d: -f2)
	DICT=./dict
else
	ncpu=$(sysctl -n hw.ncpuonline)
	DICT=./obj/dict
fi

if command -v mandoc > /dev/null; then
	echo check SYNOPSIS and usage are equal
	synopsis=$(mandoc -Tmarkdown dict.1  | \
	    sed -n '/^# SYNOPSIS/{x;d;};H;/^# DESCRIPTION/{x;p;};' | \
	    sed -e 's/[\*\\]//g' -e 's/&nbsp;/ /g' -e 's/^#.*//g' | \
	    tail +2 | tr '\n' ' ' | cut -d# -f1)
	synopsis=${synopsis%%+( )}
	usage=$($DICT -h 2>&1 | cut -d: -f2 | tail +2)
	usage=${usage%%+( )}
	if [ "$usage" != "$synopsis" ]; then
		echo "usage != synopsis: '$usage' != '$synopsis'"
		exit 1
	fi
	echo .
fi

echo verify all index files
for f in /usr/local/freedict/*; do
	b=$(basename $f);
	echo -n .
	$DICT -eD $b -m ! >/dev/null
done
echo

echo lookup every word
for f in /usr/local/freedict/*; do
	b=$(basename "$f");
	echo -n .
	cut -d'	' -f1 "$f/$b.index" | grep -v '^$' | uniq > "$tmp"
	idx=$(cat "$tmp" | wc -l)
	n=$((idx / ncpu))
	dct=$(cat "$tmp" | tr \\n \\0 | xargs -P $ncpu -n $n -0 $DICT -VeD "$b" | wc -l)
	if [ "$idx" -ne "$dct" ]; then
		cat "$tmp"
		echo "$b: $idx vs $dct"
		exit 1
	fi
done
echo

echo lookup between every word
for f in /usr/local/freedict/*; do
	b=$(basename "$f");
	echo -n .
	echo ! > "$tmp"
	cut -d'	' -f1 "$f/$b.index" | grep -v '^$' | uniq | sed -e 's/$/!/g' >> "$tmp"
	idx=$(cat "$tmp" | wc -l)
	n=$((idx / ncpu))
	dct=$(cat "$tmp" | tr \\n \\0 | xargs -P $ncpu -n $n -0 $DICT -VeD "$b")
	if [ -n "$dct" ]; then
		cat "$tmp"
		echo "$b: $dct"
		exit 1
	fi
done
echo

echo intermix words with garbage
for f in /usr/local/freedict/*; do
	b=$(basename "$f");
	echo -n .
	cut -d'	' -f1 "$f/$b.index" | grep -v '^$' | uniq > "$tmp"
	idx=$(cat "$tmp" | wc -l)
	n=$((idx / ncpu))
	dct=$(sort "$tmp" | sed -e 's/^\(.*\)/\1\
\1!/'| tr \\n \\0 | xargs -P $ncpu -n $n -0 $DICT -VeD "$b" | wc -l)
	if [ "$idx" -ne "$dct" ]; then
		cat "$tmp"
		echo "$b: $idx vs $dct"
		exit 1
	fi
done
echo
