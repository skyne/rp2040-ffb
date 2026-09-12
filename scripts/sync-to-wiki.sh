#!/bin/bash
# sync-to-wiki.sh
# Sync documentation from /docs to GitHub wiki repository

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
REPO_URL="https://github.com/skyne/rp2040-ffb.wiki.git"
WIKI_DIR="/tmp/rp2040-ffb.wiki"
DOCS_DIR="$(dirname "$0")/../docs"
ROOT_DIR="$(dirname "$0")/.."

echo -e "${GREEN}rp2040-ffb Wiki Sync Tool${NC}"
echo "=============================="
echo ""

# Step 1: Clone or update wiki
if [ -d "$WIKI_DIR" ]; then
    echo -e "${YELLOW}Wiki already cloned, pulling latest...${NC}"
    cd "$WIKI_DIR"
    git pull origin master
else
    echo -e "${YELLOW}Cloning wiki repository...${NC}"
    git clone "$REPO_URL" "$WIKI_DIR"
    cd "$WIKI_DIR"
fi

echo -e "${GREEN}✓ Wiki repository ready${NC}"
echo ""

# Step 2: Copy documentation files
echo -e "${YELLOW}Copying documentation files...${NC}"

cp "$DOCS_DIR/project-overview.md" "Project-Overview.md"
echo "  ✓ Project-Overview.md"

cp "$DOCS_DIR/comparison.md" "Comparison.md"
echo "  ✓ Comparison.md"

cp "$DOCS_DIR/faq.md" "FAQ.md"
echo "  ✓ FAQ.md"

cp "$DOCS_DIR/build-guide.md" "Complete-Build-Guide.md"
echo "  ✓ Complete-Build-Guide.md"

cp "$DOCS_DIR/wiring-diagrams.md" "Wiring-Diagrams.md"
echo "  ✓ Wiring-Diagrams.md"

cp "$DOCS_DIR/calibration-and-tuning.md" "Calibration-and-Tuning.md"
echo "  ✓ Calibration-and-Tuning.md"

cp "$DOCS_DIR/quick-reference.md" "Quick-Reference-Card.md"
echo "  ✓ Quick-Reference-Card.md"

cp "$DOCS_DIR/troubleshooting.md" "Troubleshooting-Guide.md"
echo "  ✓ Troubleshooting-Guide.md"

cp "$DOCS_DIR/firmware-development.md" "Firmware-Development-Guide.md"
echo "  ✓ Firmware-Development-Guide.md"

cp "$DOCS_DIR/simulator-integration.md" "Simulator-Integration.md"
echo "  ✓ Simulator-Integration.md"

cp "$ROOT_DIR/CONTRIBUTING.md" "Contributing-Guidelines.md"
echo "  ✓ Contributing-Guidelines.md"

echo -e "${GREEN}✓ All files copied${NC}"
echo ""

# Step 3: Create Home.md if it doesn't exist
if [ ! -f "Home.md" ]; then
    echo -e "${YELLOW}Creating Home.md...${NC}"
    cat > Home.md << 'EOF'
# rp2040-ffb Wiki

Welcome to the rp2040-ffb documentation wiki! This wiki contains comprehensive guides for building, using, and modifying your DIY force feedback steering wheel.

## 🚀 Quick Start

**New to the project?** Start here:
1. [[Project Overview]] - What is rp2040-ffb?
2. [[Comparison|Should I Build This?]] - Compare with commercial wheels
3. [[FAQ]] - Common questions answered

**Ready to build?**
1. [[Complete Build Guide]] - Step-by-step assembly
2. [[Wiring Diagrams]] - Connection references
3. [[Calibration and Tuning]] - Optimize performance

