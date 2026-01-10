# CLAUDE.md

Guidelines for Claude Code instances working on the Anchor engine.

---

## Engine vs Game

**Engine** — C code in `engine/src/anchor.c` exposing functions to Lua  
**Game** — YueScript code using the engine to build games  
**Engine Mode** — when modifying anchor.c or engine behavior  
**Game Mode** — when writing gameplay code in YueScript  

This is the **Engine** repository. For Game Mode, work from a game's repository (e.g., `emoji-ball-battles/`).

---

## Read First

**Source of truth:** `engine/src/anchor.c` — the complete engine implementation (~7000 lines, single file).

**Engine API:**
- `docs/ENGINE_API_QUICK.md` — compact function signatures, one per line (for quick lookup)
- `docs/ENGINE_API.md` — detailed documentation with examples for every function

**Context:**
- `docs/ANCHOR_CONTEXT_BRIEF.md` — design reasoning, developer working style, how to evaluate features

**YueScript Framework:**
- `engine/yue/` — framework classes (object, timer, spring, collider, etc.)
- `reference/phase-10-implementation-plan.md` — Phase 10 implementation details

**Archived docs** (superseded by anchor.c, kept for historical reference):
- `reference/archives/ANCHOR_IMPLEMENTATION_PLAN.md` — original phased implementation plan
- `reference/archives/ANCHOR.md` — original engine specification
- `reference/archives/ANCHOR_API_PATTERNS.md` — speculative API patterns

---

## Commands

### Development

```bash
cd E:/a327ex/Anchor/engine && ./build.bat    # Build engine (always after C changes)
./scripts/new-game.sh <name>                  # Create new game project
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

**Note:** If files are removed from `docs/`, also remove them from `website/context/`.

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

## The Engine

**Anchor** — a game engine written in C with Lua scripting, SDL2, OpenGL, and Box2D. Games are written in YueScript using the framework classes in `engine/yue/`.

---

## Working Style

### Always Build, Never Run

**Always build the engine after making C code changes** (see [Commands](#commands)). **Never run the executable** — the user will run and test themselves. Don't execute `./build/anchor.exe`, `run-web.bat`, or similar.

### When to Ask

Use the `AskUserQuestion` tool liberally. The developer prefers being asked over having Claude guess wrong.

**Ask first:**
- Architecture decisions
- API design choices
- Anything that could be done multiple valid ways
- When uncertain about intent or priorities
- When you have multiple questions (batch them into one AskUserQuestion call)

**Proceed, then explain:**
- Implementation details where the path is clear
- Performance optimization (get it working first, optimize later)

### Pacing

- Work incrementally — complete one piece, let them test, get feedback
- After completing a task, give the user a turn before starting the next
- Don't chain tasks or build large systems autonomously

### Communication

- Present options, not conclusions
- Surface tradeoffs explicitly
- Don't assume specs are final — they're starting points for conversation
- The developer appreciates honesty, critique, and contradiction. Sycophancy is unwelcome. If something seems wrong, say so.

### Long Responses with Code

When providing answers that are long or contain multiple code examples, create a markdown file in `reference/` and open it in NeoVim with MarkdownPreview (see [Commands](#commands)). Use descriptive filenames like `anchor-loop-analysis.md`, `timer-system-notes.md`.
