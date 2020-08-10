#!/bin/bash -e

ARCHIVE=/parallel-af.tar.gz

# First, check the uploaded archive

file "$ARCHIVE" | grep -q "gzip"

FILES_IN_ARCHIVE=$(tar -tzf "$ARCHIVE" | sort)

NUM_IN_ARCHIVE=$(echo "$FILES_IN_ARCHIVE" | wc -l)
if [ "$NUM_IN_ARCHIVE" -eq 1 ]
then
    [ "$FILES_IN_ARCHIVE" = "os" -o "$FILES_IN_ARCHIVE" = "manchester" ]
else
    if [ "$NUM_IN_ARCHIVE" -eq 2 ]
    then
        [ "$FILES_IN_ARCHIVE" = "$(echo -e "manchester\nos")" ]
    else
        exit -1;
    fi
fi

# Extract the files (boot.sh will only extract manchester and os and put them in the proper place)
/boot.sh

EXE=/service/manchester

echo "Testing assembler programs"
for f in /test/programs/assembly/*.output
do
	NAME=$(basename -s .output $f)
	BINARY="/test/programs/assembly/$NAME.bin"
	INPUT="/test/programs/assembly/$NAME.input"
	echo "Testing $NAME"

	if [ -f "$INPUT" ]
	then
		diff -w "$f" <("$EXE" -f "$BINARY" -t 1 2> /dev/null < "$INPUT")
	else
		diff -w "$f" <("$EXE" -f "$BINARY" -t 1 2> /dev/null)
	fi
	if [ $? -ne 0 ]
	then
        exit -1;
	fi
done

echo "Testing compiler programs"
for f in /test/programs/force/*.output
do
	NAME=$(basename -s .output $f)
	BINARY="/test/programs/force/$NAME.bin"
	INPUT="/test/programs/force/$NAME.input"
	echo "Testing $NAME"

	if [ -f "$INPUT" ]
	then
		diff -w "$f" <("$EXE" -f "$BINARY" -t 1 2> /dev/null < "$INPUT")
	else
		diff -w "$f" <("$EXE" -f "$BINARY" -t 1 2> /dev/null)
	fi
	if [ $? -ne 0 ]
	then
        exit -1;
	fi
done

