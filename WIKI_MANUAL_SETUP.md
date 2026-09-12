# Manual Wiki Setup Instructions

The wiki is enabled but needs an initial page before it can be cloned. Follow these steps:

## Step 1: Create the First Page

1. Go to: https://github.com/skyne/rp2040-ffb/wiki
2. Click **"Create the first page"** button
3. Leave the title as "Home"
4. Paste the following content:

```markdown
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
```

5. Click **"Save Page"**

## Step 2: Now Run the Sync Script

After creating the first page:

```bash
cd /workspace
./scripts/sync-to-wiki.sh
```

This will clone the wiki and populate all remaining pages automatically.

## Alternative: Create All Pages Manually

If the script still doesn't work, create each page manually:

### Pages to Create:

1. **Home** (already created above)

2. **Project Overview**
   - Content from: `docs/project-overview.md`

3. **Comparison**
   - Content from: `docs/comparison.md`

4. **FAQ**
   - Content from: `docs/faq.md`

5. **Complete Build Guide**
   - Content from: `docs/build-guide.md`

6. **Wiring Diagrams**
   - Content from: `docs/wiring-diagrams.md`

7. **Calibration and Tuning**
   - Content from: `docs/calibration-and-tuning.md`

8. **Quick Reference Card**
   - Content from: `docs/quick-reference.md`

9. **Troubleshooting Guide**
   - Content from: `docs/troubleshooting.md`

10. **Firmware Development Guide**
    - Content from: `docs/firmware-development.md`

11. **Simulator Integration**
    - Content from: `docs/simulator-integration.md`

12. **Contributing Guidelines**
    - Content from: `CONTRIBUTING.md`

13. **G25 G27 G29 DFGT Compatibility**
    - Extract from: `docs/faq.md` (the G25/G27/G29/DFGT section)

14. **Compatible Donor Wheels**
    - Create summary page (see template in sync script)

15. **_Sidebar** (create this last)
    - See template in sync script

## Creating Each Page

For each page:

1. Go to: https://github.com/skyne/rp2040-ffb/wiki
2. Click **"New Page"**
3. Enter the page title (exactly as shown above)
4. Paste the content from the corresponding file
5. Click **"Save Page"**

## Quick Copy Commands

To easily copy content to clipboard:

```bash
# Project Overview
cat /workspace/docs/project-overview.md | pbcopy  # macOS
cat /workspace/docs/project-overview.md | xclip -selection clipboard  # Linux

# Or just read and copy:
cat /workspace/docs/project-overview.md
```

## After Setup

Once at least the Home page is created, the sync script should work for future updates.