**Need help?**
- [[Troubleshooting Guide]] - Fix common problems
- [[Quick Reference Card]] - Command cheat sheet
- [GitHub Discussions](https://github.com/skyne/rp2040-ffb/discussions)

## 📚 Documentation Sections

### Getting Started
- [[Project Overview]] - Architecture and components
- [[Comparison|Should I Build This?]] - vs. commercial wheels
- [[FAQ]] - Frequently asked questions

### Building Your Wheel
- [[Complete Build Guide]] - Assembly instructions
- [[Wiring Diagrams]] - Pinouts and connections
- [[Compatible Donor Wheels]] - G920/G923/G25/G27/G29/DFGT

### Setup & Calibration
- [[Calibration and Tuning]] - Optimize FFB and sensors
- [[Quick Reference Card]] - Commands and settings

### Troubleshooting
- [[Troubleshooting Guide]] - Diagnose and fix issues

### Development
- [[Firmware Development Guide]] - Code architecture
- [[Contributing Guidelines]] - How to contribute
- [[Simulator Integration]] - Hardware-free development

### Advanced Topics
- [[G25 G27 G29 DFGT Compatibility]] - Encoder-based wheels

## 🔗 Quick Links

- [Main Repository](https://github.com/skyne/rp2040-ffb)
- [Releases (Firmware Downloads)](https://github.com/skyne/rp2040-ffb/releases)
- [Issues](https://github.com/skyne/rp2040-ffb/issues)
- [Discussions](https://github.com/skyne/rp2040-ffb/discussions)

## 💡 Contributing to the Wiki

Found an error or want to improve the documentation?
1. Fork the wiki repository
2. Make your changes
3. Submit a pull request

Or open an issue in the main repository.

---

*Documentation is synced from the main repository's `/docs` directory.*
*Last updated: $(date +%Y-%m-%d)*
EOF
    echo -e "${GREEN}✓ Home.md created${NC}"
else
    echo -e "${YELLOW}Home.md already exists, skipping${NC}"
fi

# Step 4: Create or update _Sidebar.md
echo -e "${YELLOW}Creating sidebar navigation...${NC}"
cat > _Sidebar.md << 'EOF'
**Getting Started**
- [[Home]]
- [[Project Overview]]
- [[Comparison|Should I Build This?]]
- [[FAQ]]

**Building**
- [[Complete Build Guide]]
- [[Wiring Diagrams]]

**Setup**
- [[Calibration and Tuning]]
- [[Quick Reference Card]]

**Troubleshooting**
- [[Troubleshooting Guide]]

**Development**
- [[Firmware Development Guide]]
- [[Contributing Guidelines]]
- [[Simulator Integration]]

**Advanced**
- [[G25 G27 G29 DFGT Compatibility]]

---

[Main Repo](https://github.com/skyne/rp2040-ffb) | [Issues](https://github.com/skyne/rp2040-ffb/issues)
EOF
echo -e "${GREEN}✓ Sidebar created${NC}"
echo ""

# Step 5: Create Compatible-Donor-Wheels.md
echo -e "${YELLOW}Creating Compatible Donor Wheels page...${NC}"
cat > Compatible-Donor-Wheels.md << 'EOF'
# Compatible Donor Wheels

## Overview

rp2040-ffb is designed to work with Logitech wheel bases that share the same dual-motor platform.

## Compatibility Chart

| Wheel | Sensor Type | Firmware | Cost (Used) | Availability |
|-------|-------------|----------|-------------|--------------|
| **G920** | MLX90363 hall | ✅ Out-of-box | $150-300 | High |
| **G923** | MLX90363 hall | ✅ Out-of-box | $200-350 | High |
| **G29** | Optical encoder | ⚠️ Needs modification | $100-200 | High |
| **G27** | Optical encoder | ⚠️ Needs modification | $100-200 | Medium |
| **G25** | Optical encoder | ⚠️ Needs modification | $100-200 | Medium |
| **DFGT** | Optical encoder | ⚠️ Needs modification | $80-150 | High |

## Recommended: G920/G923

**Best for:**
- First-time builders
- Want plug-and-play firmware
- Prefer absolute position sensing

**Pros:**
- Works with stock firmware
- No modifications needed
- MLX90363 hall sensor (no wear)
- Well documented

## Budget Option: DFGT (Driving Force GT)

**Best for:**
- Budget builds
- Comfortable with firmware modifications

**Pros:**
- Cheapest option ($80-150)
- Same motors as expensive models
- Widely available (GT5 bundle)

**Cons:**
- Requires encoder firmware modification
- 2-pedal set only
- Less community documentation

## Where to Buy

**eBay:**
- Largest selection
- Best prices for used units
- Check seller ratings

**Facebook Marketplace / Craigslist:**
- Local pickup (no shipping)
- Can test before buying
- Negotiate price

**Amazon:**
- New units available
- Fast shipping
- Easy returns

## What to Check Before Buying (Used)

✅ **Motors:**
- Should spin freely
- No grinding noises
- Test force feedback if possible

✅ **Pedals:**
- All potentiometers work
- No jitter or dead spots
- Springs intact

✅ **Physical Condition:**
- No broken plastic
- Mounting holes intact
- Gear teeth not damaged

⚠️ **Don't worry about:**
- Original PCB (you're replacing it)
- Broken USB (you're rewiring)
- Missing buttons on rim (you're building custom)

## Details

For detailed compatibility information, see:
- [[G25 G27 G29 DFGT Compatibility]] - Encoder modification guide
- [[FAQ]] - Common questions

---

*All models use the same dual-motor platform and mechanics!*
EOF
echo -e "${GREEN}✓ Compatible-Donor-Wheels.md created${NC}"
echo ""

# Step 6: Show git status
echo -e "${YELLOW}Checking for changes...${NC}"
git add .
if git diff-index --quiet HEAD --; then
    echo -e "${GREEN}No changes to commit${NC}"
    exit 0
fi

echo ""
echo -e "${YELLOW}Changes to be committed:${NC}"
git status --short

echo ""
echo -e "${YELLOW}Would you like to commit and push these changes? (y/n)${NC}"
read -r answer

if [ "$answer" = "y" ] || [ "$answer" = "Y" ]; then
    echo ""
    echo -e "${YELLOW}Enter commit message (or press Enter for default):${NC}"
    read -r commit_msg
    
    if [ -z "$commit_msg" ]; then
        commit_msg="Sync documentation from main repository ($(date +%Y-%m-%d))"
    fi
    
    git commit -m "$commit_msg"
    git push origin master
    
    echo ""
    echo -e "${GREEN}✓ Wiki updated successfully!${NC}"
    echo ""
    echo -e "View at: ${GREEN}https://github.com/skyne/rp2040-ffb/wiki${NC}"
else
    echo ""
    echo -e "${YELLOW}Changes staged but not committed.${NC}"
    echo "You can review and commit manually in: $WIKI_DIR"
fi

echo ""
echo -e "${GREEN}Done!${NC}"
