# GitHub Wiki Setup Guide

**How to organize rp2040-ffb documentation into GitHub Wiki**

This guide shows how to structure all the documentation into the GitHub wiki for easy navigation.

---

## Step 1: Enable the Wiki

1. Go to https://github.com/skyne/rp2040-ffb
2. Click **Settings** tab
3. Scroll to **Features** section
4. Check ✅ **Wikis**
5. Click **Save**

---

## Step 2: Clone the Wiki Repository

```bash
# Clone the wiki (separate from main repo)
git clone https://github.com/skyne/rp2040-ffb.wiki.git
cd rp2040-ffb.wiki
```

---

## Step 3: Wiki Structure

### Recommended Page Organization

```
Home                                    (main landing page)
├── Getting Started
│   ├── Project Overview
│   ├── Should I Build This? (Comparison)
│   └── FAQ
│
├── Building Your Wheel
│   ├── Complete Build Guide
│   ├── Wiring Diagrams
│   ├── Parts List (BOM)
│   └── Compatible Donor Wheels
│
├── Setup & Calibration
│   ├── First Power-On
│   ├── Calibration & Tuning
│   ├── Quick Reference Card
│   └── Settings Guide
│
├── Troubleshooting
│   ├── Troubleshooting Guide
│   ├── Common Issues
│   └── Recovery Procedures
│
├── Development
│   ├── Firmware Development Guide
│   ├── Contributing Guidelines
│   ├── Code Architecture
│   └── Testing
│
└── Advanced Topics
    ├── G25/G27/G29/DFGT Compatibility
    ├── Simulator Integration
    └── Custom Modifications
```

---

## Step 4: Create Wiki Pages

GitHub wiki pages are markdown files. Create these files in the wiki repo:

### File Naming Convention

GitHub wiki converts filenames to page titles:
- `Home.md` → "Home"
- `Complete-Build-Guide.md` → "Complete Build Guide"
- `FAQ.md` → "FAQ"

### Core Pages to Create

#### 1. **Home.md** (Wiki Homepage)

```markdown
# rp2040-ffb Wiki

Welcome to the rp2040-ffb documentation wiki! This wiki contains comprehensive guides for building, using, and modifying your DIY force feedback steering wheel.

## 🚀 Quick Start

**New to the project?** Start here:
1. [[Project Overview]] - What is rp2040-ffb?
2. [[Should I Build This?|Comparison]] - Compare with commercial wheels
3. [[FAQ]] - Common questions answered

**Ready to build?**
1. [[Complete Build Guide]] - Step-by-step assembly
2. [[Wiring Diagrams]] - Connection references
3. [[Calibration & Tuning]] - Optimize performance

**Need help?**
- [[Troubleshooting Guide]] - Fix common problems
- [[Quick Reference Card]] - Command cheat sheet
- [GitHub Discussions](https://github.com/skyne/rp2040-ffb/discussions)

## 📚 Documentation Sections

### Getting Started
- [[Project Overview]] - Architecture and components
- [[Should I Build This?|Comparison]] - vs. commercial wheels
- [[FAQ]] - Frequently asked questions

### Building Your Wheel
- [[Complete Build Guide]] - Assembly instructions
- [[Wiring Diagrams]] - Pinouts and connections
- [[Compatible Donor Wheels]] - G920/G923/G25/G27/G29/DFGT

### Setup & Calibration
- [[Calibration & Tuning]] - Optimize FFB and sensors
- [[Quick Reference Card]] - Commands and settings
- [[GUI Configuration Tool]] - Desktop app guide

### Troubleshooting
- [[Troubleshooting Guide]] - Diagnose and fix issues
- [[Recovery Procedures]] - Unbrick your wheel

### Development
- [[Firmware Development Guide]] - Code architecture
- [[Contributing Guidelines]] - How to contribute
- [[Simulator Integration]] - Hardware-free development

### Advanced Topics
- [[G25 G27 G29 DFGT Compatibility]] - Encoder-based wheels
- [[Custom Modifications]] - Add your own features

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

*Last updated: 2024-09-12*
```

#### 2. **Project-Overview.md**

Copy content from: `docs/project-overview.md`

#### 3. **Comparison.md** (page title: "Should I Build This?")

Copy content from: `docs/comparison.md`

#### 4. **FAQ.md**

Copy content from: `docs/faq.md`

#### 5. **Complete-Build-Guide.md**

Copy content from: `docs/build-guide.md`

#### 6. **Wiring-Diagrams.md**

Copy content from: `docs/wiring-diagrams.md`

#### 7. **Calibration-and-Tuning.md**

Copy content from: `docs/calibration-and-tuning.md`

#### 8. **Quick-Reference-Card.md**

Copy content from: `docs/quick-reference.md`

#### 9. **Troubleshooting-Guide.md**

