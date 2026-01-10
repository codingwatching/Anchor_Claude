# CLAUDE.md

Guidelines for Claude Code instances working on Anchor and games built with it.

---

## Engine vs Game

**Engine** — C code in `engine/src/anchor.c` exposing functions to Lua
**Game** — YueScript code using the engine to build games
**Engine Mode** — when modifying anchor.c or engine behavior
**Game Mode** — when writing gameplay code in YueScript

---

## Read First

**Source of truth:** `engine/src/anchor.c` — the complete engine implementation (~7000 lines, single file).

**Engine API:**
- `docs/ENGINE_API_QUICK.md` — compact function signatures, one per line (for quick lookup)
- `docs/ENGINE_API.md` — detailed documentation with examples for every function

**Context:**
- `docs/ANCHOR_CONTEXT_BRIEF.md` — design reasoning, developer working style, how to evaluate features

**Archived docs** (superseded by anchor.c, kept for historical reference):
- `reference/archives/ANCHOR_IMPLEMENTATION_PLAN.md` — original phased implementation plan
- `reference/archives/ANCHOR.md` — original engine specification
- `reference/archives/ANCHOR_API_PATTERNS.md` — speculative API patterns

---

## Commands

### Development

```bash
cd E:/a327ex/Anchor/engine && ./build.bat    # Build engine (always after C changes)
./scripts/patch-claude-code.sh               # Fix Windows timestamp bug (after CC updates)
```

### Session End

```bash
ls -t ~/.claude/projects/E--a327ex-Anchor/*.jsonl | grep -v agent | head -1   # Find latest transcript
python scripts/jsonl-to-markdown.py [in.jsonl] website/logs/title-slug.md     # Convert transcript
cp .claude/CLAUDE.md docs/* website/context/                                   # Sync context files
git add -A && git commit -m "Title..."                                         # Commit (see format below)
git push origin main                                                           # Push to GitHub
git subtree push --prefix=website blot master                                  # Push website to Blot
```

### Quick Website Push

```bash
git add -A && git commit --allow-empty-message -m "" && git subtree push --prefix=website blot master
```

### Long Responses

```bash
~/bin/neovim.exe reference/filename.md -- -c "MarkdownPreview"   # Open markdown with preview
```

---

## Session Workflow

When the user asks to end the session, see `docs/SESSION_WORKFLOW.md` for the full workflow (transcript conversion, summary writing, commit format, pushing).

---

## The Project

**Anchor** — a game engine being rewritten from Lua/LÖVE to C/Lua with SDL2, OpenGL, and Box2D.

---

## When to Ask

**Ask first:**
- Architecture decisions (the developer has opinions about action-based vs rules-based, etc.)
- Juice/feel decisions (or leave mechanical — don't invent juice)
- Anything that could be done multiple valid ways

**Proceed, then explain:**
- Implementation details (use best judgment)
- Performance optimization (get it working first, optimize later)

**Pacing:**
- Work incrementally — complete one piece, let them test, get feedback
- After completing a task, give the user a turn before starting the next
- Don't chain tasks or build large systems autonomously

**Communication:**
- Present options, not conclusions
- Surface tradeoffs explicitly
- Don't assume specs are final — they're starting points for conversation
- The developer appreciates honesty, critique, and contradiction. Sycophancy is unwelcome. If something seems wrong, say so.

---

## Working Style

### Always Build, Never Run

**Always build the engine after making C code changes** (see [Commands](#commands)). **Never run the executable** — the user will run and test themselves. Don't execute `./build/anchor.exe`, `run-web.bat`, or similar.

### Build Order

1. Get moment-to-moment gameplay working first (physics, core mechanics)
2. Then surrounding systems (metagame, UI, progression)
3. Polish and juice come throughout, not as a final phase

### Long Responses with Code

When providing answers that are long or contain multiple code examples, create a markdown file in `reference/` and open it in NeoVim with MarkdownPreview (see [Commands](#commands)). Use descriptive filenames like `anchor-loop-analysis.md`, `timer-system-notes.md`.
