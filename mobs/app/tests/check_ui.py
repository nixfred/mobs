"""Exercise the real QML with isolated state and generated media, without devices or hotkeys."""
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile
import time

binary = str(Path(sys.argv[1]).resolve())
case_filter = sys.argv[2] if len(sys.argv) > 2 else ""
with tempfile.TemporaryDirectory(prefix="mobs-ui-") as tmp:
    root = Path(tmp)
    env = os.environ | {
        "QT_QPA_PLATFORM": "offscreen", "QT_QPA_PLATFORMTHEME": "generic",
        "QT_QUICK_BACKEND": "software", "QT_IM_MODULE": "none",
        "MOBS_NO_HOTKEY": "1", "MOBS_NO_OPEN": "1", "MOBS_FAKE_CAMERAS": "1", "MOBS_FAKE_AUDIO": "1",
        "MOBS_WEB_DIR": str(root / "web"), "MOBS_X_LOGIN_URL": (Path(__file__).resolve().parent / "fake-login.html").as_uri(),
        "MOBS_X_CHAT_URL": (Path(__file__).resolve().parent / "fake-studio.html").as_uri() + "?id=%1",
        "MOBS_TEST_SOURCE": "colour", "MOBS_TEST_CAMERA": "colour",
        "MOBS_TOKEN_FILE": str(root / "token"), "MOBS_THEME_DIR": str(root / "theme"),
    }
    for key in ("MOBS_FAKE_BACKEND", "MOBS_FAKE_STOP_HANG", "MOBS_TEST_AUDIO", "MOBS_TEST_DESKTOP_AUDIO", "MOBS_SCRIPT", "MOBS_SETTINGS", "MOBS_GRAB", "MOBS_SPLASH", "MOBS_SPLASH_AT", "MOBS_PICK", "MOBS_SCREEN", "MOBS_WINDOW_SIZE"):
        env.pop(key, None)
    case_count = 0

    def _check(name, steps, size="860x560", fake_backend=False, folder_override=None, extra_env=None, ini_extra="", default_preview=False, expect_log=()):
        global case_count
        if case_filter and case_filter not in name:
            return
        case_count += 1
        case_env = env | {"MOBS_SETTINGS": str(root / f"{case_count}.ini"), "MOBS_STREAM_KEY_FILE": str(root / f"{case_count}.key"), "MOBS_LOG_FILE": str(root / f"{case_count}.log"),
                          "MOBS_SCRIPT": steps + ",quit", "MOBS_WINDOW_SIZE": size}
        # The preview is ON by default now (it lights the camera and may open the screen dialog), so scenarios that mean "preview off" say so.
        base_ini = "" if default_preview else "preview=false\n"
        Path(case_env["MOBS_SETTINGS"]).write_text(f"[General]\n{base_ini}")
        if fake_backend:
            case_env["MOBS_FAKE_BACKEND"] = "1"
            Path(case_env["MOBS_SETTINGS"]).write_text(f"[General]\nfolder={folder_override or root / 'takes'}\nreviewAfter=false\n{base_ini}{ini_extra}")
        case_env.update(extra_env or {})
        result = subprocess.run([binary], env=case_env, capture_output=True, text=True, timeout=15)
        output = result.stdout + result.stderr
        if result.returncode or "TypeError:" in output or "ReferenceError:" in output or "Unable to assign" in output:
            raise AssertionError(f"{name} ({size}): exit {result.returncode}\n{output}")
        if ("script: forcequit" if steps.endswith(",forcequit") else "script: quit") not in result.stdout:
            raise AssertionError(f"{name}: script did not complete\n{output}")
        if expect_log:
            log_file = Path(case_env["MOBS_LOG_FILE"])
            log_text = log_file.read_text() if log_file.exists() else ""
            for line in expect_log:
                if line not in log_text:
                    raise AssertionError(f"{name}: the log never said {line!r}\n{log_text[-1500:]}")
        print(f"PASS {name} ({size})", flush=True)

    failures = []

    def check(*args, **kwargs):
        # MOBS_UI_KEEP_GOING=1 reports every failing scenario in one run instead of stopping at the first (for the day a screen was redesigned).
        try:
            _check(*args, **kwargs)
        except (AssertionError, subprocess.TimeoutExpired) as error:
            if os.environ.get("MOBS_UI_KEEP_GOING") != "1":
                raise
            failures.append(args[0])
            print(f"FAIL {args[0]}: {str(error)[:700]}", flush=True)

    for size in ("860x560", "1024x768", "1180x720"):
        check("choose", "wait=0.1,checklayout", size, extra_env={"MOBS_SCREEN": "1"})
        check("ready mic off", "pick=1,mic=0,wait=0.1,checklayout", size)
        for tab in range(7):
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
    # The pick screen has three cards: the 3 key picks the last one, and 4 (once "the show") picks nothing.
    check("the keys on the pick screen: 3 picks just my camera, 4 picks nothing", "wait=0.1,expect=screen:1,key=4,wait=0.2,expect=screen:1,key=3,expect=screen:2,wait=0.1,checklayout", fake_backend=True, extra_env={"MOBS_SCREEN": "1"})
    # "All I want is screen and me": a new user lands on ready with the whole screen and the webcam, and the webcam can be dragged.
    check("a new user lands on ready with screen + me", "wait=0.3,expect=screen:2,expect=previewing:1,expect=previewFrame:1,checklayout", fake_backend=True, default_preview=True)
    for size in ("860x560", "1024x768", "1180x720"):
        check("dragging the webcam in the preview moves it and the layout still fits (webcam window off)", "pick=2,preview=1,wait=0.4,expect=camMoved:0,dragcam=-40:30,expect=camMoved:1,wait=0.2,checklayout", size, True, ini_extra="pip=false\n")
    check("the webcam can be dragged while recording (webcam window off)", "pick=2,start,expect=state:recording,wait=0.5,dragcam=-60:40,expect=camMoved:1,expect=state:recording,wait=0.2,checklayout", fake_backend=True, ini_extra="pip=false\n")
    # The webcam as a window ON the screen (the default for screen + me): SUPER + drag moves it, so there is no handle inside the picture.
    for size in ("860x560", "1024x768", "1180x720"):
        check("screen + me shows the webcam as a window and the ready screen still fits", "pick=2,preview=1,wait=0.4,expect=pipWanted:1,expect=pipShown:1,checklayout", size, True)
    check("the webcam window follows the picture: on with the preview, off without it", "pick=2,preview=0,wait=0.2,expect=pipWanted:0,expect=pipShown:0,preview=1,wait=0.3,expect=pipShown:1,preview=0,wait=0.3,expect=pipShown:0", fake_backend=True)
    check("a take always shows the webcam window, and it goes with the take", "pick=2,preview=0,expect=pipShown:0,start,expect=state:recording,wait=0.4,expect=pipWanted:1,expect=pipShown:1,stop,waitidle,wait=0.3,expect=pipShown:0", fake_backend=True)
    # A stray SUPER + Q closed the webcam window in the middle of a take on 2026-10-12 and the stream carried on without the person in it, for good.
    check("a webcam window closed during a take is put back and says so", "pick=2,preview=0,start,expect=state:recording,wait=0.4,expect=pipShown:1,closepip,wait=0.2,expect=pipShown:0,wait=0.9,expect=pipShown:1,checkpip,expect=state:recording",
          fake_backend=True, expect_log=("webcam window closed during a take: putting it back", "webcam window put back: it was closed during a take"))
    check("a webcam window closed before a take stays closed", "pick=2,preview=1,wait=0.4,expect=pipShown:1,closepip,wait=1.2,expect=pipShown:0", fake_backend=True)
    check("a webcam window closed again and again during a take is left closed, and the log says so",
          "pick=2,preview=0,start,expect=state:recording,wait=0.4,closepip,wait=0.8,closepip,wait=0.8,closepip,wait=0.8,expect=pipShown:1,closepip,wait=1.2,expect=pipShown:0,expect=state:recording",
          fake_backend=True, expect_log=("closed again and again during a take: MOBS leaves it closed",))
    check("a webcam window put back when the take is over is not put back again", "pick=2,preview=0,start,expect=state:recording,wait=0.4,closepip,wait=0.9,expect=pipShown:1,stop,waitidle,wait=0.5,expect=pipShown:0", fake_backend=True)
    # The real chat: a script in the person's own browser on another computer POSTs what his viewers say; MOBS shows it as plain text under the webcam.
    relay_env = {"MOBS_CHAT_RELAY_HOST": "127.0.0.1", "MOBS_CHAT_RELAY_PORT": "0"}
    check("the chat relay is off unless it is asked for", "pick=2,preview=1,wait=0.4,expect=chatWanted:0", fake_backend=True)
    check("the chat relay puts a posted message in the card under the webcam and says the other computer is alive",
          "pick=2,preview=1,wait=0.4,expect=chatWanted:1,expect=chatConnected:0,relaypost=ana|hello there,wait=0.7,expect=chatCount:1,expect=chatConnected:1,checkpip",
          fake_backend=True, extra_env=relay_env,
          expect_log=("relay listening on 127.0.0.1", "relay: first call from", "relay: 1 new chat line(s)", "relay: your other computer is connected"))
    check("the chat relay shows what strangers type as plain text, never as markup",
          "pick=2,preview=1,wait=0.4,relaypost=eve|<b>bold</b> <img src=\"http://x.invalid/a.png\"> hi,wait=0.7,expect=chatCount:1,piphover=chatRow,wait=0.3,expect=pipPlain:chatBody,expect=pipPlain:chatName",
          fake_backend=True, extra_env=relay_env)
    # The guided setup ("easy mode"): the first run walks the person through every step with a big NEXT, and keeps what they chose.
    setup_env = {"MOBS_SETUP": "1"}
    check("the setup guide walks from welcome to FINISH and keeps what was chosen",
          "wait=0.5,expect=setupOpen:1,expect=setupStep:0,click=setupNext,expect=setupStep:1,expect=cameraLocked:0,click=setupLockCamera,expect=cameraLocked:1,click=setupNext,expect=setupStep:2,"
          "click=setupLockMic,expect=micLocked:1,click=setupNext,expect=setupStep:3,click=setupShowScreen,wait=0.8,expect=previewFrame:1,click=setupNext,expect=setupStep:4,wait=0.5,expect=pipShown:1,click=setupNext,"
          "expect=setupStep:5,click=setupOpenStudio,click=setupNext,expect=setupStep:6,click=setupNext,expect=setupStep:7,expect=streamReady:0,clip=abc123TESTKEY,click=setupPasteKey,expect=streamReady:1,click=setupNext,"
          "expect=setupStep:8,expect=xSignedIn:0,xurl=https://x.com/home,expect=xSignedIn:1,click=setupNext,expect=setupStep:9,click=setupNext,expect=setupStep:10,click=setupNext,expect=setupOpen:0,expect=setupDone:1,expect=setupStep:0",
          fake_backend=True, extra_env=setup_env,
          expect_log=("camera locked in: Logitech BRIO", "microphone locked in", "opening https://studio.x.com/live", "guided setup finished"))
    check("the setup guide opens again where the person stopped", "wait=0.5,expect=setupOpen:1,expect=setupStep:3,click=setupBack,expect=setupStep:2", fake_backend=True, extra_env=setup_env, ini_extra="setupStep=3\n")
    check("a mark on the ready screen opens the guide at the page that fixes it", "wait=0.4,expect=setupOpen:0,click=checkMic,expect=setupOpen:1,expect=setupStep:2", fake_backend=True)
    check("signing in to X is noticed from its home page only", "setupstep=8,wait=0.5,expect=xSignedIn:0,xurl=https://x.com/i/flow/login,expect=xSignedIn:0,xurl=https://evil.example/home,expect=xSignedIn:0,xurl=https://x.com/home,expect=xSignedIn:1,wait=0.2,click=setupNext,expect=setupStep:9",
          fake_backend=True, extra_env=setup_env, expect_log=("signed in to X inside MOBS",))
    # The chat of the person's own livestream, as X shows it: a Live Studio link (Copy URL) attaches it under the webcam, and the card shows only the Live Chat part.
    check("a Live Studio link attaches the chat under the webcam, and the card shows only its Live Chat",
          "pick=2,preview=1,wait=0.4,expect=chatWanted:0,xattach=https://x.com/i/live-studio/1PJqrNXnQywxb,expect=chatXId:1PJqrNXnQywxb,expect=chatWanted:1,wait=3.8,expect=xCrop:cropped,expect=xHiddenAtLeast:3,checkpip",
          fake_backend=True, expect_log=("X chat attached",))
    check("a link that is not a Live Studio link attaches nothing", "pick=2,preview=1,wait=0.4,xattach=https://example.com/i/live-studio/1PJqrNXnQywxb,expect=chatXId:,expect=chatWanted:0", fake_backend=True)
    check("the chat of a livestream ends with the take", "pick=2,preview=1,wait=0.4,xattach=https://x.com/i/broadcasts/1PJqrNXnQywxb,expect=chatXId:1PJqrNXnQywxb,start,expect=state:recording,stop,waitidle,expect=chatXId:,expect=chatWanted:0", fake_backend=True)
    check("a web address pasted as the stream key is refused", "setupstep=7,wait=0.4,clip=rtmp://va.pscp.tv:80/x,click=setupPasteKey,wait=0.2,expect=streamReady:0", fake_backend=True, extra_env=setup_env)
    for size in ("860x560", "1024x768", "1180x720"):
        check("the setup guide fits on every page", "wait=0.4," + ",".join(f"setupstep={n},wait=0.35,checklayout" for n in range(11)), size, True, extra_env=setup_env)
    check("the webcam window is off when the setting is off, and the picture has no drag handle then either way", "pick=2,preview=1,wait=0.4,expect=pipWanted:0,expect=pipShown:0", fake_backend=True, ini_extra="pip=false\n")
    check("my screen and just my camera have no webcam window", "pick=1,preview=1,wait=0.3,expect=pipShown:0,pick=3,wait=0.3,expect=pipShown:0", fake_backend=True)
    check("the webcam window toggle in EXPERT", "pro=1,tab=0,wait=0.1,checklayout", "860x560")
    check("there is no webcam to drag in the other picks", "pick=1,wait=0.3,expect=camMoved:0,checklayout", fake_backend=True)
    # Going live (the fake backend pretends to connect, go live, drop and come back). The server address and key here are made up.
    live_setup = "streamurl=rtmp://live.example/x,streamkey=test-key-123,"
    for size in ("860x560", "1024x768", "1180x720"):
        check("live setup page with a saved key fits", live_setup + "pro=1,tab=6,expect=streamReady:1,wait=0.1,checklayout", size)
        check("ready screen armed to go live fits", live_setup + "pick=1,arm=1,expect=armed:1,wait=0.1,checklayout", size, True)
        check("ready screen armed with the preview fits", live_setup + "pick=1,preview=1,arm=1,expect=previewFrame:1,wait=0.1,checklayout", size, True)
    for size in ("860x560", "1024x768", "1180x720", "900x1600", "1206x2000"):
        check("a live take shows its state and fits",
              live_setup + "pick=1,arm=1,start,expect=state:recording,expect=armed:0,expect=broadcasting:1,wait=0.8,expect=streamState:live,expect=previewFrame:1,checklayout,"
              "streamretry,expect=streamState:reconnecting,wait=0.1,checklayout,streamback,expect=streamState:live,"
              "streamdrop,expect=streamState:failed,expect=broadcasting:0,expect=state:recording,wait=0.1,checklayout", size, True)
        check("a live take with both audio meters fits",
              live_setup + "pick=1,desktop=1,arm=1,start,expect=state:recording,wait=0.8,expect=streamState:live,checklayout,pro=1,wait=0.1,checklayout", size, True)
    check("a live take cannot be paused, and the saved copy carries on when it ends",
          live_setup + "pick=1,arm=1,start,expect=state:recording,wait=0.8,pause,expect=state:recording,expect=message:you are live,stop,waitidle,expect=armed:0,expect=streamState:off,expect=broadcasting:0", fake_backend=True)
    check("after the connection is lost the take can be paused",
          live_setup + "pick=1,arm=1,start,expect=state:recording,wait=0.8,streamdrop,expect=streamState:failed,pause,expect=state:paused,resume,expect=state:recording", fake_backend=True)
    check("going live is off again for the next take",
          live_setup + "pick=1,arm=1,start,expect=state:recording,wait=0.5,stop,waitidle,start,expect=state:recording,expect=broadcasting:0,expect=streamState:off", fake_backend=True)
    # Going live: MOBS's own window stays up with the steps (the real Live Studio dialog) until the person says they pressed Go Live, and only then steps
    # out of the picture. A plain recording has nothing to finish and hides at once.
    for size in ("860x560", "1180x720"):
        check("going live keeps MOBS's window up with the steps until I'm live is pressed",
              live_setup + "pick=1,arm=1,start,expect=state:recording,wait=0.8,expect=streamState:live,expect=mainHeld:1,checklayout,click=goLiveHide,expect=mainHeld:0,stop,waitidle,expect=mainHeld:0",
              size, True, expect_log=("going live: MOBS's window stays up", "opening https://studio.x.com/live"))
    check("a plain recording hides MOBS's window at once, with nothing to hold", "pick=1,start,expect=state:recording,wait=0.3,expect=mainHeld:0", fake_backend=True)
    check("the hold ends by itself after a while", live_setup + "pick=1,arm=1,start,expect=state:recording,wait=0.3,expect=mainHeld:1,wait=1.3,expect=mainHeld:0",
          fake_backend=True, extra_env={"MOBS_HOLD_MS": "700"}, expect_log=("hiding it now",))
    check("stopping the stream ends the hold", live_setup + "pick=1,arm=1,start,expect=state:recording,wait=0.5,expect=mainHeld:1,click=goLiveStop,waitidle,expect=mainHeld:0", fake_backend=True)
    check("arming without an address or key sends the user to set them up",
          "pick=1,arm=1,expect=armed:1,start,expect=state:idle,expect=message:go live needs,expect=setupOpen:1,expect=setupStep:5,wait=0.1,checklayout", fake_backend=True)
    check("the hotkey chip arms and disarms once a key is saved",
          live_setup + "pick=1,click=liveChip,expect=armed:1,click=liveChip,expect=armed:0", fake_backend=True)
    # The picture is on by default, and the broadcast has its own button beside Record.
    for size in ("860x560", "1024x768", "1180x720"):
        check("the ready screen shows the picture by default and fits with the GO LIVE button", "pick=2,wait=0.4,expect=preview:1,expect=previewing:1,expect=previewFrame:1,checklayout", size, True, default_preview=True)
        check("the ready screen with the picture, a saved key and the GO LIVE button fits", live_setup + "pick=2,wait=0.4,expect=previewFrame:1,checklayout", size, True, default_preview=True)
    check("the GO LIVE button arms and starts a broadcast in one click", live_setup + "pick=1,click=goLiveButton,expect=state:recording,expect=broadcasting:1,expect=armed:0,wait=0.8,expect=streamState:live", fake_backend=True)
    check("the GO LIVE button without a saved address and key opens the guide at the X page", "pick=1,click=goLiveButton,expect=state:idle,expect=setupOpen:1,expect=setupStep:5,wait=0.1,checklayout", fake_backend=True)
    check("arming to go live always shows the picture, even with the preview chip off", "pick=1,expect=previewing:0,arm=1,expect=previewing:1,expect=previewFrame:1,arm=0,expect=previewing:0", fake_backend=True)
    check("the close EXPERT button on the live page leaves EXPERT", live_setup + "pro=1,tab=6,wait=0.1,click=closeLive,expect=pro:0", fake_backend=True)
    check("a take that is not live shows no broadcast", "pick=1,start,expect=state:recording,wait=0.6,expect=broadcasting:0,expect=streamState:off,checklayout", fake_backend=True)
    # A screen share that delivers nothing but black (a dragged Region does this on gus) is named, and it never goes live.
    black = {"MOBS_BLACK_AFTER_MS": "300"}
    for size in ("860x560", "1024x768", "1180x720"):
        check("a black screen picture is named on the ready screen and fits", "pick=1,preview=1,wait=0.2,blackpicture=1,wait=0.8,expect=pictureProblem:black,checklayout", size, True, extra_env=black)
    check("GO LIVE refuses a black picture and says why", live_setup + "pick=1,preview=1,wait=0.2,blackpicture=1,wait=0.8,click=goLiveButton,expect=state:idle,expect=notice:black,wait=0.1,checklayout", fake_backend=True, extra_env=black)
    check("a black picture that comes back lets GO LIVE through",
          live_setup + "pick=1,preview=1,wait=0.2,blackpicture=1,wait=0.8,expect=pictureProblem:black,blackpicture=0,wait=0.4,expect=pictureProblemEmpty:1,click=goLiveButton,expect=state:recording,expect=broadcasting:1", fake_backend=True, extra_env=black)
    for size in ("860x560", "1024x768", "900x1600"):
        check("a live take that turns black keeps going and says so and fits",
              live_setup + "pick=1,preview=1,blackpicture=0,wait=0.4,click=goLiveButton,expect=state:recording,wait=0.8,expect=streamState:live,blackpicture=1,wait=0.8,expect=pictureProblem:black,expect=state:recording,expect=streamState:live,checklayout", size, True, extra_env=black)
    # The dock: your webcam with the chat card under it, in ONE window.
    dock = "pick=2,preview=1,wait=0.4,"
    check("dock: the webcam alone has no chat card", dock + "expect=chatWanted:0,checkpip", fake_backend=True)
    check("dock: a chat feed adds the chat card under the webcam and it fits", dock + "chatmsg=ana|hello,chatmsg=bo|what theme is that?,expect=chatWanted:1,expect=chatCount:2,wait=0.5,checkpip", fake_backend=True)
    for size in ("200x300", "360x420", "480x520", "700x900", "480x300"):
        check(f"dock: webcam and chat stay stacked and inside when the window is {size}", dock + f"chatmsg=ana|hello,chatmsg=bo|what theme is that?,pipsize={size},wait=0.5,checkpip", fake_backend=True)
    check("dock: a chat that goes away takes its card with it", dock + "chatmsg=ana|hi,expect=chatWanted:1,chatoff,expect=chatWanted:0,wait=0.3,checkpip", fake_backend=True)
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
    # One copy at a time (2026-10-12: three live takes at once, windows hidden, the webcam dead in all). Real session bus, but a throwaway
    # bus name (MOBS_CONTROL_NAME), so the real hotkey door org.mobs.Control is never touched. Skipped where there is no session bus.
    if (not case_filter or case_filter in "one copy") and shutil.which("busctl") and (os.environ.get("DBUS_SESSION_BUS_ADDRESS") or Path(f"/run/user/{os.getuid()}/bus").exists()):
        bus_name = f"org.mobs.ControlUi{os.getpid()}"

        def copy_env(tag, script):
            e = env | {"MOBS_SETTINGS": str(root / f"{tag}.ini"), "MOBS_STREAM_KEY_FILE": str(root / f"{tag}.key"), "MOBS_LOG_FILE": str(root / f"{tag}.log"),
                       "MOBS_CONTROL_NAME": bus_name, "MOBS_WINDOW_SIZE": "860x560", "MOBS_FAKE_BACKEND": "1", "MOBS_SCRIPT": script}
            e.pop("MOBS_NO_HOTKEY", None)    # the whole point: these copies DO use the bus
            Path(e["MOBS_SETTINGS"]).write_text(f"[General]\nfolder={root / 'takes'}\nreviewAfter=false\npreview=false\n")
            return e

        def copy_status():
            r = subprocess.run(["busctl", "--user", "call", bus_name, "/org/mobs/Control", "org.mobs.Control", "Status"], capture_output=True, text=True, timeout=5)
            return r.stdout.strip() if r.returncode == 0 else ""

        # Alone, a copy starts as always: the guard stays quiet when nobody else owns the name.
        alone = subprocess.run([binary], env=copy_env("alone", "wait=0.1,quit"), capture_output=True, text=True, timeout=15)
        if alone.returncode or "script: quit" not in alone.stdout or "already running" in Path(root / "alone.log").read_text():
            raise AssertionError(f"one copy: a lone copy did not start normally\n{alone.stdout}{alone.stderr}")
        case_count += 1
        print("PASS one copy: alone, a copy starts as always", flush=True)

        for state, first_script in (("idle", "wait=6,quit"), ("recording", "pick=1,start,expect=state:recording,wait=6,stop,waitidle,quit")):
            first = subprocess.Popen([binary], env=copy_env(f"first-{state}", first_script), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            try:
                for _ in range(80):    # up to 8 s for the first copy to own the name and, for the take, to be recording
                    if f'"{state}' in copy_status():
                        break
                    if first.poll() is not None:
                        raise AssertionError(f"one copy ({state}): the first copy ended early\n{first.stdout.read()}")
                    time.sleep(0.1)
                else:
                    raise AssertionError(f"one copy ({state}): the first copy never said '{state}' on the bus (said {copy_status()!r})")
                # The second copy must refuse, say why in the log, leave at once, and never run its script (which would take 3 s).
                second = subprocess.run([binary], env=copy_env(f"second-{state}", "wait=3,quit"), capture_output=True, text=True, timeout=15)
                second_log = Path(root / f"second-{state}.log").read_text()
                if second.returncode or "script: quit" in second.stdout or f"another MOBS is already running ({state}" not in second_log:
                    raise AssertionError(f"one copy ({state}): the second copy was not turned away\nexit {second.returncode}\n{second.stdout}{second.stderr}\n{second_log}")
                if state == "idle" and "not recording" not in second_log:
                    raise AssertionError(f"one copy (idle): the notice does not say the first copy is idle\n{second_log}")
                if state == "recording" and ("already running a take (recording" not in second_log or "hidden" not in second_log):
                    raise AssertionError(f"one copy (recording): the notice does not name the take and where its window went\n{second_log}")
            finally:
                out, _ = first.communicate(timeout=20)
            if first.returncode != 0 or "script: quit" not in out:
                raise AssertionError(f"one copy ({state}): the first copy was disturbed by the second\nexit {first.returncode}\n{out}")
            case_count += 1
            print(f"PASS one copy: a second copy is turned away while the first is {state}, and the first carries on", flush=True)
    if failures:
        print(f"{len(failures)} scenarios FAILED:")
        for failed in failures:
            print("  " + failed)
        sys.exit(1)
    print(f"{case_count} UI scenarios passed")
