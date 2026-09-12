# Motor Voltage Documentation Update Summary

## Overview
Updated all documentation to accurately reflect motor voltage specifications and introduce experimental 36V upgrade path.

---

## Key Changes

### 1. Stock Voltage Clarification (24V)
**Before:** Documentation mentioned "12-24V" as acceptable range  
**After:** Clear specification that stock motors are rated for 24V

**Rationale:**
- G920/G923 stock motors designed for 24V
- Stock PSU provides 24V
- Eliminates confusion about optimal voltage

### 2. Free PSU Upgrade Path
**New emphasis:** Builders can reuse stock G920/G923 24V power supply

**Benefits:**
- **$0 cost** (already included with donor wheel)
- Correct voltage rating (24V)
- Properly sized current capacity (5A+)
- OEM quality and reliability

**Documentation updates:**
- BOM: Changed power supply from "$30-50" to "$0-50"
- Multiple sections now highlight reusing stock PSU
- Safety sections clarified to use 24V as baseline

### 3. Experimental 36V Upgrade (Under Testing)

Added comprehensive new FAQ section: **"Can I run the motors at 36V for more torque?"**

#### Technical Details:
- **Theory:** BTS7960 drivers support up to 43V, ~50% more torque potential
- **Status:** EXPERIMENTAL (testing planned, not validated yet)
- **Recommendation:** Stick with 24V for reliable operation

#### Required Modifications for 36V:
1. **Motor cooling:**
   - Bolt-on aluminum heatsinks
   - Thermal paste/pads
   - Airflow path to heatsinks

2. **Driver cooling:**
   - 40mm fans on BTS7960 modules
   - 12V fan rail (separate from motor power)
   - Direct airflow to FET heatsinks

3. **Temperature monitoring:**
   - Infrared thermometer checks
   - Motors: <60°C continuous
   - Drivers: <70°C continuous

4. **Power supply:**
   - 36V PSU, 5A minimum
   - **DO NOT use stock 24V PSU!**

#### Safety Warnings:
- ⚠️ Motor lifespan reduction
- ⚠️ Increased gear wear
- ⚠️ Driver failure risk if cooling inadequate
- ⚠️ Fire risk without monitoring

#### Community Involvement:
- Testing to be conducted by project maintainer
- Results will be published
- Community members can help test (contact maintainer first)
- Document and share all temperature data

---

## Files Updated

### Main Repository (`/workspace/docs/`)
1. **build-guide.md**
   - BOM cost table (PSU: $0-50)
   - Safety warnings updated to 24V baseline
   - Power domain diagrams updated
   - Troubleshooting section voltage references

2. **faq.md**
   - Complete PSU section rewrite (3 options: stock/new/experimental)
   - New comprehensive 36V upgrade FAQ section (~60 lines)
   - FFB strength section updated with cooling requirements

3. **wiring-diagrams.md**
   - ASCII diagrams updated to 24V baseline
   - Power rail specifications updated
   - Voltage measurement table updated

4. **project-overview.md**
   - Safety checklist updated
   - Build timeline power-on step clarified
   - Risk mitigation section updated

5. **quick-reference.md**
   - Troubleshooting checklist voltage updated
   - Safety warnings updated

6. **comparison.md**
   - BOM comparison table updated ($0 PSU option)

7. **troubleshooting.md**
   - Voltage measurement references updated

### GitHub Wiki (`/tmp/rp2040-ffb.wiki/`)
All corresponding wiki pages updated with identical changes:
- Complete-Build-Guide.md
- FAQ.md
- Wiring-Diagrams.md
- Project-Overview.md
- Quick-Reference-Card.md

---

## Voltage Reference Changes Throughout Docs

| Location | Old Reference | New Reference |
|----------|--------------|---------------|
| PSU specs | "12-24V" | "24V stock" or "24V (stock)" |
| Motor power domain | "12-24V" | "24V stock, 36V experimental" |
| Safety warnings | "12-24V to GPIO" | "24V+ to GPIO" |
| BOM PSU cost | "$30-50" | "$0-50 (reuse stock)" |
| Power supply requirements | "12V or 24V" | "24V (stock G920/G923 PSU works)" |
| Fuse rating | "5A" | "5A for 24V" |

