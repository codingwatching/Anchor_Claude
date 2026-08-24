#!/usr/bin/env python3
"""Convert JSONL transcripts to readable Markdown.

Supports:
  - **Claude Code** — messages use ``type`` / ``uuid`` / ``parentUuid`` (see below).
  - **Cursor (Composer) agent** — messages use ``role`` (``user`` / ``assistant``) and
    ``message.content`` parts (``text``, ``tool_use``). Tool results are often not
    embedded in exports; tool calls are listed for context.
  - **Codex** - rollout files under ``~/.codex/sessions/.../rollout-*.jsonl`` with
    ``response_item`` entries for messages, tool calls, and tool results.
  - **Grok** — ``~/.grok/sessions/<cwd>/<id>/chat_history.jsonl`` with ``type``
    in ``system`` / ``user`` / ``assistant`` / ``reasoning`` / ``tool_result``.
"""

import json
import os
import sys
import re
from datetime import datetime

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

def is_cursor_transcript(jsonl_path):
    """True if file is Cursor agent JSONL (``role``-based lines)."""
    try:
        with open(jsonl_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                msg = json.loads(line)
                return 'role' in msg and 'type' not in msg
    except (OSError, json.JSONDecodeError, UnicodeDecodeError):
        pass
    return False


def is_codex_transcript(jsonl_path):
    """True if file is a Codex rollout JSONL transcript."""
    try:
        with open(jsonl_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                msg = json.loads(line)
                msg_type = msg.get('type')
                if msg_type == 'session_meta':
                    return True
                if msg_type in ('response_item', 'turn_context', 'event_msg') and 'payload' in msg:
                    return True
                return False
    except (OSError, json.JSONDecodeError, UnicodeDecodeError):
        pass
    return False


def is_grok_transcript(jsonl_path):
    """True if file is a Grok chat_history.jsonl transcript."""
    normalized = jsonl_path.replace('\\', '/')
    if '/.grok/sessions/' in normalized and normalized.endswith('chat_history.jsonl'):
        return True
    try:
        with open(jsonl_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                msg = json.loads(line)
                msg_type = msg.get('type')
                if msg_type in ('reasoning', 'tool_result') and 'message' not in msg:
                    return True
                if msg_type in ('system', 'user', 'assistant') and 'content' in msg and 'message' not in msg:
                    if msg_type == 'assistant' and 'tool_calls' in msg:
                        return True
                    if msg_type == 'system' and isinstance(msg.get('content'), str):
                        return True
                return False
    except (OSError, json.JSONDecodeError, UnicodeDecodeError):
        pass
    return False


def codex_text_parts(content):
    """Yield displayable text parts from Codex message content."""
    if isinstance(content, str):
        yield content
    elif isinstance(content, list):
        for item in content:
            if not isinstance(item, dict):
                continue
            if item.get('type') in ('input_text', 'output_text', 'text'):
                text = item.get('text')
                if text:
                    yield text


def extract_user_query_text(text):
    """Strip ``<user_query>`` wrapper used in Cursor exports."""
    if not text or not isinstance(text, str):
        return text
    m = re.search(r'<user_query>\s*(.*?)\s*</user_query>', text, re.DOTALL | re.IGNORECASE)
    if m:
        return m.group(1).strip()
    return text


def strip_redacted_placeholders(text):
    """Remove lines that are only ``[REDACTED]`` (Cursor omits content but leaves this marker)."""
    if not text or not isinstance(text, str):
        return text
    lines = text.split('\n')
    filtered = [ln for ln in lines if ln.strip() != '[REDACTED]']
    out = '\n'.join(filtered)
    out = re.sub(r'\n{3,}', '\n\n', out)
    return out.strip()


def format_cursor_tool_input(tool_name, tool_input):
    """Format Cursor / Composer tool_use for display (uses ``path`` not ``file_path``)."""
    if not isinstance(tool_input, dict):
        return str(tool_input)[:120]
    if tool_name in ('Read', 'ReadLints', 'Delete'):
        return f"({tool_input.get('path', tool_input.get('file_path', ''))})"
    if tool_name == 'Write':
        return f"({tool_input.get('path', '')})"
    if tool_name == 'StrReplace':
        return f"({tool_input.get('path', '')})"
    if tool_name == 'Shell':
        cmd = tool_input.get('command', '')
        if len(cmd) > 80:
            cmd = cmd[:80] + '...'
        return f"({cmd})"
    if tool_name == 'Grep':
        return f"({tool_input.get('pattern', '')})"
    if tool_name == 'Glob':
        return f"({tool_input.get('glob_pattern', tool_input.get('pattern', ''))})"
    if tool_name == 'SemanticSearch':
        return f"({tool_input.get('query', '')})"
    if tool_name == 'WebFetch':
        return f"({tool_input.get('url', '')})"
    if tool_name == 'WebSearch':
        return f"({tool_input.get('search_term', tool_input.get('query', ''))})"
    if tool_name == 'Task':
        return f"({tool_input.get('description', '')})"
    if tool_name == 'GenerateImage':
        return f"({tool_input.get('description', '')[:60]}...)"
    raw = json.dumps(tool_input, ensure_ascii=False)
    if len(raw) > 120:
        raw = raw[:117] + '...'
    return f"({raw})"


def format_codex_tool_input(tool_name, arguments):
    """Format Codex tool call arguments for display."""
    parsed = None
    if isinstance(arguments, str):
        try:
            parsed = json.loads(arguments)
        except json.JSONDecodeError:
            parsed = None
    elif isinstance(arguments, dict):
        parsed = arguments

    if isinstance(parsed, dict):
        if tool_name == 'shell_command':
            command = parsed.get('command', '')
            if len(command) > 80:
                command = command[:80] + '...'
            return f"({command})"
        if tool_name == 'apply_patch':
            return "(patch)"
        if tool_name in ('read_mcp_resource', 'view_image'):
            return f"({parsed.get('uri', parsed.get('path', ''))})"
        raw = json.dumps(parsed, ensure_ascii=False)
        if len(raw) > 120:
            raw = raw[:117] + '...'
        return f"({raw})"

    if isinstance(arguments, str):
        text = arguments.replace('\n', ' ')
        if len(text) > 120:
            text = text[:117] + '...'
        return f"({text})"

    return ""


def format_tool_input(tool_name, tool_input):
    """Format tool input for display.

    Every tool gets a non-empty summary: known tools show their most telling
    argument; anything unknown falls back to a truncated JSON dump of the
    input (same design as the Grok formatter) so no summary line is ever
    blank in the published log.
    """
    if not isinstance(tool_input, dict):
        return ""
    if tool_name == "Read":
        return f"({tool_input.get('file_path', '')})"
    elif tool_name == "Write":
        return f"({tool_input.get('file_path', '')})"
    elif tool_name == "Edit":
        return f"({tool_input.get('file_path', '')})"
    elif tool_name in ("Bash", "PowerShell"):
        cmd = ' '.join(tool_input.get('command', '').split())
        if len(cmd) > 80:
            cmd = cmd[:80] + "..."
        return f"({cmd})"
    elif tool_name == "WebFetch":
        return f"({tool_input.get('url', '')})"
    elif tool_name == "WebSearch":
        return f"({tool_input.get('query', '')})"
    elif tool_name == "Grep":
        return f"({tool_input.get('pattern', '')})"
    elif tool_name == "Glob":
        return f"({tool_input.get('pattern', '')})"
    elif tool_name in ("Task", "Agent"):
        return f"({tool_input.get('description', '')})"
    elif tool_name == "Skill":
        skill = tool_input.get('skill', '')
        args = tool_input.get('args', '')
        return f"({skill} {args})" if args else f"({skill})"
    elif tool_name == "SendUserFile":
        files = tool_input.get('files', [])
        if isinstance(files, list):
            names = ', '.join(str(f) for f in files)
        else:
            names = str(files)
        if len(names) > 100:
            names = names[:100] + '...'
        return f"({names})"
    elif tool_name == "ToolSearch":
        return f"({tool_input.get('query', '')})"
    elif tool_name == "TodoWrite":
        todos = tool_input.get('todos', [])
        return f"({len(todos)} todos)" if isinstance(todos, list) else ""
    elif tool_name == "AskUserQuestion":
        questions = tool_input.get('questions', [])
        first = ''
        if isinstance(questions, list) and questions and isinstance(questions[0], dict):
            first = questions[0].get('question', '')
        if len(first) > 80:
            first = first[:80] + '...'
        return f"({first})"
    elif tool_name == "NotebookEdit":
        return f"({tool_input.get('notebook_path', '')})"
    else:
        raw = json.dumps(tool_input, ensure_ascii=False)
        if len(raw) > 120:
            raw = raw[:117] + '...'
        return f"({raw})" if raw != '{}' else ""

def get_fence(content):
    """Get a code fence that won't conflict with content."""
    # Use tildes if content has backticks, otherwise use backticks
    if '```' in content:
        fence = "~~~"
        while fence in content:
            fence += "~"
        return fence
    else:
        return "```"

def html_escape(text):
    """Escape HTML special characters."""
    return text.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')

def strip_session_previews(text):
    """Strip first-message preview lines from find-recent-session output.

    The output format is groups of:
      TIMESTAMP SESSION_ID [<-- MOST RECENT]
         preview text...
         /path/to/file.jsonl

    Remove the preview lines (indented lines that don't contain .jsonl paths).
    """
    lines = text.split('\n')
    filtered = []
    for line in lines:
        if line.startswith('   ') and '.jsonl' not in line:
            continue
        filtered.append(line)
    return '\n'.join(filtered)

# Embedded binary payloads (screenshots and other media inside MCP tool results,
# or base64 passed through tool inputs) must NEVER reach a published log — a
# reader can rebuild the images, and screenshots can capture authenticated UIs.
# Two layers: format_tool_result replaces image items before serialization, and
# every converter passes its final output through this scrubber as a belt.
IMAGE_PAYLOAD_RE = re.compile(r'[A-Za-z0-9+/=]{500,}')

# When MEDIA_DIR is set, embedded images are DECODED and saved there (ordinal
# filenames), and the log text carries a private-path marker instead of the
# payload — the vault/media/ scheme: files live on the server in an unserved
# directory, the public log references them, and a rebuild is a mechanical
# marker->file substitution. Without MEDIA_DIR, payloads are simply stripped.
MEDIA_DIR = None
MEDIA_REF = 'media'
MEDIA_COUNT = 0

# --replays <gamedir>: weave replay markers into the transcript. Any .apr in
# <gamedir>/replays/ whose recording START time falls inside the session's
# message window gets a `::replay(<game> <file> <MB>)` directive inserted at
# the transcript position where the run began (between the last message before
# launch and the first message after). Filenames carry LOCAL time; jsonl
# timestamps are UTC - both are converted to epoch for comparison. The site's
# convert.lua renders the directive as a playable card (released games) or a
# sealed line. Matched files are printed so end-session can copy them.
REPLAYS_DIR = None

def collect_replays():
    import glob as _glob, time as _time
    if not REPLAYS_DIR:
        return []
    out = []
    game = os.path.basename(os.path.normpath(REPLAYS_DIR))
    for f in sorted(_glob.glob(os.path.join(REPLAYS_DIR, 'replays', '*.apr'))):
        name = os.path.basename(f)
        m = re.match(r'^(\d{4})(\d{2})(\d{2})-(\d{2})(\d{2})(\d{2})\.apr$', name)
        if not m:
            continue
        parts = [int(x) for x in m.groups()]
        try:
            epoch = _time.mktime((parts[0], parts[1], parts[2], parts[3], parts[4], parts[5], -1, -1, -1))
        except (OverflowError, ValueError):
            continue
        mb = os.path.getsize(f) / 1e6
        out.append({'epoch': epoch, 'file': name, 'game': game, 'mb': mb, 'emitted': False})
    return out


def msg_epoch(msg):
    ts = msg.get('timestamp')
    if not ts:
        return None
    try:
        dt = datetime.fromisoformat(ts.replace('Z', '+00:00'))
        return dt.timestamp()
    except ValueError:
        return None

def save_media_payload(b64, media_type=None):
    global MEDIA_COUNT
    import base64, os
    try:
        raw = base64.b64decode(b64, validate=False)
    except Exception:
        return None
    ext = 'bin'
    if raw[:3] == b'\xff\xd8\xff': ext = 'jpg'
    elif raw[:8] == b'\x89PNG\r\n\x1a\n': ext = 'png'
    elif raw[:6] in (b'GIF87a', b'GIF89a'): ext = 'gif'
    elif raw[:4] == b'RIFF' and raw[8:12] == b'WEBP': ext = 'webp'
    elif media_type and '/' in str(media_type): ext = str(media_type).split('/')[-1]
    if ext == 'bin' and len(raw) < 2048:
        return None   # not an image and tiny: likely not worth a file
    MEDIA_COUNT += 1
    name = '%03d.%s' % (MEDIA_COUNT, ext)
    os.makedirs(MEDIA_DIR, exist_ok=True)
    with open(os.path.join(MEDIA_DIR, name), 'wb') as f:
        f.write(raw)
    return MEDIA_REF + '/' + name

def strip_binary_payloads(text):
    def _repl(m):
        if MEDIA_DIR:
            ref = save_media_payload(m.group(0))
            if ref:
                return '[image stored privately: ' + ref + ']'
        return '[binary payload omitted]'
    return IMAGE_PAYLOAD_RE.sub(_repl, text)

def format_tool_result(result, max_lines=30):
    """Format tool result, truncating if needed."""
    if isinstance(result, list):
        # MCP results are lists of typed items; image items carry base64
        # screenshot data on a single line, which line-based truncation never
        # catches - replace them with a marker BEFORE serialization
        cleaned = []
        for it in result:
            if isinstance(it, dict) and it.get('type') == 'image':
                note = '[screenshot omitted]'
                src = it.get('source', {}) if isinstance(it.get('source'), dict) else {}
                if MEDIA_DIR and src.get('data'):
                    ref = save_media_payload(src.get('data'), src.get('media_type'))
                    if ref:
                        note = '[image stored privately: ' + ref + ']'
                cleaned.append({'type': 'image', 'note': note})
            else:
                cleaned.append(it)
        result = json.dumps(cleaned, indent=2)
    elif not isinstance(result, str):
        result = str(result)
    result = strip_binary_payloads(result)

    # Clean up system reminders
    if '<system-reminder>' in result:
        result = result[:result.find('<system-reminder>')].strip()

    lines = result.split('\n')
    if len(lines) > max_lines:
        result = '\n'.join(lines[:max_lines]) + f"\n... [{len(lines) - max_lines} more lines]"

    return result

def is_system_message(content):
    """Check if a user message is actually internal system output."""
    system_patterns = [
        r'^Caveat:',
        r'<command-name>',
        r'<local-command-stdout>',
        r'<command-message>',
        r'<command-args>',
    ]
    for pattern in system_patterns:
        if re.search(pattern, content):
            return True
    return False

def format_skill_injection(text):
    """Collapse skill-content injections into a details block.

    When a skill is invoked, the harness injects the full SKILL.md as a user
    message ("Base directory for this skill: <path>\\n\\n<content>"). Rendered
    as a blockquote that's hundreds of visible lines of reference material;
    fold it behind a [skill: <name>] summary instead. Returns markdown or None
    when the text is not a skill injection.
    """
    if not isinstance(text, str):
        return None
    m = re.match(r"\s*Base directory for this skill:\s*(\S+)\s*\n(.*)$", text, re.DOTALL)
    if not m:
        return None
    name = re.split(r"[\\/]+", m.group(1).rstrip("\\/"))[-1]
    body = html_escape(m.group(2).strip())
    return (f"<details>\n<summary><code>[skill: {html_escape(name)}]</code></summary>\n\n"
            f"<pre><code>{body}</code></pre>\n\n</details>\n\n")


def format_local_command(content):
    """Render Claude Code local-command blocks (/model, /clear, ...) compactly.

    Returns markdown to emit, '' to drop the item entirely, or None when the
    content is not a local-command block (caller falls through to its normal
    handling). Replaces the old behavior of dumping the raw <command-name>/
    <local-command-stdout> XML into a visible fenced block.
    """
    if not isinstance(content, str):
        return None
    stripped = content.lstrip()
    if stripped.startswith('<local-command-caveat>') or stripped.startswith('Caveat:'):
        return ''
    m = re.search(r'<command-name>(.*?)</command-name>', content, re.DOTALL)
    if m:
        name = m.group(1).strip()
        args_m = re.search(r'<command-args>(.*?)</command-args>', content, re.DOTALL)
        args = args_m.group(1).strip() if args_m else ''
        label = f"{name} {args}".strip()
        return f"> `{label}`\n\n"
    m = re.search(r'<local-command-stdout>(.*?)</local-command-stdout>', content, re.DOTALL)
    if m:
        out = m.group(1).strip()
        if not out:
            return ''
        escaped = html_escape(format_tool_result(out))
        return (f"<details>\n<summary><code>[command output]</code></summary>\n\n"
                f"<pre><code>{escaped}</code></pre>\n\n</details>\n\n")
    return None


def filter_rewound_messages(messages):
    """Filter out rewound messages by resolving branch points.

    A genuine rewind looks like: user sends message A, assistant responds,
    user presses Esc and resends an edited A' with the same parentUuid as
    A. Both children of the shared parent are user messages. We keep the
    branch whose chain reaches the latest line in the file.

    Mixed-type branch points (e.g. one user child + one system child whose
    parent re-anchors a system-reminder onto a previous user message, or
    one assistant child + one user child at file start) are NOT rewinds —
    they're harness side-effects where one branch is the conversation
    continuing normally and the other is metadata reattached to an
    earlier message. We must NOT orphan them; doing so silently drops
    large stretches of real conversation.

    Heuristic: only treat a parent as a branch point if it has *multiple
    user-typed children*. Other multi-child parents are kept whole.
    """
    if not messages:
        return messages

    # Build uuid -> message map with file index for ordering
    uuid_map = {}
    uuid_to_index = {}
    for i, msg in enumerate(messages):
        uuid = msg.get('uuid')
        if uuid:
            uuid_map[uuid] = msg
            uuid_to_index[uuid] = i

    # children_of: all children (used for descendant traversal).
    # user_children_of: only user-typed children (used for rewind detection).
    from collections import defaultdict
    children_of = defaultdict(list)
    user_children_of = defaultdict(list)
    for msg in messages:
        parent_uuid = msg.get('parentUuid')
        uuid = msg.get('uuid')
        if parent_uuid and uuid:
            children_of[parent_uuid].append(uuid)
            if msg.get('type') == 'user':
                user_children_of[parent_uuid].append(uuid)

    orphaned = set()

    def get_latest_descendant_index(uuid):
        """Get the highest file index reachable from this uuid."""
        visited = set()
        max_index = uuid_to_index.get(uuid, -1)
        stack = [uuid]
        while stack:
            current = stack.pop()
            if current in visited:
                continue
            visited.add(current)
            idx = uuid_to_index.get(current, -1)
            if idx > max_index:
                max_index = idx
            for child in children_of.get(current, []):
                stack.append(child)
        return max_index

    def mark_orphaned(uuid):
        """Mark this uuid and all descendants as orphaned."""
        stack = [uuid]
        while stack:
            current = stack.pop()
            if current in orphaned:
                continue
            orphaned.add(current)
            for child in children_of.get(current, []):
                stack.append(child)

    # Only fire orphaning logic at *user-vs-user* branch points (genuine rewinds).
    for parent_uuid, user_children in user_children_of.items():
        if len(user_children) > 1:
            best_child = None
            best_index = -1
            for child in user_children:
                latest = get_latest_descendant_index(child)
                if latest > best_index:
                    best_index = latest
                    best_child = child
            for child in user_children:
                if child != best_child:
                    mark_orphaned(child)

    filtered = []
    for msg in messages:
        uuid = msg.get('uuid')
        if uuid is None or uuid not in orphaned:
            filtered.append(msg)

    return filtered

def convert_cursor_jsonl_to_markdown(jsonl_path, output_path=None):
    """Convert Cursor / Composer agent JSONL to Markdown."""
    messages = []
    with open(jsonl_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                messages.append(json.loads(line))
            except json.JSONDecodeError:
                continue

    try:
        mtime = os.path.getmtime(jsonl_path)
        dt = datetime.fromtimestamp(mtime)
        header = f"# Session {dt.strftime('%Y-%m-%d %H:%M')} (file mtime)\n\n---\n\n"
    except OSError:
        header = "# Session\n\n---\n\n"

    output = header

    for msg in messages:
        role = msg.get('role')
        content = msg.get('message', {}).get('content')

        if role == 'user':
            if isinstance(content, list):
                for item in content:
                    if item.get('type') != 'text':
                        continue
                    text = item.get('text', '')
                    text = extract_user_query_text(text)
                    if not text:
                        continue
                    if is_system_message(text):
                        fence = get_fence(text)
                        output += f"{fence}\n{text}\n{fence}\n\n"
                    else:
                        lines = text.split('\n')
                        quoted = '\n'.join(f"> {line}" for line in lines)
                        output += f"{quoted}\n\n"
            elif isinstance(content, str):
                text = extract_user_query_text(content)
                lines = text.split('\n')
                quoted = '\n'.join(f"> {line}" for line in lines)
                output += f"{quoted}\n\n"

        elif role == 'assistant':
            if not isinstance(content, list):
                continue
            for item in content:
                itype = item.get('type')
                if itype == 'text':
                    text = item.get('text', '')
                    text = strip_redacted_placeholders(text)
                    if text:
                        output += f"{text}\n\n"
                elif itype == 'tool_use':
                    tool_name = item.get('name', 'Unknown')
                    tool_input = item.get('input', {})
                    fmt = format_cursor_tool_input(tool_name, tool_input)
                    output += f"<details>\n<summary><code>{tool_name} {html_escape(fmt)}</code></summary>\n\n"
                    output += f"<pre><code>{html_escape(json.dumps(tool_input, indent=2, ensure_ascii=False)[:8000])}"
                    if len(json.dumps(tool_input)) > 8000:
                        output += "\n... [truncated]"
                    output += "</code></pre>\n\n</details>\n\n"

    if output_path:
        with open(output_path, 'w', encoding='utf-8') as f:
            output = strip_binary_payloads(output)
            f.write(output)
        print(f"Written to {output_path}")
    else:
        print(strip_binary_payloads(output))

    return output


def convert_claude_jsonl_to_markdown(jsonl_path, output_path=None):
    """Convert Claude Code JSONL transcript to Markdown."""

    messages = []
    with open(jsonl_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                msg = json.loads(line)
                messages.append(msg)
            except json.JSONDecodeError:
                continue

    # Filter out rewound messages
    messages = filter_rewound_messages(messages)

    # Extract session start time
    start_time = None
    for msg in messages:
        if msg.get('timestamp') and not start_time:
            start_time = msg['timestamp']
            break

    # Format header
    if start_time:
        dt = datetime.fromisoformat(start_time.replace('Z', '+00:00'))
        header = f"# Session {dt.strftime('%Y-%m-%d %H:%M')}\n\n---\n\n"
    else:
        header = "# Session\n\n---\n\n"

    output = header
    pending_tools = {}  # tool_id -> (tool_name, formatted_input)

    replays = collect_replays()

    for msg in messages:
        msg_type = msg.get('type')
        content = msg.get('message', {}).get('content')

        # replay markers: a run that started before this message lands here
        ep = msg_epoch(msg)
        if ep is not None:
            for r in replays:
                if not r['emitted'] and r['epoch'] <= ep:
                    r['emitted'] = True
                    output += '::replay(%s %s %.0f)' % (r['game'], r['file'], r['mb']) + chr(10) + chr(10)
                    print("replay marker: %s/%s (%.0f MB)" % (r['game'], r['file'], r['mb']))

        if not content:
            continue

        # User message (plain text)
        if msg_type == 'user' and isinstance(content, str):
            skill = format_skill_injection(content)
            local = format_local_command(content)
            # Check if it's internal system output
            if skill is not None:
                output += skill
            elif local is not None:
                output += local
            elif is_system_message(content):
                fence = get_fence(content)
                output += f"{fence}\n{content}\n{fence}\n\n"
            # Check if it's HTML content (pasted HTML)
            elif content.strip().startswith('<') and re.search(r'<(p|div|details|pre|blockquote|h[1-6]|ul|ol|table)\b', content):
                # Put HTML content in a collapsible details block
                escaped = html_escape(content)
                output += f"<details>\n<summary><code>[Pasted HTML content]</code></summary>\n\n"
                output += f"<pre><code>{escaped}</code></pre>\n\n"
                output += "</details>\n\n"
            else:
                # Prefix every line with > for proper blockquote
                lines = content.split('\n')
                quoted = '\n'.join(f"> {line}" for line in lines)
                output += f"{quoted}\n\n"

        # User message (list content: tool results, images, text)
        elif msg_type == 'user' and isinstance(content, list):
            for item in content:
                item_type = item.get('type')

                if item_type == 'tool_result':
                    tool_id = item.get('tool_use_id')
                    result = item.get('content', '')

                    # Get the pending tool info
                    tool_info = pending_tools.pop(tool_id, None)

                    if tool_info:
                        tool_name, formatted_input, tool_input_data = tool_info

                        # TodoWrite: the harness result is boilerplate; the list
                        # itself lives in the tool INPUT — render it as a
                        # status checklist instead
                        if tool_name == 'TodoWrite':
                            todos = tool_input_data.get('todos', [])
                            result_text = '\n'.join(
                                f"- [{t.get('status', '?')}] {t.get('content', '')}"
                                for t in todos if isinstance(t, dict))
                        else:
                            result_text = format_tool_result(result)

                        # Strip message previews from find-recent-session output
                        if tool_name == 'Bash' and 'find-recent-session' in tool_input_data.get('command', ''):
                            result_text = strip_session_previews(result_text)

                        if result_text:
                            # Use HTML pre/code since markdown isn't processed inside HTML blocks
                            escaped = html_escape(result_text)
                            output += f"<details>\n<summary><code>{html_escape(tool_name)} {html_escape(formatted_input)}</code></summary>\n\n"
                            output += f"<pre><code>{escaped}</code></pre>\n\n"
                            output += "</details>\n\n"
                        else:
                            # No result, just show command in blockquoted code block
                            output += f"> `{tool_name} {formatted_input}`\n\n"

                elif item_type == 'image':
                    # Skip base64 image data, just note that an image was pasted
                    media_type = item.get('source', {}).get('media_type', 'image')
                    output += f"> [Pasted {media_type}]\n\n"

                elif item_type == 'text':
                    # Text content in a list
                    text = item.get('text', '')
                    if text:
                        skill = format_skill_injection(text)
                        local = format_local_command(text)
                        if skill is not None:
                            output += skill
                        elif local is not None:
                            output += local
                        elif is_system_message(text):
                            fence = get_fence(text)
                            output += f"{fence}\n{text}\n{fence}\n\n"
                        else:
                            lines = text.split('\n')
                            quoted = '\n'.join(f"> {line}" for line in lines)
                            output += f"{quoted}\n\n"

        # Assistant message
        elif msg_type == 'assistant' and isinstance(content, list):
            for item in content:
                item_type = item.get('type')

                if item_type == 'thinking':
                    # Full thinking trace, collapsed — same [Think] rendering
                    # as the Grok converter (which is the whole reason it
                    # exists there: the traces read as the session's inner
                    # narration). Claude stores them untruncated.
                    think = item.get('thinking', '')
                    if think and isinstance(think, str):
                        escaped = html_escape(think)
                        output += f"<details>\n<summary>[Think]</summary>\n\n<pre><code>{escaped}</code></pre>\n\n</details>\n\n"

                elif item_type == 'text':
                    text = item.get('text', '')
                    if text:
                        output += f"{text}\n\n"

                elif item_type == 'tool_use':
                    tool_name = item.get('name', 'Unknown')
                    tool_input = item.get('input', {})
                    tool_id = item.get('id')

                    formatted_input = format_tool_input(tool_name, tool_input)

                    # Store for matching with result
                    if tool_id:
                        pending_tools[tool_id] = (tool_name, formatted_input, tool_input)

    if output_path:
        with open(output_path, 'w', encoding='utf-8') as f:
            output = strip_binary_payloads(output)
            f.write(output)
        print(f"Written to {output_path}")
    else:
        print(strip_binary_payloads(output))

    return output


def convert_codex_jsonl_to_markdown(jsonl_path, output_path=None):
    """Convert Codex rollout JSONL to Markdown."""
    messages = []
    with open(jsonl_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                messages.append(json.loads(line))
            except json.JSONDecodeError:
                continue

    start_time = None
    for msg in messages:
        if msg.get('type') == 'session_meta':
            start_time = msg.get('payload', {}).get('timestamp') or msg.get('timestamp')
            break
        if msg.get('timestamp'):
            start_time = msg['timestamp']
            break

    if start_time:
        dt = datetime.fromisoformat(start_time.replace('Z', '+00:00'))
        header = f"# Session {dt.strftime('%Y-%m-%d %H:%M')}\n\n---\n\n"
    else:
        header = "# Session\n\n---\n\n"

    output = header
    pending_tools = {}

    for msg in messages:
        if msg.get('type') != 'response_item':
            continue

        payload = msg.get('payload', {})
        payload_type = payload.get('type')

        if payload_type == 'message':
            role = payload.get('role')
            if role == 'user':
                for text in codex_text_parts(payload.get('content')):
                    if text.lstrip().startswith('<environment_context>'):
                        continue
                    if is_system_message(text):
                        fence = get_fence(text)
                        output += f"{fence}\n{text}\n{fence}\n\n"
                    else:
                        lines = text.split('\n')
                        quoted = '\n'.join(f"> {line}" for line in lines)
                        output += f"{quoted}\n\n"

            elif role == 'assistant':
                for text in codex_text_parts(payload.get('content')):
                    output += f"{text}\n\n"

        elif payload_type == 'function_call':
            call_id = payload.get('call_id')
            tool_name = payload.get('name', 'Unknown')
            arguments = payload.get('arguments', '')
            if call_id:
                pending_tools[call_id] = (tool_name, format_codex_tool_input(tool_name, arguments))

        elif payload_type == 'function_call_output':
            call_id = payload.get('call_id')
            tool_info = pending_tools.pop(call_id, None)
            result = payload.get('output', '')

            if tool_info:
                tool_name, formatted_input = tool_info
            else:
                tool_name, formatted_input = 'Tool', ''

            result_text = format_tool_result(result)
            if result_text:
                escaped = html_escape(result_text)
                output += f"<details>\n<summary><code>{tool_name} {formatted_input}</code></summary>\n\n"
                output += f"<pre><code>{escaped}</code></pre>\n\n"
                output += "</details>\n\n"
            else:
                output += f"> `{tool_name} {formatted_input}`\n\n"

    if output_path:
        with open(output_path, 'w', encoding='utf-8') as f:
            output = strip_binary_payloads(output)
            f.write(output)
        print(f"Written to {output_path}")
    else:
        print(strip_binary_payloads(output))

    return output


def format_grok_tool_input(tool_name, arguments):
    """Format Grok tool_calls arguments for display."""
    parsed = None
    if isinstance(arguments, str):
        try:
            parsed = json.loads(arguments)
        except json.JSONDecodeError:
            parsed = None
    elif isinstance(arguments, dict):
        parsed = arguments

    if not isinstance(parsed, dict):
        raw = arguments if isinstance(arguments, str) else json.dumps(arguments, ensure_ascii=False)
        if len(raw) > 120:
            raw = raw[:117] + '...'
        return f"({raw})" if raw else ""

    if tool_name in ('read_file', 'Read'):
        return f"({parsed.get('target_file', parsed.get('file_path', ''))})"
    if tool_name in ('search_replace', 'Write', 'Edit', 'StrReplace'):
        return f"({parsed.get('file_path', parsed.get('path', parsed.get('target_file', '')))})"
    if tool_name in ('run_terminal_command', 'Bash', 'Shell'):
        cmd = parsed.get('command', '')
        if len(cmd) > 80:
            cmd = cmd[:80] + '...'
        return f"({cmd})"
    if tool_name in ('grep', 'Grep'):
        return f"({parsed.get('pattern', '')})"
    if tool_name in ('list_dir', 'Glob'):
        return f"({parsed.get('target_directory', parsed.get('glob_pattern', parsed.get('pattern', '')))})"
    if tool_name in ('web_search', 'WebSearch'):
        return f"({parsed.get('query', parsed.get('search_term', ''))})"
    if tool_name in ('web_fetch', 'WebFetch', 'open_page'):
        return f"({parsed.get('url', '')})"
    if tool_name in ('spawn_subagent', 'Task'):
        return f"({parsed.get('description', parsed.get('prompt', '')[:80])})"
    raw = json.dumps(parsed, ensure_ascii=False)
    if len(raw) > 120:
        raw = raw[:117] + '...'
    return f"({raw})"


def grok_text_parts(content):
    """Yield text strings from a Grok message content field."""
    if isinstance(content, str):
        if content:
            yield content
    elif isinstance(content, list):
        for item in content:
            if not isinstance(item, dict):
                continue
            if item.get('type') in ('text', 'input_text', 'output_text'):
                text = item.get('text')
                if text:
                    yield text


def grok_user_visible_text(msg):
    """Return the user-facing text from a Grok user/system message, or None to skip."""
    if msg.get('synthetic_reason') == 'system_reminder':
        return None
    text = '\n'.join(grok_text_parts(msg.get('content')))
    if not text:
        return None
    extracted = extract_user_query_text(text)
    if extracted != text:
        return extracted.strip() or None
    stripped = text.lstrip()
    if stripped.startswith(('<user_info>', '<system-reminder>', '<rules>', '<always_applied_workspace_rules>')):
        return None
    if msg.get('type') == 'system':
        return None
    return text.strip() or None


def convert_grok_jsonl_to_markdown(jsonl_path, output_path=None):
    """Convert a Grok chat_history.jsonl transcript to Markdown."""
    messages = []
    with open(jsonl_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                messages.append(json.loads(line))
            except json.JSONDecodeError:
                continue

    try:
        summary_path = os.path.join(os.path.dirname(jsonl_path), 'summary.json')
        start_time = None
        if os.path.isfile(summary_path):
            with open(summary_path, 'r', encoding='utf-8') as sf:
                summary = json.load(sf)
            start_time = summary.get('created_at')
        if start_time:
            dt = datetime.fromisoformat(start_time.replace('Z', '+00:00'))
            header = f"# Session {dt.strftime('%Y-%m-%d %H:%M')}\n\n---\n\n"
        else:
            mtime = os.path.getmtime(jsonl_path)
            dt = datetime.fromtimestamp(mtime)
            header = f"# Session {dt.strftime('%Y-%m-%d %H:%M')} (file mtime)\n\n---\n\n"
    except (OSError, json.JSONDecodeError, ValueError):
        header = "# Session\n\n---\n\n"

    output = header
    pending_tools = {}

    for msg in messages:
        msg_type = msg.get('type')

        if msg_type == 'system':
            continue

        if msg_type == 'user':
            text = grok_user_visible_text(msg)
            if not text:
                continue
            if is_system_message(text):
                fence = get_fence(text)
                output += f"{fence}\n{text}\n{fence}\n\n"
            else:
                quoted = '\n'.join(f"> {line}" for line in text.split('\n'))
                output += f"{quoted}\n\n"
            continue

        if msg_type == 'reasoning':
            summaries = []
            for item in msg.get('summary') or []:
                if isinstance(item, dict) and item.get('text'):
                    summaries.append(item['text'])
            think = '\n'.join(summaries).strip()
            if think:
                escaped = html_escape(think)
                output += f"<details>\n<summary>[Think]</summary>\n\n<pre><code>{escaped}</code></pre>\n\n</details>\n\n"
            continue

        if msg_type == 'assistant':
            text = '\n'.join(grok_text_parts(msg.get('content'))).strip()
            if text:
                output += f"{text}\n\n"
            for call in msg.get('tool_calls') or []:
                call_id = call.get('id')
                tool_name = call.get('name', 'Unknown')
                arguments = call.get('arguments', '')
                formatted = format_grok_tool_input(tool_name, arguments)
                if call_id:
                    pending_tools[call_id] = (tool_name, formatted, arguments)
                else:
                    output += f"<details>\n<summary><code>{html_escape(tool_name)} {html_escape(formatted)}</code></summary>\n\n"
                    raw = arguments if isinstance(arguments, str) else json.dumps(arguments, indent=2, ensure_ascii=False)
                    output += f"<pre><code>{html_escape(raw[:8000])}"
                    if len(raw) > 8000:
                        output += "\n... [truncated]"
                    output += "</code></pre>\n\n</details>\n\n"
            continue

        if msg_type == 'tool_result':
            call_id = msg.get('tool_call_id')
            tool_info = pending_tools.pop(call_id, None)
            result = msg.get('content', '')
            if isinstance(result, list):
                result = '\n'.join(grok_text_parts(result))
            result_text = format_tool_result(result)
            if tool_info:
                tool_name, formatted_input, _arguments = tool_info
            else:
                tool_name, formatted_input = 'Tool', ''
            if result_text:
                escaped = html_escape(result_text)
                output += f"<details>\n<summary><code>{html_escape(tool_name)} {html_escape(formatted_input)}</code></summary>\n\n"
                output += f"<pre><code>{escaped}</code></pre>\n\n"
                output += "</details>\n\n"
            else:
                output += f"> `{tool_name} {formatted_input}`\n\n"
            continue

    if output_path:
        with open(output_path, 'w', encoding='utf-8') as f:
            output = strip_binary_payloads(output)
            f.write(output)
        print(f"Written to {output_path}")
    else:
        print(strip_binary_payloads(output))

    return output


def convert_jsonl_to_markdown(jsonl_path, output_path=None):
    """Auto-detect Claude Code, Cursor / Composer, Codex, or Grok and convert to Markdown."""
    if is_grok_transcript(jsonl_path):
        return convert_grok_jsonl_to_markdown(jsonl_path, output_path)
    if is_codex_transcript(jsonl_path):
        return convert_codex_jsonl_to_markdown(jsonl_path, output_path)
    if is_cursor_transcript(jsonl_path):
        return convert_cursor_jsonl_to_markdown(jsonl_path, output_path)
    return convert_claude_jsonl_to_markdown(jsonl_path, output_path)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: jsonl-to-markdown.py <input.jsonl> [output.md]")
        sys.exit(1)

    input_path = sys.argv[1]
    rest = [a for a in sys.argv[2:]]
    # optional: --media-dir DIR [--media-ref PREFIX] extracts embedded images
    # instead of stripping them (see save_media_payload)
    i = 0
    positional = []
    while i < len(rest):
        if rest[i] == '--media-dir' and i + 1 < len(rest):
            MEDIA_DIR = rest[i + 1]; i += 2
        elif rest[i] == '--media-ref' and i + 1 < len(rest):
            MEDIA_REF = rest[i + 1]; i += 2
        elif rest[i] == '--replays' and i + 1 < len(rest):
            REPLAYS_DIR = rest[i + 1]; i += 2
        else:
            positional.append(rest[i]); i += 1
    output_path = positional[0] if positional else None

    convert_jsonl_to_markdown(input_path, output_path)
