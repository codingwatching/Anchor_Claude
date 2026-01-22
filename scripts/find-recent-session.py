#!/usr/bin/env python3
"""Find the most recent session by last message timestamp."""

import os
import json
import glob
import argparse
from pathlib import Path

def get_last_timestamp(jsonl_path):
    """Get the timestamp of the last message in a jsonl file."""
    try:
        with open(jsonl_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            # Read from the end to find a line with a timestamp
            for line in reversed(lines):
                line = line.strip()
                if not line:
                    continue
                try:
                    msg = json.loads(line)
                    ts = msg.get('timestamp')
                    if ts:
                        return ts
                except json.JSONDecodeError:
                    continue
    except Exception:
        pass
    return None

def find_sessions(project_folders, exclude_session_id=None):
    """Find all sessions sorted by last message timestamp."""
    results = []

    for folder in project_folders:
        folder = os.path.expanduser(folder)
        if not os.path.exists(folder):
            continue

        for jsonl_path in glob.glob(os.path.join(folder, '*.jsonl')):
            # Skip agent files
            if 'agent' in os.path.basename(jsonl_path):
                continue

            # Extract session ID from filename
            session_id = os.path.splitext(os.path.basename(jsonl_path))[0]

            # Skip excluded session
            if exclude_session_id and session_id == exclude_session_id:
                continue

            ts = get_last_timestamp(jsonl_path)
            if ts:
                results.append((ts, jsonl_path, session_id))

    # Sort by timestamp descending (most recent first)
    results.sort(key=lambda x: x[0], reverse=True)
    return results

def main():
    parser = argparse.ArgumentParser(description='Find recent Claude Code sessions')
    parser.add_argument('--exclude', '-e', help='Session ID to exclude (current session)')
    parser.add_argument('--limit', '-n', type=int, default=5, help='Number of results to show')
    parser.add_argument('--folders', '-f', nargs='+',
                        default=['~/.claude/projects/E--a327ex-Anchor',
                                 '~/.claude/projects/E--a327ex-emoji-ball-battles'],
                        help='Project folders to search')
    args = parser.parse_args()

    sessions = find_sessions(args.folders, args.exclude)

    for i, (ts, path, session_id) in enumerate(sessions[:args.limit]):
        # Get first user message for context
        first_msg = ""
        try:
            with open(path, 'r', encoding='utf-8') as f:
                for line in f:
                    msg = json.loads(line)
                    if msg.get('type') == 'user':
                        content = msg.get('message', {}).get('content', '')
                        if isinstance(content, str) and content:
                            first_msg = content[:80].replace('\n', ' ')
                            if len(content) > 80:
                                first_msg += "..."
                            break
        except:
            pass

        marker = " <-- MOST RECENT" if i == 0 else ""
        print(f"{ts} {session_id}{marker}")
        print(f"   {first_msg}")
        print(f"   {path}")
        print()

if __name__ == "__main__":
    main()
