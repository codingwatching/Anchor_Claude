#!/usr/bin/env python3
"""Convert Claude Code JSONL transcript to readable Markdown."""

import json
import sys
from datetime import datetime

def format_tool_input(tool_name, tool_input):
    """Format tool input for display."""
    if tool_name == "Read":
        return f"({tool_input.get('file_path', '')})"
    elif tool_name == "Write":
        return f"({tool_input.get('file_path', '')})"
    elif tool_name == "Edit":
        return f"({tool_input.get('file_path', '')})"
    elif tool_name == "Bash":
        cmd = tool_input.get('command', '')
        if len(cmd) > 60:
            cmd = cmd[:60] + "..."
        return f"({cmd})"
    elif tool_name == "WebFetch":
        return f"({tool_input.get('url', '')})"
    elif tool_name == "WebSearch":
        return f"({tool_input.get('query', '')})"
    elif tool_name == "Grep":
        return f"({tool_input.get('pattern', '')})"
    elif tool_name == "Glob":
        return f"({tool_input.get('pattern', '')})"
    elif tool_name == "Task":
        return f"({tool_input.get('description', '')})"
    else:
        return ""

def format_tool_result(content, max_lines=20):
    """Format tool result, truncating if needed."""
    if isinstance(content, list):
        # Tool result array
        for item in content:
            if item.get('type') == 'tool_result':
                return format_tool_result(item.get('content', ''), max_lines)
        return ""

    if not isinstance(content, str):
        return str(content)[:500]

    lines = content.split('\n')
    if len(lines) > max_lines:
        return '\n'.join(lines[:max_lines]) + f"\n... [{len(lines) - max_lines} more lines]"
    return content

def convert_jsonl_to_markdown(jsonl_path, output_path=None):
    """Convert JSONL transcript to Markdown."""

    messages = []
    with open(jsonl_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                msg = json.loads(line)
                messages.append(msg)
            except json.JSONDecodeError:
                continue

    # Extract session start time
    session_id = None
    start_time = None
    for msg in messages:
        if msg.get('sessionId'):
            session_id = msg['sessionId']
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
    pending_tool_uses = {}  # Track tool uses waiting for results

    for msg in messages:
        msg_type = msg.get('type')
        content = msg.get('message', {}).get('content')

        if not content:
            continue

        # User message (plain text)
        if msg_type == 'user' and isinstance(content, str):
            output += f"> {content}\n\n"

        # User message (tool results)
        elif msg_type == 'user' and isinstance(content, list):
            for item in content:
                if item.get('type') == 'tool_result':
                    tool_id = item.get('tool_use_id')
                    result = item.get('content', '')

                    # Handle list results first
                    if isinstance(result, list):
                        result = json.dumps(result, indent=2)
                    elif not isinstance(result, str):
                        result = str(result)

                    # Clean up result (remove system reminders)
                    if '<system-reminder>' in result:
                        result = result[:result.find('<system-reminder>')].strip()

                    if result:

                        # Truncate long results
                        lines = result.split('\n')
                        if len(lines) > 30:
                            result = '\n'.join(lines[:30]) + f"\n... [{len(lines) - 30} more lines]"

                        output += f"&nbsp;&nbsp;&nbsp;⎿ Result:\n\n```\n{result}\n```\n\n"

        # Assistant message
        elif msg_type == 'assistant' and isinstance(content, list):
            for item in content:
                item_type = item.get('type')

                if item_type == 'text':
                    text = item.get('text', '')
                    if text:
                        output += f"{text}\n\n"

                elif item_type == 'tool_use':
                    tool_name = item.get('name', 'Unknown')
                    tool_input = item.get('input', {})
                    tool_id = item.get('id')

                    formatted_input = format_tool_input(tool_name, tool_input)
                    # Two spaces before newline for markdown line break
                    output += f"⏺ {tool_name} {formatted_input}  \n"

                    # Store for matching with result
                    if tool_id:
                        pending_tool_uses[tool_id] = tool_name

    if output_path:
        with open(output_path, 'w') as f:
            f.write(output)
        print(f"Written to {output_path}")
    else:
        print(output)

    return output

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: jsonl-to-markdown.py <input.jsonl> [output.md]")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else None

    convert_jsonl_to_markdown(input_path, output_path)
