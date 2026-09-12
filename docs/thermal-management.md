# Thermal Management Guide

## Overview

Proper thermal management is critical for reliability, especially at higher power levels or with the experimental 36V upgrade. This guide covers temperature monitoring, cooling solutions, and thermal protection strategies.

---

## Temperature Limits

### Safe Operating Temperatures

| Component | Maximum Continuous | Maximum Peak | Action Required |
|-----------|-------------------|--------------|-----------------|
| **Motors** | 60°C (140°F) | 80°C (176°F) | Add heatsinks + fans |
| **BTS7960 Drivers** | 70°C (158°F) | 85°C (185°F) | Add heatsinks + fans |
| **Pico MCU** | 70°C (158°F) | 85°C (185°F) | Improve airflow |
| **Power Supply** | Per spec | Per spec | Ensure ventilation |
| **Wiring** | 60°C (140°F) | 80°C (176°F) | Check connections |

**⚠️ Warning:** Exceeding these temperatures can cause:
- Permanent motor/driver damage
- Reduced component lifespan
- Thermal runaway
- Fire hazard

---

## Monitoring Temperature

### Method 1: Infrared Thermometer (Recommended)

**Equipment:**
- Non-contact IR thermometer ($15-30)
- Point at motor housing, driver heatsinks

**Procedure:**
1. Run wheel at typical load (50% duty cap) for 10 minutes
2. Measure motor housing temperature
3. Measure BTS7960 heatsink temperature
4. Record readings every 5 minutes

**Target:** <60°C motors, <70°C drivers

### Method 2: Contact Thermocouples

**Equipment:**
- K-type thermocouple probe
- Multimeter with temperature mode
- Kapton tape to secure probe

**Procedure:**
1. Tape thermocouple to motor housing (use thermal paste)
2. Route wire safely away from moving parts
3. Monitor during use

**Advantage:** Continuous monitoring while driving

### Method 3: Thermal Camera (Advanced)

**Equipment:**
- FLIR/Seek thermal camera ($200-400)
- Smartphone attachment models available

**Use case:**
- Identify hot spots
- Verify airflow effectiveness
- Debug thermal issues

---

## Stock 24V Configuration

### Baseline Thermal Profile

**Typical temperatures at 24V, 50% duty cap:**
- Motors: 40-50°C (warm to touch)
- Drivers: 50-60°C (warm to touch)
- PSU: 40-50°C

**Cooling required:** Passive (stock heatsinks on BTS7960)

### When to Add Cooling (24V)

**Add heatsinks if:**
- Running >50% duty cap continuously
- Ambient temperature >25°C (77°F)
- Enclosed installation (limited airflow)
- Motors reaching 55°C+ during normal use

---

## Experimental 36V Configuration

**⚠️ IMPORTANT:** 36V is experimental and requires active cooling!

### Required Cooling for 36V

#### 1. Motor Cooling

**Heatsink Requirements:**
- Material: Aluminum (copper is better but heavier)
- Size: 40×40×11mm or larger per motor
- Attachment: Thermal adhesive or epoxy
- Contact: Full surface contact with motor housing

**Recommended parts:**
- Generic aluminum heatsink (eBay/AliExpress: "40mm heatsink")
- Thermal adhesive: Arctic Alumina or similar
- Cost: ~$5-10 per motor

**Installation:**
1. Clean motor housing with isopropyl alcohol
2. Apply thin layer of thermal adhesive
3. Press heatsink firmly to motor
4. Allow 24h cure time before use
5. Ensure clearance for rotation

#### 2. BTS7960 Driver Cooling

**Active cooling required:**
- 40mm × 10mm DC fans (12V recommended)
- Mount directly over driver heatsinks
- Airflow: Minimum 10 CFM per driver

**Fan mounting:**
```
    Fan (12V 40mm)
         ↓ airflow
    ┌─────────────┐
    │  BTS7960    │
    │  [Heatsink] │
    └─────────────┘
```

**Wiring:**
- Power fans from 12V rail (buck converter from 36V PSU)
- Add 100Ω resistor if fans too loud (reduces speed)
- Connect in parallel for both drivers

