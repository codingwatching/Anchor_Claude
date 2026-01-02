#!/usr/bin/env python3
"""Convert Claude Code JSONL transcript to readable Markdown."""

import json
import sys
import re
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
    elif tool_name == "Task":
        return f"({tool_input.get('description', '')})"
    else:
        return ""

def format_tool_result(result, max_lines=30):
    """Format tool result, truncating if needed."""
    if isinstance(result, list):
        result = json.dumps(result, indent=2)
    elif not isinstance(result, str):
        result = str(result)

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

    for msg in messages:
        msg_type = msg.get('type')
        content = msg.get('message', {}).get('content')

        if not content:
            continue

        # User message (plain text)
        if msg_type == 'user' and isinstance(content, str):
            # Check if it's internal system output
            if is_system_message(content):
                output += f"````\n{content}\n````\n\n"
            else:
                output += f"> {content}\n\n"

        # User message (tool results)
        elif msg_type == 'user' and isinstance(content, list):
            for item in content:
                if item.get('type') == 'tool_result':
                    tool_id = item.get('tool_use_id')
                    result = item.get('content', '')

                    # Get the pending tool info
                    tool_info = pending_tools.pop(tool_id, None)

                    if tool_info:
                        tool_name, formatted_input = tool_info
                        result_text = format_tool_result(result)

                        # Command is clickable summary, result is hidden content
                        output += f"<details>\n<summary><code>{tool_name} {formatted_input}</code></summary>\n\n"
                        if result_text:
                            output += f"````\n{result_text}\n````\n\n"
                        output += "</details>\n\n"

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

                    # Store for matching with result
                    if tool_id:
                        pending_tools[tool_id] = (tool_name, formatted_input)

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
