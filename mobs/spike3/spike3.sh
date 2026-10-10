#!/bin/bash
# Spike 3: start OBS with a throwaway config, hide it on a Hyprland special workspace, drive it over
# obs-websocket with obsctl, then check the recording and that OBS stayed hidden and alive.
# Run inside a Hyprland session (Test Drive VM). Never touches ~/.config/obs-studio.
set -u
H=$HOME/spike3; CFG=$H/cfg; REC=$H/rec; rm -rf "$H"; mkdir -p "$REC" "$CFG/obs-studio/basic/scenes" "$CFG/obs-studio/basic/profiles/Untitled" "$CFG/obs-studio/plugin_config/obs-websocket"
PASS=$(openssl rand -hex 12)
printf '[General]\nLastVersion=537001986\nMaxLogs=10\n' > "$CFG/obs-studio/global.ini"
printf '[General]\nWarnedAboutClosingDocks=true\n' > "$CFG/obs-studio/user.ini"
printf '[General]\nName=Untitled\n[SimpleOutput]\nRecQuality=Small\nRecFormat2=hybrid_mp4\n' > "$CFG/obs-studio/basic/profiles/Untitled/basic.ini"
printf '{"current_program_scene":"Scene","current_scene":"Scene","name":"Untitled","scene_order":[{"name":"Scene"}],"sources":[{"id":"scene","versioned_id":"scene","name":"Scene","uuid":"22222222-2222-4222-8222-222222222222","enabled":true,"settings":{"id_counter":0,"items":[]}}]}' > "$CFG/obs-studio/basic/scenes/Untitled.json"
printf '{"alerts_enabled":false,"auth_required":true,"first_load":false,"server_enabled":true,"server_password":"%s","server_port":4455}' "$PASS" > "$CFG/obs-studio/plugin_config/obs-websocket/config.json"

XDG_CONFIG_HOME=$CFG obs --disable-shutdown-check --disable-missing-files-check >"$H/obs.out" 2>&1 </dev/null &
OBS=$!
echo "OBS pid $OBS ($(ps -p $OBS -o comm=))"
for i in $(seq 1 40); do (exec 3<>/dev/tcp/127.0.0.1/4455) 2>/dev/null && break; sleep 1; done
(exec 3<>/dev/tcp/127.0.0.1/4455) 2>/dev/null && echo "websocket port 4455 open after ~${i}s" || { echo "FAIL websocket never opened"; kill $OBS; exit 2; }

addr_ws() { hyprctl clients -j | python3 -c "
import json,sys
for c in json.load(sys.stdin):
    if c['pid']==$OBS and c['title'].startswith('OBS'): print(c['address'], c['workspace']['name'])"; }
read ADDR WSBEFORE < <(addr_ws); echo "OBS window $ADDR on workspace '$WSBEFORE' (visible)"
hyprctl dispatch "hl.dsp.window.move({ workspace = \"special:mobs\", follow = false, window = \"address:$ADDR\" })"; echo "dispatch rc=$?"
sleep 1; read ADDR WSAFTER < <(addr_ws); echo "OBS window now on workspace '$WSAFTER'"
[ "$WSAFTER" = "special:mobs" ] && echo "PASS  OBS is hidden on special:mobs" || echo "FAIL  OBS not on special:mobs"

MOBS_OBS_PASSWORD=$PASS MOBS_REC_DIR=$REC "$H/../mobs-spike3/build/obsctl"; RC=$?
echo "obsctl rc=$RC"
read ADDR WSEND < <(addr_ws); echo "after recording OBS is on '$WSEND'; alive: $(kill -0 $OBS 2>/dev/null && echo yes || echo NO)"
F=$(ls -t "$REC"/* 2>/dev/null | head -1)
[ -n "$F" ] && { echo "file: $F"; ffprobe -v error -show_entries format=format_name,duration -show_entries stream=codec_name,width,height -of default=nw=1 "$F"; }
kill $OBS; sleep 2; kill -0 $OBS 2>/dev/null && kill -9 $OBS
echo "done"