**Recommended fans:**
- Noctua NF-A4x10 (quiet, high quality, $15)
- Generic 12V 40mm fan (louder, $2-5)

#### 3. Airflow Path

**Enclosure design:**
- Intake vents at bottom/front
- Exhaust vents at top/rear
- Natural convection helps
- Don't block fan airflow

**Ideal setup:**
```
Exhaust vent (top)
      ↑
      │ Hot air rises
      │
   [Drivers + Fans]
   [Motors + Heatsinks]
      │
      ↓ Cool air intake
Intake vent (bottom)
```

---

## Thermal Protection Strategies

### Firmware-Based Protection

**Current status:** Basic watchdog protection

**Future enhancements:**
1. **Temperature sensor input:**
   - Add NTC thermistor to motor housing
   - Wire to ADC pin (GP29 available)
   - Firmware reads temp every 100ms

2. **Thermal throttling:**
   - Reduce duty cap if temp >55°C
   - Disable motors if temp >70°C
   - Visual/audio warning in GUI

3. **Cool-down timer:**
   - Enforce rest period after high load
   - Track cumulative heat buildup

### Hardware Protection

**Thermal cutoff switch:**
- NC (normally closed) thermal switch on motor
- 60°C trip temperature
- Wired in series with motor enable
- Cost: ~$2-5

**Over-temperature indicator:**
- LED on rim connected to thermal switch
- Lights when motors too hot
- Visual warning to reduce load

---

## Testing & Validation

### Initial Thermal Baseline Test

**Before using wheel in games:**

1. **Setup:**
   - Fresh start (motors at room temp)
   - IR thermometer ready
   - Timer/stopwatch
   - Notepad for logging

2. **Test procedure:**
   ```
   Time    Duty Cap    Motor Temp    Driver Temp    Notes
   0:00    0%          25°C          25°C           Start
   0:30    10%         26°C          27°C           Gentle
   1:00    20%         28°C          30°C           
   2:00    30%         32°C          35°C           
   5:00    30%         38°C          42°C           
   10:00   30%         42°C          48°C           
   15:00   30%         44°C          50°C           Stable
   ```

3. **Analysis:**
   - Temperature should stabilize after 10-15 min
   - Note maximum safe duty cap for your setup
   - If temps exceed 60°C, add cooling

### Stress Test (36V Only)

**Only after cooling installed:**

1. Run at 50% duty cap for 30 minutes
2. Monitor temps every 5 minutes
3. Motors should stay <55°C, drivers <65°C
4. If temps rising continuously, insufficient cooling

### Long-Term Monitoring

**Weekly checks:**
- Feel motors after 1-hour session
- Should be warm (40-50°C), not hot (>60°C)
- Check for unusual smells (burning = stop immediately)

**Monthly checks:**
- Deep thermal test with IR thermometer
- Verify cooling effectiveness hasn't degraded
- Clean dust from heatsinks/fans

---

## Troubleshooting

### Problem: Motors Getting Too Hot

**Symptoms:**
- Motors uncomfortable to touch (>60°C)
- FFB weakens after extended use
- Burning smell

**Solutions:**
1. **Reduce duty cap:**
   - Lower `duty_cap` setting by 10-20%
   - Trade power for temperature

2. **Add motor heatsinks:**
   - Even at 24V, larger heatsinks help
   - Bolt-on or adhesive mount

3. **Check wiring resistance:**
   - Loose connections cause heat
   - Crimp terminals properly
   - Use adequate wire gauge (18 AWG minimum)

4. **Improve enclosure airflow:**
   - Add ventilation holes
   - Keep away from heat sources

### Problem: Drivers Overheating

**Symptoms:**
- BTS7960 heatsinks >70°C
- Intermittent motor cutouts
- Thermal shutdown

**Solutions:**
1. **Add heatsinks:**
   - Larger heatsinks for BTS7960 modules
   - Thermal paste between chip and heatsink

2. **Add active cooling:**
   - 40mm fan over each driver
   - 12V fans, always-on

