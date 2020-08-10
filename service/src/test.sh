#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

make clean
make

CASES=0
FAILURES=0

mkdir -p build/programs/assembly
mkdir -p build/programs/force

echo "Testing assembler"
for f in programs/assembly/*.output
do
	CASES=$((CASES+1))
	NAME=$(basename -s .output $f)
	ASSEMBLY="programs/assembly/$NAME.tass"
	INPUT="programs/assembly/$NAME.input"
    OUTPUT_DIR="build/programs/assembly"
    OUTPUT="$OUTPUT_DIR/$NAME.bin"
	echo "Assembling $NAME"
	python3 assembler.py --file "$ASSEMBLY" --output "$OUTPUT"
	echo "Testing $NAME (test case $CASES)"
    cp "$f" "$OUTPUT_DIR"
	if [ -f "$INPUT" ]
	then
		diff -w "$f" <(./build/manchester -f "$OUTPUT" -t 1 2> /dev/null < "$INPUT")
        cp "$INPUT" "$OUTPUT_DIR"
	else
		diff -w "$f" <(./build/manchester -f "$OUTPUT" -t 1 2> /dev/null)
	fi
	if [ $? -ne 0 ]
	then
		echo -e "${RED}FAILED TEST CASE $NAME${NC}"
		FAILURES=$((FAILURES+1))
	fi
done

echo "Testing compiler"
for f in programs/force/*.output
do
	CASES=$((CASES+1))
	NAME=$(basename -s .output $f)
	PROGRAM="programs/force/$NAME.force"
    OUTPUT_DIR="build/programs/force"
    OUTPUT="$OUTPUT_DIR/$NAME.bin"

	echo "Compiling $NAME"
	python3 compiler.py --file "$PROGRAM" --output "$OUTPUT"
	echo "Testing $NAME (test case $CASES)"

    cp "$f" "$OUTPUT_DIR"
	diff -w "$f" <(./build/manchester -f "$OUTPUT" -t 1 2> /dev/null)
	if [ $? -ne 0 ]
	then
		echo -e "${RED}FAILED TEST CASE $NAME${NC}"
		FAILURES=$((FAILURES+1))
	fi

done

if [ "$FAILURES" -eq 0 ]
then
	echo -e "${GREEN}All $CASES test cases PASSED!${NC}"
else
	echo -e "${RED}$FAILURES/$CASES test cases failed :(${NC}"
	exit -1
fi


