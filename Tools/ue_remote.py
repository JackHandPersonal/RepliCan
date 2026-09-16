"""
Send Python commands to a running Unreal Editor instance for RepliCan.

Filters discovered remote-execution nodes by project_name=="RepliCan" -- this
machine may have multiple editor instances open at once (e.g. ClaudeTest too),
and the plain first-node-found approach silently targets whichever editor
happened to respond first, which is exactly the kind of cross-project mixup
that's easy to not notice until something gets edited in the wrong project.

Requires: Edit > Project Settings > Plugins > Python > "Enable Remote Execution?" checked,
and the editor open with RepliCan loaded.

Usage:
    python ue_remote.py "<python code>"
    python ue_remote.py --file path\to\script.py
"""
import sys
import os
import time
import json
import argparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import remote_execution as ue_re

PROJECT_NAME = "RepliCan"


def run(code, mode=ue_re.MODE_EXEC_FILE, timeout=30):
    conn = ue_re.RemoteExecution()
    conn.start()
    try:
        deadline = time.time() + timeout
        node = None
        while time.time() < deadline:
            for candidate in conn.remote_nodes:
                if candidate.get("project_name") == PROJECT_NAME:
                    node = candidate
                    break
            if node:
                break
            time.sleep(0.2)
        if not node:
            found = [n.get("project_name") for n in conn.remote_nodes]
            raise RuntimeError(
                f"No Unreal Editor remote-execution node found for project '{PROJECT_NAME}'. "
                f"Is that editor open with Python Remote Execution enabled? "
                f"(nodes seen: {found})"
            )
        conn.open_command_connection(node.get("node_id"))
        return conn.run_command(code, exec_mode=mode)
    finally:
        conn.stop()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("code", nargs="?", help="Python code to execute")
    parser.add_argument("--file", help="Path to a Python script to execute instead of inline code")
    args = parser.parse_args()

    if args.file:
        with open(args.file, "r") as f:
            script = f.read()
    elif args.code:
        script = args.code
    else:
        script = "print('hello from remote exec')"

    result = run(script, mode=ue_re.MODE_EXEC_FILE)
    print(json.dumps(result, indent=2))
    if not result.get("success"):
        sys.exit(1)
