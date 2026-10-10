#!/bin/bash
# Real-hardware portal check. Throwaway OBS config, own pid only, hidden on special:mobs.
# Usage: portal.sh <workdir>   (needs a Hyprland session, bun, ffmpeg, python3 + PIL)
set -u
W=${1:?workdir}; CFG=$W/cfg; REC=$W/rec; SHOT=$W/shot.png
rm -rf "$W"; mkdir -p "$REC" "$CFG/obs-studio/basic/scenes" "$CFG/obs-studio/basic/profiles/Untitled" "$CFG/obs-studio/plugin_config/obs-websocket"
pgrep -x obs >/dev/null && { echo "ABORT: an OBS is already running (not mine); not starting another"; exit 3; }
PASS=$(openssl rand -hex 12)
printf '[General]\nLastVersion=537001986\nMaxLogs=10\n' > "$CFG/obs-studio/global.ini"
printf '[General]\nWarnedAboutClosingDocks=true\n' > "$CFG/obs-studio/user.ini"
printf '[General]\nName=Untitled\n[SimpleOutput]\nRecQuality=Small\nRecFormat2=hybrid_mp4\n' > "$CFG/obs-studio/basic/profiles/Untitled/basic.ini"
printf '{"current_program_scene":"Scene","current_scene":"Scene","name":"Untitled","scene_order":[{"name":"Scene"}],"sources":[{"id":"scene","versioned_id":"scene","name":"Scene","uuid":"22222222-2222-4222-8222-222222222222","enabled":true,"settings":{"id_counter":0,"items":[]}}]}' > "$CFG/obs-studio/basic/scenes/Untitled.json"
if [ -n "${PRESEED:-}" ]; then
  # Source created at LOAD (OBS main thread) instead of via websocket CreateInput (websocket thread).
  printf '{"current_program_scene":"MOBS","current_scene":"MOBS","name":"Untitled","scene_order":[{"name":"MOBS"}],"sources":[{"id":"pipewire-screen-capture-source","versioned_id":"pipewire-screen-capture-source","name":"my screen","uuid":"33333333-3333-4333-8333-333333333333","enabled":true,"muted":false,"volume":1.0,"flags":0,"mixers":0,"settings":{"ShowCursor":true}},{"id":"scene","versioned_id":"scene","name":"MOBS","uuid":"22222222-2222-4222-8222-222222222222","enabled":true,"settings":{"id_counter":1,"items":[{"name":"my screen","source_uuid":"33333333-3333-4333-8333-333333333333","visible":true,"locked":false,"id":1,"pos":{"x":0.0,"y":0.0},"scale":{"x":1.0,"y":1.0},"rot":0.0,"align":5,"bounds_type":0,"bounds":{"x":0.0,"y":0.0},"bounds_align":0,"crop_left":0,"crop_top":0,"crop_right":0,"crop_bottom":0}]}}]}' > "$CFG/obs-studio/basic/scenes/Untitled.json"
fi
printf '{"alerts_enabled":false,"auth_required":true,"first_load":false,"server_enabled":true,"server_password":"%s","server_port":4455}' "$PASS" > "$CFG/obs-studio/plugin_config/obs-websocket/config.json"

