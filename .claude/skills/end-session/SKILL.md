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
# Find latest transcript
ls -t ~/.claude/projects/E--a327ex-Anchor/*.jsonl | grep -v agent | head -1

# Convert to markdown (use lowercase hyphenated slug)
python E:/a327ex/Anchor/scripts/jsonl-to-markdown.py [JSONL_PATH] E:/a327ex/Anchor/website/logs/[slug].md
```

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

Format (from SESSION_WORKFLOW.md):

```markdown
# [Title]

## Summary

[1-2 sentence overview of the session's main focus]

**[Category 1]:**
- Specific detail
- Another detail
  - Sub-detail if needed

**[Category 2]:**
- Detail
- Detail

---

[Rest of transcript follows]
```

Rules:
- **Chronological order** — Categories should match conversation flow
- **Specific details** — Error messages, file names, what was tried
- **Weight planning equally** — Research, proposals, alternatives considered, user feedback on approach are as important as implementation
- **Weight problems solved** — Errors, fixes, user corrections matter

## Step 5: Get User Approval

Show the title and summary to the user. Wait for approval before proceeding. If they have corrections, fix them.

## Step 6: Update Log File

Replace the default header (`# Session YYYY-MM-DD...`) with the approved title and summary.

## Step 7: Sync and Commit

```bash
# Sync context files
cp E:/a327ex/Anchor/.claude/CLAUDE.md E:/a327ex/Anchor/docs/* E:/a327ex/Anchor/website/context/

# Stage files (exclude build artifacts and temp files)
cd E:/a327ex/Anchor
git add .claude/ docs/ framework/ engine/ scripts/ website/ reference/

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

## Step 8: Push

```bash
# Push to GitHub
git push origin main

# Push website to Blot
git subtree push --prefix=website blot master
```

## Step 9: Confirm

Tell the user:
- Commit hash
- That GitHub push succeeded
- That Blot push succeeded