3. **Check power supply:**
   - Ensure adequate current rating
   - Voltage drop under load causes excess heat

4. **Reduce PWM frequency (if custom firmware):**
   - Lower frequency = less switching losses
   - Trade: potentially more motor noise

### Problem: Uneven Heating

**Symptoms:**
- One motor hotter than the other
- One driver significantly hotter

**Causes:**
1. **Mechanical binding:**
   - One motor working harder
   - Check gear mesh, alignment

2. **Electrical imbalance:**
   - Check motor resistance (should match)
   - Verify wiring symmetry

3. **Driver mismatch:**
   - One driver may be defective
   - Swap drivers to isolate

---

## Cooling Solutions by Configuration

### Budget Build (24V, <40% duty cap)

**Cooling:**
- Stock BTS7960 heatsinks
- Natural convection
- Cost: $0

**Monitoring:**
- Touch test every session
- Monthly IR thermometer check

### Standard Build (24V, 40-50% duty cap)

**Cooling:**
- Stock BTS7960 heatsinks
- Optional: Larger heatsinks on drivers
- Cost: $5-10

**Monitoring:**
- IR thermometer after first few sessions
- Touch test regularly

### High-Performance Build (36V experimental)

**Cooling:**
- Motor heatsinks (adhesive mount)
- 2× 40mm fans on drivers
- 12V fan power rail (buck converter)
- Ventilated enclosure
- Cost: $30-50

**Monitoring:**
- NTC thermistor on motor (optional)
- IR thermometer every session initially
- Thermal cutoff switch recommended

---

## 36V Cooling Parts List

| Part | Quantity | Cost | Where to Buy |
|------|----------|------|--------------|
| Aluminum heatsink 40×40×11mm | 2 | $5-10 | eBay, AliExpress |
| Thermal adhesive (Arctic Alumina) | 1 | $8 | Amazon |
| 40mm × 10mm 12V DC fan | 2 | $4-10 | Amazon, Noctua |
| Buck converter 36V→12V, 1A | 1 | $3-5 | Amazon, AliExpress |
| NTC thermistor 10kΩ (optional) | 2 | $2 | Amazon, Digi-Key |
| Thermal cutoff switch 60°C NC (optional) | 2 | $5 | Amazon, eBay |
| **Total** | | **$30-50** | |

---

## Best Practices

### Do's ✅

- ✅ Measure temperatures before first gaming session
- ✅ Start with conservative duty cap, increase gradually
- ✅ Ensure good ventilation around wheel base
- ✅ Use thermal paste/adhesive for heatsink contact
- ✅ Monitor temps regularly for first month
- ✅ Keep cooling surfaces clean (dust reduces effectiveness)

### Don'ts ❌

- ❌ Don't run 36V without active cooling
- ❌ Don't exceed 60°C on motors during normal use
- ❌ Don't block fan airflow with cables/objects
- ❌ Don't use thermal pads alone (adhesive better for motors)
- ❌ Don't ignore burning smells (stop immediately)
- ❌ Don't operate with damaged/missing heatsinks

---

## Future Enhancements

**Planned firmware features:**

1. **Temperature telemetry:**
   - Read motor temp sensor (NTC thermistor)
   - Display in GUI
   - Log temperature history

2. **Adaptive thermal management:**
   - Auto-reduce duty cap if overheating
   - Visual warnings in GUI
   - Thermal protection curves

3. **Fan control:**
   - PWM fan speed control
   - Temperature-based fan curves
   - Silent mode (lower RPM when cool)

---

## Summary

### Stock 24V Configuration
- **Cooling:** Passive (stock heatsinks)
- **Monitoring:** Periodic touch test
- **Safe duty cap:** 40-50%
- **Extra cost:** $0

### 36V Experimental Configuration
- **Cooling:** Active (heatsinks + fans)
- **Monitoring:** IR thermometer or thermistor
- **Safe duty cap:** TBD (testing in progress)
- **Extra cost:** $30-50

**Remember:** When in doubt, measure! An IR thermometer is your best tool for safe operation.

---

**Last updated:** 2026-09-12  
**Status:** 24V tested, 36V under validation
