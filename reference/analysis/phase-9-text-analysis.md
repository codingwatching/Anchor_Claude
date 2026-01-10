# Phase 9: Text Module Analysis

## C vs YueScript Separation

### C Side — Font Infrastructure & Glyph Drawing

The C side provides the **basic font loading and glyph rendering primitives**. It doesn't know about effects, tags, or rich text — it just draws glyphs efficiently.

**1. Font Loading**
```c
// Load TTF via stb_truetype, bake glyphs to texture atlas
font_load(name, path, size)

// Optionally: bitmap font support (spritesheet + metrics JSON)
font_load_bitmap(name, image_path, metrics_path)
```
- Bake glyphs to texture atlas on load
- Store per-glyph metrics: advance width, bearing, UV coords in atlas, size
- Font registry by name (like layers)
- Default font loaded at startup (for error screen)

**2. Glyph Metrics Queries**
```c
font_get_text_width(font_name, text)    // Width in pixels
font_get_height(font_name)              // Line height
font_get_glyph_metrics(font_name, char) // {width, height, advance, bearing_x, bearing_y}
```
- Used by YueScript for layout calculations
- These are called during text formatting, not per-frame

**3. Drawing Functions**
```c
// Simple text (for debug, error screen, simple UI)
layer_draw_text(layer, text, font_name, x, y, color)

// Single glyph (for effect system — called per character)
layer_draw_glyph(layer, char, font_name, x, y, r, sx, sy, color)
// or alternatively using draw_text with single char
layer_draw_text_lt(layer, char, font_name, x, y, r, sx, sy)
```
- `layer_set_color(color)` already exists, works before draw calls
- Glyphs batch with sprites (same texture atlas approach)
- Flash support via existing vertex attribute

**4. UTF-8 Handling**
- C needs to correctly decode UTF-8 to get codepoints
- Or: Keep it simple, let Lua/YueScript iterate UTF-8, pass single codepoints to C

---

### YueScript Side — Rich Text & Effects

The YueScript side handles **all text intelligence**: parsing, layout, effects, and per-character object management.

**1. Tag Parsing**
```yuescript
-- Parse: "[shaking text](shake=4,4;color=red)"
-- Into: characters with effects attached
text_parse: =>
  -- Uses markdown-style [text](effect=arg1,arg2;effect2=arg) syntax
  -- Each character becomes an object with .effects list
```

**2. Text Layout/Formatting**
```yuescript
text_format: =>
  -- Word wrapping (look-ahead to next space)
  -- Alignment: left, center, right, justify
  -- Line breaks: explicit '|' or automatic wrapping
  -- Sets per-character: x, y, w, h, r, sx, sy, ox, oy, line, i
```

**3. Character Objects**
Each character is a **full Anchor tree object**:
```yuescript
tc = object 'text_character', {
  c: 'A'              -- the character
  x: 45, y: 0         -- local position
  w: 8, h: 12         -- glyph size
  r: 0                -- rotation
  sx: 1, sy: 1        -- scale
  ox: 0, oy: 0        -- offset (for effects like shake)
  line: 1             -- which line
  i: 5                -- global index
  effects: [...]      -- list of {effect_name, arg1, arg2, local_index}
  effect: {...}       -- lookup table: effect.keyword.i = local index
}
```

This is powerful because characters can use **other object modules**:
```yuescript
-- Shake effect using the object's shake module
shake = (dt, text, c, intensity, duration) =>
  if text.first_step
    c\shake! unless c\is 'shake'
    c\normal_shake intensity, duration
  c.ox, c.oy = c.shake_amount.x, c.shake_amount.y
```

**4. Effect System**
```yuescript
-- Effect signature: (dt, text, character, ...effect_args)
color_effect = (dt, text, c, color) =>
  text.text_layer\set_color color

wave_effect = (dt, text, c, amplitude, frequency) =>
  c.oy = math.sin(an.game_time * frequency + c.i * 0.5) * amplitude

typewriter_effect = (dt, text, c, chars_per_second) =>
  visible_count = math.floor(text.elapsed_time * chars_per_second)
  c.visible = c.i <= visible_count
```

**5. Drawing**
```yuescript
text_draw: (dt, x, y, r, sx, sy) =>
  @text_layer\push x, y, r, sx, sy
  for c in *@characters
    -- Apply all effects (modify c.ox, c.oy, c.r, c.sx, c.sy, set color, etc.)
    for effect_table in *c.effects
      effect_fn = @text_effects[effect_table[1]]
      effect_fn dt, @, c, unpack(effect_table, 2) if effect_fn

    -- Draw the glyph via C
    @text_layer\draw_text_lt c.c, @text_font_name, x + c.x + c.ox, y + c.y + c.oy, c.r, c.sx, c.sy
    @text_layer\set_color an.colors.white[0]  -- reset color
  @text_layer\pop!
```

---

### Summary Table

| Responsibility | Side | Rationale |
|---------------|------|-----------|
| Font loading (TTF → atlas) | C | Performance, stb_truetype integration |
| Glyph metrics storage | C | Efficient lookup |
| Draw single glyph | C | Batching with sprites |
| Draw simple text | C | Error screen, debug output |
| UTF-8 iteration | YueScript | Lua has good UTF-8 support |
| Tag parsing `[text](effects)` | YueScript | String manipulation, flexibility |
| Word wrap & alignment | YueScript | Uses glyph metrics from C |
| Character objects in tree | YueScript | Full object system integration |
| Effect functions | YueScript | Arbitrary Lua/YueScript code |
| Per-frame effect application | YueScript | Calls C draw functions |

---

### What C Needs to Expose

```lua
-- Font management
font_load(name, path, size)
font_get_text_width(font_name, text)
font_get_height(font_name)

-- Drawing
layer_draw_text(layer, text, font, x, y, color)        -- Simple (debug/error)
layer_draw_text_lt(layer, char, font, x, y, r, sx, sy) -- Per-glyph (effects)
layer_set_color(color)                                  -- Already exists
```

This clean separation means:
- C handles the **GPU-facing work** (atlas management, batched rendering)
- YueScript handles the **game-facing work** (parsing, layout, effects)
- Effects are infinitely flexible — they're just YueScript functions modifying character properties before the C draw call

---

## Rich Text Research Across Game Engines

Research conducted across 19 game engines to understand different approaches to rich text systems, tag syntaxes, and per-character/word effects.

---

### Unity TextMeshPro

