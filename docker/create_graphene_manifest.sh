#!/bin/bash

set -o nounset

if [ "$#" -ne 4 ]; then
	echo
	echo "Usage: $0 path_to_install_manifest.txt name_binary path_to_graphene path_to_manifest"
	echo
	echo "  path_to_install_manifest.txt  Usually generated from CMake (/build/install_manifest.txt)"
	echo "  name_binary                   The name of the binary (myapp)"
	echo "  path_to_graphene              Location of graphene installation (/graphene)"
	echo "  path_to_manifest              Output manifest being generated from this script (/app/bin/myapp.manifest)"
	echo
	exit 1
fi

INSTALL_MANIFEST=$1
BINARY=$2
GRAPHENE=$3
TARGET=$4

if [ ! -f $INSTALL_MANIFEST ]; then
	echo
	echo "File does not exist \"$INSTALL_MANIFEST\"."
	echo
	exit 1
fi

if [ ! -f $TARGET ]; then
	echo
	echo "File does not exist \"$TARGET\"."
	echo
	exit 1
fi

echo "--------------------------------------------"
echo "Create $BINARY.manifest"
echo "--------------------------------------------"

# Enumerate and append a list of trusted_files based on CMakes install_manifest.txt.
# For each file it generate a string of:
#   sgx.trusted_files.f000000 = "file:path"
#   sgx.trusted_files.f000001 = "file:path"
#   ...
#   sgx.trusted_files.f000100 = "file:path"
#   ...
# Each string will be appended at the end of the manifest file.

COUNT=0

cat $INSTALL_MANIFEST | while read line || [ -n "$line" ]; do
    basename=`basename $line`
    if [ $basename = $BINARY ]; then
        echo $basename
    else
        echo "sgx.trusted_files.f$(printf '%06d' $COUNT) = \"file:$line\"" >> $TARGET
        COUNT=$((COUNT+1))
    fi
done

# Replace the placeholders in the manifest file with the given values.

sed -i "s#_BINARY_#$BINARY#g" $TARGET
sed -i "s#_GRAPHENE_#$GRAPHENE#g" $TARGET