Copy content from: `docs/troubleshooting.md`

#### 10. **Firmware-Development-Guide.md**

Copy content from: `docs/firmware-development.md`

#### 11. **Contributing-Guidelines.md**

Copy content from: `CONTRIBUTING.md`

#### 12. **Simulator-Integration.md**

Copy content from: `docs/simulator-integration.md`

#### 13. **G25-G27-G29-DFGT-Compatibility.md**

Extract from: `docs/faq.md` (the G25/G27/G29/DFGT Compatibility section)

#### 14. **Compatible-Donor-Wheels.md**

```markdown
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

## Budget Option: DFGT

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

## Details

For detailed compatibility information, see:
- [[G25 G27 G29 DFGT Compatibility]] - Encoder modification guide
- [[FAQ]] - Common questions

---

*All models use the same dual-motor platform and mechanics!*
```

---

## Step 5: Copy Content to Wiki

### Automated Approach (Recommended)

Create a script to copy all content:

```bash
#!/bin/bash
# copy-to-wiki.sh

DOCS_DIR="/workspace/docs"
WIKI_DIR="/tmp/rp2040-ffb.wiki"

# Ensure wiki is cloned
if [ ! -d "$WIKI_DIR" ]; then
    cd /tmp
    git clone https://github.com/skyne/rp2040-ffb.wiki.git
fi

cd "$WIKI_DIR"

# Copy main documentation files
cp "$DOCS_DIR/project-overview.md" "Project-Overview.md"
cp "$DOCS_DIR/comparison.md" "Comparison.md"
cp "$DOCS_DIR/faq.md" "FAQ.md"
cp "$DOCS_DIR/build-guide.md" "Complete-Build-Guide.md"
cp "$DOCS_DIR/wiring-diagrams.md" "Wiring-Diagrams.md"
cp "$DOCS_DIR/calibration-and-tuning.md" "Calibration-and-Tuning.md"
cp "$DOCS_DIR/quick-reference.md" "Quick-Reference-Card.md"
cp "$DOCS_DIR/troubleshooting.md" "Troubleshooting-Guide.md"
cp "$DOCS_DIR/firmware-development.md" "Firmware-Development-Guide.md"
cp "$DOCS_DIR/simulator-integration.md" "Simulator-Integration.md"

# Copy root files
cp "/workspace/CONTRIBUTING.md" "Contributing-Guidelines.md"

# Create Home.md (use template above)
cat > Home.md << 'EOF'
# rp2040-ffb Wiki

Welcome to the rp2040-ffb documentation wiki!
...
EOF

# Create sidebar navigation
cat > _Sidebar.md << 'EOF'
**Getting Started**
- [[Home]]
- [[Project Overview]]
- [[Comparison|Should I Build This?]]
- [[FAQ]]

**Building**
- [[Complete Build Guide]]
- [[Wiring Diagrams]]
- [[Compatible Donor Wheels]]

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
EOF

# Commit and push
git add .
git commit -m "Initial wiki setup with comprehensive documentation"
git push origin master

echo "Wiki updated successfully!"
```

### Manual Approach

1. Go to wiki: https://github.com/skyne/rp2040-ffb/wiki
2. Click **"New Page"**
3. Enter page title (e.g., "Project Overview")
4. Copy content from corresponding doc file
5. Click **"Save Page"**
6. Repeat for each document

---

## Step 6: Create Sidebar Navigation

Create `_Sidebar.md` in the wiki repository:

```markdown
**Getting Started**
- [[Home]]
- [[Project Overview]]
- [[Comparison|Should I Build This?]]
- [[FAQ]]

**Building**
- [[Complete Build Guide]]
- [[Wiring Diagrams]]
- [[Compatible Donor Wheels]]

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

[Main Repo](https://github.com/skyne/rp2040-ffb) | [Issues](https://github.com/skyne/rp2040-ffb/issues) | [Discussions](https://github.com/skyne/rp2040-ffb/discussions)
```

This sidebar will appear on every wiki page.

---

## Step 7: Update Main README

Add wiki link to main repository README:

```markdown
## Documentation

📖 **[Visit the Wiki](https://github.com/skyne/rp2040-ffb/wiki)** for comprehensive documentation!

Quick links:
- [Complete Build Guide](https://github.com/skyne/rp2040-ffb/wiki/Complete-Build-Guide)
- [Wiring Diagrams](https://github.com/skyne/rp2040-ffb/wiki/Wiring-Diagrams)
- [Troubleshooting](https://github.com/skyne/rp2040-ffb/wiki/Troubleshooting-Guide)
- [FAQ](https://github.com/skyne/rp2040-ffb/wiki/FAQ)
```

---

## Benefits of Using GitHub Wiki

### For Users

**✅ Easy Navigation:**
- Sidebar always visible
- Wiki search built-in
- Clear hierarchy

