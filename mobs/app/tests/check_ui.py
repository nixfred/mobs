"""Exercise the real QML with isolated state and generated media, without devices or hotkeys."""
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
case_filter = sys.argv[2] if len(sys.argv) > 2 else ""
with tempfile.TemporaryDirectory(prefix="mobs-ui-") as tmp:
    root = Path(tmp)
    env = os.environ | {
        "QT_QPA_PLATFORM": "offscreen", "QT_QPA_PLATFORMTHEME": "generic",
        "QT_QUICK_BACKEND": "software", "QT_IM_MODULE": "none",
        "MOBS_NO_HOTKEY": "1", "MOBS_FAKE_CAMERAS": "1", "MOBS_FAKE_AUDIO": "1",
        "MOBS_TEST_SOURCE": "colour", "MOBS_TEST_CAMERA": "colour",
        "MOBS_TOKEN_FILE": str(root / "token"), "MOBS_THEME_DIR": str(root / "theme"),
    }
    for key in ("MOBS_FAKE_BACKEND", "MOBS_FAKE_STOP_HANG", "MOBS_TEST_AUDIO", "MOBS_TEST_DESKTOP_AUDIO", "MOBS_SCRIPT", "MOBS_SETTINGS", "MOBS_GRAB", "MOBS_SPLASH", "MOBS_SPLASH_AT", "MOBS_PICK", "MOBS_SCREEN", "MOBS_WINDOW_SIZE"):
        env.pop(key, None)
    case_count = 0

    def check(name, steps, size="860x560", fake_backend=False, folder_override=None, extra_env=None, ini_extra=""):
        global case_count
        if case_filter and case_filter not in name:
            return
        case_count += 1
        case_env = env | {"MOBS_SETTINGS": str(root / f"{case_count}.ini"),
                          "MOBS_SCRIPT": steps + ",quit", "MOBS_WINDOW_SIZE": size}
        if fake_backend:
            case_env["MOBS_FAKE_BACKEND"] = "1"
            Path(case_env["MOBS_SETTINGS"]).write_text(f"[General]\nfolder={folder_override or root / 'takes'}\nreviewAfter=false\n{ini_extra}")
        case_env.update(extra_env or {})
        result = subprocess.run([binary], env=case_env, capture_output=True, text=True, timeout=15)
        output = result.stdout + result.stderr
        if result.returncode or "TypeError:" in output or "ReferenceError:" in output or "Unable to assign" in output:
            raise AssertionError(f"{name} ({size}): exit {result.returncode}\n{output}")
        if ("script: forcequit" if steps.endswith(",forcequit") else "script: quit") not in result.stdout:
            raise AssertionError(f"{name}: script did not complete\n{output}")
        print(f"PASS {name} ({size})", flush=True)

    for size in ("860x560", "1024x768", "1180x720"):
        check("choose", "wait=0.1,checklayout", size)
        check("ready mic off", "pick=1,mic=0,wait=0.1,checklayout", size)
        for tab in range(6):
            check(f"expert page {tab}", f"pro=1,tab={tab},wait=0.1,checklayout", size)
    for size in ("860x560", "1024x768", "1180x720"):
        check("audio gain page", "openaudio,audiodetails=1,micgain=-60,wait=0.1,checklayout", size)
        check("missing input explains blocked recording", "pick=1,micdevice=missing,start,expect=state:idle,expect=message:unavailable,wait=0.1,checklayout", size)
        check("audio input check error fits", "openaudio,micdevice=missing,miccheck=1,expect=audioProblem:unavailable,wait=0.1,checklayout", size)
        check("NVIDIA quality page", "pro=1,tab=1,hardware=1,wait=0.1,checklayout", size)
    for size in ("860x560", "1024x768", "1180x720"):
        check("ready with named mic and live meter", "pick=1,micdevice=test:usb,miccheck=1,desktop=1,wait=0.1,checklayout", size, True)
        check("ready preview and live meter", "pick=1,preview=1,miccheck=1,wait=0.1,expect=previewFrame:1,checklayout", size, True)
        check("recording both audio meters", "pick=1,desktop=1,start,expect=state:recording,wait=0.1,checklayout,pause,expect=state:paused,wait=0.1,checklayout,pro=1,wait=0.1,checklayout", size, True)
    # A take shows what it is recording (fake backend: a flat colour frame). Wide, small, and a tall narrow window like a tiled one.
    for size in ("860x560", "1024x768", "1180x720", "900x1600", "1206x2000"):
        check("recording shows the live picture and fits",
              "pick=1,start,expect=state:recording,wait=0.4,expect=previewing:1,expect=previewFrame:1,checklayout,pause,expect=state:paused,wait=0.3,expect=previewing:1,checklayout", size, True)
        check("recording with both audio meters and the picture fits",
              "pick=1,desktop=1,start,expect=state:recording,wait=0.4,expect=previewFrame:1,checklayout,pro=1,wait=0.1,checklayout", size, True)
    check("the picture goes away after the take when it was not asked for",
          "pick=1,start,expect=state:recording,wait=0.3,expect=previewing:1,stop,waitidle,expect=previewing:0,expect=previewFrame:0,expect=preview:0", fake_backend=True)
    check("a preview that was asked for keeps running after the take",
          "pick=1,preview=1,wait=0.2,start,expect=state:recording,wait=0.3,expect=previewing:1,stop,waitidle,expect=previewing:1,expect=previewFrame:1", fake_backend=True)
    check("audio shortcut opens inputs", "pick=1,openaudio,expect=pro:1,expect=audioTab:2")
    check("invalid folder preserves saved destination", "pro=1,foldertext=relative/path,applyfolder,expect=folderDefault:1,wait=0.1,checklayout")
    check("preview error still fits", "pick=1,preview=1,wait=0.1,checklayout")
    check("failed preview turns off and can retry", "pick=1,preview=1,previewfailure,expect=preview:0,expect=message:preview capture failed,wait=0.1,checklayout,preview=1,expect=preview:1,expect=previewFrame:1", fake_backend=True)
    check("stalled save offers a cancellable explicit quit",
          "pick=1,start,closewindow,expect=state:stopping,expect=confirmStopQuit:0,closewindow,expect=confirmStopQuit:1,wait=0.1,checklayout,key=Space,expect=state:stopping,key=Esc,expect=confirmStopQuit:0,closewindow,expect=confirmStopQuit:1,forcequit",
          fake_backend=True, extra_env={"MOBS_FAKE_STOP_HANG": "1"})
    # The opening screen (MOBS_SPLASH=1 brings it back for scripted runs; they skip it otherwise so they land on the real screens).
    splash_on = {"MOBS_SPLASH": "1"}
    for size in ("860x560", "1024x768", "1180x720"):
        check("opening screen fits while it plays", "wait=0.9,checklayout", size, extra_env=splash_on)
    check("opening screen eats the first key, then the app answers",
          "pick=1,key=Space,expect=state:idle,wait=0.4,expect=splashDone:1,key=Space,expect=state:recording", fake_backend=True, extra_env=splash_on)
    check("a recording that starts (hotkey) clears the opening screen",
          "pick=1,start,expect=state:recording,wait=0.4,expect=splashDone:1,wait=0.1,checklayout", fake_backend=True, extra_env=splash_on)
    check("opening animation switched off in EXPERT never plays",
          "expect=splashDone:1,pick=1,key=Space,expect=state:recording", fake_backend=True, extra_env=splash_on, ini_extra="splash=false\n")
    check("expert blocks recording keys", "pick=1,key=P,expect=pro:1,key=Space,expect=state:idle,key=Esc,expect=pro:0")
    check("folder focus returns to keyboard", "pick=1,pro=1,focusfolder,key=Esc,expect=pro:0,key=P,expect=pro:1")
    check("camera panel blocks recording keys", "pick=2,panel=1,key=Space,expect=state:idle,key=Esc,wait=0.1,checklayout")
    check("camera panel small layout", "pick=2,panel=1,wait=0.1,checklayout")
    for size in ("860x560", "1024x768", "1180x720"):
        check("camera panel with six devices keeps selection accessible",
              "pick=2,panel=1,wait=0.1,checklayout," + "click=cameraPickerNext," * 5 + "click=cameraPickerDevice,expect=cameraName:Demo camera 6,wait=0.1,checklayout",
              size, True, extra_env={"MOBS_FAKE_CAMERAS": "many"})
    check("failed mic check survives unrelated settings",
          "pick=1,miccheck=1,micfailure,expect=audioProblem:capture failed,quality=2,expect=audioProblem:capture failed,desktop=1,expect=audioProblem:capture failed,micgain=-3,expect=audioProblem:capture failed,mic=0,expect=audioProblemEmpty:1", fake_backend=True)
    blocked_folder = root / "former-folder"
    blocked_folder.write_text("another file")
    check("new save failure remains visible beside a mic error",
          "pick=1,miccheck=1,micfailure,start,expect=message:cannot save,expect=notice:cannot save,expect=notice:microphone capture failed,wait=0.1,checklayout", fake_backend=True, folder_override=blocked_folder)

    take = root / "take.mp4"
    junk = root / "junk.mp4"
    junk.write_text("not a recording")
    check("failed review explains the error on Ready", f"pick=1,review={junk},expect=screen:2,expect=notice:could not open,wait=0.1,checklayout", fake_backend=True)
    subprocess.run(["ffmpeg", "-nostdin", "-v", "error", "-f", "lavfi", "-i", "testsrc2=s=320x180:r=15:d=3",
                    "-c:v", "libx264", "-pix_fmt", "yuv420p", str(take)], check=True, timeout=15)
    tools = root / "tools"
    tools.mkdir()
    probe = tools / "ffprobe"
    probe.write_text(f"#!/bin/sh\n/bin/sleep 0.6\nexec '{shutil.which('ffprobe')}' \"$@\"\n")
    probe.chmod(0o700)
    check("review loading stays responsive and can be cancelled",
          f"pick=1,beginreview={take},expect=loading:1,wait=0.1,checklayout,start,expect=state:idle,expect=message:wait for the recording,closereview,wait=0.8,expect=loading:0,expect=screen:2,expect=messageEmpty:1,start,expect=state:recording",
          fake_backend=True, extra_env={"PATH": str(tools) + os.pathsep + env.get("PATH", "")})
    check("unsaved edits survive hotkey path and window close",
          f"pick=1,review={take},split=1,cut=0,start,expect=state:idle,expect=screen:4,closewindow,expect=confirmQuit:1,wait=0.1,checklayout,key=Esc,expect=confirmQuit:0")
    check("review confirmations and layout", f"pick=1,review={take},wait=0.1,checklayout")
    for size in ("860x560", "1024x768", "1180x720"):
        check("review leave and trash confirmations fit", f"pick=1,review={take},split=1,cut=0,click=reviewDone,click=reviewDelete,wait=0.1,checklayout", size, True)
    black = root / "black.mp4"
    subprocess.run(["ffmpeg", "-nostdin", "-v", "error", "-f", "lavfi", "-i", "color=black:s=320x180:r=15:d=3",
                    "-c:v", "libx264", "-pix_fmt", "yuv420p", str(black)], check=True, timeout=15)
    export_tools = root / "export-tools"
    export_tools.mkdir()
    export_program = export_tools / "ffmpeg"
    export_program.write_text(f"#!/bin/sh\ncase \"$*\" in\n*scale=32:18,format=gray*) exec '{shutil.which('ffmpeg')}' \"$@\";;\n*) printf 'export test failure\\n' >&2; exit 1;;\nesac\n")
    export_program.chmod(0o700)
    check("review save failure remains visible beside a picture warning",
          f"pick=1,savedtake={black},waitwarning,review={black},split=1,cut=0,savereview,waitsaved,expect=reviewNotice:could not save,expect=reviewNotice:black,wait=0.1,checklayout",
          fake_backend=True, extra_env={"PATH": str(export_tools) + os.pathsep + env.get("PATH", "")})
    check("trim cancellation completes one undoable gesture",
          f"pick=1,review={take},wait=0.1,timelinepress=0,timelinemove=0.2,expect=trimMode:1,wait=0.1,checklayout,timelinecancel,expect=trimMode:0,expect=canUndo:1,expect=unsaved:1,checklayout")
    check("opening EXPERT finishes an active trim",
          f"pick=1,review={take},wait=0.1,timelinepress=0,timelinemove=0.2,expect=trimMode:1,pro=1,expect=trimMode:0,expect=canUndo:1,wait=0.1,checklayout")
    check("restoring a gap uses the timeline host callback",
          f"pick=1,review={take},split=1,cut=0,expect=unsaved:1,restoregap=0,expect=unsaved:0,wait=0.1,checklayout")
    check("split without cuts leaves content saved",
          f"pick=1,review={take},split=1,expect=unsaved:0,closereview,expect=screen:2")
    check("saving clears a blocked recording prompt",
          f"pick=1,review={take},split=1,cut=0,start,expect=message:save your edit,savereview,waitsaved,expect=unsaved:0,expect=messageEmpty:1,wait=0.1,checklayout")
    print(f"{case_count} UI scenarios passed")
