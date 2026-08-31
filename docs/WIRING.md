# HUB75 Wiring Guide

This document explains the physical wiring, and the settings that let one firmware drive panels it
was not built for.

## Start Here

✅ Use a **standard HUB75 cable, unmodified**. One pin map covers both boards — see
[The Pin Map](#the-pin-map).

**Nothing about the panel needs a custom firmware build.** The **Hardware** tab in the web interface
exposes it all as settings, applied on reboot:

| Setting | Use it when |
|---|---|
| RGB channel order | Colours are wrong — orange looks pink, sky looks teal, yellow looks violet. See [Wrong Colours Are a Setting](#wrong-colours-are-a-setting-not-a-rewire) |
| Panel driver chip | The panel stays blank or ghosts (some panels need an FM6126A/ICN2038S-style init) |
| Custom pin map | You wired to different GPIOs, or colours are wrong on only one half of the panel |
| Panel arrangement | Which panels you attached — see [Panel Geometry](#panel-geometry) |

The tab also carries a **test pattern** (three bars labelled R, G and B) and a **restore built-in
wiring** button. Both work in setup (AP) mode, so a panel left dark by a bad setting can always be
recovered without a USB cable.

## The Pin Map

**There is one pin map, and it is the MatrixPortal's.** Those GPIOs are fixed in the MatrixPortal's
PCB, and the firmware ships them as its compiled default for *both* boards — so a generic ESP32-S3
wired to match behaves identically, and any HUB75 tutorial or rainbow cable works on either.

```
Cable Color   HUB75 Label   →   ESP32-S3 GPIO
────────────────────────────────────────────────
1.  Brown      R1           →   GPIO 42
2.  Orange     G1           →   GPIO 41
3.  Yellow     B1           →   GPIO 40
4.  Green      GND          →   GND
5.  Blue       R2           →   GPIO 38
6.  Purple     G2           →   GPIO 39
7.  Gray       B2           →   GPIO 37
8.  White      E            →   GPIO 21
9.  Black      A            →   GPIO 45
10. Red        B            →   GPIO 36
11. Orange     C            →   GPIO 48
12. Yellow     D            →   GPIO 35
13. Green      CLK          →   GPIO 2
14. Blue       LAT          →   GPIO 47
15. Purple     OE           →   GPIO 14
16. White      GND          →   GND
```

Cable colours are the common rainbow-ribbon convention and repeat down the run — trust the **HUB75
label**, not the colour, if a cable disagrees.

### MatrixPortal ESP32-S3

Nothing to wire. Plug the panel's cable into the built-in HUB75 connector.

### ESP32-S3 N8R2 DevKit (Generic)

Wire to the table above, pin for pin.

> **3.3V Logic Warning:** The ESP32-S3 outputs 3.3V signals, but HUB75 panels expect 5V logic. This works with many panels but is out of spec and may cause issues with some panel batches. For guaranteed reliability, add a 74AHCT245 level shifter between the ESP32-S3 and the panel's data/control lines. The MatrixPortal S3 includes these level shifters on-board.

## Wrong Colours Are a Setting, Not a Rewire

The six colour pins — `R1 G1 B1 R2 G2 B2` — carry no meaning in the wire. Which colour arrives on
which pin is decided by the **RGB channel order**, a setting on the **Hardware** tab. So if your
colours come out wrong, **do not unplug anything**: change the order and reboot.

What "wrong" looks like: orange renders pink, sky renders teal, yellow renders violet — the image is
sharp and correctly positioned, only the hues are off.

1. **Hardware → Test pattern.** Three bars labelled R, G and B.
2. If a letter sits on the wrong colour, pick the order that matches what you see.
3. Save and reboot. Repeat if needed — there are only six permutations and the test pattern
   identifies the right one immediately.

This is also why the two boards need no separate wiring: the MatrixPortal's connector happens to
present green and blue transposed, so its default order is `RBG` where the generic board's is `RGB`.
Same pins, different label — and both are just the factory value of a setting you can change.

The default is keyed on **panel height**, not display size: 64-high panels (`2x64`, or one 128x64
module) want the standard `RGB` on a MatrixPortal, while 32-high panels want `RBG`. `4x32` is a
128x64 display built from 32-high panels, so it keeps `RBG`. Swap panel heights and the channel order
is the setting to revisit.

### Which setting fixes which problem

Every GPIO is settable, so **a sound but unconventional wiring never needs redoing** — it needs
describing. Two settings, matched to two different symptoms:

| What you see | Fix |
|---|---|
| Colours wrong, **whole panel** consistently | **RGB channel order.** One permutation, six options, test pattern identifies it in seconds |
| Colours wrong on the **top half only**, or bottom only | **Custom pin map** — swap those two GPIO values |
| Blank, garbled, half-lit, or interleaved rows | Not a colour fault. Check the address/control wiring (`A B C D E`, `CLK`, `LAT`, `OE`), the [panel arrangement](#panel-geometry), and the panel driver chip |

**Why the split:** the channel order is a *single* permutation applied to both triplets — `R1 G1 B1`
and `R2 G2 B2` are relabelled together, because a panel expects one colour convention throughout. So
it fixes any swap affecting both halves the same way, and cannot express one that affects only one
half. That case is not a dead end, it is simply the other setting's job: the custom pin map sets all
fourteen GPIOs independently, so crossing `G1`/`B1` back is two edited numbers.

Prefer the channel order where it applies. It is one choice out of six with an on-panel test pattern,
where the pin map is fourteen numbers you have to get right — and a map that fails validation is
discarded at boot in favour of the built-in one, so a typo silently gets you the default rather than
what you typed.

### The one thing settings cannot fix

A wire that is not actually connected, or is shorted. Settings can describe any *working* wiring; they
cannot conjure a signal that never arrives. If a pin reads dead no matter which GPIO you assign it to,
it is the cable or the joint.

## Standard HUB75 Pinout Reference

For reference, the standard HUB75 2×8 connector pinout is:

```
Top Row (Pins 1-8):
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ R1 │ G1 │ B1 │GND │ R2 │ G2 │ B2 │ E  │
└────┴────┴────┴────┴────┴────┴────┴────┘

Bottom Row (Pins 9-16):
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ A  │ B  │ C  │ D  │CLK │LAT │ OE │GND │
└────┴────┴────┴────┴────┴────┴────┴────┘
```

## Panel Geometry

Panel arrangement is a **setting**, not a separate firmware build. One binary per board drives every
arrangement; pick yours under **Hardware → Panel arrangement** and the device reboots into it. It
sits on the Hardware tab with the wiring because both describe the panels you attached rather than
what is drawn on them, and both are read only at boot — so changing them together costs one reboot.
That also makes it reachable in **setup (AP) mode**, which matters when a wrong arrangement is why
the panel is unreadable in the first place.

| Setting | Panels | Pixels |
|---|---|---|
| `128x32 - 2x 64x32 panels, chained` | 2x 64x32 side by side | 128x32 |
| `128x64 - 4x 64x32 panels, 2x2 grid` | 4x 64x32 in a 2x2 serpentine | 128x64 |
| `128x64 - 2x 64x64 chained, or one 128x64 module` | 2x 64x64 side by side, **or** a single 128x64 module | 128x64 |

**Two of the three are 128x64, so the pixel size does not tell you which one you have — count the
panels.** Picking the wrong one gives a scrambled or half-lit display; change the setting and reboot,
nothing is damaged.

### A single 128x64 module uses the 2x 64x64 setting

If your display is **one** 128x64 module with a single HUB75 connector rather than two chained
64x64 panels, choose the **`2x 64x64 chained, or one 128x64 module`** setting. There is no separate
option, and none is needed.

The reason is that the driver only ever uses the panel width and the chain length **multiplied
together**. Both descriptions produce the same numbers:

| | width x height, chain | pixels per row | rows per frame |
|---|---|---|---|
| Two 64x64 chained | 64 x 64, chain 2 | 128 | 32 |
| One 128x64 module | 128 x 64, chain 1 | 128 | 32 |

Same framebuffer, same address lines, same bounds — the driver cannot tell them apart. The only
physical difference is the cabling: one connector instead of two with a ribbon between them. The
shift-register chain the panel sees is the same length either way.

**The one exception is scan type, not panel count.** A minority of 128x64 modules use a
non-standard internal scan map (typically 1/16-scan "outdoor" panels) and need a scan remap that
this firmware does not currently apply. You cannot identify these from the pixel
dimensions. The symptom is distinctive: **interleaved or garbled rows** — the image is there but
shredded across the panel — as opposed to wrong colours (channel order) or the top and bottom
halves showing the same content (the E address line not reaching the panel). If you see that,
open an issue with a photo; it needs firmware support, not a setting.

## Power Connections

**Measured power requirements:** SpojBoard draws 0.3-0.7A during normal operation, with transient peaks up to 1.6A during WiFi connection, OTA updates, or boot sequences.

### Connection Options

**MatrixPortal S3:**
- Power via MatrixPortal's **USB-C port** (5V 2A+ adapter) **and** connect display power via **screw terminals** (5V/GND)
- The MatrixPortal passes USB-C power through to the LED panels via screw terminals
- Use any quality USB-C phone charger (2A or higher)

**ESP32-S3 N8R2:**
- Power LED panels directly via their screw terminals from separate 5V supply (2A+ minimum, 3A recommended)
- Power ESP32 board via USB for programming/serial (can remain connected)
- **Important**: Connect ESP32 GND to panel GND for common ground reference

### Power Supply Recommendations

| Supply Type | Rating | Suitability | Notes |
|-------------|--------|-------------|-------|
| **USB-C Wall Adapter** | 5V 2A (10W) | ✅ Adequate | Covers all normal operation + transient peaks |
| **USB-C Wall Adapter** | 5V 3A (15W) | ✅ Recommended | Extra safety margin for simultaneous loads |
| **USB-C Wall Adapter** | 5V 1A (5W) | ❌ Insufficient | Will cause brownouts during WiFi activity |
| **Screw Terminal PSU** | 5V 2-3A | ✅ For ESP32-S3 N8R2 | Use with generic dev boards |

### Compatible Power Supplies

**USB-C (recommended for MatrixPortal S3):**
- Apple 12W USB-C adapter (2.4A)
- Anker PowerPort III Nano (2A+)
- Samsung EP-TA20 (2A)
- Any quality USB-C phone charger rated 2A or higher

**Screw Terminal Supplies (for ESP32-S3 N8R2):**
- Mean Well RS-15-5 (5V 3A)
- Any regulated 5V 2-3A DC power supply with screw terminal output

⚠️ **Never power the panels from the ESP32's 5V pin** - it cannot supply enough current and will cause the board to reset.

### Understanding Power Specifications

Published HUB75 panel specifications often cite 4-6A current draw at full brightness with all LEDs displaying white. However, SpojBoard's real-world usage (text display, mixed colors, brightness 90 default) draws significantly less power:

- **Theoretical maximum** (all white, brightness 255): ~4-6A per manufacturer specs
- **Measured actual usage** (text display, brightness 90): ~0.3A average
- **Measured worst case** (text display, brightness 255): ~0.7A peak

The difference is due to:
1. Text displays use far fewer LEDs than solid colors
2. Mixed colors (red/green/blue) draw less current than white
3. Default brightness (90/255 = 35%) significantly reduces power
4. Most pixels are black (off) in typical departure display

A 2A supply provides 2.8× safety margin over measured peaks, while a 3A supply provides 4.3× margin.

## Troubleshooting

**Colors are wrong (orange looks pink, sky looks teal):**
- This is a channel-order problem, not a wiring fault. Open the **Hardware** tab, press **Test
  pattern**, and read the three bars: if the letter `R` is not on the red bar, change **RGB channel
  order** and reboot
- `RGB` is the standard cable order; `RBG` is what a MatrixPortal needs with 64x32 panels
- **If only the top or bottom half has wrong colours**, the channel order cannot fix it — it applies
  one permutation to both triplets. Swap the two offending GPIOs in the **custom pin map** instead.
  Still a settings change, not a rewire
- If the panel is hand-wired, check that all wires are firmly connected and that ESP32 and panel
  share a common ground

**Panel doesn't light up:**
- Check panel has separate 5V power supply connected
- Verify GND connection between ESP32 and panel
- Check all 16 signal wires are connected

**Flickering or artifacts:**
- Ensure common ground between ESP32 and panel
- Check for loose connections
- Verify power supply can provide sufficient current (2A minimum, 3A recommended — the same for every panel arrangement)
- If using generic ESP32-S3, consider adding a 74AHCT245 level shifter — marginal 3.3V signals can cause instability with some panels