---

## Optional Components Marking (Related Update)

While updating voltage docs, also ensured optional components are clearly marked:
- LED strips
- Rotary encoders
- Buttons
- TFT displays
- E-paper displays
- ADXL345 accelerometer

**Emphasis:** Wheel works perfectly without these (core functionality preserved)

---

## Communication Strategy

### For 24V (Stock):
- ✅ **Recommended** as default
- Emphasize it's free (reuse donor PSU)
- Known reliable, tested configuration
- No additional cooling needed

### For 36V (Experimental):
- ⚠️ **Not recommended yet**
- Clear "under testing" status
- Comprehensive safety warnings
- Detailed cooling requirements
- Call for community testing volunteers

---

## Benefits of This Update

1. **Cost Savings:** Builders realize they can reuse stock PSU ($0 vs $30-50)
2. **Clarity:** No confusion about "12V vs 24V" choice
3. **Safety:** Clear baseline voltage (24V) for all safety calculations
4. **Future-Ready:** Documents experimental 36V path without encouraging premature adoption
5. **Community Engagement:** Invites testing collaboration for 36V validation

---

## Testing Roadmap for 36V (Not Yet Started)

**Phase 1: Thermal Baseline**
- [ ] Measure stock motor temps at 24V, various duty cycles
- [ ] Measure driver temps at 24V, continuous operation
- [ ] Establish thermal headroom available

**Phase 2: Cooling Design**
- [ ] Source appropriate heatsinks (bolt-on aluminum)
- [ ] Select 40mm fans (12V, CFM requirements)
- [ ] Design airflow path (intake/exhaust)

**Phase 3: 36V Testing**
- [ ] Initial low-power 36V test (<30% duty)
- [ ] Monitor temps continuously (IR thermometer)
- [ ] Incremental duty cap increases
- [ ] Document torque gains vs temperature rise

**Phase 4: Long-Term Validation**
- [ ] 100+ hour endurance test at 36V
- [ ] Monitor for gear wear
- [ ] Check motor bearing condition
- [ ] Validate driver MOSFET health

**Phase 5: Community Documentation**
- [ ] Publish cooling design files
- [ ] Create 36V upgrade guide
- [ ] Document BOM for cooling components
- [ ] Update FAQ with validated results

---

## Git Commits

### Main Repository
```
commit 5dea170
Update documentation for motor voltage: 24V stock, 36V experimental

- Clarify stock motors are rated for 24V (not 12-24V range)
- Emphasize reusing stock G920/G923 24V PSU (free upgrade!)
- Add comprehensive FAQ section about 36V experimental upgrade
- Document required cooling (motor heatsinks + driver fans)
- Add safety warnings and temperature monitoring requirements
- Update all voltage references across docs
- Mark 36V as under testing, not recommended yet
```

### Wiki Repository
```
commit b74de5c
Update motor voltage documentation: 24V stock, 36V experimental

- Stock motors rated 24V (reuse G920/G923 PSU for free!)
- Add detailed 36V upgrade FAQ with cooling requirements
- Document motor heatsinks + driver fans needed for 36V
- Add safety warnings and temperature monitoring
- Mark 36V as experimental/under testing
```

---

## Next Steps (Future Work)

1. **Conduct 36V testing** (per roadmap above)
2. **Design cooling solutions** (heatsink mounts, fan brackets)
3. **Create 36V upgrade guide** (if testing validates it's viable)
4. **Add temperature monitoring firmware** (optional ADC input for thermistor)
5. **Community validation** (multiple builders test and report)

---

## Questions to Address in Future Testing

1. What is the actual torque increase at 36V? (measure with torque wrench)
2. How much does motor temperature rise under continuous load?
3. What is the minimum cooling required for safe 36V operation?
4. Does 36V significantly reduce motor/gear lifespan? (long-term testing)
5. Are there any firmware changes needed for 36V? (PWM frequency, current limits)
6. What is the optimal duty_cap ceiling at 36V? (vs 50% at 24V)

---

**Date:** 2026-09-12  
**Maintainer Notes:** All documentation updated. 36V path documented for transparency but marked experimental. Community can now make informed decisions about staying with reliable 24V vs helping test 36V upgrade path.
