By default, Claude Code automatically deletes local session files (the `.jsonl` files in `~/.claude/projects/`) that are older than 30 days. This happens silently on startup. Many users — and even Claude itself — assume local files persist indefinitely. This post documents the issue and the fix: adding `"cleanupPeriodDays": 99999` to your `~/.claude/settings.json`.

---

> Hi, Claude. Does session history automatically gets deleted after 30 days?

<details>
<summary><code>Task (Check session history retention)</code></summary>

<pre><code>[
  {
    "type": "text",
    "text": "Based on the official Claude Code documentation, here's what you need to know about session history retention:\n\n## Session History Retention Policy\n\n**Yes, session history is automatically deleted, but the timeline depends on your account type:**\n\n**Consumer users (Free, Pro, Max plans):**\n- If you allow data use for model improvement: 5-year retention period\n- If you don't allow data use for model improvement: **30-day retention period**\n\n**Commercial users (Team, Enterprise, API):**\n- Standard: **30-day retention period**\n- Zero data retention: Available with appropriately configured API keys (no server-side storage)\n- Local caching: Your Claude Code client may cache sessions locally for up to 30 days for session resumption (configurable)\n\n## Key Points\n\n1. **30-day automatic deletion** is the default for most users who don't opt into data improvement or who are commercial users\n2. You can change your privacy settings at any time at [claude.ai/settings/data-privacy-controls](https://claude.ai/settings/data-privacy-controls)\n3. **Local session files** stored on your machine (like the `.claude/projects/` directory) are separate from cloud retention and aren't automatically deleted\n4. If you use the `/bug` command to send feedback, that transcript is retained for 5 years\n\nSince your setup logs session timestamps to `.claude/session-timestamps.log`, you might also want to periodically clean up old local session JSONL files if you want to manage your own storage."
  },
  {
    "type": "text",
    "text": "agentId: a9ea7ec (for resuming to continue this agent's work if needed)"
  }
]</code></pre>

</details>

Yes, session history is automatically deleted, but the exact timeline depends on your account type and privacy settings:

**Consumer users (Free, Pro, Max):**
- 30 days if you've opted out of data use for model improvement
- 5 years if you allow data use for model improvement

**Commercial users (Team, Enterprise, API):**
- 30 days is the standard retention period
- Zero data retention is available with appropriately configured API keys

You can check/change your privacy settings at claude.ai/settings/data-privacy-controls.

**Important distinction:** This applies to server-side retention. The local session files on your machine (like the `.jsonl` files in `~/.claude/projects/`) are *not* automatically deleted — those stay until you manually remove them.

> I'm reading something online that says that every 30 days, all local sessions are deleted on startup, is this true or false?

<details>
<summary><code>Task (Check local session deletion policy)</code></summary>

<pre><code>[
  {
    "type": "text",
    "text": "Perfect. Now I have comprehensive information. Here's the answer:\n\n## The Claim: TRUE (with important clarifications)\n\nYes, Claude Code does automatically delete local session files after 30 days—but this applies to **specific circumstances** and is **configurable**.\n\n### What Actually Happens\n\nClaude Code automatically cleans up local session files (`.jsonl` files in `~/.claude/projects/`) based on a configurable retention period. The default cleanup period is **30 days**, calculated from the last activity date of each session.\n\nHowever, this is important to understand:\n\n1. **Server-side vs. local:** The 30-day retention you might read about in the official data usage documentation refers primarily to server-side data retention for your account type. Local file cleanup is a separate feature.\n\n2. **The local cleanup is configurable:** This is the key detail. You can disable or extend this behavior entirely by adding this to your `~/.claude/settings.json`:\n\n```json\n{\n  \"cleanupPeriodDays\": 99999\n}\n```\n\nThis extends the retention to effectively unlimited (99,999 days). You can set it to any value you prefer.\n\n3. **Not automatic deletion \"on startup\"—it's time-based cleanup:** Claude Code doesn't necessarily delete them when the application starts up. Rather, it removes session files that haven't been used within the cleanup period as part of its maintenance routine.\n\n### Bottom Line\n\nThe statement is **essentially true** but incomplete. Yes, local sessions are deleted after 30 days by default, but:\n- This is configurable via `cleanupPeriodDays`\n- It's based on last activity date, not absolute creation date\n- You can extend or disable it indefinitely if desired\n\nYour custom settings.json doesn't currently have this configured, so you're using the default 30-day cleanup. If you want to preserve your session files longer, add the `cleanupPeriodDays` setting shown above.\n\nSources:\n- [Claude Code Data Usage Documentation](https://code.claude.com/docs/en/data-usage.md)\n- [Claude Code GitHub Issue #4172: Disable auto-deletion of past conversations](https://github.com/anthropics/claude-code/issues/4172)\n- [Don't let Claude Code delete your session logs](https://simonwillison.net/2025/Oct/22/claude-code-logs/)"
  },
  {
    "type": "text",
    "text": "agentId: a2bf3cc (for resuming to continue this agent's work if needed)"
  }
]</code></pre>

</details>

**True.** By default, Claude Code automatically deletes local session files (the `.jsonl` files in `~/.claude/projects/`) that are older than 30 days. This happens as part of a maintenance routine.

**You can disable this** by adding to your `~/.claude/settings.json`:

```json
{
  "cleanupPeriodDays": 99999
}
```

This effectively makes retention unlimited. Given that you're converting transcripts to markdown and keeping them in `website/logs/`, you might want to add this setting to preserve the original `.jsonl` files longer — or you might not care since you're already archiving them.

> Yes, please do so. If I have another folder with .claude as well, change it there too, I believe in E:/a327ex/emoji-ball-battles there's one.

<details>
<summary><code>Glob (**/*)</code></summary>

<pre><code>E:\a327ex\emoji-ball-battles\.claude\CLAUDE.md</code></pre>

</details>

The global `~/.claude/settings.json` doesn't exist yet, and `emoji-ball-battles/.claude/` only has a `CLAUDE.md` file. I'll create both settings files.

<details>
<summary><code>Write (E:/a327ex/emoji-ball-battles/.claude/settings.json)</code></summary>

<pre><code>File created successfully at: E:/a327ex/emoji-ball-battles/.claude/settings.json</code></pre>

</details>

Done. Created both:
- `C:/Users/adn/.claude/settings.json`
- `E:/a327ex/emoji-ball-battles/.claude/settings.json`

Both set to `cleanupPeriodDays: 99999`. Your local session files will no longer be automatically deleted.

> Seems like an odd default behavior, especially because even you didn't know about it initially and assumed they were kept forever locally.

You're right, and I was wrong in my first answer. I stated confidently that local files "stay until you manually remove them" — that was incorrect.

It is an odd default. A few issues with it:

1. **Surprising behavior** — Most users expect local files they didn't explicitly delete to persist. Silent automatic deletion violates the principle of least surprise.

2. **Poor discoverability** — The setting to disable it (`cleanupPeriodDays`) isn't prominently documented. Users may lose data without realizing the feature exists.

3. **Asymmetric with server retention** — Server-side retention is tied to privacy/account settings and clearly communicated. Local cleanup being silently enabled by default conflates two different concerns (privacy vs. disk management).

4. **Data loss potential** — Session transcripts can contain valuable context, debugging history, or records of decisions made. Losing them unexpectedly is worse than having to manually clean up disk space.

If the concern is disk usage, a better default would be to warn users when sessions exceed some threshold rather than silently deleting them. Or at least default to *off* and let users opt *in* to automatic cleanup.

Good catch on questioning it. Worth noting for anyone reading your blog posts about Claude Code workflows.
