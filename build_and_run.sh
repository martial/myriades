#!/bin/bash

# Compile the application in Debug mode
echo "Compiling in Debug mode..."
make Debug

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful. Running application..."
    # Run the application
    cd bin/emptyExample_debug.app/Contents/MacOS/ && ./emptyExample_debug
else
    echo "Compilation failed. Fix errors before running the application."
    exit 1
fi 