XDG_CONFIG_HOME=$CFG obs --disable-shutdown-check --disable-missing-files-check >"$W/obs.out" 2>&1 </dev/null &
OBS=$!; echo "OBS pid $OBS ($(ps -p $OBS -o comm=))"
find_win() { hyprctl clients -j | python3 -c "
import json,sys
for c in json.load(sys.stdin):
    if c['pid']==$OBS and c['title'].startswith('OBS'): print(c['address'], c['workspace']['name'])"; }
ADDR=""; for i in $(seq 1 40); do read ADDR WS < <(find_win); [ -n "$ADDR" ] && break; sleep 0.5; done
[ -z "$ADDR" ] && { echo "FAIL no OBS window"; kill $OBS; exit 2; }
[ -z "${NOHIDE:-}" ] && hyprctl dispatch "hl.dsp.window.move({ workspace = \"special:mobs\", follow = false, window = \"address:$ADDR\" })" >/dev/null; sleep 1
read ADDR WS < <(find_win); echo "OBS window on workspace '$WS'"; if [ -n "${NOHIDE:-}" ]; then echo "NOHIDE: OBS left visible on workspace $WS"; else [ "$WS" = "special:mobs" ] && echo "PASS  OBS hidden" || echo "FAIL  OBS not hidden"; fi
for i in $(seq 1 30); do (exec 3<>/dev/tcp/127.0.0.1/4455) 2>/dev/null && break; sleep 1; done

MOBS_OBS_PASSWORD=$PASS MOBS_REC_DIR=$REC MOBS_SHOT=$SHOT bun "$(dirname "$(readlink -f "$0")")/portal-check.ts" >"$W/client.log" 2>&1 &
CL=$!
for t in $(seq 1 110); do
  sleep 1
  if [ -z "${NOTIFIED:-}" ] && grep -q PICKER "$W/client.log" 2>/dev/null; then NOTIFIED=1; notify-send -u critical "MOBS portal test" "A screen-share dialog should be open now. Pick your screen." 2>/dev/null; echo "-- notified at t=${t}s"; fi
  if hyprctl layers -j | grep -q share-picker; then [ -z "${PK_FIRST:-}" ] && PK_FIRST=$t; PK_LAST=$t; fi
  if [ $((t % 6)) = 1 ] && [ -z "${QUIET:-}" ]; then echo "-- t=${t}s layers:" $(hyprctl layers -j | python3 -c "
import json,sys
ns=set()
for mon in json.load(sys.stdin).values():
    for lv in mon['levels'].values():
        for l in lv: ns.add(l['namespace'])
print(' '.join(sorted(ns)))"); echo "-- t=${t}s windows (not OBS):"; hyprctl clients -j | python3 -c "
import json,sys
for c in json.load(sys.stdin):
    if c['pid']!=$OBS: print('   ',c['class'],repr(c['title'])[:48],'ws',c['workspace']['name'])" ; fi
  kill -0 $CL 2>/dev/null || break
done
kill -0 $CL 2>/dev/null && { echo "client still running after 150 s, stopping it"; kill $CL; }
echo "== picker overlay was visible from t=${PK_FIRST:-never}s to t=${PK_LAST:-never}s"; echo "== client log"; cat "$W/client.log"
read ADDR WS < <(find_win); echo "== end: OBS on '$WS', alive: $(kill -0 $OBS 2>/dev/null && echo yes || echo NO)"
F=$(ls -t "$REC"/* 2>/dev/null | head -1)
if [ -n "$F" ]; then
  echo "== recording: $(ffprobe -v error -show_entries format=duration -show_entries stream=codec_name,width,height -of csv=p=0 "$F" | tr '\n' ' ')"
  ffmpeg -nostdin -v error -y -ss 2 -i "$F" -frames:v 1 "$W/frame.png" && python3 - "$W/frame.png" "$SHOT" <<'PY'
import sys
from PIL import Image
for p in sys.argv[1:]:
    try:
        im=Image.open(p).convert("RGB"); c=im.resize((160,90)).getcolors(100000)
        px=list(im.resize((64,36)).getdata()); mean=sum(sum(x) for x in px)/(3*len(px))
        print("   %s: %dx%d, mean brightness %.0f/255, distinct colours (at 160x90) %d" % (p.split('/')[-1], im.size[0], im.size[1], mean, len(c)))
    except Exception as e: print("   ", p.split('/')[-1], "unreadable:", e)
PY
fi
kill $OBS 2>/dev/null; for i in $(seq 1 20); do kill -0 $OBS 2>/dev/null || break; sleep 0.5; done; kill -0 $OBS 2>/dev/null && kill -9 $OBS
rm -rf "$REC" "$SHOT" "$W/frame.png"; echo "done (captured frames deleted)"
