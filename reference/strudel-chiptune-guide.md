# Strudel for Videogame-Textured Music

This guide covers synthesis fundamentals and how to use Strudel to create music with a videogame feel — not authentic retro chiptune, but the middle ground where pulse waves and 8-bit textures blend with cleaner, more modern sounds. Think Speder2: videogamey but not fully retro.

The goal isn't to sound like a NES. It's to use these timbres selectively as one element in a broader palette.

---

## Part 1: Sound Fundamentals (Refresher)

Since you built a synth in college, this should feel familiar. I'll be brief but complete.

### What is Sound?

Sound is pressure waves. A speaker pushes air back and forth. The pattern of that movement over time is the **waveform**. Your synth probably generated arrays of samples representing that waveform.

### The Four Classic Waveforms

These are the building blocks of synthesis, especially chiptune:

```
Sine        Sawtooth      Square        Triangle
   ∩           /|           ___           /\
  / \         / |          |   |         /  \
 /   \       /  |          |   |        /    \
      \∪    /   |___       |___|       /      \
```

**Sine** — Pure tone, single frequency. No harmonics. Sounds hollow/flute-like.

**Sawtooth** — Contains all harmonics (1x, 2x, 3x, 4x... the fundamental frequency). Sounds buzzy, bright, aggressive.

**Square** — Contains only odd harmonics (1x, 3x, 5x...). Sounds hollow but present. The classic "8-bit" sound.

**Triangle** — Also odd harmonics, but they fall off faster. Softer than square, sounds closer to sine but with more body.

### Harmonics (Why This Matters)

When you play a note at 440Hz (A4), you're not just hearing 440Hz. Real sounds contain **overtones**—multiples of the fundamental frequency.

- 440Hz (fundamental)
- 880Hz (2nd harmonic)
- 1320Hz (3rd harmonic)
- etc.

The *which harmonics* and *how loud they are* determines the **timbre**—why a piano sounds different from a guitar at the same note.

Chiptune's character comes from using mathematically simple waveforms (square, triangle) that have specific harmonic content.

---

## Part 2: Magical 8bit Plug 2's Features

Let's break down what this plugin actually does, then map each feature to Strudel.

### Feature 1: Oscillators with Selectable Waveforms

Magical 8bit offers:
- **Square/Pulse** (with variable duty cycle)
- **Triangle**
- **Noise** (periodic and random)
- **Sawtooth**

This is your basic "what kind of sound" selector.

### Feature 2: Duty Cycle (Pulse Width)

This is the key to the NES sound. A "square wave" is actually a special case of a **pulse wave** with 50% duty cycle.

```
50% duty (square):    25% duty:           12.5% duty:
 ___     ___           _      _            _    _
|   |   |   |         | |    | |          ||   ||
|   |___|   |___      | |____| |____      ||___||___
```

The duty cycle is what percentage of each cycle the wave is "high."

- **50%** — Classic square, hollow sound
- **25%** — Thinner, more nasal, cuts through a mix well
- **12.5%** — Very thin, reedy, good for texture or background

**Why it matters:** Different pulse widths have distinct characters. 25% has a cutting quality good for leads. 50% is more hollow and neutral. Experiment to find what fits each part.

### Feature 3: ADSR Envelope

Controls how the sound evolves over time:

```
     /\
    /  \____
   /        \
  /          \
 A   D   S   R

A = Attack  — Time to reach full volume (0 = instant, snappy)
D = Decay   — Time to fall to sustain level
S = Sustain — Volume level while note is held
R = Release — Time to fade after note ends
```

Punchy videogame sounds typically use:
- Very short attack (0-10ms) — notes start immediately
- Short decay
- Medium-high sustain
- Short release

But you're not limited to this. Longer attack creates pads, longer decay creates plucks.

### Feature 4: Pitch Sweep / Bend

A pitch that slides up or down at the start of a note. Useful for:
- Drum sounds (high-to-low pitch sweep on noise)
- "Laser" effects
- Bass punch (slight downward sweep)

### Feature 5: Vibrato

Pitch modulation—the frequency wobbles up and down slightly. Controlled by:
- **Rate** — How fast the wobble (in Hz)
- **Depth** — How far the pitch moves (in semitones or cents)

### Feature 6: Lo-Fi Character (Optional)

Bit crushing and sample rate reduction add grit:
- **Bit crushing** — Reduces amplitude resolution, adds harshness
- **Sample rate reduction** — Creates aliasing artifacts

**For your style, you probably don't want much of this.** Speder2's sound isn't degraded — it's clean pulse waves mixed with other elements. Use these sparingly or not at all.

---

## Part 3: Strudel Equivalents

Now let's map each feature to Strudel code.

### Oscillators

```javascript
// Basic waveforms
note("c4").sound("square")      // Square wave
note("c4").sound("triangle")    // Triangle wave
note("c4").sound("sawtooth")    // Sawtooth
note("c4").sound("sine")        // Sine (smooth, pure)

// Noise
note("c4").sound("white")       // Hard, bright noise
note("c4").sound("pink")        // Medium noise
note("c4").sound("brown")       // Soft, darker noise
```

### Pulse Width (Duty Cycle)

Strudel recently added a `pulse` oscillator with variable width:

```javascript
// Pulse oscillator with width control
// 0.5 = square wave, lower = thinner pulse
note("c4").sound("pulse").pw(0.5)   // Square (50%)
note("c4").sound("pulse").pw(0.25)  // Thin, nasal (25%)
note("c4").sound("pulse").pw(0.125) // Very thin (12.5%)

// You can pattern the pulse width
note("c4 e4 g4 c5").sound("pulse").pw("<0.5 0.25 0.125 0.25>")

// Or modulate it continuously
note("c3").sound("pulse").pw(sine.range(0.1, 0.5).slow(4))
```

