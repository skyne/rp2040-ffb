## Description

<!-- Describe your changes in detail -->

## Type of Change

<!-- Mark relevant items with [x] -->

- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update
- [ ] Hardware change (BOM, wiring, PCB)
- [ ] Firmware protocol change (requires base + rim update)

## Testing

<!-- Describe how you tested your changes -->

### Hardware Tested

- [ ] Base MCU
- [ ] Rim MCU
- [ ] Motors (BTS7960)
- [ ] Hall sensor (MLX90363)
- [ ] Pedals
- [ ] Encoders
- [ ] LED strips
- [ ] Display (ILI9341)
- [ ] N/A (documentation/tooling only)

### Test Results

<!-- Include serial output, videos, or screenshots if relevant -->

```
Paste relevant serial output here
```

## Checklist

- [ ] My code follows the project's style guidelines
- [ ] I have performed a self-review of my code
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] I have updated documentation to reflect my changes
- [ ] My changes generate no new warnings
- [ ] I have tested on hardware (or marked N/A if not applicable)
- [ ] Firmware still fits within size limits (192 KB per MCU)
- [ ] I have updated CHANGELOG.md (for notable changes)
- [ ] Protocol changes are backward compatible OR version was incremented

## Safety Considerations

<!-- If your changes affect motor control, settings validation, or safety -->

- [ ] Motor duty caps are respected
- [ ] Settings validation added/updated
- [ ] Watchdog/timeout mechanisms still work
- [ ] N/A (no safety-critical changes)

## Related Issues

<!-- Link related issues: Fixes #123, Relates to #456 -->

## Additional Context

<!-- Add any other context, design decisions, or tradeoffs here -->
