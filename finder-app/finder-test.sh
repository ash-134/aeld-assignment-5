#!/bin/sh
set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data

if [ -d "/etc/finder-app/conf" ]; then
    CONFDIR="/etc/finder-app/conf"
else
    CONFDIR="conf"
fi

username=$(cat${CONFDIR}/username.txt)

if [ $# -lt 3 ]
then
	echo "Using default value ${WRITESTR} for string to write"
	if [ $# -lt 1 ]
	then
		echo "Using default value ${NUMFILES} for number of files to write"
	else
		NUMFILES=$1
	fi	
else
	NUMFILES=$1
	WRITESTR=$2
	WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are${NUMFILES}"
rm -rf "${WRITEDIR}"
mkdir -p "$WRITEDIR"

for i in $( seq 1$NUMFILES)
do
	if command -v writer >/dev/null 2>&1; then
		writer "$WRITEDIR/${username}$i.txt" "$WRITESTR"
	else
		./writer "$WRITEDIR/${username}$i.txt" "$WRITESTR"
	fi
done

if command -v finder.sh >/dev/null 2>&1; then
	OUTPUTSTRING=$(finder.sh "$WRITEDIR" "$WRITESTR")
else
	OUTPUTSTRING=$(./finder.sh "$WRITEDIR" "$WRITESTR")
fi

echo "${OUTPUTSTRING}" > /tmp/assignment4-result.txt
rm -rf /tmp/aeld-data

set +e
echo ${OUTPUTSTRING} \vert{} grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
	echo "success"
	exit 0
else
	echo "failed: expected ${MATCHSTR} in${OUTPUTSTRING}"
	exit 1
fi
