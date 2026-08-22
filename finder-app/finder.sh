#!/bin/bash
#finder shell script
echo "Initializing finder..."
if [ -z "$1" ]; then
   echo "Path not specified"
   exit 1
else 
   filesdir="$1"
   echo "Selected Path: $1"
fi
if [ -z "$2" ]; then
    echo "String not specified"
    exit 1
else
    searchstr="$2"
    echo "String to find: $2"    
fi
echo "Checking if path exists"
if [ ! -d "$1" ]; then
   echo "Path does not exist"
   exit 1	
fi
X="$(find "$filesdir" -type f | wc -l)"
Y="$(grep -r "$searchstr" "$filesdir" | wc -l)"
echo "The number of files are $X and the number of matching lines are $Y"
