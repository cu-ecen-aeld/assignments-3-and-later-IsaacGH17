#!/bin/bash
echo "Initializing writer..."
if [ -z "$1" ]; then
   echo "Path not specified"
   exit 1
else
   writefile="$1"
   echo "Selected Path $writefile"
fi
if [ -z "$2" ]; then
    echo "String not specified"
    exit 1
else
    writestr="$2"
   echo "Selected Path $writestr"
fi
if [ ! -d "$DIRECTORY" ]; then
  echo "PATH does not exists, creating a new one..."
  mkdir -p "$(dirname "$writefile")"
else 
   echo "Found path proceeding to write"
fi
if ! echo "$writestr" > "$writefile"; then
    echo "Error: file couldn't be written"
    exit 1
else 
    echo "$writestr" > "$writefile"    
    echo "File written successfully"
fi