**Note:** The `pw` parameter may be called `pulsewidth` or use `n` in some contexts. If `pw` doesn't work, try:
```javascript
note("c4").sound("pulse").n(0.25)
```

### Envelope (ADSR)

```javascript
// Full ADSR control
note("c4 e4 g4").sound("square")
  .attack(0.001)    // Nearly instant (1ms)
  .decay(0.1)       // Quick decay
  .sustain(0.5)     // Half volume sustain
  .release(0.1)     // Short release

// Punchy, immediate
note("c4").sound("square")
  .attack(0)
  .decay(0.05)
  .sustain(0.8)
  .release(0.1)

// Plucky sound (no sustain)
note("c4").sound("square")
  .attack(0)
  .decay(0.2)
  .sustain(0)
  .release(0.1)
```

### Pitch Effects

```javascript
// Vibrato
note("c4").sound("square")
  .vib(6)       // 6 Hz vibrato rate
  .vibmod(0.5)  // 0.5 semitone depth

// Faster, shallower
note("c4").sound("square")
  .vib(8)
  .vibmod(0.2)
```

For pitch sweeps, Strudel doesn't have a direct equivalent to Magical 8bit's sweep. Workaround:

```javascript
// Simulate pitch bend with note patterns
note("[c5 c4]").sound("square").attack(0).decay(0.1)
```

### Lo-Fi Effects (Use Sparingly)

These add grit but you probably don't need them for Speder2-style sound:

```javascript
// Bit crush (lower = more crushed)
// 16 = full quality, 1 = extremely crushed
note("c4 e4 g4").sound("square").crush(4)

// Sample rate reduction (higher = more reduced)
// 1 = original, 16 = very lo-fi
note("c4 e4 g4").sound("square").coarse(8)
```

**Note:** `coarse` only works in Chromium-based browsers.

---

## Part 4: Putting It Together

Here's an example with clean pulse waves (no lo-fi processing):

```javascript
// Lead melody with pulse wave
note("c4 e4 g4 e4 c4 g3 c4 e4")
  .sound("pulse")
  .pw(0.25)
  .attack(0.001)
  .decay(0.1)
  .sustain(0.6)
  .release(0.1)
  .vib(6)
  .vibmod(0.2)
```

```javascript
// Bass line with triangle wave
note("c2 c2 g2 g2 a2 a2 e2 e2")
  .sound("triangle")
  .attack(0)
  .decay(0.05)
  .sustain(0.8)
  .release(0.05)
```

```javascript
// Mixing: pulse lead + triangle bass + sine pad
stack(
  // Lead (videogamey texture)
  note("c5 e5 g5 e5").sound("pulse").pw(0.25)
    .attack(0).decay(0.1).sustain(0.5).release(0.1),

  // Bass
  note("c3 c3 g2 g2").sound("triangle")
    .attack(0).decay(0.05).sustain(0.8).release(0.05),

  // Pad (clean, modern)
  note("c4 e4 g4").sound("sine")
    .attack(0.3).decay(0.1).sustain(0.4).release(0.5)
)
```

The key is mixing: pulse/square/triangle for the videogame texture, sine/sawtooth or samples for everything else.

---

## Part 5: Strudel Limitations

1. **Pitch sweep envelopes** — Magical 8bit lets you define a pitch envelope (start high, sweep down). Strudel's vibrato is cyclic, not a one-shot envelope. Workaround: use rapid note patterns.

2. **Samples** — For fuller mixes, you'll want samples (drums, pads, etc.) alongside the synths. Strudel supports sample loading.

3. **Complex layering** — For production-quality mixes, you may eventually want a DAW or SuperCollider + TidalCycles for more control over mixing and effects.

---

## Part 6: Learning Path

1. **Start with the Strudel tutorial** at https://strudel.cc/workshop/getting-started
   - Learn the pattern syntax (mini-notation)
   - Get comfortable with basic sounds

2. **Experiment with oscillators**
   - Play the same melody with square, triangle, pulse, sine
   - Try different pulse widths, hear the difference
   - Notice which timbres feel "videogamey" and which feel "normal"

3. **Master envelopes**
   - Make the same note sound plucky, sustained, or pad-like
   - This shapes the feel more than the waveform choice

4. **Practice mixing**
   - Combine pulse/square leads with cleaner sounds
   - The videogame texture comes from selective use, not everything being 8-bit

5. **Study Speder2's actual songs**
   - Listen for which elements use pulse/square vs cleaner sounds
   - Notice how the videogame elements sit in the mix

---

## Quick Reference

| Magical 8bit Feature | Strudel Equivalent |
|---------------------|-------------------|
| Square wave | `.sound("square")` |
| Pulse with duty cycle | `.sound("pulse").pw(0.25)` |
| Triangle | `.sound("triangle")` |
| Noise | `.sound("white")` / `"pink"` / `"brown"` |
| Attack | `.attack(0.001)` |
| Decay | `.decay(0.1)` |
| Sustain | `.sustain(0.5)` |
| Release | `.release(0.1)` |
| Vibrato rate | `.vib(6)` |
| Vibrato depth | `.vibmod(0.5)` |
| Bit crush | `.crush(4)` (1-16, lower = more) |
| Sample rate reduction | `.coarse(8)` (Chromium only) |

---

## Sources

- [Strudel Synths Documentation](https://strudel.cc/learn/synths)
- [Strudel Effects Documentation](https://strudel.cc/learn/effects)
- [Strudel Pulse Oscillator PR](https://github.com/tidalcycles/strudel/pull/1304)
- [Strudel Getting Started](https://strudel.cc/workshop/getting-started)
