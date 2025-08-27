#!/bin/bash

# OpenFrameworks Project Auto Setup Script
# This script downloads OpenFrameworks, sets up the project structure, and builds the project
# NON-INTERACTIVE VERSION - runs automatically

set -e  # Exit on any error

# Configuration
OF_VERSION="0.12.1"
OF_PLATFORM="osx"
PROJECT_NAME="emptyExample"
REPO_URL="https://github.com/martial/myriades.git"
REPO_BRANCH="main"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if we're on macOS
check_system() {
    log_info "Checking system requirements..."
    
    if [[ "$OSTYPE" != "darwin"* ]]; then
        log_error "This script is designed for macOS only"
        exit 1
    fi
    
    # Check for Xcode command line tools
    if ! xcode-select -p &> /dev/null; then
        log_error "Xcode command line tools not found. Please install them first:"
        log_error "xcode-select --install"
        exit 1
    fi
    
    # Check for git
    if ! command -v git &> /dev/null; then
        log_error "Git not found. Please install git first."
        exit 1
    fi
    
    log_success "System requirements met"
}

# Download and extract OpenFrameworks
download_openframeworks() {
    local of_dir="of_v${OF_VERSION}_${OF_PLATFORM}_release"
    local of_archive="${of_dir}.tar.gz"
    local of_url="https://github.com/openframeworks/openFrameworks/releases/download/${OF_VERSION}/${of_archive}"
    
    log_info "Downloading OpenFrameworks v${OF_VERSION} for ${OF_PLATFORM}..."
    
    if [[ -d "$of_dir" ]]; then
        log_warning "OpenFrameworks directory already exists. Skipping download."
        return 0
    fi
    
    # Download if archive doesn't exist
    if [[ ! -f "$of_archive" ]]; then
        log_info "Downloading from: $of_url"
        curl -L -o "$of_archive" "$of_url" || {
            log_error "Failed to download OpenFrameworks"
            exit 1
        }
    else
        log_info "Using existing archive: $of_archive"
    fi
    
    # Extract
    log_info "Extracting OpenFrameworks..."
    tar -xzf "$of_archive" || {
        log_error "Failed to extract OpenFrameworks"
        exit 1
    }
    
    log_success "OpenFrameworks downloaded and extracted"
}

# Clone the project repository
setup_project() {
    local of_dir="of_v${OF_VERSION}_${OF_PLATFORM}_release"
    local project_path="$of_dir/apps/myApps/$PROJECT_NAME"
    
    log_info "Setting up project repository..."
    
    # Create myApps directory if it doesn't exist
    mkdir -p "$of_dir/apps/myApps"
    
    # Clone or update the repository
    if [[ -d "$project_path" ]]; then
        log_warning "Project directory already exists. Pulling latest changes..."
        cd "$project_path"
        git pull origin "$REPO_BRANCH" || {
            log_warning "Failed to pull changes. Continuing with existing code..."
        }
        cd - > /dev/null
    else
        log_info "Cloning repository..."
        git clone -b "$REPO_BRANCH" "$REPO_URL" "$project_path" || {
            log_error "Failed to clone repository"
            exit 1
        }
    fi
    
    log_success "Project repository set up"
}

# Compile OpenFrameworks libraries
compile_openframeworks() {
    local of_dir="of_v${OF_VERSION}_${OF_PLATFORM}_release"
    
    log_info "Compiling OpenFrameworks libraries..."
    
    cd "$of_dir"
    
    # Compile core OF libraries
    log_info "Compiling core libraries..."
    make Release -C libs/openFrameworksCompiled/project || {
        log_error "Failed to compile OpenFrameworks libraries"
        exit 1
    }
    
    cd - > /dev/null
    
    log_success "OpenFrameworks libraries compiled"
}

# Build the project
build_project() {
    local of_dir="of_v${OF_VERSION}_${OF_PLATFORM}_release"
    local project_path="$of_dir/apps/myApps/$PROJECT_NAME"
    
    log_info "Building project..."
    
    cd "$project_path"
    
    # Clean and build
    make clean
    make Release -j$(sysctl -n hw.ncpu) || {
        log_error "Failed to build project"
        exit 1
    }
    
    cd - > /dev/null
    
    log_success "Project built successfully"
}

# Create run script
create_run_script() {
    local of_dir="of_v${OF_VERSION}_${OF_PLATFORM}_release"
    local project_path="$of_dir/apps/myApps/$PROJECT_NAME"
    local run_script="run_${PROJECT_NAME}.sh"
    
    log_info "Creating run script..."
    
    cat > "$run_script" << EOF
#!/bin/bash

# Run script for $PROJECT_NAME
# This script runs the compiled OpenFrameworks application

cd "$project_path"
make RunRelease
EOF
    
    chmod +x "$run_script"
    
    log_success "Run script created: $run_script"
}

# Display final instructions
show_instructions() {
    local of_dir="of_v${OF_VERSION}_${OF_PLATFORM}_release"
    local project_path="$of_dir/apps/myApps/$PROJECT_NAME"
    
    echo ""
    log_success "🎉 Setup complete!"
    echo ""
    echo -e "${BLUE}Project location:${NC} $project_path"
    echo -e "${BLUE}OpenFrameworks:${NC} $of_dir"
    echo ""
    echo -e "${GREEN}To run the application:${NC}"
    echo "  ./run_${PROJECT_NAME}.sh"
    echo ""
    echo -e "${GREEN}To run manually:${NC}"
    echo "  cd $project_path"
    echo "  make RunRelease"
    echo ""
    echo -e "${GREEN}Project features:${NC}"
    echo "  ✅ OSC Output Controller"
    echo "  ✅ HourGlass LED/Motor Control"
    echo "  ✅ Position-based OSC addressing (/top, /bot)"
    echo "  ✅ Embedded JSON configuration"
    echo "  ✅ Change detection (no message flooding)"
    echo "  ✅ Serial-independent operation"
    echo ""
}

# Main execution
main() {
    echo -e "${BLUE}"
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║               OpenFrameworks Auto Setup                     ║"
    echo "║                                                              ║"
    echo "║  Starting automatic setup...                                ║"
    echo "║  • Download OpenFrameworks v${OF_VERSION}                              ║"
    echo "║  • Clone the myriades project repository                    ║"
    echo "║  • Compile OpenFrameworks libraries                         ║"
    echo "║  • Build the ${PROJECT_NAME} project                           ║"
    echo "║  • Create run scripts                                       ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
    echo ""
    
    # Execute setup steps automatically
    check_system
    download_openframeworks
    setup_project
    compile_openframeworks
    build_project
    create_run_script
    show_instructions
}

# Run main function
main "$@"