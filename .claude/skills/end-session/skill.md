---
name: end-session
description: End the current session. Converts transcript to markdown, writes summary, syncs files, commits, and pushes to GitHub and Blot.
---

# End Session Workflow

When the user invokes this skill, follow these steps exactly. Complete all steps before the final push.

## Step 1: Get Session Title

Ask the user for a session title (max 30 characters). Examples: "Anchor Phase 10 Part 5", "Windows Setup", "Timer System Fix"

## Step 2: Find and Convert Transcript

```bash
# Find the two most recent transcripts (current session + previous session to summarize)
ls -t ~/.claude/projects/E--a327ex-Anchor/*.jsonl | grep -v agent | head -2

# The SECOND file is the session to summarize (the first is this current end-session conversation)
# Convert to markdown directly to Blot folder (use lowercase hyphenated slug)
python E:/a327ex/Anchor/scripts/jsonl-to-markdown.py [SECOND_JSONL_PATH] E:/a327ex/anchor.blot.im/logs/[slug].md
```

**Note:** When running this skill in a fresh session, the most recent transcript is the current end-session conversation. The second most recent is the actual work session to summarize.

## Step 3: Read the Full Log (CRITICAL)

The log is often too large to read in one pass. You MUST read it systematically:

1. **Read in sequential chunks** — Start from the beginning, read 400-500 lines at a time
2. **Build a chronological outline** — As you read each chunk, note:
   - Key events/tasks in order they occurred
   - Planning phases: proposals, alternatives considered, user feedback on approach
   - Research: docs read, code examined, references consulted
   - Errors encountered and how they were fixed
   - Decisions made and why
   - User corrections or feedback
3. **Continue until you reach the end** — Don't skip sections
4. **Only then write the summary** — Use your outline to ensure correct chronological order

This prevents the error of misordering events or missing portions of the session.

## Step 4: Write Summary

The summary should be **thorough and detailed**. Each major topic deserves its own section with multiple specific bullet points. Don't compress — expand.

**Purpose:** These summaries serve as searchable records. Future Claude instances will grep through past logs to find how specific topics were handled. The more detail you include, the more useful the summary becomes for finding relevant context later.

Format (this is just an example structure — adapt sections to match what actually happened):

```markdown
# [Title]

## Summary

[1-2 sentence overview of the session's main focus]

**[Topic 1 - e.g., "Spring Module Implementation"]:**
- First specific detail about what was done
- Second detail - include file names, function names
- User correction or feedback (quote if notable)
- Technical decisions and why

**[Topic 2 - e.g., "Camera Research"]:**
- What was researched
- Key findings
- How it influenced implementation

**[Topic 3 - e.g., "Errors and Fixes"]:**
- Specific error message encountered
- Root cause identified
- How it was fixed

[Continue for each major topic...]

---

[Rest of transcript follows]
```

Rules:
- **Be thorough** — If in doubt, include more detail, not less. Each topic should be as detailed as possible while still being a summary.
- **Think searchability** — Future instances will search these logs. Include keywords, function names, error messages that someone might grep for.
- **One section per major topic** — Don't combine unrelated work into one section
- **Chronological order** — Sections should match conversation flow
- **Specific details** — Error messages, file names, function names, parameter values
- **Include user quotes** — When user gave notable feedback, quote it (e.g., "k/d variables are not intuitive at all")
- **Weight planning equally** — Research, proposals, alternatives considered, user feedback on approach are as important as implementation
- **Weight problems solved** — Errors, root causes, fixes, user corrections all matter
- **Technical specifics** — Include formulas, API signatures, parameter changes when relevant

## Step 5: Get User Approval

Show the title and summary to the user. Wait for approval before proceeding. If they have corrections, fix them.

## Step 6: Update Log File

Replace the default header (`# Session YYYY-MM-DD...`) with the approved title and summary.

## Step 7: Sync Context Files to Blot

```bash
# Sync context files directly to Blot repo (rename CLAUDE.md to avoid conflicts)
cp E:/a327ex/Anchor/.claude/CLAUDE.md E:/a327ex/anchor.blot.im/context/CLAUDE-anchor.md
cp E:/a327ex/Anchor/docs/* E:/a327ex/anchor.blot.im/context/
```

## Step 8: Commit Anchor Repo

```bash
# Stage files (exclude build artifacts and temp files)
cd E:/a327ex/Anchor
git add .claude/ docs/ framework/ engine/ scripts/ reference/

# Check what's staged
git status
```

Commit using HEREDOC with exact summary from log file:

```bash
git commit -m "$(cat <<'EOF'
[Title]

[EXACT summary text from log file]

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

## Step 9: Push Both Repos

```bash
# Push Anchor to GitHub
git push origin main

# Push Blot repo (logs and context are already there)
cd E:/a327ex/anchor.blot.im && git add -A && git commit -m "[Title]" && git push origin master
```

## Step 10: Confirm

Tell the user:
- Commit hash
- That GitHub push succeeded
- That Blot push succeeded
