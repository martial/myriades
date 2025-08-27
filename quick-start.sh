#!/bin/bash

# Quick Start Script for Myriades OpenFrameworks Project
# This is a simplified version that downloads and runs everything

set -e

echo "🚀 Myriades OpenFrameworks Quick Start"
echo "======================================"
echo ""

# Check if we're in the right place
if [[ -f "setup_openframeworks_project.sh" ]]; then
    echo "✅ Setup script found, running full setup..."
    ./setup_openframeworks_project.sh
else
    echo "❌ Setup script not found!"
    echo ""
    echo "Please run this from the project directory, or download the setup script:"
    echo "curl -O https://raw.githubusercontent.com/martial/myriades/main/setup_openframeworks_project.sh"
    echo "chmod +x setup_openframeworks_project.sh"
    echo "./setup_openframeworks_project.sh"
    exit 1
fi
