#!/usr/bin/env bash
# Close -> build -> relaunch, as one move, with the PIE guard built in.
#
# WHY THIS EXISTS. Twice on 2026-09-19 a session killed the editor while the user was in PIE. Both
# times it checked first. Both times the check printed "starts: N, teardowns: N-1" and the imbalance
# -- which IS the answer -- was read past. A check whose output a human has to interpret correctly is
# a reminder, not a check. So this script ASSERTS and aborts; the kill is unreachable while PIE is up.
#
#   Tools/build_editor.sh              # guarded: aborts if PIE is live
#   Tools/build_editor.sh --game       # build the game target too (packaging sanity)
#   Tools/build_editor.sh --no-relaunch
#   Tools/build_editor.sh --force      # ONLY with the PIE owner's explicit go-ahead
#
# --force exists because "the owner said go" is a real state this script cannot see. It is not for
# "I think they're probably done".
set -u
PROJ="C:/Dev/Games/RepliCan/RepliCan.uproject"
ROOT="/c/Dev/Games/RepliCan"
UE="/c/Program Files/Epic Games/UE_5.8/Engine"
PY="/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe"
DLL="$ROOT/Binaries/Win64/UnrealEditor-RepliCan.dll"

FORCE=0; RELAUNCH=1; GAME=0
for a in "$@"; do
  case "$a" in
    --force) FORCE=1 ;;
    --no-relaunch) RELAUNCH=0 ;;
    --game) GAME=1 ;;
  esac
done

# ---- 1. THE GUARD. Nothing below runs while PIE is live. -----------------------------------------
if [ "$FORCE" -eq 0 ]; then
  if ! "$PY" "$ROOT/Tools/pie_state.py"; then
    echo
    echo "ABORTED: not building. Ask the session whose user is playing, then pass --force."
    exit 2
  fi
else
  echo "--force: PIE guard skipped (this must be someone's explicit go-ahead, not a guess)."
fi

# ---- 2. Close ------------------------------------------------------------------------------------
PID=$(tasklist //FI "IMAGENAME eq UnrealEditor.exe" //FO CSV //NH 2>/dev/null | head -1 | cut -d, -f2 | tr -d '"')
if [ -n "$PID" ] && [ "$PID" != "INFO:" ]; then
  echo "closing editor $PID"; taskkill //PID "$PID" //F >/dev/null 2>&1
else
  echo "no editor running"
fi

# ---- 3. Build ------------------------------------------------------------------------------------
build() {
  echo "building $1 ..."
  "$UE/Build/BatchFiles/Build.bat" "$1" Win64 Development -Project="$PROJ" -WaitMutex 2>&1 \
    | grep -E "error C[0-9]|error LNK|error MSB|fatal error|Result:|Total execution"
}
build RepliCanEditor || true
LOG="/c/Users/jhand/AppData/Local/UnrealBuildTool/Log.txt"
ERRS=$(grep -cE "error C[0-9]|error LNK|error MSB|fatal error" "$LOG" 2>/dev/null)
if [ "${ERRS:-1}" -ne 0 ]; then
  echo "BUILD FAILED ($ERRS errors) -- editor NOT relaunched, fix first."
  grep -E "error C[0-9]|error LNK|fatal error" "$LOG" | head -10
  exit 1
fi
[ "$GAME" -eq 1 ] && build RepliCan

# ---- 4. Prove the DLL is newer than every source, rather than trusting "up to date" ---------------
NEWEST=$(find "$ROOT/Source/RepliCan" \( -name '*.cpp' -o -name '*.h' \) -newer "$DLL" 2>/dev/null | head -1)
if [ -n "$NEWEST" ]; then
  echo "WARNING: source newer than the DLL: ${NEWEST#$ROOT/}"
  echo "         the running editor may not contain it."
else
  echo "DLL is newer than every source file."
fi

# ---- 5. Relaunch, aborting rather than doubling up -----------------------------------------------
if [ "$RELAUNCH" -eq 1 ]; then
  N=$(tasklist //FI "IMAGENAME eq UnrealEditor.exe" //FO CSV //NH 2>/dev/null | grep -c UnrealEditor)
  if [ "$N" -gt 0 ]; then
    echo "ABORT relaunch: an editor is already running. Two editors on one project fight over the"
    echo "DDC, the asset locks and Saved/. Check before launching another."
    exit 3
  fi
  # Launch via a generated .ps1 rather than an inline -Command string. The inline form was quoted
  # through bash AND PowerShell, silently launched nothing, and still printed a success line -- the
  # build looked fine and the editor was simply gone. Verify by PID, never by exit status.
  PS1="$ROOT/Saved/relaunch_editor.ps1"
  mkdir -p "$ROOT/Saved"
  cat > "$PS1" <<'PSEOF'
$n = @(Get-Process UnrealEditor -ErrorAction SilentlyContinue).Count
if ($n -gt 0) { Write-Output "ABORT: $n editor(s) already running"; exit 3 }
$exe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$p = Start-Process -FilePath $exe -ArgumentList '"C:\Dev\Games\RepliCan\RepliCan.uproject"' -PassThru
Write-Output "relaunched PID=$($p.Id)"
PSEOF
  powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$PS1")"
  sleep 3
  N2=$(tasklist //FI "IMAGENAME eq UnrealEditor.exe" //FO CSV //NH 2>/dev/null | grep -c UnrealEditor)
  if [ "$N2" -lt 1 ]; then
    echo "RELAUNCH FAILED -- no editor is running. Start it by hand before telling anyone it is up."
    exit 4
  fi
  echo "verified: $N2 editor(s) running."
fi
