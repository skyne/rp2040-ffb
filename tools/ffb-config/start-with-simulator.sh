#!/usr/bin/env bash
# Start ffb-config with firmware simulator for hardware-free development
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SIM_DIR="$SCRIPT_DIR/../firmware-sim"
VENV_DIR="$SIM_DIR/venv"

echo "=========================================="
echo "  ffb-config with Simulator Mode"
echo "=========================================="
echo ""

# Check if simulator exists
if [ ! -f "$SIM_DIR/sim_base.py" ]; then
    echo "ERROR: Simulator not found at $SIM_DIR"
    echo "Please ensure firmware-sim/ directory exists"
    exit 1
fi

# Setup Python virtual environment for simulator
if [ ! -d "$VENV_DIR" ]; then
    echo "Setting up Python virtual environment..."
    python3 -m venv "$VENV_DIR"
    source "$VENV_DIR/bin/activate"
    pip install -q -r "$SIM_DIR/requirements.txt"
    echo "✓ Virtual environment ready"
else
    source "$VENV_DIR/bin/activate"
fi

# Detect platform for virtual serial port setup
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    # Linux - use socat to create virtual serial ports
    echo "Platform: Linux"
    echo ""
    echo "Creating virtual serial ports with socat..."
    
    # Kill any existing socat processes
    pkill -f "socat.*pty" || true
    sleep 0.5
    
    # Create virtual port pair
    socat -d -d pty,raw,echo=0,link=/tmp/ffb-sim-base pty,raw,echo=0,link=/tmp/ffb-sim-gui 2>&1 | \
        grep --line-buffered "N PTY" | head -2 &
    SOCAT_PID=$!
    
    sleep 1
    
    # Check if socat created the links
    if [ ! -L /tmp/ffb-sim-base ] || [ ! -L /tmp/ffb-sim-gui ]; then
        echo "ERROR: Failed to create virtual serial ports"
        echo "Please install socat: sudo apt-get install socat"
        exit 1
    fi
    
    SIM_PORT="/tmp/ffb-sim-base"
    GUI_PORT="/tmp/ffb-sim-gui"
    
    echo "✓ Virtual ports ready:"
    echo "  Simulator: $SIM_PORT"
    echo "  GUI:       $GUI_PORT"
    
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS - use built-in /dev/ttys*
    echo "Platform: macOS"
    echo ""
    echo "Using macOS pseudo-terminals..."
    
    # On macOS, we need to use a different approach
    # Create a named pipe for communication
    FIFO_SIM="/tmp/ffb-sim-$$"
    FIFO_GUI="/tmp/ffb-gui-$$"
    
    mkfifo "$FIFO_SIM" "$FIFO_GUI" || true
    
    # Use socat if available, otherwise suggest installation
    if command -v socat &> /dev/null; then
        socat -d -d pty,raw,echo=0,link=/tmp/ffb-sim-base pty,raw,echo=0,link=/tmp/ffb-sim-gui &
        SOCAT_PID=$!
        sleep 1
        SIM_PORT="/tmp/ffb-sim-base"
        GUI_PORT="/tmp/ffb-sim-gui"
    else
        echo "ERROR: socat not found. Please install:"
        echo "  brew install socat"
        exit 1
    fi
    
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    # Windows - use com0com (must be pre-installed)
    echo "Platform: Windows"
    echo ""
    echo "Checking for com0com virtual ports..."
    echo ""
    echo "NOTE: You must have com0com installed and configured"
    echo "      with a port pair (e.g., COM10 <-> COM11)"
    echo ""
    echo "Download: https://sourceforge.net/projects/com0com/"
    echo ""
    
    # Use default Windows COM ports
    SIM_PORT="COM10"
    GUI_PORT="COM11"
    
    echo "Using ports:"
    echo "  Simulator: $SIM_PORT"
    echo "  GUI:       $GUI_PORT"
    echo ""
    echo "If these ports don't exist, configure com0com first!"
    echo "Press Ctrl+C to abort or Enter to continue..."
    read
    
else
    echo "ERROR: Unsupported platform: $OSTYPE"
    exit 1
fi

echo ""
echo "Starting firmware simulator..."
echo ""

# Start simulator in background
python3 "$SIM_DIR/sim_base.py" --port "$SIM_PORT" > /tmp/ffb-simulator.log 2>&1 &
SIM_PID=$!

# Wait for simulator to initialize
sleep 2

# Check if simulator is still running
if ! kill -0 $SIM_PID 2>/dev/null; then
    echo "ERROR: Simulator failed to start"
    echo "Log output:"
    cat /tmp/ffb-simulator.log
    exit 1
fi

echo "✓ Simulator running (PID: $SIM_PID)"
echo "  Log: /tmp/ffb-simulator.log"
echo ""
echo "=========================================="
echo "  Starting GUI..."
echo "=========================================="
echo ""
echo "Connect to: $GUI_PORT"
echo ""
echo "🎮 SIMULATOR MODE ACTIVE"
echo "   No hardware required!"
echo ""

# Set environment variable to tell GUI it's in simulator mode
export FFB_SIMULATOR_MODE=1
export FFB_SIMULATOR_PORT="$GUI_PORT"

# Start the GUI
cd "$SCRIPT_DIR"
npm run tauri dev &
GUI_PID=$!

# Trap exit to cleanup
cleanup() {
    echo ""
    echo "Shutting down..."
    kill $SIM_PID 2>/dev/null || true
    kill $GUI_PID 2>/dev/null || true
    if [ -n "${SOCAT_PID:-}" ]; then
        kill $SOCAT_PID 2>/dev/null || true
    fi
    rm -f /tmp/ffb-sim-base /tmp/ffb-sim-gui
    echo "Cleanup complete"
}

trap cleanup EXIT INT TERM

# Wait for GUI
wait $GUI_PID
