"""Is PIE running right now? Prints one verdict line, not numbers to interpret.

RUN THIS BEFORE KILLING THE EDITOR. Exit code 0 = safe to build, 1 = PIE is LIVE, do not build.

Written 2026-09-19 after the coordinator killed the user's live PIE session mid-test. The check that
failed was not missing -- it printed "starts: 2, teardowns: 1" and the session read the presence of
numbers rather than the numbers. An imbalance IS the answer. So this tool does the arithmetic and
states a verdict; there is nothing left to skim past.

    python Tools/pie_state.py          # verdict + exit code
    python Tools/pie_state.py --quiet  # exit code only

THIS GUARDS MORE THAN BUILDS. It was written for Tools/build_editor.sh, and then on 2026-09-19 a
session ended the user's live game a different way entirely: calling
LevelEditorSubsystem.editor_play_simulate() over ue_remote, which tears down the running PIE and
starts its own. The build guard never saw it, because nobody was building. Anything that starts,
stops or restarts play -- editor_play_simulate, editor_play_in_viewport, editor_request_end_play --
has to ask this first.

A PIE run is a matched pair in the log: "Bringing up level for play" then "BeginTearingDown" for a
UEDPIE_ world. More starts than teardowns means one is still open.
"""
import os, re, sys

LOG = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "Saved", "Logs", "RepliCan.log")
START = re.compile(r"Bringing up level for play")
STOP = re.compile(r"BeginTearingDown for .*UEDPIE")
STAMP = re.compile(r"^\[([\d.\-]+:[\d.]+)\]")


def pie_state(path=LOG):
    """(is_live, starts, stops, last_event_stamp)."""
    if not os.path.exists(path):
        return False, 0, 0, None
    starts = stops = 0
    last = None
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if START.search(line):
                starts += 1
                m = STAMP.match(line)
                last = m.group(1) if m else last
            elif STOP.search(line):
                stops += 1
                m = STAMP.match(line)
                last = m.group(1) if m else last
    return starts > stops, starts, stops, last


if __name__ == "__main__":
    live, starts, stops, last = pie_state()
    if "--quiet" not in sys.argv:
        if live:
            print("PIE IS LIVE -- DO NOT BUILD. Someone is playing.")
            print("  %d start(s), %d teardown(s); last PIE event %s" % (starts, stops, last))
            print("  Ask the owning session, or wait for the teardown.")
        else:
            print("PIE is not running -- safe to build.")
            print("  %d start(s), %d teardown(s) matched." % (starts, stops))
    sys.exit(1 if live else 0)