**✅ Better Discoverability:**
- GitHub shows wiki tab prominently
- Search engines index wiki pages
- Direct links to specific sections

**✅ Clean Reading Experience:**
- No code clutter
- Focused on documentation
- Mobile-friendly

### For Maintainers

**✅ Separation of Concerns:**
- Keep `/docs` for source documentation
- Wiki for rendered, user-facing docs
- No doc files cluttering main repo view

**✅ Community Contributions:**
- Users can edit wiki directly (if enabled)
- Or fork and PR wiki changes
- Lower barrier than main repo PRs

**✅ Version Independence:**
- Wiki not tied to code versions
- Can update docs without releases
- No merge conflicts with code changes

---

## Maintaining Both Docs and Wiki

### Recommended Workflow

**1. Keep `/docs` as source of truth:**
- All documentation lives in `/workspace/docs`
- Updated via normal git commits and PRs
- CI can validate and test docs

**2. Sync to wiki periodically:**
- Manual sync after major doc updates
- Or automated via GitHub Actions
- Wiki is "compiled" view of `/docs`

**3. GitHub Action for Auto-Sync (Optional):**

`.github/workflows/sync-wiki.yml`:

```yaml
name: Sync Wiki

on:
  push:
    branches: [main]
    paths:
      - 'docs/**'
      - 'CONTRIBUTING.md'

jobs:
  sync:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Clone Wiki
        run: |
          git clone https://github.com/skyne/rp2040-ffb.wiki.git wiki
      
      - name: Copy Docs
        run: |
          cp docs/project-overview.md wiki/Project-Overview.md
          cp docs/comparison.md wiki/Comparison.md
          cp docs/faq.md wiki/FAQ.md
          cp docs/build-guide.md wiki/Complete-Build-Guide.md
          cp docs/wiring-diagrams.md wiki/Wiring-Diagrams.md
          cp docs/calibration-and-tuning.md wiki/Calibration-and-Tuning.md
          cp docs/quick-reference.md wiki/Quick-Reference-Card.md
          cp docs/troubleshooting.md wiki/Troubleshooting-Guide.md
          cp docs/firmware-development.md wiki/Firmware-Development-Guide.md
          cp docs/simulator-integration.md wiki/Simulator-Integration.md
          cp CONTRIBUTING.md wiki/Contributing-Guidelines.md
      
      - name: Push to Wiki
        run: |
          cd wiki
          git config user.name "GitHub Actions"
          git config user.email "actions@github.com"
          git add .
          git diff-index --quiet HEAD || git commit -m "Auto-sync from docs/"
          git push
```

---

## Link Adjustments

When copying to wiki, update internal links:

### In Markdown Files

**Before (docs/):**
```markdown
See [Build Guide](build-guide.md) for details.
```

**After (wiki):**
```markdown
See [[Complete Build Guide]] for details.
```

### Wiki Link Syntax

```markdown
[[Page Name]]                    # Simple link
[[Page Name|Display Text]]       # Custom text
[[Complete Build Guide#step-3]]  # Link to section
```

---

## Testing the Wiki

After setup:

1. **Visit:** https://github.com/skyne/rp2040-ffb/wiki
2. **Check navigation:** Sidebar should show all sections
3. **Test links:** Click through to verify all internal links work
4. **Search:** Try wiki search for "calibration", "troubleshooting", etc.
5. **Mobile:** View on mobile device (should be responsive)

---

## Next Steps

1. ✅ Enable wiki in repository settings
2. ✅ Create Home.md with navigation
3. ✅ Copy all documentation files
4. ✅ Create _Sidebar.md for navigation
5. ✅ Update main README with wiki link
6. ✅ (Optional) Set up auto-sync GitHub Action
7. ✅ Announce wiki availability in Discussions

---

## Summary

**What you'll have:**

- 📖 Comprehensive wiki with all documentation
- 🗂️ Clear navigation structure (sidebar)
- 🔍 Searchable content
- 📱 Mobile-friendly interface
- 🔗 Direct links to specific guides
- 📝 Easy for users to find information
- 🚀 Professional documentation platform

**Files to create in wiki:**

1. Home.md
2. Project-Overview.md
3. Comparison.md
4. FAQ.md
5. Complete-Build-Guide.md
6. Wiring-Diagrams.md
7. Calibration-and-Tuning.md
8. Quick-Reference-Card.md
9. Troubleshooting-Guide.md
10. Firmware-Development-Guide.md
11. Contributing-Guidelines.md
12. Simulator-Integration.md
13. G25-G27-G29-DFGT-Compatibility.md
14. Compatible-Donor-Wheels.md
15. _Sidebar.md

**Total documentation: ~15,000 lines organized into an accessible wiki!**

---

*For questions about wiki setup, open a GitHub Discussion.*