**Source:** [Unity TextMeshPro Documentation](https://docs.unity3d.com/Packages/com.unity.textmeshpro@4.0/manual/RichTextSupportedTags.html)

**Tag Syntax:** HTML-style `<tag>content</tag>` or `<tag="value">`

**Supported Tags:**
- **Styling:** `<b>`, `<i>`, `<u>`, `<s>`, `<strikethrough>`
- **Color:** `<color>`, `<alpha>`, `<gradient>`, `<mark>` (highlight)
- **Size/Spacing:** `<size>`, `<cspace>` (character spacing), `<line-height>`, `<mspace>` (monospace), `<voffset>` (vertical offset)
- **Layout:** `<align>`, `<indent>`, `<line-indent>`, `<margin>`, `<pos>` (horizontal position), `<br>`, `<page>`, `<nobr>` (no break)
- **Transform:** `<allcaps>`, `<lowercase>`, `<smallcaps>`, `<rotate>`
- **Advanced:** `<font>`, `<font-weight>`, `<link>`, `<sprite>`, `<style>`, `<width>`, `<noparse>`, `<space>`

**Key Insights:**
- Very comprehensive tag set
- `<link>` tag creates clickable regions with custom IDs — useful for hover/click effects on words
- `<sprite>` embeds inline images from sprite sheets
- `<style>` applies predefined style presets
- No built-in animated effects (shake, wave) — those require custom shaders or scripting
- Tags can be up to 128 characters long

---

### Godot RichTextLabel

**Source:** [Godot BBCode Documentation](https://docs.godotengine.org/en/stable/tutorials/ui/bbcode_in_richtextlabel.html)

**Tag Syntax:** BBCode-style `[tag]content[/tag]` or `[tag=value]`

**Built-in Tags:**
- Standard: `[b]`, `[i]`, `[u]`, `[s]`, `[code]`, `[color]`, `[font]`, `[font_size]`
- Layout: `[center]`, `[right]`, `[fill]`, `[indent]`, `[cell]`, `[table]`
- Special: `[url]` (links), `[img]` (images), `[hint]` (tooltips)

**Built-in Animated Effects:**
- `[wave amp=50 freq=2]` — sinusoidal wave
- `[tornado radius=5 freq=2]` — circular motion
- `[shake rate=5 level=10]` — random shake
- `[fade start=0 length=10]` — fade in characters
- `[rainbow freq=0.2 sat=10 val=20]` — color cycling

**Custom Effects API:**
```gdscript
class_name MyEffect extends RichTextEffect

var bbcode = "my_effect"  # Tag name

func _process_custom_fx(char_fx: CharFXTransform) -> bool:
    char_fx.offset = Vector2(10, 0)  # Modify position
    char_fx.color = Color.RED        # Modify color
    return true
```

**CharFXTransform provides:**
- `offset` — position offset
- `color` — character color
- `outline_color` — outline color
- `transform` — 2D transform matrix
- `visible` — show/hide
- `relative_index` — index from tag start
- `absolute_index` — index from text start
- `elapsed_time` — time since text appeared
- `character` — the character codepoint

**Key Insights:**
- Best-in-class custom effect API with per-character transform access
- `relative_index` vs `absolute_index` distinction — exactly what Anchor's `c.effect.tag.i` does
- Effects run continuously (battery warning in docs)
- Custom effects are Resources that can be saved/shared

---

### Ren'Py

**Source:** [Ren'Py Text Documentation](https://www.renpy.org/doc/html/text.html), [Custom Text Tags](https://www.renpy.org/doc/html/custom_text_tags.html)

**Tag Syntax:** Curly braces `{tag}content{/tag}` or `{tag=value}`

**Built-in Tags:**
- Styling: `{b}`, `{i}`, `{u}`, `{s}`, `{plain}` (removes all styling)
- Visual: `{color}`, `{alpha}`, `{size}`, `{font}`, `{outlinecolor}`
- Spacing: `{k}` (kerning), `{space}`, `{vspace}`
- Timing: `{cps}` (characters per second), `{w}` (wait for click), `{p}` (paragraph), `{nw}` (no wait), `{fast}` (instant)
- Special: `{a}` (links), `{image}`, `{shader}`
- Style: `{=stylename}` — apply named style directly

**Custom Text Tags:**
```python
def my_tag(tag, argument, contents):
    # contents is list of (type, data) tuples
    # Return modified list of tuples
    return [(renpy.TEXT_TEXT, "modified")]

config.custom_text_tags["mytag"] = my_tag
```

**Key Insights:**
- **Pure function requirement** — custom tags can't have side effects or depend on game state (for prediction/rollback)
- Content is passed as structured tuples, not raw strings
- Dialogue-specific tags (`{w}`, `{p}`, `{nw}`) for visual novel pacing
- `{=stylename}` is elegant — reference predefined styles inline
- Kinetic Text Tags (community) adds shake, wave, bounce via shaders

---

### Phaser 3

**Source:** [Phaser BBCode Text Plugin](https://rexrainbow.github.io/phaser3-rex-notes/docs/site/bbcodetext/)

**Tag Syntax:** BBCode `[tag]content[/tag]`

**BBCodeText Plugin (Rex Rainbow):**
- Standard formatting: `[b]`, `[i]`, `[u]`, `[s]`
- Colors: `[color=red]`, `[stroke=blue]`, `[shadow]`
- Size: `[size=24]`, `[y=-8]` (vertical offset)
- Images: `[img=key]`
- Escape: `[esc]...[/esc]` or `[raw]...[/raw]`

**DynamicBitmapText (Built-in):**
```javascript
// Per-character callback during render
var text = scene.add.dynamicBitmapText(0, 0, 'font', 'Hello');
text.setDisplayCallback(function(data) {
    // data.index, data.charCode, data.x, data.y, data.scale, data.rotation, data.tint
    data.y += Math.sin(data.index + time) * 5;
    return data;
});
```

**Key Insights:**
- **DynamicBitmapText** has a callback-based effect system — function runs per character during render
- Callback receives character data object with mutable x, y, scale, rotation, tint
- Static vs Dynamic distinction: static is faster, dynamic allows per-character effects
- Character tinting is WebGL-only

---

### KaboomJS

**Source:** [KaboomJS Documentation](https://kaboomjs.com/)

**Tag Syntax:** Square brackets `[style]text[/style]` (bitmap fonts only)

**Transform Callback:**
```javascript
add([
    text("Hello World!", {
        transform: (idx, ch) => ({
            color: hsl2rgb((time() * 0.2 + idx * 0.1) % 1, 0.7, 0.8),
            pos: vec2(0, wave(-4, 4, time() * 4 + idx * 0.5)),
            scale: wave(1, 1.2, time() * 3 + idx * 0.5),
            angle: wave(-9, 9, time() * 3 + idx * 0.5),
        }),
    }),
])
```

**Style Tags:**
```javascript
text("[green]colored[/green] and [wavy]animated[/wavy]", {
    styles: {
        "green": { color: rgb(0, 255, 0) },
        "wavy": (idx, ch) => ({ /* transform */ }),
    }
})
```

**Key Insights:**
- **Transform function** runs per-character per-frame, returns style object
- Can combine static styles and animated transforms
- `idx` (character index) and `ch` (character) passed to transform
- Styles are defined externally and referenced by name in text
- Clean separation: markup just names styles, styles define behavior

---

### PixiJS

**Source:** [PixiJS Text Documentation](https://pixijs.com/8.x/guides/components/scene-objects/text)

**Three Text Types:**
1. **Text** — Canvas-based, rich styling, slow to update
2. **BitmapText** — Pre-rendered glyphs, fast, limited styling
3. **HTMLText** — Actual HTML/CSS rendering in scene

**SplitText / SplitBitmapText:**
```javascript
const split = new SplitText({
    text: "Hello World",
    style: { /* ... */ }
});

// Access individual elements
split.chars;  // Array of character display objects
split.words;  // Array of word display objects
split.lines;  // Array of line display objects

// Animate each character
split.chars.forEach((char, i) => {
    gsap.to(char, { y: -20, delay: i * 0.1 });
});
```

**Key Insights:**
- **SplitText creates actual display objects** for chars/words/lines — each is independently animatable
- Works with GSAP or any animation library
- Anchor point per segment for rotation/scale origin
- **Caveat:** Splitting removes kerning — spacing will differ from normal text
- HTMLText supports full CSS including flexbox

---

### Defold RichText

**Source:** [Defold RichText GitHub](https://github.com/britzl/defold-richtext)

**Tag Syntax:** HTML-style `<tag>content</tag>`

**Supported Tags:**
- `<b>`, `<i>`, `<color>`, `<shadow>`, `<outline>`, `<size>`, `<font>`
- `<img>` (inline images), `<spine>` (spine animations)
- `<br/>`, `<nobr>`, `<p>` (paragraph spacing)
- `<a>` (clickable regions with message callbacks)
- `<repeat>` (duplicate text N times)

**Word-Level API:**
```lua
local words, metrics = richtext.create(text, font, settings)

-- Get words with specific tag
local tagged = richtext.tagged(words, "emphasis")

-- Split word into characters (for animation)
local chars = richtext.characters(word)

-- Truncate for typewriter effect
richtext.truncate(words, length, options)

-- Click detection on <a> tags
richtext.on_click(words, action)
```

**Key Insights:**
- **Word-level granularity** — creates one GUI node per word (not per character by default)
- `richtext.characters(word)` splits a word for per-character effects when needed
- `combine_words` option merges same-style words to reduce node count
- `<a>` tags generate click messages — built-in interactivity
- Metrics include width, height, ascent, descent per word

---

### HaxeFlixel

**Source:** [HaxeFlixel FlxText API](https://api.haxeflixel.com/flixel/text/FlxText.html)

**Markup System:**
```haxe
text.applyMarkup(
    "show $green text$ between dollar-signs and @yellow text@ here",
    [
        new FlxTextFormatMarkerPair(greenFormat, "$"),
        new FlxTextFormatMarkerPair(yellowFormat, "@")
    ]
);
```

**FlxTextFormat Properties:**
- Font color (0xRRGGBB)
- Bold flag
- Border style, color, size, quality

**FlxTypeText:**
- Typewriter effect built-in
- `delay` between characters
- `showCursor` option
- `eraseOnComplete` option

**Key Insights:**
- **Marker-based formatting** — define markers ($, @, etc.) that wrap styled regions
- No built-in BBCode/HTML — markers are arbitrary single characters
- Clears all formats when applying new markup (destructive)
- Border effects (shadow, outline) are font-level, not per-character

---

### Heaps.io

**Source:** [Heaps HtmlText](https://heaps.io/api/h2d/HtmlText.html)

**Tag Syntax:** HTML subset `<font>`, `<br>`, `<p>`, `<img>`

**HtmlText Features:**
```haxe
var html = new h2d.HtmlText(font);
html.text = '<font color="#FF0000">Red</font> and <font face="bold">Bold</font>';

// Custom tag → font mapping
html.condenseWhite = true;

// Image loading callback
html.loadImage = function(src) { return tile; };

// Drop shadow
html.dropShadow = { dx: 1, dy: 1, color: 0x000000, alpha: 0.5 };
```

**Key Insights:**
- Uses actual HTML subset, not BBCode
- Custom tags map to fonts: `<mytag>` → specific font resource
- Callbacks for loading fonts/images (not resource manager)
- SDF font support for scalable text
- Drop shadow is text-object-level, not per-character

---

### GameMaker

**Source:** [GameMaker Text Manual](https://manual.gamemaker.io/beta/en/GameMaker_Language/GML_Reference/Drawing/Text/draw_text_ext.htm)

**Built-in Functions:**
- `draw_text(x, y, string)`
- `draw_text_ext(x, y, string, sep, w)` — with line spacing and wrap width
- `draw_text_colour(x, y, string, c1, c2, c3, c4, alpha)` — gradient corners
- `draw_text_ext_transformed(x, y, string, sep, w, xscale, yscale, angle)`

**No Built-in Rich Text.** For mixed styles:
```gml
draw_set_font(font_normal);
draw_text(x, y, "Normal ");
draw_set_font(font_bold);
draw_text(x + string_width("Normal "), y, "Bold");
```

**Third-Party: Nox Text**
```gml
// Initialize
nox_text_init();

// Use effect markers in string
draw_text_nox(x, y, "Normal ~bold~ and <shake>shaking</shake>");
```

**Key Insights:**
- **No native rich text** — must manually position multiple draw calls
- Community solutions (Nox Text, Scribble) fill the gap
- `draw_text_colour` does 4-corner gradients (whole text, not per-character)
- Font switching requires manual width calculation for positioning

---

### RPG Maker

**Source:** [Yanfly Text Codes Wiki](http://www.yanfly.moe/wiki/Category:Text_Codes_(MZ))

**Escape Code Syntax:** Backslash codes `\C[n]`, `\V[n]`, etc.

**Default Codes:**
- `\V[n]` — show variable value
- `\N[n]` — show actor name
- `\C[n]` — change text color
- `\I[n]` — show icon
- `\{` / `\}` — increase/decrease font size
- `\.` / `\|` — wait 15/60 frames
- `\!` — wait for button press
- `\>` / `\<` — instant display on/off

**Plugin Extensions (Yanfly/VisuStella):**
- `\FS[n]` — font size
- `\FF[name]` — font face
- `\PX[n]` / `\PY[n]` — position offset
- `\SHAKE[...]` — shaking text
- `\WAVE[...]` — wave animation

**Key Insights:**
- Escape code system designed for message windows
- Codes are processed during text display (typewriter-style)
- Wait codes (`\.`, `\|`, `\!`) integral to dialogue pacing
- Heavy reliance on plugins for modern effects
- Message Actions plugin: trigger game events from text codes

---

### GDevelop

**Source:** [GDevelop BBText Documentation](https://wiki.gdevelop.io/gdevelop5/objects/bbtext/)

**Tag Syntax:** BBCode `[tag]content[/tag]`

**Supported Tags:**
- `[b]`, `[i]`, `[u]`, `[s]`
- `[color=#hex]`, `[font=name]`, `[size=n]`
- `[shadow]`, `[outline]`
- `[spacing=n]` (character spacing)

**Limitations:**
- No animated effects (shake, wave requested by community)
- Can't combine bold+italic reliably
- No line spacing control
- No inline sprites

**Key Insights:**
- Basic BBCode implementation
- Word wrap toggle
- Feature requests show demand for: shake, wave, inline sprites, line spacing

---

### Cute Framework

**Source:** [Cute Framework Renderer](https://randygaul.github.io/cute_framework/topics/renderer/)

**Architecture:**
- Glyphs rendered on-demand via stb_truetype
- Glyphs treated as sprites, batched into atlas
- Text, sprites, and shapes all batch together in single draw call

**Mentioned Features:**
- Word wrapping
- Vertical/horizontal layout
- Text measurement (width/height)
- **"Custom markup effects on text (like shaking, or dynamically glowing)"**

**Key Insights:**
- Unified sprite/glyph rendering (same atlas system)
- Documentation mentions markup effects but API details sparse
- Focus on single-draw-call performance
- See `cute_draw.h` for implementation

---

### LÖVR (VR)

**Source:** [LÖVR Website](https://lovr.org/)

**Features:**
- SDF font rendering (stays crisp at VR distances/scales)
- 3D text positioning
- Standard font loading

**No rich text system documented** — VR focus is on readable 3D text rather than styled 2D text.

---

### MonoGame

**Source:** [MonoGame SpriteFont Tutorial](https://docs.monogame.net/articles/tutorials/building_2d_games/16_working_with_spritefonts/index.html)

**Built-in:**
```csharp
spriteBatch.DrawString(font, "Hello", position, Color.White,
    rotation, origin, scale, SpriteEffects.None, layerDepth);

Vector2 size = font.MeasureString("Hello");
```

**No rich text.** For mixed styles:
- Multiple DrawString calls with calculated positions
- Or use third-party library

**Key Insights:**
- SpriteFont is texture atlas-based (like most game engines)
- DrawString parameters match SpriteBatch.Draw (consistent API)
- No native per-character effects or markup
- Community libraries fill the gap

---

### LÖVE 2D Libraries

**Source:** [LÖVE Forums](https://love2d.org/forums/viewtopic.php?t=1890), [awesome-love2d](https://github.com/love2d-community/awesome-love2d)

**richtext (gvx):**
```lua
rt = rich:new{"Hello {green}world{red}!", 200, ...}
```

**SYSL-Text:**
- Tag-based animations
- Automatic wrapping
- Typewriter effects

**Key Insights:**
- No built-in rich text in LÖVE
- Community libraries use curly-brace `{tag}` syntax
- Similar to what Anchor already has

---

## Key Patterns & Insights

### Tag Syntax Approaches

| Style | Engines | Example |
|-------|---------|---------|
| HTML-like | Unity, Defold, Heaps | `<color=red>text</color>` |
| BBCode | Godot, Phaser, GDevelop, Construct | `[color=red]text[/color]` |
| Curly braces | Ren'Py, LÖVE libs | `{color=red}text{/color}` |
| Markdown-like | Anchor (current) | `[text](color=red)` |
| Escape codes | RPG Maker | `\C[1]text` |
| Marker pairs | HaxeFlixel | `$text$` with format map |

### Effect Application Models

**1. Per-Character Callback (Render-time)**
- Used by: Phaser DynamicBitmapText, KaboomJS
- Function called during render, modifies position/color/scale
- Pros: No pre-processing, real-time
- Cons: Runs every frame, can't easily do state-based effects

**2. Per-Character Objects (Pre-processed)**
- Used by: Anchor, PixiJS SplitText, Defold (via characters())
- Each character becomes discrete object
- Pros: Full animation system access, state persistence
- Cons: Memory overhead, kerning loss

**3. Custom Effect Classes (Godot model)**
- Effect is a Resource with `_process_custom_fx(char_fx)` method
- Receives CharFXTransform with all character properties
- Pros: Reusable, saveable, clean API
- Cons: Godot-specific pattern

**4. Tag Function (Ren'Py model)**
- Tag function receives content, returns modified content
- Pure function, no side effects
- Pros: Predictable, works with rollback
- Cons: Limited to text transformation, not animation

### Word-Level vs Character-Level

**Anchor's current model:** Everything is per-character. This makes word-level effects (underline on hover, word highlight) awkward.

**Defold's approach:** Default is per-word nodes, with `richtext.characters(word)` to split when needed.

**PixiJS SplitText:** Provides `chars`, `words`, `lines` arrays — all three levels accessible.

**Recommendation:** Consider a hybrid:
- Parse into words first
- Words contain character arrays
- Effects can target word or character level
- `[word](underline)` vs `[w][o][r][d](each with wave)`

### Interactive Text (Links, Hover)

**Unity:** `<link="id">text</link>` — generates events with link ID
**Defold:** `<a="message">text</a>` — sends message on click
**Ren'Py:** `{a=url}text{/a}` — hyperlinks

**Pattern:** Tag marks region, engine detects click/hover, reports which region was hit.

**For Anchor:** Could add `<span id="foo">word</span>` or `[word](id=foo)`, then provide:
```lua
text:get_region("foo")  -- returns {x, y, width, height}
text:is_hovered("foo")  -- check if mouse over region
```

### Inline Images/Sprites

**Unity:** `<sprite index=3>` or `<sprite name="heart">`
**Godot:** `[img]path[/img]`
**Defold:** `<img=texture:anim/>`

**Pattern:** Reference sprite by index, name, or path. Engine reserves space in layout and draws sprite inline.

### Typewriter/Reveal Effects

**Ren'Py:** `{cps=20}` (characters per second), `{fast}` (instant)
**RPG Maker:** `\.` (15 frame wait), `\|` (60 frame wait)
**Defold:** `richtext.truncate(words, length)`

**Pattern:** Either:
1. Control display speed via tag
2. Truncate rendered text to N characters

---

## Recommendations for Anchor

### 1. Add Word-Level Grouping

Current flow:
```
raw_text → parse tags → character list → format → draw
```

Proposed:
```
raw_text → parse tags → word list (each with char list) → format → draw
```

Benefits:
- Word-level effects (underline, highlight, click regions)
- `text:get_words_with_tag("keyword")` returns word objects
- Still have character access: `word.characters`

### 2. Named Regions for Interactivity

Add a region/span concept:
```lua
"Click [here](region=btn1) to continue"

-- Query
if text:region_clicked("btn1") then
    -- handle click
end

local bounds = text:get_region_bounds("btn1")  -- {x, y, w, h}
```

### 3. Effect Scope Modifier

Allow effects to specify word vs character scope:
```lua
"[shaking word](shake;scope=word)"   -- whole word shakes together
"[shaking word](shake;scope=char)"   -- each char shakes independently (default)
```

### 4. Consider Style Presets (KaboomJS pattern)

Define styles separately, reference by name:
```lua
text_styles = {
    keyword = {color = colors.yellow, bold = true},
    shake = function(dt, text, c) ... end,
}

"The [attack speed](keyword) stat affects..."
```

Benefits:
- Cleaner text strings
- Reusable styles
- Easier to change all instances

### 5. Built-in Effects Library

Provide common effects out of the box:
- `wave(amplitude, frequency)` — vertical sine wave
- `shake(intensity)` — random offset
- `rainbow(speed)` — color cycling
- `typewriter(cps)` — reveal characters over time
- `fade(duration)` — fade in characters sequentially

Users can still define custom effects, but basics are covered.

### 6. Relative vs Absolute Index (Already Have)

Keep the `c.effect.tag.i` pattern — it's exactly what Godot's `relative_index` provides and is useful for wave/fade effects that should restart per tagged region.

---

## Sources

- [Unity TextMeshPro Rich Text](https://docs.unity3d.com/Packages/com.unity.textmeshpro@4.0/manual/RichTextSupportedTags.html)
- [Godot BBCode in RichTextLabel](https://docs.godotengine.org/en/stable/tutorials/ui/bbcode_in_richtextlabel.html)
- [Ren'Py Text Documentation](https://www.renpy.org/doc/html/text.html)
- [Ren'Py Custom Text Tags](https://www.renpy.org/doc/html/custom_text_tags.html)
- [Phaser BBCode Text Plugin](https://rexrainbow.github.io/phaser3-rex-notes/docs/site/bbcodetext/)
- [PixiJS Text Guide](https://pixijs.com/8.x/guides/components/scene-objects/text)
- [PixiJS SplitText](https://pixijs.com/8.x/guides/components/scene-objects/text/split-text)
- [Defold RichText](https://github.com/britzl/defold-richtext)
- [HaxeFlixel FlxText API](https://api.haxeflixel.com/flixel/text/FlxText.html)
- [Heaps HtmlText](https://heaps.io/api/h2d/HtmlText.html)
- [GameMaker draw_text_ext](https://manual.gamemaker.io/beta/en/GameMaker_Language/GML_Reference/Drawing/Text/draw_text_ext.htm)
- [KaboomJS](https://kaboomjs.com/)
- [GDevelop BBText](https://wiki.gdevelop.io/gdevelop5/objects/bbtext/)
- [Cute Framework Renderer](https://randygaul.github.io/cute_framework/topics/renderer/)
- [MonoGame SpriteFont](https://docs.monogame.net/articles/tutorials/building_2d_games/16_working_with_spritefonts/index.html)
- [Construct 3 Text Features](https://www.construct.net/en/blogs/construct-official-blog-1/new-text-features-construct-908)
- [LÖVR](https://lovr.org/)
- [Yanfly Text Codes](http://www.yanfly.moe/wiki/Category:Text_Codes_(MZ))

---

## Final C/Lua API

### C Side — Font Infrastructure

```c
// Font management
void font_load(const char* name, const char* path, int size);
void font_unload(const char* name);

// Metrics
float font_get_height(const char* font_name);
float font_get_text_width(const char* font_name, const char* text);
float font_get_char_width(const char* font_name, uint32_t codepoint);

// For layout — returns table {advance, bearing_x, bearing_y, width, height}
// Advance is how far to move cursor; width/height are visual bounds
GlyphMetrics font_get_glyph_metrics(const char* font_name, uint32_t codepoint);

// Drawing
// Simple text (for debug, error screens, simple UI)
void layer_draw_text(Layer* layer, const char* text, const char* font_name,
                     float x, float y, Color color);

// Single glyph with full transform (for effect system)
// Rotation is around glyph center
void layer_draw_glyph(Layer* layer, uint32_t codepoint, const char* font_name,
                      float x, float y,
                      float r, float sx, float sy,
                      Color color, float alpha);

// Draw primitives (for underline, strikethrough effects)
void layer_draw_line(Layer* layer, float x1, float y1, float x2, float y2,
                     Color color, float thickness);
void layer_draw_rect(Layer* layer, float x, float y, float w, float h,
                     Color color, float alpha);  // For highlight/mark effects
```

### Lua Bindings (Flat Functions)

```lua
-- Font management
font_load(name, path, size)
font_unload(name)

-- Metrics
font_get_height(font_name)
font_get_text_width(font_name, text)
font_get_char_width(font_name, codepoint)
font_get_glyph_metrics(font_name, codepoint)  -- returns {advance, bearing_x, bearing_y, w, h}

-- Drawing (layer is lightuserdata pointer)
layer_draw_text(layer, text, font_name, x, y, color)
layer_draw_glyph(layer, codepoint, font_name, x, y, r, sx, sy, color, alpha)
layer_draw_line(layer, x1, y1, x2, y2, color, thickness)
layer_draw_rect(layer, x, y, w, h, color, alpha)

-- Existing
layer_set_color(layer, color)
layer_push(layer, x, y, r, sx, sy)
layer_pop(layer)

-- Color helpers
hsl_to_rgb(h, s, l)      -- returns color
lerp_color(c1, c2, t)    -- returns color
```

The YueScript `Layer` class wraps these into method calls (`layer\draw_glyph ...`).

---

## Imaginary Rich Text API — YueScript Design

The goal: design an intuitive, expressive API for rich text effects in Anchor, following YueScript conventions. Characters and words are Anchor tree objects, effects use the same patterns as other Anchor systems.

### Design Principles

1. **Simple effects should be simple** — a one-liner wave effect shouldn't require boilerplate
2. **Complex effects should be possible** — stateful effects use init/update lifecycle
3. **Word-level and character-level** — effects can target words or characters
4. **Regions for interactivity** — hover/click detection on text spans
5. **Tree integration** — characters are objects, can use timers/springs/etc.

---

### Tag Syntax

Godot-style with multiple parentheses for multiple effects:

```
[tagged text](effect key=val key2=val2)(effect2)(effect3 key=val)
```

Each `()` is one effect. Within each:
- Effect name comes first
- Arguments are space-separated `key=value` pairs
- Bare values become positional: `(wave 4 3)` → `args[1]=4, args[2]=3`
- Named values: `(wave amp=4 freq=3)` → `args.amp=4, args.freq=3`

Examples:
```
[wavy text](wave amp=4 freq=3)
[shaky text](shake 2)
[multiple effects](wave amp=4)(shake)(color #ff0)
[interactive](region btn1)(underline)(color #0af)
[word bounce](bounce gravity=600)(scope word)
```

The `scope` modifier is a special argument that changes how the effect is applied:
- `(scope char)` — default, effect runs per character
- `(scope word)` — effect runs per word
- `(scope region)` — effect runs once for entire tagged span

---

### Text Creation

```yuescript
-- Text is an object, uses ^ for properties, >> or << to add to tree
Text "[Hello](wave) [world](shake)(color #ff0)" ^ {
  font: 'main'
  width: 400
  align: 'center'
  line_height: 1.2
} >> arena

-- Or with << (parent receives child)
arena << Text "[Score: 100](pulse)" ^ {font: 'ui', align: 'right'}

-- Named text object (arena.message points to it)
Text 'message', "[Click to start](pulse)" ^ {font: 'main'} >> arena

-- Keep reference
t = Text "[Hello](wave)" ^ {width: 400}
arena << t
```

### Drawing

```yuescript
-- Text draws itself via its action (added automatically)
-- Or draw manually with transform:
t\draw layer, x, y
t\draw layer, x, y, r, sx, sy
```

### Querying Structure

```yuescript
-- Characters and words are child objects in the tree
for c in *t.chars
  print c.char, c.x, c.y

for w in *t.words
  print w.text
  for c in *w.chars
    print "  ", c.char

-- Get elements with specific effect
shaking = t\with_effect 'shake'

-- Region queries (returns bounds in text-local coords)
bounds = t\region_bounds 'btn1'  -- {x, y, w, h}

-- Hit testing
if t\region_hovered 'btn1', mx, my
  -- mouse over region

if t\region_clicked 'btn1', mx, my, mouse_pressed
  -- clicked
```

**Multiple regions with same ID:** If `(region btn1)` appears multiple times in the text, all tagged spans belong to the same region. `region_bounds` returns the combined bounding box, and `region_hovered`/`region_clicked` return true if any span is hit.

```yuescript
-- Both "Yes" and "No" are part of region "choice"
Text "[Yes](region choice) or [No](region choice)" ^ {...} >> arena

-- Clicking either triggers region_clicked 'choice'
```

### Modifying Text

```yuescript
-- Change content (re-parses and re-layouts)
t\set_text "[New text](wave)"

-- Typewriter: limit visible characters
t.visible_chars = 10
t.visible_chars = nil  -- show all
```

---

### Effect API

Effects can be **functions** (simple, stateless) or **setup functions** (stateful, using `^` and `/`).

#### Simple Function Effect

Called every frame. No state, just modifies character properties.

```yuescript
-- Signature: (c, args, ctx) ->
-- c: character object (or word if scope=word)
-- args: parsed arguments {[1], [2], ..., key=val}
-- ctx: {time, dt, frame, layer, text}

effects.wave = (c, args, ctx) ->
  amp = args.amp or args[1] or 4
  freq = args.freq or args[2] or 4
  c.oy += sin(ctx.time * freq + c.local_i * 0.5) * amp
```

#### Stateful Effect (using ^ and /)

For effects that need state, use `^` to initialize and `/` to add an action. Called once per character when text is created.

```yuescript
-- Setup function: uses ^ for init, / for action
effects.shake = (c, args) ->
  intensity = args.intensity or args[1] or 3

  -- ^ sets up state on the character
  c ^ ->
    @shake_target = vec2 0, 0
    @shake_current = vec2 0, 0
    @shake_timer = 0
    @shake_interval = 0.04 + random! * 0.02

  -- / adds the update action
  c / (dt) =>
    @shake_timer += dt
    if @shake_timer > @shake_interval
      @shake_timer = 0
      @shake_target.x = (random! * 2 - 1) * intensity
      @shake_target.y = (random! * 2 - 1) * intensity

    @shake_current += (@shake_target - @shake_current) * dt * 15
    @ox += @shake_current.x
    @oy += @shake_current.y
```

#### Region-Scope Effect

For effects that draw once per tagged region (like underline, highlight).

```yuescript
effects.underline = (region, args, ctx) ->
  color = args.color or region.chars[1].color or colors.white
  thickness = args.thickness or 1
  offset = args.offset or 2

  y = region.y + region.h + offset
  ctx.layer\draw_line region.x, y, region.x + region.w, y, color, thickness

effects.underline.scope = 'region'
```

---

### Character Object

Characters are Anchor objects in the tree. They can use timers, springs, etc.

```yuescript
-- Read-only (set by layout)
c.char         -- original character (string)
c.codepoint    -- unicode codepoint
c.x, c.y       -- layout position (local to text)
c.w, c.h       -- glyph dimensions
c.line         -- which line (1-indexed)
c.i            -- global index
c.local_i      -- index within effect span (resets per tag)

-- Writable (effects modify)
c.ox, c.oy     -- offset from layout position
c.r            -- rotation (radians)
c.sx, c.sy     -- scale (default 1)
c.color        -- color value
c.alpha        -- opacity 0-1 (default 1)
c.visible      -- show/hide (default true)
c.display_char -- what to render (defaults to c.char)
```

Because characters are tree objects, effects can leverage Anchor's systems:

```yuescript
-- Effect that uses character's own spring
effects.springy = (c, args) ->
  -- ^ adds spring child and pulls it
  c ^ ->
    @ + spring 'fx', 1, 200, 10
    @fx\pull 0.5

  -- / updates scale from spring value
  c / (dt) =>
    @sy = @fx.x
```

### Word Object

When using `(scope word)`, the effect receives word objects:

```yuescript
w.text         -- word as string
w.chars        -- array of character objects
w.x, w.y       -- layout position
w.w, w.h       -- bounding box
w.line         -- which line
w.i            -- global word index
w.local_i      -- index within effect span

-- Writable
w.ox, w.oy     -- offset (propagates to all chars)
w.r            -- rotation (around word center)
w.sx, w.sy     -- scale (around word center)
w.alpha        -- opacity (propagates to chars)
```

### Region Object

When using `(scope region)`, the effect receives region objects:

```yuescript
region.x, region.y   -- bounding box position
region.w, region.h   -- bounding box size
region.chars         -- all characters in region
region.words         -- all words in region
region.id            -- the region ID from (region id)
```

### Context Object

```yuescript
ctx.time    -- seconds since text created
ctx.dt      -- delta time
ctx.frame   -- frame counter
ctx.layer   -- Layer object for drawing
ctx.text    -- reference to Text object
```

---

## 10 Built-in Effects — YueScript Implementation

### 1. Wave

Vertical sinusoidal oscillation.

```yuescript
-- Usage: [wavy text](wave) or [wavy](wave 8 3) or [wavy](wave amp=8 freq=3)

effects.wave = (c, args, ctx) ->
  amp = args.amp or args[1] or 4
  freq = args.freq or args[2] or 4
  c.oy += sin(ctx.time * freq + c.local_i * 0.5) * amp
```

---

### 2. Shake

Smooth random motion with lerped targets.

```yuescript
-- Usage: [shaky](shake) or [shaky](shake 5)

effects.shake =
  init: (c, args) ->
    target: vec2 0, 0
    current: vec2 0, 0
    timer: 0
    interval: 0.04 + random! * 0.02

  update: (c, state, args, ctx) ->
    intensity = args.intensity or args[1] or 3

    state.timer += ctx.dt
    if state.timer > state.interval
      state.timer = 0
      state.target.x = (random! * 2 - 1) * intensity
      state.target.y = (random! * 2 - 1) * intensity

    smoothing = ctx.dt * 15
    state.current += (state.target - state.current) * smoothing

    c.ox += state.current.x
    c.oy += state.current.y
```

---

### 3. Rainbow

Cycles hue through spectrum.

```yuescript
-- Usage: [rainbow](rainbow) or [rainbow](rainbow speed=2)

effects.rainbow = (c, args, ctx) ->
  speed = args.speed or args[1] or 1
  sat = args.sat or 0.8
  lit = args.lit or 0.5

  hue = (ctx.time * speed + c.local_i * 0.08) % 1
  c.color = hsl_to_rgb hue, sat, lit
```

---

### 4. Typewriter

Reveals characters over time.

```yuescript
-- Usage: [revealed](typewriter) or [revealed](typewriter 40)

effects.typewriter = (c, args, ctx) ->
  cps = args.cps or args[1] or 20
  visible_count = floor ctx.time * cps
  c.visible = c.local_i <= visible_count
```

---

### 5. Fade

Sequential fade-in with stagger.

```yuescript
-- Usage: [fading](fade) or [fading](fade 0.5 0.03)

effects.fade = (c, args, ctx) ->
  duration = args.duration or args[1] or 0.3
  stagger = args.stagger or args[2] or 0.02

  char_start = c.local_i * stagger
  progress = (ctx.time - char_start) / duration
  c.alpha = clamp progress, 0, 1
```

---

### 6. Pulse

Scale oscillation.

```yuescript
-- Usage: [pulsing](pulse) or [pulsing](pulse min=0.8 max=1.2 speed=3)

effects.pulse = (c, args, ctx) ->
  min_scale = args.min or args[1] or 0.9
  max_scale = args.max or args[2] or 1.1
  speed = args.speed or args[3] or 3

  t = (sin(ctx.time * speed + c.local_i * 0.3) + 1) / 2
  scale = lerp min_scale, max_scale, t

  c.sx *= scale
  c.sy *= scale
```

---

### 7. Jitter

Pure random offset every frame.

```yuescript
-- Usage: [jittery](jitter) or [jittery](jitter 2)

effects.jitter = (c, args, ctx) ->
  intensity = args.intensity or args[1] or 1.5
  c.ox += (random! * 2 - 1) * intensity
  c.oy += (random! * 2 - 1) * intensity
```

---

### 8. Swing

Rotation oscillation.

```yuescript
-- Usage: [swinging](swing) or [swinging](swing angle=20 speed=4)

effects.swing = (c, args, ctx) ->
  max_angle = args.angle or args[1] or 15
  speed = args.speed or args[2] or 4

  c.r += sin(ctx.time * speed + c.local_i * 0.5) * rad(max_angle)
```

---

### 9. Scramble

Random characters until revealed (decryption effect).

```yuescript
-- Usage: [decrypting](scramble) or [decrypting](scramble time=1.5)

effects.scramble =
  pool: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@#$%&*"

  init: (c, args) ->
    last_change: 0
    current_char: nil

  update: (c, state, args, ctx) ->
    reveal_time = args.time or args[1] or 1.0
    char_reveal = c.local_i * 0.04

    total_reveal = char_reveal + reveal_time

    if ctx.time < total_reveal
      -- Still scrambling
      if ctx.time - state.last_change > 0.05
        state.last_change = ctx.time
        idx = floor(random! * #effects.scramble.pool) + 1
        state.current_char = effects.scramble.pool\sub idx, idx
      c.display_char = state.current_char or c.char
    else
      c.display_char = c.char
```

---

### 10. Bounce (Word-Level)

Word drops in and bounces.

```yuescript
-- Usage: [bouncing word](bounce)(scope word)

effects.bounce =
  init: (w, args) ->
    started: false
    velocity: 0
    settled: false

  update: (w, state, args, ctx) ->
    return if state.settled

    gravity = args.gravity or args[1] or 600
    start_delay = w.local_i * 0.15

    if ctx.time < start_delay
      w.oy = -50
      w.alpha = 0
      return

    unless state.started
      state.started = true
      state.velocity = 0
      w.oy = -50
      w.alpha = 1

    -- Physics
    state.velocity += gravity * ctx.dt
    w.oy += state.velocity * ctx.dt

    -- Floor
    if w.oy >= 0
      w.oy = 0
      state.velocity *= -0.5
      if abs(state.velocity) < 30
        state.velocity = 0
        state.settled = true
```

---

## Additional Built-in Effects (Bonus)

### Underline

Region-scope effect that draws a line under tagged text.

```yuescript
-- Usage: [underlined](underline) or [link](underline color=#0af thickness=2)

effects.underline =
  scope: 'region'

  draw: (region, args, ctx) ->
    thickness = args.thickness or 1
    offset = args.offset or 2
    color = args.color or region.chars[1].color or colors.white

    y = region.y + region.h + offset
    ctx.layer\draw_line region.x, y, region.x + region.w, y, color, thickness
```

### Highlight / Mark

Background highlight behind text.

```yuescript
-- Usage: [highlighted](highlight) or [mark](highlight color=#ff0 alpha=0.3)

effects.highlight =
  scope: 'region'
  draw_order: -1  -- draw before characters

  draw: (region, args, ctx) ->
    color = args.color or colors.yellow
    alpha = args.alpha or 0.3
    padding = args.padding or 2

    ctx.layer\draw_rect region.x - padding, region.y - padding,
                        region.w + padding * 2, region.h + padding * 2,
                        color, alpha
```

---

## Complete Example

```yuescript
-- Define custom effect
glitch_effect =
  init: (c, args) ->
    next_glitch: 0
    offset: 0

  update: (c, state, args, ctx) ->
    if ctx.time > state.next_glitch
      state.next_glitch = ctx.time + 0.1 + random! * 0.3
      state.offset = (random! * 2 - 1) * 8

    c.visible = random! >= 0.02
    c.ox += state.offset

-- Create text
message = Text [[
    Welcome to [Anchor](rainbow)(pulse)!

    This text has [multiple](wave) [effects](shake)(color #f80).

    [Click here](region btn)(underline)(color #0af) to continue.

    [CRITICAL ERROR](glitch)(color #f00)(shake 1)
]],
  font: 'main'
  width: 500
  align: 'center'
  effects:
    glitch: glitch_effect

-- Add to scene
arena + message

-- In arena's update
class arena extends object
  update: (dt) =>
    if message\region_clicked 'btn', mx, my, mouse_pressed
      -- handle click

    if message\region_hovered 'btn', mx, my
      -- could change cursor, etc.

  draw: =>
    message\draw game_layer, 400, 300
```

---

## API Summary

### Text Object

```yuescript
-- Creation
t = Text content, options

-- Methods
t\draw layer, x, y
t\draw layer, x, y, r, sx, sy
t\set_text content
t\with_effect name          -- get chars/words with effect
t\region_bounds id          -- {x, y, w, h}
t\region_hovered id, mx, my
t\region_clicked id, mx, my, pressed

-- Properties
t.chars           -- all characters (array of objects)
t.words           -- all words (array of objects)
t.lines           -- all lines (array of objects)
t.visible_chars   -- limit visible chars (nil = all)
```

### Effect Definition

```yuescript
-- Simple function
effect = (c, args, ctx) -> ...

-- Stateful table
effect =
  scope: 'char'      -- 'char' (default), 'word', or 'region'
  draw_order: 0      -- negative = before glyphs, positive = after

  init: (target, args) -> state
  update: (target, state, args, ctx) -> ...
  draw: (region, args, ctx) -> ...  -- for region scope only
```

### Character Properties

| Property | Type | Access | Description |
|----------|------|--------|-------------|
| `char` | string | read | Original character |
| `codepoint` | number | read | Unicode codepoint |
| `x, y` | number | read | Layout position |
| `w, h` | number | read | Glyph dimensions |
| `i` | number | read | Global index |
| `local_i` | number | read | Index within effect span |
| `line` | number | read | Line number |
| `ox, oy` | number | write | Offset |
| `r` | number | write | Rotation (radians) |
| `sx, sy` | number | write | Scale |
| `color` | color | write | Color |
| `alpha` | number | write | Opacity (0-1) |
| `visible` | bool | write | Show/hide |
| `display_char` | string | write | What to render |

### Word Properties

| Property | Type | Access | Description |
|----------|------|--------|-------------|
| `text` | string | read | Word as string |
| `chars` | array | read | Character objects |
| `x, y` | number | read | Layout position |
| `w, h` | number | read | Bounding box |
| `i` | number | read | Global word index |
| `local_i` | number | read | Index within effect span |
| `ox, oy` | number | write | Offset (propagates) |
| `r` | number | write | Rotation |
| `sx, sy` | number | write | Scale |
| `alpha` | number | write | Opacity (propagates) |

### Region Properties

| Property | Type | Description |
|----------|------|-------------|
| `x, y` | number | Bounding box position |
| `w, h` | number | Bounding box size |
| `chars` | array | All characters in region |
| `words` | array | All words in region |
| `id` | string | Region identifier |

### Context Object

| Property | Type | Description |
|----------|------|-------------|
| `time` | number | Seconds since text created |
| `dt` | number | Delta time |
| `frame` | number | Frame counter |
| `layer` | Layer | For custom drawing |
| `text` | Text | Reference to text object |

---

## Required C Functions

After designing the effects, here's the complete list of C functions needed:

### Font

```c
void font_load(const char* name, const char* path, int size);
void font_unload(const char* name);
float font_get_height(const char* font);
float font_get_text_width(const char* font, const char* text);
float font_get_char_width(const char* font, uint32_t codepoint);
GlyphMetrics font_get_glyph_metrics(const char* font, uint32_t codepoint);
```

### Drawing

```c
void layer_draw_text(Layer* layer, const char* text, const char* font,
                     float x, float y, Color color);
void layer_draw_glyph(Layer* layer, uint32_t codepoint, const char* font,
                      float x, float y, float r, float sx, float sy,
                      Color color, float alpha);
void layer_draw_line(Layer* layer, float x1, float y1, float x2, float y2,
                     Color color, float thickness);
void layer_draw_rect(Layer* layer, float x, float y, float w, float h,
                     Color color, float alpha);
```

### Color Helpers

```c
Color hsl_to_rgb(float h, float s, float l);
Color lerp_color(Color a, Color b, float t);
```

### Math Helpers (YueScript/Lua)

```yuescript
-- Assumed available globally
sin, cos, floor, abs, rad
random!
lerp a, b, t
clamp x, min, max
vec2 x, y
```

---

## Implementation Notes

The API is implementable:

1. **Parsing** — Regex for `[text](effect args)(effect2 args)...` with multiple parens
2. **Word grouping** — Split on whitespace during parse, each word tracks its chars
3. **Effect scopes** — Check effect's `scope` field, iterate appropriate level
4. **State** — Each char/word has `effect_state[effect_name]` table
5. **Draw order** — Sort effects by `draw_order`, draw before/after glyphs
6. **Regions** — Store named regions during layout, AABB hit test
7. **Tree integration** — Characters are objects, can have timer/spring children

Estimated YueScript implementation: ~400-600 lines.
