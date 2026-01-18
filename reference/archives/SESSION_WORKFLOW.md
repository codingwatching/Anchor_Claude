# Session Workflow

When the user asks to end the session, follow this workflow. Complete all steps before committing (one commit per session).

See the Commands section in `.claude/CLAUDE.md` for quick reference.

---

## End of Session Steps

1. **Locate the session transcript** — find the most recent JSONL file
2. **Convert to Markdown** — use a lowercase, hyphenated title slug (e.g., `engine-phase-1.md`). Date-based filenames don't work with Blot.
3. **Read the converted log** to review the full session, especially if compacted. Summary must cover everything.
4. **Write a detailed summary:**
   - Chronological order (matching conversation flow)
   - Weight to one-off fixes, attempts, problems solved
   - Specific details: error messages, what was tried, what worked
5. **Create a short title** (max 30 characters)
6. **Show title + summary to user** — wait for approval before proceeding
7. **Prepend title + summary** to the log file (replace default header)
8. **Sync context files**
9. **Commit** with title as subject, full summary as body:

   **CRITICAL:** Copy summary text directly from the log file. Do not retype. The commit body must be character-for-character identical to the log file.

   ```bash
   git commit -m "Title

   [COPY-PASTE exact summary from log file]

   🤖 Generated with [Claude Code](https://claude.com/claude-code)

   Co-Authored-By: Claude <noreply@anthropic.com>"
   ```

10. **Push to GitHub**
11. **Push website to Blot**
12. **Confirm** completion to the user

---

## Log File Format

```markdown
# Short Title (Max 30 Chars)

## Summary

Opening paragraph: 1-2 sentences describing the session's main theme or focus.

**Category 1:**
- Bullet point with specific detail
- Another bullet point
  - Sub-bullet for related details

**Category 2:**
- Bullet point
- Another bullet point

---

[Transcript content follows]
```

### Format Rules

1. **Title**: Short, descriptive (max 30 characters). Examples: "Windows Setup", "Anchor Interview", "Log Generator Script Fix"

2. **Opening paragraph**: One or two sentences summarizing what the session was about. Sets context before the detailed breakdown.

3. **Category headers**: Bold text with colon (`**Category Name:**`). Group related work together. Examples:
   - System/environment work: "Windows Terminal Configuration", "NeoVim/Neovide Setup"
   - Feature work: "The Crucible Design (Chosen)", "Operator Redesign"
   - Bug fixes: "The Bug", "The Fix", "Linux-to-Windows Migration Fixes"
   - Documentation: "Documentation Updates", "Engine Documentation Cleanup"

4. **Bullet points**: Specific, detailed items under each category. Include:
   - What was done
   - Error messages encountered
   - What was tried, what worked, what didn't work
   - File names, function names, line numbers when relevant
   - Sub-bullets for grouping related details

5. **Chronological order**: Categories should roughly follow conversation flow. This helps future readers trace what happened and in what order.

6. **Separator**: Single `---` between summary and transcript

### Examples of Good Category Headers

- "Implementation Plan Interview" — for a specific activity
- "Consistency Fixes (multiple rounds, including pre-compaction work)" — with clarifying context
- "Continuation Session (Log Formatting Fixes)" — noting session boundaries
- "The Crucible Design (Chosen)" — marking decisions made

---

## Accidental Closure

If the terminal closes unexpectedly:
- The JSONL transcript exists with everything up to that point
- User can resume with `claude --continue`
- Complete the end-of-session workflow when ready

---

## After Claude Code Updates

Run `./scripts/patch-claude-code.sh` to fix the "File has been unexpectedly modified" bug on Windows. Must be rerun after every Claude Code update.

---

## Quick Website Push

Push website changes to Blot (uses separate repo at `E:/a327ex/anchor.blot.im/`):

```bash
cp -r E:/a327ex/Anchor/website/* E:/a327ex/anchor.blot.im/
cd E:/a327ex/anchor.blot.im && git add -A && git commit --allow-empty-message -m "" && git push origin master
```
