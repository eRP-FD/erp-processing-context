#!/bin/bash

# Default values for optional arguments
binary_name=""
log_name=""
debug=""
root=""
aux=""

# Function to display usage information
usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -b    binary name"
    echo "  -l    log name"
    echo "  -d    debug"
    echo "  -r    root path"
    echo "  -a    aux"
    echo "  -h    Display this help message"
    exit 1
}

# Parse command-line arguments
while getopts ":b:l:d:r:a:h" opt; do
    case $opt in
        b)
            binary_name="$OPTARG"
            ;;
        l)
            log_name="$OPTARG"
            ;;
        d)
            debug="$OPTARG"
            ;;
        r)
            root="$OPTARG"
            ;;
        a)
            aux="$OPTARG,"
            ;;
        h)
            usage
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            usage
            ;;
        :)
            echo "Option -$OPTARG requires an argument." >&2
            usage
            ;;
    esac
done

if [ -z "$binary_name" ]; then
    echo "Error: Required argument -b is missing." >&2
    usage
fi
if [ -z "$log_name" ]; then
    echo "Error: Required argument -l is missing." >&2
    usage
fi
if [ -z "$root" ]; then
    echo "Error: Required argument -r is missing." >&2
    usage
fi

sed -e "s#{binary-name}#$binary_name#g" \
    -e "s#{log-name}#$log_name#g" \
    -e "s#{custom}#$aux#g" \
    -e "s#{type}#$debug#g" \
    -e "s#{root}#$root#g" ${!#}
